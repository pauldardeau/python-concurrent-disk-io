#!/bin/sh
#ab -t 60 -n 1000 -c 400 http://127.0.0.1:7000/200,200,some_file
siege -b -r 10 -c 100 http://127.0.0.1:7000/200,200,some_file
