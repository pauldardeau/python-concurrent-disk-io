// C# - developed using mono
// install tools: sudo apt-get install mono-complete
// to build: mcs HttpThreadsServer.cs
// to run: mono HttpThreadsServer.exe

using System;
using System.IO;
using System.Net;
using System.Net.Sockets;
using System.Text;
using System.Threading;


namespace diskioserver {


public class HttpThreadsServer {

public static int SERVER_PORT = 7000;
public static int QUEUE_TIMEOUT_SECS = 4;
public static int LISTEN_BACKLOG = 500;

public static string SERVER_NAME = "HttpThreadsServer.cs";
public static string HTTP_STATUS_OK = "200 OK";
public static string HTTP_STATUS_TIMEOUT = "408 TIMEOUT";
public static string HTTP_STATUS_BAD_REQUEST = "400 BAD REQUEST";


public static long current_time_millis() {
    return DateTime.Now.Ticks / TimeSpan.TicksPerMillisecond;
}

public static void simulated_file_read(long disk_read_time_ms) {
    Thread.Sleep((int)disk_read_time_ms);
}

public static void handle_socket_request(Socket client_socket,
                                         long receipt_timestamp) {

    try {
        byte[] buffer = new Byte[1024];
        // read request from client
        int bytes_read = client_socket.Receive(buffer);

        // read request from client
        string request_text =
            Encoding.ASCII.GetString(buffer, 0, bytes_read);

        string rc = HTTP_STATUS_BAD_REQUEST;
        long tot_request_time_ms = 0;
        string file_path = "";

        // did we get anything from client?
        if ((request_text != null) && (request_text.Length > 0)) {
            // determine how long the request has waited in queue
            long start_processing_timestamp = current_time_millis();
            long queue_time_ms = start_processing_timestamp -
                receipt_timestamp;
            int queue_time_secs = (int) (queue_time_ms / 1000);

            long disk_read_time_ms = 0;

            // has this request already timed out?
            if (queue_time_secs >= QUEUE_TIMEOUT_SECS) {
                Console.WriteLine("timeout (queue)");
                rc = HTTP_STATUS_TIMEOUT;
            } else {
                string[] input_lines = request_text.Split('\n');
                if (input_lines.Length > 0) {
                    string request_line = input_lines[0];
                    string[] line_tokens = request_line.Split(' ');
                    if (line_tokens.Length > 1) {
                        string args = line_tokens[1];
                        string[] fields = args.Split(',');
                        if (fields.Length == 3) {
                            string rc_as_string = fields[0];
                            // strip off leading '/'
                            rc_as_string = rc_as_string.Substring(1);
                            string service_time_as_string = fields[1];
                            int rc_input = Convert.ToInt32(rc_as_string);
                            disk_read_time_ms =
                                Convert.ToInt64(service_time_as_string);
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
        string response_body = "" + tot_request_time_ms + "," + file_path;

        string response_headers = "HTTP/1.1 " + rc + "\n";
        response_headers +=  "Server: " + SERVER_NAME + "\n";
        response_headers += "Content-Length: " + response_body.Length + "\n";
        response_headers += "Connection: close\n";
        response_headers += "\n";

        //Console.WriteLine(rc);

        byte[] raw_response_headers =
            Encoding.ASCII.GetBytes(response_headers);
        client_socket.Send(raw_response_headers);

        byte[] raw_response_body = Encoding.ASCII.GetBytes(response_body);
        client_socket.Send(raw_response_body);
        client_socket.Shutdown(SocketShutdown.Both);
    } catch (Exception) {
        Console.WriteLine("exception caught");
    } finally {
        // close client socket connection
        try {
            client_socket.Close();
        } catch (Exception) {
        }
    }
}

public static void Main(string[] args) {
    try {
        IPEndPoint localEndPoint = new IPEndPoint(IPAddress.Loopback,
                                                  SERVER_PORT);
        Socket server_socket =
            new Socket(IPAddress.Loopback.AddressFamily,
                       SocketType.Stream,
                       ProtocolType.Tcp);
        server_socket.SetSocketOption(SocketOptionLevel.Socket,
                                      SocketOptionName.ReuseAddress,
                                      true);
        server_socket.Bind(localEndPoint);
        server_socket.Listen(LISTEN_BACKLOG);

        Console.WriteLine("server (" +
                          SERVER_NAME +
                          ") listening on port " +
                          SERVER_PORT);

        while (true) {
            try {
                Socket client_socket = server_socket.Accept();
                if (client_socket != null) {
                    long receipt_timestamp = current_time_millis();
                    Thread thread =
                        new Thread(() => handle_socket_request(client_socket,
                                                               receipt_timestamp));
                    thread.Start();
                }
            } catch (Exception) {
            }
        }
    } catch (Exception) {
        Console.WriteLine("error: unable to create server socket on port " +
                          SERVER_PORT);
    }
}

}

}
