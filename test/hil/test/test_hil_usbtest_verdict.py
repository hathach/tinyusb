#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""usbtest's per-case options and throughput, and the battery's exit status. The case run and
its verdict are agentrc's hil_usbtest, tested there; run_case here runs a fake testusb that
prints kernel tools/usb/testusb.c's own strings, main() runs with stubbed discovery. The
strings are checked against the rig's testusb by a real battery run."""
import io
import json
import os
import stat
import sys
import types
import unittest
from contextlib import redirect_stderr, redirect_stdout
from tempfile import TemporaryDirectory

TEST_DIR = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(TEST_DIR))
import usbtest_harness  # noqa: E402 - stubs serial before hil_test is imported
import usbtest  # noqa: E402

NODE = '/dev/bus/usb/001/005'
# testusb -A <node> -D <node> -t <n>: stderr first, then stdout. FAKE_OUT is a format string over the
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
        usbtest_harness.patch(self, usbtest.hil_usbtest, 'dmesg_tail', lambda: '')
        usbtest_harness.patch(self, usbtest.hil_usbtest, 'needs_sudo', lambda node: False)
        self.addCleanup(os.environ.pop, 'FAKE_OUT', None)
        self.addCleanup(os.environ.pop, 'FAKE_RC', None)

    def case(self, out, num=1, rc=0):
        os.environ['FAKE_OUT'], os.environ['FAKE_RC'] = out, str(rc)
        return usbtest.run_case(num, {'node': NODE, 'speed': '480'}, self.testusb, False, 30)

    def test_pass_line_keeps_the_case_identity(self):
        r = self.case(SPEED + '%s test %d,    3.004567 secs\n')   # "%s test %d, %4d.%.06d secs"
        self.assertEqual(r, {'num': 1, 'name': usbtest.CASE_NAMES[1], 'params': usbtest.PARAMS[1][1],
                             'status': 'PASS', 'secs': 3.004567})

    def test_perf_case_reports_throughput(self):
        r = self.case(SPEED + '%s test %d,    2.000000 secs\n', num=27)
        self.assertEqual(r['status'], 'PASS')
        self.assertEqual(r['mbps'], round(128 * 1024 * 32 / 2 / 1e6, 2))   # HS -c 128 -s 1024 -g 32

    def test_a_failed_perf_case_has_no_throughput(self):
        r = self.case(SPEED + '%s test %d --> 110 (Connection timed out)\n', num=27)
        self.assertEqual(r['status'], 'FAIL')
        self.assertNotIn('mbps', r)


class Options(unittest.TestCase):
    """The provider gets each case's options for the device's speed verbatim, --quick dividing
    every count by 8, and main's WEDGE_CONFIRM_S and sudo decision."""

    def options(self, speed, quick, num=1):
        seen = []
        usbtest_harness.patch(self, usbtest.hil_usbtest, 'run_case', lambda *a, **kw: seen.append((a, kw))
                              or {'status': 'NOTRUN'})
        usbtest.run_case(num, {'node': NODE, 'speed': speed}, '/x/testusb', quick, 30)
        (args, kw), = seen
        self.assertEqual(args[:3] + args[4:], ('/x/testusb', NODE, num, 30))
        self.assertEqual(kw, {'wedge_confirm': usbtest.WEDGE_CONFIRM_S, 'sudo_forbidden': usbtest.sudo_forbidden})
        return args[3]

    def test_full_and_high_speed(self):
        self.assertEqual(self.options('12', False), ['-c', '128', '-s', '1024', '-v', '512'])
        self.assertEqual(self.options('480', False), ['-c', '512', '-s', '1024', '-v', '512'])

    def test_quick_divides_the_count(self):
        self.assertEqual(self.options('480', True), ['-c', '64', '-s', '1024', '-v', '512'])
        self.assertEqual(self.options('12', True, num=0), ['-c', '1'])


class ExitStatus(unittest.TestCase):
    """What main() returns and says when a battery ends without a clean verdict."""

    def main(self, *extra, find=None, status='PASS', stranded=False):
        usbtest_harness.stub_device(self, usbtest, lambda num, d, tu, quick, timeout:
                                    {'num': num, 'name': 'x', 'params': '', 'status': status, 'secs': 0.1})
        usbtest_harness.patch(self, usbtest, 'register_usbtest_id', lambda: None)
        usbtest_harness.patch(self, usbtest, 'bind_usbtest', lambda d: None)
        if find:
            usbtest_harness.patch(self, usbtest, 'find_device', find)
        if stranded:
            uo = types.SimpleNamespace(path_stranded=lambda p: True)
            usbtest_harness.patch(self, usbtest, '_uo', lambda: uo)
        usbtest_harness.argv(self, *extra)
        out, err = usbtest_harness.Out(), io.StringIO()
        with redirect_stdout(out), redirect_stderr(err):
            rc = usbtest.main()
        text = out.getvalue()
        return rc, json.loads(text[text.find('{'):]), err.getvalue()

    def lookups(self, *answers):
        seq = iter(answers)
        return lambda serial: next(seq)

    def test_ambiguous_device_after_the_last_case_is_not_a_success(self):
        rc, data, _ = self.main('--tests', '0', find=self.lookups(
            dict(usbtest_harness.DEV), {'ambiguous': ['1-1', '1-2']}))
        self.assertEqual((rc, data['passed'], data['wedged']), (1, 1, True))

    def test_unreadable_device_after_the_last_case_is_not_a_success(self):
        rc, data, _ = self.main('--tests', '0', stranded=True,
                                find=self.lookups(dict(usbtest_harness.DEV), None))
        self.assertEqual((rc, data['wedged']), (1, True))

    def test_drop_after_the_last_case_fails_its_pass_only(self):
        for status, want in (('PASS', 'FAIL'), ('FAIL', 'FAIL'), ('NOTRUN', 'NOTRUN')):
            rc, data, _ = self.main('--tests', '0', status=status,
                                    find=self.lookups(dict(usbtest_harness.DEV), None))
            case = data['cases'][0]
            self.assertEqual((rc, data['wedged'], case['status']), (1, False, want), status)
            self.assertEqual('dropped off the bus' in case.get('detail', ''), status == 'PASS')

    def test_drop_mid_battery_keeps_the_pass_and_budgets_the_rest(self):
        rc, data, _ = self.main('--tests', '0,1', find=self.lookups(dict(usbtest_harness.DEV), None))
        self.assertEqual([c['status'] for c in data['cases']], ['PASS', 'BUDGET'])
        self.assertEqual((rc, data['passed'], data['notrun']), (1, 1, 1))

    def test_clean_run_still_exits_0(self):
        rc, data, err = self.main('--tests', '0')
        self.assertEqual((rc, data['wedged'], err), (0, False, ''))

    def test_failed_case_points_at_the_skill_once(self):
        for status in ('FAIL', 'NOTRUN'):
            rc, data, err = self.main('--tests', '0,1', status=status)
            self.assertEqual((rc, data['failed']), (2, 2))
            self.assertEqual(err.count('usbtest skill'), 1, status)

    def test_duplicate_case_numbers_are_refused(self):
        with self.assertRaises(SystemExit) as e:
            self.main('--tests', '0,1,0')
        self.assertIn('case 0 is listed twice', str(e.exception.code))


if __name__ == '__main__':
    unittest.main()
