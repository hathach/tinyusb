"""run_case.py with every hardware call stubbed: roster resolution, each refusal before
hardware, the flash -> enumerate -> battery -> park order, and that the board lock is released
on every path. Plumbing only: the real chain is proved by a run on a rig board."""
import importlib.util
import io
import json
import os
import subprocess
import sys
import tempfile
import unittest
from contextlib import redirect_stderr, redirect_stdout
from pathlib import Path
from unittest import mock

REPO = Path(__file__).resolve().parents[2]
SCRIPT = REPO / '.claude' / 'skills' / 'usbtest' / 'scripts' / 'run_case.py'
spec = importlib.util.spec_from_file_location('run_case', SCRIPT)
run_case = importlib.util.module_from_spec(spec)
spec.loader.exec_module(run_case)

ROSTER = {
    'boards': [
        {'name': 'solo', 'uid': 'UID1', 'flasher': {'name': 'jlink', 'uid': 'P1', 'args': '-device X'},
         'tests': {'device': True, 'host': False}},
        {'name': 'duo', 'uid': 'UID2', 'flasher': {'name': 'openocd', 'uid': 'P2', 'args': ''},
         'variant': [{'name': 'duo-a', 'flags': ''}, {'name': 'duo-b', 'flags': ''}],
         'tests': {'only': ['device/board_test', 'device/usbtest']}},
        {'name': 'hostonly', 'uid': 'UID4', 'flasher': {'name': 'openocd'}, 'tests': {'device': False, 'host': True}},
        {'name': 'skips', 'uid': 'UID5', 'flasher': {'name': 'jlink'},
         'tests': {'device': True, 'skip': ['device/usbtest']}},
        {'name': 'notlisted', 'uid': 'UID6', 'flasher': {'name': 'jlink'}, 'tests': {'only': ['device/cdc_msc']}},
        {'name': 'untested', 'uid': 'UID7', 'flasher': {'name': 'jlink'}},
    ],
    'boards-skip': [{'name': 'parked', 'uid': 'UID3', 'flasher': {'name': 'jlink'}}],
}
PASS_JSON = {'passed': 1, 'failed': 0, 'notrun': 0, 'wedged': False,
             'cases': [{'num': 29, 'name': 'toggle clear', 'status': 'PASS'}]}


class Lock:
    def __init__(self):
        self.closed = False
        self.cleared = False

    def close(self):
        self.closed = True


class Rig:
    """Stubs for everything run_case.py touches, recording the order of hardware actions."""

    def __init__(self, test):
        self.test, self.calls = test, []
        self.firmware = {'device/usbtest': '/fw/usbtest.elf', 'device/board_test': '/fw/board_test.elf'}
        self.flash_rc = 0
        self.enumerates = True
        self.verdict = (PASS_JSON, '')
        self.lock = Lock()
        self.lock_error = None
        self.marker = None
        self.peers = ([], True)
        self.marks = []
        self.mark_ok = True
        self.flash_rcs = []          # per-flash return codes, then flash_rc
        tmp = tempfile.TemporaryDirectory()
        test.addCleanup(tmp.cleanup)
        self.config = Path(tmp.name) / 'rig.json'
        self.config.write_text(json.dumps(ROSTER))
        self.cwds = []

        def flash_fn(board, fw):
            self.calls.append(('flash', board['name'], fw))
            self.cwds.append(os.getcwd())
            rc = self.flash_rcs.pop(0) if self.flash_rcs else self.flash_rc
            return subprocess.CompletedProcess('flash', rc, 'probe says no', '')

        def acquire(name, reason):
            if self.lock_error:
                raise RuntimeError(self.lock_error)
            self.calls.append(('lock', name))
            return self.lock

        def clear(fh):
            fh.cleared = True

        def battery(board, fw, tests, timeout):
            self.calls.append(('battery', tuple(tests), fw))
            if isinstance(self.verdict, BaseException):
                raise self.verdict
            return self.verdict

        for obj, name, value in (
                (run_case.hil_flash, 'find_firmware',
                 lambda variant, example, flasher=None: self.firmware.get(example) and Path(self.firmware[example])),
                (run_case.hil_flash, 'flash_primitive', lambda name: flash_fn),
                (run_case.hil_lock, 'acquire_board_lock', acquire),
                (run_case.hil_lock, 'clear_record', clear),
                (run_case.hil_lock, 'read_wedged', lambda name: self.marker),
                (run_case.hil_lock, 'write_wedged',
                 lambda name, info, fh: self.marks.append((name, info, fh)) or self.mark_ok),
                (run_case, 'enumerated', lambda uid: self.calls.append(('enumerated', uid)) or self.enumerates),
                (run_case, 'battery', battery),
                (run_case, 'live_peers', lambda: self.peers)):
            patcher = mock.patch.object(obj, name, value)
            patcher.start()
            test.addCleanup(patcher.stop)

    def run(self, *argv):
        out, err = io.StringIO(), io.StringIO()
        with mock.patch.object(sys, 'argv', ['run_case.py', '--config', str(self.config), *argv]), \
                redirect_stdout(out), redirect_stderr(err):
            rc = run_case.main()
        return rc, json.loads(out.getvalue().splitlines()[-1]), err.getvalue()


class Refusals(unittest.TestCase):
    """Each exits 2 before any lock or hardware action."""

    def refused(self, rig, *argv, says):
        rc, report, err = rig.run(*argv)
        self.assertEqual(rc, 2, err)
        self.assertIn(says, report['error'])
        self.assertEqual([c for c in rig.calls if c[0] != 'lock'], [])
        self.assertEqual(report['boardState'], 'untouched')
        return rig

    def test_board_and_variant_resolution(self):
        for argv, says in (
                (['--board', 'nosuch'], 'nosuch is not a board'),
                (['--board', 'parked'], 'parked is in boards-skip'),
                (['--board', 'hostonly'], 'hostonly does not run device/usbtest'),
                (['--board', 'skips'], 'skips does not run device/usbtest'),
                (['--board', 'notlisted'], 'notlisted does not run device/usbtest'),
                (['--board', 'untested'], 'untested does not run device/usbtest'),
                (['--board', 'duo'], 'duo has variants duo-a, duo-b: pass --variant'),
                (['--board', 'duo', '--variant', 'duo-c'], 'duo has no variant duo-c'),
                (['--board', 'solo', '--variant', 'duo-a'], 'solo has no variant duo-a')):
            rig = Rig(self)
            self.refused(rig, *argv, '--tests', '29', '--after', 'leave', says=says)
            self.assertEqual(rig.calls, [])

    def test_bad_case_lists(self):
        for tests in ('29,29', '30', 'x', '29,'):
            self.refused(Rig(self), '--board', 'solo', '--tests', tests, '--after', 'leave',
                         says='--tests')

    def test_wedged_marker_is_read_under_the_lock(self):
        rig = Rig(self)
        rig.marker = {'reason': 'confirmed wedge'}
        self.refused(rig, '--board', 'solo', '--tests', '29', '--after', 'leave', says='marked wedged')
        self.assertEqual(rig.calls, [('lock', 'solo')])
        self.assertTrue(rig.lock.cleared and rig.lock.closed)

    def test_duplicate_roster_names(self):
        roster = json.loads(json.dumps(ROSTER))
        roster['boards'].append(dict(roster['boards'][0]))
        roster['boards'][1]['variant'].append({'name': 'duo-a'})
        for board, says in (('solo', 'solo appears 2 times'), ('duo', 'lists a variant name twice')):
            rig = Rig(self)
            rig.config.write_text(json.dumps(roster))
            self.refused(rig, '--board', board, '--variant', 'duo-a', '--tests', '29', '--after', 'leave', says=says)

    def test_missing_firmware(self):
        rig = Rig(self)
        del rig.firmware['device/usbtest']
        self.refused(rig, '--board', 'solo', '--tests', '29', '--after', 'leave', says='no device/usbtest build for solo')
        rig = Rig(self)
        del rig.firmware['device/board_test']
        self.refused(rig, '--board', 'solo', '--tests', '29', '--after', 'park', says='no device/board_test build')

    def test_park_firmware_is_not_needed_to_leave(self):
        rig = Rig(self)
        del rig.firmware['device/board_test']
        self.assertEqual(rig.run('--board', 'solo', '--tests', '29', '--after', 'leave')[0], 0)

    def test_a_live_battery_or_an_unreadable_proc(self):
        rig = Rig(self)
        rig.peers = ([(4242, 'hil_test.py')], True)
        self.refused(rig, '--board', 'solo', '--tests', '29', '--after', 'leave', says='hil_test.py (pid 4242)')
        rig = Rig(self)
        rig.peers = ([], False)
        self.refused(rig, '--board', 'solo', '--tests', '29', '--after', 'leave', says='cannot read every process')

    def test_allow_concurrent_skips_only_the_peer_check(self):
        rig = Rig(self)
        rig.peers = ([(4242, 'testusb')], False)
        rc, _, _ = rig.run('--board', 'solo', '--tests', '29', '--after', 'leave', '--allow-concurrent')
        self.assertEqual(rc, 0)

    def test_lock_held_or_unavailable(self):
        rig = Rig(self)
        rig.lock_error = 'board locked: {"pid": 1, "reason": "hil_test.py"}'
        self.refused(rig, '--board', 'solo', '--tests', '29', '--after', 'leave', says='board locked')
        rig = Rig(self)
        rig.lock = None
        self.refused(rig, '--board', 'solo', '--tests', '29', '--after', 'leave', says='lock unavailable')


class Chain(unittest.TestCase):
    def released(self, rig):
        self.assertTrue(rig.lock.cleared and rig.lock.closed)

    def test_pass_then_park(self):
        rig = Rig(self)
        cwd = os.getcwd()
        rc, report, _ = rig.run('--board', 'solo', '--tests', '29', '--after', 'park')
        self.assertEqual(rc, 0)
        self.assertEqual(rig.calls, [('lock', 'solo'), ('flash', 'solo', '/fw/usbtest.elf'),
                                     ('enumerated', 'UID1'), ('battery', (29,), '/fw/usbtest.elf'),
                                     ('flash', 'solo', '/fw/board_test.elf')])
        self.assertEqual((report['pass'], report['variant'], report['boardState']),
                         (True, 'solo', 'parked on board_test'))
        self.released(rig)
        self.assertEqual(os.getcwd(), cwd)
        self.assertTrue(all(c != cwd and not os.path.exists(c) for c in rig.cwds))   # temp cwd, removed

    def test_leave_keeps_usbtest_running(self):
        rig = Rig(self)
        rc, report, _ = rig.run('--board', 'duo', '--variant', 'duo-b', '--tests', '13,29', '--after', 'leave')
        self.assertEqual(rig.calls[-1], ('battery', (13, 29), '/fw/usbtest.elf'))
        self.assertEqual(report['boardState'], 'usbtest firmware')
        self.assertEqual(rc, 1)     # the stub verdict holds one case for two requested

    def test_a_failed_case_still_parks(self):
        rig = Rig(self)
        rig.verdict = ({**PASS_JSON, 'failed': 1, 'cases': [{'num': 29, 'status': 'FAIL', 'detail': 'errno 110'}]}, '')
        rc, report, _ = rig.run('--board', 'solo', '--tests', '29', '--after', 'park')
        self.assertEqual((rc, report['pass'], report['boardState']), (1, False, 'parked on board_test'))
        self.released(rig)

    def test_wedged_is_never_parked(self):
        rig = Rig(self)
        rig.verdict = ({**PASS_JSON, 'wedged': True}, 'aborting battery')
        rc, report, _ = rig.run('--board', 'solo', '--tests', '29', '--after', 'park')
        self.assertEqual((rc, report['wedged']), (1, True))
        self.assertTrue(report['boardState'].startswith('wedged'))
        self.assertEqual(sum(c[0] == 'flash' for c in rig.calls), 1)
        self.released(rig)

    def test_no_verdict_is_never_parked(self):
        rig = Rig(self)
        rig.verdict = (None, 'killed')
        rc, report, _ = rig.run('--board', 'solo', '--tests', '29', '--after', 'park')
        self.assertEqual((rc, report['boardState']), (1, 'usbtest firmware, verdict incomplete'))
        self.assertEqual(sum(c[0] == 'flash' for c in rig.calls), 1)
        self.released(rig)

    def test_flash_failure_or_no_device_stops_there(self):
        rig = Rig(self)
        rig.flash_rc = 1
        rc, report, _ = rig.run('--board', 'solo', '--tests', '29', '--after', 'park')
        self.assertEqual((rc, report['boardState']), (1, 'flash failed'))
        self.assertIn('probe says no', report['error'])
        self.assertNotIn('enumerated', [c[0] for c in rig.calls])
        self.released(rig)
        rig = Rig(self)
        rig.enumerates = False
        rc, report, _ = rig.run('--board', 'solo', '--tests', '29', '--after', 'park')
        self.assertEqual(rc, 1)
        self.assertIn('no cafe:4010 device with serial UID1', report['error'])
        self.assertNotIn('battery', [c[0] for c in rig.calls])
        self.released(rig)

    def test_case_0_keeps_its_number(self):
        rig = Rig(self)
        rig.verdict = ({**PASS_JSON, 'cases': [{'num': 0, 'name': 'NOP', 'status': 'PASS'}]}, '')
        rc, report, _ = rig.run('--board', 'solo', '--tests', '0', '--after', 'leave')
        self.assertEqual((rc, report['cases']), (0, [{'num': 0, 'name': 'NOP', 'status': 'PASS'}]))

    def test_a_failed_park_fails_the_run(self):
        rig = Rig(self)
        rig.flash_rcs = [0, 1]
        rc, report, _ = rig.run('--board', 'solo', '--tests', '29', '--after', 'park')
        self.assertEqual((rc, report['pass']), (1, False))
        self.assertTrue(report['boardState'].startswith('park failed'))
        self.assertIn('probe says no', report['error'])
        self.released(rig)

    def test_a_confirmed_wedge_marks_the_board_under_the_lock(self):
        rig = Rig(self)
        rig.verdict = ({**PASS_JSON, 'wedged': True, 'wedge_confirmation': 'confirmed', 'serial': 'UID1',
                        'wedge_evidence': {'node': '/dev/bus/usb/001/005', 'holders': [7], 'complete': True}}, '')
        rc, report, _ = rig.run('--board', 'solo', '--tests', '29', '--after', 'park')
        self.assertEqual(rc, 1)
        [(name, info, fh)] = rig.marks
        self.assertEqual((name, fh, info['confirmation'], info['fw']), ('solo', rig.lock, 'confirmed', '/fw/usbtest.elf'))
        self.assertEqual(info['evidence'], {'node': '/dev/bus/usb/001/005', 'holders': [7], 'complete': True,
                                            'serial': 'UID1'})
        self.released(rig)
        rig = Rig(self)
        rig.mark_ok = False
        rig.verdict = ({**PASS_JSON, 'wedged': True, 'wedge_confirmation': 'confirmed'}, '')
        rc, report, _ = rig.run('--board', 'solo', '--tests', '29', '--after', 'park')
        self.assertIn('marker could not be written', report['error'])

    def test_an_unverified_wedge_is_not_marked(self):
        rig = Rig(self)
        rig.verdict = ({**PASS_JSON, 'wedged': True, 'wedge_confirmation': 'unverified'}, '')
        self.assertEqual(rig.run('--board', 'solo', '--tests', '29', '--after', 'park')[0], 1)
        self.assertEqual(rig.marks, [])

    def test_an_error_after_the_lock_still_reports(self):
        rig = Rig(self)
        rig.verdict = RuntimeError('usb_scan blew up')
        rc, report, _ = rig.run('--board', 'solo', '--tests', '29', '--after', 'park')
        self.assertEqual((rc, report['error']), (1, 'RuntimeError: usb_scan blew up'))
        self.released(rig)

    def test_a_setup_error_after_the_lock_still_reports(self):
        rig = Rig(self)
        with mock.patch.object(run_case.tempfile, 'mkdtemp', side_effect=OSError('disk full')):
            rc, report, _ = rig.run('--board', 'solo', '--tests', '29', '--after', 'park')
        self.assertEqual((rc, report['error'], report['boardState']), (1, 'OSError: disk full', 'untouched'))
        self.assertEqual(rig.calls, [('lock', 'solo')])
        self.released(rig)

    def test_a_failed_cwd_restore_still_releases_the_lock(self):
        rig = Rig(self)
        real = os.chdir
        with mock.patch.object(run_case.os, 'chdir', side_effect=lambda d: (_ for _ in ()).throw(OSError('gone'))
                               if not d.startswith(tempfile.gettempdir()) else real(d)):
            with self.assertRaises(OSError):
                rig.run('--board', 'solo', '--tests', '29', '--after', 'park')
        real(REPO)
        self.released(rig)

    def test_an_exception_still_releases_the_lock(self):
        rig = Rig(self)
        rig.verdict = KeyboardInterrupt()
        with self.assertRaises(KeyboardInterrupt):
            rig.run('--board', 'solo', '--tests', '29', '--after', 'park')
        self.released(rig)


class Battery(unittest.TestCase):
    """The usbtest.py command and its bound."""

    def command(self, flasher):
        seen = {}

        def run_cmd(cmd, timeout, split_stderr):
            seen.update(cmd=cmd, timeout=timeout)
            return subprocess.CompletedProcess(cmd, 0, 'banner\n' + json.dumps(PASS_JSON), 'note')

        board = {'name': 'b', 'uid': 'UID', 'flasher': flasher}
        with mock.patch.object(run_case.hil_util, 'run_cmd', run_cmd):
            data, err = run_case.battery(board, '/fw/u.elf', [13, 29], 60)
        self.assertEqual((data, err), (PASS_JSON, 'note'))
        return seen

    def test_recovery_only_when_the_flasher_can_deliver_it(self):
        with mock.patch.object(run_case.hil_flash, 'convoy_safe', lambda f: True):
            seen = self.command({'name': 'openocd', 'args': '', 'uid': 'P'})
        self.assertIn('--recover-fw', seen['cmd'])
        self.assertEqual(seen['cmd'][seen['cmd'].index('--tests') + 1], '13,29')
        self.assertEqual(seen['timeout'], 2 * 100 + run_case.usbtest.recovery_reserve({'name': 'openocd', 'args': ''}))
        with mock.patch.object(run_case.hil_flash, 'convoy_safe', lambda f: False):
            seen = self.command({'name': 'jlink', 'args': '', 'uid': 'P'})
        self.assertNotIn('--recover-board', seen['cmd'])
        self.assertEqual(seen['timeout'], 2 * 100 + run_case.usbtest.WEDGE_CONFIRM_S)


class LivePeers(unittest.TestCase):
    def test_matches_battery_processes_by_argv(self):
        with tempfile.TemporaryDirectory() as d:
            for pid, argv in ((11, ['sudo', '-n', '/home/u/testusb', '-D', 'x']),
                              (12, ['python3', 'test/hil/usbtest.py', '--json']),
                              (13, ['/usr/bin/python3', '/r/test/hil/hil_test.py', 'c.json']),
                              (14, ['python3', 'run_case.py']), (15, ['bash'])):
                (Path(d) / str(pid)).mkdir()
                (Path(d) / str(pid) / 'cmdline').write_bytes(b'\0'.join(a.encode() for a in argv) + b'\0')
            with mock.patch.object(run_case, 'PROC', Path(d)):
                peers, complete = run_case.live_peers()
                self.assertEqual((sorted(peers), complete),
                                 ([(11, 'testusb'), (12, 'usbtest.py'), (13, 'hil_test.py')], True))
                (Path(d) / '16').mkdir()                          # exited: no cmdline left
                self.assertTrue(run_case.live_peers()[1])
                (Path(d) / '17' / 'cmdline').mkdir(parents=True)  # unreadable for another reason
                self.assertFalse(run_case.live_peers()[1])


if __name__ == '__main__':
    unittest.main()
