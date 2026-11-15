#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# usbtmc_test.Dut.find against a fake sysfs tree and a fake pyusb: every USBTMC DUT is cafe:401c, so the
# requested serial is resolved from sysfs, bounded, before the one matching device is opened.
# Run directly:
#   python3 test/hil/test/test_hil_usbtmc.py
import os
import sys
import time
import unittest
from pathlib import Path
from tempfile import TemporaryDirectory

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

import usbtest_harness   # noqa: E402
import usbtmc_test       # noqa: E402


class FakeEp:
    def __init__(self, addr, attrs):
        self.bEndpointAddress, self.bmAttributes, self.wMaxPacketSize = addr, attrs, 64


class FakeItf(list):
    bInterfaceNumber = 0


class FakeDev:
    def __init__(self, vid=usbtmc_test.VID, pid=usbtmc_test.PID):
        self.idVendor, self.idProduct = vid, pid

    def get_active_configuration(self):
        return {(0, 0): FakeItf([FakeEp(0x01, 2), FakeEp(0x81, 2), FakeEp(0x82, 3)])}

    def is_kernel_driver_active(self, itf):
        return False


class Find(unittest.TestCase):
    def setUp(self):
        td = TemporaryDirectory()
        self.addCleanup(td.cleanup)
        self.root = Path(td.name)
        usbtest_harness.patch(self, usbtmc_test, '_SYS_USB', self.root)
        self.devices, self.opened = {}, []

        def find(**kw):
            self.assertEqual(sorted(kw), ['address', 'bus'], 'opened by bus/address only')
            self.opened.append((kw['bus'], kw['address']))
            return self.devices.get((kw['bus'], kw['address']))
        usbtest_harness.patch(self, usbtmc_test.usb.core, 'find', find)
        usbtest_harness.patch(self, usbtmc_test.usb.util, 'get_string',
                              lambda *a: self.fail('a live string read opens the device'))
        usbtest_harness.patch(self, usbtmc_test.usb.util, 'claim_interface', lambda dev, itf: None)
        self.node('usb1', '1d6b', '0002', 'root hub', 1, 1)
        (self.root / '1-9:1.0').mkdir()   # an interface: no idVendor

    def node(self, name, vid, pid, serial, bus, address):
        d = self.root / name
        d.mkdir()
        for attr, val in (('idVendor', vid), ('idProduct', pid), ('busnum', bus), ('devnum', address)):
            (d / attr).write_text(f'{val}\n')
        if serial is None:
            os.mkfifo(d / 'serial')   # a read that blocks, as a wedged peer's device lock does
            self.addCleanup(self.release, d / 'serial')
        else:
            (d / 'serial').write_text(f'{serial}\n')
        return d

    @staticmethod
    def release(fifo):
        """Unblock the reader thread left in open(); ENXIO when none is waiting."""
        try:
            os.close(os.open(fifo, os.O_WRONLY | os.O_NONBLOCK))
        except OSError:
            pass

    def dut(self, timeout=0):
        d = usbtmc_test.Dut.__new__(usbtmc_test.Dut)
        d.uid = 'U'
        d.find(timeout)
        return d

    def test_a_blocked_peer_costs_a_bounded_read_and_is_never_opened(self):
        self.node('1-1', 'cafe', '401c', None, 1, 2)
        self.node('1-2', 'cafe', '401c', 'OTHER', 1, 3)
        self.node('1-3', 'cafe', '401c', 'U', 1, 4)
        self.devices[(1, 4)] = FakeDev()
        t = time.monotonic()
        d = self.dut()
        self.assertLess(time.monotonic() - t, 5)
        self.assertEqual(self.opened, [(1, 4)])
        self.assertEqual((d.ep_out, d.ep_in, d.ep_int, d.mps), (0x01, 0x81, 0x82, 64))

    def test_an_ambiguous_serial_is_never_opened(self):
        self.node('1-1', 'cafe', '401c', 'U', 1, 2)
        self.node('2-1', 'cafe', '401c', 'U', 2, 2)
        with self.assertRaisesRegex(RuntimeError, r'ambiguous: 2 devices'):
            self.dut()
        self.assertEqual(self.opened, [])

    def test_a_device_gone_or_replaced_after_the_scan_is_not_found(self):
        self.node('1-1', 'cafe', '401c', 'U', 1, 2)
        with self.assertRaisesRegex(RuntimeError, 'serial U not found'):
            self.dut()
        self.devices[(1, 2)] = FakeDev(vid=0x1234)
        with self.assertRaisesRegex(RuntimeError, 'serial U not found'):
            self.dut()

    def test_a_device_that_appears_later_is_found(self):
        usbtest_harness.patch(self, usbtmc_test.time, 'sleep',
                              lambda s: self.node('1-1', 'cafe', '401c', 'U', 1, 2) and None)
        self.devices[(1, 2)] = FakeDev()
        self.assertEqual(self.dut(timeout=5).itf, 0)


if __name__ == '__main__':
    unittest.main()
