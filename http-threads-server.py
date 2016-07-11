# python
# to run: python http-threads-server.py


import string
import time
import socket
from threading import Thread
import signal
import sys
import httpworker


SERVER_PORT = 7000
LISTEN_BACKLOG = 500
SERVER_NAME = 'http-threads-server.py'


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
    print("server (%s) listening on port %d" % (SERVER_NAME, server_port))

    try:
        while True:
            try:
                sock, addr = server_socket.accept()
            except socket.error:
                continue
            Thread(target=lambda: httpworker.handle_socket_request(sock,
                                                                   SERVER_NAME,
                                                                   time.time())).start()
    except KeyboardInterrupt:
        sys.exit(0)


if __name__=='__main__':
    signal.signal(signal.SIGINT, exit_handler)
    signal.signal(signal.SIGPIPE, do_nothing_handler)
    main(SERVER_PORT)

