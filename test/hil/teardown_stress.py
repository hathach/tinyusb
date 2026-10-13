# SPDX-License-Identifier: MIT
"""Unconfigure/reconfigure a usbtest (cafe:4010) device while bulk traffic is still on the bus, so a
configuration change can land while a device-side DMA is in flight. The raw SET_CONFIGURATION
bypasses the kernel, which would otherwise quiesce the endpoints first. Exit 0: every iteration's
control and bulk traffic recovered; exit 1: EP0 stopped answering or an iteration ran without bulk
traffic (first failing iteration reported); exit 2: device not found or usage error."""
import argparse
import os
import sys
import threading
import time

import usb.core
import usb.util

sys.path.append(os.path.dirname(os.path.abspath(__file__)))  # PYTHONSAFEPATH drops it
import usbtest

REQ_SET_CONFIGURATION, REQ_GET_DESCRIPTOR = 0x09, 0x06
DESC_DEVICE = 0x0100
CTRL_TIMEOUT_MS = 500
BULK_LEN = 512


def find(serial):
    """The device by bounded sysfs lookup (a wedged peer must not hang the scan), else None."""
    try:
        info = usbtest.find_device(serial)
    except SystemExit as e:  # several devices and no --serial
        print(e, file=sys.stderr)
        return None
    if info is None or 'ambiguous' in info:
        print(f'usbtest device (serial {serial}): {info or "not found"}', file=sys.stderr)
        return None
    bus, address = (int(x) for x in info['node'].split('/')[-2:])
    dev = usb.core.find(bus=bus, address=address)
    if dev is None:
        print(f'usbtest device left {info["node"]} before it was opened', file=sys.stderr)
    return dev


def bulk_eps(dev):
    intf = dev.get_active_configuration()[(0, 1)]  # alt 0 carries no endpoints
    out = in_ = None
    for ep in intf:
        if usb.util.endpoint_type(ep.bmAttributes) != usb.util.ENDPOINT_TYPE_BULK:
            continue
        if usb.util.endpoint_direction(ep.bEndpointAddress) == usb.util.ENDPOINT_IN:
            in_ = ep.bEndpointAddress
        else:
            out = ep.bEndpointAddress
    if out is None or in_ is None:
        raise ValueError('usbtest alt 1 lacks a bulk endpoint pair')
    return intf.bInterfaceNumber, out, in_


def open_alt1(serial):
    """The device configured with interface alt 1, as (dev, itf, ep_out, ep_in), else None."""
    dev = find(serial)
    if dev is None:
        return None
    itf, ep_out, ep_in = bulk_eps(dev)
    if dev.is_kernel_driver_active(itf):
        dev.detach_kernel_driver(itf)
    dev.ctrl_transfer(0x00, REQ_SET_CONFIGURATION, 1, 0, timeout=CTRL_TIMEOUT_MS)
    dev.set_interface_altsetting(itf, 1)
    return dev, itf, ep_out, ep_in


def pump(dev, ep_out, ep_in, stop, counts):
    data = bytes(BULK_LEN)
    while not stop.is_set():
        for fn in (lambda: len(dev.read(ep_in, BULK_LEN, timeout=20)), lambda: dev.write(ep_out, data, timeout=20)):
            try:
                full = fn() == BULK_LEN  # usbtest never sends a short packet: a short transfer is an error
            except usb.core.USBError:
                full = False
            counts['ok' if full else 'err'] += 1


def ep0_alive(dev):
    try:
        return len(dev.ctrl_transfer(0x80, REQ_GET_DESCRIPTOR, DESC_DEVICE, 0, 18, timeout=CTRL_TIMEOUT_MS)) == 18
    except usb.core.USBError:
        return False


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--serial')
    ap.add_argument('-n', '--iterations', type=int, default=200)
    args = ap.parse_args()
    if args.iterations < 1:
        ap.error('need --iterations >= 1')

    opened = open_alt1(args.serial)
    if opened is None:
        return 2
    dev, itf, ep_out, ep_in = opened

    oks = []
    for i in range(args.iterations):
        stop, counts = threading.Event(), {'ok': 0, 'err': 0}
        t = threading.Thread(target=pump, args=(dev, ep_out, ep_in, stop, counts), daemon=True)
        t.start()
        time.sleep(0.02)
        failed = None
        try:
            dev.ctrl_transfer(0x00, REQ_SET_CONFIGURATION, 0, 0, timeout=CTRL_TIMEOUT_MS)
        except usb.core.USBError as e:
            failed = f'SET_CONFIGURATION(0): {e}'
        stop.set()
        t.join()
        oks.append(counts['ok'])
        if failed is None and not ep0_alive(dev):
            failed = 'GET_DESCRIPTOR after SET_CONFIGURATION(0) failed'
        if failed is None and counts['ok'] == 0:
            failed = 'no bulk traffic around SET_CONFIGURATION(0), nothing exercised'
        if failed is None:
            try:
                dev.ctrl_transfer(0x00, REQ_SET_CONFIGURATION, 1, 0, timeout=CTRL_TIMEOUT_MS)
                dev.set_interface_altsetting(itf, 1)
                written = dev.write(ep_out, bytes(BULK_LEN), timeout=500)
                read = len(dev.read(ep_in, BULK_LEN, timeout=500))
                if (written, read) != (BULK_LEN, BULK_LEN):
                    failed = f'short bulk after SET_CONFIGURATION(1): wrote {written} read {read} of {BULK_LEN}'
            except usb.core.USBError as e:
                failed = f'reconfigure/bulk after SET_CONFIGURATION(1): {e}'
        if failed:
            print(f'FAIL iteration {i}: {failed} (bulk ok {counts["ok"]} err {counts["err"]})')
            return 1
    print(f'bulk ok per iteration min {min(oks)} max {max(oks)}')
    print(f'PASS {args.iterations} iterations')
    return 0


if __name__ == '__main__':
    sys.exit(main())
