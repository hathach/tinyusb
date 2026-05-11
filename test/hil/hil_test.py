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
import random
import re
import select
import sys
import time
import warnings
import signal
from pathlib import Path
from typing import Any, TypedDict, NotRequired, cast

# Suppress pkg_resources deprecation warning from fs module
warnings.filterwarnings("ignore", message="pkg_resources is deprecated")
# Suppress pyfatfs unclean unmount warning
warnings.filterwarnings("ignore", message="Filesystem was not cleanly unmounted")

import serial
import subprocess
import json
import glob
from multiprocessing import Pool
from multiprocessing import TimeoutError as MpTimeoutError
import fs
import hashlib
import ctypes
from pymtp import MTP
import string

ENUM_TIMEOUT = 15

STATUS_OK = "\033[32mOK\033[0m"
STATUS_FAILED = "\033[31mFailed\033[0m"
STATUS_SKIPPED = "\033[33mSkipped\033[0m"

verbose = False
test_only = []
board_test = {}
build_dir = 'cmake-build'
skip_flash = False

class FlasherCfg(TypedDict):
    name: str
    uid: str
    args: str


class AttachedDevCfg(TypedDict, total=False):
    vid_pid: str
    serial: str
    is_cdc: bool
    is_msc: bool
    block_count: int
    block_size: int


class TestsCfg(TypedDict, total=False):
    device: bool
    dual: bool
    host: bool
    only: list[str]
    skip: list[str]
    dev_attached: list[AttachedDevCfg]


class BuildCfg(TypedDict, total=False):
    flags_on: list[str]
    args: list[str]


class Board(TypedDict):
    name: str
    uid: str
    tests: TestsCfg
    flasher: FlasherCfg
    build: NotRequired[BuildCfg]


class HilConfig(TypedDict):
    boards: list[Board]

CMD_TIMEOUT = int(os.getenv('HIL_CMD_TIMEOUT', '180'))
POOL_TIMEOUT = int(os.getenv('HIL_POOL_TIMEOUT', '3000'))


def cmd_stdout_text(out: Any) -> str:
    if out is None:
        return ''
    if isinstance(out, bytes):
        return out.decode('utf-8', errors='ignore')
    return str(out)

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

MSC_README_TXT = \
b"This is tinyusb's MassStorage Class demo.\r\n\r\n\
If you find any bugs or get any questions, feel free to file an\r\n\
issue at github.com/hathach/tinyusb"

# -------------------------------------------------------------
# Path
# -------------------------------------------------------------
OPENCOD_ADI_PATH = Path.home() / 'app' / 'openocd_adi'
TINYUSB_ROOT = Path(__file__).resolve().parents[2]

# get usb serial by id
def get_serial_dev(id, vendor_str, product_str, ifnum):
    if vendor_str and product_str:
        # known vendor and product
        vendor_str = vendor_str.replace(' ', '_')
        product_str = product_str.replace(' ', '_')
        return f'/dev/serial/by-id/usb-{vendor_str}_{product_str}_{id}-if{ifnum:02d}'
    else:
        # just use id: mostly for cp210x/ftdi flasher
        pattern = f'/dev/serial/by-id/usb-*_{id}-if*'
        port_list = glob.glob(pattern)
        if len(port_list) == 0:
            raise RuntimeError(f'No serial device found for {pattern}')
        return port_list[0]


# get usb disk by id
def get_disk_dev(id, vendor_str, lun):
    return f'/dev/disk/by-id/usb-{vendor_str}_Mass_Storage_{id}-0:{lun}'


def get_hid_dev(id, vendor_str, product_str, event):
    return f'/dev/input/by-id/usb-{vendor_str}_{product_str}_{id}-{event}'


def open_serial_dev(port: str):
    timeout = ENUM_TIMEOUT
    ser = None
    while timeout > 0:
        if os.path.exists(port):
            try:
                ser = serial.Serial(port, baudrate=115200, timeout=5)
                break
            except serial.SerialException:
                print(f'serial {port} not reaady {timeout} sec')
                pass
        time.sleep(0.1)
        timeout -= 0.1

    assert timeout > 0, f'Cannot open port f{port}' if os.path.exists(port) else f'Port {port} not existed'
    assert ser is not None
    return ser


def read_disk_file(uid: str, lun: int, fname: str) -> bytes:
    # open_fs("fat://{dev}) require 'pip install pyfatfs'
    dev = get_disk_dev(uid, 'TinyUSB', lun)
    timeout = ENUM_TIMEOUT
    while timeout > 0:
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

    raise AssertionError(f'Storage {dev} not existed')


def open_mtp_dev(uid):
    mtp = MTP()
    timeout = ENUM_TIMEOUT
    while timeout > 0:
        # unmount gio/gvfs MTP mount which blocks libmtp from accessing the device
        subprocess.run(f"gio mount -u mtp://TinyUsb_TinyUsb_Device_{uid}/",
                       shell=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        for raw in mtp.detect_devices():
            mtp.device = mtp.mtp.LIBMTP_Open_Raw_Device(ctypes.byref(raw))
            if mtp.device:
                sn = mtp.get_serialnumber().decode('utf-8')
                if sn == uid:
                    return mtp
                mtp.disconnect()
        time.sleep(1)
        timeout -= 1
    return None


def get_printer_dev(id: str, vendor_str, product_str, ifnum: int):
    """Find /dev/usb/lpX by matching USB serial, vendor, product, and interface number via sysfs"""
    vendor_str = vendor_str.replace(' ', '_') if vendor_str else ''
    product_str = product_str.replace(' ', '_') if product_str else ''
    for lp in glob.glob('/sys/class/usbmisc/lp*'):
        try:
            sn = open(f'{lp}/device/../serial').read().strip()
            if sn == id:
                return f'/dev/usb/{os.path.basename(lp)}'
        except (FileNotFoundError, PermissionError, ValueError):
            pass
    return None


def open_printer_dev(id: str, vendor_str, product_str, ifnum: int) -> str:
    """Wait for printer device to enumerate and return its path"""
    timeout = ENUM_TIMEOUT
    while timeout > 0:
        lp_dev = get_printer_dev(id, vendor_str, product_str, ifnum)
        if lp_dev and os.path.exists(lp_dev):
            return lp_dev
        time.sleep(1)
        timeout -= 1
    assert False, f'Printer device not found for {id} if{ifnum:02d}'


# -------------------------------------------------------------
# Flashing firmware
# -------------------------------------------------------------
def run_cmd(cmd: str, cwd: str | None = None, timeout: int = CMD_TIMEOUT) -> subprocess.CompletedProcess:
    popen_kwargs = {
        'cwd': cwd,
        'shell': True,
        'stdout': subprocess.PIPE,
        'stderr': subprocess.STDOUT,
        'text': True,
        'encoding': 'utf-8',
        'errors': 'replace',
    }
    if os.name != 'nt':
        popen_kwargs['preexec_fn'] = os.setsid

    p = subprocess.Popen(cmd, **popen_kwargs)
    try:
        out, _ = p.communicate(timeout=timeout)
        r = subprocess.CompletedProcess(args=cmd, returncode=p.returncode, stdout=out)
    except subprocess.TimeoutExpired as ex:
        if os.name != 'nt':
            try:
                os.killpg(p.pid, signal.SIGKILL)
            except ProcessLookupError:
                pass
        else:
            p.kill()
        out, _ = p.communicate()
        timeout_out = ex.stdout or out or b''
        title = f'COMMAND TIMEOUT ({timeout}s): {cmd}'
        print()
        if os.getenv('CI'):
            print(f"::group::{title}")
            print(cmd_stdout_text(timeout_out))
            print(f"::endgroup::")
        else:
            print(title)
            print(cmd_stdout_text(timeout_out))
        return subprocess.CompletedProcess(args=cmd, returncode=124, stdout=timeout_out)

    if r.returncode != 0:
        title = f'COMMAND FAILED: {cmd}'
        print()
        if os.getenv('CI'):
            print(f"::group::{title}")
            print(cmd_stdout_text(r.stdout))
            print(f"::endgroup::")
        else:
            print(title)
            print(cmd_stdout_text(r.stdout))
    elif verbose:
        print(cmd)
        print(cmd_stdout_text(r.stdout))
    return r


def flash_jlink(board: Board, firmware: str) -> subprocess.CompletedProcess:
    flasher = board['flasher']
    script = ['halt', 'r', f'loadfile {firmware}.elf', 'r', 'go', 'exit']
    f_jlink = Path(f'{board["name"]}_{Path(firmware).name}.jlink')
    with f_jlink.open('w') as f:
        f.writelines(f'{s}\n' for s in script)
    ret = run_cmd(f'JLinkExe -USB {flasher["uid"]} {flasher["args"]} -if swd -JTAGConf -1,-1 -speed auto -NoGui 1 -ExitOnError 1 -CommandFile {f_jlink}')
    f_jlink.unlink(missing_ok=True)
    return ret


def reset_jlink(board: Board) -> subprocess.CompletedProcess:
    flasher = board['flasher']
    script = ['halt', 'r', 'go', 'exit']
    f_jlink = Path(f'{board["name"]}_reset.jlink')
    if not f_jlink.exists():
        with f_jlink.open('w') as f:
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
                  f'{flasher["args"]} -c "init; halt; program {firmware}.elf verify; reset; exit"')
    return ret


def reset_openocd(board):
    flasher = board['flasher']
    ret = run_cmd(f'openocd -c "tcl_port disabled" -c "gdb_port disabled" -c "adapter serial {flasher["uid"]}" '
                  f'{flasher["args"]} -c "init; reset run; exit"')
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


def flash_openocd_adi(board: Board, firmware: str) -> subprocess.CompletedProcess:
    flasher = board['flasher']
    openocd = OPENCOD_ADI_PATH / 'src' / 'openocd'
    tcl_dir = OPENCOD_ADI_PATH / 'tcl'
    ret = run_cmd(f'{openocd} -c "adapter serial {flasher["uid"]}" -s {tcl_dir} '
                  f'{flasher["args"]} -c "program {firmware}.elf reset exit"')
    return ret


def reset_openocd_adi(board: Board) -> subprocess.CompletedProcess:
    flasher = board['flasher']
    openocd = OPENCOD_ADI_PATH / 'src' / 'openocd'
    tcl_dir = OPENCOD_ADI_PATH / 'tcl'
    ret = run_cmd(f'{openocd} -c "adapter serial {flasher["uid"]}" -s {tcl_dir} '
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


def flash_esptool(board: Board, firmware: str) -> subprocess.CompletedProcess:
    flasher = board['flasher']
    port = get_serial_dev(flasher["uid"], None, None, 0)
    fw_dir = Path(f'{firmware}.bin').parent
    with (fw_dir / 'config.env').open() as f:
        idf_target = json.load(f)['IDF_TARGET']
    with (fw_dir / 'flash_args').open() as f:
        flash_args = f.read().strip().replace('\n', ' ')
    command = (f'esptool --chip {idf_target} -p {port} {flasher["args"]} '
               f'--before=default_reset --after=hard_reset write_flash {flash_args}')
    ret = run_cmd(command, cwd=str(fw_dir))
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
    ser.timeout = 0.1

    # read until all expected devices are enumerated
    data = b''
    timeout = ENUM_TIMEOUT
    while timeout > 0:
        new_data = ser.read(ser.in_waiting or 1)
        if new_data:
            data += new_data
        # check if all devices found
        enum_dev_sn = []
        for l in data.decode('utf-8', errors='ignore').splitlines():
            vid_pid_sn = re.search(r'ID ([0-9a-fA-F]+):([0-9a-fA-F]+) SN (\w+)', l)
            if vid_pid_sn:
                enum_dev_sn.append(f'{vid_pid_sn.group(1)}_{vid_pid_sn.group(2)}_{vid_pid_sn.group(3)}')
        if set(declared_devs).issubset(set(enum_dev_sn)):
            break
        time.sleep(0.1)
        timeout -= 0.1
    ser.close()

    if len(data) == 0:
        assert False, 'No data from device'
    lines = data.decode('utf-8', errors='ignore').splitlines()

    enum_dev_sn = []
    for l in lines:
        vid_pid_sn = re.search(r'ID ([0-9a-fA-F]+):([0-9a-fA-F]+) SN (\w+)', l)
        if vid_pid_sn:
            print(f'\r\n  {l} ', end='')
            enum_dev_sn.append(f'{vid_pid_sn.group(1)}_{vid_pid_sn.group(2)}_{vid_pid_sn.group(3)}')

    if set(declared_devs) != set(enum_dev_sn):
        failed_msg = f'Expected {declared_devs}, Enumerated {enum_dev_sn}'
        print('\n'.join(lines))
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
    ser.timeout = 0.1

    # reset device since we can miss the first line
    ret = globals()[f'reset_{flasher["name"].lower()}'](board)
    assert ret.returncode == 0, 'Failed to reset device'

    # read until all expected devices are enumerated
    data = b''
    timeout = ENUM_TIMEOUT
    while timeout > 0:
        new_data = ser.read(ser.in_waiting or 1)
        if new_data:
            data += new_data
        # check if all devices found
        enum_dev_sn = []
        for l in data.decode('utf-8', errors='ignore').splitlines():
            vid_pid_sn = re.search(r'ID ([0-9a-fA-F]+):([0-9a-fA-F]+) SN (\w+)', l)
            if vid_pid_sn:
                enum_dev_sn.append(f'{vid_pid_sn.group(1)}_{vid_pid_sn.group(2)}_{vid_pid_sn.group(3)}')
        if set(declared_devs).issubset(set(enum_dev_sn)):
            break
        time.sleep(0.1)
        timeout -= 0.1
    ser.close()

    if len(data) == 0:
        assert False, 'No data from device'
    lines = data.decode('utf-8', errors='ignore').splitlines()

    enum_dev_sn = []
    for l in lines:
        vid_pid_sn = re.search(r'ID ([0-9a-fA-F]+):([0-9a-fA-F]+) SN (\w+)', l)
        if vid_pid_sn:
            print(f'\r\n  {l} ', end='')
            enum_dev_sn.append(f'{vid_pid_sn.group(1)}_{vid_pid_sn.group(2)}_{vid_pid_sn.group(3)}')

    if set(declared_devs) != set(enum_dev_sn):
        failed_msg = f'Expected {declared_devs}, Enumerated {enum_dev_sn}'
        print('\n'.join(lines))
        assert False, failed_msg
    return 0


def check_msc_info(lines, msc_devs):
    """Print MSC info and verify block_count/block_size against config"""
    inquiry = ''
    disk_size = ''
    for l in lines:
        if re.match(r'^[A-Za-z].*\s+(rev\s+|[0-9])', l) and 'Disk Size' not in l:
            inquiry = l.strip()
        if 'Disk Size' in l:
            disk_size = l.strip()
    if inquiry or disk_size:
        print(f'\r\n  {inquiry} {disk_size} ', end='')
    # Verify block_count and block_size from "Disk Size: COUNT SIZE-byte blocks: N MB"
    if disk_size and msc_devs:
        m = re.match(r'Disk Size:\s+(\d+)\s+(\d+)-byte blocks', disk_size)
        if m:
            actual_count = int(m.group(1))
            actual_size = int(m.group(2))
            for dev in msc_devs:
                exp_count = dev.get('block_count')
                exp_size = dev.get('block_size')
                if exp_count and actual_count == exp_count:
                    assert actual_size == exp_size, (
                        f'MSC block_size mismatch: expected {exp_size}, got {actual_size}')
                    break


def test_host_cdc_msc_hid(board):
    flasher = board['flasher']
    dev_attached = board['tests'].get('dev_attached', [])
    cdc_devs = [d for d in dev_attached if d.get('is_cdc')]
    msc_devs = [d for d in dev_attached if d.get('is_msc')]
    if not cdc_devs and not msc_devs:
        return 'skipped'

    port = get_serial_dev(flasher["uid"], None, None, 0)
    ser = open_serial_dev(port)
    ser.timeout = 0.1

    # reset device to catch mount messages
    ret = globals()[f'reset_{flasher["name"].lower()}'](board)
    assert ret.returncode == 0, 'Failed to reset device'

    # Wait for all expected mount messages
    data = b''
    timeout = ENUM_TIMEOUT
    wait_cdc = len(cdc_devs) > 0
    wait_msc = len(msc_devs) > 0
    while timeout > 0:
        new_data = ser.read(ser.in_waiting or 1)
        if new_data:
            data += new_data
            cdc_ok = (not wait_cdc) or (b'CDC Interface is mounted' in data)
            msc_ok = (not wait_msc) or (b'Disk Size' in data)
            if cdc_ok and msc_ok:
                break
        time.sleep(0.1)
        timeout -= 0.1

    # Lookup serial chip name from vid_pid
    vid_pid_name = {
        '0403_6001': 'FTDI', '0403_6010': 'FTDI', '0403_6011': 'FTDI', '0403_6014': 'FTDI',
        '10c4_ea60': 'CP210x', '10c4_ea70': 'CP210x',
        '067b_2303': 'PL2303', '067b_23a3': 'PL2303',
        '1a86_7523': 'CH340', '1a86_7522': 'CH340',
        '1a86_55d3': 'CH9102', '1a86_55d4': 'CH9102',
    }

    lines = data.decode('utf-8', errors='ignore').splitlines()

    # Verify and print CDC mount
    if cdc_devs:
        assert b'CDC Interface is mounted' in data, 'CDC device not mounted on host'
        dev = cdc_devs[0]
        chip_name = vid_pid_name.get(dev['vid_pid'], dev['vid_pid'])
        for l in lines:
            if 'CDC Interface is mounted' in l:
                print(f'\r\n  {chip_name}: {l} ', end='')

    # Verify and print MSC mount (inquiry + disk size)
    if msc_devs:
        assert b'MassStorage device is mounted' in data, 'MSC device not mounted on host'
        assert b'Disk Size' in data, 'MSC Disk Size not reported'
        check_msc_info(lines, msc_devs)

    # CDC echo test via flasher serial
    if not cdc_devs:
        ser.close()
        return

    time.sleep(2)
    ser.read(ser.in_waiting)
    ser.reset_input_buffer()

    def rand_ascii(length):
        return "".join(random.choices(string.ascii_letters + string.digits, k=length)).encode("ascii")

    packet_size = 64

    # Echo test: write random 1-packet_size chunks, wait for echo before sending next
    echo_len = 1024
    echo_data = rand_ascii(echo_len)
    ser.reset_input_buffer()
    offset = 0
    while offset < echo_len:
        chunk_size = min(random.randint(1, packet_size), echo_len - offset)
        ser.write(echo_data[offset:offset + chunk_size])
        ser.flush()
        # wait until this chunk is echoed back
        echo = b''
        t_end = time.monotonic() + 1.0
        while time.monotonic() < t_end and len(echo) < chunk_size:
            rd = ser.read(chunk_size - len(echo))
            if rd:
                echo += rd
        expected = echo_data[offset:offset + chunk_size]
        assert echo == expected, (f'CDC echo mismatch at offset {offset} ({chunk_size} bytes):\n'
                                  f'  expected: {expected}\n  received: {echo}')
        offset += chunk_size

    ser.close()


def test_host_msc_file_explorer(board):
    flasher = board['flasher']
    msc_devs = [d for d in board['tests'].get('dev_attached', []) if d.get('is_msc')]
    if not msc_devs:
        return 'skipped'

    port = get_serial_dev(flasher["uid"], None, None, 0)
    ser = open_serial_dev(port)
    ser.timeout = 0.1

    # reset device to catch mount messages
    ret = globals()[f'reset_{flasher["name"].lower()}'](board)
    assert ret.returncode == 0, 'Failed to reset device'

    # Wait for MSC mount (Disk Size message)
    data = b''
    timeout = ENUM_TIMEOUT
    while timeout > 0:
        new_data = ser.read(ser.in_waiting or 1)
        if new_data:
            data += new_data
            if b'Disk Size' in data:
                break
        time.sleep(0.1)
        timeout -= 0.1
    assert b'Disk Size' in data, 'MSC device not mounted'
    lines = data.decode('utf-8', errors='ignore').splitlines()
    check_msc_info(lines, msc_devs)

    # Send "cat README.TXT" and check response (optional — file may not exist on all drives)
    time.sleep(1)
    ser.reset_input_buffer()
    for ch in 'cat README.TXT\r':
        ser.write(ch.encode())
        ser.flush()
        time.sleep(0.002)

    resp = b''
    t = 10.0
    while t > 0:
        rd = ser.read(max(1, ser.in_waiting))
        if rd:
            resp += rd
        if b'>' in resp and resp.rstrip().endswith(b'>'):
            break
        time.sleep(0.05)
        t -= 0.05

    resp_text = resp.decode('utf-8', errors='ignore')
    if MSC_README_TXT.decode() in resp_text:
        print('README.TXT matched ', end='')

    # MSC throughput test: send dd command to read sectors
    time.sleep(0.5)
    ser.reset_input_buffer()
    for ch in 'dd 1024\r':
        ser.write(ch.encode())
        ser.flush()
        time.sleep(0.002)

    # Read dd output until prompt
    resp = b''
    t = 30.0
    while t > 0:
        rd = ser.read(max(1, ser.in_waiting))
        if rd:
            resp += rd
        if b'KB/s' in resp and b'>' in resp:
            break
        time.sleep(0.05)
        t -= 0.05

    resp_text = resp.decode('utf-8', errors='ignore')
    for line in resp_text.splitlines():
        if 'KB/s' in line:
            print(f'{line.strip()} ', end='')
            break

    ser.close()


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

    def rand_ascii(length):
        return "".join(random.choices(string.ascii_letters + string.digits, k=length)).encode("ascii")

    sizes = [32, 64, 128, 256, 512, random.randint(2000, 5000)]

    def write_and_check(writer, payload : bytes):
        payload_len = len(payload)
        for s in ser:
            s.reset_input_buffer()
        rd0 = b''
        rd1 = b''
        offset = 0
        # Write in chunks of random 1-64 bytes (device has 64-byte buffer)
        while offset < payload_len:
            chunk_size = min(random.randint(1, 64), payload_len - offset)
            ser[writer].write(payload[offset:offset + chunk_size])
            ser[writer].flush()
            rd0 += ser[0].read(chunk_size)
            rd1 += ser[1].read(chunk_size)
            offset += chunk_size
        assert rd0 == payload.lower(), f'Port0 wrong data ({payload_len}): expected {payload.lower()}... was {rd0}'
        assert rd1 == payload.upper(), f'Port1 wrong data ({payload_len}): expected {payload.upper()}... was {rd1}'

    for size in sizes:
        payload0 = rand_ascii(size)
        write_and_check(0, payload0)

        payload1 = rand_ascii(size)
        write_and_check(1, payload1)
    ser[0].close()
    ser[1].close()


def test_device_cdc_msc(board):
    uid = board['uid']
    # CDC Echo test
    port = get_serial_dev(uid, 'TinyUSB', "TinyUSB_Device", 0)
    ser = open_serial_dev(port)

    def rand_ascii(length):
        return "".join(random.choices(string.ascii_letters + string.digits, k=length)).encode("ascii")

    sizes = [32, 64, 128, 256, 512, random.randint(2000, 5000)]
    for size in sizes:
        test_str = rand_ascii(size)
        rd_str = b''
        offset = 0
        # Write in chunks of random 1-64 bytes (device has 64-byte buffer)
        while offset < size:
            chunk_size = min(random.randint(1, 64), size - offset)
            ser.write(test_str[offset:offset + chunk_size])
            ser.flush()
            rd_str += ser.read(chunk_size)
            offset += chunk_size
        assert rd_str == test_str, f'CDC wrong data ({size} bytes):\n  expected: {test_str}\n  received: {rd_str}'
    ser.close()

    # MSC Block test
    data = read_disk_file(uid, 0, 'README.TXT')
    assert data == MSC_README_TXT, f'MSC wrong data in README.TXT\n expected: {MSC_README_TXT.decode()}\n received: {data.decode()}'


def test_device_cdc_msc_freertos(board):
    test_device_cdc_msc(board)


def test_device_cdc_msc_throughput(board):
    uid = board['uid']

    def parse_speed(dd_output):
        for line in dd_output.splitlines():
            m = re.search(r'([\d.]+)\s+([kMG]?B)/s', line)
            if m:
                return f'{float(m.group(1)):.1f} {m.group(2)}ps'
        return '?'

    # Wait for MSC disk enumeration
    dev = get_disk_dev(uid, 'TinyUSB', 0)
    timeout = ENUM_TIMEOUT
    while timeout > 0:
        if os.path.exists(dev):
            break
        time.sleep(0.1); timeout -= 0.1
    assert timeout > 0, f'Disk {dev} not found'

    # Wait for CDC tty enumeration
    tty = get_serial_dev(uid, 'TinyUSB', 'Throughput', 0)
    timeout = ENUM_TIMEOUT
    while timeout > 0:
        if os.path.exists(tty):
            break
        time.sleep(0.1); timeout -= 0.1
    assert timeout > 0, f'CDC tty {tty} not found'

    # Detect speed (12 Mbps FS / 480 Mbps HS) for payload scaling
    is_fs = False
    for f in glob.glob('/sys/bus/usb/devices/*/serial'):
        try:
            if open(f).read().strip() == uid:
                is_fs = (open(os.path.join(os.path.dirname(f), 'speed')).read().strip() == '12')
                break
        except (OSError, ValueError):
            pass

    # Put tty in raw mode so dd sees pure binary throughput.
    rs = run_cmd(f'timeout 30 stty -F {tty} raw -echo')
    assert rs.returncode == 0, f'stty failed: {cmd_stdout_text(rs.stdout)}'

    # Payload aim: ~5 s per direction at FS (~830 kB/s), much less at HS.
    msc_count = 2 if is_fs else 16    # bs=1M
    cdc_count = 16 if is_fs else 128  # bs=64K

    tmp_file = f'/tmp/cdc_msc_tp_{uid}.bin'

    rw = run_cmd(f'timeout 30 dd if=/dev/zero of={tty} bs=64K count={cdc_count} 2>&1')
    assert rw.returncode == 0, f'CDC dd write failed: {cmd_stdout_text(rw.stdout)}'
    cdc_w = parse_speed(cmd_stdout_text(rw.stdout))

    rr = run_cmd(f'timeout 30 dd if={tty} of=/dev/null bs=64K count={cdc_count} iflag=fullblock 2>&1')
    assert rr.returncode == 0, f'CDC dd read failed: {cmd_stdout_text(rr.stdout)}'
    cdc_r = parse_speed(cmd_stdout_text(rr.stdout))

    rmr = run_cmd(f'dd if={dev} of={tmp_file} bs=1M count={msc_count} iflag=direct 2>&1')
    assert rmr.returncode == 0, f'MSC dd read failed: {cmd_stdout_text(rmr.stdout)}'
    msc_r = parse_speed(cmd_stdout_text(rmr.stdout))

    rmw = run_cmd(f'dd if={tmp_file} of={dev} bs=1M count={msc_count} oflag=direct 2>&1')
    assert rmw.returncode == 0, f'MSC dd write failed: {cmd_stdout_text(rmw.stdout)}'
    msc_w = parse_speed(cmd_stdout_text(rmw.stdout))

    try:
        os.remove(tmp_file)
    except OSError:
        pass

    print(f'  CDC read {cdc_r} write {cdc_w}, MSC read {msc_r} write {msc_w}  ', end='')


def test_device_dfu(board):
    uid = board['uid']

    # Wait device enum
    timeout = ENUM_TIMEOUT
    while timeout > 0:
        ret = run_cmd(f'dfu-util -l')
        stdout = cmd_stdout_text(ret.stdout)
        if f'serial="{uid}"' in stdout and 'Found DFU: [cafe:4000]' in stdout:
            break
        time.sleep(1)
        timeout = timeout - 1

    assert timeout > 0, 'Device not available'

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
    while timeout > 0:
        ret = run_cmd(f'dfu-util -l')
        stdout = cmd_stdout_text(ret.stdout)
        if f'serial="{uid}"' in stdout and 'Found Runtime: [cafe:4000]' in stdout:
            break
        time.sleep(1)
        timeout = timeout - 1

    assert timeout > 0, 'Device not available'


def test_device_hid_boot_interface(board):
    uid = board['uid']
    kbd = get_hid_dev(uid, 'TinyUSB', 'TinyUSB_Device', 'event-kbd')
    mouse1 = get_hid_dev(uid, 'TinyUSB', 'TinyUSB_Device', 'if01-event-mouse')
    mouse2 = get_hid_dev(uid, 'TinyUSB', 'TinyUSB_Device', 'if01-mouse')
    # Wait device enum
    timeout = ENUM_TIMEOUT
    while timeout > 0:
        if os.path.exists(kbd) and os.path.exists(mouse1) and os.path.exists(mouse2):
            break
        time.sleep(1)
        timeout = timeout - 1

    assert timeout > 0, 'HID device not available'


def test_device_hid_composite_freertos(id):
    # TODO implement later
    pass


def test_device_printer_to_cdc(board):
    import threading

    uid = board['uid']

    # Wait for CDC port and printer device
    cdc_port = get_serial_dev(uid, 'TinyUSB', "TinyUSB_Device", 0)
    ser = open_serial_dev(cdc_port)
    lp_dev = open_printer_dev(uid, 'TinyUSB', 'TinyUSB_Device', 2)

    # Test 0: Verify IEEE 1284 Device ID from sysfs
    expected_id = 'MFG:TinyUSB;MDL:Printer to CDC;CMD:PS;CLS:PRINTER;'
    lp_name = os.path.basename(lp_dev)
    sysfs_id_path = f'/sys/class/usbmisc/{lp_name}/device/ieee1284_id'
    if os.path.exists(sysfs_id_path):
        with open(sysfs_id_path) as f:
            ieee1284_id = f.read().strip()
        if ieee1284_id:
            assert ieee1284_id == expected_id, (f'IEEE 1284 ID mismatch:\n'
                                                f'  expected: {expected_id}\n  got: {ieee1284_id}')

    def rand_ascii(length):
        return "".join(random.choices(string.ascii_letters + string.digits, k=length)).encode("ascii")

    sizes = [32, 64, 128, 256, 512, random.randint(2000, 5000)]

    # flush any stale data
    ser.reset_input_buffer()

    # Test 1: Printer -> CDC with multiple sizes, write in random 1-64 byte chunks
    LP_WRITE_TIMEOUT = 5.0  # seconds; firmware may stall draining the printer OUT endpoint
    for size in sizes:
        test_data = rand_ascii(size)
        ser.reset_input_buffer()
        rd = b''
        offset = 0
        lp_fd = os.open(lp_dev, os.O_WRONLY | os.O_NONBLOCK)
        try:
            while offset < size:
                chunk_size = min(random.randint(1, 64), size - offset)
                buf = test_data[offset:offset + chunk_size]
                written = 0
                while written < len(buf):
                    _, wr, _ = select.select([], [lp_fd], [], LP_WRITE_TIMEOUT)
                    assert wr, f'Printer write timeout after {LP_WRITE_TIMEOUT}s (firmware not draining OUT endpoint)'
                    n = os.write(lp_fd, buf[written:])
                    written += n
                rd += ser.read(chunk_size)
                offset += chunk_size
        finally:
            os.close(lp_fd)
        # read any remaining bytes (fullspeed devices may need extra time)
        while len(rd) < size:
            remaining = ser.read(size - len(rd))
            if not remaining:
                break
            rd += remaining
        assert rd == test_data, (f'Printer->CDC wrong data ({size} bytes):\n'
                                 f'  expected: {test_data[:64]}\n  received: {rd[:64]}')

    # Test 2: CDC -> Printer with multiple sizes, write in random 1-64 byte chunks
    # Use a thread to read from printer since /dev/usb/lp read blocks
    ser.reset_input_buffer()
    time.sleep(0.5)
    for size in sizes:
        test_data = rand_ascii(size)
        rd_result = [b'', None]  # [data, error]
        reader_ready = threading.Event()

        def lp_reader():
            try:
                rd = b''
                fd = os.open(lp_dev, os.O_RDONLY)
                reader_ready.set()
                try:
                    while len(rd) < size:
                        chunk = os.read(fd, min(64, size - len(rd)))
                        if not chunk:
                            break
                        rd += chunk
                finally:
                    os.close(fd)
                rd_result[0] = rd
            except Exception as e:
                rd_result[1] = e
                reader_ready.set()

        reader = threading.Thread(target=lp_reader, daemon=True)
        reader.start()
        # wait for reader to open lp device before writing
        reader_ready.wait(timeout=5)
        time.sleep(0.1)

        # Write to CDC in small chunks with flush to avoid overflowing device FIFO
        offset = 0
        while offset < size:
            chunk_size = min(random.randint(1, 64), size - offset)
            ser.write(test_data[offset:offset + chunk_size])
            ser.flush()
            time.sleep(0.01)
            offset += chunk_size

        reader.join(timeout=10)
        assert not reader.is_alive(), f'CDC->Printer timeout ({size} bytes)'
        assert rd_result[1] is None, f'CDC->Printer read error: {rd_result[1]}'
        assert rd_result[0] == test_data, (f'CDC->Printer wrong data ({size} bytes):\n'
                                           f'  expected: {test_data[:64]}\n  received: {rd_result[0][:64]}')
        time.sleep(0.2)

    ser.close()


def test_device_mtp(board):
    uid = board['uid']

    # --- BEFORE: mute C-level stderr for libmtp vid/pid warnings ---
    fd = sys.stderr.fileno()
    _saved = os.dup(fd)
    _null = os.open(os.devnull, os.O_WRONLY)
    os.dup2(_null, fd)

    mtp = open_mtp_dev(uid)

    # --- AFTER: restore stderr ---
    os.dup2(_saved, fd)
    os.close(_null)
    os.close(_saved)

    if mtp is None or mtp.device is None:
        assert False, 'MTP device not found'

    try:
        assert b"TinyUSB" == mtp.get_manufacturer(), 'MTP wrong manufacturer'
        assert b"MTP Example" == mtp.get_modelname(), 'MTP wrong model'
        assert b'1.0' == mtp.get_deviceversion(), 'MTP wrong version'
        assert b'TinyUSB MTP' == mtp.get_devicename(), 'MTP wrong device name'

        # read and compare readme.txt and logo.png
        f1_expect = b'TinyUSB MTP Filesystem example'
        f2_md5_expect = '40ef23fc2891018d41a05d4a0d5f822f' # md5sum of logo.png
        f1 = uid.encode("utf-8") + b'_file1'
        f2 = uid.encode("utf-8") + b'_file2'
        f3 = uid.encode("utf-8") + b'_file3'
        mtp.get_file_to_file(1, f1)
        with open(f1, 'rb') as file:
            f1_data = file.read()
            os.remove(f1)
            assert f1_data == f1_expect, 'MTP file1 wrong data'
        mtp.get_file_to_file(2, f2)
        with open(f2, 'rb') as file:
            f2_data = file.read()
            os.remove(f2)
            assert f2_md5_expect == hashlib.md5(f2_data).hexdigest(), 'MTP file2 wrong data'
        # test send file
        with open(f3, "wb") as file:
            f3_data = os.urandom(random.randint(1024, 3*1024))
            file.write(f3_data)
            file.close()
            fid = mtp.send_file_from_file(f3, b'file3')
            f3_readback = f3 + b'_readback'
            mtp.get_file_to_file(fid, f3_readback)
            with open(f3_readback, 'rb') as f:
                f3_rb_data = f.read()
                os.remove(f3_readback)
                assert f3_rb_data == f3_data, 'MTP file3 wrong data'
            os.remove(f3)
            mtp.delete_object(fid)
    finally:
        mtp.disconnect()


def test_device_net_lwip_webserver(board):
    # MAC hard-coded in examples/device/net_lwip_webserver/src/main.c; Linux names the
    # USB network interface enx<MAC_lowercase_no_colons>. Device IP is 192.168.7.1 and
    # the example runs an iperf2 TCP server on port 5001 (INCLUDE_IPERF).
    import socket
    mac_no_colons = '0202846a9600'
    iface = 'enx' + mac_no_colons
    device_ip = '192.168.7.1'
    iperf_port = 5001

    # Wait for the host to get an IPv4 address in the device's subnet (DHCP served by the device).
    # USB enum + DHCP serve can take longer on the CI HIL hardware than on local — give it 30s.
    iface_timeout = 30
    deadline = time.time() + iface_timeout
    host_ip = None
    while time.time() < deadline:
        ret = subprocess.run(['ip', '-o', '-4', 'addr', 'show', iface],
                             capture_output=True, text=True, timeout=2)
        m = re.search(r'inet (192\.168\.7\.\d+)/', ret.stdout) if ret.returncode == 0 else None
        if m:
            host_ip = m.group(1)
            break
        time.sleep(0.5)
    assert host_ip, f'USB net iface {iface} did not come up with 192.168.7.x within {iface_timeout}s'

    # Poll the iperf TCP port until the device is accepting. The net stack comes up a bit
    # after DHCP completes; iperf server binding isn't instantaneous after reflash.
    deadline = time.time() + ENUM_TIMEOUT
    last_err = None
    while time.time() < deadline:
        try:
            with socket.create_connection((device_ip, iperf_port), timeout=1):
                last_err = None
                break
        except OSError as e:
            last_err = e
            time.sleep(0.3)
    assert last_err is None, f'iperf TCP {device_ip}:{iperf_port} not accepting within {ENUM_TIMEOUT}s: {last_err}'

    # Throughput: 5-second iperf2 TCP test, CSV output for stable parsing.
    # iperf2 CSV final summary line: timestamp,src_ip,src_port,dst_ip,dst_port,id,interval,bytes,bps
    ret = subprocess.run(['iperf', '-c', device_ip, '-t', '5', '-y', 'C'],
                         capture_output=True, text=True, timeout=30)
    stderr = ret.stderr.strip()
    stdout = ret.stdout.strip()
    assert ret.returncode == 0, f'iperf rc={ret.returncode}: stderr={stderr!r} stdout={stdout!r}'
    lines = [l for l in stdout.splitlines() if l]
    assert lines, f'iperf produced no output (rc={ret.returncode}, stderr={stderr!r})'
    try:
        bps = int(lines[-1].split(',')[-1])
    except (ValueError, IndexError) as e:
        raise AssertionError(f'could not parse iperf output: {lines[-1]!r} ({e})')
    mbps = bps / 1e6
    print(f'  iperf {mbps:5.1f} Mbps', end='')

    # Reject implausibly low throughput - a working USB-net link should clear this easily.
    assert mbps >= 1.0, f'iperf throughput too low: {mbps:.2f} Mbps'


def test_device_msc_dual_lun(board):
    uid = board['uid']

    # Read README from LUN 0
    data0 = read_disk_file(uid, 0, 'README0.TXT')
    readme0 = b"LUN0: " + MSC_README_TXT
    assert data0 == readme0, f'MSC LUN0 wrong data in README0.TXT\n  expected: {readme0}\n  received: {data0}'

    # Read README from LUN 1
    data1 = read_disk_file(uid, 1, 'README1.TXT')
    readme1 = b"LUN1: " + MSC_README_TXT
    assert data1 == readme1, f'MSC LUN1 wrong data in README1.TXT\n  expected: {readme1}\n  received: {data1}'


def test_device_midi_test(board):
    uid = board['uid']

    # Find MIDI device via /dev/snd/by-id using board UID
    timeout = ENUM_TIMEOUT
    midi_port = None
    while timeout > 0:
        pattern = f'/dev/snd/by-id/usb-*_{uid}-*'
        devs = glob.glob(pattern)
        if devs:
            # by-id entry points to controlCX, derive card number for midiCXD0
            link = os.path.basename(os.readlink(devs[0]))  # e.g. "controlC2"
            card_num = link.replace('controlC', '')
            midi_path = f'/dev/snd/midiC{card_num}D0'
            if os.path.exists(midi_path):
                midi_port = midi_path
                break
        time.sleep(1)
        timeout -= 1
    assert midi_port is not None, f'MIDI device not found for {uid}'

    # Read MIDI messages and verify note on/off
    import select
    with open(midi_port, 'rb') as f:
        notes = []
        # Read for up to 3 seconds to capture a few notes (286ms interval)
        end_time = time.time() + 3
        while time.time() < end_time:
            ready, _, _ = select.select([f], [], [], 0.5)
            if ready:
                data = f.read(64)
                if data:
                    # Parse MIDI bytes: note_on = 0x90, note_off = 0x80
                    i = 0
                    while i + 2 < len(data):
                        status = data[i]
                        if (status & 0xF0) == 0x90:  # Note On
                            notes.append(data[i + 1])
                            i += 3
                        elif (status & 0xF0) == 0x80:  # Note Off
                            i += 3
                        else:
                            i += 1

    assert len(notes) >= 2, f'Expected at least 2 MIDI notes, got {len(notes)}'
    # Verify notes are from the expected sequence
    note_sequence = [
        74, 78, 81, 86, 90, 93, 98, 102, 57, 61, 66, 69, 73, 78, 81, 85,
        88, 92, 97, 100, 97, 92, 88, 85, 81, 78, 74, 69, 66, 62, 57, 62,
        66, 69, 74, 78, 81, 86, 90, 93, 97, 102, 97, 93, 90, 85, 81, 78,
        73, 68, 64, 61, 56, 61, 64, 68, 74, 78, 81, 86, 90, 93, 98, 102
    ]
    for n in notes:
        assert n in note_sequence, f'Unexpected MIDI note {n}'


def test_device_hid_generic_inout(board):
    uid = board['uid']
    import hid

    # Find HID device by UID (VID=0xCafe)
    timeout = ENUM_TIMEOUT
    dev = None
    while timeout > 0:
        for d in hid.enumerate(0xCafe):
            if d['serial_number'] == uid:
                dev = d
                break
        if dev:
            break
        time.sleep(1)
        timeout -= 1
    assert dev is not None, f'HID device not found for {uid}'

    h = hid.Device(vid=dev['vendor_id'], pid=dev['product_id'], serial=uid)

    # Echo test: send random data and verify echo
    for size in [8, 32, 63]:
        # Report ID (0) + payload, padded to 64 bytes
        payload = bytes([random.randint(1, 255) for _ in range(size)])
        report = bytes([0]) + payload + bytes(64 - size)
        h.write(report)
        echo = h.read(64, timeout=2000)
        assert echo is not None and len(echo) >= size, (
            f'HID echo timeout or short read ({size} bytes)')
        assert bytes(echo[:size]) == payload, (
            f'HID echo wrong data ({size} bytes):\n'
            f'  expected: {payload.hex()}\n  received: {bytes(echo[:size]).hex()}')

    h.close()


# -------------------------------------------------------------
# Main
# -------------------------------------------------------------
# device tests
# note don't test 2 examples with cdc or 2 msc next to each other
device_tests = [
    'device/cdc_dual_ports',
    'device/dfu',
    'device/cdc_msc',
    'device/cdc_msc_throughput',
    'device/dfu_runtime',
    'device/cdc_msc_freertos',
    'device/hid_boot_interface',
    'device/msc_dual_lun',
    'device/hid_generic_inout',
    'device/printer_to_cdc',
    'device/midi_test',
    'device/mtp',
    # 'device/net_lwip_webserver',  # disabled for PR #3605: USB net iface enum is flaky on the CI HIL host
]

dual_tests = [
    'dual/host_info_to_device_cdc',
]

host_test = [
    'host/cdc_msc_hid',
    'host/msc_file_explorer',
    'host/device_info',
]


def test_example(board: Board, f1: str, example: str) -> int:
    """
    Test example firmware
    :param board: board dict
    :param f1: flags on
    :param example: example name
    :return: 0 if success/skip, 1 if failed
    """
    name = board['name']
    err_count = 0

    f1_str = ""
    if f1 != "":
        f1_str = '-f1_' + f1.replace(' ', '_')

    fw_dir = TINYUSB_ROOT / build_dir / f'cmake-build-{name}{f1_str}' / example
    fw_name = fw_dir / Path(example).name
    print(f'{name+f1_str:40} {example:30} ...', end='')

    if not fw_dir.exists() or not ((fw_name.with_suffix('.elf')).exists() or (fw_name.with_suffix('.bin')).exists()):
        print('Skip (no binary)')
        return 0

    if verbose:
        print(f'Flashing {fw_name}.elf')

    # flash firmware (unless --skip-flash), then run the test. Both may fail randomly,
    # retry a few times.
    start_s = time.time()
    flash_ok = True
    for i in range(max_retry):
        if not skip_flash:
            ret = globals()[f'flash_{board["flasher"]["name"].lower()}'](board, str(fw_name))
            flash_ok = (ret.returncode == 0)
        if flash_ok:
            try:
                tret = globals()[f'test_{example.replace("/", "_")}'](board)
                if tret == 'skipped':
                    print(f'  {STATUS_SKIPPED}', end='')
                else:
                    print('  OK', end='')
                break
            except Exception as e:
                if i == max_retry - 1:
                    err_count += 1
                    print(f'{STATUS_FAILED}: {e}')
                else:
                    print(f'\n  Test failed: {e}, retry {i+2}/{max_retry}', end='')
                    time.sleep(0.5)
        else:
            print(f'\n  Flash failed, retry {i+2}/{max_retry}', end='')
            time.sleep(0.5)

    if not flash_ok:
        err_count += 1
        print(f'  Flash {STATUS_FAILED}', end='')

    print(f'  in {time.time() - start_s:.1f}s')

    return err_count


def build_board(board: Board) -> tuple[str, int]:
    """Build firmware for this board via tools/build.py.
    Honors board config's build.flags_on variants and build.args defines.
    Output goes to cmake-build/cmake-build-BOARD[-f1_...]/ (tools/build.py layout)."""
    name = board['name']
    bcfg = cast(BuildCfg, board.get('build', {}))
    flags_on_list = bcfg.get('flags_on', [''])
    extra_defs = bcfg.get('args', [])

    failed = 0
    for f1 in flags_on_list:
        cmd = [sys.executable, str(TINYUSB_ROOT / 'tools' / 'build.py'), '-b', name]
        for d in extra_defs:
            cmd += ['-D', d]
        if f1:
            for flag in f1.split():
                cmd += ['-f1', flag]
        if verbose:
            cmd.append('-v')
            print(f'  + {" ".join(cmd)}')
        r = subprocess.run(cmd, cwd=TINYUSB_ROOT)
        if r.returncode != 0:
            failed += 1
    return name, failed


def test_board(board: Board) -> tuple[str, int, list[str]]:
    name = board['name']
    flasher = board['flasher']

    # default to all tests
    test_list = []

    if name in board_test:
        test_list = board_test[name]
    elif len(test_only) > 0:
        test_list = test_only
    else:
        if 'tests' in board:
            board_tests = board['tests']
            if board_tests.get('device') is True:
                test_list += list(device_tests)
            if board_tests.get('dual') is True:
                test_list += dual_tests
            if board_tests.get('host') is True:
                test_list += host_test
            if 'only' in board_tests:
                test_list = board_tests['only']
            if 'skip' in board_tests:
                for skip in board_tests['skip']:
                    if skip in test_list:
                        test_list.remove(skip)
                        print(f'{name:25} {skip:30} ... Skip')

    err_count = 0
    failed_tests = []
    flags_on_list = [""]
    if 'build' in board and 'flags_on' in board['build']:
        flags_on_list = board['build']['flags_on']

    for f1 in flags_on_list:
        for test in test_list:
            ec = test_example(board, f1, test)
            err_count += ec
            if ec > 0:
                failed_tests.append(test)

    # flash board_test last to disable board's usb (skipped when --skip-flash is set)
    if not skip_flash:
        test_example(board, flags_on_list[0], 'device/board_test')

    return name, err_count, sorted(set(failed_tests))


def main() -> None:
    """
    Hardware test on specified boards
    """
    global verbose
    global test_only
    global board_test
    global build_dir
    global max_retry
    global skip_flash

    duration = time.time()

    parser = argparse.ArgumentParser()
    parser.add_argument('config_file', help='Configuration JSON file')
    parser.add_argument('-b', '--board', action='append', default=[], help='Boards to test, all if not specified')
    parser.add_argument('-s', '--skip-board', action='append', default=[], help='Skip boards from test')
    parser.add_argument('-sf', '--skip-flash', action='store_true', help='Run tests without flashing firmware (use whatever is already on the board)')
    parser.add_argument('-t', '--test-only', action='append', default=[], help='Tests to run, all if not specified')
    parser.add_argument('-bt', '--board-test', action='append', default=[],
                        help='Per-board test list as BOARD:test1,test2 (overrides -t for that board); repeat for multiple boards')
    parser.add_argument('-B', '--build-dir', default='cmake-build', help='Build folder name (default: cmake-build)')
    parser.add_argument('--build', action='store_true', help='Build firmware for selected boards with cmake before running tests')
    parser.add_argument('-r', '--retry', type=int, default=3, help='Retry count for failed tests (default: 3)')
    parser.add_argument('-v', '--verbose', action='store_true', help='Verbose output')
    args = parser.parse_args()

    config_file = Path(args.config_file)
    boards = args.board
    skip_boards = args.skip_board
    verbose = args.verbose
    test_only = args.test_only
    for entry in args.board_test:
        bname, _, tnames = entry.partition(':')
        if not bname or not tnames:
            parser.error(f'invalid --board-test value: {entry!r} (expected BOARD:test1,test2)')
        board_test[bname] = [t for t in tnames.split(',') if t]
    build_dir = args.build_dir
    max_retry = args.retry
    skip_flash = args.skip_flash

    # if config file is not found, try to find it in the same directory as this script
    if not config_file.exists():
        config_file = Path(__file__).resolve().parent / config_file
    with config_file.open() as f:
        config = cast(HilConfig, json.load(f))

    if len(boards) == 0:
        config_boards = [e for e in config['boards'] if e['name'] not in skip_boards]
    else:
        config_boards = [e for e in config['boards'] if e['name'] in boards]

    build_err = 0
    if args.build:
        if build_dir != 'cmake-build':
            print(f'warning: --build writes into cmake-build/, but -B is {build_dir!r}; '
                  f'tests will not find the freshly built firmware')
        print('-' * 30)
        print(f'Build phase: {len(config_boards)} board(s)')
        print('-' * 30)
        for board in config_boards:
            _, nfail = build_board(board)
            build_err += nfail
        print('-' * 30)
        print(f'Build phase done: {build_err} failed')
        print('-' * 30)

    with Pool(processes=os.cpu_count() or 1) as pool:
        async_ret = pool.map_async(test_board, config_boards)
        try:
            mret = async_ret.get(timeout=POOL_TIMEOUT)
        except MpTimeoutError:
            pool.terminate()
            pool.join()
            raise RuntimeError(f'HIL worker pool timed out after {POOL_TIMEOUT}s')
        err_count = build_err + sum(e[1] for e in mret)
        # generate skip list for next re-run if failed: skip boards that fully passed,
        # and emit -bt BOARD:t1,t2 so each failed board only re-runs its own failed tests.
        skip_fname = config_file.with_suffix(config_file.suffix + '.skip')
        if err_count > 0:
            skip_boards += [name for name, err, _ in mret if err == 0]
            parts = [f'--skip-board {i}' for i in skip_boards]
            parts += [f'-bt {name}:{",".join(fts)}' for name, err, fts in mret if err > 0 and fts]
            with skip_fname.open('w') as f:
                f.write(' '.join(parts))
        elif skip_fname.exists():
            skip_fname.unlink()

    duration = time.time() - duration
    print()
    print("-" * 30)
    print(f'Total failed: {err_count} in {duration:.1f}s')
    print("-" * 30)
    sys.exit(err_count)


if __name__ == '__main__':
    main()
