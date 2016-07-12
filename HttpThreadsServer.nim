# nim
# to build: nim c --threads:on --d:release HttpThreadsServer.nim

# helpful link: http://goran.krampe.se/2014/10/25/nim-socketserver/


import net
import os
import strutils
import threadpool
import times

const SERVER_PORT = 7000
const QUEUE_TIMEOUT_SECS = 4
const LISTEN_BACKLOG = 500

const SERVER_NAME = "HttpThreadsServer.nim" 
const HTTP_STATUS_OK = "200 OK"
const HTTP_STATUS_TIMEOUT = "408 TIMEOUT"
const HTTP_STATUS_BAD_REQUEST = "400 BAD REQUEST"

proc simulated_file_read(elapsed_time_ms: int64) =
    sleep(int(elapsed_time_ms))

proc current_time_millis(): int64 =
    return int64(epochTime() / 1000)

proc handle_socket_request(client_socket: Socket, receipt_timestamp: int64) {.thread.} =
    try:
        var request_text = TaintedString""
        client_socket.readLine(request_text)

        var rc = HTTP_STATUS_BAD_REQUEST
        var disk_read_time_ms = 0
        var file_path = ""
        var tot_request_time_ms: int64 = 0

        if len(request_text) > 0:
            let start_processing_timestamp = current_time_millis()
            let queue_time_ms = start_processing_timestamp - receipt_timestamp
            let queue_time_secs = int(queue_time_ms) / 1000

            # has this request already timed out?
            if queue_time_secs >= QUEUE_TIMEOUT_SECS:
                echo("timeout (queue)")
                rc = HTTP_STATUS_TIMEOUT
            else:
                let line_tokens = split(request_text, ' ')
                if len(line_tokens) > 1:
                    let request_method = line_tokens[0]
                    let args_text = line_tokens[1]
                    let fields = split(args_text, ',')
                    if len(fields) == 3:
                        let first_field = fields[0]
                        let rc_as_string = first_field[1 .. len(first_field)]
                        let rc_input = parseInt(rc_as_string)
                        disk_read_time_ms = parseInt(fields[1])
                        file_path = fields[2]
                        simulated_file_read(disk_read_time_ms)
                        rc = HTTP_STATUS_OK

            # total request time is sum of time spent in queue and the
            # simulated disk read time
            tot_request_time_ms = queue_time_ms + disk_read_time_ms

        # construct response and send back to client
        let response_body = format("$1,$2", int(tot_request_time_ms), file_path)

        let response_headers = format("HTTP/1.1 $1\n" &
                                      "Server: $2\n" &
                                      "Content-Length: $3\n" &
                                      "Connection: close\n" &
                                      "\n",
                                      rc,
                                      SERVER_NAME,
                                      len(response_body))
        client_socket.send(response_headers)
        client_socket.send(response_body)
    finally:
        client_socket.close()

proc main(server_port: int) =
    let server_socket: Socket = newSocket()
    server_socket.setSockOpt(OptReuseAddr, true)
    server_socket.bindAddr(Port(server_port))
    server_socket.listen(LISTEN_BACKLOG)
    let startup_msg = format("server ($1) listening on port $2",
                             SERVER_NAME,
                             server_port)
    echo(startup_msg)

    try:
        while true:
            #var client_socket: Socket = newSocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, true)
            var client_socket: Socket = newSocket()
            accept(server_socket, client_socket)
            let receipt_timestamp = current_time_millis()
            spawn handle_socket_request(client_socket, receipt_timestamp)
    finally:
        server_socket.close()

main(SERVER_PORT)

