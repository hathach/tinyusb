import os
import glob
import sys
import subprocess
import time
from multiprocessing import Pool

import build_utils

SUCCEEDED = "\033[32msucceeded\033[0m"
FAILED = "\033[31mfailed\033[0m"
SKIPPED = "\033[33mskipped\033[0m"

build_format = '| {:29} | {:30} | {:18} | {:7} | {:6} | {:6} |'
build_separator = '-' * 106

def filter_with_input(mylist):
    if len(sys.argv) > 1:
        input_args = list(set(mylist).intersection(sys.argv))
        if len(input_args) > 0:
            mylist[:] = input_args

# If examples are not specified in arguments, build all
all_examples = []
for dir1 in os.scandir("examples"):
    if dir1.is_dir():
        for entry in os.scandir(dir1.path):
            if entry.is_dir():
                all_examples.append(dir1.name + '/' + entry.name)
filter_with_input(all_examples)
all_examples.sort()

# If family are not specified in arguments, build all
all_families = []
for entry in os.scandir("hw/bsp"):
    if entry.is_dir() and os.path.isdir(entry.path + "/boards") and entry.name not in ("esp32s2", "esp32s3"):
        all_families.append(entry.name)
            
filter_with_input(all_families)
all_families.sort()

def build_family(example, family):
    all_boards = []
    for entry in os.scandir("hw/bsp/{}/boards".format(family)):
        if entry.is_dir() and entry.name != 'pico_sdk':
            all_boards.append(entry.name)
    filter_with_input(all_boards)
    all_boards.sort()

    with Pool(processes=len(all_boards)) as pool:
        pool_args = list((map(lambda b, e=example: [e, b], all_boards)))
        result = pool.starmap(build_board, pool_args)
        return list(map(sum, list(zip(*result))))
    
def build_board(example, board):
    start_time = time.monotonic()
    flash_size = "-"
    sram_size = "-"

    # succeeded, failed, skipped
    ret = [0, 0, 0]

    # Check if board is skipped
    if build_utils.skip_example(example, board):
        status = SKIPPED
        ret[2] = 1
        print(build_format.format(example, board, status, '-', flash_size, sram_size))
    else:   
        #subprocess.run("make -C examples/{} BOARD={} clean".format(example, board), shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        build_result = subprocess.run("make -j -C examples/{} BOARD={} all".format(example, board), shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)

        if build_result.returncode == 0:
            status = SUCCEEDED
            ret[0] = 1
            (flash_size, sram_size) = build_size(example, board)
            subprocess.run("make -j -C examples/{} BOARD={} copy-artifact".format(example, board), shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        else:
            status = FAILED
            ret[1] = 1

        build_duration = time.monotonic() - start_time
        print(build_format.format(example, board, status, "{:.2f}s".format(build_duration), flash_size, sram_size))

        if build_result.returncode != 0:
            print(build_result.stdout.decode("utf-8"))

    return ret

def build_size(example, board):
    elf_file = 'examples/{}/_build/{}/*.elf'.format(example, board)
    size_output = subprocess.run('size {}'.format(elf_file), shell=True, stdout=subprocess.PIPE).stdout.decode("utf-8")
    size_list = size_output.split('\n')[1].split('\t')
    flash_size = int(size_list[0])
    sram_size = int(size_list[1]) + int(size_list[2])
    return (flash_size, sram_size)

if __name__ == '__main__':
    # succeeded, failed, skipped
    total_result = [0, 0, 0]

    print(build_separator)
    print(build_format.format('Example', 'Board', '\033[39mResult\033[0m', 'Time', 'Flash', 'SRAM'))
    total_time = time.monotonic()

    for example in all_examples:
        print(build_separator)
        for family in all_families:
            fret = build_family(example, family)
            total_result = list(map(lambda x, y: x+y, total_result, fret))

    total_time = time.monotonic() - total_time
    print(build_separator)
    print("Build Summary: {} {}, {} {}, {} {} and took {:.2f}s".format(total_result[0], SUCCEEDED, total_result[1], FAILED, total_result[2], SKIPPED, total_time))
    print(build_separator)

    sys.exit(total_result[1])
