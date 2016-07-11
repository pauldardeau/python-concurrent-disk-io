{ Pascal (fpc 3.0)
 to build: fpc HttpThreadsServer.pas
}

program HttpThreadsServer;

uses
    cthreads, classes, sockets, strutils, sysutils;

type
    PRequestRecord = ^TRequestRecord;
    TRequestRecord = record
        client_socket: longint;
        receipt_timestamp: integer;
    end;

Const
    SERVER_PORT = 7000;
    QUEUE_TIMEOUT_SECS = 4;
    LISTEN_BACKLOG = 500;
    BUFFER_SIZE = 1024;

    SERVER_NAME = 'HttpThreadsServer.pas';
    HTTP_STATUS_OK = '200 OK';
    HTTP_STATUS_TIMEOUT = '408 TIMEOUT';
    HTTP_STATUS_BAD_REQUEST = '400 BAD REQUEST';

{********************************************************************}

function CurrentTimeMillis(): integer;
begin
    CurrentTimeMillis := 0;
    { TODO: figure out how to get unix epoch time in millis }
end;

{********************************************************************}

procedure SimulatedFileRead(disk_read_time_ms: longint);
begin
    Sleep(disk_read_time_ms);
end;

{********************************************************************}

procedure HandleSocketRequest(client_socket: longint;
                              receipt_timestamp: integer);
var
    request_line: string;
    request_method: string;
    request_args: string;
    response_headers: string;
    response_body: string;
    file_path: string;
    start_processing_timestamp: integer;
    server_queue_time_ms: integer;
    server_queue_time_secs: integer;
    disk_read_time_ms: longint;
    tot_request_time_ms: longint;
    rc: string;
    request_text: string;
    bytes_read: longint;
    ms_per_second: integer;
    first_token: string;
    second_token: string;
    rc_input: longint;

begin
    request_text := '';
    { read request from client }
    bytes_read := fpRecv(client_socket, @request_text[1], BUFFER_SIZE, 0);

    rc := HTTP_STATUS_BAD_REQUEST;
    tot_request_time_ms := 0;
    file_path := '';

    { read from socket succeeded? }
    if bytes_read > 0 then
    begin
        SetLength(request_text, bytes_read);

        { determine how long the request has waited in queue }
        start_processing_timestamp := CurrentTimeMillis();
        server_queue_time_ms :=
            start_processing_timestamp - receipt_timestamp;
        ms_per_second := 1000;
        server_queue_time_secs :=
            server_queue_time_ms DIV ms_per_second;

        disk_read_time_ms := 0;

        { has this request already timed out? }
        if server_queue_time_secs >= QUEUE_TIMEOUT_SECS then
        begin
            WriteLn('timeout (queue)');
            rc := HTTP_STATUS_TIMEOUT;
        end
        else
        begin
            request_line := ExtractWord(1, request_text, [AnsiChar(#10)]);
            if Length(request_line) > 0 then
            begin
                request_method := ExtractWord(1, request_line, [' ']);
                request_args := ExtractWord(2, request_line, [' ']);
                if Length(request_args) > 0 then
                begin
                    Delete(request_args, 1, 1);
                    first_token := ExtractWord(1, request_args, [',']);
                    second_token := ExtractWord(2, request_args, [',']);
                    file_path := ExtractWord(3, request_args, [',']);

                    { strip off leading '/' }
                    first_token := MidStr(first_token, 2, Length(first_token)-1);

                    rc_input := StrToInt(first_token);
                    { rc := rc_input; }
                    disk_read_time_ms := StrToInt(second_token);
                    SimulatedFileRead(disk_read_time_ms);
                    rc := HTTP_STATUS_OK;
                end;
            end;
        end;

        { total request time is sum of time spent in queue and the
           simulated disk read time }
        tot_request_time_ms :=
            server_queue_time_ms + disk_read_time_ms;
    end;

    { create response text }
    response_body := IntToStr(tot_request_time_ms) +
                     ',' +
                     file_path;

    response_headers := 'HTTP/1.1 ' + rc + AnsiChar(#10) +
                        'Server :' + SERVER_NAME + AnsiChar(#10) +
                        'Content-Length: ' +
                            IntToStr(Length(response_body))+ AnsiChar(#10) +
                        'Connection: close' + AnsiChar(#10) +
                        AnsiChar(#10);

    { Writeln(rc); }

    { return response text to client }
    fpSend(client_socket,
           @response_headers[1],
           Length(response_headers),
           0);
    fpSend(client_socket,
           @response_body[1],
           Length(response_body),
           0);

    fpShutdown(client_socket, 2);

    { close client socket connection }
    CloseSocket(client_socket);
end;

{********************************************************************}

function ThreadHandler(p: pointer): ptrint;
var
    request_record: PRequestRecord;

begin
    request_record := PRequestRecord(p);
    HandleSocketRequest(request_record^.client_socket,
                        request_record^.receipt_timestamp);
    Dispose(request_record);
    ThreadHandler := 0;
end;

{********************************************************************}

var
    server_socket: longint;
    client_socket: longint;
    receipt_timestamp: integer;
    ServerAddr: TInetSockAddr;
    ClientAddr: TInetSockAddr;
    ClientAddrSize: LongInt;
    request_record: PRequestRecord;
    multithreaded: boolean;
    sock_opt_arg: integer;

begin
    multithreaded := true;
    server_socket := fpSocket(AF_INET, SOCK_STREAM, 0);
    if server_socket = -1 then
    begin
        WriteLn('error: unable to create server socket on port ', SERVER_PORT);
        Halt(1);
    end;

    sock_opt_arg := 1;
    fpSetSockOpt(server_socket,
                 SOL_SOCKET,
                 SO_REUSEADDR,
                 @sock_opt_arg,
                 SizeOf(sock_opt_arg));

    ServerAddr.sin_family := AF_INET;
    ServerAddr.sin_port := htons(SERVER_PORT);
    ServerAddr.sin_addr.s_addr := 0;

    if fpBind(server_socket,
              @ServerAddr,
              sizeof(ServerAddr)) <> 0 then
    begin
        WriteLn('error: unable to bind server socket');
        Halt(1);
    end;

    if fpListen(server_socket, LISTEN_BACKLOG) <> 0 then
    begin
        WriteLn('error: listen on socket server');
        Halt(1);
    end;

    WriteLn('server (' + SERVER_NAME + ') listening on port ',
            SERVER_PORT);

    repeat
        { wait for next socket connection from a client }
        client_socket := fpAccept(server_socket,
                                  @ClientAddr,
                                  @ClientAddrSize);

        if client_socket <> -1 then
        begin
            receipt_timestamp := CurrentTimeMillis();

            if multithreaded then
            begin
                New(request_record);
                request_record^.client_socket := client_socket;
                request_record^.receipt_timestamp := receipt_timestamp;
                BeginThread(@ThreadHandler, request_record);
            end
            else
                HandleSocketRequest(client_socket, receipt_timestamp);
        end;
    until false;
end.

