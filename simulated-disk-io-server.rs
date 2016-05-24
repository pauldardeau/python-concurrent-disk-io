// rust - simulate occasional problematic (long blocking) requests for disk IO
// language version: 1.5.0
// NOTE: does not compile yet
// to build: rustc simulated-disk-io-server.rs

use std::io::BufferedStream;
use std::io::net::ip::SocketAddr;
use std::io::{TcpListener, TcpStream};
use std::io::{Listener, Writer, Acceptor, Buffer};
use std::process;
use std::task::spawn;
use std::thread;
use std::time::Duration;

const READ_TIMEOUT_SECS: i32 = 4;
const STATUS_OK: i32 = 0;
const STATUS_QUEUE_TIMEOUT: i32 = 1;
const STATUS_BAD_INPUT: i32 = 2;

fn current_time_millis() -> i64 {
    //TODO: time::now()
    return 0;
}

fn simulated_file_read(disk_read_time_ms: i64) {
    thread::sleep(Duration::from_millis(disk_read_time_ms));
}

fn handle_socket_request(stream: &mut BufferedStream<TcpStream>,
                         receipt_timestamp: i64) {
    // read request from client
    let request_text = stream.read_line().unwrap();

    // read from socket succeeded?
    if errs == nil {
        // did we get anything from client?
        if request_text.trim().len() > 0 {
            // determine how long the request has waited in queue
            let start_processing_timestamp = current_time_millis();
            let server_queue_time_ms =
                start_processing_timestamp - receipt_timestamp;
            let server_queue_time_secs = server_queue_time_ms / 1000;

            let mut rc = STATUS_OK;
            let mut disk_read_time_ms = i64(0);
            let mut file_path = "";

            // has this request already timed out?
            if server_queue_time_secs >= READ_TIMEOUT_SECS {
                println!("timeout (queue)");
                rc = STATUS_QUEUE_TIMEOUT;
            } else {
                let tokens = strings.Split(request_text, ",");
                if len(tokens) == 3 {
                    result = tokens[0].parse::<int>();
                    if errs == nil {
                        rc_input = result.unwrap();
                        result =
                            tokens[1].parse::<int>();
                        if errs == nil {
                            req_response_time_ms = result.unwrap();
                            rc = rc_input;
                            disk_read_time_ms = req_response_time_ms;
                            file_path = tokens[2];
                            simulated_file_read(disk_read_time_ms);
                        }
                    }
                }
            }

            // total request time is sum of time spent in queue and the
            // simulated disk read time
            let tot_request_time_ms =
                server_queue_time_ms + disk_read_time_ms;

            // create response text
            let response_text = format!("{0},{1},{2}",
                rc,
                tot_request_time_ms,
                file_path);

            // return response text to client
            stream.write_str(response_text);
            stream.flush();
        }
    }

    // close client socket connection
    //TODO: not needed in Rust?
    //stream.close();
}

fn main() {
    let server_port = 7000;
    let address: SocketAddr  = from_str("0.0.0.0:7000").unwrap();
    let listener = TcpListener::bind(&address).unwrap();
    let mut acceptor = listener.listen().unwrap();

    if err != nil {
        println!("error: unable to create server socket on port {0}",
                 server_port);
        println!("{0}", err);
        process::exit(1);
    } else {
        println!("server listening on port {0}", server_port);

        loop {
            // wait for next socket connection from a client
            match acceptor.accept() {
                Err(_) => println!("error listen"),
                Ok(mut stream) => {
                    let receipt_timestamp = current_time_millis();
                    //TODO: run request on separate thread
                    //spawn(proc() {
                        let mut stream = BufferedStream::new(stream);
                        handle_socket_request(stream, receipt_timestamp);
                    //});
                }
            }
        }
        process::exit(0);
    }
}

