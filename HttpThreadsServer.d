// D
// to build: dmd HttpThreadsServer.d

import core.thread;
import std.concurrency;
import std.conv;
import std.datetime;
import std.socket;
import std.stdio;
import std.string;


static int SERVER_PORT = 7000;
static int QUEUE_TIMEOUT_SECS = 4;
static int LISTEN_BACKLOG = 500;

static string SERVER_NAME = "HttpThreadsServer.d";
static string HTTP_STATUS_OK = "200 OK";
static string HTTP_STATUS_TIMEOUT = "408 TIMEOUT";
static string HTTP_STATUS_BAD_REQUEST = "400 BAD REQUEST";


static void simulated_file_read(long disk_read_time_ms) {
    Thread.sleep(dur!("msecs")(disk_read_time_ms));
}

static void handle_socket_request(shared Socket shared_client_socket,
                                  shared StopWatch shared_sw) {
    Socket client_socket = cast(Socket) shared_client_socket;
    StopWatch sw = cast(StopWatch) shared_sw;

    // read request from client
    char[1024] buffer;
    auto bytes_read = client_socket.receive(buffer);
    string rc = HTTP_STATUS_BAD_REQUEST;
    string file_path = "";
    long tot_request_time_ms = 0;

    // did we get anything from client?
    if (bytes_read > 0) {
        string request_text = to!string(buffer[0..bytes_read-1]);
        // determine how long the request has waited in queue
        sw.stop();
        long queue_time_ms = sw.peek().msecs;
        int queue_time_secs = to!int(queue_time_ms / 1000);

        long disk_read_time_ms = 0;

        // has this request already timed out?
        if (queue_time_secs >= QUEUE_TIMEOUT_SECS) {
            writefln("timeout (queue)");
            rc = HTTP_STATUS_TIMEOUT;
        } else {
            string[] request_lines = split(request_text, "\n");
            if (request_lines.length > 0) {
                string request_line = request_lines[0];
                string[] line_tokens = split(request_line, " ");
                if (line_tokens.length > 1) {
                    string request_method = line_tokens[0];
                    string request_args = line_tokens[1];
                    string[] fields = split(request_args, ",");
                    if (fields.length == 3) {
                        //rc = to!int(fields[0]);
                        disk_read_time_ms = to!long(fields[1]);
                        file_path = fields[2];
                        simulated_file_read(disk_read_time_ms);
                        rc = HTTP_STATUS_OK;
                    }
                }
            }
        }

        // total request time is sum of time spent in queue and the
        // simulated disk read time
        tot_request_time_ms =
            queue_time_ms + disk_read_time_ms;
    }

    // return response to client
    string response_body = format("%d,%s",
                                  tot_request_time_ms,
                                  file_path);
    string response_headers =
        format("HTTP/1.1 %s\n"
               "Server: %s\n"
               "Content-Length: %d\n"
               "Connection: close\n"
               "\n",
               rc,
               SERVER_NAME,
               response_body.length);

    client_socket.send(response_headers);
    client_socket.send(response_body);
    client_socket.shutdown(SocketShutdown.BOTH);
    client_socket.close();
}

void main(string[] args) {
    Socket server_socket = new TcpSocket();
    server_socket.setOption(SocketOptionLevel.SOCKET,
                            SocketOption.REUSEADDR,
                            true);
    server_socket.bind(new InternetAddress(to!ushort(SERVER_PORT)));
    server_socket.listen(LISTEN_BACKLOG);

    writefln(format("server (%s) listening on port %d",
                    SERVER_NAME,
                    SERVER_PORT));

    while (true) {
        Socket client_socket = server_socket.accept();
        if (client_socket !is null) {
            StopWatch sw;
            sw.start();
            spawn(&handle_socket_request,
                  cast(shared) client_socket,
                  cast(shared) sw);
        }
    }
}

