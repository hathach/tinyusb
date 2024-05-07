import os
import sys
import time
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


def build_family(example, family, make_option):
    all_boards = []
    for entry in os.scandir("hw/bsp/{}/boards".format(family)):
        if entry.is_dir() and entry.name != 'pico_sdk':
            all_boards.append(entry.name)
    filter_with_input(all_boards)
    all_boards.sort()

    with Pool(processes=os.cpu_count()) as pool:
        pool_args = list((map(lambda b, e=example, o=make_option: [e, b, o], all_boards)))
        result = pool.starmap(build_utils.build_example, pool_args)
        # sum all element of same index (column sum)
        return list(map(sum, list(zip(*result))))


if __name__ == '__main__':
    make_option = ''
    for a in sys.argv:
        if 'TOOLCHAIN=' in sys.argv:
            make_option += ' ' + a

    # If examples are not specified in arguments, build all
    all_examples = []
    for d in os.scandir("examples"):
        if d.is_dir() and 'cmake' not in d.name and 'build_system' not in d.name:
            for entry in os.scandir(d.path):
                if entry.is_dir() and 'cmake' not in entry.name:
                    all_examples.append(d.name + '/' + entry.name)
    filter_with_input(all_examples)
    all_examples.sort()

    # If family are not specified in arguments, build all
    all_families = []
    for entry in os.scandir("hw/bsp"):
        if entry.is_dir() and os.path.isdir(entry.path + "/boards") and entry.name != 'espressif':
            all_families.append(entry.name)
    filter_with_input(all_families)
    all_families.sort()

    print(build_separator)
    print(build_utils.build_format.format('Example', 'Board', '\033[39mResult\033[0m', 'Time', 'Flash', 'SRAM'))
    total_time = time.monotonic()

    # succeeded, failed, skipped
    total_result = [0, 0, 0]
    for example in all_examples:
        print(build_separator)
        for family in all_families:
            fret = build_family(example, family, make_option)
            if len(fret) == len(total_result):
                total_result = [total_result[i] + fret[i] for i in range(len(fret))]

    total_time = time.monotonic() - total_time
    print(build_separator)
    print("Build Summary: {} {}, {} {}, {} {} and took {:.2f}s".format(total_result[0], SUCCEEDED, total_result[1],
                                                                       FAILED, total_result[2], SKIPPED, total_time))
    print(build_separator)

    sys.exit(total_result[1])
