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

all_device_example = ["cdc_msc_hid", "msc_dual_lun", "hid_generic_inout"]

all_boards = []
for entry in os.scandir("hw/bsp"):
    if entry.is_dir():
        all_boards.append(entry.name)

def build_example(example, board):
    subprocess.run("make -C examples/device/{} BOARD={} clean".format(example, board), shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    return subprocess.run("make -j 4 -C examples/device/{} BOARD={} all".format(example, board), shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)

total_time = time.monotonic()

for example in all_device_example:
    for board in all_boards:
        start_time = time.monotonic()
        build_result = build_example(example, board)
        build_duration = time.monotonic() - start_time

        if build_result.returncode != 0:
            exit_status = build_result.returncode
            success = "\033[31mfailed\033[0m"
            fail_count += 1
        else:
            success = "\033[32msucceeded\033[0m"
            success_count += 1

        if travis:
            print('travis_fold:start:build-{}-{}\\r'.format(example, board))
        print("Build {} on {} took {:.2f}s and {}".format(example, board, build_duration, success))
        if build_result.returncode != 0:
            print(build_result.stdout.decode("utf-8"))
        if travis:
            print('travis_fold:end:build-{}-{}\\r'.format(example, board))

# FreeRTOS example
#example = 'cdc_msc_hid_freertos'
#board = 'pca10056'
#build_example(example, board)

total_time = time.monotonic() - total_time

print("Build Sumamary: {} \033[32msucceeded\033[0m, {} \033[31mfailed\033[0m".format(success_count, fail_count))
print("Total build time took {:.2f}s".format(total_time))

sys.exit(exit_status)
