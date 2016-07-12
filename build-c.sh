#!/bin/sh
gcc -O2 -Wall http-threads-server.c -o http-threads-server -lm -lpthread
