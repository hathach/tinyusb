import os
import shutil
import sys
import subprocess
import time

all_device_example = ["cdc_msc_hid", "msc_dual_lun", "hid_generic_inout"]
all_boards = ["metro_m0_express", "metro_m4_express", "pca10056", "stm32f407g_disc1"]

for example in all_device_example:
    for board in all_boards:
        subprocess.run("make -j2 -C examples/device/{} BOARD={} clean".format(example, board), shell=True)
        subprocess.run("make -j2 -C examples/device/{} BOARD={} all".format(example, board), shell=True)

# FreeRTOS example
example = 'cdc_msc_hid_freertos'
board = 'pca10056'
subprocess.run("make -j2 -C examples/device/{} BOARD={} clean".format(example, board), shell=True)
subprocess.run("make -j2 -C examples/device/{} BOARD={} all".format(example, board), shell=True)
