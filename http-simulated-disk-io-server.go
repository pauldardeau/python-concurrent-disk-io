// go - simulate occasional problematic (long blocking) requests for disk IO
// to run: go http-simulated-disk-io-server.go

package main

import (
    "bufio"
    "fmt"
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

const (
    READ_TIMEOUT_SECS = 4

    HTTP_STATUS_OK = 200
    HTTP_STATUS_TIMEOUT = 408
    HTTP_STATUS_BAD_REQUEST = 400 
)

func current_time_millis() int64 {
    return time.Now().UnixNano() / int64(time.Millisecond) 
}

func simulated_file_read(disk_read_time_ms int64) {
    time.Sleep(time.Millisecond * time.Duration(disk_read_time_ms))
}

func handle_socket_request(thread_request *ThreadRequest) {
    // read request from client
    request_text, errs :=
        bufio.NewReader(thread_request.client_socket).ReadString('\n')

    // read from socket succeeded?
    if errs == nil {
        len_req_payload := len(request_text)
        // did we get anything from client?
        if len_req_payload > 0 {
            // determine how long the request has waited in queue
            var start_processing_timestamp = current_time_millis()
            var server_queue_time_ms =
                start_processing_timestamp - thread_request.receipt_timestamp
            var server_queue_time_secs = server_queue_time_ms / 1000

            var rc = HTTP_STATUS_OK
            var disk_read_time_ms = int64(0)
            var file_path = ""

            // has this request already timed out?
            if server_queue_time_secs >= READ_TIMEOUT_SECS {
                fmt.Println("timeout (queue)")
                rc = HTTP_STATUS_TIMEOUT
            } else {
                var tokens = strings.Split(request_text, ",")
                if len(tokens) == 3 {
                    rc_input, errs := strconv.Atoi(tokens[0])
                    if errs == nil {
                        req_response_time_ms, errs :=
                            strconv.ParseInt(tokens[1], 10, 64)
                        if errs == nil {
                            rc = rc_input
                            disk_read_time_ms = req_response_time_ms
                            file_path = tokens[2]
                            simulated_file_read(disk_read_time_ms)
                        }
                    }
                }
            }

            // total request time is sum of time spent in queue and the
            // simulated disk read time
            tot_request_time_ms :=
                server_queue_time_ms + disk_read_time_ms

            // create response text
            var response_text = fmt.Sprintf("%d,%d,%s",
                rc,
                tot_request_time_ms,
                file_path)

            // return response to client
            thread_request.client_socket.Write([]byte(response_text))
        }
    }

    // close client socket connection
    thread_request.client_socket.Close()
}

func main() {
    var server_port = "7000"
    //TODO: set listen backlog?
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
                var thread_request = new(ThreadRequest)
                thread_request.receipt_timestamp = current_time_millis()
                thread_request.client_socket = client_socket
                go handle_socket_request(thread_request)
            }
        }
        os.Exit(0)
    }
}

