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
#   - System packages: sudo apt install mtools libmtp9 libmtp-runtime alsa-utils iperf
#       mtools      read_disk_file (device/cdc_msc, device/msc_dual_lun)
#       libmtp9     pymtp ctypes load (device/mtp); Debian 13 uses libmtp9t64
#       libmtp-runtime  mtp-probe and the completed-device /dev/libmtp-* marker
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
import signal
import shlex
import sys
import tempfile
import time
from contextlib import redirect_stdout
from pathlib import Path
from typing import TypedDict, NotRequired, cast

import serial
import subprocess
import traceback
import json
import glob
import multiprocessing
from multiprocessing import TimeoutError as MpTimeoutError

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))  # PYTHONSAFEPATH drops it
import hil_flash
from helper import hil_health, hil_lock, hil_util
from helper.hil_util import device_tests, dual_tests, host_test

# Raw Lock/Semaphore objects in Pool initargs are inheritable only under fork
# (spawn/forkserver pickle them and fail at Pool creation), so pin it against an
# interpreter default change. Windows has no fork: fall back so it still IMPORTS there.

_mp = multiprocessing.get_context('fork') if os.name != 'nt' else multiprocessing.get_context()
Pool, Lock, Semaphore, Manager = _mp.Pool, _mp.Lock, _mp.Semaphore, _mp.Manager
import string

# Enumeration wait budget: first attempt ENUM_TIMEOUT, retries the shorter
# ENUM_TIMEOUT_RETRY -- a device that will enumerate shows up within seconds, so a failing
# test costs ~3-5x a passing one instead of 10-30x. Set per attempt by test_example(); a
# module global is safe because each pool worker is its own process.
ENUM_TIMEOUT = 8
ENUM_TIMEOUT_RETRY = 4
_enum_timeout = ENUM_TIMEOUT


def enum_timeout() -> int:
    """Enumeration wait budget for the current test attempt."""
    return _enum_timeout


def wait_until(predicate, step: float = 1.0, timeout: float | None = None):
    """Poll predicate under the per-attempt enum budget. Deadline-based so a slow predicate
    body (subprocess, libmtp scan) counts against the budget. An explicit timeout overrides
    that budget. Returns the first truthy predicate value, or None on timeout."""
    deadline = time.monotonic() + (enum_timeout() if timeout is None else timeout)
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

# Plain (non-ANSI) cell symbols for hil_report.md; a missing binary counts as skipped.
REPORT_CELL = {'pass': '✅', 'fail': '❌', 'skip': '⚪'}


class TestFail(AssertionError):
    """Fail a test but still surface a metric string in its report cell (e.g. usbtest's '❌ 29/30'
    instead of a bare ❌). The cell metric is icon-prefixed so render/tally treat it as a failure."""
    def __init__(self, msg: str, metric: str | None = None, parsed: bool = False):
        super().__init__(msg)
        self.metric = metric
        # parsed=True: a real per-case verdict, so a retry would only re-observe it
        # (test_example skips the rest). A failure to RUN the tool stays retryable.
        self.parsed = parsed


verbose = False
# Set when a HUNG usbtest case could not be recovered: the DUT's usbfs node still has a
# D-state holder, so every later flash on that board enumerates into it, blocks, survives
# SIGKILL and becomes another stray. maxtasksperchild=1 gives each board its own worker,
# so this global is board-scoped; test_board resets it anyway.
board_wedged = ''
max_retry = 1   # mirrors argparse's -r default (see main); defined HERE too so
                # test_example is callable (and testable) without going through main()
PROFILE = os.environ.get('HIL_PROFILE') == '1'  # timestamped logs + permit/flash timing + ctrl-map dump
test_only = []
board_test = {}
skip_flash = False
print_lock = None
shuffle_seed = None  # per-run seed for the per-board test-order shuffle (HIL_SHUFFLE_SEED to replay)
_current_fw = None  # firmware test_example resolved for the RUNNING test (set before each test fn)


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
    # Defense in depth (the emitter already suppresses them, see _ci_log_groups): markers
    # piped into this capture land mid-row, where GitHub renders them literally.
    lines = []
    for ln in raw.replace('\r', '\n').split('\n'):
        ln = hil_util.strip_workflow_markers(ln.strip()).strip()
        if ln:
            lines.append(ln)
    return ' | '.join(lines)

class FlasherCfg(TypedDict):
    name: str
    uid: str
    args: NotRequired[str]     # stlink entries carry no args
    vid_pid: NotRequired[str]  # openocd probe pin, verbatim (e.g. "0x2e8a 0x000c")
    verify: NotRequired[bool]  # openocd read-back verify opt-out (WCH)


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


class VariantCfg(TypedDict, total=False):
    name: str           # build dir (cmake-build-<name>) and HIL report row
    flags: str          # raw CFLAGS, e.g. "-DCFG_TUD_DWC2_DMA_ENABLE=1"
    defines: list[str]  # cmake -D defines, e.g. ["RHPORT_DEVICE=1"] (vs flags which are compiler-only)


class Board(TypedDict):
    name: str
    uid: str
    tests: TestsCfg
    flasher: FlasherCfg
    # every build knob lives here, including a board's always-on defines: a board that
    # needs one carries a single variant named after itself (metro_m4_express /
    # MAX3421_HOST=1), which is exactly what the `or [...]` default below synthesises
    variant: NotRequired[list[VariantCfg]]
    toolchain: NotRequired[str]  # CI build bucket override, e.g. "riscv-gcc" (consumed by hil_ci_set_matrix.py)


class HilConfig(TypedDict):
    boards: list[Board]

# Below the CI job ceilings so THIS guard fires first and still writes a report, well
# above a healthy fleet run (~14 min measured), and deliberately generous: firing early
# abandons boards that were still in flight (30 min fired on 5 of the last 8 HIL jobs),
# while firing late costs minutes on an already-wedged run. The drain keeps whatever had
# already finished either way.
POOL_TIMEOUT = hil_util.pos_int_env('HIL_POOL_TIMEOUT', 3600)


# Headroom on top of a battery's own budget so ONE HUNG recovery (case timeout, SIGKILL
# wait, bounded reflash, settle) can finish. Only spent when cases actually time out.
USBTEST_RECOVERY_BUDGET = hil_util.pos_int_env('HIL_USBTEST_RECOVERY_BUDGET', 250)
# How long usbtest.py may keep starting new cases (--budget). The outer run_cmd timeout is
# always this PLUS the recovery headroom, never a separate literal, or lowering one eats
# the reserve the recovery needs. 0 is refused (usbtest.py reads it as "no limit"); the
# margin over a healthy battery (~200s) keeps contention from becoming BUDGET entries.
USBTEST_BATTERY_BUDGET = hil_util.pos_int_env('HIL_USBTEST_BATTERY_BUDGET', 260)

# The battery checks its budget BEFORE dispatching a case, so it can overshoot by one
# already-started case. Our outer kill must sit ABOVE that or we SIGKILL the battery just
# as it goes to print its JSON, turning ~29 real per-case verdicts into "usbtest did not
# run" and re-paying the whole battery on retry.
# Worst case, from usbtest.py: --timeout 60 (the case) + 5s post-SIGKILL reap +
# dmesg_tail(), which is bounded by HELPER_TIMEOUT=30 and runs on BOTH the FAIL and HUNG
# timeout paths = 95s. 120 leaves a margin; 75 (my first estimate, taken before checking
# dmesg_tail) was 20s SHORT and would have killed the battery mid-print.
USBTEST_OVERSHOOT = 120
SERIAL_READ_TIMEOUT = hil_util.pos_float_env('HIL_SERIAL_READ_TIMEOUT', 5)
SERIAL_WRITE_TIMEOUT = hil_util.pos_float_env('HIL_SERIAL_WRITE_TIMEOUT', 10)


MSC_README_TXT = \
b"This is tinyusb's MassStorage Class demo.\r\n\r\n\
If you find any bugs or get any questions, feel free to file an\r\n\
issue at github.com/hathach/tinyusb"

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
                # write_timeout: see serial_write_all
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
    # write_timeout is a deadline for the whole call. A timeout means the device stopped
    # draining, and it is fatal: pyserial loses the partial-write count on raise, so
    # retrying would duplicate bytes on the wire.
    try:
        ser.write(data)
    except serial.SerialTimeoutException:
        raise AssertionError(f'Serial write timeout after {SERIAL_WRITE_TIMEOUT:.1f}s')


LP_OPEN_TIMEOUT = 5   # bound on opening the printer lp node; see test_device_printer_to_cdc
# Runs under hil_util.run_alongside as `python3 -c`. Inline rather than a file so hil_ci.sh's
# staging list does not need another entry to keep the rig working.
LP_READER = (
    'import os, sys\n'
    'fd = os.open(sys.argv[1], os.O_RDONLY)\n'
    # readiness marker: the parent must not send a byte before the node is open, or the
    # bytes are lost. A blind sleep raced CPython start-up on a loaded rig.
    'open(sys.argv[3], "w").close()\n'
    'want = int(sys.argv[2])\n'
    'buf = b""\n'
    'while len(buf) < want:\n'
    '    chunk = os.read(fd, min(64, want - len(buf)))\n'
    '    if not chunk:\n'
    '        break\n'
    '    buf += chunk\n'
    'sys.stdout.buffer.write(buf)\n'
)
MTYPE_TIMEOUT = 30  # a README-sized read is <1 s; bounds a D-state hang on a wedged device


def read_disk_file(uid: str, lun: int, fname: str) -> bytes:
    # Reads a file from an unmounted FAT volume; needs mtools. run_cmd everywhere in this
    # file rather than subprocess.run/check_output: its post-timeout reap is an unbounded
    # communicate() with no killpg (CPython 3.13.5 subprocess.py:558-565 -- kill(), then
    # communicate() with NO timeout), which never returns on a device wedged in D state,
    # where the kill is queued and never delivered. binary
    # keeps the bytes exact, split_stderr keeps mtype warnings out of them.
    dev = get_disk_dev(uid, 'TinyUSB', lun)
    last_err = None

    def try_read():
        nonlocal last_err
        if not os.path.exists(dev):
            return None
        r = hil_util.run_cmd(f"mtype -i {shlex.quote(dev)} ::/{shlex.quote(fname)}",
                             timeout=MTYPE_TIMEOUT, binary=True, split_stderr=True, quiet=True)
        if r.returncode == 0:
            if r.stdout:
                return r.stdout
            # rc 0 with no data is an answer (empty file, zeroed sectors), not "not
            # ready" — fail now instead of spinning the budget
            raise AssertionError(f'Cannot read file {fname} from {dev}: mtype returned no data')
        last_err = (r.stderr or b'').decode(errors='replace').strip() or f'mtype rc {r.returncode}'
        return None

    data = wait_until(try_read)
    if data is None:
        raise AssertionError(f'Cannot read file {fname} from {dev}: {last_err}' if last_err
                             else f'Storage {dev} not existed')
    return data


# ~5 KB of transfers plus libmtp setup takes seconds, not minutes; a larger value makes a
# wedged MTP board cost that much on every retry, all charged to the pool guard.
MTP_SESSION_MARGIN = 30  # transfer budget after enumeration; past it the session is killed


def get_printer_dev(id: str, vendor_str, product_str, ifnum: int):
    """Find /dev/usb/lpX by matching USB serial, vendor, product, and interface number via sysfs"""
    vendor_str = vendor_str.replace(' ', '_') if vendor_str else ''
    product_str = product_str.replace(' ', '_') if product_str else ''
    for lp in glob.glob('/sys/class/usbmisc/lp*'):
        try:
            # bounded: same device_lock() exposure as the sibling reads (see read_sysfs)
            sn = hil_util.read_sysfs(f'{lp}/device/../serial')
            # UNKNOWN is not None: the sentinel has no __eq__, so an unanswered read
            # would fall through both tests and read as 'not this board' -- the exact
            # absence/unknown conflation read_sysfs exists to prevent.
            if sn is None or sn is hil_util.SYSFS_UNKNOWN:
                continue
            if sn == id:
                return f'/dev/usb/{os.path.basename(lp)}'
        except OSError:   # read_sysfs swallows its own OSError/ValueError; glob can race
            pass
    return None


def open_printer_dev(id: str, vendor_str, product_str, ifnum: int) -> str:
    """Wait for printer device to enumerate and return its path"""
    def try_find():
        lp_dev = get_printer_dev(id, vendor_str, product_str, ifnum)
        return lp_dev if lp_dev and os.path.exists(lp_dev) else None

    lp_dev = wait_until(try_find)
    assert lp_dev, (f'Printer device not found for {id} if{ifnum:02d}'
                    + hil_util.sysfs_blind_note())
    return lp_dev


# -------------------------------------------------------------
# Tests: dual
# -------------------------------------------------------------
def test_dual_host_info_to_device_cdc(board):
    uid = board['uid']
    declared_devs = [f'{d["vid_pid"]}_{d["serial"]}' for d in board['tests']['dev_attached']]
    port = hil_util.get_serial_dev(uid, 'TinyUSB', "TinyUSB_Device", 0)
    ser = open_serial_dev(port)
    ser.timeout = 0.1

    data = b''
    timeout = enum_timeout()
    while timeout > 0:
        new_data = ser.read(ser.in_waiting or 1)
        if new_data:
            data += new_data
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

    port = hil_util.get_serial_dev(flasher["uid"], None, None, 0)
    ser = open_serial_dev(port)
    ser.timeout = 0.1

    # reset device since we can miss the first line
    ret = getattr(hil_flash, f'reset_{flasher["name"].lower()}')(board)
    assert ret.returncode == 0, 'Failed to reset device'

    data = b''
    timeout = enum_timeout()
    while timeout > 0:
        new_data = ser.read(ser.in_waiting or 1)
        if new_data:
            data += new_data
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

    port = hil_util.get_serial_dev(flasher["uid"], None, None, 0)
    ser = open_serial_dev(port)
    ser.timeout = 0.1

    # reset device to catch mount messages
    ret = getattr(hil_flash, f'reset_{flasher["name"].lower()}')(board)
    assert ret.returncode == 0, 'Failed to reset device'

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

    vid_pid_name = {
        '0403_6001': 'FTDI', '0403_6010': 'FTDI', '0403_6011': 'FTDI', '0403_6014': 'FTDI',
        '10c4_ea60': 'CP210x', '10c4_ea70': 'CP210x',
        '067b_2303': 'PL2303', '067b_23a3': 'PL2303',
        '1a86_7523': 'CH340', '1a86_7522': 'CH340',
        '1a86_55d3': 'CH9102', '1a86_55d4': 'CH9102',
    }

    lines = data.decode('utf-8', errors='ignore').splitlines()

    if cdc_devs:
        assert b'CDC Interface is mounted' in data, 'CDC device not mounted on host'
        dev = cdc_devs[0]
        chip_name = vid_pid_name.get(dev['vid_pid'], dev['vid_pid'])
        for l in lines:
            if 'CDC Interface is mounted' in l:
                print(f'\r\n  {chip_name}: {l} ', end='')

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

    echo_len = 1024
    echo_data = rand_ascii(echo_len)
    ser.reset_input_buffer()
    offset = 0
    while offset < echo_len:
        chunk_size = min(random.randint(1, packet_size), echo_len - offset)
        serial_write_all(ser, echo_data[offset:offset + chunk_size])
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

    port = hil_util.get_serial_dev(flasher["uid"], None, None, 0)
    ser = open_serial_dev(port)
    ser.timeout = 0.1

    # reset device to catch mount messages
    ret = getattr(hil_flash, f'reset_{flasher["name"].lower()}')(board)
    assert ret.returncode == 0, 'Failed to reset device'

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

    time.sleep(0.5)
    ser.reset_input_buffer()
    for ch in 'dd 1024\r':
        serial_write_all(ser, ch.encode())
        time.sleep(0.002)

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
    pass


def test_device_cdc_dual_ports(board):
    uid = board['uid']
    port = [
        hil_util.get_serial_dev(uid, 'TinyUSB', "TinyUSB_Device", 0),
        hil_util.get_serial_dev(uid, 'TinyUSB', "TinyUSB_Device", 2)
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
    port = hil_util.get_serial_dev(uid, 'TinyUSB', "TinyUSB_Device", 0)
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


def link_is_fs(speed) -> bool:
    """Payload scaling from a `speed` attribute. Anything not positively read as high speed
    counts as FS -- including None and SYSFS_UNKNOWN: the FS payload merely tests an HS
    board less, while the HS payload hard-fails a healthy FS board."""
    return speed not in ('480', '5000', '10000')


def dd_timeout(mib: float) -> int:
    """Bound one dd by what was ASKED for: 2.5 s/MiB is the slowest rate this test has
    measured (FS CDC, ~420 kB/s), over a 30 s floor. A flat bound fails a healthy board as
    soon as the payload grows or the leaf-hub uplink is shared."""
    return int(30 + 2.5 * mib)


def test_device_cdc_msc_throughput(board):
    uid = board['uid']

    def parse_speed(dd_output):
        for line in dd_output.splitlines():
            m = re.search(r'([\d.]+)\s+([kMG]?B)/s', line)
            if m:
                return f'{float(m.group(1)):.1f} {m.group(2)}ps'
        return '?'

    dev = get_disk_dev(uid, 'TinyUSB', 0)
    timeout = enum_timeout()
    while timeout > 0:
        if os.path.exists(dev):
            break
        time.sleep(0.1); timeout -= 0.1
    assert timeout > 0, f'Disk {dev} not found'

    tty = hil_util.get_serial_dev(uid, 'TinyUSB', 'Throughput', 0)
    timeout = enum_timeout()
    while timeout > 0:
        if os.path.exists(tty):
            break
        time.sleep(0.1); timeout -= 0.1
    assert timeout > 0, f'CDC tty {tty} not found'

    # Detect speed (12 Mbps FS / 480 Mbps HS) for payload scaling; a device we never find
    # keeps the FS payload (see link_is_fs)
    # usb_scan, not a private glob: it skips root hubs and remembers paths that already
    # stranded, so one wedged peer cannot spend this worker's blindness budget four reads
    # at a time.
    is_fs = True
    speed_known = False
    devs, _ = hil_util.usb_scan(vid='cafe', serial=uid)
    if devs:
        speed = hil_util.read_sysfs(os.path.join(devs[0]['dir'], 'speed'))
        is_fs = link_is_fs(speed)
        speed_known = speed not in (None, hil_util.SYSFS_UNKNOWN)

    # Put tty in raw mode so dd sees pure binary throughput.
    rs = hil_util.run_cmd(f'timeout 30 stty -F {tty} raw -echo')
    assert rs.returncode == 0, f'stty failed: {hil_util.cmd_stdout_text(rs.stdout)}'

    # Payload aim: ~5 s per direction at FS (~830 kB/s), much less at HS.
    msc_count = 2 if is_fs else 16    # bs=1M
    cdc_count = 16 if is_fs else 128  # bs=64K

    tmp_file = f'/tmp/cdc_msc_tp_{uid}.bin'
    t_cdc, t_msc = dd_timeout(cdc_count / 16), dd_timeout(msc_count)

    rw = hil_util.run_cmd(f'timeout {t_cdc} dd if=/dev/zero of={tty} bs=64K count={cdc_count} 2>&1')
    assert rw.returncode == 0, f'CDC dd write failed: {hil_util.cmd_stdout_text(rw.stdout)}'
    cdc_w = parse_speed(hil_util.cmd_stdout_text(rw.stdout))

    rr = hil_util.run_cmd(f'timeout {t_cdc} dd if={tty} of=/dev/null bs=64K count={cdc_count} iflag=fullblock 2>&1')
    assert rr.returncode == 0, f'CDC dd read failed: {hil_util.cmd_stdout_text(rr.stdout)}'
    cdc_r = parse_speed(hil_util.cmd_stdout_text(rr.stdout))

    # inner bound, like the CDC pair above: run_cmd's SIGKILL is merely QUEUED against a
    # dd blocked in the block layer on a half-dead device, so without one the call rides
    # CMD_TIMEOUT and is abandoned holding the disk and usbfs nodes.
    rmr = hil_util.run_cmd(f'timeout {t_msc} dd if={dev} of={tmp_file} bs=1M count={msc_count} iflag=direct 2>&1')
    assert rmr.returncode == 0, f'MSC dd read failed: {hil_util.cmd_stdout_text(rmr.stdout)}'
    msc_r = parse_speed(hil_util.cmd_stdout_text(rmr.stdout))

    rmw = hil_util.run_cmd(f'timeout {t_msc} dd if={tmp_file} of={dev} bs=1M count={msc_count} oflag=direct 2>&1')
    assert rmw.returncode == 0, f'MSC dd write failed: {hil_util.cmd_stdout_text(rmw.stdout)}'
    msc_w = parse_speed(hil_util.cmd_stdout_text(rmw.stdout))

    try:
        os.remove(tmp_file)
    except OSError:
        pass

    print(f'  CDC read {cdc_r} write {cdc_w}, MSC read {msc_r} write {msc_w}  ', end='')

    # report cell, e.g. "✅ C 652/422k M 1.1M/783k" (C=CDC, M=MSC; shared unit shown once)
    def short(s):
        return (s.split()[0].rstrip('0').rstrip('.') + s.split()[-1][0]) if ' ' in s else s

    def pair(r, w):
        r, w = short(r), short(w)
        if r[-1:] == w[-1:] and r[-1:].isalpha():
            r = r[:-1]
        return f'{r}/{w}'

    # 'FS?' when the speed could not be read: the numbers below were produced against the FS
    # payload, so an HS board reads as suspiciously slow. Say so rather than publish a green
    # cell whose scale is a guess.
    scale = '' if speed_known else ' FS?'
    return f'{REPORT_CELL["pass"]} C {pair(cdc_r, cdc_w)} M {pair(msc_r, msc_w)}{scale}'


def test_device_dfu(board):
    uid = board['uid']
    vid_pid = 'cafe:400b'

    # Deadline-based: dfu-util takes ~1 s per call, which a countdown would not charge
    # against the budget. -d pins enumeration to THIS example's ids: a bare `-l` opens every
    # DFU-capable node, and one wedged node blocks that open in D state. The pair is doubled
    # because dfu-util matches run-time and DFU-mode devices against SEPARATE id pairs
    # (parse_vendprod: an omitted DFU-mode pair matches ANY DFU-mode device). The deadline
    # is only tested BETWEEN calls, so the per-call bound is what caps a blocked open.
    deadline = time.monotonic() + enum_timeout()
    found = False
    while time.monotonic() < deadline:
        ret = hil_util.run_cmd(f'dfu-util -d {vid_pid},{vid_pid} -l', timeout=15)
        stdout = hil_util.cmd_stdout_text(ret.stdout)
        if f'serial="{uid}"' in stdout and f'Found DFU: [{vid_pid}]' in stdout:
            found = True
            break
        time.sleep(1)

    assert found, 'Device not available'

    f_dfu0 = f'dfu0_{uid}'
    f_dfu1 = f'dfu1_{uid}'

    try:
        os.remove(f_dfu0)
        os.remove(f_dfu1)
    except OSError:
        pass

    # -d as well as -S: dfu-util matches the SERIAL only after libusb_open() (dfu_util.c
    # probes the descriptor for iSerialNumber), so -S alone still opens every DFU-capable
    # node. The id filter runs BEFORE the open; -S then picks our board (see the poll).
    # Each partition is one short string, so a healthy upload is ~1 s; the bound is there
    # for a node that stops answering mid-transfer.
    ret = hil_util.run_cmd(f'dfu-util -d {vid_pid},{vid_pid} -S {uid} -a 0 -U {f_dfu0}',
                           timeout=30)
    assert ret.returncode == 0, 'Upload failed'

    ret = hil_util.run_cmd(f'dfu-util -d {vid_pid},{vid_pid} -S {uid} -a 1 -U {f_dfu1}',
                           timeout=30)
    assert ret.returncode == 0, 'Upload failed'

    with open(f_dfu0) as f:
        assert 'Hello world from TinyUSB DFU! - Partition 0' in f.read(), 'Wrong uploaded data'

    with open(f_dfu1) as f:
        assert 'Hello world from TinyUSB DFU! - Partition 1' in f.read(), 'Wrong uploaded data'

    os.remove(f_dfu0)
    os.remove(f_dfu1)


def test_device_dfu_runtime(board):
    uid = board['uid']
    vid_pid = 'cafe:400c'
    # enumeration pinned to this example's ids, same per-call bound (see test_device_dfu)
    deadline = time.monotonic() + enum_timeout()
    found = False
    while time.monotonic() < deadline:
        ret = hil_util.run_cmd(f'dfu-util -d {vid_pid},{vid_pid} -l', timeout=15)
        stdout = hil_util.cmd_stdout_text(ret.stdout)
        if f'serial="{uid}"' in stdout and f'Found Runtime: [{vid_pid}]' in stdout:
            found = True
            break
        time.sleep(1)

    assert found, 'Device not available'


def test_device_hid_boot_interface(board):
    uid = board['uid']
    kbd = get_hid_dev(uid, 'TinyUSB', 'TinyUSB_Device', 'event-kbd')
    mouse1 = get_hid_dev(uid, 'TinyUSB', 'TinyUSB_Device', 'if01-event-mouse')
    mouse2 = get_hid_dev(uid, 'TinyUSB', 'TinyUSB_Device', 'if01-mouse')
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
    uid = board['uid']

    cdc_port = hil_util.get_serial_dev(uid, 'TinyUSB', "TinyUSB_Device", 0)
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

    ser.reset_input_buffer()

    # Test 1: Printer -> CDC with multiple sizes, write in random 1-64 byte chunks
    LP_WRITE_TIMEOUT = 5.0  # seconds; firmware may stall draining the printer OUT endpoint
    for size in sizes:
        test_data = rand_ascii(size)
        ser.reset_input_buffer()
        rd = b''
        offset = 0
        # bounded: O_NONBLOCK does NOT save us -- usblp_open() takes the device mutex
        # first -- and this open runs on the worker itself, with no thread to abandon
        lp_fd = hil_util.bounded_open(lp_dev, os.O_WRONLY | os.O_NONBLOCK, 5)
        # Three-valued on purpose: an OSError here is a FACT about the node (EBUSY from
        # usblp's single-opener rule, ENOENT from a re-enumeration race, EACCES from a
        # udev gap) and must not be reported as a wedge -- that sends the operator to
        # usb-kernel-recover for hardware that is fine.
        assert lp_fd is not hil_util.SYSFS_UNKNOWN, (
            f'printer: opening {lp_dev} for write blocked (device wedged)'
            f'{hil_util.sysfs_blind_note()}')
        assert lp_fd is not None, f'printer: {lp_dev} could not be opened for write'
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

    # Test 2: CDC -> Printer with multiple sizes, write in random 1-64 byte chunks.
    # The lp read runs in a PROCESS, not a thread: /dev/usb/lp* blocks on read, usblp
    # allows a SINGLE opener, and a blocked thread cannot be abandoned without keeping
    # that fd -- which poisoned the node for every later test this worker ran. A killed
    # process takes its fd with it.
    ser.reset_input_buffer()
    time.sleep(0.5)
    for size in sizes:
        test_data = rand_ascii(size)

        ready = Path(tempfile.gettempdir()) / f'hil-lp-ready-{os.getpid()}-{size}'
        ready.unlink(missing_ok=True)

        def write_cdc():
            # WAIT for the reader to have the node open. The child has to fork, exec and
            # boot a CPython interpreter; on a loaded rig that routinely exceeds the 0.3s
            # this used to sleep, and every byte sent early is lost -- surfacing as a
            # spurious data mismatch rather than a timeout.
            deadline = time.monotonic() + LP_OPEN_TIMEOUT + 5
            while not ready.exists():
                if time.monotonic() > deadline:
                    return       # reader never opened; the rc/compare below reports it
                time.sleep(0.02)
            offset = 0
            while offset < size:
                chunk_size = min(random.randint(1, 64), size - offset)
                serial_write_all(ser, test_data[offset:offset + chunk_size])
                time.sleep(0.01)
                offset += chunk_size

        try:
            r = hil_util.run_alongside(
                [sys.executable, '-c', LP_READER, lp_dev, str(size), str(ready)],
                write_cdc, LP_OPEN_TIMEOUT + 12)
        finally:
            ready.unlink(missing_ok=True)
        # stderr, not stdout: run_alongside keeps the payload stream clean, so a traceback
        # from the reader now arrives on its own pipe
        assert r.returncode == 0, (f'CDC->Printer reader failed ({size} bytes, rc '
                                   f'{r.returncode}): {hil_util.cmd_stdout_text(r.stderr)[:200]}')
        assert r.stdout == test_data, (f'CDC->Printer wrong data ({size} bytes):\n'
                                       f'  expected: {test_data[:64]}\n  received: {r.stdout[:64]}')
        time.sleep(0.2)

    ser.close()


def test_device_mtp(board):
    # The whole session lives in mtp_test.py under run_cmd: libmtp calls are synchronous
    # ctypes that block unkillably (D state) on a wedged device, so a disposable process is
    # the only thing the harness can walk away from.
    uid = board['uid']
    script = Path(__file__).resolve().parent / 'mtp_test.py'
    # 2x, as master's in-process open_mtp_dev used: libmtp-runtime publishes
    # /dev/libmtp-* only after its SYNCHRONOUS mtp-probe finishes, seconds on a freshly
    # flashed FS board, and the gio unmount eats part of what is left before the first
    # probe. Extracting the session into a subprocess halved this by accident (8s/4s),
    # which fails healthy hardware on the retry.
    t = 2 * enum_timeout()
    r = hil_util.run_cmd(
        f'{shlex.quote(sys.executable)} {shlex.quote(str(script))} --uid {shlex.quote(uid)} --timeout {t}',
        timeout=t + MTP_SESSION_MARGIN)
    if r.returncode == 124:
        # "abandoned", not "killed": a session blocked in a usbfs ioctl (D state) never
        # receives the SIGKILL -- it lingers until its device path clears, by design
        raise AssertionError(f'MTP session wedged (abandoned after {t + MTP_SESSION_MARGIN}s; '
                             f'the session process may linger unkillable in D state)')
    assert r.returncode == 0, f'MTP session failed (rc {r.returncode}):\n{r.stdout}'


def test_device_net_lwip_webserver(board):
    # MAC hard-coded in examples/device/net_lwip_webserver/src/main.c; Linux names the
    # iface enx<MAC_lowercase_no_colons>. Device IP 192.168.7.1, iperf2 TCP server on 5001
    # (INCLUDE_IPERF).
    import socket
    mac_no_colons = '0202846a9600'
    iface = 'enx' + mac_no_colons
    device_ip = '192.168.7.1'
    iperf_port = 5001

    # Wait for an IPv4 address in the device's subnet (it serves DHCP); 30s because USB
    # enum + DHCP serve is slower on the CI HIL hardware than locally.
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

    # Poll until the device accepts: the net stack and the iperf bind come up after DHCP.
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

    # 5-second iperf2 TCP test; -y C for stable parsing (final summary line is
    # timestamp,src_ip,src_port,dst_ip,dst_port,id,interval,bytes,bps).
    ret = hil_util.run_cmd(f'iperf -c {device_ip} -t 5 -y C',
                           timeout=30, split_stderr=True, quiet=True)
    stderr = (ret.stderr or '').strip()
    stdout = (ret.stdout or '').strip()
    assert ret.returncode == 0, f'iperf rc={ret.returncode}: stderr={stderr!r} stdout={stdout!r}'
    lines = [l for l in stdout.splitlines() if l]
    assert lines, f'iperf produced no output (rc={ret.returncode}, stderr={stderr!r})'
    try:
        bps = int(lines[-1].split(',')[-1])
    except (ValueError, IndexError) as e:
        raise AssertionError(f'could not parse iperf output: {lines[-1]!r} ({e})')
    mbps = bps / 1e6
    print(f'  iperf {mbps:5.1f} Mbps', end='')

    assert mbps >= 1.0, f'iperf throughput too low: {mbps:.2f} Mbps'


def test_device_msc_dual_lun(board):
    uid = board['uid']

    data0 = read_disk_file(uid, 0, 'README0.TXT')
    readme0 = b"LUN0: " + MSC_README_TXT
    assert data0 == readme0, f'MSC LUN0 wrong data in README0.TXT\n  expected: {readme0}\n  received: {data0}'

    data1 = read_disk_file(uid, 1, 'README1.TXT')
    readme1 = b"LUN1: " + MSC_README_TXT
    assert data1 == readme1, f'MSC LUN1 wrong data in README1.TXT\n  expected: {readme1}\n  received: {data1}'


def test_device_midi_test(board):
    uid = board['uid']

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

    import select
    midi_fd = os.open(midi_port, os.O_RDONLY | os.O_NONBLOCK)
    try:
        data = bytearray()
        # Read for up to 3 seconds to capture a few notes (286ms interval)
        end_time = time.monotonic() + 3
        while (remaining := end_time - time.monotonic()) > 0:
            ready, _, _ = select.select([midi_fd], [], [], min(0.5, remaining))
            if not ready:
                continue
            try:
                chunk = os.read(midi_fd, 64)
            except BlockingIOError:
                continue
            if not chunk:
                break
            data.extend(chunk)
    finally:
        os.close(midi_fd)

    notes = []
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

    # run_cmd: ALSA capture from a wedged device blocks in D state (see read_disk_file)
    ret = hil_util.run_cmd(' '.join(shlex.quote(c) for c in cmd),
                           timeout=20, split_stderr=True, quiet=True)
    assert ret.returncode == 0, \
        f'arecord failed: {(ret.stderr or "").strip() or (ret.stdout or "").strip()}'

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

    # The producer is already running while ALSA activates streaming, so the
    # initial overwritable software FIFO (at most 224 samples) can transition
    # between ramp generations. After that startup window, require an exact ramp.
    startup_samples = 256
    for i in range(startup_samples, sample_count - 1):
        expected = (samples[i] + 1) & 0xFFFF
        assert samples[i + 1] == expected, (
            f'Audio mismatch at sample {i + 1}: expected {expected}, got {samples[i + 1]}')

    print(f'  ALSA {pcm}', end='')


def test_device_hid_generic_inout(board):
    uid = board['uid']
    import hid  # cython-hidapi (pip: hidapi, apt: python3-hid)

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
    global board_wedged
    # Runs test/hil/usbtest.py against the cafe:4010 device; the pass count goes in the
    # report cell ("✅ 30/30", or "❌ 29/30" on a partial).
    uid = board['uid']

    def usbtest_enumerated():
        """True, False, or None when a bounded read did not answer -- absence unproven."""
        # vid_pid FIRST: right after flashing, the previous example's enumeration (same
        # serial, different PID) can linger and would fail usbtest.py's lookup -- and
        # filtering on the two lock-free descriptor fields rules out every other device
        # on the bus before the one read that can block. usb_scan memoises paths that
        # already stranded, so one wedged peer cannot spend the blindness budget here.
        devs, unknown = hil_util.usb_scan(vid_pid=('cafe', '4010'), serial=uid)
        if devs:
            return True
        return None if unknown else False

    end = time.monotonic() + enum_timeout()
    seen = usbtest_enumerated()
    while time.monotonic() < end and seen is not True:
        time.sleep(0.2)
        seen = usbtest_enumerated()
    # fail before usbtest_permit: an absent device would otherwise queue on the battery
    # mutex for minutes behind real batteries just to have usbtest.py report "no device"
    if seen is not True:
        # 0/30 rather than a bare cell: the battery never ran (30 = standard case count)
        raise TestFail(
            f'no cafe:4010 device with serial {uid}' if seen is False else
            f'cannot tell whether cafe:4010 {uid} is present: the bounded sysfs reads did '
            f'not answer{hil_util.sysfs_blind_note()}',
            metric=f'{REPORT_CELL["fail"]} 0/30')
    # settle: right after flashing the enumeration can bounce once (and on dual-port parts
    # the other port's stale node — same serial and PID — lingers), and testusb run into
    # that gap sees the device drop mid-case
    time.sleep(3)

    # --keep-binding is required for concurrent batteries: usbtest.py's cleanup unbinds
    # EVERY usbtest-bound interface, killing a peer battery under USBTEST_PARALLEL > 1, and
    # that unbind path has also wedged a host xHCI (usb_hcd_alloc_bandwidth) here. Harmless
    # to leave: the next example enumerates under a different PID.
    script = Path(__file__).resolve().parent / 'usbtest.py'
    # --budget makes the battery a real bound: repeated case timeouts (a FAIL, not a HUNG,
    # so the battery keeps going) can otherwise spend the whole outer timeout inside the
    # case loop, leaving the recovery below nothing.
    cmd = (f'{shlex.quote(sys.executable)} {shlex.quote(str(script))} '
           f'--serial {shlex.quote(uid)} --json --keep-binding '
           f'--timeout 60 --budget {USBTEST_BATTERY_BUDGET}')
    # Post-hang recovery reflashes the DUT through its own probe, NEVER a root-port cycle
    # (one board reached instead of every fixture under the port; see usb-kernel-recover).
    # _current_fw is the artifact test_example flashed for THIS test: re-deriving it from
    # board['name'] reflashes the wrong build on variant-only boards. --outer-timeout lets
    # usbtest skip a reflash it cannot finish before our run_cmd kill, which would orphan
    # the flasher (own session) on the probe. Never under --skip-flash -- and say so: a
    # HUNG case then holds the DUT's usbfs lock for the rest of the run, and a probe reset
    # is no substitute (the DWC2 pullup survives a core halt).
    # ...and only when this flasher can DELIVER that reflash past a poisoned node
    # (hil_flash.convoy_safe). Otherwise the flags cost twice: the delivery adds a SECOND
    # stray, and the board reserves recovery budget for a path that cannot fire.
    # The RECOVERY flasher, which may be the roster's optional `flasher_recover` rather
    # than the primary -- a jlink/stlink board can name an openocd entry that reaches the
    # same probe convoy-safely without changing how the board is normally flashed.
    _rec_flasher = hil_flash.recover_flasher(board)
    recovery = bool(_current_fw and not skip_flash and hil_flash.convoy_safe(_rec_flasher))
    # ONE bound, computed here and used for BOTH the child's --outer-timeout and our own
    # run_cmd kill below. Three separate expressions disagreed: --skip-flash appended no
    # --outer-timeout at all (usbtest reads 0 as "no limit"), and the no-recovery branch
    # narrowed only the CHILD's view while run_cmd still waited the full reserve -- so a
    # board that cannot recover held a pool worker AND its battery permit idle for
    # USBTEST_RECOVERY_BUDGET it had no way to spend, under a usbtest width of 2.
    outer = USBTEST_BATTERY_BUDGET + (USBTEST_RECOVERY_BUDGET if recovery
                                      else USBTEST_OVERSHOOT)
    if _current_fw and skip_flash:
        print('note: --skip-flash disables usbtest hang recovery; a HUNG case will leave '
              'the device wedged until it is reflashed', flush=True)
    elif _current_fw and not recovery:
        print(f'note: {_rec_flasher["name"]} cannot deliver a reflash past a poisoned '
              f'usbfs node, so usbtest hang recovery is disabled for {board["name"]}; a '
              f'HUNG case will leave it wedged for the rest of the run', flush=True)
    if recovery:
        # ship the RECOVERY flasher as `flasher`: usbtest.py, recovery_steps and
        # convoy_safe all read board['flasher'], so substituting here keeps the entire
        # child side unaware that a second roster entry exists
        rb = json.dumps({'name': board['name'], 'flasher': _rec_flasher})
        cmd += f' --recover-board {shlex.quote(rb)} --recover-fw {shlex.quote(_current_fw)}'
    cmd += f' --outer-timeout {outer}'
    # The reserve above USBTEST_BATTERY_BUDGET exists because the battery can overrun by
    # one already-started case, and a hang there needs room for the recovery (whose reflash
    # is bounded by usbtest.RECOVER_FLASH_TIMEOUT, not HIL_CMD_TIMEOUT). Without it run_cmd
    # SIGKILLs usbtest.py mid-recovery, losing the JSON and the diagnosis.
    with hil_lock.usbtest_permit(uid):
        # split_stderr: the battery's final JSON is parsed from stdout, and stderr is the
        # only detail left when the outer timeout kills the battery before it prints
        r = hil_util.run_cmd(cmd, timeout=outer, split_stderr=True)
    out = hil_util.cmd_stdout_text(r.stdout)
    brace = out.find('{')
    try:
        # brace < 0 would slice from the END ('...rc 0' -> '0' -> int 0, whose subscript
        # raises TypeError outside the tuple below and loses the diagnosis)
        if brace < 0:
            raise ValueError('no JSON object on stdout')
        data = json.loads(out[brace:])
        passed, failed = int(data['passed']), int(data['failed'])
    except (ValueError, KeyError, TypeError, json.JSONDecodeError):
        # compact BOTH, never `or`: a battery SIGKILLed mid-print leaves a truthy JSON
        # fragment on stdout, so an `or` drops the stderr that explains the failure
        parts = [compact_output(hil_util.cmd_stdout_text(r.stderr)), compact_output(out)]
        detail = ' | '.join(p for p in parts if p)
        # Retryable even on rc 124 (run_cmd's outer kill), though the retry re-pays the
        # whole budget: 124 only says the timer expired, which a healthy battery can hit
        # under load, and test_example REFLASHES before each attempt. Where usbtest's
        # in-band recovery is off (--skip-flash, a flasher failing convoy_safe, a terminal
        # wedge) that reflash is the only thing left to unpoison the DUT for the boards
        # that share its controller.
        # No JSON to read the verdict from, so fall back to the text: a battery SIGKILLed
        # mid-hang still says HUNG on stdout, and this raise happens BEFORE the latch below
        # -- which is why the outer-timeout case, the likeliest real wedge, never latched.
        if 'HUNG' in out:
            board_wedged = (f'{board["name"]}: usbtest reported a hang and was killed '
                            f'before it could report a verdict')
        raise TestFail(f'usbtest did not run: {detail}',
                       metric=f'{REPORT_CELL["fail"]} 0/30')

    # A HUNG case that recovery could not clear leaves a D-state holder on this board's
    # usbfs node. Latch it: the remaining examples would each flash THROUGH that node,
    # block, survive SIGKILL and add another stray -- turning one wedge into one stray per
    # remaining example, which is the convoy this branch exists to contain.
    # The battery's OWN verdict first: `recovery` only says the flags were passed, not that
    # the reflash worked, so a convoy-safe board whose recovery failed used to come back
    # unlatched and flash every remaining example through the poisoned node.
    if data.get('wedged') or (not recovery and 'HUNG' in out):
        # _rec_flasher, NOT board['flasher']: recovery was decided against recover_flasher()
        # at the top of this function, and the two diverge as soon as a roster carries the
        # optional `flasher_recover` key -- naming the wrong one sends the operator to the
        # wrong probe. The wording stays on what usbtest actually reported ("still wedged"),
        # because unrecovered_hang is also set by the ambiguous/inconclusive aborts, where
        # nothing hung and the old text was false on both clauses.
        board_wedged = (f'{board["name"]}: usbtest reports the device still wedged '
                        + (f'after a recovery reflash via {_rec_flasher["name"]}' if recovery
                           else f'and {_rec_flasher["name"]} cannot deliver a recovery reflash'))

    # notrun counts toward the denominator but is NOT a failure: listing cases that never
    # ran as failures sends a maintainer bisecting one of them.
    notrun = int(data.get('notrun', 0))
    total = passed + failed + notrun
    if board_wedged and failed == 0 and notrun == 0:
        # Every case passed and the device STILL wedged -- usbtest's inconclusive/ambiguous
        # abort fires after the last case, so nothing back-fills a BUDGET entry. Reporting
        # the pass would exit 0 with a D-state holder on the rig and the board absent from
        # the re-run spec. parsed=True: a retry re-pays the whole battery to re-observe a
        # wedge, and flashes through the poisoned node to do it.
        raise TestFail(f'usbtest {passed}/{total} but the device wedged ({board_wedged})',
                       metric=f'{REPORT_CELL["fail"]} {passed}/{total}', parsed=True)
    if failed == 0 and notrun == 0 and total > 0:
        return f'{REPORT_CELL["pass"]} {passed}/{total}'
    bad = [c.get('num') for c in data.get('cases', [])
           if c.get('status') not in ('PASS', 'BUDGET')]
    why = f'usbtest {passed}/{total}'
    if bad:
        why += f' (cases failed: {bad})'
    if notrun:
        # the reason is per BUDGET entry: a hang or a device drop also aborts the battery,
        # and blaming the budget points the maintainer at the wrong thing
        reasons = {c.get('detail', '') for c in data.get('cases', [])
                   if c.get('status') == 'BUDGET'}
        reason = (reasons.pop().replace('not run: ', '') if len(reasons) == 1
                  else 'the battery stopped early')
        why += f'; {notrun} case(s) never ran ({reason}), so this says nothing about them'
    # parsed ONLY when every case ran: an aborted battery (budget expiry, kernel hang, bus
    # drop) leaves BUDGET entries, and those are exactly what a reflash retry can fix.
    raise TestFail(why, metric=f'{REPORT_CELL["fail"]} {passed}/{total}',
                   parsed=(notrun == 0))


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

    # --skip-flash runs whatever is already on the board, so any build counts as present;
    # filtering by flasher there would skip the test over an extension it never uses.
    fw_name = hil_flash.find_firmware(variant, example,
                                      flasher=None if skip_flash else board['flasher']['name'])
    if fw_name is None:
        log_line(f'{test_name} Skip (no binary)')
        return 0, 'skip', None
    # usbtest's hang recovery reflashes the exact artifact under test; re-deriving it from
    # board['name'] breaks on variant-only boards
    global _current_fw
    _current_fw = str(fw_name)

    if verbose:
        log_line(f'Firmware {fw_name}')

    global _enum_timeout
    start_s = time.time()
    flash_ok = True
    last_err = ''
    last_detail = ''
    wedge_break = False
    for i in range(max_retry):
        if board_wedged and i:
            # The latch is set MID-attempt (a HUNG usbtest whose flasher cannot recover),
            # so test_board's check between tests is too late for THIS test's own retries:
            # every further attempt re-flashes into the D-state-held node, blocks, survives
            # SIGKILL and leaves another stray. The wedge is not something a retry can fix.
            log_line(f'{test_name}  not retrying: {board_wedged}')
            # COUNT it. Breaking out here skips the i == max_retry - 1 branch that would
            # have incremented err_count, so the board rendered a red cell, contributed 0
            # to the exit status and was omitted from the re-run spec -- a rig left with a
            # D-state holder published under sys.exit(0). Latent at CI's --retry 1, live
            # for every local run and for the workflows that pass no -r.
            wedge_break = True
            break
        _enum_timeout = ENUM_TIMEOUT if i == 0 else ENUM_TIMEOUT_RETRY
        attempt_out = io.StringIO()
        with redirect_stdout(attempt_out):
            if not skip_flash:
                with hil_lock.flash_permit(board['uid']):
                    t_flash = time.monotonic()
                    try:
                        ret = getattr(hil_flash,
                                      f'flash_{board["flasher"]["name"].lower()}')(board, str(fw_name))
                    except Exception as e:
                        # A flasher that RAISES (esptool's get_serial_dev when the adapter
                        # drops off the bus, a missing config.env, an unwritable CWD) would
                        # propagate out of the worker and abort the whole drain, costing
                        # every board still in flight.
                        print(f'flash raised: {type(e).__name__}: {e}', flush=True)
                        ret = subprocess.CompletedProcess(args='flash', returncode=1,
                                                          stdout=f'{type(e).__name__}: {e}')
                    if PROFILE:
                        log_line(f'[prof] {variant} {example} flash attempt {i + 1}: '
                                 f'{time.monotonic() - t_flash:.1f}s rc={ret.returncode}')
                    flash_ok = (ret.returncode == 0)
                    # A wedged RP2040/RP2350 DAP answers nothing and the probe has no
                    # reset line, so the retry fails identically; POR it via the Rescue DP
                    # first (no-op otherwise). NOT gated on a remaining attempt: CI HIL jobs
                    # run --retry 1, and this leaves the DAP POR'd for the jobs that follow.
                    if not flash_ok and \
                            hil_flash.rescue_openocd(board, hil_util.cmd_stdout_text(ret.stdout)):
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
                    if getattr(e, 'parsed', False):
                        # a PARSED per-case result (usbtest's "29/30"): retrying re-pays
                        # the whole battery, inside the fleet's usbtest permit, to
                        # re-observe a number the JSON already reported. Only that case.
                        err_count += 1
                        metric = getattr(e, 'metric', None)
                        msg = f'{test_name}  {STATUS_FAILED}: {e}'
                        if last_detail:
                            msg += f'  {last_detail}'
                        msg += f'  in {time.time() - start_s:.1f}s'
                        log_line(msg)
                        break
                    if i == max_retry - 1:
                        err_count += 1
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

    if wedge_break and not err_count:
        # ONE error for the test, never two: a board that also failed to flash has already
        # been counted just above. Without this the test returns 0 -- red cell, clean exit
        # status, absent from the re-run spec.
        err_count += 1
    return err_count, result_status, metric


def build_board(board: Board) -> tuple[str, int]:
    """Build firmware for this board via tools/build.py.
    Honors board config's variant list (name, defines, flags).
    Output goes to cmake-build/cmake-build-<variant>/ (tools/build.py layout).

    Unbounded on purpose: --build is a local convenience (no CI workflow passes it), so
    the developer watching the build is the timeout."""
    name = board['name']
    variants = board.get('variant') or [{'name': name, 'flags': ''}]

    failed = 0
    for v in variants:
        cmd = [sys.executable, str(hil_util.TINYUSB_ROOT / 'tools' / 'build.py'), '-b', name]
        if v['name'] != name:
            cmd += ['--build-name', v['name']]
        for d in v.get('defines', []):
            cmd += ['-D', d]
        for tok in v.get('flags', '').split():
            cmd += [f'--cflag={tok}']
        if verbose:
            cmd.append('-v')
            print(f'  + {" ".join(cmd)}')
        # stdio is inherited so the build STREAMS: a silent buffer is
        # indistinguishable from a stall.
        proc = subprocess.Popen(cmd, cwd=hil_util.TINYUSB_ROOT, start_new_session=True)
        try:
            rc = proc.wait()
        except KeyboardInterrupt:
            # start_new_session means the build never saw the terminal's SIGINT
            try:
                os.killpg(proc.pid, signal.SIGKILL)
            except OSError:
                proc.kill()
            raise
        if rc != 0:
            failed += 1
    return name, failed


# pseudo-test column for a variant boundary the park-flash could not clear (see below)
BOUNDARY_CELL = 'same-PID boundary'


def test_board(board: Board) -> tuple[str, int, list[str], list, float]:
    swept = False
    name = board['name']
    flasher = board['flasher']

    global board_wedged
    board_wedged = ''
    try:
        _lock_fh = hil_lock.acquire_board_lock(name)
    except RuntimeError as e:
        log_line(f'{name:25} {STATUS_FAILED}: {e}')
        # visible report row so the ❌ matches the exit code; failed-tests stays empty so a
        # re-run repeats the whole board (no bogus -bt filter)
        return name, 1, [], [(name, {'board-locked': 'fail'}, None)], 0.0
    # after the lock: flock wait behind a concurrent run is not board cost
    t_board = time.monotonic()
    try:
        test_list = []

        if name in board_test:
            test_list = board_test[name]
        elif len(test_only) > 0:
            # Explicit -t: filter against the board's capabilities, or a device-only board
            # runs host/dual tests whose `dev_attached` config entry does not exist.
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
        rows = []  # list of (row_label, {example: status}, duration) — one per build variant
        # a -t/-bt filtered run times only a subset; report no duration so an accumulate
        # re-run keeps the previous full-run value
        partial = bool(test_only) or name in board_test
        variants = board.get('variant') or [{'name': name, 'flags': ''}]

        prev_last = None  # last test of the previous variant: the variant boundary is an adjacency too
        for v in variants:
            vname = v['name']
            # Shuffle each (board, variant)'s run order: spreads batteries and flash churn
            # across the timeline instead of convoying, and surfaces order-dependent bugs.
            # Seeded for replay (HIL_SHUFFLE_SEED). Unique per-example PIDs re-enumerate
            # between examples; only the variant boundary can repeat one.
            run_list = list(test_list)
            if shuffle_seed is not None and len(run_list) > 1:
                random.Random(f'{shuffle_seed}:{name}:{vname}').shuffle(run_list)
                if run_list[0] == prev_last:
                    run_list[0], run_list[-1] = run_list[-1], run_list[0]
            cells = {}
            if run_list and run_list[0] == prev_last and not skip_flash:
                # Same example (same PID) still repeats across the boundary (a one-test
                # -bt run has nothing to swap with). Park on board_test first: it disables
                # the board's USB, so the next flash must re-enumerate to be seen.
                t_park = time.monotonic()
                # _should_park, same as the teardown park: this is attempt 0, so
                # test_example's retry guard does not stop it flashing into a poisoned node
                park_ec, park_status, _ = (
                    test_example(board, vname, 'device/board_test') if _should_park(skip_flash)
                    else (0, 'skip', None))
                if park_ec or park_status == 'skip':
                    # Boundary not cleared: the previous variant may still be enumerated
                    # under the same PID, so this variant's tests could pass against ITS
                    # firmware. Skip them and record the boundary as the failure, so the
                    # report matches the exit code instead of rendering all-green.
                    # A 'skip' here has two very different causes: no board_test build, or
                    # _should_park refusing to flash a WEDGED board. Reporting the latter as
                    # a missing binary sends the operator hunting a build that exists.
                    wedge_skip = park_status == 'skip' and bool(board_wedged)
                    why = ('the board is wedged' if wedge_skip else
                           'no board_test binary' if park_status == 'skip' else
                           'park flash failed')
                    log_line(f'{vname:40} {"same-PID boundary":30} {STATUS_FAILED}: not cleared ({why}); '
                             f'skipping {len(run_list)} test(s) on this variant')
                    # the wedge already charged its own error through test_device_usbtest;
                    # charging again would double-count one incident in the exit code
                    if not wedge_skip:
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
                if board_wedged:
                    # Do NOT flash through a poisoned node: each attempt enumerates into
                    # it, blocks uninterruptibly and leaves another stray behind. Report
                    # the skip so the cell is not mistaken for a pass.
                    cells[test] = f'{REPORT_CELL["skip"]} board wedged'
                    # ...and re-run the WHOLE board, like the boundary-failure path above:
                    # these tests never executed, so naming them individually in the .failed
                    # spec is not enough -- an --accumulate re-run that fixes only the wedged
                    # test would merge a green cell over it and leave these skips standing
                    # from the earlier attempt, forever, under a green job.
                    board_wide_fail = True
                    continue
                ec, status, metric = test_example(board, vname, test)
                err_count += ec
                cells[test] = metric if metric else status
                if ec > 0:
                    failed_tests.append(test)
            if board_wedged:
                log_line(f'{vname:40} SKIPPING the rest of this board: {board_wedged}; '
                         f'flashing through the poisoned node would add a stray per test')
            dur = f'{time.monotonic() - t_variant:.0f}s' if run_list and not partial else None
            rows.append((vname, cells, dur))

        # excludes the teardown park-flash below; a partial (filtered) run reports 0.0 so
        # it never overwrites a cached full-run duration
        t_total = 0.0 if partial else time.monotonic() - t_board

        # park: flash board_test last to disable the board's usb; teardown, not a test,
        # so it is not recorded in the report.
        #
        # NOT on a wedged board: the latch has just skipped every remaining test precisely
        # because flashing through a D-state-held node blocks, survives SIGKILL and leaves
        # a stray -- and this park is a flash like any other. test_example's own guard does
        # not stop it (that one only suppresses RETRIES, and this is attempt 0), so the
        # containment path would add the very stray it exists to prevent.
        if _should_park(skip_flash):
            test_example(board, variants[0]['name'], 'device/board_test')

        # Sweep HERE, not in main()'s finally: maxtasksperchild=1 retires this process as
        # soon as it returns, reparenting anything it spawned to init and off the pool's
        # ppid tree, so the main-side sweep walks fresh idle workers and finds nothing.
        # Measured: 4 tasks, zero overlap, sweep 0, all 4 strays alive.
        stray = hil_health.kill_own_children()
        swept = True

        # LAST fields: whether this worker ran out of bounded-read budget, and what it could
        # not kill. Only the worker can answer either -- the blindness latch is
        # process-global and this is a separate process -- and the result tuple already
        # crosses back, so no Manager round-trip.
        return (name, err_count, [] if board_wide_fail else sorted(set(failed_tests)),
                rows, t_total, hil_util.sysfs_blind(), stray)
    finally:
        # A raise skips the sweep above, and maxtasksperchild=1 retires this process
        # immediately afterwards -- reparenting its flasher to init and erasing the ppid
        # link, so main's sweep cannot see it either. The count cannot reach the report on
        # this path (there is no result tuple), but the KILL still frees the probe.
        if not swept:
            try:
                hil_health.kill_own_children()
            except Exception as se:   # noqa: BLE001 - never mask the original failure
                print(f'warning: stray sweep failed: {type(se).__name__}: {se}', flush=True)
        if _lock_fh:
            try:
                # clear our pid record before dropping the flock: this worker process
                # lives on (pool reuse), so a stale record would make hil_lock's
                # pid-liveness checks report a freed board as locked for the rest of the run
                _lock_fh.truncate(0)
            except OSError:
                pass
            _lock_fh.close()


REPORT_MD = 'hil_report.md'
REPORT_JSON = 'hil_report.json'
# controller hints from previous runs: uid -> {'name', 'pci', 'duration'}. Only 'pci' is
# consumed (dispatch order and first-flash budgeting, never battery serialization). PCI
# addresses are boot-stable, so the cache survives reboots and goes stale on re-cabling.
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

    # metric-bearing columns pinned first, the rest alphabetical: stable regardless of the
    # shuffled execution order
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

    # tally run cells (not-run cells are absent from the dicts). A cell is a bare status or
    # a metric string carrying its own icon ("❌ 29/30"), so classify by the leading icon.
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


def _write_failed_spec(failed_fname: Path, report_dir: Path, mret: list) -> None:
    """Re-run spec: only the failed boards (-b), each restricted to its own failed tests
    (-bt); a board with failures but no test list re-runs entirely.

    Shared with the pool-guard path, which feeds it the boards that never reported. That
    path used to leave this unwritten -- and a fresh run has already unlinked it -- so
    build.yml's "Get re-run spec" step found nothing and the GitHub re-run repeated the
    whole fleet to find the one board that wedged."""
    parts = ['--accumulate']
    for name, err, fts, *_ in mret:
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


class PoolDrainTimeout(MpTimeoutError):
    """Guard expiry, carrying the rows that DID finish.

    They ride on the exception because the raise is the containment path: losing them here
    is what map_async did, and what the drain exists to stop.
    """

    def __init__(self, finished: list):
        super().__init__()
        self.finished = finished


def drain_pool(it, boards: list, deadline: float, out: list | None = None) -> list:
    """Collect imap_unordered results against ONE deadline. Returns the finished rows.

    Raises PoolDrainTimeout (carrying those same rows) when the deadline passes with boards
    still in flight -- the caller keeps them, names only what is missing, and writes a
    re-run spec covering just those.

    A function, not an inline loop, so the tests can call THIS instead of a copy of it: the
    loop's previous test built its own ThreadPool and its own drain and asserted on those,
    so deleting the real one outright kept the suite green.
    """
    # `out` is the CALLER's list: a worker that raises something other than a timeout
    # (get_serial_dev on a dropped adapter, a Manager EOFError) propagates bare, and a
    # local accumulator would take every finished board with it -- the exact loss the
    # drain replaced map_async to prevent.
    mret: list = out if out is not None else []
    for _ in boards:
        left = deadline - time.monotonic()
        if left <= 0:
            raise PoolDrainTimeout(mret)
        try:
            mret.append(it.next(timeout=left))
        except MpTimeoutError:
            raise PoolDrainTimeout(mret) from None
    return mret


def _should_park(skip_flash: bool) -> bool:
    """Flash the teardown park (device/board_test, to switch the DUT's USB off)?

    Not on a wedged board. The latch has just skipped every remaining test precisely
    because flashing through a D-state-held node blocks, survives SIGKILL and leaves a
    stray -- and the park is a flash like any other. test_example's own guard does not stop
    it either: that one only suppresses RETRIES, and the park is always attempt 0. So the
    containment path would end by adding the very stray it exists to prevent.
    """
    return not skip_flash and not board_wedged


def _stray_note(mret: list) -> str:
    """Name the strays the workers could not kill, for the report banner.

    Summed from the result tuples rather than computed in main()'s finally: that finally
    runs AFTER accumulate_report on both abort paths, so a banner appended there was
    written to a variable nobody read again.
    """
    dirty = [(r[0], r[6]) for r in mret if len(r) > 6 and r[6]]
    if not dirty:
        return ''
    total = sum(n for _, n in dirty)
    return (f'> **Rig dirty.** {total} process(es) survived SIGKILL and still hold a probe '
            f'or usbfs node into the next job: '
            f'{", ".join(f"{b} ({n})" for b, n in dirty)}.\n')


def _blind_note(mret: list) -> str:
    """Name the boards whose worker went blind, for the report banner.

    A blind worker answers SYSFS_UNKNOWN for every attribute, so its "device not found" is
    "could not tell". That already reaches the log and the per-cell failure text, but the
    TABLE is what gets quoted -- and a red cell there is read as a broken board. Seen live
    (run 31794359407): four workers blind, several cells red because of it, and a report
    that said nothing.

    Per-board, not global: maxtasksperchild=1 gives every board a fresh worker, so a board
    that ran on a healthy one is not smeared by a neighbour's wedge. Rows synthesised by
    the timeout path are 5 fields wide and have nothing to report.
    """
    blind = [r[0] for r in mret if len(r) > 5 and r[5]]
    if not blind:
        return ''
    return (f'> **Not all verdicts are evidence.** {len(blind)} board(s) ran on a worker '
            f'that went blind on sysfs -- too many bounded reads stranded on a wedged '
            f'device -- so "not found" from them means "could not tell": '
            f'{", ".join(blind)}. See the usb-kernel-recover skill.\n')


def accumulate_report(mret: list, report_dir: Path, fresh: bool, scope: str = '',
                      banner: str = '') -> str:
    """Merge this run's results into hil_report.json in report_dir, then (re)write
    the markdown matrix to hil_report.md. `fresh` (a first run, no --accumulate)
    starts a new report; otherwise a re-run accumulates so boards/tests that
    already passed are preserved while re-run cells are updated. `scope` names the
    board filter, if any, so a scoped table is not mistaken for a full one.
    Returns the md."""
    acc = {}  # ordered {row_label: [cells dict, duration str|None]}
    prior_banner = ''
    jpath = report_dir / REPORT_JSON
    if not fresh and jpath.is_file():
        try:
            saved = json.loads(jpath.read_text())
            # CI keys the report dir by run id, so the sidecar is from an earlier attempt
            for entry in saved.get('rows', []):
                acc[entry['board']] = [dict(entry['cells']), entry.get('duration')]
            # ... and so is the caveat those cells were collected under. A rerun on a rig
            # that has since recovered contributes no banner, and the .failed spec reruns
            # only FAILURES -- so the earlier attempt's passes are never re-earned and
            # would be published as clean results of a rig that was not.
            prior_banner = saved.get('banner', '')
        except (ValueError, KeyError, TypeError):
            pass  # corrupt/old sidecar: start fresh

    # current cells override prior for boards/tests that ran; a filtered run reports
    # duration None, keeping the previous full-run value
    for name, _, _, rows, *_ in mret:
        if rows and not any('board-locked' in cells for _, cells, _ in rows):
            # board ran for real: clear a stale lock-failure cell (its row is keyed by
            # board name; test rows may be variant names)
            stale = acc.get(name)
            if stale is not None:
                stale[0].pop('board-locked', None)
                if not stale[0]:
                    # variant-keyed boards never repopulate the board-name row, so drop it
                    # or it renders as a blank ghost row
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
    # by LINE, deduped: attempts repeat the same caveat far more often than they add a new
    # one, and three copies of the D-state note reads as three incidents
    seen, merged = set(), []
    for line in (prior_banner + banner).splitlines():
        if line.strip() and line not in seen:
            seen.add(line)
            merged.append(line)
    banner = '\n'.join(merged) + '\n' if merged else ''
    jpath.write_text(json.dumps({'rows': [{'board': k, 'cells': c, 'duration': d}
                                          for k, (c, d) in acc.items()],
                                 'banner': banner}, indent=2) + '\n')

    md = render_matrix([(k, c, d) for k, (c, d) in acc.items()])
    if scope:
        # a scoped run's small table is otherwise indistinguishable from a full one, and
        # it replaces the previous full table in the sticky PR comment
        md = f'_Scoped run: {scope}. Boards/tests not listed were not run._\n\n' + md
    # LAST, so it is outermost: a rig-health caveat outranks the table AND the scope note,
    # and the top of the report is where hil/SKILL.md tells the agent to look for it.
    if banner:
        md = banner + '\n' + md
    (report_dir / REPORT_MD).write_text(md + '\n', encoding='utf-8')
    return md


# containment paths print through hil_health._p: stdout may already be a dead pipe (a
# dropped ssh session), and a BrokenPipeError there would skip os._exit
_p = hil_health._p


def _abandon_exit(pool, mgr, abandoned: bool, err_count: int,
                  report: Path | None = None) -> None:
    """Free the runner when the pool could not be shut down. Returns only if not abandoned.

    Must run even while an exception is propagating: multiprocessing's atexit handler
    SIGTERMs its daemon workers (ignored in uninterruptible sleep) and then join()s them
    with NO timeout, so an abandoned pool plus any raise between the pool's finally and
    here hangs the interpreter until the job ceiling kills it. Reproduced: rc=124 at 25s
    with SIGTERM-ignoring workers standing in for D state."""
    if not abandoned:
        return
    try:
        if sys.exc_info()[0] is not None:
            # os._exit below discards the traceback, and this is often the only place the
            # real failure would ever be printed
            traceback.print_exc()
    except OSError:
        pass
    # Word this on evidence: shutdown_pool also returns False when terminate() RAISES, and
    # a live worker after terminate() is what distinguishes a wedge from a harness bug.
    # Count WORKERS only -- _pool_procs appends the Manager, our own healthy child, so
    # including it made n >= 1 always and the harness-error branch unreachable. It is killed
    # separately: os._exit skips its finalizer, and orphaned it holds the runner's stdout.
    n = hil_health.kill_pool_children(pool)
    hil_health.kill_pool_children(None, mgr)
    if n:
        _p(f'HIL worker pool would not terminate ({n} worker(s) still live, '
           f'uninterruptible); SIGKILLed them and abandoned the rest to free the '
           f'runner. Boards held by any leaked worker stay locked until the host is '
           f'power-cycled.', flush=True)
    else:
        _p('HIL worker pool shutdown failed but left no live worker behind, so this is '
           'a harness error rather than a wedged rig -- see the Pool.terminate() '
           'warning above. Exiting early anyway to free the runner; no board should '
           'stay locked.', flush=True)
    # A report already written by accumulate_report says nothing about the abandon, and a
    # green table under a red job is how an agent ends up pasting it as this run's result.
    # Prepend the caveat; best-effort, never at the cost of exiting.
    if report is not None:
        try:
            if report.exists():
                # utf-8 explicitly (the cells are ✅/❌/⚪) and catch ValueError too: a torn
                # report or a LANG=C locale raises UnicodeDecodeError -- NOT an OSError --
                # straight past os._exit, stranding the runner.
                body = report.read_text(encoding='utf-8', errors='replace')
                # Only when no banner is there yet, searched anywhere in the head rather
                # than at char 0: write_timeout_report's banner must stay FIRST (its table
                # is a PREVIOUS attempt's) and it puts the rig-health quote above itself.
                if '**HIL run ab' not in body[:2000]:
                    report.write_text(
                        '**HIL run abandoned: the worker pool would not shut down.** The '
                        'table below was collected before the abandon; treat board '
                        'results as unverified.\n\n' + body, encoding='utf-8')
        except (OSError, ValueError):
            pass
    try:
        sys.stdout.flush()
    except OSError:
        pass
    # Clamped: os._exit takes a status byte, so err_count == 256 would truncate to 0 and
    # report a failing, abandoned run as green.
    os._exit(min(err_count, 125) if err_count else 1)


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
    # default 1, not 3: the pool guard is a FLAT 3600s that does not scale with max_retry,
    # and one usbtest test at default 3 can burn 1530s of it (510s outer x3) for a single
    # board. Every CI caller already pins --retry 1; the bare invocations in the hil skill
    # and hil-validate.js run against the same one-slot rig and used to inherit 3.
    parser.add_argument('-r', '--retry', type=int, default=1, help='Retry count for failed tests (default: 1)')
    parser.add_argument('-v', '--verbose', action='store_true', help='Verbose output')
    args = parser.parse_args()
    if args.retry < 1:
        # 0 would make every test loop body never run: all-red cells, exit 0
        parser.error('--retry must be >= 1')

    config_file = Path(args.config_file)
    boards = args.board
    verbose = args.verbose
    hil_util.verbose = args.verbose
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
    if not config_boards:
        # same reason the unknown -b board exits 1: 'No tests were run.' with rc 0 reads as
        # a green HIL leg, so a roster edit emptying a leg's filter stops testing silently
        msg = (f'No boards left after the flasher filter (--flasher '
               f'{args.flasher or "-"}, --exclude-flasher {args.exclude_flasher or "-"})')
        print(msg, flush=True)
        # loud AND leaving evidence: exiting with no report at all lets the PR comment
        # keep the previous push's stale table under a red job
        try:
            rd = Path(os.environ.get('HIL_REPORT_DIR', '.'))
            rd.mkdir(parents=True, exist_ok=True)
            (rd / REPORT_MD).write_text(f'**HIL run selected no boards.** {msg}\n',
                                        encoding='utf-8')
        except OSError:
            pass
        sys.exit(1)


    # Before the build: the probe needs nothing from it, and the annotation is more useful
    # early than after a multi-board cmake build has been paid for.
    # One line, not a probe: a D-state pid at start-up is a hint for whoever reads a red
    # cell, never a reason to refuse the run. hil_pool_check does diagnosis.
    note = hil_health.d_state_note()
    if note:
        log_line(f'rig note: {note}')
    health_banner = f'> **Rig note.** {note}. Not a fault on its own -- a healthy testusb sits in D state for most of every case.\n' if note else ''

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

    # The report sidecar and the .failed re-run spec live in report_dir (CI keys it by run
    # id: persistent across attempts, private to one run). A full run starts fresh; a re-run
    # (--accumulate, which .failed always starts with) merges so already-passed boards
    # survive. -bt alone is not a re-run marker.
    report_dir = Path(os.environ.get('HIL_REPORT_DIR', '.'))
    failed_fname = report_dir / (config_file.name + '.failed')
    fresh = not args.accumulate
    # The unlink is DEFERRED to inside the pool try/except below: wiping here leaves
    # Manager() and Pool() running with the old report gone and no report-writing path
    # armed, so an EAGAIN/ENOMEM on fork gives CI an EMPTY report dir with no reason.

    seed = os.getenv('HIL_SHUFFLE_SEED') or str(int(time.time()))
    log_line(f'test-order shuffle seed: {seed} (HIL_SHUFFLE_SEED={seed} to replay); '
             f'flash/usbtest parallel per controller: {hil_lock.FLASH_PARALLEL}/{hil_lock.USBTEST_PARALLEL}; '
             f'enum timeout first/retry: {ENUM_TIMEOUT}/{ENUM_TIMEOUT_RETRY}s; '
             # all three are env-tunable, so a run that dies on the guard is otherwise
             # unattributable from the log alone
             f'pool guard: {POOL_TIMEOUT}s')

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

    # Bound BEFORE the try so the finally can name them whatever failed: Pool() forks, and
    # the EAGAIN/ENOMEM the wipe comment below worries about is most likely to come from
    # that fork -- after a convoy, where every stranded read holds a thread and an fd. Left
    # outside, an OSError there escaped with mgr LIVE and `pool` unbound, so no report was
    # written and the interpreter unwound into multiprocessing's unbounded atexit join.
    pool = mgr = cmap = None
    # Defined before the pool so _abandon_exit always has a value: a raise before
    # `err_count = build_err + ...` would turn the containment path into a NameError.
    err_count = build_err
    # Fail CLOSED: only a shutdown_pool() that actually returned True clears this, and the
    # assignment sits at the END of the inner finally, so anything raising before it
    # (kill_worker_children, a BrokenPipeError from its print) leaves _abandon_exit armed.
    pool_abandoned = True
    # BEFORE Manager()/Pool(), not inside the try: hil_ci.sh reuses a persistent REMOTE_DIR
    # and scp's the report back unconditionally, so if a fork failure (OSError/EAGAIN right
    # after a convoy -- the case this whole block guards) skipped the wipe, the finally's
    # _abandon_exit would prepend "HIL run abandoned" to the PREVIOUS run's table and
    # publish last night's board results as this run's. Nothing is live yet here, so an
    # OSError from the wipe itself just exits with its traceback -- it cannot strand the
    # interpreter in multiprocessing's unbounded atexit join, which is what deferring it
    # was protecting against.
    if fresh:
        report_dir.mkdir(parents=True, exist_ok=True)
        for f in (REPORT_JSON, REPORT_MD):
            (report_dir / f).unlink(missing_ok=True)
        failed_fname.unlink(missing_ok=True)
    try:
        mgr = Manager()
        cmap = mgr.dict()
        initargs = (Lock(), seed,
                    hil_lock.make_permit_sems(Semaphore, hil_lock.USBTEST_PARALLEL),
                    hil_lock.make_permit_sems(Semaphore, hil_lock.FLASH_PARALLEL),
                    cmap, Lock(), hints_by_uid)
        # maxtasksperchild=1: the sysfs blindness latch is process-global and permanent
        # (no decrement anywhere -- see hil_util.SYSFS_STUCK_MAX), so a worker that goes
        # blind on ONE wedged board would report 0/30 and "probe missing" for the 2-3
        # healthy boards it picked up afterwards. A fresh worker per board confines the
        # damage to the board that caused it; the extra fork is noise against a
        # flash+test cycle.
        pool = Pool(processes=os.cpu_count() or 1, initializer=init_worker,
                    initargs=initargs, maxtasksperchild=1)
        # OUTER: encloses the pool block too, not just the reporting below. An exception
        # escaping async_ret.get() (a worker exception, a Ctrl-C) runs the pool finally and
        # then propagates straight out of main(); with _abandon_exit in a sibling try it
        # was never reached.
        try:
            # imap_unordered, NOT map_async: map_async is all-or-nothing, so a guard expiry
            # threw away every board that had already finished -- up to a worker-width of
            # completed rig time -- and left the re-run spec unwritten, so CI re-tested all
            # ~26 boards to find the one that wedged. Draining as results arrive keeps what
            # finished and names only what was still in flight.
            it = pool.imap_unordered(test_board, config_boards)
            mret = []
            deadline = time.monotonic() + POOL_TIMEOUT
            try:
                mret = drain_pool(it, config_boards, deadline, out=mret)
            except MpTimeoutError as te:
                mret = te.finished
                stuck = [b['name'] for b in config_boards
                         if b['name'] not in {r[0] for r in mret}]
                # The re-run spec FIRST and before the raise: a fresh run already unlinked
                # it, so leaving it unwritten is what made the GitHub re-run repeat the
                # whole fleet. Only the boards that never reported go in it.
                _write_failed_spec(failed_fname, report_dir,
                                   [(n, 1, [], None, 0) for n in stuck]
                                   + [r for r in mret if r[1] > 0])
                # Then the report, with the rows that DID finish, before anything that can
                # block. Then RAISE into the ONE containment path: the inner finally runs
                # the ordered sweep (kill_worker_children BEFORE terminate, or a reaped
                # worker's flasher reparents out of reach), the outer one os._exit's.
                banner = (f'**HIL run abandoned: worker pool timed out after '
                          f'{POOL_TIMEOUT}s.** {len(mret)} board(s) below finished and '
                          f'are this run\'s; {len(stuck)} never reported and are NOT in '
                          f'the table: {", ".join(stuck)}. Re-run covers those.\n')
                try:
                    accumulate_report(mret, report_dir, fresh, '',
                                      health_banner + _blind_note(mret)
                                      + _stray_note(mret) + banner)
                except Exception as rerr:  # noqa: BLE001 - the raise below must still happen
                    # FALL BACK, do not just warn: accumulate_report can raise on an
                    # unwritable/root-owned report dir or a torn JSON, and _abandon_exit
                    # only PREPENDS to a report that exists. Without this the artifact
                    # upload finds nothing (if-no-files-found: ignore) and the sticky PR
                    # comment keeps the previous push's green table under a red job.
                    print(f'warning: partial report failed: {type(rerr).__name__}: {rerr}; '
                          f'falling back to the board list', flush=True)
                    try:
                        hil_health.write_timeout_report(
                            report_dir, [b for b in config_boards
                                         if b['name'] in stuck], POOL_TIMEOUT, REPORT_MD,
                            prefix=health_banner)
                    except Exception as re2:  # noqa: BLE001
                        print(f'warning: fallback report failed too: '
                              f'{type(re2).__name__}: {re2}', flush=True)
                _p(f'HIL worker pool timed out after {POOL_TIMEOUT}s; sweeping and '
                   f'shutting it down (abandoning it if a worker is unkillable)',
                   flush=True)
                raise RuntimeError(f'HIL worker pool timed out after {POOL_TIMEOUT}s')
            except Exception as e:
                # A worker RAISED -- e.g. a flasher adapter dropping off the bus makes
                # get_serial_dev raise in the worker's flash section, which no per-test
                # handler guards. Same treatment as the timeout path: the drain means
                # `mret` already holds every board that finished, so keep those rows and
                # name only the ones still in flight. (Under map_async they were all lost,
                # which is what the old banner here claimed.)
                done = {r[0] for r in mret}
                stuck = [b['name'] for b in config_boards if b['name'] not in done]
                _write_failed_spec(failed_fname, report_dir,
                                   [(n, 1, [], None, 0) for n in stuck]
                                   + [r for r in mret if r[1] > 0])
                banner = (f'**HIL run aborted: a worker raised {type(e).__name__}: {e}.** '
                          f'{len(mret)} board(s) below finished and are this run\'s; '
                          f'{len(stuck)} did not report: {", ".join(stuck)}.\n')
                try:
                    accumulate_report(mret, report_dir, fresh, '',
                                      health_banner + _blind_note(mret)
                                      + _stray_note(mret) + banner)
                except Exception as re2:  # noqa: BLE001 - the raise below must still happen
                    print(f'warning: partial report failed: {type(re2).__name__}: {re2}',
                          flush=True)
                raise

            err_count = build_err + sum(e[1] for e in mret)
            _write_failed_spec(failed_fname, report_dir, mret)
        finally:
            # Not `with Pool(...)`: its __exit__ joins the workers unbounded, hanging on
            # any worker in uninterruptible sleep. shutdown_pool bounds the same terminate()
            # by a grace period, so the pool is NOT cleanly closed/joined when it returns
            # False. Record the outcome but never exit here: the report below is the only
            # record of a run that otherwise passed.
            #
            # Same ordering as the timeout path: what the workers spawned must be
            # snapshotted and killed while its parent is alive, or terminate() reparents it
            # out of reach.
            #
            # Both calls must stay guarded: a raise here skips accumulate_report(), so a run
            # whose boards ALL passed publishes an empty report dir -- and both can raise
            # for reasons unrelated to the results. pool_abandoned stays fail-CLOSED, so
            # _abandon_exit still arms.
            try:
                # Still worth running for the TIMEOUT path, where the workers are
                # genuinely stuck mid-task and their children are still reachable through
                # the pool's ppid tree. On the normal path every worker has already swept
                # its own (kill_own_children) and retired, so this finds nothing.
                #
                # No banner from here: this finally runs AFTER accumulate_report on both
                # abort paths, so anything appended to health_banner now is written to a
                # variable nobody reads again. The report gets its count from the result
                # tuples instead, via _stray_note.
                hil_health.kill_worker_children(pool, mgr)
            except Exception as e:
                print(f'warning: worker-child sweep failed: {type(e).__name__}: {e}',
                      flush=True)
            try:
                pool_abandoned = not hil_health.shutdown_pool(pool)
            except Exception as e:
                print(f'warning: pool shutdown failed: {type(e).__name__}: {e}', flush=True)

        # refresh controller hints: pci resolved this run, plus durations from full runs
        # only (a filtered run would understate the board's real cost)
        try:
            if PROFILE:
                # debug snapshot of the run's live uid->PCI / PCI->slot resolutions
                report_dir.mkdir(parents=True, exist_ok=True)
                with (report_dir / 'hil_profile_ctrl.json').open('w') as f:
                    json.dump(dict(cmap), f, indent=1, sort_keys=True)
            uid_of = {b['name']: b['uid'] for b in config['boards']}
            for name, _, _, _, dur, *_ in mret:
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
            # our startup read, so overlay only this run's boards and replace atomically
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
        except Exception as e:
            # Deliberately broad, and it must stay that way: this best-effort refresh makes
            # Manager proxy RPCs that raise EOFError / BrokenPipeError / RemoteError when
            # the Manager child has died, none of them OSErrors -- an OSError-only guard let
            # those skip accumulate_report(). Nothing here is worth the report.
            print(f'warning: cannot persist controller hints to {CONTROLLER_CACHE}: '
                  f'{type(e).__name__}: {e}')


        # board x test result matrix -> hil_report.md (accumulates across re-runs) + stdout.
        # -b/-bt means a filtered run (PR selection or a re-run spec): say so, or the report
        # looks exactly like a full run that happened to be small
        scoped = sorted(set(args.board) | set(board_test))
        scope = f'{len(scoped)} board(s) — {", ".join(scoped)}' if scoped else ''
        report = accumulate_report(mret, report_dir, fresh, scope,
                                   health_banner + _blind_note(mret)
                                   + _stray_note(mret))
        print()
        print(report)
        print(f'\nReport written to {(report_dir / REPORT_MD).resolve()}')

        duration = time.time() - duration
        print()
        print("-" * 30)
        print(f'Total failed: {err_count} in {duration:.1f}s')
        print("-" * 30)
    finally:
        # In the finally, not after: any raise above (accumulate_report sits outside the
        # OSError handler) would skip the abandon path and unwind into multiprocessing's
        # unbounded atexit join, hanging the runner.
        _abandon_exit(pool, mgr, pool_abandoned, err_count, report_dir / REPORT_MD)
    # Same clamp: exit status is a byte either way, so 256 failures would report green.
    sys.exit(min(err_count, 125))


if __name__ == '__main__':
    main()
