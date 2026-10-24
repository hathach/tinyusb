#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""usbtest's dynamic id is registered once per run, before any battery: a new_id write runs
driver_attach, which takes the device lock of every cafe:4010 interface on the rig and so
waits for every peer battery's in-flight testusb case. The registration, its lock and the
binding are agentrc's hil_usbtest, tested there; these cover what usbtest.py hands it and
when. Stubbed sysfs; the kernel side is proved by a rig run."""
import ast
import io
import json
import os
import sys
import time
import types
import unittest
from contextlib import redirect_stderr, redirect_stdout
from pathlib import Path
from tempfile import TemporaryDirectory

TEST_DIR = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(TEST_DIR))
import usbtest_harness  # noqa: E402 - stubs serial before hil_test is imported
import usbtest  # noqa: E402
from helper import hil_lock  # noqa: E402

HIL_DIR = Path(TEST_DIR).parent
ENTRY = f'{usbtest.VID} {usbtest.PID} 0 {usbtest.GZ_REF}'


class FakeDriver:
    """A usbtest driver dir whose new_id behaves like usb_store_new_id: every write appends
    a listing line, with no duplicate check. Patched into the provider usbtest.py loaded."""

    def __init__(self, test, loaded=True, listed=''):
        td = TemporaryDirectory()
        test.addCleanup(td.cleanup)
        self.root = Path(td.name)
        self.driver = self.root / 'drivers/usbtest'
        self.sys_usb = self.root / 'devices'
        self.sys_usb.mkdir(parents=True)
        self.locks = self.root / 'locks'
        self.listed = listed
        self.writes = []
        self.write_delay = 0.0
        self.modprobe = []
        if loaded:
            self.load()
        self.test = test
        for name, value in (('DRIVER', self.driver), ('SYS_USB', self.sys_usb),
                            ('sysfs_write', self.sysfs_write), ('_sudo_soft', self.sudo_soft)):
            self.patch(name, value)
        test.addCleanup(setattr, hil_lock, 'BOARD_LOCK_DIR', hil_lock.BOARD_LOCK_DIR)
        hil_lock.BOARD_LOCK_DIR = str(self.locks)

    def patch(self, name, value, module=None):
        module = module or usbtest.hil_usbtest
        self.test.addCleanup(setattr, module, name, getattr(module, name))
        setattr(module, name, value)

    def load(self):
        self.driver.mkdir(parents=True, exist_ok=True)
        (self.driver / 'new_id').write_text(self.listed)

    def sudo_soft(self, cmd, **kw):
        self.modprobe.append(cmd)
        if cmd == ['modprobe', 'usbtest']:
            self.load()
        return types.SimpleNamespace(returncode=0, stderr='', stdout='')

    def sysfs_write(self, path, data, check=True):
        time.sleep(self.write_delay)
        path = Path(path)
        self.writes.append((path.name, data))
        if path == self.driver / 'new_id':
            f = self.driver / 'new_id'
            f.write_text(f.read_text() + ' '.join(data.split()[:2]) + '\n')
        return True

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
    def test_registers_our_id_with_the_gadget_zero_profile_in_the_board_lock_dir(self):
        fake = FakeDriver(self)
        for _ in range(2):
            usbtest.register_usbtest_id()
        self.assertEqual(fake.writes, [('new_id', ENTRY)])
        self.assertEqual([p.name for p in fake.locks.iterdir()], [usbtest.hil_usbtest.REGISTER_LOCK_NAME])

    def test_binds_interface_0_of_the_device(self):
        fake = FakeDriver(self, listed='cafe 4010\n')
        intf = fake.interface('1-1')

        def write(path, data, check=True):
            fake.writes.append((Path(path).name, data))
            (intf / 'driver').symlink_to(fake.driver)
            return True
        fake.patch('sysfs_write', write)
        usbtest.bind_usbtest({'sysname': '1-1'})
        self.assertEqual(fake.writes, [('bind', '1-1:1.0')])


class StandaloneLeavesThePeersAlone(unittest.TestCase):
    """A standalone battery writes no registry entry and
    unbinds nothing: a peer mid-battery keeps its binding and the shared id stays."""

    def run_main(self, *extra):
        fake = FakeDriver(self, listed='cafe 4010\n')
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

    def test_a_full_run_leaves_the_id_and_both_bindings(self):
        fake, ours, peer = self.run_main()
        self.assertEqual(fake.writes, [])
        self.assertTrue((ours / 'driver').is_symlink() and (peer / 'driver').is_symlink())
        self.assertEqual((fake.driver / 'new_id').read_text(), 'cafe 4010\n')


class SudoTestusbRefusesRecovery(unittest.TestCase):
    """Under sudo a HUNG case's reap proves only that the wrapper exited, so a battery that
    was asked to recover must refuse to start rather than promise what it cannot confirm."""

    def run_main(self, *extra):
        ran = []
        usbtest_harness.stub_device(self, usbtest, lambda num, d, tu, quick, timeout:
                                    ran.append(num) or {'num': num, 'name': 'x', 'params': '',
                                                        'status': 'PASS', 'detail': ''})
        usbtest_harness.patch(self, usbtest, 'needs_sudo', lambda node: True)
        def mutated(*a):
            raise AssertionError('the refusal must come before any registry or bind write')

        usbtest_harness.patch(self, usbtest, 'register_usbtest_id', mutated)
        usbtest_harness.patch(self, usbtest, 'bind_usbtest', mutated)
        usbtest_harness.argv(self, *extra)
        with redirect_stdout(usbtest_harness.Out()), redirect_stderr(io.StringIO()):
            try:
                usbtest.main()
            except SystemExit as e:
                return ran, str(e.code)
        return ran, None

    def test_a_recovery_request_refuses_before_any_case(self):
        ran, err = self.run_main('--recover-board', '{"name": "b", "flasher": {"name": "openocd"}}')
        self.assertEqual(ran, [])
        self.assertIn('cannot be confirmed', err)

    def test_without_a_recovery_request_sudo_still_runs(self):
        usbtest_harness.patch(self, usbtest, 'register_usbtest_id', lambda: None)
        usbtest_harness.patch(self, usbtest, 'bind_usbtest', lambda d: None)
        ran = []
        usbtest_harness.stub_device(self, usbtest, lambda num, d, tu, quick, timeout:
                                    ran.append(num) or {'num': num, 'name': 'x', 'params': '',
                                                        'status': 'PASS', 'detail': ''})
        usbtest_harness.patch(self, usbtest, 'needs_sudo', lambda node: True)
        usbtest_harness.argv(self)
        with redirect_stdout(usbtest_harness.Out()), redirect_stderr(io.StringIO()):
            usbtest.main()
        self.assertEqual(ran, [1])


class SudoAfterAdmission(unittest.TestCase):
    """A node that turns root-only after main's check (a re-enumeration between cases) must
    still not get a sudo-wrapped testusb under a recovery request: run_case hands main's
    decision to the provider on every case."""

    def setUp(self):
        usbtest_harness.patch(self, usbtest.hil_usbtest, 'needs_sudo', lambda node: True)
        self.addCleanup(setattr, usbtest, 'sudo_forbidden', usbtest.sudo_forbidden)

    def test_run_case_refuses_sudo_when_forbidden(self):
        usbtest.sudo_forbidden = True
        launched = []
        usbtest_harness.patch(self, usbtest.hil_usbtest.subprocess, 'Popen',
                              lambda *a, **k: launched.append(a) or (_ for _ in ()).throw(AssertionError))
        r = usbtest.run_case(1, dict(usbtest_harness.DEV), '/bin/true', False, 5)
        self.assertEqual((r['status'], launched), ('FAIL', []))
        self.assertEqual((r['num'], r['name']), (1, usbtest.CASE_NAMES[1]))
        self.assertIn('refusing a sudo-wrapped testusb', r['detail'])


class Callers(unittest.TestCase):
    def test_standalone_usbtest_registers_before_binding(self):
        src = (HIL_DIR / 'usbtest.py').read_text()
        main = next(n for n in ast.walk(ast.parse(src))
                    if isinstance(n, ast.FunctionDef) and n.name == 'main')
        # a statement of main's own body, so no condition can skip it, and ahead of the bind
        reg = next(n.lineno for n in main.body if isinstance(n, ast.Expr)
                   and ast.unparse(n.value) == 'register_usbtest_id()')
        bind = min(n.lineno for n in ast.walk(main) if isinstance(n, ast.Call)
                   and ast.unparse(n.func) == 'bind_usbtest')
        self.assertLess(reg, bind)


class HilTestRegistersBeforeThePool(unittest.TestCase):
    """hil_test registers once, before the pool, only when a selected board runs
    device/usbtest; a failure stops the run with a report before any battery."""

    def setUp(self):
        import hil_test
        self.hil_test = hil_test
        self.calls = []
        self.addCleanup(setattr, usbtest, 'register_usbtest_id', usbtest.register_usbtest_id)
        usbtest.register_usbtest_id = lambda: self.calls.append('register')

    def select(self, boards, test_only=(), board_test=None):
        ht = self.hil_test
        for name in ('test_only', 'board_test'):
            self.addCleanup(setattr, ht, name, getattr(ht, name))
        ht.test_only = list(test_only)
        ht.board_test = dict(board_test or {})
        ht.register_usbtest_if_selected(boards, Path('.'), fresh=True)

    def test_the_roster_capability_selects_it(self):
        self.select([{'name': 'a', 'tests': {'host': True}}, {'name': 'b', 'tests': {'device': True}}])
        self.assertEqual(self.calls, ['register'])

    def test_a_roster_skip_removes_it(self):
        self.select([{'name': 'a', 'tests': {'device': True, 'skip': ['device/usbtest']}}])
        self.assertEqual(self.calls, [])

    def test_a_t_list_without_it(self):
        self.select([{'name': 'a', 'tests': {'device': True}}], test_only=['device/cdc_msc'])
        self.assertEqual(self.calls, [])

    def test_a_t_list_with_it(self):
        self.select([{'name': 'a', 'tests': {'device': True}}], test_only=['device/usbtest'])
        self.assertEqual(self.calls, ['register'])

    def test_a_bt_list_selects_it(self):
        self.select([{'name': 'a', 'tests': {'host': True}}], board_test={'a': ['device/usbtest']})
        self.assertEqual(self.calls, ['register'])

    def test_no_board_runs_it(self):
        self.select([{'name': 'a', 'tests': {'host': True}}])
        self.assertEqual(self.calls, [])

    def test_a_failure_stops_the_run_with_a_report(self):
        def fail():
            raise SystemExit('cannot load usbtest module: nope')
        usbtest.register_usbtest_id = fail
        with TemporaryDirectory() as td, redirect_stdout(io.StringIO()) as out:
            previous = Path(td) / self.hil_test.hil_report.REPORT_JSON
            previous.write_text('{"rows": [{"board": "stale", "cells": {"device/usbtest": "30/30"}}]}')
            with self.assertRaises(SystemExit) as cm:
                self.hil_test.register_usbtest_if_selected(
                    [{'name': 'a', 'tests': {'only': ['device/usbtest']}}], Path(td), fresh=True)
            report = previous.read_text()
        self.assertEqual(cm.exception.code, 1)
        self.assertIn('cannot load usbtest module', out.getvalue())
        self.assertIn('cannot load usbtest module', report)
        self.assertNotIn('stale', report)   # the previous run's table does not survive
        # a selected board was never tested: aborted, so the shared rules read it as a blocker
        self.assertEqual(json.loads(report)['_hil']['termination'], 'aborted')

    def test_called_before_the_build_and_the_pool(self):
        src = (HIL_DIR / 'hil_test.py').read_text()
        main = next(n for n in ast.walk(ast.parse(src))
                    if isinstance(n, ast.FunctionDef) and n.name == 'main')
        lines = {name: min((n.lineno for n in ast.walk(main) if isinstance(n, ast.Call)
                            and ast.unparse(n.func) == name), default=None)
                 for name in ('register_usbtest_if_selected', 'build_board', '_start_pool')}
        self.assertIsNotNone(lines['register_usbtest_if_selected'])
        self.assertLess(lines['register_usbtest_if_selected'], lines['build_board'])
        self.assertLess(lines['register_usbtest_if_selected'], lines['_start_pool'])


if __name__ == '__main__':
    unittest.main()
