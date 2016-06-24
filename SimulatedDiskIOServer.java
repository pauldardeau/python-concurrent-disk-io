// simulate occasional problematic (long blocking) requests for disk IO

import java.net.*;
import java.io.*;
import java.util.*;


class ReadResponse {
    public int rc;
    public int bytes_read;
    public long elapsed_time_ms;
    public String file_path;

    public ReadResponse(int rc,
                        int bytes_read,
                        long elapsed_time_ms,
                        String file_path) {
        this.rc = rc;
        this.bytes_read = bytes_read;
        this.elapsed_time_ms = elapsed_time_ms;
        this.file_path = file_path;
    }

    public void addQueueTime(long queue_time_ms) {
        this.elapsed_time_ms += queue_time_ms;
    }

    public String toString() {
        return "" + rc + "," + bytes_read + "," + elapsed_time_ms + "," + file_path;
    }
}


public class SimulatedDiskIOServer {

public static int NUM_WORKER_THREADS = 5;

public static int READ_TIMEOUT_SECS = 4;

public static int STATUS_READ_SUCCESS  = 0;
public static int STATUS_READ_TIMEOUT  = 1;
public static int STATUS_READ_IO_FAIL  = 2;
public static int STATUS_QUEUE_TIMEOUT = 3;

// percentage of requests that result in very long response times (many seconds)
public static double PCT_LONG_IO_RESPONSE_TIMES = 0.10;

// percentage of requests that result in IO failure
public static double PCT_IO_FAIL = 0.001;

// max size that would be read
public static double MAX_READ_BYTES = 100000;

// min/max values for io failure
public static double MIN_TIME_FOR_IO_FAIL = 0.3;
public static double MAX_TIME_FOR_IO_FAIL = 3.0;

// min/max values for slow read (dying disk)
public static double MIN_TIME_FOR_SLOW_IO = 6.0;
public static double MAX_TIME_FOR_SLOW_IO = 20.0;

// min/max values for normal io
public static double MIN_TIME_FOR_NORMAL_IO = 0.075;
public static double MAX_TIME_FOR_NORMAL_IO = 0.4;

// maximum ADDITIONAL time above timeout experienced by requests that time out
public static double MAX_TIME_ABOVE_TIMEOUT = MAX_TIME_FOR_SLOW_IO * 0.8;

private static Random randomGenerator = new Random();


public static double random_value_between(double min_value, double max_value) {
    double rand_value = randomGenerator.nextDouble() * max_value;
    if (rand_value < min_value) {
        rand_value = min_value;
    } 
    return rand_value;
}


public static ReadResponse simulated_file_read(String file_path,
                                               long elapsed_time_ms,
                                               int read_timeout_secs) {

    final long read_timeout_ms = read_timeout_secs * 1000;
    int num_bytes_read = 0;
    double rand_number = randomGenerator.nextDouble();
    boolean request_with_slow_read = false;
    int rc;
    double min_response_time;
    double max_response_time;

    if (rand_number <= PCT_IO_FAIL) {
        // simulate read io failure
        rc = STATUS_READ_IO_FAIL;
        min_response_time = MIN_TIME_FOR_IO_FAIL;
        max_response_time = MAX_TIME_FOR_IO_FAIL;
    } else {
        rc = STATUS_READ_SUCCESS;
        if (rand_number <= PCT_LONG_IO_RESPONSE_TIMES) {
            // simulate very slow request
            request_with_slow_read = true;
            min_response_time = MIN_TIME_FOR_SLOW_IO;
            max_response_time = MAX_TIME_FOR_SLOW_IO;
        } else {
            // simulate typical read response time
            min_response_time = MIN_TIME_FOR_NORMAL_IO;
            max_response_time = MAX_TIME_FOR_NORMAL_IO;
        }
        num_bytes_read = (int) (randomGenerator.nextDouble() * MAX_READ_BYTES);
    }

    // do we need to generate the response time? (i.e., not predetermined)
    if (-1 == elapsed_time_ms) {
        elapsed_time_ms =
            (long) (1000.0 * random_value_between(min_response_time,
                                                  max_response_time));

        if (elapsed_time_ms > read_timeout_ms && !request_with_slow_read) {
            rc = STATUS_READ_TIMEOUT;
            elapsed_time_ms =
                (long) (1000.0 * random_value_between(0,
                                                      MAX_TIME_ABOVE_TIMEOUT));
            num_bytes_read = 0;
        }
    }

    // ***********  simulate the disk read  ***********
    try {
        Thread.sleep(elapsed_time_ms);
    } catch (InterruptedException ignored) {
    }

    if (rc == STATUS_READ_TIMEOUT) {
        System.out.println("timeout (assigned)");
    } else if (rc == STATUS_READ_IO_FAIL) {
        System.out.println("io fail");
    } else if (rc == STATUS_READ_SUCCESS) {
        // what would otherwise have been a successful read turns into a
        // timeout error if simulated execution time exceeds timeout value
        if (elapsed_time_ms > read_timeout_ms) {
            //TODO: it would be nice to increment a counter here and show
            // the counter value as part of print
            System.out.println("timeout (service)");
            rc = STATUS_READ_TIMEOUT;
            num_bytes_read = 0;
        }
    }

    return new ReadResponse(rc, num_bytes_read, elapsed_time_ms, file_path);
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

            // unless otherwise specified, let server generate
            // the response time
            long predetermined_response_time_ms = -1;
            String file_path;

            // look for field delimiter in request
            int pos_field_delimiter = request_text.indexOf(",");
            if (pos_field_delimiter > -1) {
                file_path = request_text.substring(pos_field_delimiter + 1);
                final int num_digits = pos_field_delimiter;
                if ((num_digits > 0) && (num_digits < 10)) {
                    String req_time_digits = request_text.substring(0, pos_field_delimiter);
                    final long req_response_time_ms = Long.parseLong(req_time_digits);
                    if (req_response_time_ms > 0) {
                        predetermined_response_time_ms = req_response_time_ms;
                    }
                }
            } else {
                file_path = request_text;
            }

            ReadResponse read_resp;

            // has this request already timed out?
            if (queue_time_secs >= READ_TIMEOUT_SECS) {
                System.out.println("timeout (queue)");
                read_resp = new ReadResponse(STATUS_QUEUE_TIMEOUT,
                                             0,  // bytes read
                                             0,  // elapsed time ms
                                             file_path);
            } else {
                // *********  perform simulated read of disk file  *********
                read_resp =
                    simulated_file_read(file_path,
                                        predetermined_response_time_ms,
                                        READ_TIMEOUT_SECS);
            }

            // total request time is sum of time spent in queue and the
            // simulated disk read time
            read_resp.addQueueTime(queue_time_ms);

            // return response text to client
            writer.write(read_resp.toString() + "\n");
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
    int server_port = 6000;
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

