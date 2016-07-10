// go
// install tools:
//   sudo curl -O https://storage.googleapis.com/golang/go1.6.linux-amd64.tar.gz
//   tar xf go1.6.linux-amd64.tar.gz
//   sudo mv go /usr/local
//   export PATH=/usr/local/go/bin:$PATH
// to build: go build http-server.go
// to run: ./http-server

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
    SERVER_PORT = 7000
    QUEUE_TIMEOUT_SECS = 4
    LISTEN_BACKLOG = 500

    SERVER_NAME = "http-server.go"
    HTTP_STATUS_OK = "200 OK"
    HTTP_STATUS_TIMEOUT = "408 TIMEOUT"
    HTTP_STATUS_BAD_REQUEST = "400 BAD REQUEST"
)

func current_time_millis() int64 {
    return time.Now().UnixNano() / int64(time.Millisecond) 
}

func simulated_file_read(disk_read_time_ms int64) {
    time.Sleep(time.Millisecond * time.Duration(disk_read_time_ms))
}

func handle_socket_request(thread_request *ThreadRequest) {
    // read request from client
    request_line, errs :=
        bufio.NewReader(thread_request.client_socket).ReadString('\n')
    var rc = HTTP_STATUS_BAD_REQUEST
    var tot_request_time_ms = int64(0)
    var file_path = ""

    // read from socket succeeded?
    if errs == nil {
        len_req_payload := len(request_line)
        // did we get anything from client?
        if len_req_payload > 0 {
            // determine how long the request has waited in queue
            var start_processing_timestamp = current_time_millis()
            var server_queue_time_ms =
                start_processing_timestamp - thread_request.receipt_timestamp
            var server_queue_time_secs = server_queue_time_ms / 1000

            var disk_read_time_ms = int64(0)

            // has this request already timed out?
            if server_queue_time_secs >= QUEUE_TIMEOUT_SECS {
                fmt.Println("timeout (queue)")
                rc = HTTP_STATUS_TIMEOUT
            } else {
                var line_tokens = strings.Split(request_line, " ")
                if len(line_tokens) > 1 {
                    //request_method := line_tokens[0]
                    args_text := line_tokens[1]
                    var fields = strings.Split(args_text, ",")
                    if len(fields) == 3 {
                        //rc_input, errs := strconv.Atoi(fields[0])
                        //if errs == nil {
                        req_response_time_ms, errs :=
                            strconv.ParseInt(fields[1], 10, 64)
                        if errs == nil {
                            //rc = rc_input
                            disk_read_time_ms = req_response_time_ms
                            file_path = fields[2]
                            simulated_file_read(disk_read_time_ms)
                            rc = HTTP_STATUS_OK
                        }
                        //}
                    }
                }
            }

            // total request time is sum of time spent in queue and the
            // simulated disk read time
            tot_request_time_ms =
                server_queue_time_ms + disk_read_time_ms
        }
    }

    // create response
    var response_body = fmt.Sprintf("%d,%s",
       tot_request_time_ms,
       file_path)

    var response_headers = fmt.Sprintf("HTTP/1.1 %s\n" +
                                       "Server: %s\n" +
                                       "Content-Length: %d\n" +
                                       "Connection: close\n" +
                                       "\n",
                                       rc,
                                       SERVER_NAME,
                                       len(response_body))
    // return response to client
    thread_request.client_socket.Write([]byte(response_headers))
    thread_request.client_socket.Write([]byte(response_body))

    // close client socket connection
    thread_request.client_socket.Close()
}

func main() {
    //TODO: set listen backlog
    //TODO: set reuse addr
    server_socket, err := net.Listen("tcp",
                                     fmt.Sprintf("localhost:%d", SERVER_PORT))

    if err != nil {
        fmt.Println("error: unable to create server socket on port %s",
                    SERVER_PORT)
        fmt.Println(fmt.Sprintf("%v", err))
        os.Exit(1)
    } else {
        //defer server_socket.Body.Close()
        fmt.Println(fmt.Sprintf("server (%s) listening on port %d",
                                SERVER_NAME,
                                SERVER_PORT))

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

