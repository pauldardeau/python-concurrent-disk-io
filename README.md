# python-concurrent-disk-io

Project Focus (problem definition)
----------------------------------
The focus of this project is to explore server functionality that
receives a request from a TCP socket and then simulates a blocking
IO disk read and then returns a reply to the client. Ideally, the
server should support multiple concurrent requests and service
the requests in parallel.

Motivation/Background
---------------------
The motivation for this project comes from OpenStack Swift where
the 'object server' (written in Python and making use of 'eventlet')
could occasionally hang due to dying disk drives that cause excessive
read times (measured in seconds as opposed to milliseconds).

Baseline
--------
The baseline (reference) for this project is the eventlet-based
Python implementation (simulated-disk-io-server.py).

Alternatives Explored
---------------------
The goal of this project is to demonstrate the problem in a concise
and understandable manner and to explore various alternatives (whether
they be eventlet alternatives in Python or even different programming
languages).

Implementations
---------------

| File                               | Language      | Problem? | Tool |
| ----                               | --------      | -------- | ---------- |
| SimulatedDiskIOServer.cs           | C#            | N        | mono |
| SimulatedDiskIOServer.d            | D             | N        | dmd |
| SimulatedDiskIOServer.java         | Java          | N        | |
| SimulatedDiskIOServer.nim          | Nim           | **Y**    | |
| jython-simulated-disk-io-server.py | Python/Jython | N        | |
| simulated-disk-io-server.c         | C             | N        | |
| simulated-disk-io-server.go        | Go            | N        | |
| simulated-disk-io-server.py        | Python        | **Y**    | Python 2.7 |
| simulated-disk-io-server.rs        | Rust          | N        | Rust 1.9 |


