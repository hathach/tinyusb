import os
import sys
import time
import subprocess
import pathlib
from multiprocessing import Pool

import build_utils

SUCCEEDED = "\033[32msucceeded\033[0m"
FAILED = "\033[31mfailed\033[0m"
SKIPPED = "\033[33mskipped\033[0m"

build_separator = '-' * 106

make_iar_option = 'CC=iccarm'

def filter_with_input(mylist):
    if len(sys.argv) > 1:
        input_args = list(set(mylist).intersection(sys.argv))
        if len(input_args) > 0:
            mylist[:] = input_args


def build_family(family, make_option):
    all_boards = []
    for entry in os.scandir("hw/bsp/{}/boards".format(family)):
        if entry.is_dir() and entry.name != 'pico_sdk':
            all_boards.append(entry.name)
    all_boards.sort()

    # success, failed, skipped
    ret = [0, 0, 0]
    for board in all_boards:
        start_time = time.monotonic()

        build_dir = f"cmake-build/cmake-build-{board}"

        # Generate build
        r = subprocess.run(f"cmake examples -B {build_dir} -G \"Ninja\" -DFAMILY={family} -DBOARD"
                           f"={board}", shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)

        # Build
        if r.returncode == 0:
            r = subprocess.run(f"cmake --build {build_dir}", shell=True, stdout=subprocess.PIPE,
                               stderr=subprocess.STDOUT)

        duration = time.monotonic() - start_time

        if r.returncode == 0:
            status = SUCCEEDED
            ret[0] += 1
        else:
            status = FAILED
            ret[1] += 1

        flash_size = "-"
        sram_size = "-"
        example = 'all'
        print(build_utils.build_format.format(example, board, status, "{:.2f}s".format(duration), flash_size, sram_size))

        if r.returncode != 0:
            # group output in CI
            print(f"::group::{board} build error")
            print(r.stdout.decode("utf-8"))
            print(f"::endgroup::")

    return ret


if __name__ == '__main__':
    # IAR CC
    if make_iar_option not in sys.argv:
        make_iar_option = ''

    # If family are not specified in arguments, build all supported
    all_families = []
    for entry in os.scandir("hw/bsp"):
        if entry.is_dir() and entry.name != 'espressif' and os.path.isfile(entry.path + "/family.cmake"):
            all_families.append(entry.name)
    filter_with_input(all_families)
    all_families.sort()

    print(build_separator)
    print(build_utils.build_format.format('Example', 'Board', '\033[39mResult\033[0m', 'Time', 'Flash', 'SRAM'))
    total_time = time.monotonic()

    # succeeded, failed, skipped
    total_result = [0, 0, 0]
    for family in all_families:
        fret = build_family(family, make_iar_option)
        if len(fret) == len(total_result):
            total_result = [total_result[i] + fret[i] for i in range(len(fret))]

    total_time = time.monotonic() - total_time
    print(build_separator)
    print("Build Summary: {} {}, {} {}, {} {} and took {:.2f}s".format(total_result[0], SUCCEEDED, total_result[1],
                                                                       FAILED, total_result[2], SKIPPED, total_time))
    print(build_separator)

    sys.exit(total_result[1])
