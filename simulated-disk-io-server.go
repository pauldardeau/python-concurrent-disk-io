// simulate occasional problematic (long blocking) requests for disk IO

package main

import (
    "bufio"
    "fmt"
    "math/rand"
    "net"
    "os"
    "strconv"
    "strings"
    "time"
)

type ThreadRequest struct {
    client_socket net.Conn
    receipt_timestamp int64
}

type ReadResponse struct {
    rc int
    bytes_read int
    elapsed_time_ms int64
}

const (
    NUM_WORKER_THREADS = 5

    READ_TIMEOUT_SECS = 4

    STATUS_READ_SUCCESS  = 0
    STATUS_READ_TIMEOUT  = 1
    STATUS_READ_IO_FAIL  = 2
    STATUS_QUEUE_TIMEOUT = 3

    // percentage of requests that result in very long response times (many seconds)
    PCT_LONG_IO_RESPONSE_TIMES = 0.10

    // percentage of requests that result in IO failure
    PCT_IO_FAIL = 0.001

    // max size that would be read
    MAX_READ_BYTES = 100000

    // min/max values for io failure
    MIN_TIME_FOR_IO_FAIL = 0.3
    MAX_TIME_FOR_IO_FAIL = 3.0

    // min/max values for slow read (dying disk)
    MIN_TIME_FOR_SLOW_IO = 6.0
    MAX_TIME_FOR_SLOW_IO = 20.0

    // min/max values for normal io
    MIN_TIME_FOR_NORMAL_IO = 0.075
    MAX_TIME_FOR_NORMAL_IO = 0.4

    // maximum ADDITIONAL time above timeout experienced by requests that time out
    MAX_TIME_ABOVE_TIMEOUT = MAX_TIME_FOR_SLOW_IO * 0.8
)

var r = rand.New(rand.NewSource(53))


func current_time_millis() int64 {
    return time.Now().UnixNano() / int64(time.Millisecond) 
}

func random_value() float64 {
    return r.Float64()
}

func random_value_between(min_value float64, max_value float64) float64 {
    var rand_value = random_value() * max_value
    if rand_value < min_value {
        rand_value = min_value
    } 
    return rand_value
}

func simulated_file_read(file_path string,
                         elapsed_time_ms int64, 
                         read_timeout_secs int,
                         read_response *ReadResponse) {

    var read_timeout_ms = int64(read_timeout_secs * 1000)
    var num_bytes_read int
    var rand_number = random_value()
    var request_with_slow_read = false
    var rc = STATUS_READ_SUCCESS
    var min_response_time float64
    var max_response_time float64

    if rand_number <= PCT_IO_FAIL {
        // simulate read io failure
        rc = STATUS_READ_IO_FAIL
        num_bytes_read = 0
        min_response_time = MIN_TIME_FOR_IO_FAIL
        max_response_time = MAX_TIME_FOR_IO_FAIL
    } else {
        rc = STATUS_READ_SUCCESS
        num_bytes_read = int(random_value_between(0, MAX_READ_BYTES))
        if rand_number <= PCT_LONG_IO_RESPONSE_TIMES {
            // simulate very slow request
            request_with_slow_read = true
            min_response_time = MIN_TIME_FOR_SLOW_IO
            max_response_time = MAX_TIME_FOR_SLOW_IO
        } else {
            // simulate typical read response time
            min_response_time = MIN_TIME_FOR_NORMAL_IO
            max_response_time = MAX_TIME_FOR_NORMAL_IO
        }
    }

    // do we need to generate the response time? (i.e., not predetermined)
    if -1 == elapsed_time_ms {
        elapsed_time_ms =
            int64(1000 * random_value_between(min_response_time,
                                              max_response_time))

        if elapsed_time_ms > read_timeout_ms && !request_with_slow_read {
            rc = STATUS_READ_TIMEOUT
            elapsed_time_ms +=
                int64(1000.0 * random_value_between(0,
                                                    MAX_TIME_ABOVE_TIMEOUT))
            num_bytes_read = 0
        }
    }

    // ***********  simulate the disk read  ***********
    time.Sleep(time.Millisecond * time.Duration(elapsed_time_ms))

    if rc == STATUS_READ_TIMEOUT {
        fmt.Println("timeout (assigned)")
    } else if rc == STATUS_READ_IO_FAIL {
        fmt.Println("io fail")
    } else if rc == STATUS_READ_SUCCESS {
        // what would otherwise have been a successful read turns into a
        // timeout error if simulated execution time exceeds timeout value
        if elapsed_time_ms > read_timeout_ms {
            //TODO: it would be nice to increment a counter here and show
            // the counter value as part of print
            fmt.Println("timeout (service)")
            rc = STATUS_READ_TIMEOUT
            num_bytes_read = 0
        }
    }

    read_response.rc = rc
    read_response.bytes_read = num_bytes_read
    read_response.elapsed_time_ms = elapsed_time_ms
}

func handle_socket_request(thread_request *ThreadRequest) {
    var read_resp ReadResponse

    // read request from client
    request_text, errs :=
        bufio.NewReader(thread_request.client_socket).ReadString('\n')

    // read from socket succeeded?
    if errs == nil {
        len_req_payload := len(request_text)
        // did we get anything from client?
        if len_req_payload > 0 {
            // discard the newline from request text
            //if (request_text[len_req_payload-1] == '\n') {
            //    request_text[len_req_payload-1] = '\0'
            //}

            // determine how long the request has waited in queue
            var start_processing_timestamp = current_time_millis()
            var queue_time_ms =
                start_processing_timestamp - thread_request.receipt_timestamp
            var queue_time_secs = queue_time_ms / 1000

            // unless otherwise specified, let server generate
            // the response time
            var predetermined_response_time_ms = int64(-1)

            // look for field delimiter in request
            var field_delimiter = strings.Index(request_text, ",")
            var file_path string
            if field_delimiter != -1 {
                file_path = request_text[field_delimiter + 1:]
                num_digits := field_delimiter
                if (num_digits > 0) && (num_digits < 10) {
                    req_time_digits := request_text[0:field_delimiter]
                    req_response_time_ms, errs :=
                        strconv.ParseInt(req_time_digits, 10, 64)
                    if errs == nil {
                        predetermined_response_time_ms = req_response_time_ms
                    }
                }
            } else {
                file_path = request_text
            }

            // has this request already timed out?
            if queue_time_secs >= READ_TIMEOUT_SECS {
                fmt.Println("timeout (queue)")
                read_resp.rc = STATUS_QUEUE_TIMEOUT
                read_resp.bytes_read = 0
                read_resp.elapsed_time_ms = 0
            } else {
                // *********  perform simulated read of disk file  *********
                simulated_file_read(file_path,
                                    predetermined_response_time_ms,
                                    READ_TIMEOUT_SECS,
                                    &read_resp)
            }

            // total request time is sum of time spent in queue and the
            // simulated disk read time
            tot_request_time_ms :=
                queue_time_ms + read_resp.elapsed_time_ms

            // create response text
            var response_text = fmt.Sprintf("%d,%d,%d,%s",
                     read_resp.rc,
                     read_resp.bytes_read,
                     tot_request_time_ms,
                     file_path)

            // return response text to client
            thread_request.client_socket.Write([]byte(response_text))
        }
    }

    // close client socket connection
    thread_request.client_socket.Close()
}

func main() {
    var server_port = "7000"
    server_socket, err := net.Listen("tcp", "localhost:" + server_port)

    if err != nil {
        fmt.Println("error: unable to create server socket on port " +
                    server_port)
        fmt.Println(fmt.Sprintf("%v", err))
        os.Exit(1)
    } else {
        //defer server_socket.Body.Close()
        fmt.Println("server listening on port " + server_port)

        for {
            // wait for next socket connection from a client
            client_socket, err := server_socket.Accept()
            if err == nil {
                // create a new thread. for something real, a thread
                // pool would be used, but creating a new thread for
                // each request should be fine for our purposes.
                var thread_request = new(ThreadRequest)
                thread_request.receipt_timestamp = current_time_millis()
                thread_request.client_socket = client_socket
                go handle_socket_request(thread_request)
            }
        }
        os.Exit(0)
    }
}

