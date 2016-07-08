#!/bin/sh
ab -n 500 -c 8 127.0.0.1:7000/200,4,some_file
