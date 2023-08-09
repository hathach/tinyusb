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

import os
import sys
import time
import serial
import subprocess
import json


def get_serial_dev(id, product, ifnum):
    # get usb serial by id
    return f'/dev/serial/by-id/usb-TinyUSB_{product}_{id}-if{ifnum:02d}'


def get_disk_dev(id, lun):
    # get usb disk by id
    return f'/dev/disk/by-id/usb-TinyUSB_Mass_Storage_{id}-0:{lun}'


def get_hid_dev(id, product, event):
    return f'/dev/input/by-id/usb-TinyUSB_{product}_{id}-{event}'


def flash_jlink(sn, dev, firmware):
    script = ['halt', 'r', f'loadfile {firmware}', 'r', 'go', 'exit']
    f = open('flash.jlink', 'w')
    f.writelines(f'{s}\n' for s in script)
    f.close()
    ret = subprocess.run(f'JLinkExe -USB {sn} -device {dev} -if swd -JTAGConf -1,-1 -speed auto -NoGui 1 -ExitOnError 1 -CommandFile flash.jlink',
                         shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    stdout = ret.stdout.decode()
    os.remove('flash.jlink')
    assert ret.returncode == 0, 'Flash failed\n' + stdout


def test_board_test(id):
    # Dummy test
    pass

def test_cdc_dual_ports(id):
    port1 = get_serial_dev(id, "TinyUSB_Device", 0)
    port2 = get_serial_dev(id, "TinyUSB_Device", 2)

    # Wait device enum
    timeout = 10
    while timeout:
        if os.path.exists(port1) and os.path.exists(port2):
            break
        time.sleep(1)
        timeout = timeout - 1

    assert timeout, 'Device not available'

    # Echo test
    ser1 = serial.Serial(port1)
    ser2 = serial.Serial(port2)

    ser1.timeout = 1
    ser2.timeout = 1

    str1 = b"test_no1"
    ser1.write(str1)
    ser1.flush()
    assert ser1.read(100) == str1.lower(), 'Port1 wrong data'
    assert ser2.read(100) == str1.upper(), 'Port2 wrong data'

    str2 = b"test_no2"
    ser2.write(str2)
    ser2.flush()
    assert ser1.read(100) == str2.lower(), 'Port1 wrong data'
    assert ser2.read(100) == str2.upper(), 'Port2 wrong data'


def test_cdc_msc(id):
    port = get_serial_dev(id, "TinyUSB_Device", 0)
    file = f'/media/blkUSB_{id[-8:]}.02/README.TXT'
    # Wait device enum
    timeout = 10
    while timeout:
        if os.path.exists(port) and os.path.isfile(file):
            break
        time.sleep(1)
        timeout = timeout - 1

    assert timeout, 'Device not available'

    # Echo test
    ser1 = serial.Serial(port)

    ser1.timeout = 1

    str = b"test_str"
    ser1.write(str)
    ser1.flush()
    assert ser1.read(100) == str, 'CDC wrong data'

    # Block test
    f = open(file, 'rb')
    data = f.read()

    readme = \
    b"This is tinyusb's MassStorage Class demo.\r\n\r\n\
If you find any bugs or get any questions, feel free to file an\r\n\
issue at github.com/hathach/tinyusb"

    assert data == readme, 'MSC wrong data'

def test_dfu(id):
    # Wait device enum
    timeout = 10
    while timeout:
        ret = subprocess.run(f'dfu-util -l',
                         shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        stdout = ret.stdout.decode()
        if f'serial="{id}"' in stdout and 'Found DFU: [cafe:4000]' in stdout:
            break
        time.sleep(1)
        timeout = timeout - 1

    assert timeout, 'Device not available'

    # Test upload
    try:
        os.remove('dfu0')
        os.remove('dfu1')
    except OSError:
        pass

    ret = subprocess.run(f'dfu-util -S {id} -a 0 -U dfu0',
                         shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    assert ret.returncode == 0, 'Upload failed'

    ret = subprocess.run(f'dfu-util -S {id} -a 1 -U dfu1',
                         shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    assert ret.returncode == 0, 'Upload failed'

    with open('dfu0') as f:
        assert 'Hello world from TinyUSB DFU! - Partition 0' in f.read(),  'Wrong uploaded data'

    with open('dfu1') as f:
        assert 'Hello world from TinyUSB DFU! - Partition 1' in f.read(),  'Wrong uploaded data'

    os.remove('dfu0')
    os.remove('dfu1')


def test_dfu_runtime(id):
    # Wait device enum
    timeout = 10
    while timeout:
        ret = subprocess.run(f'dfu-util -l',
                         shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        stdout = ret.stdout.decode()
        if f'serial="{id}"' in stdout and 'Found Runtime: [cafe:4000]' in stdout:
            break
        time.sleep(1)
        timeout = timeout - 1

    assert timeout, 'Device not available'


def test_hid_boot_interface(id):
    kbd = get_hid_dev(id, 'TinyUSB_Device', 'event-kbd')
    mouse1 = get_hid_dev(id, 'TinyUSB_Device', 'if01-event-mouse')
    mouse2 = get_hid_dev(id, 'TinyUSB_Device', 'if01-mouse')
    # Wait device enum
    timeout = 10
    while timeout:
        if os.path.exists(kbd) and os.path.exists(mouse1) and os.path.exists(mouse2):
            break
        time.sleep(1)
        timeout = timeout - 1

    assert timeout, 'Device not available'


if __name__ == '__main__':
    if len(sys.argv) != 2:
        print('Usage:')
        print('python hitl_test.py config.json')
        sys.exit(-1)

    with open(f'{os.path.dirname(__file__)}/{sys.argv[1]}') as f:
        config = json.load(f)

    # all possible tests, board_test is last to disable board's usb
    all_tests = [
        'cdc_dual_ports', 'cdc_msc', 'dfu', 'dfu_runtime', 'hid_boot_interface', 'board_test'
    ]

    for board in config['boards']:
        print(f'Testing board:{board["name"]}')

        # default to all tests
        if 'tests' in board:
            test_list = board['tests']
        else:
            test_list = all_tests

        # remove skip_tests
        if 'tests_skip' in board:
            for skip in board['tests_skip']:
                if skip in test_list:
                    test_list.remove(skip)

        for test in test_list:
            mk_elf = f'examples/device/{test}/_build/{board["name"]}/{test}.elf'
            cmake_elf = f'cmake-build/cmake-build-{board["name"]}/device/{test}/{test}.elf'
            if os.path.isfile(cmake_elf):
                elf = cmake_elf
            elif os.path.isfile(mk_elf):
                elf = mk_elf
            else:
                print(f'Cannot find firmware file for {test}')
                sys.exit(-1)

            if board['debugger'].lower() == 'jlink':
                flash_jlink(board['debugger_sn'], board['cpu'], elf)
            else:
                # ToDo
                pass
            print(f'  {test} ...', end='')
            locals()[f'test_{test}'](board['uid'])
            print('OK')
