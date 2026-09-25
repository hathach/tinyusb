#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Unit tests for hil_remote.py's run records: --run-id's started and completion receipts and
# the `wait` that reads them. No rig: run() is stubbed, the processes are local.
# Run directly:
#   python3 test/hil/test/test_hil_remote.py
import contextlib
import importlib.util
import io
import json
import os
import subprocess
import sys
import textwrap
import threading
import time
import unittest
from unittest import mock
from pathlib import Path
from tempfile import TemporaryDirectory

TEST_DIR = Path(__file__).resolve().parent
SCRIPT = TEST_DIR.parents[2] / '.claude/skills/hil/scripts/hil_remote.py'
spec = importlib.util.spec_from_file_location('hil_remote', SCRIPT)
hil_remote = importlib.util.module_from_spec(spec)
spec.loader.exec_module(hil_remote)
for name in ('hil_report', 'hil_args'):
    hil_remote.helper(name)  # imported from the real checkout before a test moves ROOT


class RunRecords(unittest.TestCase):
    def setUp(self):
        tmp = TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        self.root = Path(tmp.name)
        for name, value in (('ROOT', self.root), ('POLL_SECS', 0.05), ('START_SECS', 20)):
            old = getattr(hil_remote, name)
            setattr(hil_remote, name, value)
            self.addCleanup(setattr, hil_remote, name, old)

    def stub_run(self, fn):
        old = hil_remote.run
        hil_remote.run = fn
        self.addCleanup(setattr, hil_remote, 'run', old)

    def wait(self, *argv):
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            rc = hil_remote.main(['wait', *argv])
        return rc, json.loads(out.getvalue())

    def started(self, run_id):
        """A started record nobody holds: its run is gone."""
        started, _ = hil_remote.run_paths(run_id)
        started.parent.mkdir(exist_ok=True)
        hil_remote.write_json(started, {'runId': run_id, 'pid': 1, 'startedAt': 0})

    def launch(self, run_body, run_id):
        """A real background launch in its own process, whose run() is run_body."""
        script = textwrap.dedent(f'''
            import importlib.util, os, signal, sys, time
            from pathlib import Path
            spec = importlib.util.spec_from_file_location('hil_remote', {str(SCRIPT)!r})
            m = importlib.util.module_from_spec(spec); spec.loader.exec_module(m)
            m.ROOT = Path({str(self.root)!r})
            m.run = lambda argv, receipt=None: {run_body}
            sys.exit(m.main(sys.argv[1:]))
        ''')
        p = subprocess.Popen([sys.executable, '-c', script, '--run-id', run_id])
        self.addCleanup(p.wait)
        self.addCleanup(p.kill)
        return p

    def test_a_finished_run_leaves_its_exit_and_reports(self):
        seen = []
        self.stub_run(lambda argv, receipt=None: seen.append(argv) or (2, ['hil_report.md', 'hil_report.json']))
        self.assertEqual(hil_remote.main(['-b', 'pico', '--run-id', 'r1', '-v']), 2)
        self.assertEqual(seen, [['-b', 'pico', '-v']], '--run-id is not passed on to hil_test.py')
        self.assertEqual(self.wait('r1'), (0, {'state': 'done', 'runId': 'r1', 'exit': 2,
                                               'reports': ['hil_report.md', 'hil_report.json']}))

    def test_a_refused_run_records_its_exit_and_no_report(self):
        self.stub_run(lambda argv, receipt=None: hil_remote.fail('no build under cmake-build/'))
        with contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit) as e:
            hil_remote.main(['--run-id=r2'])
        self.assertEqual(e.exception.code, 'error: no build under cmake-build/', 'the refusal still reaches stderr')
        self.assertEqual(self.wait('r2')[1], {'state': 'done', 'runId': 'r2', 'exit': 1, 'reports': []})

    def test_a_run_id_is_used_once(self):
        self.stub_run(lambda argv, receipt=None: (0, []))
        hil_remote.main(['--run-id', 'r3'])
        with self.assertRaises(SystemExit) as e:
            hil_remote.main(['--run-id', 'r3'])
        self.assertIn('was used before', e.exception.code)

    def test_a_live_run_is_still_running_at_the_timeout(self):
        p = self.launch('(time.sleep(30), (0, []))[1]', 'r4')
        rc, status = self.wait('r4', '--timeout', '2')  # the start grace covers the launch
        self.assertEqual((rc, status['state'], status['runId'], status['pid']), (hil_remote.RUNNING, 'running', 'r4', p.pid))

    def test_an_earlier_runs_done_record_does_not_answer_for_this_one(self):
        self.stub_run(lambda argv, receipt=None: (0, ['hil_report.md']))
        hil_remote.main(['--run-id', 'old'])
        self.launch('(time.sleep(30), (0, []))[1]', 'new')
        self.assertEqual(self.wait('new', '--timeout', '1')[1]['state'], 'running')

    def test_a_started_record_nobody_holds_is_dead(self):
        self.started('r5')
        self.assertEqual(self.wait('r5', '--timeout', '5')[0], hil_remote.DEAD)

    def test_a_run_that_ends_while_waited_on_is_done(self):
        """wait blocks across the start and the end: the records land after wait began polling."""
        self.launch("(time.sleep(0.5), (0, ['hil_report.md']))[1]", 'r6')
        self.assertEqual(self.wait('r6', '--timeout', '20'), (0, {'state': 'done', 'runId': 'r6', 'exit': 0, 'reports': ['hil_report.md']}))

    def test_a_wrapper_killed_before_its_done_record_is_dead(self):
        self.launch('os.kill(os.getpid(), signal.SIGKILL)', 'r7').wait()
        rc, status = self.wait('r7', '--timeout', '5')
        self.assertEqual((rc, status['state']), (hil_remote.DEAD, 'dead'))
        self.assertFalse(hil_remote.run_paths('r7')[1].exists())

    def test_a_wait_begun_before_the_launch_wrote_its_record_waits_for_it(self):
        self.stub_run(lambda argv, receipt=None: (0, []))
        timer = threading.Timer(0.1, hil_remote.main, [['--run-id', 'r8']])
        timer.start()
        self.addCleanup(timer.join)
        self.assertEqual(self.wait('r8', '--timeout', '20')[1]['state'], 'done')

    def test_the_start_grace_ends_with_the_timeout(self):
        hil_remote.START_SECS, hil_remote.POLL_SECS = 30, 5
        begun = time.monotonic()
        with self.assertRaises(SystemExit) as e:
            hil_remote.main(['wait', 'never', '--timeout', '0.1'])
        self.assertIn('no run never was started', e.exception.code)
        self.assertLess(time.monotonic() - begun, 1, 'neither the grace nor its poll outlasts --timeout')

    def test_a_timeout_past_the_tool_cap_is_refused(self):
        for bad in ('571', '-1', 'nan', 'inf'):
            with self.subTest(timeout=bad), contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit) as e:
                hil_remote.main(['wait', 'x', '--timeout', bad])
            self.assertEqual(e.exception.code, 2)

    def test_unusable_run_ids_are_refused(self):
        hil_remote.START_SECS = 0.2
        for argv, why in ((['wait', 'nope'], 'no run nope was started'), (['wait', '../x'], 'a run id is'),
                          (['--run-id', '.hidden'], 'a run id is'), (['-v', '--run-id'], '--run-id needs a value'),
                          (['--receipt='], '--receipt needs a value'), (['--run-id', ''], '--run-id needs a value')):
            with self.subTest(argv=argv), self.assertRaises(SystemExit) as e:
                hil_remote.main(argv)
            self.assertIn(why, e.exception.code)


class BuildReceipts(unittest.TestCase):
    CONFIG = {'boards': [{'name': 'a', 'flasher': {'name': 'jlink'}, 'variant': [{'name': 'a'}, {'name': 'a-dma'}]},
                         {'name': 'b', 'flasher': {'name': 'openocd'}}]}

    def setUp(self):
        tmp = TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        self.root = Path(tmp.name)
        old = hil_remote.ROOT
        hil_remote.ROOT = self.root
        self.addCleanup(setattr, hil_remote, 'ROOT', old)
        env = mock.patch.dict(os.environ)
        env.start()
        self.addCleanup(env.stop)
        # a pre-commit hook exports GIT_DIR/GIT_INDEX_FILE: the temp checkout's git must not act on the real one
        for k in [k for k in os.environ if k == 'CONFIG' or k.startswith('GIT_')]:
            del os.environ[k]
        (self.root / 'test/hil').mkdir(parents=True)
        (self.root / 'test/hil/hil_test.py').write_text('')
        (self.root / 'test/hil/tinyusb.json').write_text(json.dumps(self.CONFIG))
        (self.root / '.gitignore').write_text('cmake-build/\n.hil-remote/\n')
        self.git('init', '-q')
        self.commit('base')
        for v in ('a', 'a-dma', 'b'):
            d = self.dir(v)
            d.mkdir(parents=True)
            (d / 'cdc.elf').write_text(v)
            (d / 'flash_args').write_text('--x')
            (d / 'build.ninja').write_text('not staged')
        self.receipt = self.root / '.hil-remote/build.json'  # its dir does not exist before the first run

    def git(self, *argv):
        subprocess.run(['git', '-C', str(self.root), '-c', 'user.name=t', '-c', 'user.email=t@t', *argv], check=True)

    def commit(self, msg):
        self.git('add', '-A')
        self.git('commit', '-q', '--allow-empty', '-m', msg)

    def dir(self, variant):
        return self.root / 'cmake-build' / f'cmake-build-{variant}'

    def check(self, *boards):
        hil_remote.check_receipt(self.receipt, (self.root / 'test/hil/tinyusb.json').resolve(), list(boards),
                                 [d for b in boards for d in hil_remote.variant_dirs(self.CONFIG, 'cmake-build', b)])

    def write(self, *boards):
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            rc = hil_remote.main(['receipt', '--out', str(self.receipt), *(x for b in boards for x in ('-b', b))])
        return rc, json.loads(out.getvalue())

    def refused(self, fn, *argv):
        with contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit) as e:
            fn(*argv)
        return e.exception.code

    def test_a_receipt_pins_every_staged_file_of_every_variant_to_head(self):
        rc, out = self.write('a', 'b')
        self.assertEqual(rc, 0)
        receipt = json.loads(self.receipt.read_text())
        head = subprocess.run(['git', '-C', str(self.root), 'rev-parse', 'HEAD'], capture_output=True, text=True).stdout.strip()
        self.assertEqual((receipt['head'], receipt['boards'], out['files']), (head, ['a', 'b'], 6))
        self.assertEqual(sorted(receipt['files']), sorted(f'cmake-build/cmake-build-{v}/{f}' for v in ('a', 'a-dma', 'b')
                                                           for f in ('cdc.elf', 'flash_args')), 'build.ninja is not staged')
        self.check('a', 'b')
        self.check('b')  # a retry of a subset

    def test_an_unbuilt_variant_gets_no_receipt(self):
        (self.dir('a-dma') / 'cdc.elf').unlink()
        self.assertIn('not built: cmake-build/cmake-build-a-dma', self.refused(self.write, 'a'), 'flash_args alone is no firmware')
        for f in ('flash_args', 'build.ninja'):
            (self.dir('a-dma') / f).unlink()
        self.dir('a-dma').rmdir()
        self.assertIn('not built: cmake-build/cmake-build-a-dma', self.refused(self.write, 'a'))

    def test_a_tree_that_is_not_clean_gets_no_receipt(self):
        (self.root / 'src.h').write_text('untracked source a build could read')
        self.assertIn('the tree is not clean:\n?? src.h', self.refused(self.write, 'b'))
        (self.root / 'src.h').unlink()
        (self.root / 'test/hil/hil_test.py').write_text('changed')
        self.assertIn('M test/hil/hil_test.py', self.refused(self.write, 'b'))

    def test_a_rebuilt_or_removed_artifact_is_refused(self):
        self.write('a', 'b')
        (self.dir('a') / 'cdc.elf').write_text('rebuilt')
        (self.dir('b') / 'flash_args').unlink()
        why = self.refused(self.check, 'a', 'b')
        self.assertIn('2 staged file(s) differ from it, first cmake-build/cmake-build-a/cdc.elf, cmake-build/cmake-build-b/flash_args', why)

    def test_a_moved_head_is_refused(self):
        self.write('b')
        (self.root / 'README').write_text('x')
        self.commit('later')
        self.assertIn('it is for', self.refused(self.check, 'b'))

    def test_a_roster_edited_since_the_build_is_refused(self):
        self.write('a')
        (self.root / 'test/hil/tinyusb.json').write_text(json.dumps({**self.CONFIG, 'note': 'a-dma gained a define'}))
        self.assertIn('is not the one it was built from', self.refused(self.check, 'a'))

    def test_a_board_the_receipt_does_not_cover_is_refused(self):
        self.write('b')
        self.assertIn('it does not cover a', self.refused(self.check, 'a', 'b'))

    def test_a_run_checks_the_receipt_before_touching_the_rig(self):
        self.write('b')
        (self.dir('b') / 'cdc.elf').write_text('rebuilt')
        self.assertIn('staged file(s) differ', self.refused(hil_remote.main, ['--receipt', str(self.receipt), '-b', 'b']))


class CheckBuildReceipt(unittest.TestCase):
    """check_build.py --receipt: the receipt is written by the build that made the firmware."""

    @classmethod
    def setUpClass(cls):
        spec = importlib.util.spec_from_file_location('check_build', TEST_DIR.parents[2] / '.claude/skills/build/scripts/check_build.py')
        cls.cb = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(cls.cb)

    def main(self, *argv, status='', receipt=None, built='ok'):
        written = []
        cb = self.cb
        with mock.patch.object(cb, 'git_status', return_value=status), \
                mock.patch.object(cb, 'build_one', lambda b, *a, **k: {'board': b, 'status': built}), \
                mock.patch.object(cb, 'write_receipt', lambda *a: written.append(a) or (receipt or {'head': 'abc'})), \
                contextlib.redirect_stdout(io.StringIO()) as out, contextlib.redirect_stderr(io.StringIO()):
            try:
                rc = cb.main(['--board', 'raspberry_pi_pico', '--shared', '--variants', 'test/hil/tinyusb.json', *argv])
            except SystemExit as e:
                rc = e.code
        return rc, json.loads(out.getvalue().splitlines()[-1]), written

    def test_a_passing_build_writes_the_receipt_of_its_boards(self):
        rc, out, written = self.main('--receipt', 'r.json')
        self.assertEqual((rc, out['pass'], out['receipt']), (0, True, {'head': 'abc'}))
        self.assertEqual(written, [('r.json', ['raspberry_pi_pico'], 'test/hil/tinyusb.json')])

    def test_no_receipt_without_every_example_or_from_a_dirty_tree(self):
        for argv, status, why in ((['-e', 'device/cdc_msc'], '', 'every example'), ([], ' M src/tusb.c', 'clean tree before the build')):
            rc, out, written = self.main('--receipt', 'r.json', *argv, status=status)
            self.assertEqual((rc, written), (2, []))
            self.assertIn(why, out['error'])

    def test_a_refused_receipt_or_a_failed_build_fails_the_run(self):
        rc, out, _ = self.main('--receipt', 'r.json', receipt={'error': 'the tree is not clean: M hw/bsp/family.json'})
        self.assertEqual((rc, out['pass'], out['receipt']['error'][:18]), (2, False, 'the tree is not cl'))
        rc, out, written = self.main('--receipt', 'r.json', built='failed')
        self.assertEqual((rc, out['pass'], written), (1, False, []))


class CopyBack(unittest.TestCase):
    def test_it_names_what_it_copied(self):
        with TemporaryDirectory() as tmp:
            root = Path(tmp)
            old = hil_remote.ROOT, hil_remote.rsync
            self.addCleanup(lambda: (setattr(hil_remote, 'ROOT', old[0]), setattr(hil_remote, 'rsync', old[1])))
            hil_remote.ROOT = root

            def rsync(guard, *args, optional=False):
                Path(args[-1]).write_text('x')
                return 0
            hil_remote.rsync = rsync
            with contextlib.redirect_stdout(io.StringIO()):
                copied = hil_remote.copy_back('rig', '/tmp/t', 'tok', 'tinyusb.json')
            self.assertEqual(copied, ['hil_report.md', 'hil_report.json', 'tinyusb.json.failed'])


if __name__ == '__main__':
    unittest.main()
