#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# The hil_lock adapter: callers get agentrc's board-lock primitive from the agentrc helper/agentrc.py
# selects (agentrc's tests/test_hil_lock.py covers the primitive itself), hil_test refuses a board
# whose lock is unavailable before flashing, a recovery blocker refuses the board as a blocker
# rather than contention (RecoveryBlockerIsNotContention), an openocd_esp session left running passes the
# board's lock to a cleanup holder (RetainedLock), `hold --all` takes this host's roster, and an
# incomplete agentrc refuses at import (test_hil_agentrc.py covers the loader). Every lock lives in a
# temp dir under a name no rig uses. Run directly:
#   python3 test/hil/test/test_hil_lock.py
import io
import json
import os
import shutil
import subprocess
import sys
import tempfile
import time
import types
import unittest
from contextlib import redirect_stdout
from unittest import mock

HIL_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
REPO = os.path.dirname(os.path.dirname(HIL_DIR))
sys.path.insert(0, HIL_DIR)

from usbtest_harness import patch   # noqa: E402 - stubs pyserial before hil_test imports it
import hil_flash         # noqa: E402
import hil_test          # noqa: E402
from helper import agentrc, hil_lock, hil_util   # noqa: E402
from test_hil_agentrc import scratch_agentrc, scratch_env   # noqa: E402

BOARD = 'hil-lock-selftest-board'
LOCK = 'hil-lock-selftest-lock'     # a lock named apart from its roster board
PROVIDER = 'skills/hil/scripts/hil_lock.py'
PROCESS = 'skills/hil/scripts/hil_process.py'


class LockDir(unittest.TestCase):
    def setUp(self):
        td = tempfile.TemporaryDirectory()
        self.addCleanup(td.cleanup)
        self.dir = td.name
        patch(self, hil_lock, 'BOARD_LOCK_DIR', self.dir)
        env = mock.patch.dict(os.environ)
        env.start()
        self.addCleanup(env.stop)
        os.environ.pop('HIL_NO_BOARD_LOCK', None)

    def lock_dir_is_a_file(self):
        path = os.path.join(self.dir, 'not-a-dir')
        open(path, 'w').close()
        patch(self, hil_lock, 'BOARD_LOCK_DIR', path)


class RunnerRefusesBeforeFlash(LockDir):
    """hil_test.test_board on an unusable lock dir: the board fails as lock-unavailable and
    returns before anything flashes."""

    def setUp(self):
        super().setUp()
        self.lock_dir_is_a_file()
        self.addCleanup(setattr, hil_test, 'board_wedged', hil_test.board_wedged)
        self.log = []
        patch(self, hil_test, 'log_line', self.log.append)
        patch(self, hil_flash, 'flash_primitive', lambda *a: self.fail('flashed'))
        self.board = {'name': BOARD, 'uid': 'ABC123', 'flasher': {'name': 'jlink', 'uid': 'P'},
                      'tests': {'only': ['device/cdc_msc']}}

    def test_lock_unavailable_fails_the_board_unflashed(self):
        name, err, failed, rows, _ = hil_test.test_board(self.board)
        self.assertEqual((name, err, failed), (BOARD, 1, []))
        cells = rows[0][1]
        self.assertEqual(list(cells), [hil_test.hil_report.LOCK_UNAVAILABLE_CELL])
        cell = cells[hil_test.hil_report.LOCK_UNAVAILABLE_CELL]
        self.assertTrue(cell.startswith(f'{hil_test.hil_report.REPORT_CELL["fail"]} board lock unavailable: '),
                        cell)
        self.assertEqual(hil_test.hil_report.cell_state(cell), 'fail')
        self.assertIn(f'{hil_test.STATUS_FAILED}: board lock unavailable: ', self.log[0])

    def test_contention_keeps_its_own_cell(self):
        def held(name, **kw):
            raise RuntimeError('board locked: {"pid": 1}')
        patch(self, hil_lock, 'acquire_board_lock', held)
        _, err, _, rows, _ = hil_test.test_board(self.board)
        self.assertEqual(err, 1)
        self.assertEqual(rows[0][1], {hil_test.hil_report.LOCKED_CELL: 'fail'})

    def test_a_registered_board_is_locked_by_its_registry_name(self):
        probe, reg = next(iter(hil_util.hil_boards().registry().items()))
        board = hil_util.hil_boards().resolve({'name': BOARD, 'uid': 'ABC123', 'probe': probe,
                                               'tests': {'only': ['device/cdc_msc']}})
        locked = []

        def held(name, **kw):
            locked.append(name)
            raise RuntimeError('board locked: {"pid": 1}')
        patch(self, hil_lock, 'acquire_board_lock', held)
        self.assertEqual(hil_test.test_board(board)[0], BOARD)
        self.assertEqual(locked, [reg['name']])


class RecoveryBlockerIsNotContention(LockDir):
    """A board refused by a recovery blocker (`hil` Board locks) gets a recovery-blocker cell naming
    its holder, never board-locked, and nothing flashes; another session's hold stays board-locked."""

    def setUp(self):
        super().setUp()
        patch(self, hil_test, 'log_line', lambda line: None)
        patch(self, hil_flash, 'flash_primitive', lambda *a: self.fail('flashed'))
        self.board = {'name': BOARD, 'uid': 'ABC123', 'flasher': {'name': 'jlink', 'uid': 'P'},
                      'tests': {'only': ['device/cdc_msc']}}

    def hold(self, reason, **extra):
        """Another session's hold: its flock and record, until the returned handle closes."""
        fh = hil_lock.flock_nb(BOARD)
        self.addCleanup(fh.close)
        hil_lock.write_record(fh, reason, **extra)
        return fh

    def refusal(self):
        name, err, failed, rows, _ = hil_test.test_board(self.board)
        self.assertEqual((name, err, failed, len(rows)), (BOARD, 1, [], 1))
        return rows[0][1]

    def assert_blocker(self, *named):
        cells = self.refusal()
        self.assertEqual(list(cells), [hil_test.hil_report.RECOVERY_BLOCKER_CELL])
        cell = cells[hil_test.hil_report.RECOVERY_BLOCKER_CELL]
        self.assertEqual(hil_test.hil_report.cell_state(cell), 'fail')
        for text in named:
            self.assertIn(text, cell)

    def test_a_live_cleanup_holder(self):
        self.hold('hil_test.py: openocd_esp cleanup pending', role=hil_lock.CLEANUP_ROLE, token='t0k')
        self.assert_blocker('board locked: ', '"role": "cleanup-holder"', '"token": "t0k"', hil_lock.RECOVERY_BLOCKER)

    def test_a_dead_cleanup_holder(self):
        self.hold('hil_test.py: openocd_esp cleanup pending', role=hil_lock.CLEANUP_ROLE, token='t0k').close()
        self.assert_blocker('stale record', '"role": "cleanup-holder"', '"token": "t0k"')

    def test_a_protected_record_left_on_a_free_flock(self):
        self.hold(hil_lock.CI_REASON, protected=True).close()
        self.assert_blocker('stale record', '"protected": true', f'"reason": "{hil_lock.CI_REASON}"')

    def test_a_takeover_journal(self):
        with open(os.path.join(self.dir, BOARD + hil_lock.JOURNAL), 'w') as f:
            json.dump({'pid': 1, 'predecessor': '{}'}, f)
        self.assert_blocker('takeover journal', '"pid": 1')

    def test_a_takeover_journal_on_a_held_flock(self):
        # take_over holds the flock and journals before it rewrites the protected predecessor record
        self.hold(hil_lock.CI_REASON, protected=True)
        with open(os.path.join(self.dir, BOARD + hil_lock.JOURNAL), 'w') as f:
            json.dump({'token': 't0k', 'pid': 1, 'locks': [BOARD], 'predecessor': '{}'}, f)
        self.assert_blocker('board locked: ', '"protected": true', 'takeover journal', '"token": "t0k"',
                            hil_lock.RECOVERY_BLOCKER)

    def test_another_sessions_hold_stays_contention(self):
        for extra in ({}, {'protected': True}):   # a live protected record is a run, not a blocker
            with self.subTest(**extra):
                fh = self.hold(hil_lock.CI_REASON, **extra)
                self.assertEqual(self.refusal(), {hil_test.hil_report.LOCKED_CELL: 'fail'})
                hil_lock.release_board_lock(fh)


class Adapter(LockDir):
    def test_callers_get_the_loaded_agentrcs_provider(self):
        self.assertEqual(os.path.realpath(hil_lock.__file__),
                         os.path.realpath(os.path.join(agentrc.identity()['root'], PROVIDER)))
        self.assertIs(hil_test.hil_lock, hil_lock)

    def test_a_patched_lock_dir_reaches_the_provider(self):
        self.assertEqual(hil_lock.lock_path(BOARD), os.path.join(self.dir, f'{BOARD}.lock'))


class InstalledDependency(unittest.TestCase):
    """The adapter's CLI in a scratch tree whose installed agentrc (AGENTRC_DIR) is a checkout of
    the closure this process loaded, against a lock dir of its own."""

    def setUp(self):
        td = tempfile.TemporaryDirectory()
        self.addCleanup(td.cleanup)
        self.root = td.name
        for rel in ('test/hil/helper/__init__.py', 'test/hil/helper/agentrc.py', 'test/hil/helper/hil_lock.py',
                    'test/hil/helper/hil_util.py', 'test/hil/agentrc.json'):
            os.makedirs(os.path.join(self.root, os.path.dirname(rel)), exist_ok=True)
            shutil.copy(os.path.join(REPO, rel), os.path.join(self.root, rel))
        self.install = os.path.join(self.root, 'agentrc')

    def provide(self, lock_dir='/tmp/tinyusb-hil-locks', process=True):
        """The install, its hil_lock's BOARD_LOCK_DIR at lock_dir; without hil_process unless `process` (the
        release predicate and the cleanup hand-over load it beside hil_lock)."""
        with open(os.path.join(agentrc.identity()['root'], PROVIDER)) as f:
            text = f.read()
        moved = text.replace("BOARD_LOCK_DIR = '/tmp/tinyusb-hil-locks'", f'BOARD_LOCK_DIR = {lock_dir!r}')
        self.assertTrue(lock_dir == '/tmp/tinyusb-hil-locks' or moved != text, 'BOARD_LOCK_DIR line moved')
        scratch_agentrc(self.install, {PROVIDER: moved}, () if process else (PROCESS,))

    def cli(self, *argv, host='ci'):
        # -P: the script must find helper/agentrc.py without the script-dir sys.path entry; the
        # script runs as __main__ on a machine whose hostname is `host`
        run = ('import runpy, socket, sys; host = sys.argv.pop(2); socket.gethostname = lambda: host; '
               "sys.argv.pop(0); runpy.run_path(sys.argv[0], run_name='__main__')")
        r = subprocess.run([sys.executable, '-P', '-c', run, 'test/hil/helper/hil_lock.py', host, *argv],
                           cwd=self.root, env=scratch_env(self.install), capture_output=True, text=True, timeout=30)
        return r.returncode, r.stdout + r.stderr

    def roster(self, rel, *names):
        with open(os.path.join(self.root, rel), 'w') as f:
            json.dump({'boards': [{'name': n} for n in names]}, f)

    def test_hold_all_takes_the_roster_with_boards_skip(self):
        lock_dir = os.path.join(self.root, 'locks')
        self.provide(lock_dir)
        names = [f'hil-lock-selftest-{n}' for n in ('a', 'b', 'parked', 'other')]
        with open(os.path.join(self.root, 'test/hil/tinyusb.json'), 'w') as f:
            json.dump({'boards': [{'name': names[0]}, {'name': names[1]}], 'boards-skip': [{'name': names[2]}]}, f)
        other = os.path.join(self.root, 'other.json')
        with open(other, 'w') as f:
            json.dump({'boards': [{'name': names[3]}]}, f)
        self.addCleanup(self.cli, 'release', '--all')
        self.assertEqual(self.cli('hold', '--all', '--reason', 'selftest'), (0, f'held: {", ".join(names[:3])}\n'))
        self.assertEqual(self.cli('hold', '--all', '--config', other, '--reason', 'selftest'),
                         (0, f'held: {names[3]}\n'))
        rc, out = self.cli('status')
        self.assertEqual(sorted(line.split(':')[0] for line in out.splitlines()), sorted(names))
        rc, _ = self.cli('release', '--all')
        self.assertEqual(rc, 0)
        self.assertEqual(self.cli('status'), (0, 'no locks\n'))

    def test_hold_all_takes_this_hosts_roster(self):
        self.provide(os.path.join(self.root, 'locks'))
        self.roster('test/hil/tinyusb.json', 'hil-lock-selftest-ci')
        self.roster('test/hil/hfp.json', 'hil-lock-selftest-tusb')
        self.addCleanup(self.cli, 'release', '--all')
        self.assertEqual(self.cli('hold', '--all', '--reason', 'selftest', host='tusb'),
                         (0, 'held: hil-lock-selftest-tusb\n'))

    def test_hold_all_on_an_unknown_host_takes_local_json(self):
        self.provide(os.path.join(self.root, 'locks'))
        self.roster('test/hil/tinyusb.json', 'hil-lock-selftest-ci')
        self.roster('test/hil/local.json', 'hil-lock-selftest-local')
        self.addCleanup(self.cli, 'release', '--all')
        self.assertEqual(self.cli('hold', '--all', '--reason', 'selftest', host='htpc'),
                         (0, 'held: hil-lock-selftest-local\n'))

    def test_hold_all_on_an_unknown_host_without_local_json_needs_a_named_roster(self):
        self.provide(os.path.join(self.root, 'locks'))
        self.roster('test/hil/tinyusb.json', 'hil-lock-selftest-ci')
        self.roster('local.json', 'hil-lock-selftest-local')   # in the cwd, not test/hil: never the default
        self.addCleanup(self.cli, 'release', '--all')
        rc, out = self.cli('hold', '--all', '--reason', 'selftest', host='htpc')
        self.assertEqual(rc, 2)
        self.assertIn('host htpc has no roster here', out)
        self.assertEqual(self.cli('status'), (0, 'no locks\n'))
        self.assertEqual(self.cli('hold', '--all', '--config', 'local.json', '--reason', 'selftest', host='htpc'),
                         (0, 'held: hil-lock-selftest-local\n'))

    def test_hold_all_refuses_a_missing_roster(self):
        self.provide(os.path.join(self.root, 'locks'))
        for host, argv in (('tusb', ()), ('ci', ('--config', 'missing.json'))):   # no hfp.json in this tree
            with self.subTest(host=host):
                rc, out = self.cli('hold', '--all', *argv, '--reason', 'selftest', host=host)
                self.assertEqual(rc, 1)
                self.assertIn('ERROR: cannot read board roster', out)
        self.assertEqual(self.cli('status'), (0, 'no locks\n'))

    def test_hold_all_refuses_a_malformed_roster(self):
        self.provide(os.path.join(self.root, 'locks'))
        for cfg in ({'boards': None}, ['hil-lock-selftest'], {'boards': ['hil-lock-selftest']}):
            with self.subTest(cfg=cfg):
                with open(os.path.join(self.root, 'bad.json'), 'w') as f:
                    json.dump(cfg, f)
                rc, out = self.cli('hold', '--all', '--config', 'bad.json', '--reason', 'selftest')
                self.assertEqual(rc, 1)
                self.assertRegex(out, r'ERROR: cannot (read board roster|name the board locks of) bad\.json')
                self.assertNotIn('Traceback', out)
        self.assertEqual(self.cli('status'), (0, 'no locks\n'))

    def test_a_provider_without_hil_process_beside_it_is_refused(self):
        self.provide(os.path.join(self.root, 'locks'), process=False)
        rc, out = self.cli('status')
        self.assertNotEqual(rc, 0)
        self.assertIn('lacks skills/hil/scripts/hil_process.py', out)


def _board_in_worker(board):
    return hil_test.test_board(board)[1]


def _next_task(x):
    return x


class RetainedLock(LockDir):
    """An openocd_esp session the provider left running (cleanup 'pending', on the returned
    operation or an exception's `.operation`) stops the board: no retry, no further test, no
    park, and its lock passes to an acknowledged cleanup holder instead of being released."""

    def setUp(self):
        super().setUp()
        self.RECEIPT = os.path.join(self.dir, 'receipt.json')
        from types import SimpleNamespace
        self.ns = SimpleNamespace
        self.log, self.flashes = [], []
        patch(self, hil_test, 'log_line', self.log.append)
        for var in hil_util.openocd_esp().CI_JOB_ENV:
            os.environ.pop(var, None)
        patch(self, hil_test, 'max_retry', 2)
        patch(self, hil_test, 'skip_flash', False)
        patch(self, hil_test, 'reset_dut_tt', lambda board: True)
        patch(self, hil_test.usbtest, 'strays_on', lambda uid: [])
        patch(self, hil_flash, 'find_firmware', lambda *a, **kw: hil_test.Path('/fw/cdc_msc.bin'))
        patch(self, hil_flash, 'cleanup_pending', {})
        patch(self, hil_flash, 'OPENOCD_ESP_DIR', hil_test.Path(self.dir) / 'openocd_esp')
        patch(self, hil_test, 'test_device_cdc_msc', lambda board: None)
        self.board = {'name': BOARD, 'uid': 'ABC123', 'tests': {'only': ['device/cdc_msc']},
                      'flasher': {'name': 'openocd_esp', 'uid': '30:ED:A0:E0:C9:02',
                                  'args': '-f board/esp32p4-builtin.cfg'}}
        self.lock = BOARD
        self.addCleanup(self.end_holder)

    def provider(self, outcome, security_cleanup='done', security_receipts=()):
        """hil_util.openocd_esp() whose flash records the lock record it ran under, then
        returns or raises `outcome`; its security read ends with `security_cleanup`."""
        def flash(flasher, segments, **kw):
            self.flashes.append(hil_lock.read_record(self.lock))
            if isinstance(outcome, BaseException):
                raise outcome
            return outcome
        security = self.ns(security={'flashEncryption': 'off', 'secureBoot': 'off', 'source': 'test'},
                           returncode=0, cleanup=security_cleanup, receipts=security_receipts)
        self.reads = []
        fake = self.ns(admit=lambda f: ('esp32p4', ['board/esp32p4-builtin.cfg']), flash_openocd_esp=flash,
                       segments_from_flash_args=lambda *a, **kw: [{'offset': 0, 'file': '/fw/cdc_msc.bin', 'size': 4}],
                       read_security=lambda *a, **kw: self.reads.append(a) or security,
                       refusal=hil_util.openocd_esp().refusal)
        patch(self, hil_util, 'openocd_esp', lambda: fake)

    def op(self, cleanup, rc=1):
        return self.ns(returncode=rc, stdout='', cleanup=cleanup, receipts=(self.RECEIPT,) if cleanup == 'pending' else (),
                       session={'failure': 'init failed', 'log': '/x/openocd.log', 'dir': '/nonexistent'})

    def end_session(self):
        """The receipt as `hil_openocd_esp.py end` leaves it on exit 0, which the holder's release waits for."""
        with open(self.RECEIPT, 'w') as f:
            json.dump({'tool': 'hil_openocd_esp', 'pid': 4242, 'startTime': '99', 'closed': True, 'cleanup': 'done'}, f)

    def end_holder(self):
        rec = hil_lock.read_record(self.lock)
        if hil_lock.is_cleanup_holder(rec):
            self.end_session()
            hil_lock.release_token(rec['token'])

    def assert_retained(self):
        rec = hil_lock.read_record(self.lock)
        self.assertTrue(hil_lock.is_cleanup_holder(rec), rec)
        self.assertIn(self.RECEIPT, rec['detail'])
        self.assertEqual(rec['retained'], [self.RECEIPT])
        self.assertEqual((rec['protected'], rec['locks']), (True, [self.lock]))
        with self.assertRaises(hil_lock.BoardLocked):         # the holder keeps the flock itself
            hil_lock.take_board_lock(self.lock, 'peer')
        self.assertIsNone(hil_lock.journal_state(self.lock))  # handed over, not taken over
        return rec

    def run_board(self):
        # only hil_test's own sleeps, not the provider's in the lock hand-over
        sleep = mock.Mock()
        fake_time = types.ModuleType('time')
        fake_time.__dict__.update(vars(time), sleep=sleep)
        with mock.patch.object(hil_test, 'time', fake_time):
            name, err, failed, rows, _ = hil_test.test_board(self.board)
        sleep.assert_not_called()   # never the 0.5 s retry wait
        self.assertEqual((name, err, failed), (BOARD, 2, []))   # the test's flash and the blocker
        self.assertEqual(len(self.flashes), 1, 'retried or parked beside an outstanding session')
        self.assertEqual((self.flashes[0]['protected'], self.flashes[0]['locks']), (True, [self.lock]),
                         self.flashes[0])
        rec = self.assert_retained()
        cell = rows[-1][1][hil_test.hil_report.CLEANUP_PENDING_CELL]
        self.assertEqual(hil_test.hil_report.cell_state(cell), 'fail')
        self.assertIn(self.RECEIPT, cell)
        self.assertIn(f'release --token {rec["token"]}', cell)
        return rows

    def test_pending_on_return_hands_the_lock_to_a_cleanup_holder(self):
        self.provider(self.op('pending', rc=124))
        self.run_board()
        self.assertEqual(hil_flash.cleanup_pending, {BOARD: (self.RECEIPT,)})
        # the token ends the holder only once recovery ended the session, and only then is the board free again
        token = hil_lock.read_record(BOARD)['token']
        with self.assertRaisesRegex(hil_lock.ReleaseRefused, 'retained sessions are not proven ended'):
            hil_lock.release_token(token)
        self.end_session()
        hil_lock.release_token(token)
        hil_lock.release_board_lock(hil_lock.take_board_lock(BOARD, 'peer'))

    def test_a_registered_boards_pending_hands_over_its_registry_lock(self):
        probe, reg = next(iter(hil_util.hil_boards().registry().items()))
        board = hil_util.hil_boards().resolve({'name': BOARD, 'uid': 'ABC123', 'probe': probe,
                                               'tests': {'only': ['device/cdc_msc']}})
        self.board = {**board, 'flasher': self.board['flasher']}
        self.lock = reg['name']
        self.provider(self.op('pending', rc=124))
        self.run_board()
        self.assertEqual(hil_flash.cleanup_pending, {BOARD: (self.RECEIPT,)})
        self.assertEqual(hil_lock.record_state(BOARD)[0], 'absent')  # never the roster name's lock

    def test_pending_on_an_exception_hands_it_over_too(self):
        err = RuntimeError('Tcl connection lost')
        err.operation = self.op('pending', rc=None)
        self.provider(err)
        self.run_board()

    def test_pending_on_a_final_flash_attempt_stops_the_same_way(self):
        patch(self, hil_test, 'max_retry', 1)
        self.provider(self.op('pending', rc=124))
        self.run_board()

    TRACKED = {'token': 't0k', 'supervisor': {'pid': 1, 'start': '1', 'lock': [1, 2]}, 'lock': '/s/hil_op_t0k.lock',
               'record': '/s/hil_op_t0k.json', 'log': '/s/hil_op_t0k.log', 'survivors': None}

    def handed(self):
        """hand_to_cleanup_holder's calls, each refused as the real one refuses nothing here: a
        fake tracked receipt would never recheck done, so a real holder could never be ended."""
        handed = []

        def hand(handles, reason, **kw):
            handed.append({'handles': list(handles), **kw})
            raise ValueError('cleanup holder handoff refused in this test')
        patch(self, hil_lock, 'hand_to_cleanup_holder', hand)
        return handed

    def test_a_security_read_left_running_stops_before_the_flash_and_holds_its_receipt(self):
        tracked = self.TRACKED
        self.provider(self.op('done', rc=0), security_cleanup='pending', security_receipts=(tracked,))
        handed = self.handed()
        name, err, failed, rows, _ = hil_test.test_board(self.board)
        self.assertEqual(self.flashes, [], 'an openocd session started beside the esptool child')
        self.assertEqual(hil_flash.cleanup_pending, {BOARD: (tracked,)})
        (kw,) = handed
        self.assertEqual(kw['receipts'], [tracked])
        self.assertIn('esptool child, record /s/hil_op_t0k.json', kw['detail'])
        rec = hil_lock.read_record(BOARD)                      # the refused hand-over's blocker carries it too
        self.assertEqual((rec['protected'], rec['receipts'], rec['locks']), (True, [tracked], [BOARD]))
        cell = rows[-1][1][hil_test.hil_report.CLEANUP_PENDING_CELL]
        self.assertIn('esptool child, record /s/hil_op_t0k.json', cell)
        self.assertIn('the protected record is the recovery blocker', cell)

    def test_a_security_read_of_unknown_cleanup_is_read_once_latched_and_handed_over(self):
        # its supervisor did not report: no proof the child ended, so no retry, park or release
        self.provider(self.op('done', rc=0), security_cleanup='unknown', security_receipts=(self.TRACKED,))
        handed = self.handed()
        name, err, failed, rows, _ = hil_test.test_board(self.board)
        self.assertEqual((len(self.reads), self.flashes), (1, []))
        self.assertEqual(hil_flash.cleanup_pending, {BOARD: (self.TRACKED,)})
        (kw,) = handed
        self.assertEqual(kw['receipts'], [self.TRACKED])
        rec = hil_lock.read_record(BOARD)
        self.assertEqual((rec['protected'], rec['receipts']), (True, [self.TRACKED]))

    def test_only_tracked_receipts_reach_the_holders_recheck(self):
        # a session whose receipt file could not be written leaves its native record: `end`'s, never a recheck's
        native = {'pid': 4242, 'startTime': '99', 'argv': ['openocd'], 'ports': [None, None, 6666], 'token': 'x',
                  'log': '/w/openocd.log', 'closed': False, 'tool': 'hil_openocd_esp', 'init': 'ok', 'submitted': 3}
        hil_flash.cleanup_pending[BOARD] = (native, self.TRACKED, self.RECEIPT)
        handed = self.handed()
        fh = hil_lock.take_board_lock(LOCK, 'hil_test.py', protected=True)
        cell = hil_test.retain_board_lock(BOARD, LOCK, fh, hil_test.cleanup_pending(BOARD))
        (kw,) = handed
        self.assertEqual(kw['handles'], [LOCK])
        self.assertEqual(hil_lock.read_record(LOCK)['locks'], [LOCK])
        self.assertEqual(kw['receipts'], [self.TRACKED])
        for text in ('openocd_esp session pid 4242 start 99, receipt file unwritten, log /w/openocd.log',
                     'esptool child, record /s/hil_op_t0k.json', self.RECEIPT):
            self.assertIn(text, kw['detail'])
            self.assertIn(text, cell)

    def run_failing_test(self):
        """run_board over a test whose own reset left an openocd_esp session running, then failed."""
        def failing_test(board):
            hil_flash.cleanup_pending[BOARD] = (self.RECEIPT,)
            raise RuntimeError('no enumeration')
        patch(self, hil_test, 'test_device_cdc_msc', failing_test)
        self.provider(self.op('done', rc=0))
        self.run_board()

    def test_pending_after_a_failed_test_stops_the_same_way(self):
        self.run_failing_test()

    def test_pending_after_a_failed_final_test_attempt_stops_the_same_way(self):
        patch(self, hil_test, 'max_retry', 1)
        self.run_failing_test()

    def test_pending_on_an_interrupt_is_handed_over_on_the_way_out(self):
        err = KeyboardInterrupt()
        err.operation = self.op('pending', rc=None)
        self.provider(err)
        with self.assertRaises(KeyboardInterrupt):
            hil_test.test_board(self.board)
        self.assert_retained()
        self.assertIn('lock held by cleanup holder', self.log[-1])

    def test_an_interrupt_before_the_receipt_is_stored_keeps_the_board_blocked(self):
        # the provider returned its pending operation, then the interrupt lands before the store
        class Interrupted(dict):
            def __setitem__(self, key, value):
                raise KeyboardInterrupt
        patch(self, hil_flash, 'cleanup_pending', Interrupted())
        self.provider(self.op('pending', rc=124))
        with self.assertRaises(KeyboardInterrupt):
            hil_test.test_board(self.board)
        self.assertEqual(len(self.flashes), 1)
        rec = hil_lock.read_record(BOARD)
        self.assertTrue(hil_lock.is_recovery_blocker(rec), rec)
        with self.assertRaises(hil_lock.RecoveryBlocked):
            hil_lock.take_board_lock(BOARD, 'peer')

    def test_no_later_session_starts_on_the_board(self):
        self.provider(self.op('pending'))
        with redirect_stdout(io.StringIO()):
            hil_flash.flash_openocd_esp(self.board, '/fw/cdc_msc.bin')
        with self.assertRaisesRegex(RuntimeError, 'cleanup pending'):
            hil_flash.flash_openocd_esp(self.board, '/fw/cdc_msc.bin')
        self.assertEqual(len(self.flashes), 1)

    def test_the_workers_next_task_runs_while_the_holder_lives(self):
        # hil_test's Pool replaces a worker per board: a holder keeping the worker's
        # multiprocessing descriptors would stall the next board until it exits
        self.provider(self.op('pending'))
        pool = hil_test._mp.Pool(1, maxtasksperchild=1)
        self.addCleanup(pool.terminate)
        self.assertEqual(pool.apply_async(_board_in_worker, (self.board,)).get(timeout=60), 2)
        self.assertEqual(pool.apply_async(_next_task, (7,)).get(timeout=20), 7)
        self.assert_retained()

    def test_under_a_ci_job_the_route_is_refused_before_the_provider(self):
        job_env = hil_util.openocd_esp().CI_JOB_ENV
        self.provider(self.op('done', rc=0))
        for var in job_env:
            with self.subTest(var=var), mock.patch.dict(os.environ, {var: 'x'}), \
                    self.assertRaisesRegex(RuntimeError, 'CI job'):
                hil_flash.flash_openocd_esp(self.board, '/fw/cdc_msc.bin')
        self.assertEqual(self.flashes, [])

    def test_done_releases_the_lock_as_usual(self):
        self.provider(self.op('done', rc=0))
        name, err, failed, rows, _ = hil_test.test_board(self.board)
        self.assertEqual((err, failed), (0, []))
        self.assertEqual(hil_flash.cleanup_pending, {})
        self.assertEqual(len(self.flashes), 2)                 # the test and the park
        self.assertEqual(hil_lock.record_state(BOARD)[0], 'empty')
        hil_lock.release_board_lock(hil_lock.take_board_lock(BOARD, 'peer'))

    def assert_blocker(self, cell, why, lock=BOARD):
        self.assertEqual(hil_test.hil_report.cell_state(cell), 'fail')
        self.assertIn(why, cell)
        self.assertIn('the protected record is the recovery blocker', cell)
        rec = hil_lock.read_record(lock)
        self.assertFalse(hil_lock.is_cleanup_holder(rec), rec)
        self.assertEqual((rec['protected'], rec['locks'], rec['detail']),
                         (True, [lock], f'openocd_esp cleanup pending, receipt {self.RECEIPT}'))
        with self.assertRaises(hil_lock.RecoveryBlocked):
            hil_lock.take_board_lock(lock, 'peer')

    def test_an_unacknowledged_hand_over_leaves_the_protected_record_as_the_blocker(self):
        self.provider(self.op('pending'))
        # a holder that exits before reporting readiness
        patch(self, hil_lock, '_holder_argv', lambda job: [sys.executable, '-c', 'pass'])
        _, err, _, rows, _ = hil_test.test_board(self.board)
        self.assertEqual(err, 2)
        self.assert_blocker(rows[-1][1][hil_test.hil_report.CLEANUP_PENDING_CELL], 'cleanup holder handoff failed')

    def test_a_refused_hand_over_protects_the_record_as_the_blocker(self):
        # an unprotected handle: hand_to_cleanup_holder refuses, changing nothing, and the board
        # is protected here instead of released
        fh = hil_lock.take_board_lock(LOCK, hil_lock.CI_REASON)
        cell = '❌ ' + hil_test.retain_board_lock(BOARD, LOCK, fh, f'openocd_esp cleanup pending, receipt {self.RECEIPT}')
        self.assertTrue(fh.closed)
        self.assert_blocker(cell, 'cleanup holder handoff refused', lock=LOCK)


if __name__ == '__main__':
    unittest.main()
