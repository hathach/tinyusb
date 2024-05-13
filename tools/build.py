import argparse
import random
import os
import sys
import time
import subprocess
from pathlib import Path
from multiprocessing import Pool

import build_utils

SUCCEEDED = "\033[32msucceeded\033[0m"
FAILED = "\033[31mfailed\033[0m"

build_separator = '-' * 106


def run_cmd(cmd):
    #print(cmd)
    r = subprocess.run(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    title = 'command error'
    if r.returncode != 0:
        # print build output if failed
        if os.getenv('CI'):
            print(f"::group::{title}")
            print(r.stdout.decode("utf-8"))
            print(f"::endgroup::")
        else:
            print(title)
            print(r.stdout.decode("utf-8"))
    return r

def find_family(board):
    bsp_dir = Path("hw/bsp")
    for family_dir in bsp_dir.iterdir():
        if family_dir.is_dir():
            board_dir = family_dir / 'boards' / board
            if board_dir.exists():
                return family_dir.name
    return None


def get_examples(family):
    all_examples = []
    for d in os.scandir("examples"):
        if d.is_dir() and 'cmake' not in d.name and 'build_system' not in d.name:
            for entry in os.scandir(d.path):
                if entry.is_dir() and 'cmake' not in entry.name:
                    if family != 'espressif' or 'freertos' in entry.name:
                        all_examples.append(d.name + '/' + entry.name)

    if family == 'espressif':
        all_examples.append('device/board_test')
        all_examples.append('device/video_capture')
    all_examples.sort()
    return all_examples


def build_board_cmake(board, toolchain):
    start_time = time.monotonic()
    ret = [0, 0, 0]

    build_dir = f"cmake-build/cmake-build-{board}"
    family = find_family(board)
    if family == 'espressif':
        # for espressif, we have to build example individually
        all_examples = get_examples(family)
        for example in all_examples:
            r = run_cmd(f'cmake examples/{example} -B {build_dir}/{example} -G "Ninja" -DBOARD={board} -DMAX3421_HOST=1')
            if r.returncode == 0:
                r = run_cmd(f'cmake --build {build_dir}/{example}')
            if r.returncode == 0:
                ret[0] += 1
            else:
                ret[1] += 1
    else:
        r = run_cmd(f'cmake examples -B {build_dir} -G "Ninja" -DBOARD={board} -DCMAKE_BUILD_TYPE=MinSizeRel -DTOOLCHAIN={toolchain}')
        if r.returncode == 0:
            r = run_cmd(f"cmake --build {build_dir}")
        if r.returncode == 0:
            ret[0] += 1
        else:
            ret[1] += 1

    duration = time.monotonic() - start_time

    if ret[1] == 0:
        status = SUCCEEDED
    else:
        status = FAILED

    flash_size = "-"
    sram_size = "-"
    example = 'all'
    title = build_utils.build_format.format(example, board, status, "{:.2f}s".format(duration), flash_size, sram_size)
    print(title)
    return ret


def build_board_make_all_examples(board, toolchain, all_examples):
    start_time = time.monotonic()
    ret = [0, 0, 0]

    with Pool(processes=os.cpu_count()) as pool:
        pool_args = list((map(lambda e, b=board, o=f"TOOLCHAIN={toolchain}": [e, b, o], all_examples)))
        r = pool.starmap(build_utils.build_example, pool_args)
        # sum all element of same index (column sum)
        rsum = list(map(sum, list(zip(*r))))
        ret[0] += rsum[0]
        ret[1] += rsum[1]
        ret[2] += rsum[2]
    duration = time.monotonic() - start_time
    if ret[1] == 0:
        status = SUCCEEDED
    else:
        status = FAILED

    flash_size = "-"
    sram_size = "-"
    example = 'all'
    title = build_utils.build_format.format(example, board, status, "{:.2f}s".format(duration), flash_size, sram_size)
    print(title)
    return ret


def build_family(family, toolchain, build_system, one_per_family, boards):
    all_boards = []
    for entry in os.scandir(f"hw/bsp/{family}/boards"):
        if entry.is_dir() and entry.name != 'pico_sdk':
            all_boards.append(entry.name)
    all_boards.sort()

    ret = [0, 0, 0]

    # If only-one flag is set, select one random board
    if one_per_family:
        for b in boards:
            # skip if -b already specify one in this family
            if find_family(b) == family:
                return ret
        all_boards = [random.choice(all_boards)]

    # success, failed, skipped
    all_examples = get_examples(family)
    for board in all_boards:
        r = [0, 0, 0]
        if build_system == 'cmake':
            r = build_board_cmake(board, toolchain)
        elif build_system == 'make':
            r = build_board_make_all_examples(board, toolchain, all_examples)
        ret[0] += r[0]
        ret[1] += r[1]
        ret[2] += r[2]

    return ret


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('families', nargs='*', default=[], help='Families to build')
    parser.add_argument('-b', '--board', action='append', default=[], help='Boards to build')
    parser.add_argument('-t', '--toolchain', default='gcc', help='Toolchain to use, default is gcc')
    parser.add_argument('-s', '--build-system', default='cmake', help='Build system to use, default is cmake')
    parser.add_argument('-1', '--one-per-family', action='store_true', default=False, help='Build only one random board inside a family')
    args = parser.parse_args()

    families = args.families
    boards = args.board
    toolchain = args.toolchain
    build_system = args.build_system
    one_per_family = args.one_per_family

    if len(families) == 0 and len(boards) == 0:
        print("Please specify families or board to build")
        return 1

    print(build_separator)
    print(build_utils.build_format.format('Example', 'Board', '\033[39mResult\033[0m', 'Time', 'Flash', 'SRAM'))
    total_time = time.monotonic()
    result = [0, 0, 0]

    # build families
    all_families = []
    if 'all' in families:
        for entry in os.scandir("hw/bsp"):
            if entry.is_dir() and entry.name != 'espressif' and os.path.isfile(entry.path + "/family.cmake"):
                all_families.append(entry.name)
    else:
        all_families = list(families)
    all_families.sort()

    # succeeded, failed
    for f in all_families:
        fret = build_family(f, toolchain, build_system, one_per_family, boards)
        result[0] += fret[0]
        result[1] += fret[1]
        result[2] += fret[2]

    # build boards
    for b in boards:
        r = [0, 0, 0]
        if build_system == 'cmake':
            r = build_board_cmake(b, toolchain)
        elif build_system == 'make':
            all_examples = get_examples(find_family(b))
            r = build_board_make_all_examples(b, toolchain, all_examples)
        result[0] += r[0]
        result[1] += r[1]
        result[2] += r[2]

    total_time = time.monotonic() - total_time
    print(build_separator)
    print(f"Build Summary: {result[0]} {SUCCEEDED}, {result[1]} {FAILED} and took {total_time:.2f}s")
    print(build_separator)
    return result[1]


if __name__ == '__main__':
    sys.exit(main())
