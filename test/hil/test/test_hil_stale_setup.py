#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Offline tests for stale_setup: argument limits, what counts as abandoning the delayed request,
# and the verdict on each later check, against a scripted device. Whether the late answer really
# is dropped is proven on the rig.
import errno
import os
import struct
import sys
import unittest
import unittest.mock

import usb.core

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
import stale_setup
import teardown_stress

DEVICE_DESC = bytes(range(18))


class FakeDev:
    """usbtest's EP0 as stale_setup sees it. 0x5d raises `abandon_error` (a timeout by default) or
    returns after `abandon_s` on `now`; GET_STATUS takes `status_s` and returns or raises `status`;
    0x5e reports `newer` and `stages` with the delay count."""

    def __init__(self, abandon_error=None, newer=1, stages=stale_setup.STAGES_SETUP_DATA_ACK, readback=None,
                 status=bytes(2), status_s=0.001, abandon_s=0.034):
        if abandon_error is None:
            abandon_error = usb.core.USBTimeoutError('t', errno=errno.ETIMEDOUT)
        self.abandon_error = abandon_error
        self.newer, self.stages, self.readback = newer, stages, readback
        self.status, self.status_s, self.abandon_s, self.now = status, status_s, abandon_s, 0.0
        self.delays, self.written = 0, b''

    def clock(self):
        return self.now

    def is_kernel_driver_active(self, itf):
        return False

    def set_interface_altsetting(self, itf, alt):
        pass

    def ctrl_transfer(self, bm, req, val, idx, data_or_len=None, timeout=None):
        if req == stale_setup.REQ_DELAYED:
            self.now += self.abandon_s
            self.delays += 1
            if self.abandon_error is not False:
                raise self.abandon_error
            return 0
        if req == stale_setup.REQ_GET_STATUS:
            self.now += self.status_s
            if isinstance(self.status, Exception):
                raise self.status
            return self.status
        if req == stale_setup.REQ_DELAY_STATS:
            return struct.pack('<HHH', self.newer, self.delays, self.stages)
        if req == stale_setup.REQ_WRITE:
            self.written = bytes(data_or_len)
            return len(self.written)
        if req == stale_setup.REQ_READ:
            return self.readback if self.readback is not None else self.written
        if req == teardown_stress.REQ_GET_DESCRIPTOR:
            return DEVICE_DESC
        return 0


def run(dev, *argv):
    with unittest.mock.patch.object(teardown_stress, 'find', return_value=dev), \
         unittest.mock.patch.object(teardown_stress, 'bulk_eps', return_value=(0, 0x01, 0x81)), \
         unittest.mock.patch.object(stale_setup.time, 'monotonic', dev.clock if dev else None), \
         unittest.mock.patch.object(sys, 'argv', ['stale_setup.py', '-n', '2', *argv]), \
         unittest.mock.patch('builtins.print') as out:
        rc = stale_setup.main()
    return rc, ' '.join(str(c.args[0]) for c in out.call_args_list)


class Args(unittest.TestCase):
    def test_limits_rejected_before_device_access(self):
        for argv in (['-n', '0'], ['-n', '-1'], ['--delay', '2'], ['--delay', '1001']):
            with self.subTest(argv=argv), \
                 unittest.mock.patch.object(teardown_stress, 'find') as find, \
                 unittest.mock.patch.object(sys, 'argv', ['stale_setup.py', *argv]), \
                 unittest.mock.patch('sys.stderr'):
                with self.assertRaises(SystemExit) as e:
                    stale_setup.main()
                self.assertEqual(2, e.exception.code)
                find.assert_not_called()

    def test_device_not_found(self):
        self.assertEqual(2, run(None)[0])


class Verdict(unittest.TestCase):
    def test_every_action_recovered_and_exercised_passes(self):
        rc, out = run(FakeDev())
        self.assertEqual(0, rc, out)
        self.assertIn('PASS', out)

    def test_first_request_after_the_cancel_may_fail_at_once(self):
        rc, out = run(FakeDev(status=usb.core.USBError('pipe', errno=errno.EPIPE)))
        self.assertEqual(0, rc, out)

    def test_first_request_after_the_cancel_failing_late_fails(self):
        # a STALL after the device's delay is the device's answer, not the host's
        rc, out = run(FakeDev(status=usb.core.USBError('pipe', errno=errno.EPIPE), status_s=0.07))
        self.assertEqual(1, rc)
        self.assertIn('GET_STATUS after the abandoned request', out)

    def test_first_request_after_the_cancel_failing_after_a_host_pause_fails(self):
        # the host resumed late: the EPIPE is quick after GET_STATUS but past the delayed answer
        rc, out = run(FakeDev(status=usb.core.USBError('pipe', errno=errno.EPIPE), abandon_s=0.06, status_s=0.04))
        self.assertEqual(1, rc)
        self.assertIn('GET_STATUS after the abandoned request', out)

    def test_first_request_after_the_cancel_other_error_fails(self):
        rc, out = run(FakeDev(status=usb.core.USBTimeoutError('t', errno=errno.ETIMEDOUT)))
        self.assertEqual(1, rc)
        self.assertIn('GET_STATUS after the abandoned request', out)

    def test_first_request_after_the_cancel_answered_short_fails(self):
        rc, out = run(FakeDev(status=b''))
        self.assertEqual(1, rc)
        self.assertIn('returned 0 bytes', out)

    def test_abandonment_must_be_a_timeout(self):
        rc, out = run(FakeDev(abandon_error=usb.core.USBError('pipe', errno=errno.EPIPE)))
        self.assertEqual(1, rc)
        self.assertIn('expected a timeout', out)

    def test_delayed_request_completing_supersedes_nothing(self):
        rc, out = run(FakeDev(abandon_error=False))
        self.assertEqual(1, rc)
        self.assertIn('nothing superseded', out)

    def test_wrong_readback_fails(self):
        rc, out = run(FakeDev(readback=bytes(stale_setup.DATA_LEN)))
        self.assertEqual(1, rc)
        self.assertIn('0x5c read back', out)

    def test_stage_sequence_other_than_setup_data_ack_fails(self):
        for stages in (0x1323, 0x1233, 0x13, 0x12):
            with self.subTest(stages=hex(stages)):
                rc, out = run(FakeDev(stages=stages))
                self.assertEqual(1, rc)
                self.assertIn('0x5b stages', out)

    def test_no_newer_setup_during_the_delay_is_not_exercised(self):
        rc, out = run(FakeDev(newer=0))
        self.assertEqual(1, rc)
        self.assertIn('not exercised', out)


if __name__ == '__main__':
    unittest.main()
