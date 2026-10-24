#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Reset_TT for the cases and transitions that can strand a hub TT buffer (helper/hil_tt,
agentrc's hil_tt, whose own tests cover the TT lookup, the request and the helper's outcomes):
when usbtest and hil_test send it, and that its child runs through this harness's runner.
Whether the hub frees the buffer is proven on the rig."""
import io
import json
import os
import subprocess
import sys
import types
import unittest
from contextlib import redirect_stderr, redirect_stdout
from pathlib import Path
from tempfile import TemporaryDirectory

TEST_DIR = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(TEST_DIR))
import usbtest_harness  # noqa: E402 - stubs serial before hil_test is imported
import hil_flash  # noqa: E402
import hil_test  # noqa: E402
import usbtest  # noqa: E402
from helper import hil_tt, hil_util  # noqa: E402


class Battery(unittest.TestCase):
    """usbtest resets after each unlink case and each failed case, and an unconfirmed reset
    ends the battery unsuccessfully without calling the device wedged."""

    def main(self, tests, ok=True, tt=None):
        fails = {13: 'errno 110 (Connection timed out)', 29: 'errno 32 (Broken pipe)'}
        last, resets = [], []

        def run_case(num, d, tu, quick, timeout):
            last[:] = [num]
            r = {'num': num, 'name': 'x', 'params': ''}
            if num in fails:
                r.update(status='FAIL', detail=fails[num])
            else:
                r.update(status='PASS', secs=0.1)
            return r

        tt = tt or types.SimpleNamespace(reset_tt=lambda busport, speed, run: resets.append(last[0]) or ok)
        usbtest_harness.stub_device(self, usbtest, run_case)
        usbtest_harness.patch(self, usbtest, 'register_usbtest_id', lambda: None)
        usbtest_harness.patch(self, usbtest, 'bind_usbtest', lambda d: None)
        usbtest_harness.patch(self, usbtest, '_tt', lambda: tt)
        usbtest_harness.argv(self, '--tests', tests)
        out = usbtest_harness.Out()
        with redirect_stdout(out), redirect_stderr(io.StringIO()):
            rc = usbtest.main()
        return resets, json.loads(out.getvalue()), rc

    def test_which_cases_reset(self):
        resets, data, _ = self.main('1,11,12,24,13,29')
        self.assertEqual(resets, [11, 12, 24, 13, 29])
        self.assertEqual((data['failed'], data['wedged']), (2, False))

    def test_unconfirmed_reset_ends_the_battery(self):
        resets, data, rc = self.main('1,11,12', ok=False)
        self.assertEqual(resets, [11])
        self.assertEqual([c['status'] for c in data['cases']], ['PASS', 'FAIL', 'BUDGET'])
        self.assertEqual((data['wedged'], rc > 0), (False, True))

    def test_unconfirmed_reset_after_the_last_case_is_not_a_pass(self):
        _, data, rc = self.main('11', ok=False)
        self.assertEqual(([c['status'] for c in data['cases']], rc > 0), (['FAIL'], True))

    def test_the_child_runs_through_the_harness_runner(self):
        # agentrc's hil_tt itself, its child caught where hil_util.run_cmd is looked up per call
        usbtest_harness.patch(self, hil_tt, 'tt_port', lambda b, s: ('/dev/bus/usb/005/058', 2))
        usbtest_harness.patch(self, hil_tt, '_unconfirmed', set())
        usbtest_harness.patch(self, hil_tt, '_process', lambda: self.fail("hil_tt's own runner, not the harness's"))
        spawned = []
        usbtest_harness.patch(self, hil_util, 'run_cmd', lambda cmd, **kw: spawned.append((cmd, kw))
                              or subprocess.CompletedProcess(cmd, 124, '', None))
        _, data, _ = self.main('11', tt=hil_tt)
        (cmd, kw), = spawned
        self.assertEqual(cmd[cmd.index('-I'):], ['-I', hil_tt.__file__, '/dev/bus/usb/005/058', '2'])
        self.assertIn(os.path.join('lib', 'agentrc', ''), hil_tt.__file__)
        self.assertEqual(kw, {'timeout': hil_tt.HELPER_TIMEOUT, 'quiet': True})
        self.assertEqual(data['cases'][0]['status'], 'FAIL')   # a timed-out helper is unconfirmed


class BeforeFlash(unittest.TestCase):
    """hil_test resets the port's TT before each flash, from one unambiguous live port, else
    the last port it saw the board on."""

    def resets(self, *scans, ok=True):
        it = iter(scans)
        usbtest_harness.patch(self, hil_test.usb_observe, 'usb_scan', lambda **kw: next(it))
        usbtest_harness.patch(self, hil_test.usb_observe, 'read_sysfs', lambda path, *a: '12')
        seen = []
        usbtest_harness.patch(self, hil_test.hil_tt, 'reset_tt',
                              lambda busport, speed, run: seen.append((busport, speed)) or ok)
        usbtest_harness.patch(self, hil_test, '_dut_port', {})
        self.results = [hil_test.reset_dut_tt({'uid': 'U', 'name': 'b'}) for _ in scans]
        return seen

    def test_uses_the_live_port_then_the_last_seen_one(self):
        seen = self.resets([{'busport': '5-1.2', 'dir': '/x'}], [])
        self.assertEqual(seen, [('5-1.2', '12')] * 2)

    def test_a_gone_device_is_reset_only_if_last_seen_at_full_speed(self):
        td = TemporaryDirectory()
        self.addCleanup(td.cleanup)
        intf = Path(td.name) / '5-1' / '5-1:1.0'
        intf.mkdir(parents=True)
        (intf / 'bInterfaceProtocol').write_text('02\n')
        (intf.parent / 'busnum').write_text('5\n')
        (intf.parent / 'devnum').write_text('58\n')
        usbtest_harness.patch(self, hil_tt, 'SYS_USB', Path(td.name))
        usbtest_harness.patch(self, hil_tt, '_unconfirmed', set())
        usbtest_harness.patch(self, hil_tt, '_process', lambda: self.fail("hil_tt's own runner, not the harness's"))
        spawned = []
        usbtest_harness.patch(self, hil_util, 'run_cmd', lambda cmd, **kw: spawned.append(cmd)
                              or subprocess.CompletedProcess(cmd, 0, '', None))
        for speed, helpers in (('480', 0), ('12', 1)):
            spawned.clear()
            it = iter([[{'busport': '5-1.2', 'dir': '/x'}], []])
            sysfs = iter([speed])   # read only while the device is present
            usbtest_harness.patch(self, hil_test.usb_observe, 'usb_scan', lambda **kw: next(it))
            usbtest_harness.patch(self, hil_test.usb_observe, 'read_sysfs', lambda path, *a: next(sysfs))
            usbtest_harness.patch(self, hil_test, '_dut_port', {})
            hil_test.reset_dut_tt({'uid': 'U', 'name': 'b'})
            spawned.clear()
            self.assertTrue(hil_test.reset_dut_tt({'uid': 'U', 'name': 'b'}))
            self.assertEqual(len(spawned), helpers, speed)

    def test_ambiguity_is_left_alone_and_keeps_the_cache(self):
        two = [{'busport': '5-1.3', 'dir': '/x'}, {'busport': '5-1.4', 'dir': '/y'}]
        self.assertEqual(self.resets(two), [])
        seen = self.resets([{'busport': '5-1.2', 'dir': '/x'}], two, [])
        self.assertEqual(seen, [('5-1.2', '12')] * 2)

    def test_unconfirmed_reset_is_returned(self):
        two = [{'busport': '5-1.3', 'dir': '/x'}, {'busport': '5-1.4', 'dir': '/y'}]
        self.resets([{'busport': '5-1.2', 'dir': '/x'}], two, ok=False)
        self.assertEqual(self.results, [False, True])

    def test_unconfirmed_reset_is_named_in_the_attempt(self):
        logs = []
        usbtest_harness.patch(self, hil_flash, 'find_firmware', lambda *a, **k: Path('/fw.elf'))
        usbtest_harness.patch(self, hil_flash, 'flash_primitive',
                              lambda name: lambda *a, **k: subprocess.CompletedProcess('flash', 1, ''))
        usbtest_harness.patch(self, hil_test, 'reset_dut_tt', lambda b: False)
        usbtest_harness.patch(self, hil_test, 'skip_flash', False)
        usbtest_harness.patch(self, hil_test, 'max_retry', 1)
        usbtest_harness.patch(self, hil_test, 'log_line', lambda m, **k: logs.append(m))
        with redirect_stdout(io.StringIO()):
            hil_test.test_example({'name': 'b', 'uid': 'U', 'flasher': {'name': 'openocd'}},
                                  'v', 'device/cdc_msc')
        self.assertIn('Reset_TT unconfirmed', logs[-1])

    def test_reset_runs_before_the_flash(self):
        order = []

        usbtest_harness.patch(self, hil_flash, 'find_firmware', lambda *a, **k: Path('/fw.elf'))
        usbtest_harness.patch(self, hil_flash, 'flash_primitive',
                              lambda name: lambda *a, **k: order.append('flash')
                              or subprocess.CompletedProcess('flash', 1, ''))
        usbtest_harness.patch(self, hil_test, 'reset_dut_tt', lambda b: order.append('reset') or True)
        usbtest_harness.patch(self, hil_test, 'skip_flash', False)
        usbtest_harness.patch(self, hil_test, 'max_retry', 1)
        usbtest_harness.patch(self, hil_test, 'log_line', lambda *a, **k: None)
        with redirect_stdout(io.StringIO()):
            hil_test.test_example({'name': 'b', 'uid': 'U', 'flasher': {'name': 'openocd'}},
                                  'v', 'device/cdc_msc')
        self.assertEqual(order[:2], ['reset', 'flash'])


if __name__ == '__main__':
    unittest.main()
