import os
import sys
import time


def main(timeout_secs, server_port, iteration_count, file_name):
    for i in range(iteration_count):
        start_time_secs = time.time()
        cmd = 'nc localhost %d < file_list.txt' % server_port
        rc = os.system(cmd)
        if rc != 0:
            sys.exit(1)
        else:
            end_time_secs = time.time()
            total_time_secs = end_time_secs - start_time_secs
            if total_time_secs > timeout_secs:
                print('***client timeout %d' % total_time_secs)


if __name__=='__main__':
    main(5, 7000, 1000, 'some_file')

