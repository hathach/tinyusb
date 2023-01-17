import os
import sys
import time
import subprocess
from multiprocessing import Pool

import build_utils

SUCCEEDED = "\033[32msucceeded\033[0m"
FAILED = "\033[31mfailed\033[0m"
SKIPPED = "\033[33mskipped\033[0m"

build_separator = '-' * 106


def filter_with_input(mylist):
    if len(sys.argv) > 1:
        input_args = list(set(mylist).intersection(sys.argv))
        if len(input_args) > 0:
            mylist[:] = input_args


if __name__ == '__main__':
    # If examples are not specified in arguments, build all
    all_examples = []
    for dir1 in os.scandir("examples"):
        if dir1.is_dir():
            for entry in os.scandir(dir1.path):
                if entry.is_dir():
                    all_examples.append(dir1.name + '/' + entry.name)
    filter_with_input(all_examples)
    all_examples.sort()

    # If boards are not specified in arguments, build all
    all_boards = []
    for entry in os.scandir("hw/bsp"):
        if entry.is_dir() and os.path.exists(entry.path + "/board.mk"):
            all_boards.append(entry.name)
    filter_with_input(all_boards)
    all_boards.sort()

    # Get dependencies
    for b in all_boards:
        subprocess.run("make -C examples/device/board_test BOARD={} get-deps".format(b), shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)

    print(build_separator)
    print(build_utils.build_format.format('Example', 'Board', '\033[39mResult\033[0m', 'Time', 'Flash', 'SRAM'))
    total_time = time.monotonic()

    # succeeded, failed, skipped
    total_result = [0, 0, 0]
    for example in all_examples:
        print(build_separator)
        with Pool(processes=os.cpu_count()) as pool:
            pool_args = list((map(lambda b, e=example, o='': [e, b, o], all_boards)))
            result = pool.starmap(build_utils.build_example, pool_args)
            # sum all element of same index (column sum)
            result = list(map(sum, list(zip(*result))))
            
            # add to total result
            total_result = list(map(lambda x, y: x + y, total_result, result))

    total_time = time.monotonic() - total_time
    print(build_separator)
    print("Build Summary: {} {}, {} {}, {} {} and took {:.2f}s".format(total_result[0], SUCCEEDED, total_result[1],
                                                                       FAILED, total_result[2], SKIPPED, total_time))
    print(build_separator)

    sys.exit(total_result[1])
