import os
import sys
import subprocess


# dependency lookup (ABC sorted)
# deps = {
#    'LPC11UXX' : [ [] ]
# }


def get_family_dep(family):
    for entry in os.scandir("hw/bsp/{}/boards".format(family)):
        if entry.is_dir():
            result = subprocess.run("make -C examples/device/board_test BOARD={} get-deps".format(entry.name),
                                    shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
            print(result.stdout.decode("utf-8"))
            return result.returncode

status = 0
all_family = sys.argv[1:]
for f in all_family:
    status += get_family_dep(f)

sys.exit(status)