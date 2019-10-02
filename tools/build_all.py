import os
import shutil
import sys
import subprocess
import time

travis = False
if "TRAVIS" in os.environ and os.environ["TRAVIS"] == "true":
    travis = True

success_count = 0
fail_count = 0
exit_status = 0

total_time = time.monotonic()

all_examples = []
for entry in os.scandir("examples/device"):
    if entry.is_dir():
        all_examples.append(entry.name)

# TODO update freeRTOS example to work with all boards (only nrf52840 now)
all_examples.remove("cdc_msc_hid_freertos")

all_boards = []
for entry in os.scandir("hw/bsp"):
    if entry.is_dir():
        all_boards.append(entry.name)


def build_example(example, board):
    subprocess.run("make -C examples/device/{} BOARD={} clean".format(example, board), shell=True,
                   stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    return subprocess.run("make -j 4 -C examples/device/{} BOARD={} all".format(example, board), shell=True,
                          stdout=subprocess.PIPE, stderr=subprocess.STDOUT)


build_format = '| {:30} | {:30} | {:9} '
build_separator = '-' * 87
print(build_separator)
print((build_format + '| {:5} |').format('Example', 'Board', 'Result', 'Time'))
for example in all_examples:
    print(build_separator)
    for board in all_boards:
        start_time = time.monotonic()
        build_result = build_example(example, board)
        build_duration = time.monotonic() - start_time

        if build_result.returncode == 0:
            success = "\033[32msucceeded\033[0m"
            success_count += 1
        else:
            exit_status = build_result.returncode
            success = "\033[31mfailed\033[0m   "
            fail_count += 1

        if travis:
            print('travis_fold:start:build-{}-{}\\r'.format(example, board))

        print((build_format + '| {:.2f}s |').format(example, board, success, build_duration))
        if build_result.returncode != 0:
            print(build_result.stdout.decode("utf-8"))

        if travis:
            print('travis_fold:end:build-{}-{}\\r'.format(example, board))

# FreeRTOS example
# example = 'cdc_msc_hid_freertos'
# board = 'pca10056'
# build_example(example, board)

total_time = time.monotonic() - total_time
print(build_separator)
print("Build Sumamary: {} \033[32msucceeded\033[0m, {} \033[31mfailed\033[0m and took {:.2f}s".format(success_count, fail_count, total_time))
print(build_separator)

sys.exit(exit_status)
