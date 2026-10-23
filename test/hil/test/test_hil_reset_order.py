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

RTT_BOARD = {'name': 'b', 'uid': 'U', 'logger': 'rtt', 'flasher': {'name': 'jlink', 'uid': 'P'}}
VCOM_BOARD = {'name': 'b', 'uid': 'U', 'flasher': {'name': 'openocd', 'uid': 'P'}}


class ResetCallersKeepTheirOrder(unittest.TestCase):
    """An RTT console owns the probe, so the reset precedes it and the ring keeps the boot
    burst; a VCOM survives the reset, so it opens first to catch the banner."""

    def setUp(self):
        self.calls = []

    def patch(self, obj, name, value):
        usbtest_harness.patch(self, obj, name, value)

    def fake_reset(self, rc=0):
        self.patch(hil_flash, 'reset_primitive', lambda name: (
            lambda board, **kw: self.calls.append(('reset', name)) or types.SimpleNamespace(returncode=rc, stdout=b'')))

    def no_reset(self):
        self.patch(hil_flash, 'reset_primitive', lambda name: None)

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


if __name__ == '__main__':
    unittest.main()
