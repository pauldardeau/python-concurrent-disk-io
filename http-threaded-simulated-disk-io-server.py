# python threads - simulate occasional problematic (long blocking) requests
# to run: python http-threaded-simulated-disk-io-server.py


import string
import time
import socket
from threading import Thread
import signal
import sys


READ_TIMEOUT_SECS = 4
LISTEN_BACKLOG = 500

SERVER_NAME = 'http-threaded-simulated-disk-io-server.py'
HTTP_STATUS_OK = '200 OK'
HTTP_STATUS_TIMEOUT = '408 TIMEOUT'
HTTP_STATUS_BAD_REQUEST = '400 BAD REQUEST'


def simulated_file_read(elapsed_time_ms):
    time.sleep(elapsed_time_ms / 1000.0)  # seconds


def handle_socket_request(sock, receipt_timestamp):
    request_text = ''
    try:
        buffer_text = sock.recv(1024)
        if buffer_text is not None and len(buffer_text) > 0:
            request_text = buffer_text
    except socket.error:
        request_text = ''
    
    if len(request_text) > 0:
        request_text = request_text.rstrip()
        start_processing_timestamp = time.time()
        queue_time_ms = start_processing_timestamp - receipt_timestamp
        queue_time_secs = queue_time_ms / 1000

        rc = HTTP_STATUS_OK
        disk_read_time_ms = 0
        file_path = ''

        # has this request already timed out?
        if queue_time_secs >= READ_TIMEOUT_SECS:
            print("timeout (queue)")
            rc = HTTP_STATUS_TIMEOUT
        else:
            line_tokens = string.split(request_text, ' ')
            if len(line_tokens) > 1:
                request_method = line_tokens[0]
                args_text = line_tokens[1]
                fields = string.split(args_text, ',')
                if len(fields) == 3:
                    first_field = fields[0].strip('/')
                    #rc = int(first_field)
                    disk_read_time_ms = long(fields[1])
                    file_path = fields[2]
                    simulated_file_read(disk_read_time_ms)
                else:
                    rc = HTTP_STATUS_BAD_REQUEST
            else:
                rc = HTTP_STATUS_BAD_REQUEST

        # total request time is sum of time spent in queue and the
        # simulated disk read time
        tot_request_time_ms = queue_time_ms + disk_read_time_ms
    else:
        rc = HTTP_STATUS_BAD_REQUEST
        tot_request_time_ms = 0
        file_path = ''

    # construct response and send back to client
    response_body = "%d,%s" % \
        (tot_request_time_ms, file_path)

    response_headers = 'HTTP/1.1 %s\n' % rc
    response_headers += "%s\n" % SERVER_NAME
    response_headers += "Content-Length: %d\n" % len(response_body)
    response_headers += "Connection: close\n"
    response_headers += "\n"

    try:
        sock.send(response_headers)
        sock.send(response_body)
    except socket.error:
        pass

    try:
        sock.close()
    except socket.error:
        pass


def do_nothing_handler(sig, dummy):
    pass


def exit_handler(sig, dummy):
    global server_socket
    server_socket.shutdown(socket.SHUT_RDWR)
    sys.exit(0)


def main(server_port):
    global server_socket
    server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    server_socket.bind(('', server_port))
    server_socket.listen(LISTEN_BACKLOG)
    print("server listening on port %d" % server_port)

    try:
        while True:
            try:
                sock, addr = server_socket.accept()
            except socket.error:
                continue
            receipt_timestamp = time.time()
            Thread(target=lambda: handle_socket_request(sock, receipt_timestamp)).start()
    except KeyboardInterrupt:
        sys.exit(0)


if __name__=='__main__':
    signal.signal(signal.SIGINT, exit_handler)
    signal.signal(signal.SIGPIPE, do_nothing_handler)
    server_port = 7000
    main(server_port)

