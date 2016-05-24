// simulate occasional problematic (long blocking) requests for disk IO
// to build:
//    gcc -Wall simulated-disk-io-server.c -o simulated-disk-io-server -lm -lpthread

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>


typedef struct _ThreadRequest {
    int client_socket;
    long receipt_timestamp;
    pthread_t thread;
} ThreadRequest;


static const int READ_TIMEOUT_SECS = 4;

static const int STATUS_OK = 0;
static const int STATUS_QUEUE_TIMEOUT = 1;
static const int STATUS_BAD_INPUT = 2;


long current_time_millis() {
    struct timespec spec;
    clock_gettime(CLOCK_REALTIME, &spec);
    return round(spec.tv_nsec / 1.0e6);
}

void simulated_file_read(long disk_read_time_ms) {
    usleep(disk_read_time_ms * 1000);  // microseconds
}

void handle_socket_request(ThreadRequest* thread_request) {
    char request_text[256];
    char response_text[256];
    const char* file_path = NULL;

    memset(request_text, 0, 256);
    memset(response_text, 0, 256);

    // read request from client
    const int rc = read(thread_request->client_socket,
                        request_text,
                        256);

    // read from socket succeeded?
    if (rc > 0) {
        const int len_req_payload = strlen(request_text);
        // did we get anything from client?
        if (len_req_payload > 0) {
            // discard the newline from request text
            if (request_text[len_req_payload-1] == '\n') {
                request_text[len_req_payload-1] = '\0';
            }

            // determine how long the request has waited in queue
            const long start_processing_timestamp = current_time_millis();
            const long server_queue_time_ms =
                start_processing_timestamp - thread_request->receipt_timestamp;
            const int server_queue_time_secs = server_queue_time_ms / 1000;

            int rc = STATUS_OK;
            long disk_read_time_ms = 0L;

            // has this request already timed out?
            if (server_queue_time_secs >= READ_TIMEOUT_SECS) {
                printf("timeout (queue)\n");
                rc = STATUS_QUEUE_TIMEOUT;
            } else {
                // assume the worst
                rc = STATUS_BAD_INPUT;

                // tokenize input
                char* save_ptr;
                char* rc_digits = strtok_r(request_text, ",", &save_ptr);
                char* req_time_digits = strtok_r(NULL, ",", &save_ptr);
                file_path = strtok_r(NULL, ",", &save_ptr);
                if ((rc_digits != NULL) &&
                    (req_time_digits != NULL) &&
                    (file_path != NULL) &&
                    (strlen(rc_digits) > 0) &&
                    (strlen(req_time_digits) > 0) &&
                    (strlen(file_path) > 0)) {

                    const int rc_input = atoi(rc_digits);
                    const long req_time = atol(req_time_digits);
                    if (req_time >= 0L) {
                        rc = rc_input;
                        disk_read_time_ms = req_time;
                        // ****  simulate disk read  ****
                        simulated_file_read(disk_read_time_ms);
                    }
                }
            }

            // total request time is sum of time spent in queue and the
            // simulated disk read time
            const long tot_request_time_ms =
                server_queue_time_ms + disk_read_time_ms;

            // create response text
            snprintf(response_text, 255, "%d,%ld,%s\n",
                     rc,
                     tot_request_time_ms,
                     file_path);

            // return response text to client
            write(thread_request->client_socket,
                  response_text,
                  strlen(response_text));
        }
    }

    // close client socket connection
    close(thread_request->client_socket);
    free(thread_request);
}

void* thread_handle_socket_request(void* thread_arg) {
    handle_socket_request((ThreadRequest*)thread_arg);
    return NULL;
}

int main(int argc, char* argv[]) {
    const int server_port = 7000;
    int server_socket;
    int sock;
    int rc;
    struct sockaddr_in serv_addr;
    struct sockaddr_in cli_addr;
    socklen_t cli_addr_len = sizeof(cli_addr);

    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        goto no_socket;
    }

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(server_port);
    serv_addr.sin_addr.s_addr = INADDR_ANY;

    rc = bind(server_socket,
              (struct sockaddr *) &serv_addr,
              sizeof(serv_addr));
    if (rc < 0) {
        goto no_socket;
    }

    rc = listen(server_socket, 5);
    if (rc < 0) {
        goto no_socket;
    }

    printf("server listening on port %d\n", server_port);

    while (1) {
        // wait for next socket connection from a client
        sock = accept(server_socket,
                      (struct sockaddr *) &cli_addr,
                      &cli_addr_len);
        if (sock > -1) {
            // create a new thread. for something real, a thread
            // pool would be used, but creating a new thread for
            // each request should be fine for our purposes.
            ThreadRequest* thread_request = malloc(sizeof(ThreadRequest));
            thread_request->receipt_timestamp = current_time_millis();
            thread_request->client_socket = sock;
            pthread_create(&thread_request->thread,
                           NULL,
                           thread_handle_socket_request,
                           (void*)thread_request);
        }
    }
    return 0;

no_socket:
    if (server_socket > 0) {
        close(server_socket);
    }
    printf("error: unable to create server socket on port %d",
           server_port);
    exit(1);
}

