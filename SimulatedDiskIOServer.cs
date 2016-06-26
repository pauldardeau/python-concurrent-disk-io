// C# - simulate occasional problematic (long blocking) requests for disk IO
// to build: mcs SimulatedDiskIOServer.cs
// to run: mono SimulatedDiskIOServer.exe

using System;
using System.IO;
using System.Net.Sockets;
using System.Text;
using System.Threading;


namespace diskioserver {


public class SimulatedDiskIOServer {

public static int READ_TIMEOUT_SECS = 4;

public static int STATUS_OK = 0;
public static int STATUS_QUEUE_TIMEOUT = 1;
public static int STATUS_BAD_INPUT = 2;


public static long current_time_millis() {
    return DateTime.Now.Ticks / TimeSpan.TicksPerMillisecond;
}

public static void simulated_file_read(long disk_read_time_ms) {
    Thread.Sleep((int)disk_read_time_ms);
}

public static void handle_socket_request(TcpClient client_socket,
                                         long receipt_timestamp) {
    StreamReader reader = null;
    StreamWriter writer = null;

    try {
        reader = new StreamReader(client_socket.GetStream(), Encoding.UTF8);
        writer = new StreamWriter(client_socket.GetStream());

        // read request from client
        string request_text = reader.ReadLine();

        // did we get anything from client?
        if ((request_text != null) && (request_text.Length > 0)) {
            // determine how long the request has waited in queue
            long start_processing_timestamp = current_time_millis();
            long queue_time_ms = start_processing_timestamp -
                receipt_timestamp;
            int queue_time_secs = (int) (queue_time_ms / 1000);

            int rc = STATUS_OK;
            long disk_read_time_ms = 0;
            string file_path = "";

            // has this request already timed out?
            if (queue_time_secs >= READ_TIMEOUT_SECS) {
                Console.WriteLine("timeout (queue)");
                rc = STATUS_QUEUE_TIMEOUT;
            } else {
                string[] tokens = request_text.Split(',');
                if (tokens.Length == 3) {
                    rc = Convert.ToInt32(tokens[0]);
                    disk_read_time_ms = Convert.ToInt64(tokens[1]);
                    file_path = tokens[2];
                    simulated_file_read(disk_read_time_ms);
                }
            }

            // total request time is sum of time spent in queue and the
            // simulated disk read time
            long tot_request_time_ms =
                queue_time_ms + disk_read_time_ms;

            // return response text to client
            writer.Write("" + rc + "," +
                         tot_request_time_ms + "," +
                         file_path + "\n");
            writer.Flush();
        }
    } catch (Exception e) {
        //e.printStackTrace();
    } finally {
        if (reader != null) {
            reader.Close();
        }

        if (writer != null) {
            writer.Close();
        }

        // close client socket connection
        try {
            client_socket.Close();
        } catch (Exception ignored) {
        }
    }
}

public static void Main(string[] args) {
    int server_port = 7000;
    try {
        TcpListener server_socket = new TcpListener(server_port);
        server_socket.Start();
        Console.WriteLine("server listening on port " + server_port);

        while (true) {
            try {
                TcpClient client_socket = default(TcpClient);
                client_socket = server_socket.AcceptTcpClient();
                if (client_socket != null) {
                    long receipt_timestamp = current_time_millis();
                    Thread thread =
                        new Thread(() => handle_socket_request(client_socket,
                                                               receipt_timestamp));
                    thread.Start();
                }
            } catch (Exception ignored) {
            }
        }
    } catch (Exception e) {
        Console.WriteLine("error: unable to create server socket on port " + server_port);
    }
}

}

}
