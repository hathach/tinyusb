import os
import glob
import sys
import subprocess
import time

success_count = 0
fail_count = 0
exit_status = 0

total_time = time.monotonic()

# 1st Argument is Example, build all examples if not existed
all_examples = []
if len(sys.argv) > 1:
    all_examples.append(sys.argv[1])
else:
    for entry in os.scandir("examples/device"):
        if entry.is_dir():
            all_examples.append(entry.name)

    # TODO update freeRTOS example to work with all boards (only nrf52840 now)
    all_examples.remove("cdc_msc_hid_freertos")
all_examples.sort()

# 2nd Argument is Board, build all boards if not existed
all_boards = []
if len(sys.argv) > 2:
    all_boards.append(sys.argv[2])
else:
    for entry in os.scandir("hw/bsp"):
        if entry.is_dir():
            all_boards.append(entry.name)
all_boards.sort()


def build_example(example, board):
    subprocess.run("make -C examples/device/{} BOARD={} clean".format(example, board), shell=True,
                   stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    return subprocess.run("make -j 4 -C examples/device/{} BOARD={} all".format(example, board), shell=True,
                          stdout=subprocess.PIPE, stderr=subprocess.STDOUT)

def skip_example(example, board):
    ex_dir = 'examples/device/' + example
    board_mk = 'hw/bsp/{}/board.mk'.format(board)
    for skip_file in glob.iglob(ex_dir + '/.skip.MCU_*'):
        mcu_cflag = '-DCFG_TUSB_MCU=OPT_' + os.path.basename(skip_file).split('.')[2]
        with open(board_mk) as mk:
            if mcu_cflag in mk.read():
                return 1

    return 0

build_format = '| {:30} | {:30} | {:9} '
build_separator = '-' * 87
print(build_separator)
print((build_format + '| {:5} |').format('Example', 'Board', 'Result', 'Time'))
for example in all_examples:
    print(build_separator)
    for board in all_boards:
        start_time = time.monotonic()

        # Check if board is skipped
        if skip_example(example, board):
            success = "\033[33mskipped\033[0m  "
            print((build_format + '| {:.2f}s |').format(example, board, success, 0))
        else:
            build_result = build_example(example, board)

            if build_result.returncode == 0:
                success = "\033[32msucceeded\033[0m"
                success_count += 1
            else:
                exit_status = build_result.returncode
                success = "\033[31mfailed\033[0m   "
                fail_count += 1

            build_duration = time.monotonic() - start_time
            print((build_format + '| {:.2f}s |').format(example, board, success, build_duration))

            if build_result.returncode != 0:
                print(build_result.stdout.decode("utf-8"))

# FreeRTOS example
# example = 'cdc_msc_hid_freertos'
# board = 'pca10056'
# build_example(example, board)

total_time = time.monotonic() - total_time
print(build_separator)
print("Build Sumamary: {} \033[32msucceeded\033[0m, {} \033[31mfailed\033[0m and took {:.2f}s".format(success_count, fail_count, total_time))
print(build_separator)

sys.exit(exit_status)
