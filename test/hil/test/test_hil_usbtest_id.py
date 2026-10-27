#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""usbtest.py's binding: the device is Gadget Zero (0525:a4a0), which usbtest binds by its static
id, so usbtest.py writes no registry entry and only reprobes its own interface. The binding is
agentrc's hil_usbtest, tested there; these cover what usbtest.py hands it. Stubbed sysfs; the
kernel side is proved by a rig run."""
import io
import os
import re
import subprocess
import sys
import unittest
from contextlib import redirect_stderr, redirect_stdout
from pathlib import Path
from tempfile import TemporaryDirectory

TEST_DIR = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(TEST_DIR))
import usbtest_harness  # noqa: E402 - stubs serial before hil_test is imported
import usbtest  # noqa: E402


class FakeDriver:
    """A usbtest driver dir and its interfaces, patched into the provider usbtest.py loaded;
    every sysfs write and child run is recorded."""

    def __init__(self, test):
        td = TemporaryDirectory()
        test.addCleanup(td.cleanup)
        self.root = Path(td.name)
        self.driver = self.root / 'drivers/usbtest'
        self.driver.mkdir(parents=True)
        self.sys_usb = self.root / 'devices'
        self.sys_usb.mkdir()
        self.writes, self.runs = [], []
        self.test = test
        for name, value in (('DRIVER', self.driver), ('SYS_USB', self.sys_usb),
                            ('sysfs_write', self.sysfs_write), ('run', self.run),
                            ('node_writable', lambda node: True)):   # as the udev rule makes it
            self.patch(name, value)

    def patch(self, name, value):
        module = usbtest.hil_usbtest
        self.test.addCleanup(setattr, module, name, getattr(module, name))
        setattr(module, name, value)

    def sysfs_write(self, verb, *args):
        self.writes.append((verb, *args))
        return True

    def run(self, cmd, **kw):
        self.runs.append(cmd)
        intf = self.sys_usb / cmd[-1]
        if not (intf / 'driver').is_symlink():   # the CONNECT probe binds usbtest
            (intf / 'driver').symlink_to(self.driver)
        return subprocess.CompletedProcess(cmd, 0, '', '')

    def interface(self, sysname, driver=None):
        intf = self.sys_usb / f'{sysname}:1.0'
        intf.mkdir()
        if driver:
            d = self.root / 'drivers' / driver
            d.mkdir(parents=True, exist_ok=True)
            (intf / 'driver').symlink_to(d)
            (d / intf.name).symlink_to(intf)   # the driver dir lists what it binds, as sysfs does
        return intf


class Wrappers(unittest.TestCase):
    def test_the_device_is_the_firmwares_gadget_zero_id(self):
        # pinned against the descriptor: usbtest binds only an id in its static table
        src = (Path(TEST_DIR).parents[2] / 'examples/device/usbtest/src/usb_descriptors.c').read_text()
        ids = [int(re.search(rf'\.{f}\s*=\s*(0x[0-9a-fA-F]+)', src).group(1), 16)
               for f in ('idVendor', 'idProduct')]
        self.assertEqual(ids, [0x0525, 0xa4a0])
        self.assertEqual((usbtest.VID, usbtest.PID), ('0525', 'a4a0'))
        from helper import hil_util
        self.assertEqual(hil_util.USBTEST_ID, (usbtest.VID, usbtest.PID))

    def test_reprobes_interface_0_through_the_devices_node(self):
        fake = FakeDriver(self)
        fake.interface('1-1')
        usbtest.bind_usbtest({'sysname': '1-1', 'node': '/dev/bus/usb/001/005'})
        self.assertEqual([c[-3:] for c in fake.runs], [['connect', '/dev/bus/usb/001/005', '1-1:1.0']])
        self.assertEqual(fake.writes, [])


class StandaloneLeavesThePeersAlone(unittest.TestCase):
    """A standalone battery writes no registry entry and
    unbinds nothing: a peer mid-battery keeps its binding."""

    def run_main(self, *extra):
        fake = FakeDriver(self)
        ours = fake.interface('1-1', driver='usbtest')
        peer = fake.interface('2-1', driver='usbtest')
        usbtest_harness.stub_device(self, usbtest, lambda num, d, tu, quick, timeout:
                                    {'num': num, 'name': 'x', 'params': '', 'status': 'PASS', 'detail': ''})
        usbtest_harness.argv(self, *extra)
        # both bindings are visible to a driver-wide unbind loop, so its absence is what is tested
        self.assertEqual(sorted(p.name for p in fake.driver.glob('*:*')), ['1-1:1.0', '2-1:1.0'])
        with redirect_stdout(usbtest_harness.Out()), redirect_stderr(io.StringIO()):
            usbtest.main()
        return fake, ours, peer

    def test_a_full_run_leaves_both_bindings(self):
        fake, ours, peer = self.run_main()
        self.assertEqual((fake.writes, fake.runs), ([], []))
        self.assertTrue((ours / 'driver').is_symlink() and (peer / 'driver').is_symlink())


class UnwritableNodeRefuses(unittest.TestCase):
    """testusb never runs under sudo: a node this user cannot write stops the battery before
    any bind, and one that turns root-only between cases (a re-enumeration) fails that case."""

    def test_main_refuses_before_any_case(self):
        ran = []
        usbtest_harness.stub_device(self, usbtest, lambda num, d, tu, quick, timeout:
                                    ran.append(num) or {'num': num, 'name': 'x', 'params': '',
                                                        'status': 'PASS', 'detail': ''})
        usbtest_harness.patch(self, usbtest.hil_usbtest, 'node_writable', lambda node: False)

        def mutated(*a, **k):
            raise AssertionError('the refusal must come before any module load or CONNECT')

        for name in ('load_usbtest', 'run', '_root_soft'):
            usbtest_harness.patch(self, usbtest.hil_usbtest, name, mutated)
        usbtest_harness.argv(self)
        with redirect_stdout(usbtest_harness.Out()), redirect_stderr(io.StringIO()):
            with self.assertRaises(SystemExit) as cm:
                usbtest.main()
        self.assertEqual(ran, [])
        self.assertIn("is not writable", str(cm.exception.code))
        self.assertIn("udev rule", str(cm.exception.code))

    def test_run_case_fails_without_launching(self):
        usbtest_harness.patch(self, usbtest.hil_usbtest, 'node_writable', lambda node: False)
        launched = []
        usbtest_harness.patch(self, usbtest.hil_usbtest.subprocess, 'Popen',
                              lambda *a, **k: launched.append(a) or (_ for _ in ()).throw(AssertionError))
        r = usbtest.run_case(1, dict(usbtest_harness.DEV), '/bin/true', False, 5)
        self.assertEqual((r['status'], launched), ('FAIL', []))
        self.assertEqual((r['num'], r['name']), (1, usbtest.CASE_NAMES[1]))
        self.assertIn('udev rule', r['detail'])


if __name__ == '__main__':
    unittest.main()
