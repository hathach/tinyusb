import sys
import subprocess
import os

# TOP is tinyusb root dir
TOP = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

def get_family_dep(family):
    for entry in os.scandir("{}/hw/bsp/{}/boards".format(TOP, family)):
        if entry.is_dir():
            result = subprocess.run("make -C {}/examples/device/board_test BOARD={} get-deps".format(TOP, entry.name),
                                    shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
            print(result.stdout.decode("utf-8"))
            return result.returncode


status = 0
for d in sys.argv[1:]:
    status += get_family_dep(d)

sys.exit(status)
