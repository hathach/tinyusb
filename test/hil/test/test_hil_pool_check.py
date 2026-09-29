#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# hil_pool_check's verdicts with the bus, the flasher and the lock dir faked: no board is
# touched. Run directly:
#   python3 test/hil/test/test_hil_pool_check.py
import os
import sys
import tempfile
import types
import unittest

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from helper import hil_lock, hil_pool_check   # noqa: E402

BOARD = {'name': 'b', 'uid': 'U', 'flasher': {'name': 'openocd', 'uid': 'P'}}


def patch(test, obj, name, value):
    test.addCleanup(setattr, obj, name, getattr(obj, name))
    setattr(obj, name, value)


class Lock(unittest.TestCase):
    def setUp(self):
        td = tempfile.TemporaryDirectory()
        self.addCleanup(td.cleanup)
        self.dir = td.name
        patch(self, hil_lock, 'BOARD_LOCK_DIR', self.dir)

    def test_a_held_board_reports_its_holder(self):
        held = hil_lock.flock_nb('b')
        self.addCleanup(held.close)
        hil_lock.write_record(held, 'hil_test.py')
        got = hil_pool_check.lock_board('b')
        self.assertIsInstance(got, str)
        self.assertIn('hil_test.py', got)
        self.assertFalse(got.startswith('ERROR:'))

    def test_an_unopenable_lock_file_is_an_error_not_a_holder(self):
        os.mkdir(os.path.join(self.dir, 'b.lock'))   # open(O_RDWR) on a dir: EISDIR
        got = hil_pool_check.lock_board('b')
        self.assertTrue(got.startswith('ERROR:'), got)

    def test_a_free_board_is_taken_under_the_protected_reason(self):
        fh = hil_pool_check.lock_board('b')
        self.addCleanup(hil_pool_check.unlock_board, fh)
        self.assertEqual(hil_lock.read_record('b')['reason'], 'pool_check')
        self.assertIn('pool_check', hil_lock.PROTECTED_REASONS)


class DeviceVerdict(unittest.TestCase):
    """check_device: the re-enumerated PID decides between a stale build (warn) and a
    silent flash no-op (fail)."""

    def run_check(self, vidpid, built_this_run):
        patch(self, hil_pool_check, 'get_expected_pid', lambda ex: '4001')
        patch(self, hil_pool_check, 'wait_device',
              lambda *a: ('1-1', vidpid, 2) if vidpid else None)
        builds = {('b', 'device/x'): ('fw', 'ok')} if built_this_run else {}
        patch(self, hil_pool_check, '_builds', builds)
        note, row = [], {'status': 'failed'}
        return hil_pool_check.check_device(BOARD, 'device/x', 'b', 1, note, row), note, row

    def test_the_expected_pid_passes(self):
        ok, note, row = self.run_check('cafe:4001', built_this_run=False)
        self.assertTrue(ok)
        self.assertEqual(note, [])

    def test_a_stale_prebuilt_image_warns(self):
        ok, note, _ = self.run_check('cafe:4010', built_this_run=False)
        self.assertTrue(ok)
        self.assertIn('stale build or silent flash no-op', note[0])

    def test_a_wrong_pid_after_this_runs_build_is_a_silent_no_op(self):
        ok, note, row = self.run_check('cafe:4010', built_this_run=True)
        self.assertFalse(ok)
        self.assertEqual(row['status'], 'flash-failed')
        self.assertIn('silent flash no-op', note[0])

    def test_no_enumeration_fails_without_a_reset_retry(self):
        patch(self, hil_pool_check.hil_flash, 'reset_primitive',
              lambda name: self.fail('the pool check no longer resets a board'))
        ok, _, row = self.run_check(None, built_this_run=False)
        self.assertFalse(ok)
        self.assertIn('not enumerated', row['device'])


class HostVerdict(unittest.TestCase):
    def alive(self, data, flashed_example):
        patch(self, hil_pool_check, 'check_host_serial', lambda board: data)
        note, row = [], {'status': 'failed'}
        return hil_pool_check.host_alive(BOARD, note, row, flashed_example), note, row

    def test_board_test_output_after_an_example_flash_is_a_silent_no_op(self):
        ok, note, row = self.alive(b'Hello from TinyUSB\r\nU', flashed_example=True)
        self.assertFalse(ok)
        self.assertEqual(row['status'], 'flash-failed')

    def test_example_output_is_alive(self):
        ok, _, _ = self.alive(b'TinyUSB Host Example\r\n', flashed_example=True)
        self.assertTrue(ok)

    def test_silence_is_not_alive_and_nothing_is_reflashed(self):
        patch(self, hil_pool_check, 'call_flasher',
              lambda *a: self.fail('the pool check no longer reflashes a silent board'))
        ok, _, _ = self.alive(b'', flashed_example=False)
        self.assertFalse(ok)


class Park(unittest.TestCase):
    """A park is verified: board_test never enumerates, so the device must leave the bus."""

    def park(self, on_bus_after, flash_rc=0):
        state = {'on_bus': True}
        patch(self, hil_pool_check, 'find_device',
              lambda uid, pid: ('1-1', 'cafe:4001', 1) if state['on_bus'] else None)
        patch(self, hil_pool_check, 'ensure_board_test', lambda *a: 'board_test.elf')

        def flash(board, fw):
            state['on_bus'] = on_bus_after
            return types.SimpleNamespace(returncode=flash_rc, stdout='Error: no target')
        patch(self, hil_pool_check.hil_flash, 'flash_primitive', lambda name: flash)
        patch(self, hil_pool_check.time, 'sleep', lambda s: None)
        clock = iter(range(0, 1000, 5))
        patch(self, hil_pool_check.time, 'monotonic', lambda: next(clock))
        note, row = [], {'status': 'ok'}
        hil_pool_check.park_board(BOARD, 'device', row, note)
        return note, row

    def test_a_device_that_leaves_the_bus_is_parked(self):
        note, row = self.park(on_bus_after=False)
        self.assertEqual(row['status'], 'ok')
        self.assertEqual(note, [])

    def test_a_device_still_enumerated_is_a_silent_park(self):
        note, row = self.park(on_bus_after=True)
        self.assertEqual(row['status'], 'flash-failed')
        self.assertIn('park unverified', note[-1])

    def test_a_failed_park_flash_fails_an_ok_row(self):
        note, row = self.park(on_bus_after=True, flash_rc=1)
        self.assertEqual(row['status'], 'flash-failed')
        self.assertIn('park flash failed', note[-1])


if __name__ == '__main__':
    unittest.main()
