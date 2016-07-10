// C
// to build:
//    gcc -O2 -Wall http-threads-server.c -o http-threads-server -lm -lpthread

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <pthread.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define BUFFER_SIZE 1024

typedef struct _ThreadRequest {
    int client_socket;
    long receipt_timestamp;
    pthread_t thread;
} ThreadRequest;


static const int SERVER_PORT = 7000;
static const int QUEUE_TIMEOUT_SECS = 4;
static const int LISTEN_BACKLOG = 500;

static char SERVER_NAME[] = "http-threads-server.c";
static char HTTP_STATUS_OK[] = "200 OK";
static char HTTP_STATUS_TIMEOUT[] = "408 TIMEOUT";
static char HTTP_STATUS_BAD_REQUEST[] = "400 BAD REQUEST";


long current_time_millis() {
    struct timespec spec;
    clock_gettime(CLOCK_REALTIME, &spec);
    return round(spec.tv_nsec / 1.0e6);
}

void simulated_file_read(long disk_read_time_ms) {
    usleep(disk_read_time_ms * 1000);  // microseconds
}

void handle_socket_request(ThreadRequest* thread_request) {
    char request_text[BUFFER_SIZE];
    char response_headers[BUFFER_SIZE];
    char response_body[BUFFER_SIZE];
    const char* file_path = NULL;
    long tot_request_time_ms = 0L;
    const char* rc = HTTP_STATUS_BAD_REQUEST;

    memset(request_text, 0, BUFFER_SIZE);
    memset(response_headers, 0, BUFFER_SIZE);
    memset(response_body, 0, BUFFER_SIZE);

    // read request from client
    const int bytes_read = read(thread_request->client_socket,
                                request_text,
                                BUFFER_SIZE);

    // read from socket succeeded?
    if (bytes_read > 0) {
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

            long disk_read_time_ms = 0L;

            // has this request already timed out?
            if (server_queue_time_secs >= QUEUE_TIMEOUT_SECS) {
                printf("timeout (queue)\n");
                rc = HTTP_STATUS_TIMEOUT;
            } else {
                // tokenize input
                const char* pos_space = strchr(request_text, ' ');
                if (pos_space != NULL) {
                    char* save_ptr;
                    const char* rc_digits = strtok_r((char*)pos_space+1, ",", &save_ptr);
                    const char* req_time_digits = strtok_r(NULL, ",", &save_ptr);
                    file_path = strtok_r(NULL, ",", &save_ptr);
                    if ((rc_digits != NULL) &&
                        (req_time_digits != NULL) &&
                        (file_path != NULL) &&
                        (strlen(rc_digits) > 0) &&
                        (strlen(req_time_digits) > 0) &&
                        (strlen(file_path) > 0)) {

                        //const int rc_input = atoi(rc_digits);
                        const long req_time = atol(req_time_digits);
                        if (req_time >= 0L) {
                            //rc = rc_input;
                            disk_read_time_ms = req_time;
                            // ****  simulate disk read  ****
                            simulated_file_read(disk_read_time_ms);
                            rc = HTTP_STATUS_OK;
                        }
                    }
                }
            }

            // total request time is sum of time spent in queue and the
            // simulated disk read time
            tot_request_time_ms =
                server_queue_time_ms + disk_read_time_ms;
        }
    }

    // create response text
    snprintf(response_body, BUFFER_SIZE, "%ld,%s",
             tot_request_time_ms,
             file_path);

    size_t len_response_body = strlen(response_body);

    snprintf(response_headers, BUFFER_SIZE,
             "HTTP/1.1 %s\n"
             "Server: %s\n"
             "Content-Length: %zd\n"
             "Connection: close\n"
             "\n",
             rc,
             SERVER_NAME,
             len_response_body);

    size_t bytes_written;
    size_t len_response_headers = strlen(response_headers);

    // return response to client
    bytes_written = write(thread_request->client_socket,
                          response_headers,
                          len_response_headers);

    if (bytes_written == len_response_headers) {
        bytes_written = write(thread_request->client_socket,
                              response_body,
                              len_response_body);
    }

    shutdown(thread_request->client_socket, SHUT_WR);

    // close client socket connection
    close(thread_request->client_socket);
    free(thread_request);
}

void* thread_handle_socket_request(void* thread_arg) {
    handle_socket_request((ThreadRequest*)thread_arg);
    return NULL;
}

int main(int argc, char* argv[]) {
    int server_socket;
    int sock;
    int rc;
    struct sockaddr_in serv_addr;
    struct sockaddr_in cli_addr;
    socklen_t cli_addr_len = sizeof(cli_addr);

    // install our signal handlers
    signal(SIGPIPE, SIG_IGN);

    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        goto no_socket;
    }

    int enable = 1;
    setsockopt(server_socket,
               SOL_SOCKET,
               SO_REUSEADDR,
               &enable,
               sizeof(int));

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(SERVER_PORT);
    serv_addr.sin_addr.s_addr = INADDR_ANY;

    rc = bind(server_socket,
              (struct sockaddr *) &serv_addr,
              sizeof(serv_addr));
    if (rc < 0) {
        goto no_socket;
    }

    rc = listen(server_socket, LISTEN_BACKLOG);
    if (rc < 0) {
        goto no_socket;
    }

    printf("server (%s) listening on port %d\n", SERVER_NAME, SERVER_PORT);

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
    printf("error: unable to create server socket on port %d\n",
           SERVER_PORT);
    exit(1);
}

