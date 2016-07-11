// Scala
// to build: scalac HttpThreadsServer.scala
// to run: scala HttpThreadsServer

import java.net._
import java.io._
import java.text._
import java.util._


object HttpThreadsServer {

    val SERVER_PORT = 7000
    val QUEUE_TIMEOUT_SECS = 4
    val LISTEN_BACKLOG = 500

    val SERVER_NAME = "HttpThreadsServer.scala"
    val HTTP_STATUS_OK = "200 OK"
    val HTTP_STATUS_TIMEOUT = "408 TIMEOUT"
    val HTTP_STATUS_BAD_REQUEST = "400 BAD REQUEST"

    def current_time_millis(): Long = {
        //TODO: implement current_time_millis
        return 0
    }

    def simulated_file_read(disk_read_time_ms: Long) = {
        Thread.sleep(disk_read_time_ms)
    }

    def handle_socket_request(sock: Socket,
                              receipt_timestamp: Long) = {
        var reader: BufferedReader = null
        var writer: BufferedWriter = null
        var rc: String = HTTP_STATUS_BAD_REQUEST
        var file_path = ""
        var tot_request_time_ms: Long = 0

        try {
            reader =
                new BufferedReader(new InputStreamReader(sock.getInputStream()))
            writer =
                new BufferedWriter(new OutputStreamWriter(sock.getOutputStream()))

            // read request from client
            val request_text: String = reader.readLine()

            // did we get anything from client?
            if ((request_text != null) && (request_text.length() > 0)) {
                // determine how long the request has waited in queue
                val start_processing_timestamp: Long = current_time_millis()
                val queue_time_ms: Long = start_processing_timestamp -
                    receipt_timestamp
                val x: Long = queue_time_ms / 1000
                val queue_time_secs: Int = x.asInstanceOf[Int]

                var disk_read_time_ms: Long = 0

                // has this request already timed out?
                if (queue_time_secs >= QUEUE_TIMEOUT_SECS) {
                    println("timeout (queue)")
                    rc = HTTP_STATUS_TIMEOUT
                } else {
                    val stRequest = new StringTokenizer(request_text, " ")
                    if (stRequest.countTokens() > 1) {
                        val request_method = stRequest.nextToken()
                        val request_args = stRequest.nextToken()
                        val stArgs = new StringTokenizer(request_args, ",")
                        if (stArgs.countTokens() == 3) {
                            try {
                                var rc_as_string = stArgs.nextToken();
                                rc_as_string = rc_as_string.substring(1);
                                val rc_input =
                                    Integer.parseInt(rc_as_string)
                                disk_read_time_ms = stArgs.nextToken().toLong
                                file_path = stArgs.nextToken()
                                simulated_file_read(disk_read_time_ms)
                                rc = HTTP_STATUS_OK
                            } finally {
                                // ignore it
                            }
                        }
                    }
                }

                // total request time is sum of time spent in queue and the
                // simulated disk read time
                tot_request_time_ms =
                    queue_time_ms + disk_read_time_ms
            }

            // return response to client
            val response_body = "" +
                                tot_request_time_ms +
                                "," +
                                file_path

            val response_headers = "HTTP/1.1 " + rc + "\n" +
                                   "Server: " + SERVER_NAME + "\n" +
                                   "Content-Length: " +
                                       response_body.length() + "\n" +
                                   "Connection: close\n" +
                                   "\n"

            writer.write(response_headers)
            writer.write(response_body)
            writer.flush()
        } finally {
            if (reader != null) {
                reader.close()
            }
            if (writer != null) {
                writer.close()
            }

            // close client socket connection
            sock.close()
        }
    }

    def main(args: Array[String]) = {
        val server_socket = new ServerSocket(SERVER_PORT, LISTEN_BACKLOG)
        server_socket.setReuseAddress(true)

        println("server (" +
                SERVER_NAME +
                ") listening on port " +
                SERVER_PORT)

        while (true) {
            val sock: Socket = server_socket.accept()
            if (sock != null) {
                val receipt_timestamp: Long = current_time_millis()
                val thread = new Thread {
                    override def run() {
                        handle_socket_request(sock, receipt_timestamp)
                    }
                }
                thread.start
            }
        }
    }
}

