import os
import glob
import sys
import subprocess
import time

import build_utils

SUCCEEDED = "\033[32msucceeded\033[0m"
FAILED = "\033[31mfailed\033[0m"
SKIPPED = "\033[33mskipped\033[0m"

success_count = 0
fail_count = 0
skip_count = 0
exit_status = 0

total_time = time.monotonic()

build_format = '| {:30} | {:30} | {:18} | {:7} | {:6} | {:6} |'
build_separator = '-' * 107

def filter_with_input(mylist):
    if len(sys.argv) > 1:
        input_args = list(set(mylist).intersection(sys.argv))
        if len(input_args) > 0:
            mylist[:] = input_args


# Build all examples if not specified
all_examples = [entry.replace('examples/', '') for entry in glob.glob("examples/*/*_freertos")]
filter_with_input(all_examples)
all_examples.append('device/board_test')
all_examples.sort()

# Build all boards if not specified
all_boards = []
for entry in os.scandir("hw/bsp/espressif/boards"):
    if entry.is_dir():
        all_boards.append(entry.name)
filter_with_input(all_boards)
all_boards.sort()

def build_board(example, board):
    global success_count, fail_count, skip_count, exit_status
    start_time = time.monotonic()

    # Check if board is skipped
    build_dir = f"cmake-build/cmake-build-{board}/{example}"

    # Generate and build
    r = subprocess.run(f"cmake examples/{example} -B {build_dir} -G \"Ninja\" -DBOARD={board} -DMAX3421_HOST=1",
                       shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    if r.returncode == 0:
        r = subprocess.run(f"cmake --build {build_dir}", shell=True, stdout=subprocess.PIPE,
                           stderr=subprocess.STDOUT)
    build_duration = time.monotonic() - start_time
    flash_size = "-"
    sram_size = "-"

    if r.returncode == 0:
        success = SUCCEEDED
        success_count += 1
        #(flash_size, sram_size) = build_size(example, board)
    else:
        exit_status = r.returncode
        success = FAILED
        fail_count += 1

    title = build_format.format(example, board, success, "{:.2f}s".format(build_duration), flash_size, sram_size)
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


def build_size(example, board):
    #elf_file = 'examples/device/{}/_build/{}/{}-firmware.elf'.format(example, board, board)
    elf_file = 'examples/device/{}/_build/{}/*.elf'.format(example, board)
    size_output = subprocess.run('size {}'.format(elf_file), shell=True, stdout=subprocess.PIPE).stdout.decode("utf-8")
    size_list = size_output.split('\n')[1].split('\t')
    flash_size = int(size_list[0])
    sram_size = int(size_list[1]) + int(size_list[2])
    return (flash_size, sram_size)


print(build_separator)
print(build_format.format('Example', 'Board', '\033[39mResult\033[0m', 'Time', 'Flash', 'SRAM'))
print(build_separator)

for example in all_examples:
    for board in all_boards:
        build_board(example, board)

total_time = time.monotonic() - total_time
print(build_separator)
print("Build Summary: {} {}, {} {}, {} {} and took {:.2f}s".format(success_count, SUCCEEDED, fail_count, FAILED, skip_count, SKIPPED, total_time))
print(build_separator)

sys.exit(exit_status)
