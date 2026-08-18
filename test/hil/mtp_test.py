#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# One MTP test session for one board, in a disposable process. Every libmtp call is
# synchronous ctypes in our own address space and blocks in a usbfs ioctl in D state on
# a wedged device, where not even SIGKILL is delivered — so the session must be
# something the harness can abandon: hil_test.test_device_mtp runs it under
# hil_util.run_cmd (killpg + bounded reap, rc 124 on timeout). Imports stay stdlib +
# pymtp: nothing here may pull in the harness.
#
# Exit 0 on a fully passing session; 1 with the failure on stdout/stderr otherwise.
import argparse
import ctypes
import glob
import hashlib
import os
import signal
import subprocess
import sys
import threading
import time

sys.path.append(os.path.dirname(os.path.abspath(__file__)))  # PYTHONSAFEPATH drops it
# -- APPEND so PYTHONPATH still wins (the tests steer a fake pymtp that way)

from pathlib import Path
from pymtp import LIBMTP_DeviceEntry, LIBMTP_RawDevice, MTP

FILE1_EXPECT = b'TinyUSB MTP Filesystem example'
FILE2_MD5_EXPECT = '40ef23fc2891018d41a05d4a0d5f822f'  # md5sum of logo.png


# Real paths by default; the offline tests point these at a fixture tree, the same way
# they steer the pymtp fake through FAKE_PYMTP_*.
# The one test seam: '' in production, a tmpdir in the offline tests, which mirror the
# real layout beneath it. This runs as a SUBPROCESS (a libmtp call blocked in a usbfs
# ioctl hangs its thread forever, so the session must be somewhere killable), and neither
# monkeypatching nor import shadowing crosses that boundary -- unlike the fake pymtp,
# which the tests inject through PYTHONPATH alone.
_ROOT = os.environ.get('HIL_MTP_FAKE_ROOT', '')
_MARKER_GLOB = f'{_ROOT}/dev/libmtp-*'
_SYS_USB = Path(f'{_ROOT}/sys/bus/usb/devices')
_USB_DEV = Path(f'{_ROOT}/dev/bus/usb')


def _bounded_read(path, grace: float = 2.0):
    """Read a sysfs attribute with a wall-clock bound, or return None.

    `serial` is served under the device lock a wedged usbfs ioctl holds, and EVERY MTP DUT
    is cafe:4017 -- so the vid/pid filter below cannot rule out a wedged NEIGHBOUR, and an
    unbounded read of its serial would burn this session's whole budget and report a
    healthy board as wedged. Stdlib only by design (this file never imports the harness),
    so this is a small local twin of hil_util.read_sysfs.
    """
    out = {}

    def _read():
        try:
            out['v'] = path.read_text().strip()
        except OSError:
            pass

    t = threading.Thread(target=_read, daemon=True)
    t.start()
    t.join(grace)
    return out.get('v')


def _ready_marker(uid: str):
    """(busnum, devnum) of the udev-ready MTP device with this serial, or None.

    /dev/libmtp-<sysname> is published by libmtp-runtime AFTER its synchronous mtp-probe
    accepts the device, so this set is both small and ready -- unlike a sysfs-wide scan,
    which races re-enumerations from other boards' jobs. Requires the libmtp-runtime
    package.
    """
    for marker_name in glob.glob(_MARKER_GLOB):
        marker = Path(marker_name)
        try:
            dev = _SYS_USB / marker.name[len('libmtp-'):]
            # vid/pid first: lock-free descriptor fields, so they rule out every other
            # device before the `serial` read, which the kernel serves under the device
            # lock a wedged usbfs ioctl would hold
            if ((dev / 'idVendor').read_text().strip() != 'cafe'
                    or (dev / 'idProduct').read_text().strip() != '4017'):
                continue
            # bounded: this one CAN block, and a wedged neighbour shares the vid/pid above
            serial = _bounded_read(dev / 'serial')
            if serial is None or serial.lower() != uid.lower():
                continue
            busnum = int((dev / 'busnum').read_text())
            devnum = int((dev / 'devnum').read_text())
            node = _USB_DEV / f'{busnum:03d}' / f'{devnum:03d}'
            if marker.resolve(strict=True) != node or not os.access(node, os.R_OK | os.W_OK):
                continue
            return busnum, devnum
        except (OSError, ValueError):
            # a marker can vanish while another board flashes: not our device's problem
            continue
    return None


def _gvfs_unmount(uid: str, deadline: float) -> None:
    """Drop any gvfs claim on this device, immediately before opening it.

    Called only once the udev marker exists. gvfs claims an MTP device AFTER udev
    probing, so before the marker there is nothing to unmount: an earlier call is a
    guaranteed no-op that still forks a process, and it leaves the gap between the
    unmount and the open unprotected -- the hang this exists to prevent. Per-iteration
    calls also forked one gio per second of the enumeration budget.
    """
    # Popen, not run(timeout=): run's post-timeout reap is an unbounded wait(), and a gio
    # blocked in D state on a wedged usbfs node does not die on SIGKILL, so run(timeout=2)
    # can hang for good. Bounded by at most HALF of what is LEFT of our own budget, never
    # a fixed sub-bound: the parent gives us --timeout 8 (4 on a retry), so anything larger
    # collapsed the poll loop to one attempt and made a slow gio look like a wedged session.
    gio_bound = max(0.5, min(3.0, (deadline - time.monotonic()) / 2))
    try:
        # argv, not shell=True: uid comes from a hand-edited roster and is board firmware
        # output, so a space or $(...) would unmount the wrong URI (leaving the gvfs mount
        # held) or run as us.
        gio = subprocess.Popen(['gio', 'mount', '-u',
                                f'mtp://TinyUsb_TinyUsb_Device_{uid}/'],
                               stdout=subprocess.DEVNULL,
                               stderr=subprocess.DEVNULL, start_new_session=True)
    except OSError:
        # glib2.0-bin absent (ci.lan has no gio at all): nothing holds a gvfs mount
        # either, so go straight on to the open.
        return
    try:
        gio.wait(timeout=gio_bound)
    except subprocess.TimeoutExpired:
        try:
            os.killpg(gio.pid, signal.SIGKILL)
        except OSError:
            gio.kill()
        try:
            gio.wait(timeout=2)     # reap it: an abandoned gio leaves a zombie
        except subprocess.TimeoutExpired:
            pass
        print('gio unmount timed out; continuing', file=sys.stderr)


def open_mtp_dev(uid: str, timeout: float):
    mtp = MTP()
    deadline = time.monotonic() + timeout
    while True:
        try:
            # pymtp raises USB_LAYER/PTP_LAYER/GENERAL/AlreadyConnected on a board still
            # settling right after a flash; an unguarded raise would skip the rest of the
            # enumeration budget (and the disconnect) instead of retrying.
            #
            # Never detect_devices(): that PROBES every MTP device on the rig, so a board
            # still initialising in a parallel job answers our scan (the race #3790 fixed).
            # libmtp-runtime publishes /dev/libmtp-<sysname> only after its own mtp-probe
            # has accepted a device, so start from that small, ready-only set and open OUR
            # device directly by bus/dev address.
            target = _ready_marker(uid)
            if target:
                # ready first, THEN unmount, then open -- see _gvfs_unmount
                _gvfs_unmount(uid, deadline)
                busnum, devnum = target
                # TinyUSB needs no libmtp quirks, so the raw entry can be built here
                entry = LIBMTP_DeviceEntry(None, 0xcafe, None, 0x4017, 0)
                raw = LIBMTP_RawDevice(entry, busnum, devnum)
                mtp.device = mtp.mtp.LIBMTP_Open_Raw_Device(ctypes.byref(raw))
                if mtp.device:
                    serial = mtp.get_serialnumber()
                    if (serial.decode('utf-8') if serial else '').lower() == uid.lower():
                        return mtp
                    mtp.disconnect()
        except Exception as e:
            print(f'mtp poll: {type(e).__name__}: {e}', file=sys.stderr)
            # only when a device was actually opened: pymtp's `self.device == None`
            # guard does NOT catch a ctypes NULL pointer (falsy, but != None), so
            # disconnecting blindly calls LIBMTP_Release_Device(NULL)
            if getattr(mtp, 'device', None):
                try:
                    mtp.disconnect()
                except Exception:
                    pass
                mtp.device = None
        if time.monotonic() >= deadline:
            return None
        time.sleep(1)


def run_session(uid: str, timeout: float) -> int:
    mtp = open_mtp_dev(uid, timeout)
    if mtp is None or mtp.device is None:
        print('MTP device not found')
        return 1

    try:
        assert b"TinyUSB" == mtp.get_manufacturer(), 'MTP wrong manufacturer'
        assert b"MTP Example" == mtp.get_modelname(), 'MTP wrong model'
        assert b'1.0' == mtp.get_deviceversion(), 'MTP wrong version'
        assert b'TinyUSB MTP' == mtp.get_devicename(), 'MTP wrong device name'

        f1 = uid.encode("utf-8") + b'_file1'
        f2 = uid.encode("utf-8") + b'_file2'
        f3 = uid.encode("utf-8") + b'_file3'
        mtp.get_file_to_file(1, f1)
        with open(f1, 'rb') as file:
            f1_data = file.read()
            os.remove(f1)
            assert f1_data == FILE1_EXPECT, 'MTP file1 wrong data'
        mtp.get_file_to_file(2, f2)
        with open(f2, 'rb') as file:
            f2_data = file.read()
            os.remove(f2)
            assert FILE2_MD5_EXPECT == hashlib.md5(f2_data).hexdigest(), 'MTP file2 wrong data'
        with open(f3, "wb") as file:
            # 1524-byte payload + 12-byte MTP header = 3 full 512-byte buffers, so this
            # exercises delivery of the final OUT payload before its ZLP. Deliberate and
            # FIXED: a random size hits that boundary in ~0.2% of runs, which is not a test
            # of it. Deterministic content so a mismatch is reproducible.
            f3_data = bytes((i % 251) + 1 for i in range(1524))
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
    except AssertionError as e:
        print(e)
        return 1
    finally:
        mtp.disconnect()
    return 0


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument('--uid', required=True, help='board_get_unique_id serial to match')
    parser.add_argument('--timeout', type=float, default=30, help='enumeration wait budget (s)')
    args = parser.parse_args()
    return run_session(args.uid, args.timeout)


if __name__ == '__main__':
    sys.exit(main())
