// Java - simulate occasional problematic (long blocking) requests for disk IO
// to build: javac HttpThreadedSimulatedDiskIOServer.java
// to run: java HttpThreadedSimulatedDiskIOServer

import java.net.*;
import java.io.*;
import java.text.*;
import java.util.*;


public class HttpThreadedSimulatedDiskIOServer {

    public static final int READ_TIMEOUT_SECS = 4;

    public static final String SERVER_NAME = "HttpThreadedSimulatedDiskIOServer.java";
    public static final String HTTP_STATUS_OK = "200 OK";
    public static final String HTTP_STATUS_TIMEOUT = "408 TIMEOUT";
    public static final String HTTP_STATUS_BAD_REQUEST = "400 BAD REQUEST";


    public static void simulated_file_read(long disk_read_time_ms) {
        try {
            Thread.sleep(disk_read_time_ms);
        } catch (InterruptedException ignored) {
        }
    }

    public static void handle_socket_request(Socket sock,
                                             long receipt_timestamp) {
        BufferedReader reader = null;
        BufferedWriter writer = null;

        try {
            reader =
                new BufferedReader(new InputStreamReader(sock.getInputStream()));
            writer =
                new BufferedWriter(new OutputStreamWriter(sock.getOutputStream()));

            // read request from client
            String request_text = reader.readLine();

            // did we get anything from client?
            if ((request_text != null) && (request_text.length() > 0)) {
                // determine how long the request has waited in queue
                final long start_processing_timestamp = System.currentTimeMillis();
                final long queue_time_ms = start_processing_timestamp -
                receipt_timestamp;
                final int queue_time_secs = (int) (queue_time_ms / 1000);

                String rc = HTTP_STATUS_OK;
                long disk_read_time_ms = 0;
                String file_path = "";

                // has this request already timed out?
                if (queue_time_secs >= READ_TIMEOUT_SECS) {
                System.out.println("timeout (queue)");
                    rc = HTTP_STATUS_TIMEOUT;
                } else {
                    StringTokenizer stRequestLine = new StringTokenizer(request_text, " ");
                    if (stRequestLine.countTokens() > 1) { 
                        // skip over the first one (http verb)
                        stRequestLine.nextToken();
                        StringTokenizer st = new StringTokenizer(stRequestLine.nextToken(),",");
                        if (st.countTokens() == 3) {
                            int rc_input = Integer.parseInt(st.nextToken());
                            disk_read_time_ms = Long.parseLong(st.nextToken());
                            file_path = st.nextToken();
                            simulated_file_read(disk_read_time_ms);
                        } else {
                            rc = HTTP_STATUS_BAD_REQUEST;
                        }
                    } else {
                        rc = HTTP_STATUS_BAD_REQUEST;
                    }
                }

                // total request time is sum of time spent in queue and the
                // simulated disk read time
                final long tot_request_time_ms =
                    queue_time_ms + disk_read_time_ms;

                // return response to client
                String response_body = "" + tot_request_time_ms + "," + file_path;

                String response_headers = "HTTP/1.1 " + rc + "\n" +
                                          "Server: " + SERVER_NAME + "\n" +
                                          "Content-Length: " + response_body.length() + "\n" +
                                          "\n";
                writer.write(response_headers);
                writer.write(response_body);
                writer.flush();
            }
        } catch (IOException e) {
            e.printStackTrace();
        } finally {
            if (reader != null) {
                try {
                    reader.close();
                } catch (IOException ignored) {
                }
            }
            if (writer != null) {
                try { 
                    writer.close();
                } catch (IOException ignored) {
                }
            }

            // close client socket connection
            try {
                sock.close();
            } catch (IOException ignored) {
            }
        }
    }

    public static void main(String[] args) {
        int server_port = 7000;
        try {
            ServerSocket server_socket = new ServerSocket(server_port);
            System.out.println("server listening on port " + server_port);

            while (true) {
                try {
                    final Socket sock = server_socket.accept();
                    if (sock != null) {
                        final long receipt_timestamp = System.currentTimeMillis();
                        new Thread(new Runnable() {
                            @Override
                            public void run() {
                                handle_socket_request(sock, receipt_timestamp);
                            }
                        }).start();
                    }
                } catch (IOException ignored) {
                }
            }
        } catch (IOException e) {
            System.out.println("error: unable to create server socket on port " + server_port);
        }
    }

}

