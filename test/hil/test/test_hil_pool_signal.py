#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# hil_test.run_boards, the dispatch main() runs: one process per board, the pool guard keeping what
# finished, and a signalled or killed run never leaving a board flashing and testing under its
# lock -- the run's SIGTERM, a terminal's group SIGINT and the run's SIGKILL each end the board
# through its interrupt path, once. test_board is stood in for (fork carries the patch into each
# board process); the signal cases run in a run of their own session.
# Run directly:
#   python3 test/hil/test/test_hil_pool_signal.py
import json
import os
import signal
import subprocess
import sys
import time
import unittest
from pathlib import Path
from tempfile import TemporaryDirectory

TEST_DIR = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(TEST_DIR))
from usbtest_harness import patch, wait_for   # noqa: E402 - stubs pyserial before hil_test imports it
import hil_test  # noqa: E402
from helper import hil_report  # noqa: E402

RUN = r'''
import fcntl, os, signal, sys, time
sys.path[:0] = [sys.argv[2], os.path.dirname(sys.argv[2])]
import usbtest_harness  # noqa: F401
import hil_test
out, sigint, window = sys.argv[1], sys.argv[3], sys.argv[4]
cleanup = float(sys.argv[5])
hil_test.STOP_GRACE_S = float(sys.argv[6])

def mark(name, text=''):
    with open(os.path.join(out, name), 'a') as f:
        f.write(text)

def read(name):
    path = os.path.join(out, name)
    return open(path).read() if os.path.exists(path) else ''

def task(board):
    lock = open(os.path.join(out, 'board.lock'), 'w')
    try:
        fcntl.flock(lock, fcntl.LOCK_EX)
        mark('worker', str(os.getpid()))
        time.sleep(30)
    finally:
        time.sleep(cleanup)   # the run's SIGTERM lands here; raised, it would lose the lines below
        mark('cleaned', 'x')
        lock.close()

if window == 'child':     # the board process held between its fork and its handlers' unblock
    hil_test._die_with_parent = lambda: (mark('forked', '1'), time.sleep(1))
if window == 'parent':    # the run held between a board's fork and its enrolment in `live`
    start = hil_test._BoardProcess.__init__
    def enrolling(self, *args):
        start(self, *args)
        mark('forked', '1')
        time.sleep(1)
    hil_test._BoardProcess.__init__ = enrolling

# a terminal run's SIGINT handler, or a background run's inherited SIG_IGN
signal.signal(signal.SIGINT, signal.SIG_IGN if sigint == 'ignore' else signal.default_int_handler)
hil_test.test_board = task
rows = []
with hil_test._sigterm_as_interrupt():
    try:
        hil_test.run_boards([{'name': 'b'}], time.monotonic() + 60, rows, 'seed')
    except KeyboardInterrupt as e:   # main's partial report, after the stop
        code = 128 + getattr(e, 'signum', signal.SIGINT)
        mark('reported', repr(rows) + ' ' + read('cleaned'))
        hil_test._exit_past_left_boards(code)
        sys.exit(code)
'''

# main() itself over two fake boards: 'done' finishes while 'cut' is mid-battery, and cut's cleanup
# outlasts the stop grace. The run's SIGTERM lands as done's row is taken ('taken'), or the test sends
# it while done's process is stopped after writing only its result's header ('partial').
MAIN = r'''
import json, os, signal, struct, sys, time
from pathlib import Path
sys.path[:0] = [sys.argv[2], os.path.dirname(sys.argv[2])]
import usbtest_harness  # noqa: F401
import hil_test
from helper import hil_lock
out, mode = Path(sys.argv[1]), sys.argv[3]
hil_lock.BOARD_LOCK_DIR = out / 'locks'
hil_test.STOP_GRACE_S = 0.3
hil_test.hil_flash.probe_busy = lambda board: []
hil_test._load_controller_hints = lambda: {}
hil_test._save_controller_hints = lambda *a: None
signal.signal(signal.SIGINT, signal.default_int_handler)

def battery(board, parked):
    if board['name'] == 'cut':
        (out / 'ready').touch()
        time.sleep(30)
    while not (out / 'ready').exists():
        time.sleep(0.01)
    parked.append(board['name'])
    if mode == 'partial':
        write = os.write
        def header_then_stop(fd, data):
            if bytes(data[:8]) == struct.pack('!Q', len(data) - 8):   # the result frame
                os.write = write
                n = write(fd, data[:8])
                (out / 'paused').write_text(str(os.getpid()))
                os.kill(os.getpid(), signal.SIGSTOP)
                return n
            return write(fd, data)
        os.write = header_then_stop
    return 0, [], False, [(board['name'], {'device/cdc_msc': 'pass', 'log': 'x' * 200000}, '1s')], ''
hil_test._board_battery = battery
verdict = hil_test._give_verdict
def slow_verdict(task, board, *args):
    if board['name'] == 'cut':
        time.sleep(2)
    verdict(task, board, *args)
    if board['name'] == 'cut':
        (out / 'cleaned').touch()
hil_test._give_verdict = slow_verdict

def sigterm_as_the_row_is_taken(frame, event, arg):
    taking = frame.f_code is hil_test._BoardProcess.ended.__code__ and frame.f_locals['self']
    # the row read and its pipe closed: the step an interrupt must not tear
    if event == 'line' and taking and taking.result is not None:
        sys.settrace(None)
        (out / 'signalled').write_text(str(time.monotonic()))
        os.kill(os.getpid(), signal.SIGTERM)
    return sigterm_as_the_row_is_taken
if mode == 'taken':
    sys.settrace(sigterm_as_the_row_is_taken)

cfg = {'boards': [{'name': n, 'uid': str(i), 'flasher': {'name': 'jlink', 'uid': 'fake-' + n}}
                  for i, n in enumerate(('done', 'cut'))]}
(out / 'rig.json').write_text(json.dumps(cfg))
os.environ['HIL_REPORT_DIR'] = str(out)
sys.argv = ['hil_test.py', str(out / 'rig.json')]
hil_test.main()
'''


def alive(pid: int) -> bool:
    try:
        return Path(f'/proc/{pid}/stat').read_text().rsplit(')', 1)[1].split()[0] != 'Z'
    except (FileNotFoundError, ProcessLookupError):
        return False


def group_left(pgid: int) -> list:
    """The live processes of process group `pgid`."""
    left = []
    for stat in Path('/proc').glob('[0-9]*/stat'):
        try:
            fields = stat.read_text().rsplit(')', 1)[1].split()
        except (FileNotFoundError, ProcessLookupError):
            continue
        if fields[0] != 'Z' and int(fields[2]) == pgid:
            left.append(int(stat.parent.name))
    return left


def lock_free(path: Path) -> bool:
    import fcntl
    with open(path, 'w') as f:
        try:
            fcntl.flock(f, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError:
            return False
    return True


class FakeBoards(unittest.TestCase):
    """run_boards over `task` standing in for test_board, `width` boards at once."""

    def run_boards(self, boards, deadline, task, width=4):
        patch(self, hil_test, 'test_board', task)
        patch(self, hil_test, 'log_line', lambda msg: None)
        patch(self, os, 'cpu_count', lambda: width)
        self.rows = []
        hil_test.run_boards(boards, deadline, self.rows, 'seed')
        return self.rows


class PoolGuardKeepsWhatFinished(FakeBoards):
    """The guard's 30-minute predecessor fired on 5 of the last 8 HIL jobs, so this is the
    common failure, not an edge case: map_async discarded every board that had finished and
    left the re-run spec unwritten, so CI re-tested all ~26 to find the one that wedged."""

    def setUp(self):
        td = TemporaryDirectory()
        self.addCleanup(td.cleanup)
        self.started = Path(td.name)

    def task(self, board):
        (self.started / board['name']).touch()
        if board['name'] == 'wedged':
            time.sleep(60)
        if board['name'] == 'raises':
            raise ValueError('no per-test handler took this')
        return (board['name'], 0, [], [], None)

    def boards(self, *names):
        return [{'name': n} for n in names]

    def started_boards(self) -> list:
        return sorted(f.name for f in self.started.iterdir())

    def test_finished_rows_survive_a_guard_expiry(self):
        with self.assertRaises(hil_test.PoolGuardExpired):
            self.run_boards(self.boards('fast1', 'fast2', 'wedged'), time.monotonic() + 1, self.task)
        self.assertEqual(sorted(r[0] for r in self.rows), ['fast1', 'fast2'])

    def test_an_expired_deadline_starts_nothing(self):
        with self.assertRaises(hil_test.PoolGuardExpired):
            self.run_boards(self.boards('a', 'b'), time.monotonic() - 1, self.task)     # already past
        self.assertEqual((self.rows, self.started_boards()), ([], []))

    def test_a_board_that_raises_aborts_the_run_with_the_finished_rows_kept(self):
        with self.assertRaisesRegex(ValueError, 'no per-test handler'):
            self.run_boards(self.boards('a', 'raises', 'wedged'), time.monotonic() + 30, self.task, width=1)
        self.assertEqual([r[0] for r in self.rows], ['a'])
        self.assertEqual(self.started_boards(), ['a', 'raises'], 'boards start in config order, one at a time')

    def test_every_board_finishing_returns_them_all_in_order(self):
        rows = self.run_boards(self.boards('a', 'b', 'c'), time.monotonic() + 30, self.task, width=1)
        self.assertEqual([r[0] for r in rows], ['a', 'b', 'c'])


class ABoardResult(FakeBoards):
    def test_larger_than_the_pipe_buffer_arrives_whole(self):
        row = ('big', 0, [], [('big', {'log': 'x' * 300000}, None)], None)
        self.assertEqual(self.run_boards([{'name': 'big'}], time.monotonic() + 30, lambda board: row), [row])


class ABoardProcessThatDies(FakeBoards):
    def test_is_a_failed_row_at_once_and_the_run_goes_on(self):
        def task(board):
            if board['name'] == 'dies':
                os.kill(os.getpid(), signal.SIGKILL)
            return (board['name'], 0, [], [], None)
        board = {'name': 'dies', 'variant': [{'name': 'dies-a'}, {'name': 'dies-b'}]}
        started = time.monotonic()
        rows = self.run_boards([board, {'name': 'next'}], time.monotonic() + 60, task)
        self.assertLess(time.monotonic() - started, 10, 'waited for a row that never comes')
        died = {r[0]: r for r in rows}['dies']
        self.assertEqual(died[1], 1)
        cell = f'{hil_report.REPORT_CELL["fail"]} board process died (killed by SIGKILL)'
        self.assertEqual(died[3], [('dies-a', {hil_report.RUN_ABORTED_CELL: cell}, None),
                                   ('dies-b', {hil_report.RUN_ABORTED_CELL: cell}, None)])
        self.assertEqual(sorted(r[0] for r in rows), ['dies', 'next'])


@unittest.skipUnless(sys.platform.startswith('linux'), 'Linux PR_SET_PDEATHSIG and /proc')
class BoardProcessSignals(unittest.TestCase):
    def start(self, sigint='default', window='', cleanup=0.15, grace=60, marker='worker'):
        tmp = TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        self.out = Path(tmp.name)
        script = self.out / 'run.py'
        script.write_text(RUN)
        run = subprocess.Popen([sys.executable, str(script), str(self.out), TEST_DIR, sigint, window,
                                str(cleanup), str(grace)],
                               start_new_session=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        self.addCleanup(self._kill_group, run)
        self.assertTrue(wait_for(lambda: self.read(marker)), f'no {marker} marker')
        return run

    @staticmethod
    def _kill_group(run):
        try:
            os.killpg(run.pid, signal.SIGKILL)
        except ProcessLookupError:
            pass
        run.wait()

    def read(self, name) -> str:
        path = self.out / name
        return path.read_text() if path.exists() else ''

    def assert_ended_once(self, run, rc=None, reported='[] x'):
        self.assertTrue(wait_for(lambda: run.poll() is not None), 'the run hung')
        if rc is not None:
            self.assertEqual(run.returncode, rc)
        self.assertTrue(wait_for(lambda: not group_left(run.pid)), f'processes outlived the run: {group_left(run.pid)}')
        self.assertEqual(self.read('cleaned'), 'x' if self.read('worker') else '')
        self.assertEqual(self.read('reported'), reported, 'reported before its board stopped')
        self.assertTrue(lock_free(self.out / 'board.lock'))

    def test_the_runs_sigterm_runs_the_boards_interrupt_path(self):
        run = self.start()
        os.kill(run.pid, signal.SIGTERM)
        self.assert_ended_once(run, rc=143)

    def test_a_terminals_group_sigint_runs_the_boards_cleanup_once(self):
        run = self.start()
        os.killpg(run.pid, signal.SIGINT)
        self.assert_ended_once(run, rc=130)

    def test_a_sigkilled_runs_board_process_exits_through_its_interrupt_path(self):
        run = self.start()
        os.kill(run.pid, signal.SIGKILL)
        self.assert_ended_once(run, rc=-signal.SIGKILL, reported='')

    def test_a_run_started_with_sigint_ignored_keeps_its_board_ignoring_it(self):
        run = self.start(sigint='ignore')
        os.killpg(run.pid, signal.SIGINT)
        time.sleep(0.3)
        self.assertEqual((run.poll(), self.read('cleaned')), (None, ''))
        os.kill(run.pid, signal.SIGTERM)
        self.assert_ended_once(run, rc=143)

    def test_a_sigterm_as_a_board_is_enrolled_still_stops_it_once_before_the_report(self):
        run = self.start(window='parent', marker='forked')
        self.assertTrue(wait_for(lambda: self.read('worker')), 'the board never started')
        os.kill(run.pid, signal.SIGTERM)     # the run still blocks it: the board is not in `live` yet
        self.assert_ended_once(run, rc=143)

    def test_a_sigterm_before_a_board_process_unblocks_ends_it_before_its_board(self):
        run = self.start(window='child', marker='forked')
        os.kill(run.pid, signal.SIGTERM)     # the run's stop SIGTERMs the board process, which still blocks it
        self.assert_ended_once(run, rc=143, reported='[] ')
        self.assertEqual(self.read('worker'), '', 'the board started after its stop')

    def test_a_board_past_its_stop_grace_never_holds_up_the_runs_exit(self):
        run = self.start(cleanup=3, grace=0.5)
        os.kill(run.pid, signal.SIGTERM)
        signalled = time.monotonic()
        self.assertTrue(wait_for(lambda: run.poll() is not None, 2.5), 'the run waited on its board')
        self.assertLess(time.monotonic() - signalled, 2.5)
        self.assertEqual((run.returncode, self.read('reported'), self.read('cleaned')), (143, '[] ', ''))
        self.assertFalse(lock_free(self.out / 'board.lock'), 'the board was killed in its cleanup')
        self.assertTrue(wait_for(lambda: self.read('cleaned') == 'x', 5), 'the board never finished its cleanup')
        self.assertTrue(wait_for(lambda: lock_free(self.out / 'board.lock'), 5))
        self.assertTrue(wait_for(lambda: not group_left(run.pid)), f'processes outlived the board: {group_left(run.pid)}')


@unittest.skipUnless(sys.platform.startswith('linux'), 'Linux /proc')
class AnInterruptedMain(unittest.TestCase):
    def main(self, mode):
        tmp = TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        self.out = out = Path(tmp.name)
        script = out / 'main.py'
        script.write_text(MAIN)
        run = subprocess.Popen([sys.executable, str(script), str(out), TEST_DIR, mode],
                               start_new_session=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        self.addCleanup(BoardProcessSignals._kill_group, run)
        return run

    def assert_reported(self, run, rows, rerun):
        out = self.out
        self.assertTrue(wait_for(lambda: run.poll() is not None), 'the run hung')
        exited = time.monotonic()
        self.assertEqual(run.returncode, 143)
        self.assertLess(exited - float((out / 'signalled').read_text()), 1.5, 'the run waited on a board')
        doc = json.loads((out / hil_report.REPORT_JSON).read_text())
        self.assertEqual([r['board'] for r in doc['rows']], rows)
        self.assertIn('interrupted by SIGTERM', doc['caveat'])
        self.assertEqual((out / 'rig.json.failed').read_text(), rerun)
        self.assertFalse((out / 'cleaned').exists())

    def assert_boards_end_on_their_own(self, run):
        self.assertTrue(wait_for(lambda: (self.out / 'cleaned').exists(), 5), 'the board never finished its cleanup')
        self.assertTrue(wait_for(lambda: not group_left(run.pid)), f'processes outlived the run: {group_left(run.pid)}')

    def test_a_sigterm_as_a_row_is_taken_keeps_it_and_exits_past_the_cleaning_board(self):
        run = self.main('taken')
        self.assert_reported(run, ['done'], '--accumulate -b cut')
        self.assert_boards_end_on_their_own(run)

    def test_a_board_stopped_mid_result_never_holds_up_the_runs_report(self):
        run = self.main('partial')
        self.assertTrue(wait_for(lambda: (self.out / 'paused').exists()), 'the board never wrote its header')
        (self.out / 'signalled').write_text(str(time.monotonic()))
        os.kill(run.pid, signal.SIGTERM)
        self.assert_reported(run, [], '--accumulate -b done -b cut')
        os.kill(int((self.out / 'paused').read_text()), signal.SIGCONT)
        self.assert_boards_end_on_their_own(run)


class SigtermAsInterrupt(unittest.TestCase):
    def test_the_runs_sigterm_unwinds_with_its_signal_and_its_handler_goes_with_the_run(self):
        before = signal.getsignal(signal.SIGTERM)
        with self.assertRaises(hil_test.RunInterrupted) as cm, hil_test._sigterm_as_interrupt():
            signal.raise_signal(signal.SIGTERM)
        self.assertEqual(cm.exception.signum, signal.SIGTERM)
        self.assertIs(signal.getsignal(signal.SIGTERM), before)


if __name__ == '__main__':
    unittest.main()
