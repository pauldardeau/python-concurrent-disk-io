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


typedef struct _ReadResponse {
    int rc;
    int bytes_read;
    long elapsed_time_ms;
    char file_path[256];
} ReadResponse;


static const int NUM_WORKER_THREADS = 5;

static const int READ_TIMEOUT_SECS = 4;

// status codes to indicate whether read succeeded, timed out, or failed
static const int STATUS_READ_SUCCESS = 0;
static const int STATUS_READ_TIMEOUT = 1;
static const int STATUS_READ_IO_FAIL = 2;

// percentage of requests that result in very long response times (many seconds)
static const double PCT_LONG_IO_RESPONSE_TIMES = 0.10;

// percentage of requests that result in IO failure
static const double PCT_IO_FAIL = 0.001;

// max size that would be read
static const double MAX_READ_BYTES = 100000;

// min/max values for io failure
static const double MIN_TIME_FOR_IO_FAIL = 0.3;
static const double MAX_TIME_FOR_IO_FAIL = 3.0;

// min/max values for slow read (dying disk)
static const double MIN_TIME_FOR_SLOW_IO = 6.0;
static const double MAX_TIME_FOR_SLOW_IO = 20.0;

// min/max values for normal io
static const double MIN_TIME_FOR_NORMAL_IO = 0.075;
static const double MAX_TIME_FOR_NORMAL_IO = 0.4;

// maximum ADDITIONAL time above timeout experienced by requests that time out
static double MAX_TIME_ABOVE_TIMEOUT;


double random_value() {
    return rand() / (double) (RAND_MAX);
}

double random_value_between(double min_value, double max_value) {
    double rand_value = random_value() * max_value;
    if (rand_value < min_value) {
        rand_value = min_value;
    } 
    return rand_value;
}

void simulated_file_read(const char* file_path,
                         long elapsed_time_ms, 
                         int read_timeout_secs,
                         ReadResponse* read_response) {

    const long read_timeout_ms = read_timeout_secs * 1000;
    int num_bytes_read;
    const double rand_number = random_value();
    int request_with_slow_read = 0;
    int rc;
    double min_response_time;
    double max_response_time;

    if (rand_number <= PCT_IO_FAIL) {
        // simulate read io failure
        rc = STATUS_READ_IO_FAIL;
        num_bytes_read = 0;
        min_response_time = MIN_TIME_FOR_IO_FAIL;
        max_response_time = MAX_TIME_FOR_IO_FAIL;
    } else {
        rc = STATUS_READ_SUCCESS;
        num_bytes_read = (int) random_value_between(0, MAX_READ_BYTES);
        if (rand_number <= PCT_LONG_IO_RESPONSE_TIMES) {
            // simulate very slow request
            request_with_slow_read = 1;
            min_response_time = MIN_TIME_FOR_SLOW_IO;
            max_response_time = MAX_TIME_FOR_SLOW_IO;
        } else {
            // simulate typical read response time
            min_response_time = MIN_TIME_FOR_NORMAL_IO;
            max_response_time = MAX_TIME_FOR_NORMAL_IO;
        }
    }

    // do we need to generate the response time? (i.e., not predetermined)
    if (-1L == elapsed_time_ms) {
        elapsed_time_ms =
            (long) (1000.0 * random_value_between(min_response_time,
                                                  max_response_time));

        if (elapsed_time_ms > read_timeout_ms && !request_with_slow_read) {
            rc = STATUS_READ_TIMEOUT;
            elapsed_time_ms =
                (long) (1000.0 * random_value_between(0,
                                                      MAX_TIME_ABOVE_TIMEOUT));
            num_bytes_read = 0;
        }
    }

    // ***********  simulate the disk read  ***********
    usleep(elapsed_time_ms * 1000);  // microseconds

    if (rc == STATUS_READ_TIMEOUT) {
        printf("timeout (assigned)\n");
    } else if (rc == STATUS_READ_IO_FAIL) {
        printf("io fail\n");
    } else if (rc == STATUS_READ_SUCCESS) {
        // what would otherwise have been a successful read turns into a
        // timeout error if simulated execution time exceeds timeout value
        if (elapsed_time_ms > read_timeout_ms) {
            //TODO: it would be nice to increment a counter here and show
            // the counter value as part of print
            printf("timeout (service)\n");
            rc = STATUS_READ_TIMEOUT;
            num_bytes_read = 0;
        }
    }

    read_response->rc = rc;
    read_response->bytes_read = num_bytes_read;
    read_response->elapsed_time_ms = elapsed_time_ms;
    strncpy(read_response->file_path, file_path, 255);
}

void handle_socket_request(int sock) {
    ReadResponse read_resp;
    char request_text[256];
    const char* file_path;
    char response_text[256];

    memset(request_text, 0, 256);

    // read request from client
    const int rc = read(sock, request_text, 256);

    // read from socket succeeded?
    if (rc > 0) {
        const int len_req_payload = strlen(request_text);
        // did we get anything from client?
        if (len_req_payload > 0) {
            // discard the newline from request text
            if (request_text[len_req_payload-1] == '\n') {
                request_text[len_req_payload-1] = '\0';
            }

            // unless otherwise specified, let server generate
            // the response time
            int predetermined_response_time_ms = -1;

            // look for field delimiter in request
            const char* field_delimiter = strchr(request_text, ',');
            if (field_delimiter != NULL) {
                file_path = field_delimiter + 1;
                const int num_digits = field_delimiter - request_text;
                if (num_digits > 0 && num_digits < 10) {
                    char req_time_digits[10];
                    memset(req_time_digits, 0, sizeof(req_time_digits));
                    strncpy(req_time_digits, request_text, num_digits);
                    const long req_response_time_ms = atol(req_time_digits);
                    if (req_response_time_ms > 0) {
                        predetermined_response_time_ms = req_response_time_ms;
                    }
                }
            } else {
                file_path = request_text;
            }

            memset(&read_resp, 0, sizeof(read_resp));

            // *********  perform simulated read of disk file  *********            
            simulated_file_read(file_path,
                                predetermined_response_time_ms,
                                READ_TIMEOUT_SECS,
                                &read_resp);

            // create response text
            snprintf(response_text, 255, "%d,%d,%ld,%s\n",
                     read_resp.rc,
                     read_resp.bytes_read,
                     read_resp.elapsed_time_ms,
                     read_resp.file_path);

            // return response text to client
            write(sock, response_text, strlen(response_text));
        }
    }

    // close client socket connection
    close(sock);
}

void* thread_handle_socket_request(void* thread_arg) {
    const int sock = (int) thread_arg;
    handle_socket_request(sock);
    return NULL;
}

int main(int argc, char* argv[]) {
    MAX_TIME_ABOVE_TIMEOUT = MAX_TIME_FOR_SLOW_IO * 0.8;
    const int server_port = 6000;
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
    srand(time(NULL));

    while (1) {
        // wait for next socket connection from a client
        sock = accept(server_socket,
                      (struct sockaddr *) &cli_addr,
                      &cli_addr_len);
        if (sock > -1) {
            // create a new thread. for something real, a thread
            // pool would be used, but creating a new thread for
            // each request should be fine for our purposes.
            pthread_t worker_thread;
            pthread_create(&worker_thread,
                           NULL,
                           thread_handle_socket_request,
                           (void*)sock);
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

