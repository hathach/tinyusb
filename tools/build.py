#!/usr/bin/env python3
import argparse
import random
import os
import sys
import time
import subprocess
from pathlib import Path
from multiprocessing import Pool

import build_utils

STATUS_OK = "\033[32mOK\033[0m"
STATUS_FAILED = "\033[31mFailed\033[0m"
STATUS_SKIPPED = "\033[33mSkipped\033[0m"

RET_OK = 0
RET_FAILED = 1
RET_SKIPPED = 2

build_format = '| {:30} | {:40} | {:16} | {:5} |'
build_separator = '-' * 95
build_status = [STATUS_OK, STATUS_FAILED, STATUS_SKIPPED]

verbose = False

# -----------------------------
# Helper
# -----------------------------
def run_cmd(cmd):
    #print(cmd)
    r = subprocess.run(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    title = f'Command Error: {cmd}'
    if r.returncode != 0:
        # print build output if failed
        if os.getenv('GITHUB_ACTIONS'):
            print(f"::group::{title}")
            print(r.stdout.decode("utf-8"))
            print(f"::endgroup::")
        else:
            print(title)
            print(r.stdout.decode("utf-8"))
    elif verbose:
        print(cmd)
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
        all_examples.append('host/device_info')
    all_examples.sort()
    return all_examples


def print_build_result(board, example, status, duration):
    if isinstance(duration, (int, float)):
        duration = "{:.2f}s".format(duration)
    print(build_format.format(board, example, build_status[status], duration))

# -----------------------------
# CMake
# -----------------------------
def cmake_board(board, toolchain, build_flags_on):
    ret = [0, 0, 0]
    start_time = time.monotonic()

    build_dir = f'cmake-build/cmake-build-{board}'
    build_flags = ''
    if len(build_flags_on) > 0:
        build_flags =  ' '.join(f'-D{flag}=1' for flag in build_flags_on)
        build_flags = f'-DCFLAGS_CLI="{build_flags}"'
        build_dir += '-f1_' + '_'.join(build_flags_on)

    family = find_family(board)
    if family == 'espressif':
        # for espressif, we have to build example individually
        all_examples = get_examples(family)
        for example in all_examples:
            if build_utils.skip_example(example, board):
                ret[2] += 1
            else:
                rcmd = run_cmd(f'cmake examples/{example} -B {build_dir}/{example} -G "Ninja" '
                               f'-DBOARD={board} {build_flags}')
                if rcmd.returncode == 0:
                    rcmd = run_cmd(f'cmake --build {build_dir}/{example}')
                ret[0 if rcmd.returncode == 0 else 1] += 1
    else:
        rcmd = run_cmd(f'cmake examples -B {build_dir} -G "Ninja" -DBOARD={board} -DCMAKE_BUILD_TYPE=MinSizeRel '
                       f'-DTOOLCHAIN={toolchain} {build_flags}')
        if rcmd.returncode == 0:
            cmd = f"cmake --build {build_dir}"
            # circleci docker return $nproc as 36 core, limit parallel according to resource class. Required for IAR, also prevent crashed/killed by docker
            if os.getenv('CIRCLECI'):
                resource_class = { 'small': 1, 'medium': 2, 'medium+': 3, 'large': 4 }
                for rc in resource_class:
                    if rc in os.getenv('CIRCLE_JOB'):
                        cmd += f' --parallel {resource_class[rc]}'
                        break
            rcmd = run_cmd(cmd)
        ret[0 if rcmd.returncode == 0 else 1] += 1

    example = 'all'
    print_build_result(board, example, 0 if ret[1] == 0 else 1, time.monotonic() - start_time)
    return ret


# -----------------------------
# Make
# -----------------------------
def make_one_example(example, board, make_option):
    # Check if board is skipped
    if build_utils.skip_example(example, board):
        print_build_result(board, example, 2, '-')
        r = 2
    else:
        start_time = time.monotonic()
        # skip -j for circleci
        if not os.getenv('CIRCLECI'):
            make_option += ' -j'
        make_cmd = f"make -C examples/{example} BOARD={board} {make_option}"
        # run_cmd(f"{make_cmd} clean")
        build_result = run_cmd(f"{make_cmd} all")
        r = 0 if build_result.returncode == 0 else 1
        print_build_result(board, example, r, time.monotonic() - start_time)

    ret = [0, 0, 0]
    ret[r] = 1
    return ret


def make_board(board, toolchain):
    print(build_separator)
    all_examples = get_examples(find_family(board))
    start_time = time.monotonic()
    ret = [0, 0, 0]
    with Pool(processes=os.cpu_count()) as pool:
        pool_args = list((map(lambda e, b=board, o=f"TOOLCHAIN={toolchain}": [e, b, o], all_examples)))
        r = pool.starmap(make_one_example, pool_args)
        # sum all element of same index (column sum)
        ret = list(map(sum, list(zip(*r))))
    example = 'all'
    print_build_result(board, example, 0 if ret[1] == 0 else 1, time.monotonic() - start_time)
    return ret


# -----------------------------
# Build Family
# -----------------------------
def build_boards_list(boards, toolchain, build_system, build_flags_on):
    ret = [0, 0, 0]
    for b in boards:
        r = [0, 0, 0]
        if build_system == 'cmake':
            r = cmake_board(b, toolchain, build_flags_on)
        elif build_system == 'make':
            r = make_board(b, toolchain)
        ret[0] += r[0]
        ret[1] += r[1]
        ret[2] += r[2]
    return ret


def build_family(family, toolchain, build_system, build_flags_on, one_per_family, boards):
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

    ret = build_boards_list(all_boards, toolchain, build_system, build_flags_on)
    return ret


# -----------------------------
# Main
# -----------------------------
def main():
    global verbose

    parser = argparse.ArgumentParser()
    parser.add_argument('families', nargs='*', default=[], help='Families to build')
    parser.add_argument('-b', '--board', action='append', default=[], help='Boards to build')
    parser.add_argument('-t', '--toolchain', default='gcc', help='Toolchain to use, default is gcc')
    parser.add_argument('-s', '--build-system', default='cmake', help='Build system to use, default is cmake')
    parser.add_argument('-f1', '--build-flags-on', action='append', default=[], help='Build flag to pass to build system')
    parser.add_argument('-1', '--one-per-family', action='store_true', default=False, help='Build only one random board inside a family')
    parser.add_argument('-v', '--verbose', action='store_true', help='Verbose output')
    args = parser.parse_args()

    families = args.families
    boards = args.board
    toolchain = args.toolchain
    build_system = args.build_system
    build_flags_on = args.build_flags_on
    one_per_family = args.one_per_family
    verbose = args.verbose

    if len(families) == 0 and len(boards) == 0:
        print("Please specify families or board to build")
        return 1

    print(build_separator)
    print(build_format.format('Board', 'Example', '\033[39mResult\033[0m', 'Time'))
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

    # succeeded, failed, skipped
    for f in all_families:
        r = build_family(f, toolchain, build_system, build_flags_on, one_per_family, boards)
        result[0] += r[0]
        result[1] += r[1]
        result[2] += r[2]

    # build boards
    r = build_boards_list(boards, toolchain, build_system, build_flags_on)
    result[0] += r[0]
    result[1] += r[1]
    result[2] += r[2]

    total_time = time.monotonic() - total_time
    print(build_separator)
    print(f"Build Summary: {result[0]} {STATUS_OK}, {result[1]} {STATUS_FAILED} and took {total_time:.2f}s")
    print(build_separator)
    return result[1]


if __name__ == '__main__':
    sys.exit(main())
