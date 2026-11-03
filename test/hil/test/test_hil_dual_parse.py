#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# dual/host_info_to_device_cdc's 'ID vid:pid SN serial' parsing when tuh_mount_cb's notice lands
# mid-line (#4374). The console is a fake: no port opens.
# Run directly:
#   python3 test/hil/test/test_hil_dual_parse.py
import os
import sys
import unittest

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

import usbtest_harness   # noqa: E402 - stubs pyserial before hil_test imports it
import hil_test          # noqa: E402
from helper import hil_util   # noqa: E402

CDC = ('1a86_55d4', '52D2003414')
MSC = ('21c4_0cc7', '90005893730A1A63')


def record(daddr, vid_pid, serial):
    """The lines print_device_info() writes up to the descriptor dump, serial included."""
    vid, pid = vid_pid.split('_')
    return f'Device {daddr}: ID {vid}:{pid} SN {serial}\r\nDevice Descriptor:\r\n'


# mimxrt1064_evk, run 41228404934: device 2 mounted while device 1's serial was being fetched
ISSUE = ('mounted device 1\r\n'
         'Device 1: ID 1a86:55d4 SN mounted device 2\r\n52D2003414\r\nDevice Descriptor:\r\n'
         + record(2, *MSC))
CLEAN = 'mounted device 1\r\n' + record(1, *CDC) + 'mounted device 2\r\n' + record(2, *MSC)


def devs(text):
    return [dev for _, dev in hil_test.parse_dev_info_records(text)]


class ParseDevInfoRecords(unittest.TestCase):
    def test_issue_transcript(self):
        self.assertEqual(devs(ISSUE), ['1a86_55d4_52D2003414', '21c4_0cc7_90005893730A1A63'])
        self.assertEqual(hil_test.parse_dev_info_records(ISSUE)[0][0], 'Device 1: ID 1a86:55d4 SN 52D2003414')

    def test_clean_transcript(self):
        self.assertEqual(devs(CLEAN), ['1a86_55d4_52D2003414', '21c4_0cc7_90005893730A1A63'])

    def test_every_split_of_the_issue_transcript(self):
        """Each prefix is what one read may have accumulated: it yields no serial but a full one."""
        full = {'1a86_55d4_52D2003414', '21c4_0cc7_90005893730A1A63'}
        for n in range(len(ISSUE) + 1):
            self.assertLessEqual(set(devs(ISSUE[:n])), full, repr(ISSUE[:n]))

    def test_notice_split_across_reads(self):
        head = 'Device 1: ID 1a86:55d4 SN mounted device 1'
        self.assertEqual(devs(head), [])
        self.assertEqual(devs(head + '2\r'), [])
        self.assertEqual(devs(head + '2\r\n52D2003414\r\n'), ['1a86_55d4_52D2003414'])

    def test_partial_serial_waits_for_its_line_end(self):
        self.assertEqual(devs('Device 1: ID 1a86:55d4 SN 52D20'), [])
        self.assertEqual(devs('Device 1: ID 1a86:55d4 SN mounted device 2\r\n52D20'), [])
        self.assertEqual(devs('Device 1: ID 1a86:55d4 SN 52D2003414\r'), [])

    def test_repeated_notices(self):
        text = ('Device 1: ID 1a86:55d4 SN mounted device 2\r\nunmounted device 2\r\nmounted device 3\r\n'
                '52D2003414\r\n' + record(3, *MSC) + 'unmounted device 3\r\nmounted device 3\r\n' + record(3, *MSC))
        self.assertEqual(devs(text), ['1a86_55d4_52D2003414', '21c4_0cc7_90005893730A1A63',
                                      '21c4_0cc7_90005893730A1A63'])

    def test_multi_digit_address(self):
        self.assertEqual(devs('Device 1: ID 1a86:55d4 SN mounted device 12\r\n52D2003414\r\n'),
                         ['1a86_55d4_52D2003414'])

    def test_wrong_and_missing_serials_pass_through(self):
        """A failed serial fetch prints '0' and an empty serial matches nothing; the cell's
        comparison against the roster then fails them."""
        text = record(1, '1a86_55d4', '52D2999999') + record(2, '21c4_0cc7', '0') + 'Device 3: ID 0951:1603 SN \r\n'
        self.assertEqual(devs(text), ['1a86_55d4_52D2999999', '21c4_0cc7_0'])


class FakeSerial:
    """Hands out one chunk per read, then nothing."""
    def __init__(self, chunks):
        self.chunks = list(chunks)
        self.timeout = None

    @property
    def in_waiting(self):
        return len(self.chunks[0]) if self.chunks else 0

    def read(self, n):
        return self.chunks.pop(0) if self.chunks else b''

    def close(self):
        pass


class CellVerdict(unittest.TestCase):
    BOARD = {'uid': 'U', 'tests': {'dev_attached': [{'vid_pid': CDC[0], 'serial': CDC[1]},
                                                    {'vid_pid': MSC[0], 'serial': MSC[1]}]}}

    def run_cell(self, text, size):
        data = text.encode()
        chunks = [data[i:i + size] for i in range(0, len(data), size)]
        usbtest_harness.patch(self, hil_util, 'get_serial_dev', lambda *a: '/dev/null')
        usbtest_harness.patch(self, hil_test, 'open_serial_dev', lambda port: FakeSerial(chunks))
        usbtest_harness.patch(self, hil_test, 'enum_timeout', lambda: len(chunks) + 1)   # 10 reads a unit
        usbtest_harness.patch(self, hil_test.time, 'sleep', lambda s: None)
        out = usbtest_harness.Out()
        usbtest_harness.patch(self, sys, 'stdout', out)
        return hil_test.test_dual_host_info_to_device_cdc(self.BOARD)

    def test_issue_transcript_passes_in_any_chunking(self):
        for size in (1, 7, 43, len(ISSUE)):
            self.assertEqual(self.run_cell(ISSUE, size), 0, size)

    def test_wrong_serial_fails(self):
        with self.assertRaisesRegex(AssertionError, r"Enumerated \['1a86_55d4_52D2999999'"):
            self.run_cell(record(1, '1a86_55d4', '52D2999999') + record(2, *MSC), 16)


if __name__ == '__main__':
    unittest.main()
