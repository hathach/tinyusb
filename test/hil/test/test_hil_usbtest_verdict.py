#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""usbtest's per-case verdict and the battery's exit status. run_case runs a fake testusb
that prints kernel tools/usb/testusb.c's own strings; main() runs with stubbed discovery.
The strings are checked against the rig's testusb by a real battery run."""
import io
import json
import os
import stat
import sys
import unittest
from contextlib import redirect_stderr, redirect_stdout
from tempfile import TemporaryDirectory

TEST_DIR = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(TEST_DIR))
import usbtest_harness  # noqa: E402 - stubs serial before hil_test is imported
import usbtest  # noqa: E402

NODE = '/dev/bus/usb/001/005'
# testusb -D <node> -t <n>: stderr first, then stdout. FAKE_OUT is a format string over the
# node and case; '%s' is the device name as testusb prints it.
FAKE_TESTUSB = f'''#!{sys.executable}
import os, sys
node = sys.argv[sys.argv.index('-D') + 1]
case = int(sys.argv[sys.argv.index('-t') + 1])
sys.stdout.write(os.environ['FAKE_OUT'].replace('%s', node).replace('%d', str(case)))
sys.exit(int(os.environ.get('FAKE_RC', '0')))
'''
SPEED = '%s: %s may see only control tests\nhigh speed\t%s\t0\n'   # testusb.c main, handle_testdev


class RunCase(unittest.TestCase):
    def setUp(self):
        td = TemporaryDirectory()
        self.addCleanup(td.cleanup)
        self.testusb = os.path.join(td.name, 'testusb')
        with open(self.testusb, 'w') as f:
            f.write(FAKE_TESTUSB)
        os.chmod(self.testusb, stat.S_IRWXU)
        usbtest_harness.patch(self, usbtest, 'dmesg_tail', lambda: '')
        usbtest_harness.patch(self, usbtest.os, 'access', lambda path, mode: True)   # no sudo wrap
        self.addCleanup(os.environ.pop, 'FAKE_OUT', None)
        self.addCleanup(os.environ.pop, 'FAKE_RC', None)

    def case(self, out, num=1, rc=0):
        os.environ['FAKE_OUT'], os.environ['FAKE_RC'] = out, str(rc)
        return usbtest.run_case(num, {'node': NODE, 'speed': '480'}, self.testusb, False, 30)

    def test_pass_line(self):
        r = self.case(SPEED + '%s test %d,    3.004567 secs\n')   # "%s test %d, %4d.%.06d secs"
        self.assertEqual((r['status'], r['secs']), ('PASS', 3.004567))

    def test_perf_case_reports_throughput(self):
        r = self.case(SPEED + '%s test %d,    2.000000 secs\n', num=27)
        self.assertEqual(r['status'], 'PASS')
        self.assertEqual(r['mbps'], round(128 * 1024 * 32 / 2 / 1e6, 2))   # HS -c 128 -s 1024 -g 32

    def test_error_line(self):
        r = self.case(SPEED + '%s test %d --> 110 (Connection timed out)\n')   # "%s test %d --> %d (%s)"
        self.assertEqual((r['status'], r['detail']), ('FAIL', 'errno 110 (Connection timed out)'))

    def test_another_cases_result_line_is_not_this_ones(self):
        r = self.case(SPEED + '%s test 2,    1.000000 secs\n', num=1)
        self.assertEqual(r['status'], 'NOTRUN')

    def test_opened_but_no_result_is_a_kernel_skip(self):
        r = self.case(SPEED)                           # -EOPNOTSUPP: testusb continues silently
        self.assertEqual(r['status'], 'NOTRUN')

    def test_other_nodes_it_could_not_open_do_not_change_a_skip(self):
        r = self.case('/dev/bus/usb/002/001: Permission denied\n' + SPEED)   # ftw's perror(name)
        self.assertEqual(r['status'], 'NOTRUN')

    def test_never_opened_the_node_is_a_failure_not_a_skip(self):
        for out, rc in (("can't open dev file r/w: Permission denied\n", 0),
                        ("must specify '-a' or '-D dev', or DEVICE=/dev/bus/usb/BBB/DDD in env\n", 1),
                        ('USB device files are missing\n', 255),
                        ('', 0)):
            r = self.case(out, rc=rc)
            self.assertEqual(r['status'], 'FAIL', out)
            self.assertEqual(r['detail'], f'testusb did not run the case (rc {rc})')
            self.assertEqual(r['stderr'], out.strip())

    def test_speed_line_for_another_node_does_not_count(self):
        r = self.case('high speed\t/dev/bus/usb/001/006\t0\n')
        self.assertEqual(r['status'], 'FAIL')


class ExitStatus(unittest.TestCase):
    """What main() returns and says when a battery ends without a clean verdict."""

    def main(self, *extra, find=None, status='PASS', stranded=False):
        usbtest_harness.stub_device(self, usbtest, lambda num, d, tu, quick, timeout:
                                    {'num': num, 'name': 'x', 'params': '', 'status': status, 'secs': 0.1,
                                     **({'_proc': None} if status == 'HUNG' else {})})
        usbtest_harness.patch(self, usbtest, 'register_usbtest_id', lambda: None)
        usbtest_harness.patch(self, usbtest, 'bind_usbtest', lambda d: None)
        if find:
            usbtest_harness.patch(self, usbtest, 'find_device', find)
        if stranded:
            hu = type('Hu', (), {'path_stranded': staticmethod(lambda p: True),
                                 'strand_note': staticmethod(lambda: '')})
            usbtest_harness.patch(self, usbtest, '_hu', lambda: hu)
        usbtest_harness.argv(self, *extra)
        out, err = usbtest_harness.Out(), io.StringIO()
        with redirect_stdout(out), redirect_stderr(err):
            rc = usbtest.main()
        text = out.getvalue()
        return rc, json.loads(text[text.find('{'):]), err.getvalue()

    def lookups(self, *answers):
        seq = iter(answers)
        return lambda serial, first=False: next(seq)

    def test_ambiguous_device_after_the_last_case_is_not_a_success(self):
        rc, data, err = self.main('--tests', '0', find=self.lookups(
            dict(usbtest_harness.DEV), {'ambiguous': ['1-1', '1-2']}))
        self.assertEqual((data['passed'], data['wedged']), (1, True))
        self.assertEqual(rc, 1)
        self.assertIn('unrecovered hang', err)

    def test_unreadable_device_after_the_last_case_is_not_a_success(self):
        rc, data, err = self.main('--tests', '0', stranded=True,
                                  find=self.lookups(dict(usbtest_harness.DEV), None))
        self.assertEqual((rc, data['wedged']), (1, True))

    def test_clean_run_still_exits_0(self):
        rc, data, err = self.main('--tests', '0')
        self.assertEqual((rc, data['wedged'], err), (0, False, ''))

    def test_hung_case_without_recovery_stays_wedged(self):
        rc, data, err = self.main('--tests', '0', status='HUNG')
        self.assertEqual((rc, data['wedged']), (1, True))
        self.assertIn('unrecovered hang', err)

    def test_duplicate_case_numbers_are_refused(self):
        with self.assertRaises(SystemExit) as e:
            self.main('--tests', '0,1,0')
        self.assertIn('case 0 is listed twice', str(e.exception.code))


if __name__ == '__main__':
    unittest.main()
