"""wedge_drill.py with every hardware call stubbed: the refusals, each verdict, that the halt
never outlives the battery, that the drill's own cleanup is kept apart from the harness's
recovery, and that the lock is released with one verdict line on every path. Plumbing only: the
wedge and its recovery are proved by a run on a rig board."""
import importlib.util
import io
import json
import subprocess
import sys
import tempfile
import threading
import unittest
from contextlib import redirect_stderr, redirect_stdout
from pathlib import Path
from unittest import mock

REPO = Path(__file__).resolve().parents[2]
SCRIPTS = REPO / '.claude' / 'skills' / 'usbtest' / 'scripts'
sys.path.insert(0, str(SCRIPTS))
spec = importlib.util.spec_from_file_location('wedge_drill', SCRIPTS / 'wedge_drill.py')
wedge_drill = importlib.util.module_from_spec(spec)
spec.loader.exec_module(wedge_drill)
run_case = wedge_drill.run_case

JLINK_REC = {'name': 'openocd', 'args': '-f interface/jlink.cfg -f target/stm32f4x.cfg'}
ROSTER = {'boards': [
    {'name': 'ok', 'uid': 'UID1', 'flasher': {'name': 'jlink', 'uid': 'P1', 'args': '-device X'},
     'flasher_recover': JLINK_REC, 'tests': {'device': True}},
    {'name': 'norec', 'uid': 'UID2', 'flasher': {'name': 'jlink', 'uid': 'P2', 'args': '-device X'},
     'tests': {'device': True}},
    {'name': 'esp', 'uid': 'UID3', 'flasher': {'name': 'esptool', 'uid': 'P3'}, 'tests': {'device': True}},
]}
RECOVERED = ('auto-recovering: resetting ok via openocd probe\nreset rc 0\n'
             'recovery freed the device: testusb reaped\n')
HUNG = {'passed': 0, 'failed': 1, 'notrun': 0, 'wedged': False,
        'cases': [{'num': 27, 'status': 'HUNG', 'detail': 'testusb stuck in D state'}]}
SMOKE_CASE = 1
SMOKE_PASS = {'passed': 1, 'failed': 0, 'notrun': 0, 'wedged': False,
              'cases': [{'num': 1, 'status': 'PASS'}]}


class Lock:
    closed = False

    def close(self):
        self.closed = True


class Rig:
    def __init__(self, test):
        self.calls = []
        self.lock = Lock()
        self.halt_rc = 0
        self.halt_raises = None
        self.halt_raises_after = None   # raised after the halt landed, like a broken pipe
        self.case27 = (HUNG, RECOVERED, '')
        self.smoke = (SMOKE_PASS, '', '')
        self.ids = [('/dev/bus/usb/001/005', 5), ('/dev/bus/usb/001/006', 6)]
        self.case_seen = True
        self.running_after = True
        self.left_testusb = None     # a testusb still on the node after the drill's cleanup
        self.flash_errors = {}       # firmware -> error string
        self.case_pid_alive_at_end = False
        self.reset_raises = None
        self.smoke_raises = None
        self.flash_raises = {}       # firmware -> exception
        self.halted = threading.Event()
        tmp = tempfile.TemporaryDirectory()
        test.addCleanup(tmp.cleanup)
        self.config = Path(tmp.name) / 'rig.json'
        self.config.write_text(json.dumps(ROSTER))

        def battery(board, fw, tests, timeout):
            self.calls.append(('battery', tuple(tests)))
            if tests == [SMOKE_CASE] and self.smoke_raises:
                raise self.smoke_raises
            if tests == [27]:
                if self.case_seen:
                    self.halted.wait(5)   # a wedged case does not end before the halt
                return self.case27
            return self.smoke

        def halt(board, timeout):
            self.calls.append(('halt', board['flasher']['name'], timeout))
            self.halted.set()
            if self.halt_raises:
                raise self.halt_raises
            if self.halt_raises_after:
                raise self.halt_raises_after
            return subprocess.CompletedProcess('halt', self.halt_rc, 'halt out', '')

        def reset(board, timeout):
            self.calls.append(('cleanup-reset', board['flasher']['name']))
            if self.reset_raises:
                raise self.reset_raises
            return subprocess.CompletedProcess('reset', 0, '', '')

        def flash(board, fw):
            self.calls.append(('flash', fw))
            if fw in self.flash_raises:
                raise self.flash_raises[fw]
            return self.flash_errors.get(fw)

        def alive(pid):
            if self.calls and self.calls[-1][0] == 'cleanup-reset':
                return self.case_pid_alive_at_end
            return not self.halted.is_set() or self.running_after

        stubs = (
            (run_case.hil_flash, 'find_firmware', lambda variant, example, flasher=None: Path(f'/fw/{example}')),
            (run_case, 'flash', flash),
            (run_case, 'enumerated', lambda uid: True),
            (run_case, 'battery', battery),
            (run_case, 'live_peers', lambda: ([], True)),
            (run_case.hil_lock, 'acquire_board_lock', lambda name, reason: self.lock),
            (run_case.hil_lock, 'clear_record', lambda fh: None),
            (run_case.hil_flash, 'halt_openocd', halt),
            (run_case.hil_flash, 'reset_primitive', lambda name: reset),
            (wedge_drill, 'identity', lambda uid: self.ids.pop(0) if len(self.ids) > 1 else self.ids[0]),
            (wedge_drill, 'testusb_pid', lambda node, case=None: (4242 if self.case_seen else None)
             if case == 27 else self.left_testusb),
            (wedge_drill, 'age', lambda pid: 0.5),
            (wedge_drill, 'alive', alive),
        )
        for obj, name, value in stubs:
            patcher = mock.patch.object(obj, name, value)
            patcher.start()
            test.addCleanup(patcher.stop)

    def run(self, *argv):
        out, err = io.StringIO(), io.StringIO()
        with mock.patch.object(sys, 'argv', ['wedge_drill.py', '--config', str(self.config), *argv]), \
                redirect_stdout(out), redirect_stderr(err):
            rc = wedge_drill.main()
        lines = out.getvalue().splitlines()
        self.verdicts = [l for l in lines if l.startswith('{')]
        return rc, json.loads(lines[-1])

    def parked(self):
        return ('flash', '/fw/device/board_test') in self.calls


class Verdicts(unittest.TestCase):
    def test_a_recovered_wedge_passes(self):
        rig = Rig(self)
        rc, r = rig.run('--board', 'ok', '--delay', '0')
        self.assertEqual((rc, r['drill']), (0, 'pass'), r)
        self.assertEqual(r['recovery'], {'attempted': True, 'reset_rc': 0, 'reaped': True})
        self.assertEqual(r['device'], {'before': 5, 'after': 6})
        self.assertEqual(r['smoke'], 'PASS')
        self.assertIsNone(r['cleanup'])
        self.assertEqual(r['cases'][0]['status'], 'HUNG')   # the case itself stays a failure
        self.assertNotIn(('cleanup-reset', 'openocd'), rig.calls)
        self.assertTrue(rig.parked() and rig.lock.closed)
        # no flash between the recovery and the smoke case
        batteries = [i for i, c in enumerate(rig.calls) if c[0] == 'battery']
        self.assertFalse(any(c[0] == 'flash' for c in rig.calls[batteries[0]:batteries[1]]))

    def test_a_case_that_ends_before_the_halt_is_inconclusive(self):
        rig = Rig(self)
        rig.case_seen = False
        rc, r = rig.run('--board', 'ok', '--delay', '0')
        self.assertEqual((rc, r['drill']), (1, 'inconclusive'))
        self.assertNotIn('halt', [c[0] for c in rig.calls])
        self.assertIsNone(r['cleanup'])
        self.assertTrue(rig.parked() and rig.lock.closed)

    def test_a_failed_halt_is_inconclusive_and_cleaned_up(self):
        rig = Rig(self)
        rig.halt_rc = 1
        rig.case27 = ({**HUNG, 'cases': [{'num': 27, 'status': 'PASS'}]}, '', '')
        rc, r = rig.run('--board', 'ok', '--delay', '0')
        self.assertEqual(r['drill'], 'inconclusive')
        self.assertIn('halt failed', r['halt']['note'])
        self.assertEqual(r['cleanup'], 'drill reset rc 0')

    def test_no_hung_case_is_inconclusive(self):
        rig = Rig(self)
        rig.case27 = ({**HUNG, 'cases': [{'num': 27, 'status': 'FAIL', 'detail': 'timeout'}]}, '', '')
        rc, r = rig.run('--board', 'ok', '--delay', '0')
        self.assertEqual(r['drill'], 'inconclusive')
        self.assertEqual(r['cleanup'], 'drill reset rc 0')
        self.assertTrue(rig.parked())

    def test_an_unrecovered_wedge_fails_and_is_not_parked(self):
        rig = Rig(self)
        rig.case27 = ({**HUNG, 'wedged': True}, 'auto-recovering: resetting ok via openocd probe\nreset rc 0\n', '')
        rig.left_testusb = 777       # the D-state testusb the recovery did not free
        rc, r = rig.run('--board', 'ok', '--delay', '0')
        self.assertEqual((rc, r['drill']), (1, 'fail'))
        self.assertEqual(r['cleanup'], 'drill reset rc 0')
        self.assertFalse(rig.parked())
        self.assertIn('not parked', r['boardState'])
        self.assertTrue(rig.lock.closed)

    def test_a_failed_reset_is_not_a_recovery(self):
        rig = Rig(self)
        rig.case27 = (HUNG, RECOVERED.replace('reset rc 0', 'reset rc 1'), '')
        self.assertEqual(rig.run('--board', 'ok', '--delay', '0')[1]['drill'], 'fail')

    def test_the_same_devnum_is_not_a_re_enumeration(self):
        rig = Rig(self)
        rig.ids = [('/dev/bus/usb/001/005', 5)]
        rc, r = rig.run('--board', 'ok', '--delay', '0')
        self.assertEqual(r['drill'], 'fail')
        self.assertIn('fresh enumeration', r['reason'])

    def test_a_failed_smoke_case_fails(self):
        rig = Rig(self)
        rig.smoke = ({**SMOKE_PASS, 'cases': [{'num': 1, 'status': 'FAIL'}]}, '', '')
        rc, r = rig.run('--board', 'ok', '--delay', '0')
        self.assertEqual((r['drill'], r['smoke']), ('fail', 'FAIL'))
        self.assertEqual(r['cleanup'], 'drill reset rc 0')

    def test_sigterm_after_the_halt_still_cleans_up_and_releases(self):
        rig = Rig(self)
        rig.halt_raises = run_case.Terminated()
        rc, r = rig.run('--board', 'ok', '--delay', '0')
        self.assertEqual(r['drill'], 'inconclusive')
        self.assertEqual(r['cleanup'], 'drill reset rc 0')
        self.assertFalse(rig.parked())
        self.assertTrue(rig.lock.closed)
        self.assertEqual(len(rig.verdicts), 1)


class UnsafeStates(unittest.TestCase):
    """The board is parked only on positive evidence that it is usable."""

    def test_a_wedged_smoke_case_is_not_parked(self):
        rig = Rig(self)
        rig.smoke = ({**SMOKE_PASS, 'wedged': True,
                      'cases': [{'num': 1, 'status': 'HUNG'}]}, '', '')
        rig.left_testusb = 777
        rc, r = rig.run('--board', 'ok', '--delay', '0')
        self.assertEqual((rc, r['drill'], r['wedged']), (1, 'fail', True))
        self.assertFalse(rig.parked())
        self.assertIn('not parked', r['boardState'])

    def test_a_missing_verdict_is_not_parked_while_testusb_remains(self):
        rig = Rig(self)
        rig.case27 = (None, '', 'usbtest.py killed at its bound (rc 124)')
        rig.left_testusb = 777
        rc, r = rig.run('--board', 'ok', '--delay', '0')
        self.assertEqual((r['drill'], r['wedged']), ('fail', None))
        self.assertEqual(r['cleanup'], 'drill reset rc 0')
        self.assertFalse(rig.parked())

    def test_the_original_testusb_still_alive_on_an_old_node_is_not_parked(self):
        """The device re-enumerated on a new node; the drilled testusb still holds the old one."""
        rig = Rig(self)
        rig.case27 = (None, '', 'usbtest.py killed at its bound (rc 124)')
        rig.case_pid_alive_at_end = True
        rc, r = rig.run('--board', 'ok', '--delay', '0')
        self.assertEqual(r['cleanup'], 'drill reset rc 0')
        self.assertFalse(rig.parked())

    def test_a_wedged_smoke_case_is_not_parked_even_after_cleanup(self):
        """The cleanup reset moves the device to a new node; the smoke testusb holds the old one."""
        rig = Rig(self)
        rig.smoke = ({**SMOKE_PASS, 'wedged': True, 'cases': [{'num': 1, 'status': 'HUNG'}]}, '', '')
        rc, r = rig.run('--board', 'ok', '--delay', '0')
        self.assertEqual(r['cleanup'], 'drill reset rc 0')
        self.assertFalse(rig.parked())

    def test_an_exception_in_the_injection_leaves_the_state_unknown(self):
        rig = Rig(self)
        rig.halt_raises_after = OSError('pipe')
        rig.case27 = ({**HUNG, 'wedged': True}, '', '')
        rc, r = rig.run('--board', 'ok', '--delay', '0')
        self.assertIsNone(r['wedged'])
        self.assertEqual(r['cleanup'], 'drill reset rc 0')
        self.assertFalse(rig.parked())

    def test_an_exception_in_the_smoke_battery_leaves_the_state_unknown(self):
        rig = Rig(self)
        rig.smoke_raises = OSError('spawn')
        rc, r = rig.run('--board', 'ok', '--delay', '0')
        self.assertIsNone(r['wedged'])
        self.assertFalse(rig.parked())

    def test_a_case_that_ends_during_the_halt_is_inconclusive(self):
        rig = Rig(self)
        rig.running_after = False
        rig.case27 = ({**HUNG, 'cases': [{'num': 27, 'status': 'PASS'}]}, '', '')
        rc, r = rig.run('--board', 'ok', '--delay', '0')
        self.assertEqual(r['drill'], 'inconclusive')
        self.assertIn('ended while the halt ran', r['halt']['note'])
        self.assertEqual(r['cleanup'], 'drill reset rc 0')
        self.assertTrue(rig.parked())

    def test_a_late_process_leaves_no_room_to_halt(self):
        rig = Rig(self)
        with mock.patch.object(wedge_drill, 'age', lambda pid: 45):
            rc, r = rig.run('--board', 'ok', '--delay', '0', '--timeout', '60')
        self.assertEqual(r['drill'], 'inconclusive')
        self.assertNotIn('halt', [c[0] for c in rig.calls])

    def test_a_failed_park_is_not_a_clean_exit(self):
        rig = Rig(self)
        rig.flash_errors['/fw/device/board_test'] = 'probe gone'
        rc, r = rig.run('--board', 'ok', '--delay', '0')
        self.assertEqual((rc, r['drill']), (1, 'pass'))
        self.assertIn('park failed', r['error'])

    def test_keyboard_interrupt_after_the_halt_cleans_up_with_one_verdict(self):
        rig = Rig(self)
        rig.halt_raises = KeyboardInterrupt()
        rc, r = rig.run('--board', 'ok', '--delay', '0')
        self.assertEqual(r['cleanup'], 'drill reset rc 0')
        self.assertTrue(rig.lock.closed)
        self.assertEqual(len(rig.verdicts), 1)


class InterruptedCleanup(unittest.TestCase):
    def assert_unknown_with_one_verdict(self, rig, r, rc):
        self.assertEqual(rc, 1)
        self.assertIn('not parked', r['boardState'])
        self.assertTrue(r['error'])
        self.assertTrue(rig.lock.closed)
        self.assertEqual(len(rig.verdicts), 1)

    def test_a_signal_during_the_cleanup_reset(self):
        rig = Rig(self)
        rig.case27 = ({**HUNG, 'cases': [{'num': 27, 'status': 'FAIL'}]}, '', '')
        rig.reset_raises = run_case.Terminated()
        rc, r = rig.run('--board', 'ok', '--delay', '0')
        self.assert_unknown_with_one_verdict(rig, r, rc)

    def test_a_signal_during_parking(self):
        rig = Rig(self)
        rig.flash_raises['/fw/device/board_test'] = KeyboardInterrupt()
        rc, r = rig.run('--board', 'ok', '--delay', '0')
        self.assert_unknown_with_one_verdict(rig, r, rc)


class JoinThroughSignals(unittest.TestCase):
    def test_join_waits_for_the_battery_and_reraises_the_signal(self):
        class Worker:
            def __init__(self):
                self.joins = 0

            def is_alive(self):
                return self.joins < 2

            def join(self):
                self.joins += 1
                if self.joins == 1:
                    raise run_case.Terminated()

        w = Worker()
        with self.assertRaises(run_case.Terminated):
            wedge_drill.join(w)
        self.assertEqual(w.joins, 2)


class Peers(unittest.TestCase):
    def test_a_running_drill_is_a_peer(self):
        self.assertEqual(run_case.argv_peer(['python3', '/x/wedge_drill.py', '--board', 'b']),
                         'wedge_drill.py')

    def test_a_peer_after_the_lock_refuses_before_any_flash(self):
        rig = Rig(self)
        scans = [([], True), ([(99, 'testusb')], True)]
        with mock.patch.object(run_case, 'live_peers', lambda: scans.pop(0)):
            rc, r = rig.run('--board', 'ok')
        self.assertEqual(rc, 2)
        self.assertEqual(rig.calls, [])
        self.assertTrue(rig.lock.closed)


class Refusals(unittest.TestCase):
    def refused(self, *argv, says):
        rig = Rig(self)
        rc, r = rig.run(*argv)
        self.assertEqual(rc, 2)
        self.assertIn(says, r['error'])
        self.assertEqual(r['boardState'], 'untouched')
        self.assertEqual(rig.calls, [])

    def test_no_openocd_recovery_flasher(self):
        self.refused('--board', 'norec', says='halts through an openocd recovery flasher')
        self.refused('--board', 'esp', says='halts through an openocd recovery flasher')

    def test_no_room_for_the_halt(self):
        self.refused('--board', 'ok', '--delay', '50', '--timeout', '60', says='must end before')
        self.refused('--board', 'ok', '--delay', '-1', says='must end before')


if __name__ == '__main__':
    unittest.main()
