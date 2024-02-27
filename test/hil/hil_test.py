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
import click
import serial
import subprocess
import json
import glob

# for RPI double reset
try:
    import gpiozero
except ImportError:
    pass


ENUM_TIMEOUT = 10


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


# Currently not used, left as reference
def get_disk_dev(id, vendor_str, lun):
    # get usb disk by id
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
                ser = serial.Serial(port, timeout=1)
                break
            except serial.SerialException:
                pass
        time.sleep(0.5)
        timeout = timeout - 0.5
    assert timeout, 'Device not available or Cannot open port'
    return ser


def read_disk_file(id, fname):
    # on different self-hosted, the mount point is different
    file_list = [
        f'/media/blkUSB_{id[-8:]}.02/{fname}',
        f'/media/{os.getenv("USER")}/TinyUSB MSC/{fname}'
    ]
    timeout = ENUM_TIMEOUT
    while timeout:
        for file in file_list:
            if os.path.isfile(file):
                with open(file, 'rb') as f:
                    data = f.read()
                    return data

        time.sleep(1)
        timeout = timeout - 1

    assert timeout, 'Device not available'
    return None


# -------------------------------------------------------------
# Flashing firmware
# -------------------------------------------------------------
def flash_jlink(board, firmware):
    script = ['halt', 'r', f'loadfile {firmware}', 'r', 'go', 'exit']
    with open('flash.jlink', 'w') as f:
        f.writelines(f'{s}\n' for s in script)
    ret = subprocess.run(
        f'JLinkExe -USB {board["flasher_sn"]} {board["flasher_args"]} -if swd -JTAGConf -1,-1 -speed auto -NoGui 1 -ExitOnError 1 -CommandFile flash.jlink',
        shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    os.remove('flash.jlink')
    return ret


def flash_openocd(board, firmware):
    ret = subprocess.run(
        f'openocd -c "adapter serial {board["flasher_sn"]}" {board["flasher_args"]} -c "program {firmware} reset exit"',
        shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    return ret


def flash_esptool(board, firmware):
    port = get_serial_dev(board["flasher_sn"], None, None, 0)
    dir = os.path.dirname(firmware)
    with open(f'{dir}/config.env') as f:
        IDF_TARGET = json.load(f)['IDF_TARGET']
    with open(f'{dir}/flash_args') as f:
        flash_args = f.read().strip().replace('\n', ' ')
    command = (f'esptool.py --chip {IDF_TARGET} -p {port} {board["flasher_args"]} '
               f'--before=default_reset --after=hard_reset write_flash {flash_args}')
    ret = subprocess.run(command, shell=True, cwd=dir, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    return ret


def doublereset_with_rpi_gpio(board):
    # Off = 0 = Reset
    led = gpiozero.LED(board["flasher_reset_pin"])

    led.off()
    time.sleep(0.1)
    led.on()
    time.sleep(0.1)
    led.off()
    time.sleep(0.1)
    led.on()

def flash_bossac(board, firmware):
    # double reset to enter bootloader
    doublereset_with_rpi_gpio(board)

    port = get_serial_dev(board["uid"], board["flashser_vendor"], board["flasher_product"], 0)
    timeout = ENUM_TIMEOUT
    while timeout:
        if os.path.exists(port):
            break
        else:
            time.sleep(0.5)
            timeout = timeout - 0.5
    assert timeout, 'bossac bootloader is not available'
    # sleep a bit more for bootloader to be ready
    time.sleep(0.5)
    ret = subprocess.run(f'bossac --port {port} {board["flasher_args"]} -U -i -R -e -w {firmware}', shell=True, stdout=subprocess.PIPE,
                         stderr=subprocess.STDOUT)
    return ret

# -------------------------------------------------------------
# Tests
# -------------------------------------------------------------
def test_board_test(id):
    # Dummy test
    pass


def test_cdc_dual_ports(id):
    port1 = get_serial_dev(id, 'TinyUSB', "TinyUSB_Device", 0)
    port2 = get_serial_dev(id, 'TinyUSB', "TinyUSB_Device", 2)

    ser1 = open_serial_dev(port1)
    ser2 = open_serial_dev(port2)

    # Echo test
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
    # Echo test
    port = get_serial_dev(id, 'TinyUSB', "TinyUSB_Device", 0)
    ser = open_serial_dev(port)

    str = b"test_str"
    ser.write(str)
    ser.flush()
    assert ser.read(100) == str, 'CDC wrong data'

    # Block test
    data = read_disk_file(id, 'README.TXT')
    readme = \
    b"This is tinyusb's MassStorage Class demo.\r\n\r\n\
If you find any bugs or get any questions, feel free to file an\r\n\
issue at github.com/hathach/tinyusb"

    assert data == readme, 'MSC wrong data'


def test_cdc_msc_freertos(id):
    test_cdc_msc(id)


def test_dfu(id):
    # Wait device enum
    timeout = ENUM_TIMEOUT
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
        assert 'Hello world from TinyUSB DFU! - Partition 0' in f.read(), 'Wrong uploaded data'

    with open('dfu1') as f:
        assert 'Hello world from TinyUSB DFU! - Partition 1' in f.read(), 'Wrong uploaded data'

    os.remove('dfu0')
    os.remove('dfu1')


def test_dfu_runtime(id):
    # Wait device enum
    timeout = ENUM_TIMEOUT
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
    kbd = get_hid_dev(id, 'TinyUSB', 'TinyUSB_Device', 'event-kbd')
    mouse1 = get_hid_dev(id, 'TinyUSB', 'TinyUSB_Device', 'if01-event-mouse')
    mouse2 = get_hid_dev(id, 'TinyUSB', 'TinyUSB_Device', 'if01-mouse')
    # Wait device enum
    timeout = ENUM_TIMEOUT
    while timeout:
        if os.path.exists(kbd) and os.path.exists(mouse1) and os.path.exists(mouse2):
            break
        time.sleep(1)
        timeout = timeout - 1

    assert timeout, 'Device not available'


def test_hid_composite_freertos(id):
    # TODO implement later
    pass


# -------------------------------------------------------------
# Main
# -------------------------------------------------------------
@click.command()
@click.argument('config_file')
@click.option('-b', '--board', multiple=True, default=None, help='Boards to test, all if not specified')
def main(config_file, board):
    """
    Hardware test on specified boards
    """
    config_file = os.path.join(os.path.dirname(__file__), config_file)
    with open(config_file) as f:
        config = json.load(f)

    # all possible tests
    all_tests = [
        'cdc_dual_ports', 'cdc_msc', 'dfu', 'dfu_runtime', 'hid_boot_interface',
    ]

    if len(board) == 0:
        config_boards = config['boards']
    else:
        config_boards = [e for e in config['boards'] if e['name'] in board]

    for item in config_boards:
        print(f'Testing board:{item["name"]}')
        flasher = item['flasher'].lower()

        # default to all tests
        if 'tests' in item:
            test_list = item['tests']
        else:
            test_list = all_tests

        # board_test is added last to disable board's usb
        test_list.append('board_test')

        # remove skip_tests
        if 'tests_skip' in item:
            for skip in item['tests_skip']:
                if skip in test_list:
                    test_list.remove(skip)

        for test in test_list:
            fw_list = [
                # cmake: esp32 & samd51 use .bin file
                f'cmake-build/cmake-build-{item["name"]}/device/{test}/{test}.elf',
                f'cmake-build/cmake-build-{item["name"]}/device/{test}/{test}.bin',
                # make
                f'examples/device/{test}/_build/{item["name"]}/{test}.elf'
            ]

            fw = None
            for f in fw_list:
                if os.path.isfile(f):
                    fw = f
                    break

            if fw is None:
                print(f'Cannot find binary file for {test}')
                sys.exit(-1)

            print(f'  {test} ...', end='')

            # flash firmware. It may fail randomly, retry a few times
            for i in range(3):
                ret = globals()[f'flash_{flasher}'](item, fw)
                if ret.returncode == 0:
                    break
                else:
                    print(f'Flashing failed, retry {i+1}')
                    time.sleep(1)

            assert ret.returncode == 0, 'Flash failed\n' + ret.stdout.decode()

            # run test
            globals()[f'test_{test}'](item['uid'])

            print('OK')


if __name__ == '__main__':
    main()
