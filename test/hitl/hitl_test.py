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
# SUBSYSTEM=="block", SUBSYSTEMS=="usb", MODE="0666", PROGRAM="/bin/sh -c 'echo $$ID_SERIAL_SHORT | rev | cut -c -8 | rev'", SYMLINK+="blkUSB_%c.%s{bInterfaceNumber}"
# SUBSYSTEM=="tty", SUBSYSTEMS=="usb", MODE="0666", PROGRAM="/bin/sh -c 'echo $$ID_SERIAL_SHORT | rev | cut -c -8 | rev'", SYMLINK+="ttyUSB_%c.%s{bInterfaceNumber}"

import os
import sys
import time
import serial
import subprocess
import json

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

def test_cdc_dual_ports(id):
    port1 = f'/dev/ttyUSB_{id[-8:]}.00'
    port2 = f'/dev/ttyUSB_{id[-8:]}.02'
    # Wait device enum
    timeout = 10
    while timeout:
        if os.path.exists(port1) and os.path.exists(port2):
            break
        time.sleep(1)
        timeout = timeout - 1

    assert os.path.exists(port1) and os.path.exists(port2), \
        'Device not available'

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

    print('cdc_dual_ports test done')

def test_cdc_msc(id):
    port  = f'/dev/ttyUSB_{id[-8:]}.00'
    block = f'/dev/blkUSB_{id[-8:]}.02'
    # Wait device enum
    timeout = 10
    while timeout:
        if os.path.exists(port) and os.path.exists(block):
            break
        time.sleep(1)
        timeout = timeout - 1

    assert os.path.exists(port) and os.path.exists(block), \
        'Device not available'

    # Echo test
    ser1 = serial.Serial(port)

    ser1.timeout = 1

    str = b"test_str"
    ser1.write(str)
    ser1.flush()
    assert ser1.read(100) == str, 'Port wrong data'

    # Block test
    f = open(block, 'rb')
    data = f.read()

    readme = \
    b"This is tinyusb's MassStorage Class demo.\r\n\r\n\
If you find any bugs or get any questions, feel free to file an\r\n\
issue at github.com/hathach/tinyusb"

    assert data[0x600:0x600 + len(readme)] == readme, 'Block wrong data'
    print('cdc_msc test done')

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

    print('dfu test done')

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

    print('dfu_runtime test done')


if __name__ == '__main__':
    with open(f'{os.path.dirname(__file__)}/hitl_config.json') as f:
        config = json.load(f)

    for device in config['devices']:
        print(f"Testing device:{device['device']}")
        for test in device['tests']:
            if device['debugger'].lower() == 'jlink':
                flash_jlink(device['debugger_sn'], device['device'], test['firmware'])
            else:
                # ToDo
                pass
            locals()[f'test_{test["name"]}'](device['uid'])
