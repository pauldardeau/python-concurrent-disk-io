#!/bin/sh
ab -n 1000 -c 100 127.0.0.1:7000/200,4,some_file
