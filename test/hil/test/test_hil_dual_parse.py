#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# dual/host_info_to_device_cdc's 'ID vid:pid SN serial' parsing when tuh_mount_cb's notice lands
# mid-line (#4374), and the host examples' device-info and MSC capacity records. The console is a fake:
# no port opens.
# Run directly:
#   python3 test/hil/test/test_hil_dual_parse.py
import os
import sys
import unittest

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

if sys.platform == 'win32':
    raise unittest.SkipTest("hil_test imports agentrc's rig harness (fcntl locks, process groups): POSIX only")

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

    def test_punctuated_serial_is_kept_whole(self):
        text = record(1, '1a86_55d4', 'ABC-DEF.1:2_3') + 'mounted device 2\r\n' + record(2, '1a86_55d4', 'AB')
        self.assertEqual(devs(text), ['1a86_55d4_ABC-DEF.1:2_3', '1a86_55d4_AB'])
        self.assertEqual(devs('Device 1: ID 1a86:55d4 SN ABC-DEF\r'), [])


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
    BOARD = {'uid': 'U', 'dev_attached': [{'vid_pid': CDC[0].replace('_', ':'), 'serial': CDC[1]},
                                          {'vid_pid': MSC[0].replace('_', ':'), 'serial': MSC[1]}]}

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

    def test_declared_devs_print_as_the_host_examples_do(self):
        board = {'dev_attached': [{'vid_pid': 'abcd:1234', 'serial': '\u0409'}, {'vid_pid': '1a86:7523'}]}
        self.assertEqual(hil_test.declared_devs(board), ['abcd_1234_\u0409', '1a86_7523_0'])
        with self.assertRaisesRegex(AssertionError, "agentrc's board registry"):
            hil_test.declared_devs({'tests': {}})

    def test_issue_transcript_passes_in_any_chunking(self):
        for size in (1, 7, 43, len(ISSUE)):
            self.assertEqual(self.run_cell(ISSUE, size), 0, size)

    def test_wrong_serial_fails(self):
        with self.assertRaisesRegex(AssertionError, r"Enumerated \['1a86_55d4_52D2999999'"):
            self.run_cell(record(1, '1a86_55d4', '52D2999999') + record(2, *MSC), 16)


class HostDeviceInfo(unittest.TestCase):
    """host/device_info completes on, and compares, terminated records only."""

    def run_cell(self, board, text, size):
        data = text.encode()
        chunks = [data[i:i + size] for i in range(0, len(data), size)]
        usbtest_harness.patch(self, hil_test, 'open_console_reset', lambda b: FakeSerial(chunks))
        usbtest_harness.patch(self, hil_test, 'enum_timeout', lambda: len(chunks) + 1)
        usbtest_harness.patch(self, hil_test.time, 'sleep', lambda s: None)
        usbtest_harness.patch(self, sys, 'stdout', usbtest_harness.Out())
        return hil_test.test_host_device_info(board)

    def test_issue_transcript_passes_in_any_chunking(self):
        for size in (1, 7, 43, len(ISSUE)):
            self.assertEqual(self.run_cell(CellVerdict.BOARD, ISSUE, size), 0, size)

    def test_a_declared_prefix_of_the_serial_fails(self):
        # the read that ends at '52D20' must not complete a board that declares 52D20
        board = {'dev_attached': [{'vid_pid': '1a86:55d4', 'serial': '52D20'}]}
        text = 'Device 1: ID 1a86:55d4 SN 52D20' + '03414\r\nDevice Descriptor:\r\n'
        with self.assertRaisesRegex(AssertionError, r"Enumerated \['1a86_55d4_52D2003414'\]"):
            self.run_cell(board, text, len('Device 1: ID 1a86:55d4 SN 52D20'))

    def test_a_declared_serial_that_prefixes_the_punctuated_one_fails(self):
        board = {'dev_attached': [{'vid_pid': '1a86:55d4', 'serial': 'AB'}]}
        with self.assertRaisesRegex(AssertionError, r"Enumerated \['1a86_55d4_AB-1'\]"):
            self.run_cell(board, record(1, '1a86_55d4', 'AB-1'), 7)

    def test_a_declared_punctuated_serial_passes(self):
        board = {'dev_attached': [{'vid_pid': '1a86:55d4', 'serial': 'AB-1'}]}
        self.assertEqual(self.run_cell(board, record(1, '1a86_55d4', 'AB-1'), 7), 0)


class MscCapacity(unittest.TestCase):
    """check_msc_info: a complete 'Disk Size' record, matching a declared count/size pair."""
    LEXAR = {'vid_pid': '21c4:0cc7', 'is_msc': True, 'block_size': 512, 'block_count': 60620800}

    @staticmethod
    def disk(count, size=512):
        return f'Disk Size: {count} {size}-byte blocks: {count * size // (1024 * 1024)} MB\r\n'

    def check(self, text, devs):
        usbtest_harness.patch(self, sys, 'stdout', usbtest_harness.Out())
        hil_test.check_msc_info(text, devs)

    def test_the_declared_capacity_passes(self):
        self.check('Lexar    USB Flash Drive  PMAP\r\n' + self.disk(60620800), [self.LEXAR])

    def test_an_esp_idf_line_ending_passes(self):
        self.check(self.disk(60620800).replace('\r\n', '\r\r\n'), [self.LEXAR])

    def test_a_wrong_count_fails(self):
        with self.assertRaisesRegex(AssertionError, 'reported 30310400 512-byte blocks'):
            self.check(self.disk(30310400), [self.LEXAR])

    def test_a_wrong_size_fails(self):
        with self.assertRaisesRegex(AssertionError, 'reported 60620800 4096-byte blocks'):
            self.check(self.disk(60620800, 4096), [self.LEXAR])

    def test_a_truncated_or_malformed_record_fails(self):
        for text in ('Disk Size: 606', 'Disk Size: 60620800 512-byte blocks', 'Disk Size: garbage\r\n'):
            with self.subTest(text), self.assertRaisesRegex(AssertionError, 'not reported as a complete'):
                self.check(text, [self.LEXAR])

    def test_any_declared_device_may_match_and_every_record_must(self):
        other = {'vid_pid': '048d:04d2', 'is_msc': True, 'block_size': 512, 'block_count': 30720000}
        self.check(self.disk(30720000), [self.LEXAR, other])
        with self.assertRaisesRegex(AssertionError, 'reported 1 512-byte blocks'):
            self.check(self.disk(30720000) + self.disk(1), [self.LEXAR, other])

    def test_an_undeclared_field_matches_any_value(self):
        self.check(self.disk(123), [{'vid_pid': '21c4:0cc7', 'is_msc': True, 'block_size': 512}])
        self.check(self.disk(123, 4096), [{'vid_pid': '21c4:0cc7', 'is_msc': True}])

    def test_the_explorer_waits_for_the_record_to_end(self):
        # a read that stops inside the count must not end the wait with a truncated record
        head = 'MassStorage device is mounted\r\nDisk Size: 606'
        text = head + '20800 512-byte blocks: 29600 MB\r\n'
        chunks = [head.encode(), text[len(head):].encode()]
        seen = []

        class Console(FakeSerial):
            def reset_input_buffer(self):
                raise StopIteration   # the capacity check is done: stop before the shell commands

        usbtest_harness.patch(self, hil_test, 'open_console_reset', lambda b: Console(chunks))
        usbtest_harness.patch(self, hil_test, 'enum_timeout', lambda: 1)
        usbtest_harness.patch(self, hil_test.time, 'sleep', lambda s: None)
        usbtest_harness.patch(self, hil_test, 'check_msc_info', lambda text, devs: seen.append(text))
        usbtest_harness.patch(self, sys, 'stdout', usbtest_harness.Out())
        with self.assertRaises(StopIteration):
            hil_test.test_host_msc_file_explorer({'dev_attached': [self.LEXAR]})
        self.assertEqual(seen, [text])


if __name__ == '__main__':
    unittest.main()
