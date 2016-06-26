// D - simulate occasional problematic (long blocking) requests for disk IO
// to build: dmd SimulatedDiskIOServer.d

import core.thread;
import std.concurrency;
import std.conv;
import std.datetime;
import std.socket;
import std.stdio;
import std.string;


static int READ_TIMEOUT_SECS = 4;

static int STATUS_OK = 0;
static int STATUS_QUEUE_TIMEOUT = 1;
static int STATUS_BAD_INPUT = 2;


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

    // did we get anything from client?
    if (bytes_read > 0) {
        string request_text = to!string(buffer[0..bytes_read-1]);
        // determine how long the request has waited in queue
        sw.stop();
        long queue_time_ms = sw.peek().msecs;
        int queue_time_secs = to!int(queue_time_ms / 1000);

        int rc = STATUS_OK;
        long disk_read_time_ms = 0;
        string file_path = "";

        // has this request already timed out?
        if (queue_time_secs >= READ_TIMEOUT_SECS) {
            writefln("timeout (queue)");
            rc = STATUS_QUEUE_TIMEOUT;
        } else {
            string[] tokens = split(request_text, ",");
            if (tokens.length == 3) {
                rc = to!int(tokens[0]);
                disk_read_time_ms = to!long(tokens[1]);
                file_path = tokens[2];
                simulated_file_read(disk_read_time_ms);
            } else {
                rc = STATUS_BAD_INPUT;
            }
        }

        // total request time is sum of time spent in queue and the
        // simulated disk read time
        long tot_request_time_ms =
            queue_time_ms + disk_read_time_ms;

        // return response text to client
        string response_text = format("%d,%d,%s\n",
                                      rc,
                                      tot_request_time_ms,
                                      file_path);
        client_socket.send(response_text);
        client_socket.shutdown(SocketShutdown.BOTH);
        client_socket.close();
    }
}

void main(string[] args) {
    int server_port = 7000;

    Socket server_socket = new TcpSocket();
    server_socket.setOption(SocketOptionLevel.SOCKET,
                            SocketOption.REUSEADDR,
                            true);
    server_socket.bind(new InternetAddress("localhost",
                                           to!ushort(server_port)));
    server_socket.listen(5);

    writefln(format("server listening on port %d",
                    server_port));

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

