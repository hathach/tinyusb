import os
import glob
import sys
import subprocess
import time

success_count = 0
fail_count = 0
skip_count = 0
exit_status = 0

total_time = time.monotonic()

build_format = '| {:23} | {:30} | {:9} | {:7} | {:6} | {:6} |'
build_separator = '-' * 100

# 1st Argument is Example, build all examples if not existed
all_examples = []
if len(sys.argv) > 1:
    all_examples.append(sys.argv[1])
else:
    for entry in os.scandir("examples/device"):
        # Only includes example with CMakeLists.txt for esp32s
        if entry.is_dir() and os.path.exists(entry.path + "/CMakeLists.txt"):
            all_examples.append(entry.name)
all_examples.sort()

# 2nd Argument is Board, build all boards if not existed
all_boards = []
if len(sys.argv) > 2:
    all_boards.append(sys.argv[2])
else:
    for entry in os.scandir("hw/bsp"):
        if entry.is_dir():
            with open(entry.path + '/board.mk') as mk:
                # Only includes ESP32-S2 board
                if 'CROSS_COMPILE = xtensa-esp32s2-elf-' in mk.read():
                    all_boards.append(entry.name)
                                
all_boards.sort()

def build_example(example, board):
    subprocess.run("make -C examples/device/{} BOARD={} clean".format(example, board), shell=True,
                   stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    return subprocess.run("make -j 4 -C examples/device/{} BOARD={} all".format(example, board), shell=True,
                          stdout=subprocess.PIPE, stderr=subprocess.STDOUT)

def build_size(example, board):
    #elf_file = 'examples/device/{}/_build/build-{}/{}-firmware.elf'.format(example, board, board)
    elf_file = 'examples/device/{}/_build/build-{}/*.elf'.format(example, board)
    size_output = subprocess.run('size {}'.format(elf_file), shell=True, stdout=subprocess.PIPE).stdout.decode("utf-8")
    size_list = size_output.split('\n')[1].split('\t')
    flash_size = int(size_list[0])
    sram_size = int(size_list[1]) + int(size_list[2])
    return (flash_size, sram_size)

def skip_example(example, board):
    return 0

print(build_separator)
print(build_format.format('Example', 'Board', 'Result', 'Time', 'Flash', 'SRAM'))
print(build_separator)

for example in all_examples:
    for board in all_boards:
        start_time = time.monotonic()

        flash_size = "-"
        sram_size = "-"

        # Check if board is skipped
        if skip_example(example, board):
            success = "\033[33mskipped\033[0m  "
            skip_count += 1
            print(build_format.format(example, board, success, '-', flash_size, sram_size))
        else:
            build_result = build_example(example, board)

            if build_result.returncode == 0:
                success = "\033[32msucceeded\033[0m"
                success_count += 1
                (flash_size, sram_size) = build_size(example, board)
            else:
                exit_status = build_result.returncode
                success = "\033[31mfailed\033[0m   "
                fail_count += 1

            build_duration = time.monotonic() - start_time
            print(build_format.format(example, board, success, "{:.2f}s".format(build_duration), flash_size, sram_size))

            if build_result.returncode != 0:
                print(build_result.stdout.decode("utf-8"))



total_time = time.monotonic() - total_time
print(build_separator)
print("Build Sumamary: {} \033[32msucceeded\033[0m, {} \033[31mfailed\033[0m, {} \033[33mskipped\033[0m and took {:.2f}s".format(success_count, fail_count, skip_count, total_time))
print(build_separator)

sys.exit(exit_status)
