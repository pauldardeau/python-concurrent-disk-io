# python
# language version: 2.7

import eventlet
from eventlet.green import socket
import string
import time
import httpworker


SERVER_PORT = 7000
LISTEN_BACKLOG = 500
SERVER_NAME = 'http-eventlet-server.py'


def main(server_port):
    server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    server_socket.bind(('', server_port))
    server_socket.listen(LISTEN_BACKLOG)
    print("server (%s) listening on port %d" % (SERVER_NAME, server_port))

    try:
        while True:
            sock, addr = server_socket.accept()
            eventlet.spawn(httpworker.handle_socket_request,
                           sock,
                           SERVER_NAME,
                           time.time())
    except KeyboardInterrupt:
        pass  # exit


if __name__=='__main__':
    main(SERVER_PORT)

