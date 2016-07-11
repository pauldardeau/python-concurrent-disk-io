# python functionality for use with various python servers

import socket
import time


QUEUE_TIMEOUT_SECS = 4

HTTP_STATUS_OK = '200 OK'
HTTP_STATUS_TIMEOUT = '408 TIMEOUT'
HTTP_STATUS_BAD_REQUEST = '400 BAD REQUEST'


def simulated_file_read(elapsed_time_ms):
    time.sleep(elapsed_time_ms / 1000.0)  # seconds


def handle_request(request_text, server_name, receipt_timestamp):

    rc = HTTP_STATUS_BAD_REQUEST
    tot_request_time_ms = 0
    file_path = ''

    if len(request_text) > 0:
        start_processing_timestamp = time.time()
        queue_time_ms = start_processing_timestamp - receipt_timestamp
        queue_time_secs = queue_time_ms / 1000

        # has this request already timed out?
        if queue_time_secs >= QUEUE_TIMEOUT_SECS:
            print("timeout (queue)")
            rc = HTTP_STATUS_TIMEOUT
        else:
            line_tokens = request_text.split(' ')
            if len(line_tokens) > 1:
                request_method = line_tokens[0]
                args = line_tokens[1]
                fields = args.split(',')
                if len(fields) == 3:
                    #first_field = fields[0].strip('/')
                    #rc = int(first_field)
                    disk_read_time_ms = int(fields[1])
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
    response_headers += "Server: %s\n" % server_name
    response_headers += "Content-Length: %d\n" % len(response_body)
    response_headers += "Connection: close\n"
    response_headers += "\n"

    return (response_headers, response_body)


def handle_socket_request(sock, server_name, receipt_timestamp):
    reader = sock.makefile('r')
    writer = sock.makefile('w')
    request_text = reader.readline()
    reader.close()

    headers, body = handle_request(request_text,
                                   server_name,
                                   receipt_timestamp)

    writer.write(headers)
    writer.write(body)
    writer.flush()

    writer.close()
    sock.close()

