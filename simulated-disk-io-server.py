# simulate occasional problematic (long blocking) requests within eventlet

import eventlet
from eventlet.green import socket
import random
import time


NUM_GREEN_THREADS = 5

READ_TIMEOUT = 4

# status codes to indicate whether read succeeded, timed out, or failed
STATUS_READ_SUCCESS = 0
STATUS_READ_TIMEOUT = 1
STATUS_READ_IO_FAIL = 2

# percentage of requests that result in very long response times (many seconds)
PCT_LONG_IO_RESPONSE_TIMES = 0.10

# percentage of requests that result in IO failure
PCT_IO_FAIL = 0.001

# max size that would be read
MAX_READ_BYTES = 100000

# min/max values for io failure
MIN_TIME_FOR_IO_FAIL = 0.3
MAX_TIME_FOR_IO_FAIL = 3.0

# min/max values for slow read (dying disk)
MIN_TIME_FOR_SLOW_IO = 6.0
MAX_TIME_FOR_SLOW_IO = 20.0

# min/max values for normal io
MIN_TIME_FOR_NORMAL_IO = 0.075
MAX_TIME_FOR_NORMAL_IO = 0.4

# maximum ADDITIONAL time above timeout experienced by requests that time out
MAX_TIME_ABOVE_TIMEOUT = MAX_TIME_FOR_SLOW_IO * 0.8


def random_value_between(min_value,max_value):
    rand_value = random.random() * max_value
    if rand_value < min_value:
        rand_value = min_value
    return rand_value


def simulated_file_read(file_path, read_timeout):
    start_time = time.time()
    num_bytes_read = 0
    rand_number = random.random()
    request_with_slow_read = False

    if rand_number <= PCT_IO_FAIL:
        # simulate read io failure
        rc = STATUS_READ_IO_FAIL
        elapsed_time = random_value_between(MIN_TIME_FOR_IO_FAIL, MAX_TIME_FOR_IO_FAIL)
    else:
        rc = STATUS_READ_SUCCESS
        if rand_number <= PCT_LONG_IO_RESPONSE_TIMES:
            # simulate very slow request
            request_with_slow_read = True
            elapsed_time = random_value_between(MIN_TIME_FOR_SLOW_IO, MAX_TIME_FOR_SLOW_IO)
        else:
            # simulate typical read response time
            elapsed_time = random_value_between(MIN_TIME_FOR_NORMAL_IO, MAX_TIME_FOR_NORMAL_IO)
        num_bytes_read = int(random.random() * MAX_READ_BYTES)

    if elapsed_time > read_timeout and not request_with_slow_read:
        rc = STATUS_READ_TIMEOUT
        elapsed_time = random_value_between(0, MAX_TIME_ABOVE_TIMEOUT)
        num_bytes_read = 0

    time.sleep(elapsed_time)

    end_time = time.time()

    if rc == STATUS_READ_TIMEOUT:
        print("timeout (assigned)")
    elif rc == STATUS_READ_IO_FAIL:
        print("io fail")
    elif rc == STATUS_READ_SUCCESS:
        # what would otherwise have been a successful read turns into a
        # timeout error if simulated execution time exceeds timeout value
        exec_time = end_time - start_time
        if exec_time > read_timeout:
            #TODO: it would be nice to increment a counter here and show
            # the counter value as part of print
            print("timeout (service)")
            rc = STATUS_READ_TIMEOUT
            num_bytes_read = 0
            elapsed_time = exec_time

    return (rc, num_bytes_read, elapsed_time)


def handle_socket_request(sock):
    reader = sock.makefile('r')
    writer = sock.makefile('w')
    file_path = reader.readline()
    if file_path:
        read_resp = simulated_file_read(file_path, READ_TIMEOUT)
        read_resp_text = "%d,%d,%f,%s" % (read_resp[0], read_resp[1], read_resp[2], file_path)
        writer.write(read_resp_text+'\n')
        writer.flush()
    reader.close()
    writer.close()
    sock.close()


def main(server_port):
    server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server_socket.bind(('', server_port))
    server_socket.listen(100)
    print("server listening on port %d" % server_port)

    while True:
        sock, addr = server_socket.accept()
        eventlet.spawn(handle_socket_request, sock)


if __name__=='__main__':
    server_port = 6000
    main(server_port)

