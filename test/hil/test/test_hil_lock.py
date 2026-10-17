#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# hil_lock fails closed: an unusable lock dir, lock file or holder record refuses the board,
# contention names its holder, and release never kills a protected or cleanup holder. Every
# lock lives in a temp dir under a name no rig uses. Run directly:
#   python3 test/hil/test/test_hil_lock.py
import fcntl
import io
import json
import os
import resource
import signal
import stat
import subprocess
import sys
import tempfile
import unittest
from contextlib import redirect_stderr, redirect_stdout
from unittest import mock

HIL_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, HIL_DIR)

from usbtest_harness import patch   # noqa: E402 - stubs pyserial before hil_test imports it
import hil_flash         # noqa: E402
import hil_test          # noqa: E402
from helper import hil_lock   # noqa: E402

BOARD = 'hil-lock-selftest-board'

# Another process holding BOARD's flock with the given record bytes ('PID' is replaced by its
# own pid); it prints 'ready' and holds until stdin closes.
HOLDER = r'''
import fcntl, os, sys
fh = open(os.path.join(sys.argv[1], sys.argv[2] + '.lock'), 'ab+')
fcntl.flock(fh, fcntl.LOCK_EX | fcntl.LOCK_NB)
fh.truncate(0)
fh.write(bytes.fromhex(sys.argv[3]).replace(b'PID', str(os.getpid()).encode()))
fh.flush()
print('ready', flush=True)
sys.stdin.read()
'''


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

    def holder(self, record: bytes):
        p = subprocess.Popen([sys.executable, '-c', HOLDER, self.dir, BOARD, record.hex()],
                             stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True)
        self.addCleanup(p.wait)
        self.addCleanup(p.stdin.close)
        self.addCleanup(p.stdout.close)
        self.assertEqual(p.stdout.readline().strip(), 'ready')
        return p

    def record(self, **fields) -> bytes:
        return json.dumps({'pid': 'PID', 'since': '2026-10-17T00:00:00+0000', **fields}
                          ).replace('"PID"', 'PID').encode()

    def lock_dir_is_a_file(self):
        path = os.path.join(self.dir, 'not-a-dir')
        open(path, 'w').close()
        patch(self, hil_lock, 'BOARD_LOCK_DIR', path)

    def acquire(self):
        fh = hil_lock.acquire_board_lock(BOARD, reason='selftest')
        if fh is not None:
            self.addCleanup(fh.close)
        return fh


class FailsClosed(LockDir):
    def assert_unavailable(self):
        with self.assertRaises(hil_lock.LockUnavailable) as cm:
            self.acquire()
        self.assertTrue(str(cm.exception).startswith('board lock unavailable: '), cm.exception)
        self.assertIsInstance(cm.exception.__cause__, OSError)
        return cm.exception

    def test_lock_dir_is_a_regular_file(self):
        self.lock_dir_is_a_file()
        self.assert_unavailable()

    @unittest.skipIf(os.geteuid() == 0, 'root ignores the dir mode')
    def test_unwritable_lock_dir(self):
        os.chmod(self.dir, stat.S_IRUSR | stat.S_IXUSR)
        self.addCleanup(os.chmod, self.dir, stat.S_IRWXU)
        self.assert_unavailable()

    def test_record_write_failing_in_the_buffered_flush_refuses_and_drops_the_flock(self):
        # RLIMIT_FSIZE 0 fails the record at its flush (EFBIG), and close() re-raises it;
        # in a child, so the limit never reaches this process
        r, w = os.pipe()
        pid = os.fork()
        if pid == 0:
            os.close(r)
            try:
                signal.signal(signal.SIGXFSZ, signal.SIG_IGN)
                resource.setrlimit(resource.RLIMIT_FSIZE, (0, 0))
                try:
                    hil_lock.acquire_board_lock(BOARD, reason='selftest')
                    got = 'returned'
                except BaseException as e:
                    got = f'{type(e).__name__}|{type(e.__cause__).__name__}|{e}'
                with open(hil_lock.lock_path(BOARD), 'r+') as fh:
                    fcntl.flock(fh, fcntl.LOCK_EX | fcntl.LOCK_NB)   # raises if still held
                os.write(w, f'{got}|{hil_lock.read_record(BOARD)}'.encode())
            finally:
                os._exit(0)
        os.close(w)
        os.waitpid(pid, 0)
        with os.fdopen(r) as f:
            kind, cause, msg, record = f.read().split('|')
        self.assertEqual((kind, cause), ('LockUnavailable', 'OSError'))
        self.assertTrue(msg.startswith('board lock unavailable: holder record: '), msg)
        self.assertEqual(record, 'None')

    def test_bypass_still_returns_none(self):
        self.lock_dir_is_a_file()
        os.environ['HIL_NO_BOARD_LOCK'] = '1'
        self.assertIsNone(hil_lock.acquire_board_lock(BOARD))


class Contention(LockDir):
    def test_another_process_holds_it(self):
        self.holder(self.record(reason='hil_test.py'))
        with self.assertRaisesRegex(RuntimeError, r'^board locked: .*hil_test\.py') as cm:
            self.acquire()
        self.assertNotIsInstance(cm.exception, hil_lock.LockUnavailable)

    def test_an_unparsable_record_is_an_unknown_holder(self):
        for record in (b'{"pid": PID, "rea', b'\xff\xfe\x00garbage'):
            with self.subTest(record=record):
                proc = self.holder(record)
                with self.assertRaisesRegex(RuntimeError, '^board locked: unknown holder$'):
                    self.acquire()
                proc.stdin.close()
                proc.wait()


CLEANUP = {'reason': 'tinyuf2 cleanup', 'role': 'cleanup-holder', 'board': 'x',
           'detail': 'park failed', 'survivors': []}


class CleanupHolder(LockDir):
    def setUp(self):
        super().setUp()
        self.holder(self.record(**CLEANUP))

    def test_acquire_names_a_recovery_blocker(self):
        with self.assertRaisesRegex(RuntimeError, '^board locked: ') as cm:
            self.acquire()
        self.assertIn(hil_lock.RECOVERY_BLOCKER, str(cm.exception))

    def test_hold_names_a_recovery_blocker(self):
        err = io.StringIO()
        with redirect_stderr(err), redirect_stdout(io.StringIO()):
            rc = hil_lock.cmd_hold([BOARD], 'selftest hold')
        self.assertEqual(rc, 1)
        self.assertIn(hil_lock.RECOVERY_BLOCKER, err.getvalue())

    def test_status_names_a_recovery_blocker(self):
        out = io.StringIO()
        with redirect_stdout(out):
            hil_lock.cmd_status()
        self.assertIn(hil_lock.RECOVERY_BLOCKER, out.getvalue())


class Release(LockDir):
    def setUp(self):
        super().setUp()
        patch(self, hil_lock.time, 'sleep', lambda s: None)

    def test_never_kills_a_protected_or_cleanup_holder(self):
        table = [(CLEANUP, hil_lock.RECOVERY_BLOCKER)]
        table += [({'reason': r}, 'wait for it to finish') for r in sorted(hil_lock.PROTECTED_REASONS)]
        for fields, says in table:
            with self.subTest(reason=fields['reason']):
                proc = self.holder(self.record(**fields))
                ino = os.stat(hil_lock.lock_path(BOARD)).st_ino
                err = io.StringIO()
                with redirect_stderr(err), redirect_stdout(io.StringIO()):
                    rc = hil_lock.cmd_release([BOARD])
                self.assertEqual(rc, 1)
                self.assertIn(says, err.getvalue())
                self.assertIsNone(proc.poll(), f'release killed {fields["reason"]}')
                self.assertEqual(os.stat(hil_lock.lock_path(BOARD)).st_ino, ino)
                self.assertEqual(hil_lock.read_record(BOARD)['reason'], fields['reason'])
                proc.stdin.close()
                proc.wait()

    def test_stale_record_with_a_dead_pid_is_cleared(self):
        dead = subprocess.Popen([sys.executable, '-c', ''])
        dead.wait()
        with open(hil_lock.lock_path(BOARD), 'w') as f:
            json.dump({'pid': dead.pid, 'reason': 'gone'}, f)
        with redirect_stderr(io.StringIO()), redirect_stdout(io.StringIO()):
            rc = hil_lock.cmd_release([BOARD])
        self.assertEqual(rc, 0)
        self.assertTrue(os.path.exists(hil_lock.lock_path(BOARD)), 'release must never unlink')
        self.assertIsNone(hil_lock.read_record(BOARD))


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
        def held(name):
            raise RuntimeError('board locked: {"pid": 1}')
        patch(self, hil_lock, 'acquire_board_lock', held)
        _, err, _, rows, _ = hil_test.test_board(self.board)
        self.assertEqual(err, 1)
        self.assertEqual(rows[0][1], {hil_test.hil_report.LOCKED_CELL: 'fail'})


if __name__ == '__main__':
    unittest.main()
