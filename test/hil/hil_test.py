#
# The MIT License (MIT)
#
# Copyright (c) 2023 HiFiPhile
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.

# udev rules :
# ACTION=="add", SUBSYSTEM=="tty", SUBSYSTEMS=="usb", MODE="0666", PROGRAM="/bin/sh -c 'echo $$ID_SERIAL_SHORT | rev | cut -c -8 | rev'", SYMLINK+="ttyUSB_%c.%s{bInterfaceNumber}"
# ACTION=="add", SUBSYSTEM=="block", SUBSYSTEMS=="usb", ENV{ID_FS_USAGE}=="filesystem", MODE="0666", PROGRAM="/bin/sh -c 'echo $$ID_SERIAL_SHORT | rev | cut -c -8 | rev'", RUN{program}+="/usr/bin/systemd-mount --no-block --automount=yes --collect $devnode /media/blkUSB_%c.%s{bInterfaceNumber}"

import argparse
import os
import re
import sys
import time
import serial
import subprocess
import json
import glob
from multiprocessing import Pool
import fs

ENUM_TIMEOUT = 30

STATUS_OK = "\033[32mOK\033[0m"
STATUS_FAILED = "\033[31mFailed\033[0m"
STATUS_SKIPPED = "\033[33mSkipped\033[0m"

verbose = False

# get usb serial by id
def get_serial_dev(id, vendor_str, product_str, ifnum):
    if vendor_str and product_str:
        # known vendor and product
        vendor_str = vendor_str.replace(' ', '_')
        product_str = product_str.replace(' ', '_')
        return f'/dev/serial/by-id/usb-{vendor_str}_{product_str}_{id}-if{ifnum:02d}'
    else:
        # just use id: mostly for cp210x/ftdi flasher
        pattern = f'/dev/serial/by-id/usb-*_{id}-if{ifnum:02d}*'
        port_list = glob.glob(pattern)
        return port_list[0]


# get usb disk by id
def get_disk_dev(id, vendor_str, lun):
    return f'/dev/disk/by-id/usb-{vendor_str}_Mass_Storage_{id}-0:{lun}'


def get_hid_dev(id, vendor_str, product_str, event):
    return f'/dev/input/by-id/usb-{vendor_str}_{product_str}_{id}-{event}'


def open_serial_dev(port):
    timeout = ENUM_TIMEOUT
    ser = None
    while timeout:
        if os.path.exists(port):
            try:
                # slight delay since kernel may occupy the port briefly
                time.sleep(0.5)
                timeout = timeout - 0.5
                ser = serial.Serial(port, timeout=5)
                break
            except serial.SerialException:
                pass
        time.sleep(0.5)
        timeout = timeout - 0.5

    assert timeout, f'Cannot open port f{port}' if os.path.exists(port) else f'Port {port} not existed'
    return ser


def read_disk_file(uid, lun, fname):
    # open_fs("fat://{dev}) require 'pip install pyfatfs'
    dev = get_disk_dev(uid, 'TinyUSB', lun)
    timeout = ENUM_TIMEOUT
    while timeout:
        if os.path.exists(dev):
            fat = fs.open_fs(f'fat://{dev}?read_only=true')
            try:
                with fat.open(fname, 'rb') as f:
                    data = f.read()
            finally:
                fat.close()
            assert data, f'Cannot read file {fname} from {dev}'
            return data
        time.sleep(1)
        timeout -= 1

    assert timeout, f'Storage {dev} not existed'
    return None


# -------------------------------------------------------------
# Flashing firmware
# -------------------------------------------------------------
def run_cmd(cmd):
    r = subprocess.run(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    if r.returncode != 0:
        title = f'COMMAND FAILED: {cmd}'
        print()
        if os.getenv('CI'):
            print(f"::group::{title}")
            print(r.stdout.decode("utf-8"))
            print(f"::endgroup::")
        else:
            print(title)
            print(r.stdout.decode("utf-8"))
    elif verbose:
        print(cmd)
        print(r.stdout.decode("utf-8"))
    return r


def flash_jlink(board, firmware):
    script = ['halt', 'r', f'loadfile {firmware}.elf', 'r', 'go', 'exit']
    f_jlink = f'{board["name"]}_{os.path.basename(firmware)}.jlink'
    with open(f_jlink, 'w') as f:
        f.writelines(f'{s}\n' for s in script)
    ret = run_cmd(f'JLinkExe -USB {board["flasher_sn"]} {board["flasher_args"]} -if swd -JTAGConf -1,-1 -speed auto -NoGui 1 -ExitOnError 1 -CommandFile {f_jlink}')
    os.remove(f_jlink)
    return ret


def flash_stlink(board, firmware):
    ret = run_cmd(f'STM32_Programmer_CLI --connect port=swd sn={board["flasher_sn"]} --write {firmware}.elf --go')
    return ret


def flash_stflash(board, firmware):
    ret = run_cmd(f'st-flash --serial {board["flasher_sn"]} write {firmware}.bin 0x8000000')
    return ret


def flash_openocd(board, firmware):
    ret = run_cmd(f'openocd -c "adapter serial {board["flasher_sn"]}" {board["flasher_args"]} -c "program {firmware}.elf reset exit"')
    return ret


def flash_openocd_wch(board, firmware):
    # Content of the wch-riscv.cfg file
    cfg_content = """
adapter driver wlinke
adapter speed 6000
transport select sdi

wlink_set_address 0x00000000
set _CHIPNAME wch_riscv
sdi newtap $_CHIPNAME cpu -irlen 5 -expected-id 0x00001

set _TARGETNAME $_CHIPNAME.cpu

target create $_TARGETNAME.0 wch_riscv -chain-position $_TARGETNAME
$_TARGETNAME.0 configure  -work-area-phys 0x20000000 -work-area-size 10000 -work-area-backup 1
set _FLASHNAME $_CHIPNAME.flash

flash bank $_FLASHNAME wch_riscv 0x00000000 0 0 0 $_TARGETNAME.0

echo "Ready for Remote Connections"
"""
    f_wch = f"wch-riscv_{board['uid']}.cfg"
    if not os.path.exists(f_wch):
        with open(f_wch, 'w') as file:
            file.write(cfg_content)

    ret = run_cmd(f'openocd_wch -c "adapter serial {board["flasher_sn"]}" -f {f_wch} '
                  f'-c "program {firmware}.elf reset exit"')
    return ret


def flash_openocd_adi(board, firmware):
    openocd_adi_script_path = f'{os.getenv("HOME")}/app/openocd_adi/tcl'
    if not os.path.exists(openocd_adi_script_path):
        openocd_adi_script_path = '/home/pi/openocd_adi/tcl'

    ret = run_cmd(f'openocd_adi -c "adapter serial {board["flasher_sn"]}" -s {openocd_adi_script_path} '
                  f'{board["flasher_args"]} -c "program {firmware}.elf reset exit"')
    return ret

def flash_wlink_rs(board, firmware):
    # wlink use index for probe selection and lacking usb serial support
    ret = run_cmd(f'wlink flash {firmware}.elf')
    return ret


def flash_esptool(board, firmware):
    port = get_serial_dev(board["flasher_sn"], None, None, 0)
    dir = os.path.dirname(f'{firmware}.bin')
    with open(f'{dir}/config.env') as f:
        IDF_TARGET = json.load(f)['IDF_TARGET']
    with open(f'{dir}/flash_args') as f:
        flash_args = f.read().strip().replace('\n', ' ')
    command = (f'esptool.py --chip {IDF_TARGET} -p {port} {board["flasher_args"]} '
               f'--before=default_reset --after=hard_reset write_flash {flash_args}')
    ret = subprocess.run(command, shell=True, cwd=dir, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    return ret


def flash_uniflash(board, firmware):
    ret = run_cmd(f'dslite.sh {board["flasher_args"]} -f {firmware}.hex')
    return ret


# -------------------------------------------------------------
# Tests: dual
# -------------------------------------------------------------

def test_dual_host_info_to_device_cdc(board):
    uid = board['uid']
    declared_devs = [f'{d["vid_pid"]}_{d["serial"]}' for d in board['tests']['dual_attached']]

    port = get_serial_dev(uid, 'TinyUSB', "TinyUSB_Device", 0)
    ser = open_serial_dev(port)
    # read from cdc, first line should contain vid/pid and serial
    data = ser.read(1000)
    lines = data.decode('utf-8').splitlines()
    enum_dev_sn = []
    for l in lines:
        vid_pid_sn = re.search(r'ID ([0-9a-fA-F]+):([0-9a-fA-F]+) SN (\w+)', l)
        if vid_pid_sn:
            print(f'\r\n  {l} ', end='')
            enum_dev_sn.append(f'{vid_pid_sn.group(1)}_{vid_pid_sn.group(2)}_{vid_pid_sn.group(3)}')

    assert(set(declared_devs) == set(enum_dev_sn)), \
        f'Enumerated devices {enum_dev_sn} not match with declared {declared_devs}'
    return 0


# -------------------------------------------------------------
# Tests: device
# -------------------------------------------------------------
def test_device_board_test(board):
    # Dummy test
    pass


def test_device_cdc_dual_ports(board):
    uid = board['uid']
    port1 = get_serial_dev(uid, 'TinyUSB', "TinyUSB_Device", 0)
    port2 = get_serial_dev(uid, 'TinyUSB', "TinyUSB_Device", 2)

    ser1 = open_serial_dev(port1)
    ser2 = open_serial_dev(port2)

    # Echo test
    str1 = b"test_no1"
    ser1.write(str1)
    ser1.flush()
    assert ser1.read(len(str1)) == str1.lower(), 'Port1 wrong data'
    assert ser2.read(len(str1)) == str1.upper(), 'Port2 wrong data'

    str2 = b"test_no2"
    ser2.write(str2)
    ser2.flush()
    assert ser1.read(len(str2)) == str2.lower(), 'Port1 wrong data'
    assert ser2.read(len(str2)) == str2.upper(), 'Port2 wrong data'


def test_device_cdc_msc(board):
    uid = board['uid']
    # Echo test
    port = get_serial_dev(uid, 'TinyUSB', "TinyUSB_Device", 0)
    ser = open_serial_dev(port)

    str = b"test_str"
    ser.write(str)
    ser.flush()
    assert ser.read(len(str)) == str, 'CDC wrong data'

    # Block test
    data = read_disk_file(uid,0,'README.TXT')
    readme = \
    b"This is tinyusb's MassStorage Class demo.\r\n\r\n\
If you find any bugs or get any questions, feel free to file an\r\n\
issue at github.com/hathach/tinyusb"

    assert data == readme, 'MSC wrong data'


def test_device_cdc_msc_freertos(board):
    test_device_cdc_msc(board)


def test_device_dfu(board):
    uid = board['uid']

    # Wait device enum
    timeout = ENUM_TIMEOUT
    while timeout:
        ret = subprocess.run(f'dfu-util -l',
                             shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        stdout = ret.stdout.decode()
        if f'serial="{uid}"' in stdout and 'Found DFU: [cafe:4000]' in stdout:
            break
        time.sleep(1)
        timeout = timeout - 1

    assert timeout, 'Device not available'

    f_dfu0 = f'dfu0_{uid}'
    f_dfu1 = f'dfu1_{uid}'

    # Test upload
    try:
        os.remove(f_dfu0)
        os.remove(f_dfu1)
    except OSError:
        pass

    ret = run_cmd(f'dfu-util -S {uid} -a 0 -U {f_dfu0}')
    assert ret.returncode == 0, 'Upload failed'

    ret = run_cmd(f'dfu-util -S {uid} -a 1 -U {f_dfu1}')
    assert ret.returncode == 0, 'Upload failed'

    with open(f_dfu0) as f:
        assert 'Hello world from TinyUSB DFU! - Partition 0' in f.read(), 'Wrong uploaded data'

    with open(f_dfu1) as f:
        assert 'Hello world from TinyUSB DFU! - Partition 1' in f.read(), 'Wrong uploaded data'

    os.remove(f_dfu0)
    os.remove(f_dfu1)


def test_device_dfu_runtime(board):
    uid = board['uid']

    # Wait device enum
    timeout = ENUM_TIMEOUT
    while timeout:
        ret = subprocess.run(f'dfu-util -l',
                             shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        stdout = ret.stdout.decode()
        if f'serial="{uid}"' in stdout and 'Found Runtime: [cafe:4000]' in stdout:
            break
        time.sleep(1)
        timeout = timeout - 1

    assert timeout, 'Device not available'


def test_device_hid_boot_interface(board):
    uid = board['uid']
    kbd = get_hid_dev(uid, 'TinyUSB', 'TinyUSB_Device', 'event-kbd')
    mouse1 = get_hid_dev(uid, 'TinyUSB', 'TinyUSB_Device', 'if01-event-mouse')
    mouse2 = get_hid_dev(uid, 'TinyUSB', 'TinyUSB_Device', 'if01-mouse')
    # Wait device enum
    timeout = ENUM_TIMEOUT
    while timeout:
        if os.path.exists(kbd) and os.path.exists(mouse1) and os.path.exists(mouse2):
            break
        time.sleep(1)
        timeout = timeout - 1

    assert timeout, 'HID device not available'


def test_device_hid_composite_freertos(id):
    # TODO implement later
    pass


# -------------------------------------------------------------
# Main
# -------------------------------------------------------------
# all possible tests: board_test is added last to disable board's usb
all_tests = [
    'device/cdc_dual_ports',
    'device/cdc_msc',
    'device/dfu',
    'device/cdc_msc_freertos',  # don't test 2 cdc_msc next to each other
    'device/dfu_runtime',
    'device/hid_boot_interface',

    'dual/host_info_to_device_cdc',
    'device/board_test'
]


def test_board(board):
    name = board['name']
    flasher = board['flasher'].lower()

    # default to all tests
    test_list = list(all_tests)

    if 'tests' in board:
        board_tests = board['tests']
        if 'only' in board_tests:
            test_list = board_tests['only'] + ['device/board_test']
        if 'skip' in board_tests:
            for skip in board_tests['skip']:
                if skip in test_list:
                    test_list.remove(skip)

    err_count = 0
    for test in test_list:
        fw_dir = f'cmake-build/cmake-build-{name}/{test}'
        if not os.path.exists(fw_dir):
            fw_dir = f'examples/cmake-build-{name}/{test}'
        fw_name = f'{fw_dir}/{os.path.basename(test)}'
        print(f'{name:25} {test:30} ... ', end='')

        if not os.path.exists(fw_dir):
            print('Skip')
            continue

        # flash firmware. It may fail randomly, retry a few times
        for i in range(3):
            ret = globals()[f'flash_{flasher}'](board, fw_name)
            if ret.returncode == 0:
                break
            else:
                print(f'Flashing failed, retry {i+1}')
                time.sleep(1)

        if ret.returncode == 0:
            try:
                ret = globals()[f'test_{test.replace("/", "_")}'](board)
                print('OK')
            except Exception as e:
                err_count += 1
                print(STATUS_FAILED)
                print(f'  {e}')
        else:
            err_count += 1
            print(f'Flash {STATUS_FAILED}')
    return err_count


def main():
    """
    Hardware test on specified boards
    """
    global verbose

    parser = argparse.ArgumentParser()
    parser.add_argument('config_file', help='Configuration JSON file')
    parser.add_argument('-b', '--board', action='append', default=[], help='Boards to test, all if not specified')
    parser.add_argument('-v', '--verbose', action='store_true', help='Verbose output')
    args = parser.parse_args()

    config_file = args.config_file
    boards = args.board
    verbose = args.verbose

    # if config file is not found, try to find it in the same directory as this script
    if not os.path.exists(config_file):
        config_file = os.path.join(os.path.dirname(__file__), config_file)
    with open(config_file) as f:
        config = json.load(f)

    if len(boards) == 0:
        config_boards = config['boards']
    else:
        config_boards = [e for e in config['boards'] if e['name'] in boards]

    with Pool(processes=os.cpu_count()) as pool:
        err_count = sum(pool.map(test_board, config_boards))

    print()
    print("-" * 30)
    print(f'Total failed: {err_count}')
    print("-" * 30)
    sys.exit(err_count)


if __name__ == '__main__':
    main()
