# SPDX-License-Identifier: MIT
"""Abandon a usbtest (cafe:4010) control request the device answers late (vendor 0x5d), then check
that the next requests are answered correctly: the late answer was made for a SETUP the host has
already superseded, and must not be taken as a stage of the next one. 0x5e proves the next SETUP
reached the device before the late answer, and that the next 0x5b went through SETUP, DATA and ACK
once each. The first request after the abandoned one only absorbs what the host does after a
cancelled control URB: behind a hub it can fail before reaching the device. Exit 0: every
iteration recovered and was exercised;
exit 1: a later request failed or an iteration was not exercised; exit 2: device not found."""
import argparse
import errno
import os
import struct
import sys
import time

import usb.core

sys.path.append(os.path.dirname(os.path.abspath(__file__)))  # PYTHONSAFEPATH drops it
import teardown_stress as ts

REQ_GET_STATUS, REQ_DELAYED, REQ_DELAY_STATS, REQ_WRITE, REQ_READ = 0x00, 0x5d, 0x5e, 0x5b, 0x5c
VENDOR_OUT, VENDOR_IN = 0x40, 0xC0
DATA_LEN = 64
DELAY_DATA_LEN = 32  # its own length and firmware buffer, so a stale arm cannot pass for the next request's
STAGES_SETUP_DATA_ACK = 0x123
# (bmRequestType, wIndex = firmware action, wLength or OUT data)
ACTIONS = {
    'status':   (VENDOR_OUT, 0, None),
    'data-in':  (VENDOR_IN, 1, DELAY_DATA_LEN),
    'data-out': (VENDOR_OUT, 1, bytes([0xa5]) * DELAY_DATA_LEN),
    'stall':    (VENDOR_OUT, 2, None),
}


def delay_stats(dev):
    return struct.unpack('<HHH', bytes(dev.ctrl_transfer(VENDOR_IN, REQ_DELAY_STATS, 0, 0, 6, timeout=1000)))


def abandon(dev, action, delay_ms):
    req_type, w_index, payload = ACTIONS[action]
    try:
        dev.ctrl_transfer(req_type, REQ_DELAYED, delay_ms, w_index, payload, timeout=delay_ms // 3)
    except usb.core.USBTimeoutError:
        return None
    except usb.core.USBError as e:
        return f'0x5d failed with {e}, expected a timeout'
    return '0x5d completed within its timeout, nothing superseded'


def absorb_after_cancel(dev, deadline):
    # Seen as EPIPE ~1 ms after submission while the device was still delaying and never saw it, so
    # an EPIPE counts only before `deadline`, well ahead of the delayed answer. Reaching the device
    # instead makes it the superseding request, so it must then be answered.
    try:
        got = dev.ctrl_transfer(0x80, REQ_GET_STATUS, 0, 0, 2, timeout=1000)
    except usb.core.USBError as e:
        if e.errno == errno.EPIPE and time.monotonic() < deadline:
            return None
        return f'GET_STATUS after the abandoned request: {e}'
    if len(got) != 2:
        return f'GET_STATUS after the abandoned request returned {len(got)} bytes'
    return None


def check_next(dev, device_desc, seq):
    pattern = bytes((seq + k) & 0xff for k in range(DATA_LEN))
    dev.ctrl_transfer(VENDOR_OUT, REQ_WRITE, 0, 0, pattern, timeout=1000)
    got = bytes(dev.ctrl_transfer(VENDOR_IN, REQ_READ, 0, 0, DATA_LEN, timeout=1000))
    if got != pattern:
        return f'0x5c read back {got[:8].hex()}.. len {len(got)}, wrote {pattern[:8].hex()}..'
    got = bytes(dev.ctrl_transfer(0x80, ts.REQ_GET_DESCRIPTOR, ts.DESC_DEVICE, 0, 18, timeout=1000))
    if got != device_desc:
        return f'device descriptor {got.hex()} != {device_desc.hex()}'
    return None


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--serial')
    ap.add_argument('-n', '--iterations', type=int, default=50, help='per action')
    ap.add_argument('--delay', type=int, default=100, help='ms the device waits before answering')
    ap.add_argument('--actions', default=','.join(ACTIONS))
    args = ap.parse_args()
    if args.iterations < 1 or not 3 <= args.delay <= 1000:
        ap.error('need --iterations >= 1 and 3 <= --delay <= 1000 (firmware limit; delay/3 is the timeout, 0 = none)')

    opened = ts.open_alt1(args.serial)
    if opened is None:
        return 2
    dev = opened[0]
    device_desc = bytes(dev.ctrl_transfer(0x80, ts.REQ_GET_DESCRIPTOR, ts.DESC_DEVICE, 0, 18, timeout=1000))

    _, delays, _ = delay_stats(dev)
    seq = 0
    for action in args.actions.split(','):
        for i in range(args.iterations):
            seq += 1
            where = f'{action} iteration {i}'
            deadline = time.monotonic() + args.delay / 2000
            failed = abandon(dev, action, args.delay)
            if failed:
                print(f'FAIL {where}: {failed}')
                return 1
            try:
                failed = absorb_after_cancel(dev, deadline) or check_next(dev, device_desc, seq)
                newer, now, stages = delay_stats(dev)
            except usb.core.USBError as e:
                print(f'FAIL {where}: request after the abandoned one: {e}')
                return 1
            if not failed and stages != STAGES_SETUP_DATA_ACK:
                failed = f'0x5b stages {stages:#x}, expected {STAGES_SETUP_DATA_ACK:#x}'
            if failed:
                print(f'FAIL {where}: {failed}')
                return 1
            if now != (delays + 1) & 0xffff or newer == 0:
                print(f'FAIL {where}: not exercised (delays {delays}->{now}, newer SETUPs {newer})')
                return 1
            delays = now
    print(f'PASS {args.actions} x {args.iterations}')
    return 0


if __name__ == '__main__':
    sys.exit(main())
