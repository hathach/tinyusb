#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""usbtest's dynamic id is registered once per run, before any battery: a new_id write runs
driver_attach, which takes the device lock of every cafe:4010 interface on the rig and so
waits for every peer battery's in-flight testusb case. Stubbed sysfs; the kernel side is
proved by a rig run."""
import ast
import errno
import fcntl
import io
import os
import select
import signal
import subprocess
import sys
import threading
import time
import types
import unittest
import unittest.mock
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
        self.test = test
        for name, value in (('DRIVER', self.driver), ('SYS_USB', self.sys_usb),
                            ('sysfs_write', self.sysfs_write), ('_sudo_soft', self.sudo_soft)):
            self.patch(name, value)
        test.addCleanup(setattr, hil_lock, 'BOARD_LOCK_DIR', hil_lock.BOARD_LOCK_DIR)
        hil_lock.BOARD_LOCK_DIR = str(self.locks)

    def patch(self, name, value, module=None):
        module = module or usbtest
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


class RegisterOnce(unittest.TestCase):
    def test_writes_the_profiled_entry_only_when_the_id_is_not_listed(self):
        for listed, writes in (('', [('new_id', ENTRY)]),
                               ('cafe 4010\n', []),
                               ('0525 a4a0\ncafe 4010\n', []),
                               ('0525 a4a0\ncafe 4011\ncafe 4010 ff\n', [('new_id', ENTRY)])):
            with self.subTest(listed=listed):
                fake = FakeDriver(self, listed=listed)
                usbtest.register_usbtest_id()
                self.assertEqual(fake.writes, writes)

    def test_idempotent(self):
        fake = FakeDriver(self)
        for _ in range(3):
            usbtest.register_usbtest_id()
        self.assertEqual(fake.writes, [('new_id', ENTRY)])

    def test_an_unusable_lock_dir_is_a_named_exit(self):
        fake = FakeDriver(self)
        fake.locks.write_text('')   # a file where the lock dir should be
        with self.assertRaises(SystemExit) as cm:
            usbtest.register_usbtest_id()
        self.assertIn('registration lock', str(cm.exception))
        self.assertEqual(fake.writes, [])

    def test_a_refused_flock_is_a_named_exit(self):
        fake = FakeDriver(self)

        def no_locks(fh, op):
            raise OSError(errno.ENOLCK, 'No locks available')
        with unittest.mock.patch.object(fcntl, 'flock', no_locks):
            with self.assertRaises(SystemExit) as cm:
                usbtest.register_usbtest_id()
        self.assertIn('No locks available', str(cm.exception))
        self.assertEqual(fake.writes, [])

    def test_a_peer_mid_write_holds_the_lock_until_its_attach_returns(self):
        # the kernel lists the id BEFORE driver_attach: a second initializer must not trust
        # the listing while the first's write is still in flight
        fake = FakeDriver(self)
        published = threading.Event()
        release = threading.Event()
        real = fake.sysfs_write

        def slow_write(path, data, check=True):
            real(path, data, check)      # the fake lists the id now, as the kernel does
            published.set()
            release.wait(5)              # ... and driver_attach is still running
            return True
        fake.patch('sysfs_write', slow_write)
        second_done = threading.Event()
        first = threading.Thread(target=usbtest.register_usbtest_id, daemon=True)
        second = threading.Thread(target=lambda: (usbtest.register_usbtest_id(), second_done.set()),
                                  daemon=True)

        def reap():   # runs first among the cleanups: before the patches and the temp dir go
            release.set()
            started = [t for t in (first, second) if t.ident is not None]
            for t in started:
                t.join(5)
            self.assertFalse([t for t in started if t.is_alive()], 'initializer thread outlived the test')
        self.addCleanup(reap)
        first.start()
        self.assertTrue(published.wait(5))
        second.start()
        self.assertFalse(second_done.wait(0.5), 'a battery would start beside the attach')
        release.set()
        first.join(5)
        self.assertTrue(second_done.wait(5))
        self.assertEqual(fake.writes, [('new_id', ENTRY)])

    def test_loads_the_module_when_missing(self):
        fake = FakeDriver(self, loaded=False)
        usbtest.register_usbtest_id()
        self.assertEqual(fake.modprobe, [['modprobe', 'usbtest']])
        self.assertEqual(fake.writes, [('new_id', ENTRY)])

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


class AKilledHolderReleasesTheLock(unittest.TestCase):
    """The registration flock dies with its holder, however the holder dies: the next
    initializer registers at once, against the same lock file."""

    def test_the_next_initializer_registers_without_waiting(self):
        fake = FakeDriver(self)
        fake.locks.mkdir()
        lock = fake.locks / usbtest.REGISTER_LOCK_NAME
        ready_r, ready_w = os.pipe()
        self.addCleanup(os.close, ready_r)
        try:
            holder = subprocess.Popen(
                [sys.executable, '-c',
                 'import fcntl, os, sys, time\n'
                 'fh = open(sys.argv[1], "a")\n'
                 'fcntl.flock(fh, fcntl.LOCK_EX)\n'
                 'os.write(int(sys.argv[2]), b"1")\n'
                 'time.sleep(60)\n',
                 str(lock), str(ready_w)], pass_fds=(ready_w,))
        finally:
            os.close(ready_w)
        self.addCleanup(lambda: holder.poll() is None and (holder.kill(), holder.wait(5)))
        ready, _, _ = select.select([ready_r], [], [], 10)
        self.assertTrue(ready and os.read(ready_r, 1) == b'1', 'holder never took the lock')
        with open(lock) as fh:   # the child really holds it: the kill below is what frees it
            with self.assertRaises(BlockingIOError):
                fcntl.flock(fh, fcntl.LOCK_EX | fcntl.LOCK_NB)
        inode = lock.stat().st_ino

        holder.send_signal(signal.SIGKILL)
        self.assertEqual(holder.wait(5), -signal.SIGKILL)
        self.addCleanup(setattr, usbtest, 'REGISTER_LOCK_TIMEOUT', usbtest.REGISTER_LOCK_TIMEOUT)
        usbtest.REGISTER_LOCK_TIMEOUT = 2
        with unittest.mock.patch.object(fcntl, 'flock', wraps=fcntl.flock) as flock:
            usbtest.register_usbtest_id()
        attempts = [c for c in flock.call_args_list if c.args[1] & fcntl.LOCK_EX]
        self.assertEqual(len(attempts), 1)   # taken first try: the dead holder's lock is gone
        self.assertEqual(fake.writes, [('new_id', ENTRY)])
        self.assertEqual(lock.stat().st_ino, inode)


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
        fake.patch('sysfs_write', write)
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


class StandaloneLeavesThePeersAlone(unittest.TestCase):
    """A standalone battery, with or without --keep-binding, writes no registry entry and
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

    def test_without_keep_binding(self):
        fake, ours, peer = self.run_main()
        self.assertEqual(fake.writes, [])
        self.assertTrue((ours / 'driver').is_symlink() and (peer / 'driver').is_symlink())
        self.assertEqual((fake.driver / 'new_id').read_text(), 'cafe 4010\n')

    def test_keep_binding_is_still_accepted(self):
        fake, _ours, peer = self.run_main('--keep-binding')
        self.assertEqual(fake.writes, [])
        self.assertTrue((peer / 'driver').is_symlink())


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
