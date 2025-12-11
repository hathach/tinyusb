#!/usr/bin/env python3
import argparse
import random
import os
import sys
import time
import subprocess
import shlex
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
clean_build = False
parallel_jobs = os.cpu_count()

# CI board control lists (used when running under CI)
ci_skip_boards = {
    'rp2040': [
        'adafruit_feather_rp2040_usb_host',
        'adafruit_fruit_jam',
        'adafruit_metro_rp2350',
        'feather_rp2040_max3421',
        'pico_sdk',
        'raspberry_pi_pico_w',
    ],
}

ci_preferred_boards = {
    'stm32h7': ['stm32h743eval'],
}


# -----------------------------
# Helper
# -----------------------------
def run_cmd(cmd):
    if isinstance(cmd, str):
        raise TypeError("run_cmd expects a list/tuple of args, not a string")
    args = cmd
    cmd_display = " ".join(args)
    r = subprocess.run(args, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    title = f'Command Error: {cmd_display}'
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
        print(cmd_display)
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
def cmake_board(board, build_args, build_flags_on):
    ret = [0, 0, 0]
    start_time = time.monotonic()

    build_dir = f'cmake-build/cmake-build-{board}'
    build_flags = []
    if len(build_flags_on) > 0:
        cli_flags = ' '.join(f'-D{flag}=1' for flag in build_flags_on)
        build_flags.append(f'-DCFLAGS_CLI={cli_flags}')
        build_dir += '-f1_' + '_'.join(build_flags_on)

    family = find_family(board)
    if family == 'espressif':
        # for espressif, we have to build example individually
        all_examples = get_examples(family)
        for example in all_examples:
            if build_utils.skip_example(example, board):
                ret[2] += 1
            else:
                rcmd = run_cmd([
                    'idf.py', '-C', f'examples/{example}', '-B', f'{build_dir}/{example}', '-GNinja',
                    f'-DBOARD={board}', *build_flags, 'build'
                ])
                ret[0 if rcmd.returncode == 0 else 1] += 1
    else:
        rcmd = run_cmd(['cmake', 'examples', '-B', build_dir, '-GNinja',
                        f'-DBOARD={board}', '-DCMAKE_BUILD_TYPE=MinSizeRel', '-DLINKERMAP_OPTION=-q -f tinyusb/src',
                        *build_args, *build_flags])
        if rcmd.returncode == 0:
            if clean_build:
                run_cmd(["cmake", "--build", build_dir, '--target', 'clean'])
            cmd = ["cmake", "--build", build_dir, '--parallel', str(parallel_jobs)]
            rcmd = run_cmd(cmd)
        if rcmd.returncode == 0:
            ret[0] += 1
            run_cmd(["cmake", "--build", build_dir, '--target', 'tinyusb_metrics'])
            # print(rcmd.stdout.decode("utf-8"))
        else:
            ret[1] += 1

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
        make_args = ["make", "-C", f"examples/{example}", f"BOARD={board}", '-j', str(parallel_jobs)]
        if make_option:
            make_args += shlex.split(make_option)
        if clean_build:
            run_cmd(make_args + ["clean"])
        build_result = run_cmd(make_args + ['all'])
        r = 0 if build_result.returncode == 0 else 1
        print_build_result(board, example, r, time.monotonic() - start_time)

    ret = [0, 0, 0]
    ret[r] = 1
    return ret


def make_board(board, build_args):
    print(build_separator)
    family = find_family(board);
    all_examples = get_examples(family)
    start_time = time.monotonic()
    ret = [0, 0, 0]
    if family == 'espressif' or family == 'rp2040':
        # espressif and rp2040 do not support make, use cmake instead
        final_status = 2
    else:
        with Pool(processes=os.cpu_count()) as pool:
            pool_args = list((map(lambda e, b=board, o=f"{build_args}": [e, b, o], all_examples)))
            r = pool.starmap(make_one_example, pool_args)
            # sum all element of same index (column sum)
            ret = list(map(sum, list(zip(*r))))
        final_status = 0 if ret[1] == 0 else 1
    print_build_result(board, 'all', final_status, time.monotonic() - start_time)
    return ret


# -----------------------------
# Build Family
# -----------------------------
def build_boards_list(boards, build_defines, build_system, build_flags_on):
    ret = [0, 0, 0]
    for b in boards:
        r = [0, 0, 0]
        if build_system == 'cmake':
            build_args = [f'-D{d}' for d in build_defines]
            r = cmake_board(b, build_args, build_flags_on)
        elif build_system == 'make':
            build_args = ' '.join(f'{d}' for d in build_defines)
            r = make_board(b, build_args)
        ret[0] += r[0]
        ret[1] += r[1]
        ret[2] += r[2]
    return ret


def get_family_boards(family, one_random, one_first):
    """Get list of boards for a family.

    Args:
        family: Family name
        one_random: If True, return only one random board
        one_first: If True, return only the first board (alphabetical)

    Returns:
        List of board names
    """
    skip_list = []
    preferred_list = []
    if os.getenv('GITHUB_ACTIONS') or os.getenv('CIRCLECI'):
        skip_list = ci_skip_boards.get(family, [])
        preferred_list = ci_preferred_boards.get(family, [])

    all_boards = []
    for entry in os.scandir(f"hw/bsp/{family}/boards"):
        if entry.is_dir() and entry.name not in skip_list:
            all_boards.append(entry.name)
    if not all_boards:
        print(f"No boards found for family '{family}'")
        return []
    all_boards.sort()

    # If only-one flags are set, honor select list first, then pick first or random
    if one_first or one_random:
        if preferred_list:
            return [preferred_list[0]]
        if one_first:
            return [all_boards[0]]
        if one_random:
            return [random.choice(all_boards)]

    return all_boards


# -----------------------------
# Main
# -----------------------------
def main():
    global verbose
    global clean_build
    global parallel_jobs

    parser = argparse.ArgumentParser()
    parser.add_argument('families', nargs='*', default=[], help='Families to build')
    parser.add_argument('-b', '--board', action='append', default=[], help='Boards to build')
    parser.add_argument('-c', '--clean', action='store_true', default=False, help='Clean before build')
    parser.add_argument('-t', '--toolchain', default='gcc', help='Toolchain to use, default is gcc')
    parser.add_argument('-s', '--build-system', default='cmake', help='Build system to use, default is cmake')
    parser.add_argument('-D', '--define-symbol', action='append', default=[], help='Define to pass to build system')
    parser.add_argument('-f1', '--build-flags-on', action='append', default=[], help='Build flag to pass to build system')
    parser.add_argument('--one-random', action='store_true', default=False,
                        help='Build only one random board of each specified family')
    parser.add_argument('--one-first', action='store_true', default=False,
                        help='Build only the first board (alphabetical) of each specified family')
    parser.add_argument('-j', '--jobs', type=int, default=os.cpu_count(), help='Number of jobs to run in parallel')
    parser.add_argument('-v', '--verbose', action='store_true', help='Verbose output')
    args = parser.parse_args()

    families = args.families
    boards = args.board
    toolchain = args.toolchain
    build_system = args.build_system
    build_defines = args.define_symbol
    build_flags_on = args.build_flags_on
    one_random = args.one_random
    one_first = args.one_first
    verbose = args.verbose
    clean_build = args.clean
    parallel_jobs = args.jobs

    build_defines.append(f'TOOLCHAIN={toolchain}')

    if len(families) == 0 and len(boards) == 0:
        print("Please specify families or board to build")
        return 1

    print(build_separator)
    print(build_format.format('Board', 'Example', '\033[39mResult\033[0m', 'Time'))
    total_time = time.monotonic()

    # get all families
    all_families = []
    if 'all' in families:
        for entry in os.scandir("hw/bsp"):
            if entry.is_dir() and entry.name != 'espressif' and os.path.isfile(entry.path + "/family.cmake"):
                all_families.append(entry.name)
    else:
        all_families = list(families)
    all_families.sort()

    # get boards from families and append to boards list
    all_boards = list(boards)
    for f in all_families:
        all_boards.extend(get_family_boards(f, one_random, one_first))

    # build all boards
    result = build_boards_list(all_boards, build_defines, build_system, build_flags_on)

    total_time = time.monotonic() - total_time
    print(build_separator)
    print(f"Build Summary: {result[0]} {STATUS_OK}, {result[1]} {STATUS_FAILED} and took {total_time:.2f}s")
    print(build_separator)

    return result[1]


if __name__ == '__main__':
    sys.exit(main())
