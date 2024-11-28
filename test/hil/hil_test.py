#!/usr/bin/env python3
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

WCH_RISCV_CONTENT = """
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

# -------------------------------------------------------------
# Path
# -------------------------------------------------------------
OPENCOD_ADI_PATH = f'{os.getenv("HOME")}/app/openocd_adi'
TINYUSB_ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

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
                ser = serial.Serial(port, baudrate=115200, timeout=5)
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
def run_cmd(cmd, cwd=None):
    r = subprocess.run(cmd, cwd=cwd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
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
    flasher = board['flasher']
    script = ['halt', 'r', f'loadfile {firmware}.elf', 'r', 'go', 'exit']
    f_jlink = f'{board["name"]}_{os.path.basename(firmware)}.jlink'
    with open(f_jlink, 'w') as f:
        f.writelines(f'{s}\n' for s in script)
    ret = run_cmd(f'JLinkExe -USB {flasher["uid"]} {flasher["args"]} -if swd -JTAGConf -1,-1 -speed auto -NoGui 1 -ExitOnError 1 -CommandFile {f_jlink}')
    os.remove(f_jlink)
    return ret


def reset_jlink(board):
    flasher = board['flasher']
    script = ['halt', 'r', 'go', 'exit']
    f_jlink = f'{board["name"]}_reset.jlink'
    if not os.path.exists(f_jlink):
        with open(f_jlink, 'w') as f:
            f.writelines(f'{s}\n' for s in script)
    ret = run_cmd(f'JLinkExe -USB {flasher["uid"]} {flasher["args"]} -if swd -JTAGConf -1,-1 -speed auto -NoGui 1 -ExitOnError 1 -CommandFile {f_jlink}')
    return ret


def flash_stlink(board, firmware):
    flasher = board['flasher']
    return run_cmd(f'STM32_Programmer_CLI --connect port=swd sn={flasher["uid"]} --write {firmware}.elf --go')


def reset_stlink(board):
    flasher = board['flasher']
    return run_cmd(f'STM32_Programmer_CLI --connect port=swd sn={flasher["uid"]} --rst --go')

def flash_stflash(board, firmware):
    flasher = board['flasher']
    ret = run_cmd(f'st-flash --serial {flasher["uid"]} write {firmware}.bin 0x8000000')
    return ret


def reset_stflash(board):
    flasher = board['flasher']
    return subprocess.CompletedProcess(args=['dummy'], returncode=0)


def flash_openocd(board, firmware):
    flasher = board['flasher']
    ret = run_cmd(f'openocd -c "tcl_port disabled" -c "gdb_port disabled" -c "adapter serial {flasher["uid"]}" '
                  f'{flasher["args"]} -c init -c halt -c "program {firmware}.elf verify" -c reset -c exit')
    return ret


def reset_openocd(board):
    flasher = board['flasher']
    ret = run_cmd(f'openocd -c "tcl_port disabled" -c "gdb_port disabled" -c "adapter serial {flasher["uid"]}" '
                  f'{flasher["args"]} -c "reset exit"')
    return ret


def flash_openocd_wch(board, firmware):
    flasher = board['flasher']
    f_wch = f"wch-riscv_{board['uid']}.cfg"
    if not os.path.exists(f_wch):
        with open(f_wch, 'w') as file:
            file.write(WCH_RISCV_CONTENT)

    ret = run_cmd(f'openocd_wch -c "adapter serial {flasher["uid"]}" -f {f_wch} '
                  f'-c "program {firmware}.elf reset exit"')
    return ret


def reset_openocd_wch(board):
    flasher = board['flasher']
    f_wch = f"wch-riscv_{board['uid']}.cfg"
    if not os.path.exists(f_wch):
        with open(f_wch, 'w') as file:
            file.write(WCH_RISCV_CONTENT)

    ret = run_cmd(f'openocd_wch -c "adapter serial {flasher["uid"]}" -f {f_wch} -c "program reset exit"')
    return ret


def flash_openocd_adi(board, firmware):
    flasher = board['flasher']
    ret = run_cmd(f'{OPENCOD_ADI_PATH}/src/openocd -c "adapter serial {flasher["uid"]}" -s {OPENCOD_ADI_PATH}/tcl '
                  f'{flasher["args"]} -c "program {firmware}.elf reset exit"')
    return ret


def reset_openocd_adi(board):
    flasher = board['flasher']
    ret = run_cmd(f'{OPENCOD_ADI_PATH}/src/openocd -c "adapter serial {flasher["uid"]}" -s {OPENCOD_ADI_PATH}/tcl '
                  f'{flasher["args"]} -c "program reset exit"')
    return ret


def flash_wlink_rs(board, firmware):
    flasher = board['flasher']
    # wlink use index for probe selection and lacking usb serial support
    ret = run_cmd(f'wlink flash {firmware}.elf')
    return ret


def reset_wlink_rs(board):
    flasher = board['flasher']
    # wlink use index for probe selection and lacking usb serial support
    ret = run_cmd(f'wlink reset')
    return ret


def flash_esptool(board, firmware):
    flasher = board['flasher']
    port = get_serial_dev(flasher["uid"], None, None, 0)
    fw_dir = os.path.dirname(f'{firmware}.bin')
    with open(f'{fw_dir}/config.env') as f:
        idf_target = json.load(f)['IDF_TARGET']
    with open(f'{fw_dir}/flash_args') as f:
        flash_args = f.read().strip().replace('\n', ' ')
    command = (f'esptool.py --chip {idf_target} -p {port} {flasher["args"]} '
               f'--before=default_reset --after=hard_reset write_flash {flash_args}')
    ret = run_cmd(command, cwd=fw_dir)
    return ret


def reset_esptool(board):
    flasher = board['flasher']
    return subprocess.CompletedProcess(args=['dummy'], returncode=0)


def flash_uniflash(board, firmware):
    flasher = board['flasher']
    ret = run_cmd(f'dslite.sh {flasher["args"]} -f {firmware}.hex')
    return ret


def reset_uniflash(board):
    flasher = board['flasher']
    return subprocess.CompletedProcess(args=['dummy'], returncode=0)


# -------------------------------------------------------------
# Tests: dual
# -------------------------------------------------------------
def test_dual_host_info_to_device_cdc(board):
    uid = board['uid']
    declared_devs = [f'{d["vid_pid"]}_{d["serial"]}' for d in board['tests']['dev_attached']]
    port = get_serial_dev(uid, 'TinyUSB', "TinyUSB_Device", 0)
    ser = open_serial_dev(port)

    # read from cdc, first line should contain vid/pid and serial
    data = ser.read(1000)
    ser.close()
    if len(data) == 0:
        assert False, 'No data from device'
    lines = data.decode('utf-8').splitlines()

    enum_dev_sn = []
    for l in lines:
        vid_pid_sn = re.search(r'ID ([0-9a-fA-F]+):([0-9a-fA-F]+) SN (\w+)', l)
        if vid_pid_sn:
            print(f'\r\n  {l} ', end='')
            enum_dev_sn.append(f'{vid_pid_sn.group(1)}_{vid_pid_sn.group(2)}_{vid_pid_sn.group(3)}')

    if set(declared_devs) != set(enum_dev_sn):
        failed_msg = f'Expected {declared_devs}, Enumerated {enum_dev_sn}'
        assert False, failed_msg
    return 0


# -------------------------------------------------------------
# Tests: host
# -------------------------------------------------------------
def test_host_device_info(board):
    flasher = board['flasher']
    declared_devs = [f'{d["vid_pid"]}_{d["serial"]}' for d in board['tests']['dev_attached']]

    port = get_serial_dev(flasher["uid"], None, None, 0)
    ser = open_serial_dev(port)

    # reset device since we can miss the first line
    ret = globals()[f'reset_{flasher["name"].lower()}'](board)
    assert ret.returncode == 0,  'Failed to reset device'

    data = ser.read(1000)
    ser.close()
    if len(data) == 0:
        assert False, 'No data from device'

    lines = data.decode('utf-8').splitlines()
    enum_dev_sn = []
    for l in lines:
        vid_pid_sn = re.search(r'ID ([0-9a-fA-F]+):([0-9a-fA-F]+) SN (\w+)', l)
        if vid_pid_sn:
            print(f'\r\n  {l} ', end='')
            enum_dev_sn.append(f'{vid_pid_sn.group(1)}_{vid_pid_sn.group(2)}_{vid_pid_sn.group(3)}')

    if set(declared_devs) != set(enum_dev_sn):
        failed_msg = f'Expected {declared_devs}, Enumerated {enum_dev_sn}'
        assert False, failed_msg

    return 0


# -------------------------------------------------------------
# Tests: device
# -------------------------------------------------------------
def test_device_board_test(board):
    # Dummy test
    pass


def test_device_cdc_dual_ports(board):
    uid = board['uid']
    port = [
        get_serial_dev(uid, 'TinyUSB', "TinyUSB_Device", 0),
        get_serial_dev(uid, 'TinyUSB', "TinyUSB_Device", 2)
    ]
    ser = [open_serial_dev(p) for p in port]

    str_test = [ b"test_no1", b"test_no2" ]
    # Echo test write to each port and read back
    for i in range(len(str_test)):
        s = str_test[i]
        l = len(s)
        ser[i].write(s)
        ser[i].flush()
        rd = [ ser[i].read(l) for i in range(len(ser)) ]
        assert rd[0] == s.lower(), f'Port1 wrong data: expected {s.lower()} was {rd[0]}'
        assert rd[1] == s.upper(), f'Port2 wrong data: expected {s.upper()} was {rd[1]}'
    ser[0].close()
    ser[1].close()


def test_device_cdc_msc(board):
    uid = board['uid']
    # Echo test
    port = get_serial_dev(uid, 'TinyUSB', "TinyUSB_Device", 0)
    ser = open_serial_dev(port)

    test_str = b"test_str"
    ser.write(test_str)
    ser.flush()
    rd_str = ser.read(len(test_str))
    ser.close()
    assert  rd_str == test_str, f'CDC wrong data: expected: {test_str} was {rd_str}'

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
        ret = run_cmd(f'dfu-util -l')
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
        ret = run_cmd(f'dfu-util -l')
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
# device tests
# note don't test 2 examples with cdc or 2 msc next to each other
device_tests = [
    'device/cdc_dual_ports',
    'device/dfu',
    'device/cdc_msc',
    'device/dfu_runtime',
    'device/cdc_msc_freertos',
    'device/hid_boot_interface',
]

dual_tests = [
    'dual/host_info_to_device_cdc',
]

host_test = [
    'host/device_info',
]


def test_board(board):
    name = board['name']
    flasher = board['flasher']

    # default to all tests
    test_list = []

    if 'tests' in board:
        board_tests = board['tests']
        if 'device' in board_tests and board_tests['device'] == True:
            test_list += list(device_tests)
        if 'dual' in board_tests and board_tests['dual'] == True:
            test_list += dual_tests
        if 'host' in board_tests and board_tests['host'] == True:
            test_list += host_test
        if 'only' in board_tests:
            test_list = board_tests['only']
        if 'skip' in board_tests:
            for skip in board_tests['skip']:
                if skip in test_list:
                    test_list.remove(skip)
                    print(f'{name:25} {skip:30} ... Skip')

    # board_test is added last to disable board's usb
    test_list.append('device/board_test')

    err_count = 0
    flags_on_list = [""]
    if 'build' in board and 'flags_on' in board['build']:
        flags_on_list = board['build']['flags_on']

    for f1 in flags_on_list:
        f1_str = ""
        if f1 != "":
            f1_str = '-f1_' + f1.replace(' ', '_')
        for test in test_list:
            fw_dir = f'{TINYUSB_ROOT}/cmake-build/cmake-build-{name}{f1_str}/{test}'
            if not os.path.exists(fw_dir):
                fw_dir = f'{TINYUSB_ROOT}/examples/cmake-build-{name}{f1_str}/{test}'
            fw_name = f'{fw_dir}/{os.path.basename(test)}'
            print(f'{name+f1_str:40} {test:30} ... ', end='')

            if not os.path.exists(fw_dir) or not (os.path.exists(f'{fw_name}.elf') or os.path.exists(f'{fw_name}.bin')):
                print('Skip (no binary)')
                continue

            # flash firmware. It may fail randomly, retry a few times
            max_rety = 2
            for i in range(max_rety):
                ret = globals()[f'flash_{flasher["name"].lower()}'](board, fw_name)
                if ret.returncode == 0:
                    try:
                        globals()[f'test_{test.replace("/", "_")}'](board)
                        print('OK')
                        break
                    except Exception as e:
                        if i == max_rety - 1:
                            err_count += 1
                            print(STATUS_FAILED)
                            print(f'  {e}')
                        else:
                            print()
                            print(f'  Test failed: {e}, retry {i+1}')
                            time.sleep(1)
                else:
                    print(f'Flashing failed, retry {i+1}')
                    time.sleep(1)

            if ret.returncode != 0:
                err_count += 1
                print(f'Flash {STATUS_FAILED}')

    return err_count


def main():
    """
    Hardware test on specified boards
    """
    global verbose

    duration = time.time()

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

    duration = time.time() - duration
    print()
    print("-" * 30)
    print(f'Total failed: {err_count} in {duration:.1f}s')
    print("-" * 30)
    sys.exit(err_count)


if __name__ == '__main__':
    main()
