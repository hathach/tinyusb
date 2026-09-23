#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Offline tests for mtp_raw: container framing, the ZLP rules, the transport error
# mapping and the case runner's containment, against a scripted link. The protocol cases
# themselves are proven on the rig (hil_test.py -t device/mtp); a link that mimics the
# device's state machine here would only test itself.
import os
import struct
import sys
import unittest
import unittest.mock

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
import mtp_raw
from mtp_raw import HDR, RawError, RawMtp, Stall, TYPE_COMMAND, TYPE_DATA, TYPE_RESPONSE


class FakeLink:
    """Records writes; serves reads and control replies from queues. An entry that is an
    exception instance is raised instead of returned."""

    def __init__(self, mps=64):
        self.mps, self.itf, self.ep_in, self.ep_out = mps, 0, 0x81, 0x02
        self.writes, self.reads, self.ctrls, self.cleared = [], [], [], []
        self.ctrl_log = []

    def write(self, data):
        self.writes.append(bytes(data))

    def _pop(self, q, what):
        if not q:
            raise AssertionError(f'unscripted {what}')
        r = q.pop(0)
        if isinstance(r, Exception):
            raise r
        return r

    def read(self, size):
        r = self._pop(self.reads, 'read')
        return r[:size]

    def ctrl(self, bm, req, val, idx, data_or_len):
        self.ctrl_log.append((bm, req, val, idx, data_or_len))
        return self._pop(self.ctrls, f'control {req:#04x}')

    def clear_halt(self, ep):
        self.cleared.append(ep)


def status_reply(code, *eps):
    return struct.pack('<HH', 4 + 2 * len(eps), code) + b''.join(struct.pack('<H', e) for e in eps)


class Framing(unittest.TestCase):
    def test_command_container_and_tid(self):
        link = FakeLink()
        m = RawMtp(link)
        self.assertEqual(0, m.command(mtp_raw.OP_OPEN_SESSION, 1))  # tid 0 outside a session
        self.assertEqual([struct.pack('<IHHII', 16, TYPE_COMMAND, 0x1002, 0, 1)], link.writes)
        self.assertEqual(1, m.command(mtp_raw.OP_GET_DEVICE_INFO))

    def test_zlp_follows_a_packet_multiple(self):
        link = FakeLink(mps=64)
        m = RawMtp(link)
        m.send_container(TYPE_DATA, 0x100D, bytes(64 - HDR))
        self.assertEqual(2, len(link.writes))
        self.assertEqual(b'', link.writes[1])
        link.writes.clear()
        m.send_container(TYPE_DATA, 0x100D, bytes(64 - HDR + 1))
        self.assertEqual(1, len(link.writes))

    def test_response_parse_and_mismatch(self):
        link = FakeLink()
        m = RawMtp(link)
        tid = m.command(mtp_raw.OP_GET_DEVICE_INFO)
        link.reads.append(struct.pack('<IHHI', HDR + 8, TYPE_RESPONSE, 0x2001, tid) + struct.pack('<II', 7, 9))
        self.assertEqual((0x2001, (7, 9)), m.read_response(tid))
        link.reads.append(struct.pack('<IHHI', HDR, TYPE_RESPONSE, 0x2001, tid + 5))
        with self.assertRaises(RawError):
            m.read_response(tid)

    def test_data_block_length_must_match(self):
        link = FakeLink()
        m = RawMtp(link)
        tid = m.command(mtp_raw.OP_GET_OBJECT, 1)
        link.reads.append(struct.pack('<IHHI', HDR + 100, TYPE_DATA, 0x1009, tid) + bytes(60))
        with self.assertRaises(RawError):
            m.read_data(tid)

    def test_runt_container_is_an_error(self):
        with self.assertRaises(RawError):
            mtp_raw.parse(b'\x08\x00\x00\x00')

    def test_object_info_layout(self):
        ds = mtp_raw.object_info(100, 'ab')
        self.assertEqual(mtp_raw.OBJECT_INFO_HDR, ds.index(b'\x03a\x00b\x00\x00\x00'))
        self.assertEqual(100, struct.unpack_from('<I', ds, 8)[0])
        self.assertEqual('ab', mtp_raw.object_info_name(ds))
        self.assertEqual(255, mtp_raw.mtp_string('x' * 254)[0])
        self.assertEqual(b'\x00', mtp_raw.mtp_string(''))

    def test_device_status_parse(self):
        link = FakeLink()
        m = RawMtp(link)
        link.ctrls.append(status_reply(0x201F, 0x81, 0x02))
        self.assertEqual((0x201F, [0x81, 0x02]), m.device_status())
        link.ctrls.append(status_reply(0x2001))
        self.assertEqual((0x2001, []), m.device_status())
        link.ctrls.append(struct.pack('<HH', 8, 0x2001))  # declares more than it sent
        with self.assertRaises(RawError):
            m.device_status()
        self.assertEqual((0xA1, 0x67, 0, 0, 16), link.ctrl_log[0])

    def test_cancel_and_reset_requests(self):
        link = FakeLink()
        m = RawMtp(link)
        link.ctrls += [b'', b'']
        m.cancel(0x12345678)
        m.device_reset()
        self.assertEqual((0x21, 0x64, 0, 0, struct.pack('<HI', 0x4001, 0x12345678)), link.ctrl_log[0])
        self.assertEqual((0x21, 0x66, 0, 0, b''), link.ctrl_log[1])


def response(tid, code=0x2001):
    return struct.pack('<IHHI', HDR, TYPE_RESPONSE, code, tid)


def handles_reply(tid, *handles):
    """GetObjectHandles data block and OK response."""
    body = struct.pack('<I%dI' % len(handles), len(handles), *handles)
    return [struct.pack('<IHHI', HDR + len(body), TYPE_DATA, 0x1007, tid) + body, response(tid)]


class Containment(unittest.TestCase):
    def setUp(self):
        patcher = unittest.mock.patch.object(mtp_raw.time, 'sleep', lambda s: None)
        patcher.start()
        self.addCleanup(patcher.stop)

    def run_cases(self, m, cases, baseline=frozenset()):
        with unittest.mock.patch.object(mtp_raw, 'CASES', cases):
            return mtp_raw.run_cases(m, baseline, log=lambda s: None)

    def test_wait_status_ok_is_bounded(self):
        link = FakeLink()
        m = RawMtp(link)
        link.ctrls += [status_reply(0x201F, 0x81, 0x02)] * (mtp_raw.STATUS_POLLS + 5)
        with self.assertRaises(RawError):
            m.wait_status_ok()
        self.assertEqual(mtp_raw.STATUS_POLLS, len(link.ctrl_log))

    def test_expect_stall_needs_a_stall(self):
        mtp_raw.expect_stall(lambda: (_ for _ in ()).throw(Stall('EPIPE')))
        with self.assertRaises(RawError):
            mtp_raw.expect_stall(lambda: None)

    def test_runner_stops_when_the_device_will_not_recover(self):
        link = FakeLink()
        m = RawMtp(link)
        calls = []
        cases = [('a', lambda m: calls.append('a')),
                 ('b', lambda m: (_ for _ in ()).throw(RawError('boom'))),
                 ('c', lambda m: calls.append('c'))]
        link.ctrls += [status_reply(0x2001)]                 # case a's idle check
        link.reads += handles_reply(1)                       # ... no objects
        link.ctrls += [b'']                                  # recover: device reset ...
        link.ctrls += [status_reply(0x201F, 0x81, 0x02)] * mtp_raw.STATUS_POLLS  # ... never ready
        failures = self.run_cases(m, cases)
        self.assertEqual(['a'], calls)
        self.assertEqual(2, len(failures))
        self.assertIn('b: RawError: boom', failures[0])
        self.assertIn('did not recover', failures[1])
        self.assertEqual([0x81, 0x02], link.cleared)

    def test_runner_continues_after_a_recovered_failure(self):
        link = FakeLink()
        m = RawMtp(link)
        calls = []
        cases = [('b', lambda m: (_ for _ in ()).throw(RawError('boom'))),
                 ('c', lambda m: calls.append('c'))]
        link.ctrls += [b'', status_reply(0x2001)]            # recover: reset, status ok
        link.reads += [response(0)] + handles_reply(1)       # OpenSession, nothing to delete
        link.ctrls += [status_reply(0x2001)]                 # case c's idle check
        link.reads += handles_reply(2)
        failures = self.run_cases(m, cases)
        self.assertEqual(['c'], calls)
        self.assertEqual(1, len(failures))

    def test_malformed_reply_is_contained(self):
        link = FakeLink()
        m = RawMtp(link)
        cases = [('odd', lambda m: m.device_status())]
        link.ctrls += [b'\x05\x00\x01\x20\x81']             # odd length: struct.error inside
        link.ctrls += [b'', status_reply(0x2001)]            # recover
        link.reads += [response(0)] + handles_reply(1)
        failures = self.run_cases(m, cases)
        self.assertEqual(1, len(failures))
        self.assertIn('odd: ', failures[0])
        self.assertEqual([0x81, 0x02], link.cleared)

    def test_case_leaving_an_object_is_a_failure(self):
        link = FakeLink()
        m = RawMtp(link)
        cases = [('leak', lambda m: None)]                  # the device grew object 3 anyway
        link.ctrls += [status_reply(0x2001)]                 # idle check passes ...
        link.reads += handles_reply(1, 1, 3)                 # ... but 3 is beyond the baseline
        link.ctrls += [b'', status_reply(0x2001)]            # recover
        link.reads += [response(0)] + handles_reply(1, 1, 3) + [response(2)]  # delete 3
        failures = self.run_cases(m, cases, baseline={1})
        self.assertEqual(1, len(failures))
        self.assertIn('left objects [3]', failures[0])
        self.assertEqual(struct.pack('<IHHII', 16, TYPE_COMMAND, 0x100B, 2, 3), link.writes[-1])


class UsbErrorMapping(unittest.TestCase):
    def test_pipe_error_is_a_stall_and_timeout_is_an_error(self):
        try:
            import usb.core
        except ImportError:
            self.skipTest('pyusb not installed')
        import errno
        stall = usb.core.USBError('pipe', errno=errno.EPIPE)
        timeout = usb.core.USBTimeoutError('t', errno=errno.ETIMEDOUT)
        self.assertIsInstance(mtp_raw.UsbLink._wrap(stall), Stall)
        self.assertIsInstance(mtp_raw.UsbLink._wrap(timeout), RawError)
        self.assertNotIsInstance(mtp_raw.UsbLink._wrap(timeout), Stall)


if __name__ == '__main__':
    unittest.main()
