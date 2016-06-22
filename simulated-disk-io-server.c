// simulate occasional problematic (long blocking) requests within eventlet

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>


typedef struct _ReadResponse {
    int rc;
    int bytes_read;
    double elapsed_time;
    char file_path[256];
} ReadResponse;


static int NUM_GREEN_THREADS = 5;

static int READ_TIMEOUT = 4;

// status codes to indicate whether read succeeded, timed out, or failed
static int STATUS_READ_SUCCESS = 0;
static int STATUS_READ_TIMEOUT = 1;
static int STATUS_READ_IO_FAIL = 2;

// percentage of requests that result in very long response times (many seconds)
static double PCT_LONG_IO_RESPONSE_TIMES = 0.10;

// percentage of requests that result in IO failure
static double PCT_IO_FAIL = 0.001;

// max size that would be read
static double MAX_READ_BYTES = 100000;

// min/max values for io failure
static double MIN_TIME_FOR_IO_FAIL = 0.3;
static double MAX_TIME_FOR_IO_FAIL = 3.0;

// min/max values for slow read (dying disk)
static double MIN_TIME_FOR_SLOW_IO = 6.0;
static double MAX_TIME_FOR_SLOW_IO = 20.0;

// min/max values for normal io
static double MIN_TIME_FOR_NORMAL_IO = 0.075;
static double MAX_TIME_FOR_NORMAL_IO = 0.4;

// maximum ADDITIONAL time above timeout experienced by requests that time out
static double MAX_TIME_ABOVE_TIMEOUT; // = MAX_TIME_FOR_SLOW_IO * 0.8;


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

long current_time_millis() {
    //TODO: implement current_time_millis
    return 0L;
}

void simulated_file_read(const char* file_path,
                         int read_timeout,
                         ReadResponse* read_response) {

    long start_time = current_time_millis();
    int num_bytes_read = 0;
    double rand_number = random_value();
    int request_with_slow_read = 0;
    int rc;
    long elapsed_time = 0;

    if (rand_number <= PCT_IO_FAIL) {
        // simulate read io failure
        rc = STATUS_READ_IO_FAIL;
        elapsed_time =
            (long) (1000.0 * random_value_between(MIN_TIME_FOR_IO_FAIL,
                                                  MAX_TIME_FOR_IO_FAIL));
    } else {
        rc = STATUS_READ_SUCCESS;
        if (rand_number <= PCT_LONG_IO_RESPONSE_TIMES) {
            // simulate very slow request
            request_with_slow_read = 1;
            elapsed_time =
                (long) (1000.0 * random_value_between(MIN_TIME_FOR_SLOW_IO,
                                                      MAX_TIME_FOR_SLOW_IO));
        } else {
            // simulate typical read response time
            elapsed_time =
                (long) (1000.0 * random_value_between(MIN_TIME_FOR_NORMAL_IO,
                                                      MAX_TIME_FOR_NORMAL_IO));
        }
        num_bytes_read = (int) random_value_between(0, MAX_READ_BYTES);
    }

    if (elapsed_time > read_timeout && !request_with_slow_read) {
        rc = STATUS_READ_TIMEOUT;
        elapsed_time =
            (long) (1000.0 * random_value_between(0, MAX_TIME_ABOVE_TIMEOUT));
        num_bytes_read = 0;
    }

    sleep(elapsed_time);

    long end_time = current_time_millis();

    if (rc == STATUS_READ_TIMEOUT) {
        printf("timeout (assigned)\n");
    } else if (rc == STATUS_READ_IO_FAIL) {
        printf("io fail\n");
    } else if (rc == STATUS_READ_SUCCESS) {
        // what would otherwise have been a successful read turns into a
        // timeout error if simulated execution time exceeds timeout value
        long exec_time = end_time - start_time;
        if (exec_time > read_timeout) {
            //TODO: it would be nice to increment a counter here and show
            // the counter value as part of print
            printf("timeout (service)\n");
            rc = STATUS_READ_TIMEOUT;
            num_bytes_read = 0;
            elapsed_time = exec_time;
        }
    }

    read_response->rc = rc;
    read_response->bytes_read = num_bytes_read;
    read_response->elapsed_time = elapsed_time;
    strncpy(read_response->file_path, file_path, 255);
}

void handle_socket_request(int sock) {
    ReadResponse read_resp;
    char file_path[256];
    char read_resp_text[256];

    memset(file_path, 0, 256);
    if (read(sock, file_path, 256) > 0) {
        if (strlen(file_path) > 0) {
            simulated_file_read(file_path, READ_TIMEOUT, &read_resp);
            snprintf(read_resp_text, 255, "%d,%d,%f,%s\n",
                     read_resp.rc,
                     read_resp.bytes_read,
                     read_resp.elapsed_time,
                     read_resp.file_path);
            write(sock, read_resp_text, strlen(read_resp_text));
        }
    }
    close(sock);
}

int main(int argc, char* argv[]) {
    MAX_TIME_ABOVE_TIMEOUT = MAX_TIME_FOR_SLOW_IO * 0.8;
    int server_port = 6000;
    int server_socket = 0;
    int sock;
    int rc;
    struct sockaddr_in serv_addr;
    struct sockaddr_in cli_addr;
    int cli_addr_len = sizeof(cli_addr);

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
        sock = accept(server_socket,
                      (struct sockaddr *) &cli_addr,
                      &cli_addr_len);
        if (sock > -1) {
            //TODO: change this to run the request on a separate thread
            handle_socket_request(sock);
        }
    }
    return 0;

no_socket:
    if (server_socket > 0) {
        close(server_socket);
    }
    printf("error: unable to create server socket on port %d",
           server_port);
}

