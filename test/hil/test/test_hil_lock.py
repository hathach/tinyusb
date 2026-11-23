#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# The hil_lock adapter: callers get agentrc's board-lock primitive from the agentrc helper/agentrc.py
# selects (agentrc's tests/test_hil_lock.py covers the primitive and board_task itself), hil_test refuses a
# board whose lock is unavailable or whose probe is busy before flashing, a recovery blocker refuses the board
# as a blocker rather than contention (RecoveryBlockerIsNotContention), each board task ends on its
# cleanup verdict (FailedCleanup, Verdicts), `hold --all` takes this host's roster, and an
# incomplete agentrc refuses at import (test_hil_agentrc.py covers the loader). Every lock lives in a
# temp dir under a name no rig uses. Run directly:
#   python3 test/hil/test/test_hil_lock.py
import errno
import json
import os
import shutil
import signal
import subprocess
import sys
import tempfile
import time
import types
import unittest
from pathlib import Path
from unittest import mock

HIL_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
REPO = os.path.dirname(os.path.dirname(HIL_DIR))
sys.path.insert(0, HIL_DIR)

if sys.platform == 'win32':
    raise unittest.SkipTest("agentrc's board lock is an fcntl flock: POSIX only")

import fcntl   # noqa: E402
from usbtest_harness import patch, wait_for   # noqa: E402 - stubs pyserial before hil_test imports it
import hil_flash         # noqa: E402
import hil_test          # noqa: E402
from helper import agentrc, hil_lock, hil_util   # noqa: E402
from test_hil_agentrc import scratch_agentrc, scratch_env   # noqa: E402
from test_hil_flash import admit_flash, fake_openocd_esp   # noqa: E402

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
        self.users = []     # what the busy check names; an exception raises from it
        patch(self, hil_flash, 'probe_busy', self.probe_busy)

    def probe_busy(self, board):
        if isinstance(self.users, BaseException):
            raise self.users
        return self.users

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
        def held(name, *a, **kw):
            raise hil_lock.BoardLocked('board locked: {"pid": 1}')
        patch(self, hil_lock, 'acquire_board_lock', held)
        _, err, _, rows, _ = hil_test.test_board(self.board)
        self.assertEqual(err, 1)
        self.assertEqual(rows[0][1], {hil_test.hil_report.LOCKED_CELL: 'fail'})

    def test_a_registered_board_is_locked_by_its_registry_name(self):
        probe, reg = next(iter(hil_util.hil_boards().registry().items()))
        board = hil_util.hil_boards().resolve({'name': BOARD, 'uid': 'ABC123', 'probe': probe,
                                               'tests': {'only': ['device/cdc_msc']}})
        locked = []

        def held(name, *a, **kw):
            locked.append(name)
            raise hil_lock.BoardLocked('board locked: {"pid": 1}')
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

    def test_a_takeover_journal_on_a_held_flock_stays_contention(self):
        self.hold(hil_lock.CI_REASON, protected=True)
        with open(os.path.join(self.dir, BOARD + hil_lock.JOURNAL), 'w') as f:
            json.dump({'token': 't0k', 'pid': 1, 'locks': [BOARD], 'predecessor': '{}'}, f)
        self.assertEqual(self.refusal(), {hil_test.hil_report.LOCKED_CELL: 'fail'})

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
        release predicate and end_failed load it beside hil_lock)."""
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


class FailedCleanup(LockDir):
    """What an openocd_esp operation left not proven ended, listed in the provider's pending
    record of the board's USJ (a session's receipt, an esptool child's, or an unreceipted entry),
    stops the board: no retry, no further test, no park, and its receipts are logged before the
    task releases its lock. The fake provider refuses and lists as the real one does (its
    pending_refusal and latch, which agentrc's tests pin)."""

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
        patch(self, hil_flash, 'OPENOCD_ESP_DIR', hil_test.Path(self.dir) / 'openocd_esp')
        patch(self, hil_test, 'test_device_cdc_msc', lambda board: None)
        self.board = {'name': BOARD, 'uid': 'ABC123', 'tests': {'only': ['device/cdc_msc']},
                      'flasher': {'name': 'openocd_esp', 'uid': '30:ED:A0:E0:C9:02',
                                  'args': '-f board/esp32p4-builtin.cfg'}}
        self.lock = BOARD
        self.real = hil_util.openocd_esp()
        self.serial = self.board['flasher']['uid']

    def pending(self):
        return self.real.pending(self.serial)

    def provider(self, outcome, security_cleanup='done', security_receipts=()):
        """hil_util.openocd_esp() whose flash records the lock record it ran under, then
        returns or raises `outcome`; its security read ends with `security_cleanup`. Each refuses
        while the pending record lists an entry and lists what it leaves, as the provider does."""
        real, serial = self.real, self.serial

        def refuse_pending():
            if why := real.pending_refusal(serial):
                raise RuntimeError(why)

        def flash(flasher, segments, *, security, **kw):
            admit_flash(flasher, security)
            refuse_pending()
            self.flashes.append(hil_lock.read_record(self.lock))
            if isinstance(outcome, BaseException):
                real.latch(serial, getattr(outcome, 'operation', None), 'test flash')
                raise outcome
            real.latch(serial, outcome, 'test flash')
            return outcome

        def read_security(serial, **kw):
            refuse_pending()
            self.reads.append(serial)
            op = self.ns(security={'flashEncryption': 'off', 'secureBoot': 'off', 'source': 'test'}, serial=serial,
                         returncode=0, cleanup=security_cleanup, receipts=security_receipts)
            if op.cleanup != 'done':
                real.latch(serial, op, 'test read_security')
                e = RuntimeError(f'esptool_session: cleanup {op.cleanup}, its child is not proven ended')
                e.operation = op
                raise e
            return op
        self.reads = []
        fake_openocd_esp(self, self.dir, admit=lambda f: ('esp32p4', ['board/esp32p4-builtin.cfg']),
                         flash_openocd_esp=flash, read_security=read_security,
                         segments_from_flash_args=lambda *a, **kw: [{'offset': 0, 'file': '/fw/cdc_msc.bin', 'size': 4}])

    def op(self, cleanup, rc=1):
        return self.ns(returncode=rc, stdout='', cleanup=cleanup, receipts=(self.RECEIPT,) if cleanup == 'pending' else (),
                       session={'failure': 'init failed', 'log': '/x/openocd.log', 'dir': '/nonexistent'})

    def end_session(self):
        """The receipt as `hil_openocd_esp.py end` leaves it on exit 0."""
        with open(self.RECEIPT, 'w') as f:
            json.dump({'tool': 'hil_openocd_esp', 'pid': 4242, 'startTime': '99', 'closed': True, 'cleanup': 'done'}, f)

    def assert_released(self, lock=None):
        lock = lock or self.lock
        self.assertEqual(hil_lock.record_state(lock)[0], 'empty')
        fh = hil_lock.take_board_lock(lock, 'peer')
        try:
            self.assertTrue(fh.cleanup_failures)
            return fh.cleanup_failures[-1]
        finally:
            hil_lock.release_board_lock(fh)

    def assert_logged(self):
        failure = self.assert_released()
        self.assertIn(self.RECEIPT, failure['detail'])
        self.assertEqual(failure['retained'], [self.RECEIPT])
        self.assertEqual((failure['task']['cleanupOwed'], failure['task']['locks']), (True, [self.lock]))
        return failure

    def run_board(self):
        # no retry wait beside an outstanding session
        sleep = mock.Mock()
        fake_time = types.ModuleType('time')
        fake_time.__dict__.update(vars(time), sleep=sleep)
        with mock.patch.object(hil_test, 'time', fake_time):
            name, err, failed, rows, _ = hil_test.test_board(self.board)
        sleep.assert_not_called()   # never the 0.5 s retry wait
        self.assertEqual((name, err, failed), (BOARD, 2, []))   # the test's flash and the blocker
        self.assertEqual(len(self.flashes), 1, 'retried or parked beside an outstanding session')
        self.assertEqual((self.flashes[0]['cleanupOwed'], self.flashes[0]['locks']), (True, [self.lock]),
                         self.flashes[0])
        self.assert_logged()
        cell = rows[-1][1][hil_test.hil_report.CLEANUP_PENDING_CELL]
        self.assertEqual(hil_test.hil_report.cell_state(cell), 'fail')
        self.assertIn(self.RECEIPT, cell)
        self.assertIn('lock released', cell)
        self.assertIn(str(Path(self.dir) / f'{self.lock}.cleanup.jsonl'), cell)
        return rows

    def test_pending_on_return_logs_the_failure_and_releases_the_lock(self):
        self.provider(self.op('pending', rc=124))
        self.run_board()
        self.assertEqual(self.pending().read(), [{'receipt': self.RECEIPT}])
        failures = hil_lock.open_cleanup_failures(BOARD)
        self.assertTrue(hil_lock.resolve_restored(BOARD, failures, evidence='parked'))
        self.assertEqual(hil_lock.open_cleanup_failures(BOARD), failures)
        self.end_session()
        self.assertEqual(hil_lock.resolve_restored(BOARD, failures, evidence='parked'), [])
        self.assertEqual(hil_lock.open_cleanup_failures(BOARD), [])
        self.assertEqual(self.real.pending_refusal(self.serial), '')     # the ended session clears the record
        self.assertEqual(self.pending().read(), [])
        self.assertIn('earlier in this run', hil_test.cleanup_pending(self.board))   # this run's latch stays

    def test_a_registered_boards_failure_is_logged_under_its_registry_lock(self):
        probe, reg = next(iter(hil_util.hil_boards().registry().items()))
        board = hil_util.hil_boards().resolve({'name': BOARD, 'uid': 'ABC123', 'probe': probe,
                                               'tests': {'only': ['device/cdc_msc']}})
        self.board = {**board, 'flasher': self.board['flasher']}
        self.lock = reg['name']
        self.provider(self.op('pending', rc=124))
        self.run_board()
        self.assertEqual(self.pending().read(), [{'receipt': self.RECEIPT}])
        self.assertEqual(hil_lock.record_state(BOARD)[0], 'absent')  # never the roster name's lock

    def test_pending_on_an_exception_logs_and_releases_too(self):
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

    def test_a_security_read_left_running_stops_before_the_flash_and_logs_its_receipt(self):
        self.provider(self.op('done', rc=0), security_cleanup='pending', security_receipts=(self.TRACKED,))
        name, err, failed, rows, _ = hil_test.test_board(self.board)
        self.assertEqual((name, err, failed), (BOARD, 2, []))
        self.assertEqual(self.flashes, [], 'an openocd session started beside the esptool child')
        self.assertEqual(self.pending().read(), [{'receipt': self.TRACKED}])
        failure = self.assert_released()
        self.assertEqual((failure['receipts'], failure['retained']), ([self.TRACKED], []))
        cell = rows[-1][1][hil_test.hil_report.CLEANUP_PENDING_CELL]
        self.assertIn('esptool child, record /s/hil_op_t0k.json', cell)
        self.assertIn('lock released', cell)

    def test_a_security_read_of_unknown_cleanup_is_read_once_latched_and_logged(self):
        self.provider(self.op('done', rc=0), security_cleanup='unknown', security_receipts=(self.TRACKED,))
        hil_test.test_board(self.board)
        self.assertEqual((len(self.reads), self.flashes), (1, []))
        self.assertEqual(self.pending().read(), [{'receipt': self.TRACKED}])
        self.assertEqual(self.assert_released()['receipts'], [self.TRACKED])

    def test_a_session_its_record_could_not_list_stops_the_board_and_is_logged(self):
        # the provider's latch raises carrying the Operation when the record cannot take its entry
        def full(record, sessions):
            raise OSError(errno.ENOSPC, 'No space left on device')
        patch(self, self.real.hil_lock.PendingSessions, 'add', full)
        self.provider(self.op('pending', rc=124))
        rows = self.run_board()                     # no retry, no park; the receipt is retained evidence
        self.assertIn('could not list', rows[-1][1][hil_test.hil_report.CLEANUP_PENDING_CELL])
        self.assertEqual(self.pending().read(), [])  # never listed, still refused in this process

    def test_a_child_whose_entry_ended_later_still_stops_the_run(self):
        # the first security read leaves its child pending, whose recheck is done by the next check, so the
        # record clears; the run stops after the first read all the same and logs the cleanup failure
        patch(self, self.real.hil_process, 'recheck', lambda receipt: 'done')
        self.provider(self.op('done', rc=0), security_cleanup='pending', security_receipts=(self.TRACKED,))
        name, err, failed, rows, _ = hil_test.test_board(self.board)
        self.assertEqual((err, len(self.reads), self.flashes), (2, 1, []))
        self.assertEqual(self.pending().read(), [])
        failure = self.assert_released()
        self.assertEqual((failure['receipts'], failure['retained']), ([self.TRACKED], []))
        self.assertIn('earlier in this run', rows[-1][1][hil_test.hil_report.CLEANUP_PENDING_CELL])

    def test_a_write_ahead_entry_a_killed_run_left_stops_the_next_run(self):
        # the provider lists a session's receipt path before spawning its server: a run SIGKILLed after that
        # leaves a receipt that may never have been written, which reads unreadable and refuses
        unwritten = os.path.join(self.dir, 'killed', 'receipt.json')
        self.pending().add([{'receipt': unwritten}])
        self.provider(self.op('done', rc=0))
        name, err, failed, rows, _ = hil_test.test_board(self.board)
        self.assertEqual((err, self.reads, self.flashes), (1, [], []))     # the blocker alone: no test starts
        self.assertEqual(rows[0][1]['device/cdc_msc'], f'{hil_test.hil_report.REPORT_CELL["skip"]} cleanup pending')
        failure = self.assert_released()
        self.assertEqual(failure['retained'], [unwritten])
        self.assertIn(unwritten, rows[-1][1][hil_test.hil_report.CLEANUP_PENDING_CELL])

    def test_a_pending_child_that_ends_after_the_release_still_fails_the_board(self):
        # every test skipped beside it: the blocker seen under the lock stands, or the board reports
        # green with no test run and drops out of the re-run spec
        self.pending().add([{'receipt': self.TRACKED}])
        patch(self, self.real.hil_process, 'recheck',
              lambda receipt: 'pending' if hil_lock.record_state(self.lock)[0] == 'record' else 'done')
        self.provider(self.op('done', rc=0))
        name, err, failed, rows, _ = hil_test.test_board(self.board)
        self.assertEqual(hil_test.cleanup_pending(self.board), '')     # ended once the lock was released
        self.assertEqual((err, failed, self.reads, self.flashes), (1, [], [], []))
        self.assertEqual(rows[0][1]['device/cdc_msc'], f'{hil_test.hil_report.REPORT_CELL["skip"]} cleanup pending')
        cell = rows[-1][1][hil_test.hil_report.CLEANUP_PENDING_CELL]
        self.assertIn('esptool child, record /s/hil_op_t0k.json', cell)
        self.assertIn('lock released', cell)
        self.assertEqual(self.assert_released()['receipts'], [self.TRACKED])

    def test_a_stray_beside_a_pending_record_is_never_swept_through_the_recovery_probe(self):
        # a convoy-safe recovery entry other than openocd_esp resets the chip without consulting the USJ's record
        self.board['flasher_recover'] = {'name': 'openocd', 'args': '-f interface/ftdi/esp32_devkitj_v1.cfg'}
        self.pending().add([{'receipt': self.RECEIPT}])
        resets = []
        patch(self, hil_test.usbtest, 'strays_on', lambda uid: [{'pid': 40, 'start': '1', 'node': '/dev/bus/usb/1/2'}])
        patch(self, hil_test.usbtest, 'RECOVER_SETTLE', 0)
        patch(self, hil_test.usbtest, '_uo', lambda: types.SimpleNamespace(proc_holds=lambda stray: False))
        patch(self, hil_flash, 'convoy_safe', lambda flasher: True)
        patch(self, hil_flash, 'reset_openocd', lambda board, timeout=None, **kw: resets.append(board['flasher']))
        self.provider(self.op('done', rc=0))
        name, err, failed, rows, _ = hil_test.test_board(self.board)
        self.assertEqual((resets, self.reads, self.flashes), ([], [], []))
        self.assertEqual((err, failed), (1, []))      # the blocker alone
        self.assertEqual(rows[0][1]['device/cdc_msc'], f'{hil_test.hil_report.REPORT_CELL["skip"]} cleanup pending')
        self.assertIn(self.RECEIPT, rows[-1][1][hil_test.hil_report.CLEANUP_PENDING_CELL])
        self.assertTrue(any('stray testusb not swept' in line for line in self.log), self.log)

    def security_state_dir(self):
        """The provider's write-ahead entry of its run_tracked state dir, with the marker its owner flocks
        until the child's receipt is listed (aheadLock), as a killed read leaves them: (entry, marker)."""
        state = os.path.join(self.dir, 'security-ops')
        os.makedirs(state, mode=0o700)
        marker = os.path.join(state, 'ahead_0123456789abcdef.lock')
        open(marker, 'w').close()
        entry = {'stateDir': state, 'what': 'read_security on /dev/ttyACM0', 'aheadLock': marker}
        self.pending().add([{'receipt': entry}])
        return entry, marker

    def test_a_security_reads_state_dir_whose_children_ended_clears_by_itself(self):
        # ended once its marker is released and recheck_dir finds every operation there done
        self.security_state_dir()
        self.provider(self.op('done', rc=0))
        name, err, failed, rows, _ = hil_test.test_board(self.board)
        self.assertEqual((err, failed, len(self.flashes)), (0, [], 2))     # the test and the park
        self.assertEqual(self.pending().read(), [])

    def test_a_security_reads_state_dir_whose_marker_is_held_stops_the_board_unrecheckable(self):
        # a live owner, whose child may not have started in the dir yet: never read done from an empty dir
        entry, marker = self.security_state_dir()
        self.provider(self.op('done', rc=0))
        with open(marker) as held:
            fcntl.flock(held, fcntl.LOCK_EX)
            _, err, _, _, _ = hil_test.test_board(self.board)
        self.assertEqual((err, self.reads, self.flashes), (1, [], []))
        self.assertEqual(self.pending().read(), [{'receipt': entry}])
        failure = self.assert_released()
        self.assertEqual(failure['retained'], [])
        (logged,) = failure['receipts']
        self.assertIn(f"esptool children in state dir {entry['stateDir']}", logged['unrecheckable'])

    def test_a_security_read_that_left_no_receipt_is_logged_unrecheckable(self):
        self.provider(self.op('done', rc=0), security_cleanup='unknown')
        hil_test.test_board(self.board)
        self.assertEqual(self.flashes, [])
        failure = self.assert_released()
        (entry,) = failure['receipts']
        self.assertIn('left no receipt', entry['unrecheckable'])
        self.assertEqual(failure['retained'], [])
        self.assertTrue(hil_lock.resolve_restored(BOARD, [failure], evidence='parked'))   # stays open

    def test_tracked_and_retained_receipts_are_logged_separately(self):
        native = {'pid': 4242, 'startTime': '99', 'argv': ['openocd'], 'ports': [None, None, 6666], 'token': 'x',
                  'log': '/w/openocd.log', 'closed': False, 'tool': 'hil_openocd_esp', 'init': 'ok', 'submitted': 3}
        self.pending().add([{'receipt': r} for r in (native, self.TRACKED, self.RECEIPT)])
        cell = hil_test.cleanup_pending(self.board)
        with hil_lock.board_task(LOCK, 'hil_test.py', busy=list) as task:
            task.failed(detail=cell, **hil_test.pending_evidence(self.board, cell))
        self.assertTrue(task.released)
        failure = self.assert_released(LOCK)
        self.assertEqual(failure['task']['locks'], [LOCK])
        self.assertEqual(failure['receipts'], [self.TRACKED])
        self.assertEqual(failure['retained'], [native, self.RECEIPT])
        for text in ('openocd_esp session pid 4242 start 99, receipt file unwritten, log /w/openocd.log',
                     'esptool child, record /s/hil_op_t0k.json', self.RECEIPT):
            self.assertIn(text, failure['detail'])
            self.assertIn(text, cell)

    def run_failing_test(self):
        """run_board over a test whose own reset left an openocd_esp session running, then failed."""
        def failing_test(board):
            self.pending().add([{'receipt': self.RECEIPT}])
            raise RuntimeError('no enumeration')
        patch(self, hil_test, 'test_device_cdc_msc', failing_test)
        self.provider(self.op('done', rc=0))
        self.run_board()

    def test_pending_after_a_failed_test_stops_the_same_way(self):
        self.run_failing_test()

    def test_pending_after_a_failed_final_test_attempt_stops_the_same_way(self):
        patch(self, hil_test, 'max_retry', 1)
        self.run_failing_test()

    def test_pending_on_an_interrupt_is_logged_and_released_on_the_way_out(self):
        err = KeyboardInterrupt()
        err.operation = self.op('pending', rc=None)
        self.provider(err)
        with self.assertRaises(KeyboardInterrupt):
            hil_test.test_board(self.board)
        self.assert_logged()
        self.assertIn('lock released', self.log[-1])

    def test_an_interrupt_before_the_receipt_is_listed_logs_unrecheckable_evidence(self):
        # the session ended pending, then the interrupt lands before the provider lists its receipt
        self.provider(KeyboardInterrupt())
        with self.assertRaises(KeyboardInterrupt):
            hil_test.test_board(self.board)
        self.assertEqual(len(self.flashes), 1)
        failure = self.assert_released()
        self.assertIn('receipt missing', failure['receipts'][0]['unrecheckable'])
        self.assertTrue(hil_lock.resolve_restored(BOARD, [failure], evidence='parked'))

    def run_interrupted_verdict(self):
        """run_failing_test's board under a real SIGINT raised as the verdict gathers its evidence,
        before task.failed stores it: the pending receipts are still logged, then the interrupt raised."""
        old = signal.signal(signal.SIGINT, signal.default_int_handler)
        self.addCleanup(signal.signal, signal.SIGINT, old)
        evidence, raised = hil_test.pending_evidence, []

        def interrupted(board, pending):
            if not raised:
                raised.append(board['name'])
                signal.raise_signal(signal.SIGINT)
            return evidence(board, pending)
        patch(self, hil_test, 'pending_evidence', interrupted)

        def failing_test(board):
            self.pending().add([{'receipt': self.RECEIPT}])
            raise RuntimeError('no enumeration')
        patch(self, hil_test, 'test_device_cdc_msc', failing_test)
        self.provider(self.op('done', rc=0))
        with self.assertRaises(KeyboardInterrupt):
            hil_test.test_board(self.board)
        self.assertEqual(raised, [BOARD])
        failure = self.assert_logged()
        self.assertNotIn('without a cleanup verdict', failure['detail'])
        self.assertIn('lock released', self.log[-1])

    def test_an_interrupt_as_the_verdict_starts_still_logs_the_pending_receipts(self):
        self.run_interrupted_verdict()

    def test_an_interrupt_as_the_collector_is_called_still_logs_the_pending_receipts(self):
        fired = []

        def tracer(frame, event, arg):    # a real SIGINT as BoardTask calls _give_verdict, its collector
            if not fired and event == 'call' and frame.f_code is hil_test._give_verdict.__code__:
                fired.append(1)
                signal.raise_signal(signal.SIGINT)
        prev = sys.gettrace()
        sys.settrace(tracer)
        self.addCleanup(sys.settrace, prev)
        self.run_interrupted_verdict()
        sys.settrace(prev)
        self.assertEqual(fired, [1])

    def test_a_board_whose_cleanup_failed_does_not_block_the_next_boards_process(self):
        self.provider(self.op('pending'))
        real = hil_test.test_board

        def board_task(board):
            return real(board) if board is self.board else (board['name'], 0, [], [], None)
        patch(self, hil_test, 'test_board', board_task)
        rows = []
        with mock.patch.object(os, 'cpu_count', return_value=1):
            hil_test.run_boards([self.board, {'name': 'next'}], time.monotonic() + 60, rows, 'seed')
        self.assertEqual([(r[0], r[1]) for r in rows], [(self.board['name'], 2), ('next', 0)])
        self.assert_logged()

    def test_done_releases_the_lock_as_usual(self):
        self.provider(self.op('done', rc=0))
        name, err, failed, rows, _ = hil_test.test_board(self.board)
        self.assertEqual((err, failed), (0, []))
        self.assertEqual(self.pending().read(), [])
        self.assertEqual(len(self.flashes), 2)                 # the test and the park
        self.assertEqual(hil_lock.record_state(BOARD)[0], 'empty')
        hil_lock.release_board_lock(hil_lock.take_board_lock(BOARD, 'peer'))

    def test_a_log_write_failure_still_releases_and_reports_the_error(self):
        self.provider(self.op('pending'))
        with mock.patch.object(hil_lock, 'log_cleanup_failure', side_effect=OSError('log is read-only')):
            _, err, _, rows, _ = hil_test.test_board(self.board)
        self.assertEqual(err, 2)
        cell = rows[-1][1][hil_test.hil_report.CLEANUP_PENDING_CELL]
        self.assertIn('failure NOT logged', cell)
        self.assertIn('cleanup log not written', cell)
        self.assertIn('log is read-only', cell)
        self.assertEqual(hil_lock.record_state(BOARD)[0], 'record')    # kept, its only evidence: the next take logs it
        fh = hil_lock.take_board_lock(BOARD, 'peer')
        self.assertEqual(fh.cleanup_failures[-1]['how'], 'found-left')
        hil_lock.release_board_lock(fh)

    def test_a_later_run_reports_the_latest_failure_and_counts_other_open_ones(self):
        first = hil_lock.log_cleanup_failure(BOARD, 'earlier', detail='old session', retained=[self.RECEIPT])
        second = hil_lock.log_cleanup_failure(BOARD, 'earlier', detail='old child', receipts=[self.TRACKED])
        self.provider(self.op('done', rc=0))
        _, err, _, rows, _ = hil_test.test_board(self.board)
        self.assertEqual(err, 0)
        cell = rows[-1][1][hil_test.hil_report.PREVIOUS_CLEANUP_CELL]
        self.assertEqual(cell, hil_lock.failure_text(second) + ' (+1 more open); 2 still open')
        self.assertNotIn(first['id'], cell)
        self.assertEqual(hil_lock.open_cleanup_failures(BOARD), [first, second])
        self.assertIn('2 open cleanup failure(s)', self.log[0])
        for failure in (first, second):
            self.assertIn(failure['id'], self.log[0])
        self.assertIn('stays open:', self.log[-1])

    def test_a_successful_park_resolves_only_the_failures_with_ended_evidence(self):
        self.end_session()
        ended = hil_lock.log_cleanup_failure(BOARD, 'earlier', detail='old session', retained=[self.RECEIPT])
        pending = hil_lock.log_cleanup_failure(BOARD, 'earlier', detail='old child', receipts=[self.TRACKED])
        self.provider(self.op('done', rc=0))
        _, err, _, rows, _ = hil_test.test_board(self.board)
        self.assertEqual(err, 0)
        self.assertEqual(rows[0][1][hil_test.hil_report.PREVIOUS_CLEANUP_CELL],
                         hil_lock.failure_text(pending) + ' (+1 more open); 1 still open')
        self.assertEqual(hil_lock.open_cleanup_failures(BOARD), [pending])

    def test_a_single_previous_failure_is_attached_to_the_first_variant_row(self):
        failure = hil_lock.log_cleanup_failure(BOARD, 'earlier', detail='old session', retained=[self.RECEIPT])
        self.board['variant'] = [{'name': 'different-variant'}]
        self.provider(self.op('done', rc=0))
        _, err, _, rows, _ = hil_test.test_board(self.board)
        self.assertEqual(err, 0)
        self.assertEqual(len(rows), 1)
        self.assertEqual(rows[0][0], 'different-variant')
        self.assertEqual(rows[0][1][hil_test.hil_report.PREVIOUS_CLEANUP_CELL],
                         hil_lock.failure_text(failure) + '; 1 still open')

    def test_a_restoration_exception_keeps_its_diagnostic_in_the_notice(self):
        failure = hil_lock.log_cleanup_failure(BOARD, 'earlier', detail='old session', retained=[self.RECEIPT])
        self.provider(self.op('done', rc=0))
        with mock.patch.object(hil_lock, 'resolve_restored', side_effect=OSError('log inaccessible')):
            _, err, _, rows, _ = hil_test.test_board(self.board)
        self.assertEqual(err, 0)
        self.assertEqual(rows[0][1][hil_test.hil_report.PREVIOUS_CLEANUP_CELL],
                         hil_lock.failure_text(failure) + "; board task notes: restoration: OSError('log inaccessible')")

    def test_skip_flash_does_not_claim_an_earlier_cleanup_restored(self):
        self.end_session()
        failure = hil_lock.log_cleanup_failure(BOARD, 'earlier', detail='old session', retained=[self.RECEIPT])
        patch(self, hil_test, 'skip_flash', True)
        self.provider(self.op('done', rc=0))
        self.assertEqual(hil_test.test_board(self.board)[1], 0)
        self.assertEqual(hil_lock.open_cleanup_failures(BOARD), [failure])

    def test_an_unproven_park_does_not_resolve_an_earlier_failure_and_fails_its_own_cleanup(self):
        self.end_session()
        failure = hil_lock.log_cleanup_failure(BOARD, 'earlier', detail='old session', retained=[self.RECEIPT])
        for outcome, why in (((1, 'fail', None), 'park flash failed'),
                             ((0, 'skip', None), 'no park (device/board_test) firmware')):
            with self.subTest(outcome=outcome):
                def example(board, variant, test):
                    return outcome if test == 'device/board_test' else (0, 'pass', None)
                with mock.patch.object(hil_test, 'test_example', example):
                    _, err, _, rows, _ = hil_test.test_board(self.board)
                self.assertEqual(err, 0)    # the park is teardown: its failure is the next take's notice
                opened = hil_lock.open_cleanup_failures(BOARD)
                self.assertEqual(opened[0], failure)
                self.assertEqual((opened[-1]['detail'], opened[-1]['receipts']), (f'board not restored: {why}', []))
                self.assertIn('board not restored', self.log[-1])

    def test_previous_failures_do_not_erase_a_current_wedge_on_the_same_row(self):
        hil_lock.log_cleanup_failure(BOARD, 'earlier', detail='old session', retained=[self.RECEIPT])
        patch(self, hil_test.usbtest, 'strays_on', lambda uid: ['stray'])
        patch(self, hil_test.usbtest, 'recover_strays', lambda board, strays: 'still wedged')
        _, err, _, rows, _ = hil_test.test_board(self.board)
        self.assertEqual(err, 1)
        with tempfile.TemporaryDirectory() as tmp:
            report = hil_test.hil_report
            report.accumulate_report([(BOARD, err, [], rows, None)], Path(tmp), True)
            doc, _ = report._load(Path(tmp))
        result = report.normalised_rows({'boards': [self.board]}, [BOARD], doc)[0]
        self.assertEqual(result['verdict'], 'blocker')
        self.assertIn('wedged', result['detail'])
        self.assertIn('old session', result['detail'])

    def test_a_free_legacy_record_is_logged_and_admitted(self):
        self.provider(self.op('done', rc=0))
        for extra in ({'role': hil_lock.CLEANUP_ROLE, 'token': 'old'}, {'protected': True}, {'cleanupOwed': True}):
            with self.subTest(extra=extra):
                fh = hil_lock.flock_nb(BOARD)
                hil_lock.write_record(fh, 'old task', **extra)
                fh.close()
                _, err, _, rows, _ = hil_test.test_board(self.board)
                self.assertEqual(err, 0)
                self.assertIn('old task', rows[-1][1][hil_test.hil_report.PREVIOUS_CLEANUP_CELL])

    def test_a_free_takeover_journal_is_logged_and_admitted(self):
        journal = {'pid': 1, 'predecessor': '{}'}
        Path(self.dir, BOARD + hil_lock.JOURNAL).write_text(json.dumps(journal))
        self.provider(self.op('done', rc=0))
        _, err, _, rows, _ = hil_test.test_board(self.board)
        self.assertEqual(err, 0)
        self.assertIn(hil_test.hil_report.PREVIOUS_CLEANUP_CELL, rows[-1][1])
        failure, = [e for e in hil_lock.cleanup_events(BOARD) if e['event'] == 'failed']
        self.assertEqual(failure['journal'], journal)


_HELD = []


def _gap_in_worker(lock):
    """A task whose interrupt landed as __exit__ was entered: its flock still held, and its handle
    still referenced (as by a traceback), when the task returns."""
    task = hil_lock.board_task(lock, 'hil_test.py', busy=list)
    task.__enter__()
    _HELD.append(task)
    return os.getpid()


class Admission(LockDir):
    """The busy check runs under the taken lock before any hardware access: a named user or an
    inconclusive check refuses the board with a probe-busy cell, releases the lock and logs nothing."""

    def setUp(self):
        super().setUp()
        self.log = []
        patch(self, hil_test, 'log_line', self.log.append)
        patch(self, hil_flash, 'flash_primitive', lambda *a: self.fail('flashed'))
        patch(self, hil_test.usbtest, 'strays_on', lambda uid: self.fail('swept'))
        self.board = {'name': BOARD, 'uid': 'ABC123', 'flasher': {'name': 'jlink', 'uid': 'P'},
                      'tests': {'only': ['device/cdc_msc']}}

    def refusal(self):
        events = hil_lock.cleanup_events(BOARD)
        name, err, failed, rows, _ = hil_test.test_board(self.board)
        self.assertEqual((name, err, failed, len(rows)), (BOARD, 1, [], 1))
        self.assertEqual(hil_lock.record_state(BOARD)[0], 'empty')
        self.assertEqual(hil_lock.cleanup_events(BOARD), events, 'a refusal logged an event')
        hil_lock.release_board_lock(hil_lock.take_board_lock(BOARD, 'peer'))   # free
        return rows[0][1]

    def test_a_live_probe_user_refuses_the_board(self):
        self.users = [{'pid': 4242, 'cmd': 'JLinkExe -USB 000000123'}]
        cells = self.refusal()
        cell = cells[hil_test.hil_report.PROBE_BUSY_CELL]
        self.assertEqual(cell, f'{hil_test.hil_report.REPORT_CELL["fail"]} probe in use by pid 4242 '
                               f'(JLinkExe -USB 000000123)')
        self.assertEqual(hil_test.hil_report.cell_state(cell), 'fail')

    def test_a_long_probe_user_list_is_bounded(self):
        self.users = [{'pid': p, 'cmd': 'STM32_Programmer_CLI ' + 'x' * 300} for p in (1, 2, 3)]
        cell = self.refusal()[hil_test.hil_report.PROBE_BUSY_CELL]
        self.assertEqual(len(cell), len(f'{hil_test.hil_report.REPORT_CELL["fail"]} probe in use by ') + 500)

    def test_an_inconclusive_check_refuses_too(self):
        self.users = hil_flash.ProbeCheckFailed('RuntimeError: /proc may hide other users\' processes')
        cell = self.refusal()[hil_test.hil_report.PROBE_BUSY_CELL]
        self.assertIn('probe check: RuntimeError: /proc may hide', cell)

    def test_a_refusal_still_reports_the_failures_the_take_found(self):
        failure = hil_lock.log_cleanup_failure(BOARD, 'earlier', detail='old session')
        self.users = [{'pid': 1, 'cmd': 'openocd'}]
        cells = self.refusal()
        self.assertEqual(cells[hil_test.hil_report.PREVIOUS_CLEANUP_CELL], hil_lock.failure_text(failure))
        self.assertEqual(hil_lock.open_cleanup_failures(BOARD), [failure])

    def test_a_refusal_whose_lock_did_not_end_is_a_blocker(self):
        self.users = [{'pid': 1, 'cmd': 'openocd'}]
        with mock.patch.object(hil_lock, 'clear_record', return_value=False):
            _, err, failed, rows, _ = hil_test.test_board(self.board)
        self.assertEqual((err, failed), (1, []))
        cells = rows[0][1]
        self.assertIn(hil_test.hil_report.PROBE_BUSY_CELL, cells)
        cell = cells[hil_test.hil_report.CLEANUP_PENDING_CELL]
        self.assertIn(f'board task ended with errors; {BOARD}: lock released', cell)
        self.assertIn('record not cleared', cell)
        self.assertTrue(any('BLOCKER: board task ended with errors' in line for line in self.log), self.log)

    def test_the_bypass_still_runs_the_busy_check(self):
        os.environ['HIL_NO_BOARD_LOCK'] = '1'
        self.users = [{'pid': 1, 'cmd': 'openocd'}]
        _, err, _, rows, _ = hil_test.test_board(self.board)
        self.assertEqual(err, 1)
        self.assertIn(hil_test.hil_report.PROBE_BUSY_CELL, rows[0][1])
        self.assertEqual(hil_lock.record_state(BOARD)[0], 'absent')

    def test_an_unrelated_error_is_no_refusal(self):
        # e.g. the provider's bounded /proc walk failing to start its thread: the run aborts on it
        self.users = RuntimeError("can't start new thread")
        with self.assertRaisesRegex(RuntimeError, "can't start new thread"):
            hil_test.test_board(self.board)
        self.assertEqual(hil_lock.record_state(BOARD)[0], 'empty')
        self.assertEqual(hil_lock.cleanup_events(BOARD), [])

    def test_a_refused_board_is_not_run_in_the_report(self):
        self.users = [{'pid': 1, 'cmd': 'openocd'}]
        _, err, _, rows, _ = hil_test.test_board(self.board)
        report = hil_test.hil_report
        with tempfile.TemporaryDirectory() as tmp:
            report.accumulate_report([(BOARD, err, [], rows, None)], Path(tmp), True)
            doc, _ = report._load(Path(tmp))
        result = report.normalised_rows({'boards': [self.board]}, [BOARD], doc)[0]
        self.assertEqual(result['verdict'], 'not-run')
        self.assertIn('probe in use by pid 1', result['detail'])


class Verdicts(LockDir):
    """test_board's cleanup verdict on a board with no provider session: a successful park
    restores, --skip-flash ends clean, an unproven park or an interrupt fails the cleanup, and a
    later park resolves that failure."""

    def setUp(self):
        super().setUp()
        self.log = []
        patch(self, hil_test, 'log_line', self.log.append)
        patch(self, hil_test, 'skip_flash', False)
        patch(self, hil_test.usbtest, 'strays_on', lambda uid: [])
        self.park = (0, 'pass', None)
        self.test = (1, 'fail', None)
        patch(self, hil_test, 'test_example',
              lambda board, variant, test: self.park if test == 'device/board_test' else self.result(test))
        self.board = {'name': BOARD, 'uid': 'ABC123', 'flasher': {'name': 'jlink', 'uid': 'P'},
                      'tests': {'only': ['device/cdc_msc']}}

    def result(self, test):
        if isinstance(self.test, BaseException):
            raise self.test
        return self.test

    def events(self, kind):
        return [e for e in hil_lock.cleanup_events(BOARD) if e['event'] == kind]

    def test_a_failed_test_with_a_successful_park_restores(self):
        earlier = hil_lock.log_cleanup_failure(BOARD, 'earlier', detail='killed worker')
        _, err, failed, _, _ = hil_test.test_board(self.board)
        self.assertEqual((err, failed), (1, ['device/cdc_msc']))
        self.assertEqual(self.events('failed'), [earlier])
        self.assertEqual([e['failure'] for e in self.events('restored')], [earlier['id']])
        self.assertEqual(self.events('restored')[0]['evidence'], 'device/board_test park succeeded')
        self.assertEqual(hil_lock.record_state(BOARD)[0], 'empty')

    def test_a_wedge_fails_the_cleanup_under_skip_flash_too(self):
        patch(self, hil_test, 'skip_flash', True)
        self.park = None    # never parked

        def wedging(test):
            hil_test.board_wedged = 'usbtest HUNG'
            return 1, 'fail', None
        patch(self, self, 'result', wedging)
        self.addCleanup(setattr, hil_test, 'board_wedged', '')
        self.assertEqual(hil_test.test_board(self.board)[1], 1)
        failed, = self.events('failed')
        self.assertEqual(failed['detail'], 'board not restored: not parked: the board is wedged: usbtest HUNG')

    def test_skip_flash_ends_clean(self):
        patch(self, hil_test, 'skip_flash', True)
        self.test = (0, 'pass', None)
        self.park = None    # never parked
        self.assertEqual(hil_test.test_board(self.board)[1], 0)
        self.assertEqual(hil_lock.cleanup_events(BOARD), [])

    def test_an_interrupt_fails_the_cleanup_and_a_later_park_resolves_it(self):
        self.test = KeyboardInterrupt()
        with self.assertRaises(KeyboardInterrupt):
            hil_test.test_board(self.board)
        failure, = self.events('failed')
        self.assertEqual((failure['how'], failure['receipts']), ('ended-failed', []))
        self.assertEqual(failure['detail'], 'board battery aborted; then KeyboardInterrupt')
        self.assertIn('lock released', self.log[-1])
        self.test = (0, 'pass', None)
        _, err, _, rows, _ = hil_test.test_board(self.board)
        self.assertEqual(err, 0)
        self.assertIn(failure['id'], rows[0][1][hil_test.hil_report.PREVIOUS_CLEANUP_CELL])
        self.assertEqual(hil_lock.open_cleanup_failures(BOARD), [])

    def test_an_exception_fails_the_cleanup_and_propagates(self):
        self.test = RuntimeError('harness bug')
        with self.assertRaisesRegex(RuntimeError, 'harness bug'):
            hil_test.test_board(self.board)
        failure, = self.events('failed')
        self.assertIn('RuntimeError: harness bug', failure['detail'])

    def test_a_finalization_error_fails_the_board(self):
        self.test = (0, 'pass', None)
        with mock.patch.object(hil_lock, 'clear_record', return_value=False):
            _, err, failed, rows, _ = hil_test.test_board(self.board)
        self.assertEqual((err, failed), (1, []))
        cell = rows[-1][1][hil_test.hil_report.CLEANUP_PENDING_CELL]
        self.assertIn(f'board task ended with errors; {BOARD}: lock released, failure logged in', cell)
        self.assertIn('record not cleared', cell)

    def test_advisory_task_errors_never_fail_the_board(self):
        self.test = (0, 'pass', None)
        broken = types.SimpleNamespace(InterruptHold=mock.Mock(side_effect=OSError('no signal mask')))
        with mock.patch.object(hil_lock, '_process', return_value=broken):
            _, err, _, rows, _ = hil_test.test_board(self.board)
        self.assertEqual(err, 0)
        self.assertNotIn(hil_test.hil_report.CLEANUP_PENDING_CELL, rows[-1][1])

    def test_a_worker_releases_its_board_when_it_exits(self):
        # a process per board: a task left holding its flock (the __exit__-entry gap) frees it when
        # its process ends after that one task; the record it left is logged found-left by the next take
        def gap(board):
            return (board['name'], 0, [], [], _gap_in_worker(BOARD))
        patch(self, hil_test, 'test_board', gap)
        rows = []
        hil_test.run_boards([{'name': 'b'}], time.monotonic() + 30, rows, 'seed')
        self.assertEqual(rows[0][0], 'b')
        self.assertNotEqual(rows[0][4], os.getpid())
        taken = []

        def take():
            try:
                taken.append(hil_lock.take_board_lock(BOARD, 'peer'))
            except hil_lock.BoardLocked:
                return False
            return True
        self.assertTrue(wait_for(take, 20), 'the board process kept the flock')
        fh = taken[0]
        try:
            self.assertEqual([f['how'] for f in fh.cleanup_failures], ['found-left'])
        finally:
            hil_lock.release_board_lock(fh)


if __name__ == '__main__':
    unittest.main()
