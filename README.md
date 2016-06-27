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

Intent
------
The **intent** of this project is **NOT** to give any opinions or
recommendations, but rather to take an objective look at the
problem and explore various alternatives.

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

Client
------
The client utility used for testing is 'make_requests.py'.

Implementations
---------------

| File                               | Language      | Problem? | Tool |
| ----                               | --------      | -------- | ---------- |
| SimulatedDiskIOServer.cs           | C#            | N        | Mono C# compiler version 4.2.1.0 |
| SimulatedDiskIOServer.d            | D             | N        | DMD64 D Compiler v2.069.2-devel |
| SimulatedDiskIOServer.java         | Java (JVM)    | N        | openjdk version 1.8.0 |
| SimulatedDiskIOServer.nim          | Nim           | **Y**    | Nim 0.11.2 |
| SimulatedDiskIOServer.scala        | Scala (JVM)   | N        | Scala compiler version 2.11.8 |
| jython-simulated-disk-io-server.py | Python (JVM)  | N        | Jython 2.7.0 |
| simulated-disk-io-server.c         | C             | N        | gcc 4.8.5 |
| simulated-disk-io-server.go        | Go            | N        | go version go1.2.1 linux/amd64 |
| simulated-disk-io-server.py        | Python        | **Y**    | Python 2.7 and eventlet |
| simulated-disk-io-server.rs        | Rust          | N        | Rust 1.9 |


