# nim - simulate occasional problematic (long blocking) requests
# to build: nim c --threads:on --d:release SimulatedDiskIOServer.nim

# helpful link: http://goran.krampe.se/2014/10/25/nim-socketserver/


import net
import os
import strutils
import threadpool
import times

const server_port = 7000
const STATUS_OK = 0
const STATUS_QUEUE_TIMEOUT = 1
const STATUS_BAD_INPUT = 2

proc simulated_file_read(elapsed_time_ms: int64) =
    sleep(int(elapsed_time_ms))

proc current_time_millis(): int64 =
    return int64(epochTime() / 1000)

proc handle_socket_request(client_socket: Socket, receipt_timestamp: int64) {.thread.} =
    try:
        var request_text = TaintedString""
        client_socket.readLine(request_text)

        if len(request_text) > 0:
            let start_processing_timestamp = current_time_millis()
            let queue_time_ms = start_processing_timestamp - receipt_timestamp
            let queue_time_secs = int(queue_time_ms) / 1000

            var rc = STATUS_OK
            var disk_read_time_ms = 0
            var file_path = ""

            # has this request already timed out?
            if queue_time_secs >= 4:
                echo("timeout (queue)")
                rc = STATUS_QUEUE_TIMEOUT
            else:
                let fields = split(request_text, ',')
                if len(fields) == 3:
                    rc = parseInt(fields[0])
                    disk_read_time_ms = parseInt(fields[1])
                    file_path = fields[2]
                    simulated_file_read(disk_read_time_ms)
                else:
                    rc = STATUS_BAD_INPUT

            # total request time is sum of time spent in queue and the
            # simulated disk read time
            let tot_request_time_ms = queue_time_ms + disk_read_time_ms

            # construct response and send back to client
            let response_text = format("$1,$2,$3\n", rc, int(tot_request_time_ms), file_path)
            client_socket.send(response_text)
    finally:
        client_socket.close()

proc main(server_port: int) =
    let server_socket: Socket = newSocket()
    server_socket.setSockOpt(OptReuseAddr, true)
    server_socket.bindAddr(Port(server_port), "127.0.0.1")
    server_socket.listen()
    echo("server listening on port " & $server_port)

    try:
        while true:
            var client_socket: Socket = Socket()
            accept(server_socket, client_socket)
            let receipt_timestamp = current_time_millis()
            spawn handle_socket_request(client_socket, receipt_timestamp)
    finally:
        server_socket.close()

main(server_port)

