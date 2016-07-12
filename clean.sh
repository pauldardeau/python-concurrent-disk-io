#!/bin/sh
rm -f http-server
rm -f HttpThreadsServer
rm -f http-threads-server
rm -f *.o
rm -f *.class
rm -f *.pyc
rm -f *.exe
rm -rf nimcache/*
if [ -d nimcache ] ; then
   rmdir nimcache
fi

