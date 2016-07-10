// Java
// install tools:
//    sudo add-apt-repository ppa:openjdk-r/ppa
//    sudo apt-get update
//    sudo apt-get install openjdk-8-jdk
// to build: javac HttpThreadsServer.java
// to run: java HttpThreadsServer

import java.net.*;
import java.io.*;
import java.text.*;
import java.util.*;


public class HttpThreadsServer {

    public static final int SERVER_PORT = 7000;
    public static final int QUEUE_TIMEOUT_SECS = 4;
    public static final int LISTEN_BACKLOG = 500;

    public static final String SERVER_NAME = "HttpThreadsServer.java";
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
            String rc = HTTP_STATUS_BAD_REQUEST;
            String file_path = "";
            long tot_request_time_ms = 0;

            // did we get anything from client?
            if ((request_text != null) && (request_text.length() > 0)) {
                // determine how long the request has waited in queue
                final long start_processing_timestamp = System.currentTimeMillis();
                final long queue_time_ms = start_processing_timestamp -
                receipt_timestamp;
                final int queue_time_secs = (int) (queue_time_ms / 1000);

                long disk_read_time_ms = 0;

                // has this request already timed out?
                if (queue_time_secs >= QUEUE_TIMEOUT_SECS) {
                    System.out.println("timeout (queue)");
                    rc = HTTP_STATUS_TIMEOUT;
                } else {
                    StringTokenizer stRequestLine = new StringTokenizer(request_text, " ");
                    if (stRequestLine.countTokens() > 1) { 
                        // skip over the first one (http verb)
                        stRequestLine.nextToken();
                        StringTokenizer stArgs =
                            new StringTokenizer(stRequestLine.nextToken(),",");
                        if (stArgs.countTokens() == 3) {
                            String rc_as_string = stArgs.nextToken();
                            if (rc_as_string.startsWith("/")) {
                                rc_as_string = rc_as_string.substring(1);
                            }
                            int rc_input = 0;
                            try {
                                Integer.parseInt(rc_as_string);
                                disk_read_time_ms =
                                    Long.parseLong(stArgs.nextToken());
                                file_path = stArgs.nextToken();
                                simulated_file_read(disk_read_time_ms);
                                rc = HTTP_STATUS_OK;
                            } catch (Throwable t) {
                                rc = HTTP_STATUS_BAD_REQUEST;
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
            String response_body = "" + tot_request_time_ms + "," + file_path;

            String response_headers = "HTTP/1.1 " + rc + "\n" +
                                      "Server: " + SERVER_NAME + "\n" +
                                      "Content-Length: " + response_body.length() + "\n" +
                                      "Connection: close\n" +
                                      "\n";
            writer.write(response_headers);
            writer.write(response_body);
            writer.flush();
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
        try {
            ServerSocket server_socket =
                new ServerSocket(SERVER_PORT, LISTEN_BACKLOG);
            server_socket.setReuseAddress(true);
            System.out.println("server (" +
                               SERVER_NAME +
                               ") listening on port " +
                               SERVER_PORT);

            while (true) {
                try {
                    final Socket sock = server_socket.accept();
                    if (sock != null) {
                        final long receipt_timestamp =
                            System.currentTimeMillis();
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
            System.out.println("error: unable to create server socket on port " + SERVER_PORT);
        }
    }

}

