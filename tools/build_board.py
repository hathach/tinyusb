import os
import glob
import sys
import subprocess
import time

SUCCEEDED = "\033[32msucceeded\033[0m"
FAILED = "\033[31mfailed\033[0m"
SKIPPED = "\033[33mskipped\033[0m"

success_count = 0
fail_count = 0
skip_count = 0
exit_status = 0

total_time = time.monotonic()

build_format = '| {:29} | {:30} | {:18} | {:7} | {:6} | {:6} |'
build_separator = '-' * 106

def filter_with_input(mylist):
    if len(sys.argv) > 1:
        input_args = list(set(mylist).intersection(sys.argv))
        if len(input_args) > 0:
            mylist[:] = input_args

# If examples are not specified in arguments, build all
all_examples = []
for entry in os.scandir("examples/device"):
    if entry.is_dir():
        all_examples.append("device/" + entry.name)
for entry in os.scandir("examples/host"):
    if entry.is_dir():
        all_examples.append("host/" + entry.name)
filter_with_input(all_examples)
all_examples.sort()

# If boards are not specified in arguments, build all
all_boards = []
for entry in os.scandir("hw/bsp"):
    if entry.is_dir() and os.path.exists(entry.path + "/board.mk"):
        all_boards.append(entry.name)
filter_with_input(all_boards)
all_boards.sort()

def build_board(example, board):
    global success_count, fail_count, skip_count, exit_status
    start_time = time.monotonic()
    flash_size = "-"
    sram_size = "-"

    # Check if board is skipped
    if skip_example(example, board):
        success = SKIPPED
        skip_count += 1
        print(build_format.format(example, board, success, '-', flash_size, sram_size))
    else:
        subprocess.run("make -C examples/{} BOARD={} clean".format(example, board), shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        build_result = subprocess.run("make -j -C examples/{} BOARD={} all".format(example, board), shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)

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
    elf_file = 'examples/{}/_build/{}/*.elf'.format(example, board)
    size_output = subprocess.run('size {}'.format(elf_file), shell=True, stdout=subprocess.PIPE).stdout.decode("utf-8")
    size_list = size_output.split('\n')[1].split('\t')
    flash_size = int(size_list[0])
    sram_size = int(size_list[1]) + int(size_list[2])
    return (flash_size, sram_size)

def skip_example(example, board):
    ex_dir = 'examples/' + example
    board_mk = 'hw/bsp/{}/board.mk'.format(board)
    with open(board_mk) as mk:
        mk_contents = mk.read()

        # Skip all OPT_MCU_NONE these are WIP port
        if '-DCFG_TUSB_MCU=OPT_MCU_NONE' in mk_contents:
            return 1

        # Skip if CFG_TUSB_MCU in board.mk to match skip file
        for skip_file in glob.iglob(ex_dir + '/.skip.MCU_*'):
            mcu_cflag = '-DCFG_TUSB_MCU=OPT_' + os.path.basename(skip_file).split('.')[2]
            if mcu_cflag in mk_contents:
                return 1

        # Build only list, if exists only these MCU are built
        only_list = list(glob.iglob(ex_dir + '/.only.MCU_*'))
        if len(only_list) > 0:
            for only_file in only_list:
                mcu_cflag = '-DCFG_TUSB_MCU=OPT_' + os.path.basename(only_file).split('.')[2]
                if mcu_cflag in mk_contents:
                    return 0
            return 1

    return 0

print(build_separator)
print(build_format.format('Example', 'Board', '\033[39mResult\033[0m', 'Time', 'Flash', 'SRAM'))

for example in all_examples:
    print(build_separator)
    for board in all_boards:
        build_board(example, board)

total_time = time.monotonic() - total_time
print(build_separator)
print("Build Summary: {} {}, {} {}, {} {} and took {:.2f}s".format(success_count, SUCCEEDED, fail_count, FAILED, skip_count, SKIPPED, total_time))
print(build_separator)

sys.exit(exit_status)
