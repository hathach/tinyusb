#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Where each caller of hil_flash.reset_primitive resets relative to its console, and what a
# flasher without a reset-only mode (None) does there. Consoles are fakes: no port opens.
# Run directly:
#   python3 test/hil/test/test_hil_reset_order.py
import os
import sys
import types
import unittest

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

import usbtest_harness   # noqa: E402 - stubs pyserial before hil_test imports it
import hil_flash         # noqa: E402
import hil_test          # noqa: E402
from helper import hil_pool_check   # noqa: E402

RTT_BOARD = {'name': 'b', 'uid': 'U', 'logger': 'rtt', 'flasher': {'name': 'jlink', 'uid': 'P'}}
VCOM_BOARD = {'name': 'b', 'uid': 'U', 'flasher': {'name': 'openocd', 'uid': 'P'}}


class ResetCallersKeepTheirOrder(unittest.TestCase):
    """An RTT console owns the probe, so the reset precedes it and the ring keeps the boot
    burst; a VCOM survives the reset, so it opens first to catch the banner, and
    hil_pool_check also flushes the pre-reset backlog before resetting."""

    def setUp(self):
        self.calls = []
        self.patch(hil_pool_check, 'say', lambda msg: None)

    def patch(self, obj, name, value):
        usbtest_harness.patch(self, obj, name, value)

    def fake_reset(self, rc=0):
        self.patch(hil_flash, 'reset_primitive', lambda name: (
            lambda board: self.calls.append(('reset', name)) or types.SimpleNamespace(returncode=rc, stdout=b'')))

    def no_reset(self):
        self.patch(hil_flash, 'reset_primitive', lambda name: None)

    def fake_consoles(self):
        """JlinkRtt and serial.Serial both become a console that says hello at once."""
        calls = self.calls

        class Console:
            def __init__(self, *a, **kw):
                calls.append('open')

            def reset_input_buffer(self):
                calls.append('flush')

            def write(self, data):
                pass

            def read(self, n):
                return b'Hello from TinyUSB\n'

            def close(self):
                pass
        self.patch(hil_pool_check.hil_util, 'JlinkRtt', Console)
        self.patch(sys.modules['serial'], 'Serial', Console)
        self.patch(hil_pool_check.hil_util, 'get_serial_dev', lambda *a: '/dev/null')

    def console(self, board):
        self.patch(hil_test, 'open_board_console', lambda b: self.calls.append('open') or 'console')
        return hil_test.open_console_reset(board)

    def test_hil_test_resets_an_rtt_board_before_opening_its_console(self):
        self.fake_reset()
        self.assertEqual(self.console(RTT_BOARD), 'console')
        self.assertEqual(self.calls, [('reset', 'jlink'), 'open'])

    def test_hil_test_opens_a_vcom_before_resetting(self):
        self.fake_reset()
        self.assertEqual(self.console(VCOM_BOARD), 'console')
        self.assertEqual(self.calls, ['open', ('reset', 'openocd')])

    def test_hil_test_fails_a_failed_reset(self):
        self.fake_reset(rc=1)
        with self.assertRaises(AssertionError):
            self.console(VCOM_BOARD)

    def test_hil_test_only_opens_the_console_without_a_reset(self):
        self.no_reset()
        self.console(dict(VCOM_BOARD, flasher={'name': 'esptool'}))
        self.assertEqual(self.calls, ['open'])

    def test_pool_check_resets_an_rtt_board_before_attaching(self):
        self.fake_consoles()
        self.fake_reset()
        self.assertTrue(hil_pool_check.check_host_serial(RTT_BOARD, want_hello=True))
        self.assertEqual(self.calls, [('reset', 'jlink'), 'open'])

    def test_pool_check_never_attaches_after_a_failed_rtt_reset(self):
        # the previous run's ring is intact: attaching would score stale output as life
        self.fake_consoles()
        self.fake_reset(rc=1)
        self.assertIsNone(hil_pool_check.check_host_serial(RTT_BOARD, want_hello=True))
        self.assertEqual(self.calls, [('reset', 'jlink')])

    def test_pool_check_flushes_a_vcom_before_resetting(self):
        self.fake_consoles()
        self.fake_reset()
        self.assertTrue(hil_pool_check.check_host_serial(VCOM_BOARD, want_hello=True))
        self.assertEqual(self.calls, ['open', 'flush', ('reset', 'openocd')])

    def recover(self, flasher):
        self.patch(hil_pool_check, 'get_expected_pid', lambda example: None)
        self.patch(hil_pool_check, 'wait_device', lambda *a: self.calls.append('wait') or None)
        note, row = [], {}
        ok = hil_pool_check.device_recover_and_check(dict(VCOM_BOARD, flasher={'name': flasher}),
                                                     'device/x', '', None, note, row, {})
        return ok, note, row

    def test_pool_check_skips_the_retry_wait_without_a_reset(self):
        self.no_reset()
        ok, note, row = self.recover('esptool')
        self.assertFalse(ok)
        self.assertEqual(self.calls, ['wait'], 'burned a second wait with nothing reset')
        self.assertIn('no hardware reset', note[0])
        self.assertIn('not enumerated', row.get('device', ''))

    def test_pool_check_resets_then_waits_again(self):
        self.fake_reset()
        ok, _note, _row = self.recover('openocd')
        self.assertFalse(ok)
        self.assertEqual(self.calls, ['wait', ('reset', 'openocd'), 'wait'])


if __name__ == '__main__':
    unittest.main()
