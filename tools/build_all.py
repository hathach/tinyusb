import os
import shutil
import sys
import subprocess
import time

PARALLEL = "-j 4"
travis = False
if "TRAVIS" in os.environ and os.environ["TRAVIS"] == "true":
    PARALLEL="-j 2"
    travis = True

success_count = 0
fail_count = 0
exit_status = 0

all_device_example = ["cdc_msc_hid", "msc_dual_lun", "hid_generic_inout"]
all_boards = ["metro_m0_express", "metro_m4_express", "pca10056", "feather_nrf52840_express", "stm32f407g_disc1"]

def build_example(example, board):
    subprocess.run("make -C examples/device/{} BOARD={} clean".format(example, board), shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    return subprocess.run("make {} -C examples/device/{} BOARD={} all".format(PARALLEL, example, board), shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)

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
#subprocess.run("make -j2 -C examples/device/{} BOARD={} clean all".format(example, board), shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)


total_time = time.monotonic() - total_time

print("Total build time took {:.2f}s".format(total_time))
sys.exit(exit_status)
