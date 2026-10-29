#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# hil_test.py --sysview-out: test_board captures a roster sysview board under its lock, before its one final park,
# which runs even when the capture is interrupted, and records the capture for `sysview_ci.py decode` without
# touching the board's verdict. Run directly:
#   python3 test/hil/test/test_hil_sysview_capture.py
import json
import os
import sys
import tempfile
import time
import unittest
from unittest import mock

HIL_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, HIL_DIR)

from usbtest_harness import patch   # noqa: E402 - stubs pyserial before hil_test imports it
import hil_flash         # noqa: E402
import hil_test          # noqa: E402
import sysview_ci        # noqa: E402
from helper import hil_lock, hil_tt, hil_util   # noqa: E402

BOARD = 'hil-sysview-selftest-board'
CAPTURED = ({'raw': f'{BOARD}-capture.SVDat', 'workload_ok': True}, None)


class SysviewCapture(unittest.TestCase):
    def setUp(self):
        td = tempfile.TemporaryDirectory()
        self.addCleanup(td.cleanup)
        self.out = os.path.join(td.name, 'sv')
        os.mkdir(self.out)
        patch(self, hil_lock, 'BOARD_LOCK_DIR', os.path.join(td.name, 'locks'))
        os.environ.pop('HIL_NO_BOARD_LOCK', None)
        for name, value in (('sysview_out', self.out), ('sysview_head', 'H' * 40), ('sysview_commit', 'abc1234'),
                            ('pool_deadline', None), ('skip_flash', False), ('board_wedged', ''),
                            ('log_line', lambda line: None)):
            patch(self, hil_test, name, value)
        patch(self, hil_test.usbtest, 'strays_on', lambda uid: [])
        patch(self, hil_flash, 'find_firmware', lambda variant, example, flasher=None: f'{variant}/{example}.elf')
        self.events = []
        self.park = (0, 'pass', None)

        def test_example(board, variant, example):
            self.events.append(example)
            return self.park if example == 'device/board_test' else (0, 'pass', None)
        patch(self, hil_test, 'test_example', test_example)
        patch(self, sysview_ci, 'find_capture_firmware', lambda board, head: ('fw.elf', {}, None))
        self.capture(lambda: CAPTURED)
        self.board = {'name': BOARD, 'uid': 'U', 'flasher': {'name': 'jlink', 'uid': 'P'},
                      'tests': {'only': ['device/cdc_msc']},
                      'sysview': {'example': 'device/cdc_msc', 'workload': 'idle', 'duration_s': 15}}

    def capture(self, outcome):
        def capture_one(board, fw, manifest, out_dir):
            self.events.append('capture')
            return outcome()
        patch(self, sysview_ci, 'capture_one', capture_one)

    def run_board(self):
        name, err, failed, rows, _ = hil_test.test_board(self.board)
        self.assertEqual((name, failed), (BOARD, []))
        return err, rows

    def result(self):
        with open(os.path.join(self.out, f'sysview-{BOARD}.json')) as f:
            return json.load(f)

    def assert_no_result(self):
        self.assertEqual(os.listdir(self.out), [])

    def test_captured_before_the_one_final_park(self):
        err, rows = self.run_board()
        self.assertEqual(self.events, ['device/cdc_msc', 'capture', 'device/board_test'])
        self.assertEqual((err, rows[0][1]), (0, {'device/cdc_msc': 'pass'}))
        r = self.result()
        self.assertEqual((r['commit'], r['error'], r['capture']),
                         ('abc1234', sysview_ci.NOT_DECODED,
                          {'raw': f'{BOARD}-capture.SVDat', 'workload_ok': True, 'status': 'captured',
                           'park_error': None}))

    def test_the_record_carries_the_checkout_identity(self):
        identity = {'sha': 'M' * 40, 'base_sha': 'B' * 40, 'head_sha': 'H' * 40, 'base_ref': 'master'}
        patch(self, hil_test, 'sysview_identity', identity)
        self.run_board()
        self.assertEqual({k: self.result()[k] for k in identity}, identity)

    def test_a_dual_selection_captures_and_a_host_only_one_does_not(self):
        self.board['tests'] = {'only': ['dual/host_info_to_device_cdc']}
        self.run_board()
        self.assertIn('capture', self.events)
        self.events.clear()
        os.remove(os.path.join(self.out, f'sysview-{BOARD}.json'))
        self.board['tests'] = {'only': ['host/cdc_msc_hid']}
        self.run_board()
        self.assertEqual(self.events, ['host/cdc_msc_hid', 'device/board_test'])
        self.assert_no_result()

    def test_nothing_without_the_option_or_a_roster_entry(self):
        for name, ctx in (('no option', mock.patch.object(hil_test, 'sysview_out', None)),
                          ('no entry', mock.patch.dict(self.board, {'sysview': None}))):
            with self.subTest(name), ctx:
                if name == 'no entry':
                    del self.board['sysview']
                self.events.clear()
                self.run_board()
                self.assertEqual(self.events, ['device/cdc_msc', 'device/board_test'])
                self.assert_no_result()

    def test_refused_unflashed(self):
        def wedge(board, variant, example):
            self.events.append(example)
            hil_test.board_wedged = 'stray testusb'
            return 0, 'pass', None
        for name, ctx, why in (
                ('skip flash', mock.patch.object(hil_test, 'skip_flash', True), 'skipped: --skip-flash'),
                ('wedged', mock.patch.object(hil_test, 'test_example', wedge),
                 'skipped: the board is wedged: stray testusb'),
                ('no park image', mock.patch.object(hil_flash, 'find_firmware', lambda *a, **k: None),
                 'skipped: no park (device/board_test) firmware'),
                ('pool guard', mock.patch.object(hil_test, 'pool_deadline', time.monotonic() + 10),
                 'left before the pool guard')):
            with self.subTest(name), ctx:
                self.events.clear()
                err, _ = self.run_board()
                self.assertNotIn('capture', self.events)
                r = self.result()
                self.assertIn(why, r['error'])
                self.assertEqual((err, r['capture']['status'], r['capture']['park_error']), (0, 'failed', None))

    def test_refused_beside_a_pending_provider_cleanup(self):
        patch(self, hil_flash, 'cleanup_pending', {'other-board': ['r.json']})
        capture, error, flashed = hil_test._sysview_capture(self.board, BOARD)
        self.assertEqual((capture, error, flashed),
                         (None, 'skipped: other-board: openocd_esp cleanup pending, receipt r.json', False))
        self.assertEqual(self.events, [])

    def test_a_capture_that_fits_before_the_pool_guard_runs(self):
        need = sysview_ci.capture_bound_s(self.board) + hil_test.park_bound_s() + hil_test.SYSVIEW_MARGIN_S
        patch(self, hil_test, 'pool_deadline', time.monotonic() + need + 30)
        self.run_board()
        self.assertIn('capture', self.events)

    def test_a_missing_image_or_a_failed_capture_leaves_the_verdict_alone(self):
        self.park = (1, 'fail', None)
        for name, ctx, why in (
                ('no image', mock.patch.object(sysview_ci, 'find_capture_firmware',
                                               lambda b, h: (None, None, 'no image of device/cdc_msc')),
                 'no image of device/cdc_msc'),
                ('failed', mock.patch.object(sysview_ci, 'capture_one', lambda b, fw, m, d: (None, 'flash failed: rc=1')),
                 'flash failed: rc=1')):
            with self.subTest(name), ctx:
                self.events.clear()
                err, rows = self.run_board()
                self.assertEqual((err, rows[0][1]), (0, {'device/cdc_msc': 'pass'}))
                self.assertEqual(self.events[-1], 'device/board_test')
                r = self.result()
                # a capture that never flashed leaves the park to the run
                self.assertEqual((r['error'], r['capture']['status'], r['capture']['park_error']),
                                 (why, 'failed', None if name == 'no image' else 'park flash failed'))

    def test_a_capture_or_its_record_raising_leaves_the_board_its_rows(self):
        def raises(*a, **k):
            raise OSError(28, 'No space left on device')
        for name, ctx in (('capture', mock.patch.object(hil_test, '_sysview_capture', raises)),
                          ('record', mock.patch.object(sysview_ci, 'write_result', raises))):
            with self.subTest(name), ctx:
                self.events.clear()
                err, rows = self.run_board()
                self.assertEqual((err, rows[0][1]), (0, {'device/cdc_msc': 'pass'}))
                self.assertEqual(self.events[-1], 'device/board_test')
                if name == 'capture':
                    self.assertIn('No space left', self.result()['error'])

    def test_a_failed_park_after_a_capture_is_kept_with_it(self):
        self.park = (1, 'fail', None)
        err, _ = self.run_board()
        r = self.result()
        self.assertEqual((err, r['error'], r['capture']['park_error']),
                         (0, sysview_ci.NOT_DECODED, 'park flash failed'))

    def test_an_interrupted_capture_still_parks_then_propagates(self):
        def interrupted():
            raise KeyboardInterrupt
        self.capture(interrupted)
        with self.assertRaises(KeyboardInterrupt):
            hil_test.test_board(self.board)
        self.assertEqual(self.events, ['device/cdc_msc', 'capture', 'device/board_test'])
        self.assert_no_result()
        hil_lock.take_board_lock(BOARD, 'after').close()   # the lock was released

    def test_a_capture_that_wedges_the_board_is_not_parked(self):
        def wedging():
            hil_test.board_wedged = 'D-state usbfs holder'
            return CAPTURED
        self.capture(wedging)
        self.run_board()
        self.assertEqual(self.events, ['device/cdc_msc', 'capture'])
        self.assertEqual(self.result()['capture']['park_error'], 'not parked: the board is wedged: D-state usbfs holder')

    def test_the_park_bound_covers_each_attempt(self):
        patch(self, hil_util, 'CMD_TIMEOUT', 100)
        patch(self, hil_util, 'REAP_GRACE', 7)
        patch(self, hil_test, 'max_retry', 2)
        self.assertEqual(hil_test.park_bound_s(), 2 * (hil_tt.HELPER_TIMEOUT + 7 + 2 * (100 + 7) + 1))


if __name__ == '__main__':
    unittest.main()
