# python
# language version: 2.7

import eventlet
from eventlet.green import socket
import string
import time


SERVER_PORT = 7000
QUEUE_TIMEOUT_SECS = 4
LISTEN_BACKLOG = 500

SERVER_NAME = 'http-eventlet-server.py'
HTTP_STATUS_OK = '200 OK'
HTTP_STATUS_TIMEOUT = '408 TIMEOUT'
HTTP_STATUS_BAD_REQUEST = '400 BAD REQUEST'


def simulated_file_read(elapsed_time_ms):
    time.sleep(elapsed_time_ms / 1000.0)  # seconds


def handle_socket_request(sock, receipt_timestamp):
    reader = sock.makefile('r')
    writer = sock.makefile('w')
    request_text = reader.readline()

    rc = HTTP_STATUS_BAD_REQUEST
    tot_read_time_ms = 0
    file_path = ''

    if request_text:
        start_processing_timestamp = time.time()
        queue_time_ms = start_processing_timestamp - receipt_timestamp
        queue_time_secs = queue_time_ms / 1000

        # has this request already timed out?
        if queue_time_secs >= QUEUE_TIMEOUT_SECS:
            print("timeout (queue)")
            rc = HTTP_STATUS_TIMEOUT
        else:
            line_tokens = string.split(request_text, ' ')
            if len(line_tokens) > 1:
                request_method = line_tokens[0]
                args = line_tokens[1]
                fields = string.split(args, ',')
                if len(fields) == 3:
                    #rc = int(fields[0])
                    disk_read_time_ms = long(fields[1])
                    file_path = fields[2]
                    simulated_file_read(disk_read_time_ms)
                    rc = HTTP_STATUS_OK

        # total request time is sum of time spent in queue and the
        # simulated disk read time
        tot_request_time_ms = queue_time_ms + disk_read_time_ms

    # construct response and send back to client
    response_body = "%d,%s" % \
        (tot_request_time_ms, file_path)

    response_headers = 'HTTP/1.1 %s\n' % rc
    response_headers += 'Server: %s\n' % SERVER_NAME
    response_headers += 'Content-Length: %d\n' % len(response_body)
    response_headers += 'Connection: close\n'
    response_headers += '\n'

    writer.write(response_headers)
    writer.write(response_body)
    writer.flush()

    reader.close()
    writer.close()
    sock.close()


def main(server_port):
    server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    server_socket.bind(('', server_port))
    server_socket.listen(LISTEN_BACKLOG)
    print("server (%s) listening on port %d" % (SERVER_NAME, server_port))

    try:
        while True:
            sock, addr = server_socket.accept()
            receipt_timestamp = time.time()
            eventlet.spawn(handle_socket_request, sock, receipt_timestamp)
    except KeyboardInterrupt:
        pass  # exit


if __name__=='__main__':
    main(SERVER_PORT)

