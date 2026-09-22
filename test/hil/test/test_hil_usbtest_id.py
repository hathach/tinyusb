#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""usbtest's dynamic id is registered once per run, before any battery: a new_id write runs
driver_attach, which takes the device lock of every cafe:4010 interface on the rig and so
waits for every peer battery's in-flight testusb case. Stubbed sysfs; the kernel side is
proved by a rig run."""
import ast
import io
import os
import sys
import threading
import time
import types
import unittest
from contextlib import redirect_stderr, redirect_stdout
from pathlib import Path
from tempfile import TemporaryDirectory

TEST_DIR = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(TEST_DIR))
# hil_test imports pyserial, which the bare pre-commit runner lacks; nothing here opens a port
serial_stub = types.ModuleType('serial')
serial_stub.Serial = type('Serial', (), {})
serial_stub.SerialException = type('SerialException', (Exception,), {})
serial_stub.SerialTimeoutException = type('SerialTimeoutException', (Exception,), {})
sys.modules.setdefault('serial', serial_stub)
import usbtest  # noqa: E402

HIL_DIR = Path(TEST_DIR).parent
ENTRY = f'{usbtest.VID} {usbtest.PID} 0 {usbtest.GZ_REF}'


class FakeDriver:
    """A usbtest driver dir whose new_id behaves like usb_store_new_id: every write appends
    a listing line, with no duplicate check."""

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
        for name, value in (('DRIVER', self.driver), ('SYS_USB', self.sys_usb),
                            ('REGISTER_LOCK_DIR', str(self.locks)),
                            ('sysfs_write', self.sysfs_write), ('_sudo_soft', self.sudo_soft)):
            test.addCleanup(setattr, usbtest, name, getattr(usbtest, name))
            setattr(usbtest, name, value)

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
        return intf


class RegisterOnce(unittest.TestCase):
    def test_writes_the_profiled_entry_when_none_is_listed(self):
        fake = FakeDriver(self)
        usbtest.register_usbtest_id()
        self.assertEqual(fake.writes, [('new_id', ENTRY)])

    def test_an_existing_entry_means_no_write(self):
        fake = FakeDriver(self, listed='cafe 4010\n')
        usbtest.register_usbtest_id()
        self.assertEqual(fake.writes, [])

    def test_idempotent(self):
        fake = FakeDriver(self)
        for _ in range(3):
            usbtest.register_usbtest_id()
        self.assertEqual(fake.writes, [('new_id', ENTRY)])

    def test_our_entry_after_another_id_counts(self):
        fake = FakeDriver(self, listed='0525 a4a0\ncafe 4010\n')
        usbtest.register_usbtest_id()
        self.assertEqual(fake.writes, [])

    def test_another_ids_entry_does_not_count(self):
        fake = FakeDriver(self, listed='0525 a4a0\ncafe 4011\ncafe 4010 ff\n')
        usbtest.register_usbtest_id()
        self.assertEqual(fake.writes, [('new_id', ENTRY)])

    def test_loads_the_module_when_missing(self):
        fake = FakeDriver(self, loaded=False)
        usbtest.register_usbtest_id()
        self.assertEqual(fake.modprobe, [['modprobe', 'usbtest']])
        self.assertEqual(fake.writes, [('new_id', ENTRY)])

    def test_never_removes_an_id(self):
        fake = FakeDriver(self, listed='cafe 4010\n')
        usbtest.register_usbtest_id()
        fake2 = FakeDriver(self)
        usbtest.register_usbtest_id()
        self.assertFalse([w for w in fake.writes + fake2.writes if w[0] == 'remove_id'])

    def test_two_initializers_make_one_entry(self):
        fake = FakeDriver(self)
        fake.write_delay = 0.3   # both would pass an unlocked check before either wrote
        errors = []

        def init():
            try:
                usbtest.register_usbtest_id()
            except BaseException as e:   # noqa: BLE001 - surfaced below
                errors.append(e)
        threads = [threading.Thread(target=init) for _ in range(2)]
        for t in threads:
            t.start()
        for t in threads:
            t.join(10)
        self.assertEqual(errors, [])
        self.assertEqual(fake.writes, [('new_id', ENTRY)])
        self.assertEqual((fake.driver / 'new_id').read_text(), 'cafe 4010\n')

    def test_the_lock_is_bounded_and_released(self):
        import fcntl
        fake = FakeDriver(self)
        fake.locks.mkdir()
        holder = open(fake.locks / usbtest.REGISTER_LOCK_NAME, 'a')
        self.addCleanup(holder.close)
        fcntl.flock(holder, fcntl.LOCK_EX)
        self.addCleanup(setattr, usbtest, 'REGISTER_LOCK_TIMEOUT', usbtest.REGISTER_LOCK_TIMEOUT)
        usbtest.REGISTER_LOCK_TIMEOUT = 0.3
        outcome = []

        def contend():
            try:
                usbtest.register_usbtest_id()
            except SystemExit as e:
                outcome.append(str(e))
        t0 = time.monotonic()
        t = threading.Thread(target=contend, daemon=True)
        t.start()
        t.join(5)
        self.assertFalse(t.is_alive(), 'the registration lock wait is not bounded')
        self.assertLess(time.monotonic() - t0, 3)
        self.assertEqual(len(outcome), 1)
        self.assertIn('usbtest id registration lock', outcome[0])
        self.assertEqual(fake.writes, [])
        fcntl.flock(holder, fcntl.LOCK_UN)
        usbtest.register_usbtest_id()
        probe = open(fake.locks / usbtest.REGISTER_LOCK_NAME, 'a')
        self.addCleanup(probe.close)
        fcntl.flock(probe, fcntl.LOCK_EX | fcntl.LOCK_NB)   # raises if still held


class BindOwnInterface(unittest.TestCase):
    def test_already_bound_returns_at_once(self):
        fake = FakeDriver(self, listed='cafe 4010\n')
        fake.interface('1-1', driver='usbtest')
        usbtest.bind_usbtest({'sysname': '1-1'})
        self.assertEqual(fake.writes, [])

    def test_a_foreign_driver_is_unbound_and_only_our_interface_bound(self):
        fake = FakeDriver(self, listed='cafe 4010\n')
        intf = fake.interface('1-1', driver='cdc_acm')
        fake.interface('2-1', driver='cdc_acm')

        def write(path, data, check=True):
            # resolved now: the interface's driver link names the file the kernel gets
            fake.writes.append((str(Path(path).resolve()), data))
            if Path(path).name == 'unbind':
                (intf / 'driver').unlink()
            elif Path(path).name == 'bind':
                (intf / 'driver').symlink_to(fake.driver)
            return True
        self.addCleanup(setattr, usbtest, 'sysfs_write', usbtest.sysfs_write)
        usbtest.sysfs_write = write
        foreign = (intf / 'driver').resolve()
        usbtest.bind_usbtest({'sysname': '1-1'})
        self.assertEqual(fake.writes, [(str(foreign / 'unbind'), '1-1:1.0'),
                                       (str(fake.driver.resolve() / 'bind'), '1-1:1.0')])

    def test_bind_never_touches_the_registry(self):
        src = (HIL_DIR / 'usbtest.py').read_text()
        fn = next(n for n in ast.walk(ast.parse(src))
                  if isinstance(n, ast.FunctionDef) and n.name == 'bind_usbtest')
        body = ast.unparse(fn)
        for word in ('new_id', 'remove_id', 'modprobe'):
            self.assertNotIn(word, body)


class Callers(unittest.TestCase):
    def test_standalone_usbtest_registers_before_binding(self):
        src = (HIL_DIR / 'usbtest.py').read_text()
        main = next(n for n in ast.walk(ast.parse(src))
                    if isinstance(n, ast.FunctionDef) and n.name == 'main')
        # a statement of main's own body, so no condition can skip it, and ahead of the bind
        top = [ast.unparse(n.value.func) for n in main.body
               if isinstance(n, ast.Expr) and isinstance(n.value, ast.Call)]
        self.assertIn('register_usbtest_id', top)
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
        for name in ('test_only', 'board_test', 'log_line'):
            self.addCleanup(setattr, ht, name, getattr(ht, name))
        ht.test_only = list(test_only)
        ht.board_test = dict(board_test or {})
        logged = []
        ht.log_line = logged.append
        ht.register_usbtest_if_selected(boards, Path('.'), fresh=True)
        return logged

    def test_the_roster_capability_selects_it(self):
        self.select([{'name': 'a', 'tests': {'host': True}}, {'name': 'b', 'tests': {'device': True}}])
        self.assertEqual(self.calls, ['register'])

    def test_a_roster_skip_removes_it_without_logging(self):
        logged = self.select([{'name': 'a', 'tests': {'device': True, 'skip': ['device/usbtest']}}])
        self.assertEqual(self.calls, [])
        self.assertEqual(logged, [], 'the selection check logged a Skip line of its own')

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
            with self.assertRaises(SystemExit) as cm:
                self.hil_test.register_usbtest_if_selected(
                    [{'name': 'a', 'tests': {'only': ['device/usbtest']}}], Path(td), fresh=True)
            report = (Path(td) / self.hil_test.hil_report.REPORT_JSON).read_text()
        self.assertEqual(cm.exception.code, 1)
        self.assertIn('cannot load usbtest module', out.getvalue())
        self.assertIn('cannot load usbtest module', report)

    def test_called_before_the_build_and_the_pool(self):
        src = (HIL_DIR / 'hil_test.py').read_text()
        main = next(n for n in ast.walk(ast.parse(src))
                    if isinstance(n, ast.FunctionDef) and n.name == 'main')
        order = [ast.unparse(n.func) for n in ast.walk(main) if isinstance(n, ast.Call)]
        lines = {name: min(n.lineno for n in ast.walk(main) if isinstance(n, ast.Call)
                           and ast.unparse(n.func) == name)
                 for name in ('register_usbtest_if_selected', 'build_board', '_start_pool')}
        self.assertIn('register_usbtest_if_selected', order)
        self.assertLess(lines['register_usbtest_if_selected'], lines['build_board'])
        self.assertLess(lines['register_usbtest_if_selected'], lines['_start_pool'])


if __name__ == '__main__':
    unittest.main()
