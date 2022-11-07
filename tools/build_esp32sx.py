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

build_format = '| {:23} | {:30} | {:18} | {:7} | {:6} | {:6} |'
build_separator = '-' * 100

def filter_with_input(mylist):
    if len(sys.argv) > 1:
        input_args = list(set(mylist).intersection(sys.argv))
        if len(input_args) > 0:
            mylist[:] = input_args

# Build all examples if not specified
all_examples = []
for entry in os.scandir("examples/device"):
    # Only includes example with CMakeLists.txt for esp32s, and skip board_test to speed up ci
    if entry.is_dir() and os.path.exists(entry.path + "/sdkconfig.defaults") and entry.name != 'board_test':
        all_examples.append(entry.name)
filter_with_input(all_examples)
all_examples.sort()

# Build all boards if not specified
all_boards = []
for entry in os.scandir("hw/bsp/esp32s2/boards"):
    if entry.is_dir():
        all_boards.append(entry.name)
for entry in os.scandir("hw/bsp/esp32s3/boards"):
    if entry.is_dir():
        all_boards.append(entry.name)
filter_with_input(all_boards)
all_boards.sort()

def build_board(example, board):
    global success_count, fail_count, skip_count, exit_status
    start_time = time.monotonic()
    flash_size = "-"
    sram_size = "-"

    # Check if board is skipped
    if build_utils.skip_example(example, board):
        success = SKIPPED
        skip_count += 1
        print(build_format.format(example, board, success, '-', flash_size, sram_size))
    else:
        subprocess.run("make -C examples/device/{} BOARD={} clean".format(example, board), shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        build_result = subprocess.run("make -j -C examples/device/{} BOARD={} all".format(example, board), shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)

        if build_result.returncode == 0:
            success = SUCCEEDED
            success_count += 1
            (flash_size, sram_size) = build_size(example, board)
        else:
            exit_status = build_result.returncode
            success = FAILED
            fail_count += 1

        build_duration = time.monotonic() - start_time
        print(build_format.format(example, board, success, "{:.2f}s".format(build_duration), flash_size, sram_size))

        if build_result.returncode != 0:
            print(build_result.stdout.decode("utf-8"))

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
