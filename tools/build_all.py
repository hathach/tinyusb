import os
import shutil
import sys
import subprocess
import time

all_boards = ["metro_m0_express", "metro_m4_express", "pca10056", "stm32f407g_disc1"]

for board in all_boards:
    subprocess.run("make -j2 -C examples/device/cdc_msc_hid BOARD={} clean".format(board), shell=True)
    subprocess.run("make -j2 -C examples/device/cdc_msc_hid BOARD={} all".format(board), shell=True)
