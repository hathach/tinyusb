import os
import sys
import time
import subprocess
import click
from multiprocessing import Pool

import build_utils

SUCCEEDED = "\033[32msucceeded\033[0m"
FAILED = "\033[31mfailed\033[0m"

build_separator = '-' * 106


def build_board_cmake(board, toolchain):
    start_time = time.monotonic()
    build_dir = f"cmake-build/cmake-build-{board}"

    # Generate build
    r = subprocess.run(f"cmake examples -B {build_dir} -G \"Ninja\" -DBOARD={board} -DCMAKE_BUILD_TYPE=MinSizeRel " \
                       f"-DTOOLCHAIN={toolchain}", shell=True, stdout=subprocess.PIPE,
                       stderr=subprocess.STDOUT)

    # Build
    if r.returncode == 0:
        r = subprocess.run(f"cmake --build {build_dir}", shell=True, stdout=subprocess.PIPE,
                           stderr=subprocess.STDOUT)

    duration = time.monotonic() - start_time

    if r.returncode == 0:
        status = SUCCEEDED
    else:
        status = FAILED

    flash_size = "-"
    sram_size = "-"
    example = 'all'
    title = build_utils.build_format.format(example, board, status, "{:.2f}s".format(duration), flash_size, sram_size)

    if r.returncode == 0:
        print(title)
    else:
        # print build output if failed
        if os.getenv('CI'):
            print(f"::group::{title}")
            print(r.stdout.decode("utf-8"))
            print(f"::endgroup::")
        else:
            print(title)
            print(r.stdout.decode("utf-8"))

    return r.returncode == 0


def get_examples_make():
    all_examples = []
    for d in os.scandir("examples"):
        if d.is_dir() and 'cmake' not in d.name and 'build_system' not in d.name:
            for entry in os.scandir(d.path):
                if entry.is_dir() and 'cmake' not in entry.name:
                    all_examples.append(d.name + '/' + entry.name)
    all_examples.sort()
    return all_examples

def build_family(family, toolchain, build_system):
    all_boards = []
    for entry in os.scandir(f"hw/bsp/{family}/boards"):
        if entry.is_dir() and entry.name != 'pico_sdk':
            all_boards.append(entry.name)
    all_boards.sort()

    # success, failed
    ret = [0, 0]
    if build_system == 'cmake':
        for board in all_boards:
            if build_board_cmake(board, toolchain):
                ret[0] += 1
            else:
                ret[1] += 1
    elif build_system == 'make':
        all_examples = get_examples_make()
        for example in all_examples:
            with Pool(processes=os.cpu_count()) as pool:
                pool_args = list((map(lambda b, e=example, o=f"TOOLCHAIN={toolchain}": [e, b, o], all_boards)))
                result = pool.starmap(build_utils.build_example, pool_args)
                # sum all element of same index (column sum)
                return list(map(sum, list(zip(*result))))
    return ret


@click.command()
@click.argument('family', nargs=-1, required=False)
@click.option('-b', '--board', multiple=True, default=None, help='Boards to build')
@click.option('-t', '--toolchain', default='gcc', help='Toolchain to use, default is gcc')
@click.option('-s', '--build-system', default='cmake', help='Build system to use, default is cmake')
def main(family, board, toolchain, build_system):
    if len(family) == 0 and len(board) == 0:
        print("Please specify family or board to build")
        return 1

    print(build_separator)
    print(build_utils.build_format.format('Example', 'Board', '\033[39mResult\033[0m', 'Time', 'Flash', 'SRAM'))
    total_time = time.monotonic()
    total_result = [0, 0]

    # build families: cmake, make
    if family is not None:
        all_families = []
        if 'all' in family:
            for entry in os.scandir("hw/bsp"):
                if entry.is_dir() and entry.name != 'espressif' and os.path.isfile(entry.path + "/family.cmake"):
                    all_families.append(entry.name)
        else:
            all_families = list(family)
        all_families.sort()

        # succeeded, failed
        for f in all_families:
            fret = build_family(f, toolchain, build_system)
            total_result[0] += fret[0]
            total_result[1] += fret[1]

    # build board (only cmake)
    if board is not None:
        for b in board:
            if build_board_cmake(b, toolchain):
                total_result[0] += 1
            else:
                total_result[1] += 1

    total_time = time.monotonic() - total_time
    print(build_separator)
    print(f"Build Summary: {total_result[0]} {SUCCEEDED}, {total_result[1]} {FAILED} and took {total_time:.2f}s")
    print(build_separator)
    return total_result[1]


if __name__ == '__main__':
    sys.exit(main())
