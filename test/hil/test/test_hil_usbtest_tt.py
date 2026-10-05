#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Reset_TT for the cases and transitions that can strand a hub TT buffer (helper/hil_tt):
which TT a DUT sits behind (a fake sysfs tree), the request, the helper's outcomes, and when
usbtest and hil_test send it. Whether the hub frees the buffer is proven on the rig."""
import io
import json
import os
import struct
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


class TtPort(unittest.TestCase):
    def setUp(self):
        td = TemporaryDirectory()
        self.addCleanup(td.cleanup)
        self.sys = Path(td.name)
        usbtest_harness.patch(self, hil_tt, 'SYS_USB', self.sys)

    def hub(self, name, protocol='02', busnum=5, devnum=58, config=1):
        intf = self.sys / name / f'{name}:{config}.0'
        intf.mkdir(parents=True)
        (intf / 'bInterfaceProtocol').write_text(protocol + '\n')
        (self.sys / name / 'busnum').write_text(f'{busnum}\n')
        (self.sys / name / 'devnum').write_text(f'{devnum}\n')

    def test_full_speed_device_behind_a_multi_tt_hub(self):
        self.hub('5-1')
        self.assertEqual(hil_tt.tt_port('5-1.2', '12'), ('/dev/bus/usb/005/058', 2))

    def test_nested_hub_and_any_configuration_value(self):
        self.hub('3-1.4', busnum=3, devnum=7, config=2)
        self.assertEqual(hil_tt.tt_port('3-1.4.7', '1.5'), ('/dev/bus/usb/003/007', 7))

    def test_no_reset_where_it_cannot_help_or_would_disturb_others(self):
        self.hub('5-1')
        self.hub('6-1', protocol='01')    # single TT: shared by every port
        self.hub('7-1', protocol='00')    # hub running at full speed: no TT
        for busport, speed in (('5-1.2', '480'),   # high speed: no split
                               ('5-1', '12'),      # root port
                               ('6-1.1', '12'), ('7-1.1', '12'),
                               ('8-1.1', '12')):   # hub gone
            self.assertIsNone(hil_tt.tt_port(busport, speed), busport)


class Request(unittest.TestCase):
    def test_reset_tt_request_bytes(self):
        sent = []
        usbtest_harness.patch(self, hil_tt.fcntl, 'ioctl',
                              lambda f, req, arg: sent.append((req, arg)))
        with TemporaryDirectory() as td:
            node = os.path.join(td, 'node')
            open(node, 'w').close()
            hil_tt.reset_tt_ioctl(node, 3)
        (req, arg), = sent
        # bmRequestType, bRequest, wValue, wIndex, wLength, timeout ms, data pointer
        self.assertEqual(struct.unpack('@BBHHHIP', arg), (0x23, 9, 0, 3, 0, 1000, 0))
        if struct.calcsize('P') == 8:   # the ioctl the rig's LP64 kernel answers
            self.assertEqual((req, len(arg)), (0xC0185500, 24))


class Helper(unittest.TestCase):
    """reset_tt through its real subprocess path: a timeout is unconfirmed and stops later
    helpers for that hub; any other failure only reports."""

    def reset(self, *outcomes):
        seq = iter(outcomes)
        calls = []

        def run_cmd(cmd, **kw):
            calls.append(cmd)
            o = next(seq)
            if isinstance(o, BaseException):
                raise o
            return subprocess.CompletedProcess(cmd, o, 'why', None)

        usbtest_harness.patch(self, hil_util, 'run_cmd', run_cmd)
        usbtest_harness.patch(self, hil_tt, 'tt_port', lambda b, s: ('/dev/bus/usb/005/058', 2))
        usbtest_harness.patch(self, hil_tt, '_unconfirmed', set())
        err = io.StringIO()
        with redirect_stderr(err):
            oks = [hil_tt.reset_tt('5-1.2', '12') for _ in outcomes]
        return oks, len(calls), err.getvalue()

    def test_timeout_is_unconfirmed_and_not_repeated_for_that_hub(self):
        oks, spawned, err = self.reset(124, 0)
        self.assertEqual((oks, spawned, 'timed out' in err), ([False, False], 1, True))

    def test_a_real_timeout_keeps_its_banner_off_stdout(self):
        usbtest_harness.patch(self, hil_tt, 'tt_port', lambda b, s: ('/dev/null', 2))
        usbtest_harness.patch(self, hil_tt, '_unconfirmed', set())
        usbtest_harness.patch(self, hil_tt, 'HELPER_TIMEOUT', 1)
        usbtest_harness.patch(self, hil_util, 'REAP_GRACE', 1)
        real_run = hil_util.run_cmd     # the real runner, on a child that outlives the bound
        usbtest_harness.patch(self, hil_util, 'run_cmd',
                              lambda cmd, **kw: real_run(['sleep', '30'], **kw))
        out, err = io.StringIO(), io.StringIO()
        with redirect_stdout(out), redirect_stderr(err):
            ok = hil_tt.reset_tt('5-1.2', '12')
        self.assertEqual((ok, out.getvalue()), (False, ''))
        self.assertIn('COMMAND TIMEOUT', err.getvalue())

    def test_other_failures_only_report(self):
        oks, spawned, err = self.reset(1, FileNotFoundError('sudo'), 0)
        self.assertEqual((oks, spawned, err.count('failed')), ([True] * 3, 3, 2))


class Battery(unittest.TestCase):
    """usbtest resets after each unlink case and each failed case, and an unconfirmed reset
    ends the battery unsuccessfully without calling the device wedged."""

    def main(self, tests, ok=True):
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

        tt = types.SimpleNamespace(reset_tt=lambda busport, speed: resets.append(last[0]) or ok)
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


class BeforeFlash(unittest.TestCase):
    """hil_test resets the port's TT before each flash, from one unambiguous live port, else
    the last port it saw the board on."""

    def resets(self, *scans, ok=True):
        it = iter(scans)
        usbtest_harness.patch(self, hil_test.hil_util, 'usb_scan', lambda **kw: next(it))
        usbtest_harness.patch(self, hil_test.hil_util, 'read_sysfs', lambda path, *a: '12')
        seen = []
        usbtest_harness.patch(self, hil_test.hil_tt, 'reset_tt',
                              lambda busport, speed: seen.append((busport, speed)) or ok)
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
        spawned = []
        usbtest_harness.patch(self, hil_util, 'run_cmd', lambda cmd, **kw: spawned.append(cmd)
                              or subprocess.CompletedProcess(cmd, 0, '', None))
        for speed, helpers in (('480', 0), ('12', 1)):
            spawned.clear()
            it = iter([[{'busport': '5-1.2', 'dir': '/x'}], []])
            sysfs = iter([speed])   # read only while the device is present
            usbtest_harness.patch(self, hil_test.hil_util, 'usb_scan', lambda **kw: next(it))
            usbtest_harness.patch(self, hil_test.hil_util, 'read_sysfs', lambda path, *a: next(sysfs))
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
