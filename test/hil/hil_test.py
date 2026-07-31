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

# Host setup (required: a missing tool fails its test rather than skipping it):
#   - System packages: sudo apt install mtools libmtp9 alsa-utils iperf
#       mtools      read_disk_file (device/cdc_msc, device/msc_dual_lun)
#       libmtp9     pymtp ctypes load (device/mtp); Debian 13 uses libmtp9t64
#       alsa-utils  arecord (device/audio_test_freertos)
#       iperf       throughput tests (device/net_lwip_*)
#       openocd     unified openocd from https://github.com/hathach/openocd (branch tinyusb) for wch, rp2040/rp2350, analog max32
#   - device/usbtest: usbtest kernel module + testusb binary (kernel tools/usb/testusb.c) on PATH,
#     plus sudo for modprobe / sysfs writes
#   - Python packages: pip install -r requirements.txt
#
# udev rules :
# ACTION=="add", SUBSYSTEM=="tty", SUBSYSTEMS=="usb", MODE="0666", PROGRAM="/bin/sh -c 'echo $$ID_SERIAL_SHORT | rev | cut -c -8 | rev'", SYMLINK+="ttyUSB_%c.%s{bInterfaceNumber}"
# ACTION=="add", SUBSYSTEM=="block", SUBSYSTEMS=="usb", ENV{ID_FS_USAGE}=="filesystem", MODE="0666", PROGRAM="/bin/sh -c 'echo $$ID_SERIAL_SHORT | rev | cut -c -8 | rev'", RUN{program}+="/usr/bin/systemd-mount --no-block --automount=yes --collect $devnode /media/blkUSB_%c.%s{bInterfaceNumber}"

import argparse
import io
import itertools
import os
import random
import re
import select
import sys
import time
from contextlib import redirect_stdout
from pathlib import Path
from typing import TypedDict, NotRequired, cast

import serial
import subprocess
import json
import glob
import multiprocessing
from multiprocessing import TimeoutError as MpTimeoutError

import hil_flash
import hil_lock
from hil_examples import device_tests, dual_tests, host_test

# Raw Lock/Semaphore objects passed via Pool initargs are inheritable only under the fork
# start method (spawn/forkserver pickle them and fail at Pool creation) — pin it so a
# future interpreter default change cannot break the run at startup.
_mp = multiprocessing.get_context('fork')
Pool, Lock, Semaphore, Manager = _mp.Pool, _mp.Lock, _mp.Semaphore, _mp.Manager
import hashlib
import ctypes
from pymtp import MTP
import string

# Enumeration wait budget. The first attempt gets ENUM_TIMEOUT; retry attempts get the
# shorter ENUM_TIMEOUT_RETRY - the board was just re-flashed again, and a device that is
# going to enumerate shows up within a few seconds, so a failing test costs ~3-5x a
# passing one instead of 10-30x. Per-attempt value is set by test_example(); each pool
# worker is its own process, so a module global is safe.
ENUM_TIMEOUT = 8
ENUM_TIMEOUT_RETRY = 4
_enum_timeout = ENUM_TIMEOUT


def enum_timeout() -> int:
    """Enumeration wait budget for the current test attempt."""
    return _enum_timeout


def wait_until(predicate, step: float = 1.0):
    """Poll predicate under the per-attempt enum budget. Deadline-based so a slow predicate
    body (subprocess, libmtp scan) counts against the budget. Returns the first truthy
    predicate value, or None on timeout."""
    deadline = time.monotonic() + enum_timeout()
    while True:
        r = predicate()
        if r:
            return r
        if time.monotonic() >= deadline:
            return None
        time.sleep(step)

STATUS_OK = "\033[32mOK\033[0m"
STATUS_FAILED = "\033[31mFailed\033[0m"
STATUS_SKIPPED = "\033[33mSkipped\033[0m"

# Plain (non-ANSI) cell symbols for the markdown matrix report (hil_report.md).
# A missing binary is reported as skipped too.
REPORT_CELL = {'pass': '✅', 'fail': '❌', 'skip': '⚪'}


class TestFail(AssertionError):
    """Fail a test but still surface a metric string in its report cell (e.g. usbtest's '❌ 29/30'
    instead of a bare ❌). The cell metric is icon-prefixed so render/tally treat it as a failure."""
    def __init__(self, msg: str, metric: str | None = None):
        super().__init__(msg)
        self.metric = metric


verbose = False
PROFILE = os.environ.get('HIL_PROFILE') == '1'  # timestamped logs + permit/flash timing + ctrl-map dump
test_only = []
board_test = {}
skip_flash = False
print_lock = None
shuffle_seed = None  # per-run seed for the per-board test-order shuffle (HIL_SHUFFLE_SEED to replay)


def init_worker(lock, seed, b_mutexes, f_sems, cmap, cmeta, hints_by_uid):
    global print_lock, shuffle_seed
    print_lock = lock
    shuffle_seed = seed
    hil_lock.init_scheduling(b_mutexes, f_sems, cmap, cmeta, hints_by_uid, log_fn=log_line)


def log_line(msg: str) -> None:
    if PROFILE:
        msg = f'{time.time():.3f} {msg}'
    out = sys.__stdout__ if sys.__stdout__ is not None else sys.stdout
    if print_lock is not None:
        with print_lock:
            print(msg, file=out, flush=True)
    else:
        print(msg, file=out, flush=True)


def compact_output(raw: str) -> str:
    if not raw:
        return ''
    lines = [ln.strip() for ln in raw.replace('\r', '\n').split('\n') if ln.strip()]
    return ' | '.join(lines)

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
    args: list[str]


class VariantCfg(TypedDict, total=False):
    name: str           # build dir (cmake-build-<name>) and HIL report row
    flags: str          # raw CFLAGS, e.g. "-DCFG_TUD_DWC2_DMA_ENABLE=1"
    defines: list[str]  # cmake -D defines, e.g. ["RHPORT_DEVICE=1"] (vs flags which are compiler-only)


class Board(TypedDict):
    name: str
    uid: str
    tests: TestsCfg
    flasher: FlasherCfg
    build: NotRequired[BuildCfg]
    variant: NotRequired[list[VariantCfg]]
    toolchain: NotRequired[str]  # CI build bucket override, e.g. "riscv-gcc" (consumed by hil_ci_set_matrix.py)


class HilConfig(TypedDict):
    boards: list[Board]

POOL_TIMEOUT = int(os.getenv('HIL_POOL_TIMEOUT', '4200'))  # usbtest batteries are serialized fleet-wide, lengthening the tail
SERIAL_READ_TIMEOUT = float(os.getenv('HIL_SERIAL_READ_TIMEOUT', '5'))
SERIAL_WRITE_TIMEOUT = float(os.getenv('HIL_SERIAL_WRITE_TIMEOUT', '10'))


MSC_README_TXT = \
b"This is tinyusb's MassStorage Class demo.\r\n\r\n\
If you find any bugs or get any questions, feel free to file an\r\n\
issue at github.com/hathach/tinyusb"

# get usb disk by id
def get_disk_dev(id, vendor_str, lun):
    return f'/dev/disk/by-id/usb-{vendor_str}_Mass_Storage_{id}-0:{lun}'


def get_hid_dev(id, vendor_str, product_str, event):
    return f'/dev/input/by-id/usb-{vendor_str}_{product_str}_{id}-{event}'


def get_alsa_capture_dev(id):
    pattern = f'/dev/snd/by-id/usb-*_{id}-*'
    for dev in glob.glob(pattern):
        try:
            link = os.path.basename(os.path.realpath(dev))
        except OSError:
            continue
        m = re.match(r'controlC(\d+)', link)
        if m:
            return f'hw:{m.group(1)},0'
    return None


def open_serial_dev(port: str):
    timeout = enum_timeout()
    ser = None
    while timeout > 0:
        if os.path.exists(port):
            try:
                # write_timeout: a wedged device otherwise blocks ser.write() forever,
                # hanging the worker until the pool/job timeout kills the whole run
                ser = serial.Serial(port, baudrate=115200, timeout=SERIAL_READ_TIMEOUT,
                                    write_timeout=SERIAL_WRITE_TIMEOUT)
                break
            except serial.SerialException:
                print(f'serial {port} not reaady {timeout} sec')
                pass
        time.sleep(0.1)
        timeout -= 0.1

    assert timeout > 0, f'Cannot open port f{port}' if os.path.exists(port) else f'Port {port} not existed'
    assert ser is not None
    return ser


def serial_write_all(ser: serial.Serial, data: bytes):
    # write_timeout is a total deadline for the whole call (pyserial keeps partial progress
    # internally). A timeout means the device stopped draining — treat it as fatal: pyserial
    # loses the partial-write count on raise, so retrying would duplicate bytes on the wire.
    try:
        ser.write(data)
    except serial.SerialTimeoutException:
        raise AssertionError(f'Serial write timeout after {SERIAL_WRITE_TIMEOUT:.1f}s')


def read_disk_file(uid: str, lun: int, fname: str) -> bytes:
    # Reads a file from a FAT volume on a block device without mounting it.
    # Requires mtools: `apt install mtools` (no pip dependency).
    dev = get_disk_dev(uid, 'TinyUSB', lun)
    last_err = None

    def try_read():
        nonlocal last_err
        if not os.path.exists(dev):
            return None
        try:
            data = subprocess.check_output(
                ['mtype', '-i', dev, f'::/{fname}'], stderr=subprocess.PIPE)
            assert data, f'Cannot read file {fname} from {dev}'
            return data
        except subprocess.CalledProcessError as e:
            last_err = e.stderr.decode(errors='replace').strip()
            return None

    data = wait_until(try_read)
    if data is None:
        raise AssertionError(f'mtype failed on {dev}: {last_err}' if last_err else f'Storage {dev} not existed')
    return data


def open_mtp_dev(uid):
    mtp = MTP()

    def try_open():
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
        return None

    return wait_until(try_open)


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
    def try_find():
        lp_dev = get_printer_dev(id, vendor_str, product_str, ifnum)
        return lp_dev if lp_dev and os.path.exists(lp_dev) else None

    lp_dev = wait_until(try_find)
    assert lp_dev, f'Printer device not found for {id} if{ifnum:02d}'
    return lp_dev


# -------------------------------------------------------------
# Tests: dual
# -------------------------------------------------------------
def test_dual_host_info_to_device_cdc(board):
    uid = board['uid']
    declared_devs = [f'{d["vid_pid"]}_{d["serial"]}' for d in board['tests']['dev_attached']]
    port = hil_flash.get_serial_dev(uid, 'TinyUSB', "TinyUSB_Device", 0)
    ser = open_serial_dev(port)
    ser.timeout = 0.1

    # read until all expected devices are enumerated
    data = b''
    timeout = enum_timeout()
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

    port = hil_flash.get_serial_dev(flasher["uid"], None, None, 0)
    ser = open_serial_dev(port)
    ser.timeout = 0.1

    # reset device since we can miss the first line
    ret = getattr(hil_flash, f'reset_{flasher["name"].lower()}')(board)
    assert ret.returncode == 0, 'Failed to reset device'

    # read until all expected devices are enumerated
    data = b''
    timeout = enum_timeout()
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

    port = hil_flash.get_serial_dev(flasher["uid"], None, None, 0)
    ser = open_serial_dev(port)
    ser.timeout = 0.1

    # reset device to catch mount messages
    ret = getattr(hil_flash, f'reset_{flasher["name"].lower()}')(board)
    assert ret.returncode == 0, 'Failed to reset device'

    # Wait for all expected mount messages
    data = b''
    timeout = enum_timeout()
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
        serial_write_all(ser, echo_data[offset:offset + chunk_size])
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

    port = hil_flash.get_serial_dev(flasher["uid"], None, None, 0)
    ser = open_serial_dev(port)
    ser.timeout = 0.1

    # reset device to catch mount messages
    ret = getattr(hil_flash, f'reset_{flasher["name"].lower()}')(board)
    assert ret.returncode == 0, 'Failed to reset device'

    # Wait for MSC mount (Disk Size message)
    data = b''
    timeout = enum_timeout()
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
        serial_write_all(ser, ch.encode())
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
        serial_write_all(ser, ch.encode())
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
    speed = None
    for line in resp_text.splitlines():
        if 'KB/s' in line:
            print(f'{line.strip()} ', end='')
            m = re.search(r'([\d.]+)\s*([KMG]B/s)', line)  # MSC read speed for the report cell
            if m:
                speed = f'{m.group(1)} {m.group(2)}'
            break

    ser.close()
    assert speed is not None, 'MSC read produced no speed report (dd stalled or failed)'
    return speed


def test_host_msc_file_explorer_freertos(board):
    return test_host_msc_file_explorer(board)


# -------------------------------------------------------------
# Tests: device
# -------------------------------------------------------------
def test_device_board_test(board):
    # Dummy test
    pass


def test_device_cdc_dual_ports(board):
    uid = board['uid']
    port = [
        hil_flash.get_serial_dev(uid, 'TinyUSB', "TinyUSB_Device", 0),
        hil_flash.get_serial_dev(uid, 'TinyUSB', "TinyUSB_Device", 2)
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
            serial_write_all(ser[writer], payload[offset:offset + chunk_size])
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
    port = hil_flash.get_serial_dev(uid, 'TinyUSB', "TinyUSB_Device", 0)
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
            serial_write_all(ser, test_str[offset:offset + chunk_size])
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
    timeout = enum_timeout()
    while timeout > 0:
        if os.path.exists(dev):
            break
        time.sleep(0.1); timeout -= 0.1
    assert timeout > 0, f'Disk {dev} not found'

    # Wait for CDC tty enumeration
    tty = hil_flash.get_serial_dev(uid, 'TinyUSB', 'Throughput', 0)
    timeout = enum_timeout()
    while timeout > 0:
        if os.path.exists(tty):
            break
        time.sleep(0.1); timeout -= 0.1
    assert timeout > 0, f'CDC tty {tty} not found'

    # Detect speed (12 Mbps FS / 480 Mbps HS) for payload scaling
    is_fs = False
    for f in glob.glob('/sys/bus/usb/devices/*/serial'):
        try:
            if open(f).read().strip().lower() == uid.lower():
                is_fs = (open(os.path.join(os.path.dirname(f), 'speed')).read().strip() == '12')
                break
        except (OSError, ValueError):
            pass

    # Put tty in raw mode so dd sees pure binary throughput.
    rs = hil_flash.run_cmd(f'timeout 30 stty -F {tty} raw -echo')
    assert rs.returncode == 0, f'stty failed: {hil_flash.cmd_stdout_text(rs.stdout)}'

    # Payload aim: ~5 s per direction at FS (~830 kB/s), much less at HS.
    msc_count = 2 if is_fs else 16    # bs=1M
    cdc_count = 16 if is_fs else 128  # bs=64K

    tmp_file = f'/tmp/cdc_msc_tp_{uid}.bin'

    rw = hil_flash.run_cmd(f'timeout 30 dd if=/dev/zero of={tty} bs=64K count={cdc_count} 2>&1')
    assert rw.returncode == 0, f'CDC dd write failed: {hil_flash.cmd_stdout_text(rw.stdout)}'
    cdc_w = parse_speed(hil_flash.cmd_stdout_text(rw.stdout))

    rr = hil_flash.run_cmd(f'timeout 30 dd if={tty} of=/dev/null bs=64K count={cdc_count} iflag=fullblock 2>&1')
    assert rr.returncode == 0, f'CDC dd read failed: {hil_flash.cmd_stdout_text(rr.stdout)}'
    cdc_r = parse_speed(hil_flash.cmd_stdout_text(rr.stdout))

    rmr = hil_flash.run_cmd(f'dd if={dev} of={tmp_file} bs=1M count={msc_count} iflag=direct 2>&1')
    assert rmr.returncode == 0, f'MSC dd read failed: {hil_flash.cmd_stdout_text(rmr.stdout)}'
    msc_r = parse_speed(hil_flash.cmd_stdout_text(rmr.stdout))

    rmw = hil_flash.run_cmd(f'dd if={tmp_file} of={dev} bs=1M count={msc_count} oflag=direct 2>&1')
    assert rmw.returncode == 0, f'MSC dd write failed: {hil_flash.cmd_stdout_text(rmw.stdout)}'
    msc_w = parse_speed(hil_flash.cmd_stdout_text(rmw.stdout))

    try:
        os.remove(tmp_file)
    except OSError:
        pass

    print(f'  CDC read {cdc_r} write {cdc_w}, MSC read {msc_r} write {msc_w}  ', end='')

    # compact read/write speeds for the report cell, e.g. "✅ C 652/422k M 1.1M/783k"
    # (C=CDC, M=MSC; the unit is shown once when both sides share it)
    def short(s):
        return (s.split()[0].rstrip('0').rstrip('.') + s.split()[-1][0]) if ' ' in s else s

    def pair(r, w):
        r, w = short(r), short(w)
        if r[-1:] == w[-1:] and r[-1:].isalpha():
            r = r[:-1]
        return f'{r}/{w}'

    return f'{REPORT_CELL["pass"]} C {pair(cdc_r, cdc_w)} M {pair(msc_r, msc_w)}'


def test_device_dfu(board):
    uid = board['uid']

    # Wait device enum. Deadline-based: dfu-util -l itself takes ~1 s per call, which a
    # per-iteration countdown would not charge against the budget.
    deadline = time.monotonic() + enum_timeout()
    found = False
    while time.monotonic() < deadline:
        ret = hil_flash.run_cmd(f'dfu-util -l')
        stdout = hil_flash.cmd_stdout_text(ret.stdout)
        if f'serial="{uid}"' in stdout and 'Found DFU: [cafe:400b]' in stdout:
            found = True
            break
        time.sleep(1)

    assert found, 'Device not available'

    f_dfu0 = f'dfu0_{uid}'
    f_dfu1 = f'dfu1_{uid}'

    # Test upload
    try:
        os.remove(f_dfu0)
        os.remove(f_dfu1)
    except OSError:
        pass

    ret = hil_flash.run_cmd(f'dfu-util -S {uid} -a 0 -U {f_dfu0}')
    assert ret.returncode == 0, 'Upload failed'

    ret = hil_flash.run_cmd(f'dfu-util -S {uid} -a 1 -U {f_dfu1}')
    assert ret.returncode == 0, 'Upload failed'

    with open(f_dfu0) as f:
        assert 'Hello world from TinyUSB DFU! - Partition 0' in f.read(), 'Wrong uploaded data'

    with open(f_dfu1) as f:
        assert 'Hello world from TinyUSB DFU! - Partition 1' in f.read(), 'Wrong uploaded data'

    os.remove(f_dfu0)
    os.remove(f_dfu1)


def test_device_dfu_runtime(board):
    uid = board['uid']
    # Wait device enum (deadline-based, see test_device_dfu)
    deadline = time.monotonic() + enum_timeout()
    found = False
    while time.monotonic() < deadline:
        ret = hil_flash.run_cmd(f'dfu-util -l')
        stdout = hil_flash.cmd_stdout_text(ret.stdout)
        if f'serial="{uid}"' in stdout and 'Found Runtime: [cafe:400c]' in stdout:
            found = True
            break
        time.sleep(1)

    assert found, 'Device not available'


def test_device_hid_boot_interface(board):
    uid = board['uid']
    kbd = get_hid_dev(uid, 'TinyUSB', 'TinyUSB_Device', 'event-kbd')
    mouse1 = get_hid_dev(uid, 'TinyUSB', 'TinyUSB_Device', 'if01-event-mouse')
    mouse2 = get_hid_dev(uid, 'TinyUSB', 'TinyUSB_Device', 'if01-mouse')
    # Wait device enum
    timeout = enum_timeout()
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
    cdc_port = hil_flash.get_serial_dev(uid, 'TinyUSB', "TinyUSB_Device", 0)
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
            serial_write_all(ser, test_data[offset:offset + chunk_size])
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
    deadline = time.monotonic() + iface_timeout
    host_ip = None
    while time.monotonic() < deadline:
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
    deadline = time.monotonic() + enum_timeout()
    last_err = None
    while time.monotonic() < deadline:
        try:
            with socket.create_connection((device_ip, iperf_port), timeout=1):
                last_err = None
                break
        except OSError as e:
            last_err = e
            time.sleep(0.3)
    assert last_err is None, f'iperf TCP {device_ip}:{iperf_port} not accepting within {enum_timeout()}s: {last_err}'

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
    timeout = enum_timeout()
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
        end_time = time.monotonic() + 3
        while time.monotonic() < end_time:
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


def test_device_audio_test_freertos(board):
    uid = board['uid']

    if os.name == 'nt':
        return 'skipped'

    pcm = None
    timeout = enum_timeout()
    while timeout > 0:
        pcm = get_alsa_capture_dev(uid)
        if pcm:
            break
        time.sleep(1)
        timeout -= 1

    assert pcm is not None, f'ALSA capture device not found for {uid}'

    raw_path = f'/tmp/tinyusb_audio_{uid}.raw'
    cmd = [
        'arecord',
        '-D', pcm,
        '-q',
        '-f', 'S16_LE',
        '-c', '1',
        '-r', '48000',
        '-d', '2',
        '-t', 'raw',
        raw_path,
    ]

    ret = subprocess.run(cmd, capture_output=True, text=True, timeout=20)
    assert ret.returncode == 0, f'arecord failed: {ret.stderr.strip() or ret.stdout.strip()}'

    try:
        with open(raw_path, 'rb') as f:
            raw = f.read()
    finally:
        try:
            os.remove(raw_path)
        except OSError:
            pass

    assert len(raw) >= 48000, f'Captured too little audio: {len(raw)} bytes'
    assert (len(raw) % 2) == 0, f'Invalid 16-bit audio length: {len(raw)}'

    sample_count = len(raw) // 2
    samples = [int.from_bytes(raw[i:i + 2], 'little', signed=False) for i in range(0, len(raw), 2)]
    assert sample_count > 1024, f'Not enough samples captured: {sample_count}'

    # The firmware sends a continuous uint16 ramp. Using ALSA hw: capture bypasses
    # PulseAudio processing, so most adjacent samples should differ by exactly 1.
    total_diffs = sample_count - 1
    one_step = 0
    near_step = 0
    for i in range(total_diffs):
        d = (samples[i + 1] - samples[i]) & 0xFFFF
        if d == 1:
            one_step += 1
        if d in (0, 1, 2, 47, 48, 49):
            near_step += 1

    one_ratio = one_step / total_diffs
    near_ratio = near_step / total_diffs
    assert one_ratio >= 0.85, f'Unexpected audio pattern (strict ratio={one_ratio:.3f})'
    assert near_ratio >= 0.98, f'Unexpected audio pattern (relaxed ratio={near_ratio:.3f})'

    print(f'  ALSA {pcm} strict={one_ratio:.3f} relaxed={near_ratio:.3f}', end='')


def test_device_hid_generic_inout(board):
    uid = board['uid']
    import hid  # cython-hidapi (pip: hidapi, apt: python3-hid)

    # Find HID device by UID (VID=0xCafe)
    timeout = enum_timeout()
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

    h = hid.device()
    h.open(dev['vendor_id'], dev['product_id'], uid)
    try:
        # Echo test: send random data and verify echo
        for size in [8, 32, 63]:
            # Report ID (0) + payload, padded to 64 bytes
            payload = bytes([random.randint(1, 255) for _ in range(size)])
            report = bytes([0]) + payload + bytes(64 - size)
            h.write(report)
            echo = h.read(64, 2000)
            assert echo and len(echo) >= size, (
                f'HID echo timeout or short read ({size} bytes)')
            assert bytes(echo[:size]) == payload, (
                f'HID echo wrong data ({size} bytes):\n'
                f'  expected: {payload.hex()}\n  received: {bytes(echo[:size]).hex()}')
    finally:
        h.close()


def test_device_usbtest(board):
    # Run the Linux testusb tier-4 battery (test/hil/usbtest.py) against the enumerated cafe:4010
    # device; surface the pass count in the report cell ("✅ 30/30", or "❌ 29/30" on a partial).
    uid = board['uid']

    def usbtest_enumerated():
        # match VID:PID too, not just the serial: right after flashing, the previous example's
        # enumeration (same serial, different PID) can linger and would fail usbtest.py's lookup
        for f in glob.glob('/sys/bus/usb/devices/*/serial'):
            d = os.path.dirname(f)
            try:
                if (open(f).read().strip().lower() == uid.lower()
                        and open(os.path.join(d, 'idVendor')).read().strip() == 'cafe'
                        and open(os.path.join(d, 'idProduct')).read().strip() == '4010'):
                    return True
            except OSError:
                pass
        return False

    end = time.monotonic() + enum_timeout()
    while time.monotonic() < end and not usbtest_enumerated():
        time.sleep(0.2)
    # fail before usbtest_permit: an absent device would otherwise queue on the battery
    # mutex for minutes behind real batteries just to have usbtest.py report "no device"
    if not usbtest_enumerated():
        # 0/30 rather than a bare cell: the battery never ran (30 = standard case count)
        raise TestFail(f'no cafe:4010 device with serial {uid}',
                       metric=f'{REPORT_CELL["fail"]} 0/30')
    # settle: right after flashing the enumeration can bounce once (and on dual-port parts like
    # CH32V307 the other port's stale usbtest node — same serial and PID — lingers a moment);
    # running testusb into that gap sees the device drop mid-case
    time.sleep(3)

    # --keep-binding is required for concurrent batteries: usbtest.py's cleanup unbinds
    # EVERY usbtest-bound interface (releasing stale same-PID grabs), which would kill a
    # peer battery mid-run under USBTEST_PARALLEL > 1; the unbind path has also wedged a
    # host xHCI (usb_hcd_alloc_bandwidth) on this rig. Leaving bindings is harmless with
    # unique example PIDs - the next example re-enumerates under a different PID and binds
    # its normal driver. usbtest_permit budgets USBTEST_PARALLEL batteries per controller.
    script = Path(__file__).resolve().parent / 'usbtest.py'
    cmd = f'python3 "{script}" --serial "{uid}" --json --keep-binding --timeout 60'
    with hil_lock.usbtest_permit(uid):
        r = hil_flash.run_cmd(cmd, timeout=200)
    out = hil_flash.cmd_stdout_text(r.stdout)
    brace = out.find('{')
    try:
        data = json.loads(out[brace:])
        passed, failed = int(data['passed']), int(data['failed'])
    except (ValueError, KeyError, json.JSONDecodeError):
        raise TestFail(f'usbtest did not run: {compact_output(out) or hil_flash.cmd_stdout_text(r.stderr)}',
                       metric=f'{REPORT_CELL["fail"]} 0/30')

    total = passed + failed
    if failed == 0 and total > 0:
        return f'{REPORT_CELL["pass"]} {passed}/{total}'
    bad = [c.get('num') for c in data.get('cases', []) if c.get('status') != 'PASS']
    raise TestFail(f'usbtest {passed}/{total} (cases failed: {bad})',
                   metric=f'{REPORT_CELL["fail"]} {passed}/{total}')


# -------------------------------------------------------------
# Main
# -------------------------------------------------------------


def test_example(board: Board, variant: str, example: str) -> tuple[int, str, str | None]:
    """
    Test example firmware
    :param board: board dict
    :param variant: build variant name = build dir (cmake-build-<variant>) and report row
    :param example: example name
    :return: (err_count, status, metric) where err_count is 0 on success/skip or
             1 on failure, status is one of 'pass'/'fail'/'skip' (a missing binary
             counts as 'skip'), and metric is an optional string a test returns to
             show in its report cell instead of the pass symbol (e.g. speed)
    """
    err_count = 0
    result_status = 'fail'
    metric = None

    test_name = f'{variant:40} {example:30} ...'

    # --skip-flash runs whatever is already on the board, so any build counts as present:
    # only the flashing path needs the artifact this board's flasher actually consumes.
    # Filtering there too would skip the test as "no binary" over an extension it never uses.
    fw_name = hil_flash.find_firmware(variant, example,
                                      flasher=None if skip_flash else board['flasher']['name'])
    if fw_name is None:
        log_line(f'{test_name} Skip (no binary)')
        return 0, 'skip', None

    if verbose:
        log_line(f'Firmware {fw_name}')

    # flash firmware (unless --skip-flash), then run the test. Both may fail randomly,
    # retry a few times.
    global _enum_timeout
    start_s = time.time()
    flash_ok = True
    last_err = ''
    last_detail = ''
    for i in range(max_retry):
        _enum_timeout = ENUM_TIMEOUT if i == 0 else ENUM_TIMEOUT_RETRY
        attempt_out = io.StringIO()
        with redirect_stdout(attempt_out):
            if not skip_flash:
                with hil_lock.flash_permit(board['uid']):
                    t_flash = time.monotonic()
                    ret = getattr(hil_flash, f'flash_{board["flasher"]["name"].lower()}')(board, str(fw_name))
                    if PROFILE:
                        log_line(f'[prof] {variant} {example} flash attempt {i + 1}: '
                                 f'{time.monotonic() - t_flash:.1f}s rc={ret.returncode}')
                    flash_ok = (ret.returncode == 0)
                    # A wedged RP2040/RP2350 DAP answers nothing and the probe has no reset
                    # line, so the retry would fail identically; POR it via the Rescue DP
                    # first. No-op for every other board and every other flash failure.
                    if not flash_ok and i + 1 < max_retry and \
                            hil_flash.rescue_openocd(board, hil_flash.cmd_stdout_text(ret.stdout)):
                        log_line(f'{variant} {example}: DAP wedged, rescued via Rescue DP')
            if flash_ok:
                try:
                    tret = globals()[f'test_{example.replace("/", "_")}'](board)
                    last_detail = compact_output(attempt_out.getvalue())
                    if tret == 'skipped':
                        status = STATUS_SKIPPED
                        result_status = 'skip'
                    else:
                        status = STATUS_OK
                        result_status = 'pass'
                        # a test may return a string to show in its report cell (e.g. speed)
                        metric = tret if isinstance(tret, str) else None
                    msg = f'{test_name}  {status}'
                    if last_detail:
                        msg += f'  {last_detail}'
                    msg += f'  in {time.time() - start_s:.1f}s'
                    log_line(msg)
                    break
                except Exception as e:
                    last_err = str(e)
                    last_detail = compact_output(attempt_out.getvalue())
                    if i == max_retry - 1:
                        err_count += 1
                        # a failing test may still carry a metric to show in its cell (e.g. "❌ 29/30")
                        metric = getattr(e, 'metric', None)
                        msg = f'{test_name}  {STATUS_FAILED}: {e}'
                        if last_detail:
                            msg += f'  {last_detail}'
                        msg += f'  in {time.time() - start_s:.1f}s'
                        log_line(msg)
                    else:
                        msg = f'{test_name}  retry {i+2}/{max_retry}: test failed: {e}'
                        if last_detail:
                            msg += f'  {last_detail}'
                        log_line(msg)
                        time.sleep(0.5)
            else:
                last_err = 'Flash failed'
                last_detail = compact_output(attempt_out.getvalue())
                if i < max_retry - 1:
                    msg = f'{test_name}  retry {i+2}/{max_retry}: flash failed'
                    if last_detail:
                        msg += f'  {last_detail}'
                    log_line(msg)
                time.sleep(0.5)

    if not flash_ok:
        err_count += 1
        msg = f'{test_name}  Flash {STATUS_FAILED}'
        if last_err:
            msg += f': {last_err}'
        if last_detail:
            msg += f'  {last_detail}'
        msg += f'  in {time.time() - start_s:.1f}s'
        log_line(msg)

    return err_count, result_status, metric


def build_board(board: Board) -> tuple[str, int]:
    """Build firmware for this board via tools/build.py.
    Honors board config's variant list and build.args defines.
    Output goes to cmake-build/cmake-build-<variant>/ (tools/build.py layout)."""
    name = board['name']
    bcfg = cast(BuildCfg, board.get('build', {}))
    extra_defs = bcfg.get('args', [])
    variants = board.get('variant') or [{'name': name, 'flags': ''}]

    failed = 0
    for v in variants:
        cmd = [sys.executable, str(hil_flash.TINYUSB_ROOT / 'tools' / 'build.py'), '-b', name]
        for d in extra_defs:
            cmd += ['-D', d]
        if v['name'] != name:
            cmd += ['--build-name', v['name']]
        for d in v.get('defines', []):
            cmd += ['-D', d]
        for tok in v.get('flags', '').split():
            cmd += [f'--cflag={tok}']
        if verbose:
            cmd.append('-v')
            print(f'  + {" ".join(cmd)}')
        r = subprocess.run(cmd, cwd=hil_flash.TINYUSB_ROOT)
        if r.returncode != 0:
            failed += 1
    return name, failed


# pseudo-test column for a variant boundary the park-flash could not clear (see below)
BOUNDARY_CELL = 'same-PID boundary'


def test_board(board: Board) -> tuple[str, int, list[str], list, float]:
    name = board['name']
    flasher = board['flasher']

    try:
        _lock_fh = hil_lock.acquire_board_lock(name)
    except RuntimeError as e:
        log_line(f'{name:25} {STATUS_FAILED}: {e}')
        # visible report row so the ❌ matches the exit code; failed-tests stays
        # empty so a re-run repeats the whole board (no bogus -bt test filter)
        return name, 1, [], [(name, {'board-locked': 'fail'}, None)], 0.0
    # after the lock: flock wait behind a concurrent run is not board cost
    t_board = time.monotonic()
    try:
        # default to all tests
        test_list = []

        if name in board_test:
            test_list = board_test[name]
        elif len(test_only) > 0:
            # Explicit -t: filter against the board's capabilities so a device-only
            # board doesn't try to run host/dual tests (the test functions need a
            # `dev_attached` entry in the board config that won't exist).
            board_tests = board.get('tests', {})
            if 'only' in board_tests:
                allowed = set(board_tests['only'])
                test_list = [t for t in test_only if t in allowed]
            else:
                for t in test_only:
                    category = t.split('/', 1)[0]
                    if board_tests.get(category) is True:
                        test_list.append(t)
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
                            log_line(f'{name:25} {skip:30} ... Skip')

        err_count = 0
        failed_tests = []
        board_wide_fail = False  # re-run the whole board, not a subset of its tests
        rows = []  # list of (row_label, {example: status}, duration) — one row per build variant
        # a -t/-bt filtered run times only a subset; report no duration so an accumulate
        # re-run keeps the previous full-run value
        partial = bool(test_only) or name in board_test
        variants = board.get('variant') or [{'name': name, 'flags': ''}]

        prev_last = None  # last test of the previous variant: the variant boundary is an adjacency too
        for v in variants:
            vname = v['name']
            # Shuffle each (board, variant)'s run order — de-synchronizes the worker pool so
            # usbtest batteries and flash churn spread across the timeline instead of convoying,
            # and surfaces order-dependent bugs. Seeded for replay (HIL_SHUFFLE_SEED, logged by
            # main). Unique per-example PIDs make any two different examples re-enumerate; only
            # the variant boundary can repeat the same example (same PID) — swap it away.
            run_list = list(test_list)
            if shuffle_seed is not None and len(run_list) > 1:
                random.Random(f'{shuffle_seed}:{name}:{vname}').shuffle(run_list)
                if run_list[0] == prev_last:
                    run_list[0], run_list[-1] = run_list[-1], run_list[0]
            cells = {}
            if run_list and run_list[0] == prev_last and not skip_flash:
                # Same example (same PID) still repeats across the boundary: a one-test
                # list (the common case for a -bt scoped run) leaves nothing to swap
                # with. Park on board_test first - it disables the board's USB, so the
                # PID goes away and the next flash must re-enumerate to be seen.
                t_park = time.monotonic()
                park_ec, park_status, _ = test_example(board, vname, 'device/board_test')
                if park_ec or park_status == 'skip':
                    # Boundary not cleared: the previous variant's device may still be
                    # enumerated under the same PID, so this variant's tests could pass
                    # against its firmware. Skip them - a false green proves nothing and
                    # is worse than a gap - and record the boundary itself as the failure
                    # (a visible ❌ cell, mirroring the board-lock row above) so the report
                    # matches the exit code instead of rendering all-green.
                    why = 'no board_test binary' if park_status == 'skip' else 'park flash failed'
                    log_line(f'{vname:40} {"same-PID boundary":30} {STATUS_FAILED}: not cleared ({why}); '
                             f'skipping {len(run_list)} test(s) on this variant')
                    err_count += 1
                    cells[BOUNDARY_CELL] = 'fail'
                    # blaming run_list[0] would re-run an innocent test that then passes,
                    # leaving the boundary unretested; re-run the whole board instead
                    board_wide_fail = True
                    # leave prev_last alone: the board still holds the previous variant's
                    # firmware, so the next variant must attempt the park again
                    run_list = []
                t_board += time.monotonic() - t_park  # park is teardown, not board cost
            if run_list:
                prev_last = run_list[-1]
            t_variant = time.monotonic()
            for test in run_list:
                ec, status, metric = test_example(board, vname, test)
                err_count += ec
                cells[test] = metric if metric else status
                if ec > 0:
                    failed_tests.append(test)
            dur = f'{time.monotonic() - t_variant:.0f}s' if run_list and not partial else None
            rows.append((vname, cells, dur))

        # board duration excludes the teardown park-flash below; a partial (filtered)
        # run reports 0.0 so it never overwrites a cached full-run duration
        t_total = 0.0 if partial else time.monotonic() - t_board

        # flash board_test last to disable board's usb (skipped when --skip-flash is set);
        # this is teardown/park, not a test — not recorded in the report
        if not skip_flash:
            test_example(board, variants[0]['name'], 'device/board_test')

        return name, err_count, [] if board_wide_fail else sorted(set(failed_tests)), rows, t_total
    finally:
        if _lock_fh:
            try:
                # clear our pid record before dropping the flock: this worker
                # process lives on (pool reuse), so a stale record would make
                # hil_lock.py's pid-liveness checks report a freed board as
                # still locked for the rest of the run
                _lock_fh.truncate(0)
            except OSError:
                pass
            _lock_fh.close()


REPORT_MD = 'hil_report.md'
REPORT_JSON = 'hil_report.json'
# controller hints learned from previous runs: uid -> {'name', 'pci', 'duration'}. Only
# 'pci' is consumed (dispatch order and first-flash budgeting, never battery
# serialization); name/duration are informational. PCI addresses are boot-stable (bus
# numbers are not), so the cache survives reboots and only goes stale on re-cabling.
CONTROLLER_CACHE = Path.home() / '.cache' / 'tinyusb-hil' / 'controller_cache.json'


def schedule_boards(boards: list, pci_of_uid: dict) -> list:
    """Dispatch order: round-robin across host controllers so every controller's
    serialized usbtest battery chain is fed from t=0 instead of one card's boards
    convoying at the head of the queue. Boards without a controller hint form their
    own bucket; config order is kept within a bucket."""
    buckets = {}
    for b in boards:
        buckets.setdefault(pci_of_uid.get(b['uid'], '?'), []).append(b)
    return [b for grp in itertools.zip_longest(*buckets.values()) for b in grp if b is not None]


def render_matrix(rows_all: list) -> str:
    """Render rows (list of (row_label, {example: status}, duration)) as an aligned
    markdown matrix: columns = tests (bare names) centered, boards left-aligned,
    per-row duration as the trailing column."""
    seen = set()
    for _, cells, _ in rows_all:
        seen.update(cells)
    if not seen:
        return 'No tests were run.'

    # metric-bearing columns pinned first (usbtest score, throughput, explorer read speed),
    # the rest alphabetical by bare test name: stable regardless of the (shuffled) execution order
    pinned = ['usbtest', 'cdc_msc_throughput', 'msc_file_explorer', 'msc_file_explorer_freertos']

    def col_key(t):
        name = t.rsplit('/', 1)[-1]
        return (pinned.index(name) if name in pinned else len(pinned), name, t)

    columns = sorted(seen, key=col_key)
    headers = [c.rsplit('/', 1)[-1] for c in columns] + ['duration']  # bare example names

    def cell(cells, col):
        v = cells.get(col)
        if v is None:
            return ''
        return REPORT_CELL.get(v, v)  # status symbol, or a metric string (e.g. speed) verbatim

    rows_vals = [(lbl, [cell(cells, c) for c in columns] + [dur or ''])
                 for lbl, cells, dur in rows_all]
    board_hdr = 'Board'
    board_w = max([len(board_hdr)] + [len(lbl) for lbl, _ in rows_vals])
    col_w = [max([len(h)] + [len(vals[i]) for _, vals in rows_vals])
             for i, h in enumerate(headers)]

    def line(label, values):
        padded = [label.ljust(board_w)] + [v.center(w) for v, w in zip(values, col_w)]
        return '| ' + ' | '.join(padded) + ' |'

    header = line(board_hdr, headers)
    sep = '| ' + '-' * board_w + ' | ' + ' | '.join(':' + '-' * (w - 2) + ':' for w in col_w) + ' |'
    body = [line(lbl, vals) for lbl, vals in rows_vals]

    # tally run cells (blank/not-run cells are absent from the dicts). A cell is a bare status
    # ('pass'/'fail'/'skip') or a metric string that carries its own icon (e.g. "❌ 29/30" is a
    # fail, "✅ 30/30" / "✅ CDC …" a pass), so classify by the leading icon.
    def cell_kind(v):
        if v == 'fail' or (isinstance(v, str) and v.startswith(REPORT_CELL['fail'])):
            return 'fail'
        if v == 'skip' or (isinstance(v, str) and v.startswith(REPORT_CELL['skip'])):
            return 'skip'
        return 'pass'
    kinds = [cell_kind(v) for _, cells, _ in rows_all for v in cells.values()]
    failed = kinds.count('fail')
    skipped = kinds.count('skip')
    passed = kinds.count('pass')
    summary = (f'**{REPORT_CELL["pass"]} {passed} passed · {REPORT_CELL["fail"]} {failed} failed · '
               f'{REPORT_CELL["skip"]} {skipped} skipped · blank not run**')

    return summary + '\n\n' + '\n'.join([header, sep] + body)


def accumulate_report(mret: list, report_dir: Path, fresh: bool, scope: str = '') -> str:
    """Merge this run's results into hil_report.json in report_dir, then (re)write
    the markdown matrix to hil_report.md. `fresh` (a first run, no --accumulate)
    starts a new report; otherwise a re-run accumulates so boards/tests that
    already passed are preserved while re-run cells are updated. `scope` names the
    board filter, if any, so a scoped table is not mistaken for a full one.
    Returns the md."""
    acc = {}  # ordered {row_label: [cells dict, duration str|None]}
    jpath = report_dir / REPORT_JSON
    if not fresh and jpath.is_file():
        try:
            saved = json.loads(jpath.read_text())
            # CI keys the report dir by run id, so the sidecar can only have been
            # written by an earlier attempt of the same run
            for entry in saved.get('rows', []):
                acc[entry['board']] = [dict(entry['cells']), entry.get('duration')]
        except (ValueError, KeyError, TypeError):
            pass  # corrupt/old sidecar: start fresh

    # merge this run: current cells override prior for boards/tests that ran; a filtered
    # run reports duration None, keeping the previous full-run value
    for name, _, _, rows, _ in mret:
        if rows and not any('board-locked' in cells for _, cells, _ in rows):
            # board ran for real this time: clear a stale lock-failure cell
            # (its row is keyed by board name; test rows may be variant names)
            stale = acc.get(name)
            if stale is not None:
                stale[0].pop('board-locked', None)
                if not stale[0]:
                    # variant-keyed boards never repopulate the board-name row —
                    # drop it or it renders as a blank ghost row
                    del acc[name]
        for row_label, cells, dur in rows:
            row = acc.setdefault(row_label, [{}, None])
            # the boundary cell is only ever written on failure, so a re-run of this
            # variant that cleared the boundary must drop the previous attempt's ❌
            if BOUNDARY_CELL not in cells:
                row[0].pop(BOUNDARY_CELL, None)
            row[0].update(cells)
            if dur is not None:
                row[1] = dur

    report_dir.mkdir(parents=True, exist_ok=True)
    jpath.write_text(json.dumps({'rows': [{'board': k, 'cells': c, 'duration': d}
                                          for k, (c, d) in acc.items()]}, indent=2) + '\n')

    md = render_matrix([(k, c, d) for k, (c, d) in acc.items()])
    if scope:
        # a scoped run's small table is otherwise indistinguishable from a full one,
        # and it replaces the previous full table in the sticky PR comment
        md = f'_Scoped run: {scope}. Boards/tests not listed were not run._\n\n' + md
    (report_dir / REPORT_MD).write_text(md + '\n', encoding='utf-8')
    return md


def main() -> None:
    """
    Hardware test on specified boards
    """
    global verbose
    global test_only
    global board_test
    global max_retry
    global skip_flash

    duration = time.time()

    parser = argparse.ArgumentParser()
    parser.add_argument('config_file', help='Configuration JSON file')
    parser.add_argument('-b', '--board', action='append', default=[], help='Boards to test, all if not specified')
    parser.add_argument('--flasher', action='append', default=[],
                        help='Only boards using these flashers, e.g. esptool '
                             '(for splitting one config across CI jobs)')
    parser.add_argument('--exclude-flasher', action='append', default=[],
                        help='Exclude boards using these flashers')
    parser.add_argument('-a', '--accumulate', action='store_true',
                        help='Merge results into the existing report instead of starting fresh '
                             '(re-runs; the .failed file starts with this)')
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
    verbose = args.verbose
    hil_flash.verbose = args.verbose
    test_only = args.test_only
    for entry in args.board_test:
        bname, _, tnames = entry.partition(':')
        if not bname or not tnames:
            parser.error(f'invalid --board-test value: {entry!r} (expected BOARD:test1,test2)')
        board_test[bname] = [t for t in tnames.split(',') if t]
    hil_flash.build_dir = args.build_dir
    max_retry = args.retry
    skip_flash = args.skip_flash

    # if config file is not found, try to find it in the same directory as this script
    if not config_file.exists():
        config_file = Path(__file__).resolve().parent / config_file
    with config_file.open() as f:
        config = cast(HilConfig, json.load(f))

    if len(boards) == 0:
        config_boards = list(config['boards'])
    else:
        unknown = [b for b in boards if b not in {e['name'] for e in config['boards']}]
        if unknown:
            # exiting 0 with 'No tests were run.' would read as a green HIL run
            print(f'ERROR: board(s) not in {config_file.name}: {", ".join(unknown)}')
            sys.exit(1)
        config_boards = [e for e in config['boards'] if e['name'] in boards]
    config_boards = [e for e in config_boards if e['flasher']['name'] not in args.exclude_flasher
                     and (not args.flasher or e['flasher']['name'] in args.flasher)]

    build_err = 0
    if args.build:
        if hil_flash.build_dir != 'cmake-build':
            print(f'warning: --build writes into cmake-build/, but -B is {hil_flash.build_dir!r}; '
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

    # HIL report sidecar (hil_report.json/.md) and the .failed re-run spec live in
    # report_dir (CI keys it by run id, so it persists across run attempts but is
    # private to one run). A full run starts fresh; a re-run (--accumulate, which
    # the generated .failed spec always starts with) merges so already-passed
    # boards/tests are preserved. Clear prior state up front on a fresh run so a
    # crash mid-run can't leave a stale report or re-run spec for a retry.
    # -bt alone is not a re-run marker: PR-scoped first attempts pass -bt too.
    report_dir = Path(os.environ.get('HIL_REPORT_DIR', '.'))
    failed_fname = report_dir / (config_file.name + '.failed')
    fresh = not args.accumulate
    if fresh:
        report_dir.mkdir(parents=True, exist_ok=True)
        for f in (REPORT_JSON, REPORT_MD):
            (report_dir / f).unlink(missing_ok=True)
        failed_fname.unlink(missing_ok=True)

    seed = os.getenv('HIL_SHUFFLE_SEED') or str(int(time.time()))
    log_line(f'test-order shuffle seed: {seed} (HIL_SHUFFLE_SEED={seed} to replay); '
             f'flash/usbtest parallel per controller: {hil_lock.FLASH_PARALLEL}/{hil_lock.USBTEST_PARALLEL}; '
             f'enum timeout first/retry: {ENUM_TIMEOUT}/{ENUM_TIMEOUT_RETRY}s')

    hints = {}
    try:
        with CONTROLLER_CACHE.open() as f:
            loaded = json.load(f)
        # tolerate a hand-edited/torn cache: keep only the expected uid -> dict shape
        if isinstance(loaded, dict):
            hints = {k: v for k, v in loaded.items() if isinstance(v, dict)}
    except (OSError, ValueError):
        pass
    hints_by_uid = {uid: h['pci'] for uid, h in hints.items() if h.get('pci')}
    config_boards = schedule_boards(config_boards, hints_by_uid)
    log_line('dispatch order: ' + ', '.join(b['name'] for b in config_boards))

    mgr = Manager()
    cmap = mgr.dict()
    initargs = (Lock(), seed,
                [Semaphore(hil_lock.USBTEST_PARALLEL) for _ in range(hil_lock.CONTROLLER_SLOTS)],
                [Semaphore(hil_lock.FLASH_PARALLEL) for _ in range(hil_lock.CONTROLLER_SLOTS)],
                cmap, Lock(), hints_by_uid)
    with Pool(processes=os.cpu_count() or 1, initializer=init_worker, initargs=initargs) as pool:
        async_ret = pool.map_async(test_board, config_boards)
        try:
            mret = async_ret.get(timeout=POOL_TIMEOUT)
        except MpTimeoutError:
            pool.terminate()
            pool.join()
            raise RuntimeError(f'HIL worker pool timed out after {POOL_TIMEOUT}s')

        err_count = build_err + sum(e[1] for e in mret)
        # generate the re-run spec if anything failed: run ONLY the failed boards (-b),
        # each restricted to its own failed tests (-bt); a board with failures but no
        # test list (e.g. board-locked) re-runs entirely. --accumulate preserves the
        # already-passed cells in the report.
        parts = ['--accumulate']
        for name, err, fts, _, _ in mret:
            if err > 0:
                parts.append(f'-b {name}')
                if fts:
                    parts.append(f'-bt {name}:{",".join(fts)}')
        if len(parts) > 1:  # build-only failures have no boards to re-run
            report_dir.mkdir(parents=True, exist_ok=True)
            with failed_fname.open('w') as f:
                f.write(' '.join(parts))
        else:
            failed_fname.unlink(missing_ok=True)

    # refresh controller hints: pci resolved this run, plus board durations when the
    # full test list ran (a -t/-bt filtered run would understate the board's real cost)
    try:
        if PROFILE:
            # debug snapshot of the run's live uid->PCI / PCI->slot resolutions
            report_dir.mkdir(parents=True, exist_ok=True)
            with (report_dir / 'hil_profile_ctrl.json').open('w') as f:
                json.dump(dict(cmap), f, indent=1, sort_keys=True)
        uid_of = {b['name']: b['uid'] for b in config['boards']}
        for name, _, _, _, dur in mret:
            uid = uid_of.get(name)
            if uid is None:
                continue
            h = dict(hints.get(uid) or {})
            h['name'] = name  # informational: cache is keyed by uid
            h['pci'] = cmap.get(f'uid:{uid}') or h.get('pci')
            if dur > 0:  # test_board reports 0.0 for filtered (partial) runs
                h['duration'] = round(dur, 1)
            hints[uid] = h
        # merge-on-write: another HIL job (e.g. the esp split) may have finished since
        # our startup read - re-read and overlay only this run's boards so its entries
        # survive, then replace atomically so a concurrent reader never sees a torn file
        merged = {}
        try:
            with CONTROLLER_CACHE.open() as f:
                cur = json.load(f)
            if isinstance(cur, dict):
                merged = {k: v for k, v in cur.items() if isinstance(v, dict)}
        except (OSError, ValueError):
            pass
        merged.update({uid_of[n]: hints[uid_of[n]] for n, *_ in mret if n in uid_of})
        CONTROLLER_CACHE.parent.mkdir(parents=True, exist_ok=True)
        tmp = CONTROLLER_CACHE.with_suffix('.json.tmp')
        with tmp.open('w') as f:
            json.dump(merged, f, indent=1, sort_keys=True)
        tmp.replace(CONTROLLER_CACHE)
    except OSError as e:
        print(f'warning: cannot persist controller hints to {CONTROLLER_CACHE}: {e}')

    # board x test result matrix -> hil_report.md (accumulates across re-runs) + stdout
    # -b/-bt in play means a filtered run (PR selection or a re-run spec): say so in the
    # report, which otherwise looks exactly like a full run that happened to be small
    scoped = sorted(set(args.board) | set(board_test))
    scope = f'{len(scoped)} board(s) — {", ".join(scoped)}' if scoped else ''
    report = accumulate_report(mret, report_dir, fresh, scope)
    print()
    print(report)
    print(f'\nReport written to {(report_dir / REPORT_MD).resolve()}')

    duration = time.time() - duration
    print()
    print("-" * 30)
    print(f'Total failed: {err_count} in {duration:.1f}s')
    print("-" * 30)
    sys.exit(err_count)


if __name__ == '__main__':
    main()
