#!/bin/bash
for i in {1..1000}
do
   nc localhost 6000 < file_list.txt
   if [ $? -ne 0 ]; then
      break
   fi
done
