#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# usbtmc_test.Dut.find against a fake sysfs tree and a fake pyusb: every USBTMC DUT is cafe:401c, so the
# requested serial is resolved from sysfs, bounded, before the one matching device is opened, and read
# again from that device before it is claimed.
# SessionChild: the sysfs walk and bounded serial read session_child.py gives usbtmc_test and mtp_test.
# Run directly:
#   python3 test/hil/test/test_hil_usbtmc.py
import os
import sys
import time
import unittest
from pathlib import Path
from tempfile import TemporaryDirectory

if sys.platform == 'win32':
    raise unittest.SkipTest("the fake sysfs tree needs os.mkfifo and ':' in node names: POSIX only")

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

import session_child     # noqa: E402
import usbtest_harness   # noqa: E402
import usbtmc_test       # noqa: E402


class FakeEp:
    def __init__(self, addr, attrs):
        self.bEndpointAddress, self.bmAttributes, self.wMaxPacketSize = addr, attrs, 64


class FakeItf(list):
    bInterfaceNumber = 0


class FakeDev:
    iSerialNumber = 3

    def __init__(self, vid=usbtmc_test.VID, pid=usbtmc_test.PID, serial='U'):
        self.idVendor, self.idProduct, self.serial = vid, pid, serial
        self.detached = []

    def get_string(self, index):
        assert index == self.iSerialNumber
        if self.serial is None:
            raise usbtmc_test.usb.core.USBError('Pipe error')
        return self.serial

    def get_active_configuration(self):
        return {(0, 0): FakeItf([FakeEp(0x01, 2), FakeEp(0x81, 2), FakeEp(0x82, 3)])}

    def is_kernel_driver_active(self, itf):
        return True

    def detach_kernel_driver(self, itf):
        self.detached.append(itf)


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
        # reached only through find, which opens just the sysfs match: no neighbour's string is read
        usbtest_harness.patch(self, usbtmc_test.usb.util, 'get_string', lambda dev, index: dev.get_string(index))
        self.claimed, self.disposed = [], []
        usbtest_harness.patch(self, usbtmc_test.usb.util, 'claim_interface', lambda dev, itf: self.claimed.append(dev))
        usbtest_harness.patch(self, usbtmc_test.usb.util, 'dispose_resources', self.disposed.append)
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

    def test_another_board_that_took_the_address_is_never_claimed(self):
        # it keeps the scanned bus/address and vid:pid; only its own serial, unreadable or another, tells it apart
        self.node('1-1', 'cafe', '401c', 'U', 1, 2)
        impostors = [FakeDev(serial='OTHER'), FakeDev(serial=None)]
        board = FakeDev()
        queue = iter(impostors[1:] + [board])
        self.devices[(1, 2)] = impostors[0]
        usbtest_harness.patch(self, usbtmc_test.time, 'sleep',
                              lambda s: self.devices.__setitem__((1, 2), next(queue)))
        d = self.dut(timeout=5)
        self.assertIs(d.dev, board)
        self.assertEqual(self.claimed, [board])
        self.assertEqual(board.detached, [0])
        self.assertEqual([i.detached for i in impostors], [[], []])
        self.assertEqual(self.disposed, impostors)

    def test_a_device_that_appears_later_is_found(self):
        usbtest_harness.patch(self, usbtmc_test.time, 'sleep',
                              lambda s: self.node('1-1', 'cafe', '401c', 'U', 1, 2) and None)
        self.devices[(1, 2)] = FakeDev()
        self.assertEqual(self.dut(timeout=5).itf, 0)


class SessionChild(unittest.TestCase):
    def setUp(self):
        td = TemporaryDirectory()
        self.addCleanup(td.cleanup)
        self.root = Path(td.name)

    def node(self, name, serial='U', bus=1, address=2):
        d = self.root / name
        d.mkdir()
        for attr, val in (('idVendor', 'cafe'), ('idProduct', '401c'), ('busnum', bus), ('devnum', address)):
            (d / attr).write_text(f'{val}\n')
        if serial is None:
            os.mkfifo(d / 'serial')
        else:
            (d / 'serial').write_text(f'{serial}\n')
        return d

    def find(self):
        return session_child.find_devices(self.root, 'cafe', '401c', 'U')

    def strand(self, fifo):
        """A first read of `fifo` given up on: its reader stays blocked in open() until released."""
        self.addCleanup(Find.release, fifo)
        self.assertIsNone(session_child.bounded_read(fifo, grace=0.1))
        return session_child._stranded[str(fifo)][1]

    def test_root_hubs_and_interfaces_are_skipped_before_any_read(self):
        self.node('usb1')
        self.node('1-9:1.0')
        self.node('1-1')
        read = []
        attr = session_child._attr
        usbtest_harness.patch(self, session_child, '_attr', lambda dev, name: read.append(dev) or attr(dev, name))
        self.assertEqual(self.find(), [(1, 2)])
        self.assertEqual({Path(d).name for d in read}, {'1-1'})

    def test_a_blocked_serial_gets_no_second_reader_until_the_first_returns(self):
        fifo = self.node('1-1', serial=None) / 'serial'
        stuck = self.strand(fifo)
        self.assertEqual(self.find(), [])
        self.assertIs(session_child._stranded[str(fifo)][1], stuck, 'a second reader joined the blocked one')
        Find.release(fifo)
        stuck.join(2)
        self.assertFalse(stuck.is_alive())
        self.assertIsNone(session_child.bounded_read(fifo, grace=0.1))   # no writer again: a fresh read blocks
        self.assertIsNot(session_child._stranded[str(fifo)][1], stuck, 'not retried after the reader finished')

    def test_a_re_enumerated_node_is_read_while_the_old_reader_is_still_blocked(self):
        d = self.node('1-1', serial=None)
        stuck = self.strand(d / 'serial')
        (d / 'serial').rename(d / 'serial.old')   # the old node, its reader still blocked on it
        self.addCleanup(Find.release, d / 'serial.old')
        (d / 'serial').write_text('U\n')
        self.assertEqual(self.find(), [(1, 2)])
        self.assertTrue(stuck.is_alive())

    def test_a_serial_read_across_a_re_enumeration_is_rejected(self):
        d = self.node('1-1')

        def read_then_re_enumerate(path, grace=None):
            d.rename(self.root / 'gone')
            self.node('1-1', address=3)
            return 'U'
        usbtest_harness.patch(self, session_child, 'bounded_read', read_then_re_enumerate)
        self.assertEqual(self.find(), [], 'serial of one enumeration paired with the address of the next')


if __name__ == '__main__':
    unittest.main()
