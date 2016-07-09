// Scala - simulate occasional problematic (long blocking) requests for disk IO
// to build: scalac HttpThreadedSimulatedDiskIOServer.scala
// to run: scala HttpThreadedSimulatedDiskIOServer

import java.net._
import java.io._
import java.text._
import java.util._


object HttpThreadedSimulatedDiskIOServer {

    val READ_TIMEOUT_SECS = 4
    val LISTEN_BACKLOG = 500
    val HTTP_STATUS_OK = 200
    val HTTP_STATUS_TIMEOUT = 408
    val HTTP_STATUS_BAD_REQUEST = 400

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
        var rc: HTTP_STATUS_OK
        var file_path = ""
        val tot_request_time_ms: Long = 0

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
                if (queue_time_secs >= READ_TIMEOUT_SECS) {
                    println("timeout (queue)")
                    rc = HTTP_STATUS_TIMEOUT
                } else {
                    val st = new StringTokenizer(request_text,",")
                    if (st.countTokens() == 3) {
                        rc = Integer.parseInt(st.nextToken())
                        disk_read_time_ms = st.nextToken().toLong
                        file_path = st.nextToken()
                        simulated_file_read(disk_read_time_ms)
                    }
                }

                // total request time is sum of time spent in queue and the
                // simulated disk read time
                tot_request_time_ms: Long =
                    queue_time_ms + disk_read_time_ms
            } else {
                rc = HTTP_BAD_REQUEST
                tot_request_time_ms = 0
            }

            // return response to client
            writer.write("" + rc + "," +
                         tot_request_time_ms + "," +
                         file_path + "\n")
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
        val server_port = 7000
        //TODO: set listen backlog?
        val server_socket = new ServerSocket(server_port)
        println("server listening on port " + server_port)

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

