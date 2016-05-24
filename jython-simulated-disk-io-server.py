# jython - simulate occasional problematic (long blocking) requests
# to run: jython jython-simulated-disk-io-server.py


import string
import time
import socket
from threading import Thread


READ_TIMEOUT_SECS = 4

STATUS_OK = 0
STATUS_QUEUE_TIMEOUT = 1
STATUS_BAD_INPUT = 2


def simulated_file_read(elapsed_time_ms):
    time.sleep(elapsed_time_ms / 1000.0)  # seconds


def handle_socket_request(sock, receipt_timestamp):
    reader = sock.makefile('r')
    writer = sock.makefile('w')
    request_text = reader.readline()
    if request_text:
        start_processing_timestamp = time.time()
        queue_time_ms = start_processing_timestamp - receipt_timestamp
        queue_time_secs = queue_time_ms / 1000

        rc = STATUS_OK
        disk_read_time_ms = 0
        file_path = ''

        # has this request already timed out?
        if queue_time_secs >= READ_TIMEOUT_SECS:
            print("timeout (queue)")
            rc = STATUS_QUEUE_TIMEOUT
        else:
            fields = string.split(request_text, ',')
            if len(fields) == 3:
                rc = int(fields[0])
                disk_read_time_ms = long(fields[1])
                file_path = fields[2]
                simulated_file_read(disk_read_time_ms)
            else:
                rc = STATUS_BAD_INPUT

        # total request time is sum of time spent in queue and the
        # simulated disk read time
        tot_request_time_ms = queue_time_ms + disk_read_time_ms

        # construct response and send back to client
        read_resp_text = "%d,%d,%s" % \
            (rc, tot_request_time_ms, file_path)
        writer.write(read_resp_text)
        writer.flush()
    reader.close()
    writer.close()
    sock.close()


def main(server_port):
    server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server_socket.bind(('', server_port))
    server_socket.listen(100)
    print("server listening on port %d" % server_port)

    try:
        while True:
            sock, addr = server_socket.accept()
            receipt_timestamp = time.time()
            Thread(target=lambda: handle_socket_request(sock, receipt_timestamp)).start()
    except KeyboardInterrupt:
        pass  # exit


if __name__=='__main__':
    server_port = 7000
    main(server_port)

