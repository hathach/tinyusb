import os
import sys
import time
import subprocess
import click

import build_utils

SUCCEEDED = "\033[32msucceeded\033[0m"
FAILED = "\033[31mfailed\033[0m"

build_separator = '-' * 106


def build_board(board, toolchain):
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

    if os.getenv('CI'):
        # always print build output if in CI
        print(f"::group::{title}")
        print(r.stdout.decode("utf-8"))
        print(f"::endgroup::")
    else:
        # print build output if failed
        print(title)
        if r.returncode != 0:
            print(r.stdout.decode("utf-8"))

    return r.returncode == 0


def build_family(family, toolchain):
    all_boards = []
    for entry in os.scandir("hw/bsp/{}/boards".format(family)):
        if entry.is_dir() and entry.name != 'pico_sdk':
            all_boards.append(entry.name)
    all_boards.sort()

    # success, failed
    ret = [0, 0]
    for board in all_boards:
        if build_board(board, toolchain):
            ret[0] += 1
        else:
            ret[1] += 1

    return ret


@click.command()
@click.argument('family', nargs=-1, required=False)
@click.option('-b', '--board', multiple=True, default=None, help='Boards to build')
@click.option('-t', '--toolchain', default='gcc', help='Toolchain to use, default is gcc')
def main(family, board, toolchain):
    if len(family) == 0 and len(board) == 0:
        print("Please specify family or board to build")
        return 1

    print(build_separator)
    print(build_utils.build_format.format('Example', 'Board', '\033[39mResult\033[0m', 'Time', 'Flash', 'SRAM'))
    total_time = time.monotonic()
    total_result = [0, 0]

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
            fret = build_family(f, toolchain)
            total_result[0] += fret[0]
            total_result[1] += fret[1]

    if board is not None:
        for b in board:
            if build_board(b, toolchain):
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
