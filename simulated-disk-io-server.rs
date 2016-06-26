// rust - simulate occasional problematic (long blocking) requests for disk IO
// language version: 1.9.0
// to build: rustc simulated-disk-io-server.rs
// NOTE: this is my first Rust program, so much of it is probably crappy

use std::io::Read;
use std::io::Write;
use std::net::Shutdown;
use std::net::{TcpListener, TcpStream};
use std::str;
use std::thread;
use std::time::Duration;

const READ_TIMEOUT_SECS: u32 = 4;
const STATUS_OK: i32 = 0;
const STATUS_QUEUE_TIMEOUT: i32 = 1;
//const STATUS_BAD_INPUT: i32 = 2;

fn current_time_millis() -> u64 {
    //TODO: time::now()
    return 0;
}

fn simulated_file_read(disk_read_time_ms: u64) {
    thread::sleep(Duration::from_millis(disk_read_time_ms));
}

fn handle_socket_request(stream: &mut TcpStream,
                         receipt_timestamp: u64) {
    // read request from client
    let mut request_buffer = [0; 512];
    if stream.read(&mut request_buffer).unwrap() > 0 {
        let request_text = str::from_utf8(&request_buffer).unwrap();
        // did we get anything from client?
        if request_text.trim().len() > 0 {
            // determine how long the request has waited in queue
            let start_processing_timestamp = current_time_millis();
            let server_queue_time_ms =
                start_processing_timestamp - receipt_timestamp;
            let x = server_queue_time_ms / 1000;
            let server_queue_time_secs = x as u32;

            let mut rc = STATUS_OK;
            let mut disk_read_time_ms: u64 = 0;
            let mut file_path = "";

            // has this request already timed out?
            if server_queue_time_secs >= READ_TIMEOUT_SECS {
                println!("timeout (queue)");
                rc = STATUS_QUEUE_TIMEOUT;
            } else {
                let tokens: Vec<&str> = request_text.split(',').collect();
                if tokens.len() == 3 {
                    let parse_rc_result = tokens[0].parse();
                    match parse_rc_result {
                        Err(_) => {},
                        Ok(rc_input) => {
                            let parse_time_result = tokens[1].parse();
                            match parse_time_result {
                                Err(_) => {},
                                Ok(req_response_time_ms) => {
                                    rc = rc_input;
                                    disk_read_time_ms = req_response_time_ms;
                                    file_path = tokens[2];
                                    simulated_file_read(disk_read_time_ms);
                                }
                            }
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
            stream.write_all(response_text.as_bytes()).unwrap();
        }
    }

    let _ = stream.shutdown(Shutdown::Both);
}

fn main() {
    let server_port = 7000;
    //TODO: build string using server_port
    let listener = TcpListener::bind("127.0.0.1:7000").unwrap();
    println!("server listening on port {0}", server_port);

    for stream in listener.incoming() {
        match stream {
            Err(_) => {},
            Ok(mut stream) => {
                let receipt_timestamp = current_time_millis();
                thread::spawn(move|| {
                    handle_socket_request(&mut stream, receipt_timestamp);
                });
            }
        }
    }
}

