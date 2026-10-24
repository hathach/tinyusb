#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Unit tests for hil_remote.py, the adapter over agentrc's hil_remote_core: --run-id and
# `wait` wired to the core's run records, the build receipts, check_build.py --receipt and the
# report copy-back's wiring. No rig: run() or the core's transport is stubbed. The run records
# themselves are the core's, tested with it (lib/agentrc/tests/test_hil_remote_core.py).
# Run directly:
#   python3 test/hil/test/test_hil_remote.py
import contextlib
import importlib.util
import io
import json
import os
import shutil
import subprocess
import sys
import types
import unittest
from unittest import mock
from pathlib import Path
from tempfile import TemporaryDirectory

TEST_DIR = Path(__file__).resolve().parent
SCRIPT = TEST_DIR.parents[2] / '.claude/skills/hil-contract/scripts/hil_remote.py'
spec = importlib.util.spec_from_file_location('hil_remote', SCRIPT)
hil_remote = importlib.util.module_from_spec(spec)
spec.loader.exec_module(hil_remote)
for name in ('hil_report', 'hil_args'):
    hil_remote.helper(name)  # imported from the real checkout before a test moves ROOT
report = hil_remote.helper('hil_report')
TREE = hil_remote.core.Tree('rig', '/tmp/t', '/tmp/t.lock', 'tok', Path('/nonexistent'))


class RunRecords(unittest.TestCase):
    def setUp(self):
        tmp = TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        self.root = Path(tmp.name)
        old = hil_remote.ROOT
        hil_remote.ROOT = self.root
        self.addCleanup(setattr, hil_remote, 'ROOT', old)

    def stub_run(self, fn):
        old = hil_remote.run
        hil_remote.run = fn
        self.addCleanup(setattr, hil_remote, 'run', old)

    def wait(self, *argv):
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            rc = hil_remote.main(['wait', *argv])
        return rc, json.loads(out.getvalue())

    def test_a_finished_run_leaves_its_exit_and_reports(self):
        seen = []
        self.stub_run(lambda argv, receipt=None: seen.append(argv) or (2, ['hil_report.md', 'hil_report.json']))
        self.assertEqual(hil_remote.main(['-b', 'pico', '--run-id', 'r1', '-v']), 2)
        self.assertEqual(seen, [['-b', 'pico', '-v']], '--run-id is not passed on to hil_test.py')
        self.assertTrue((self.root / hil_remote.RUNS_DIR / 'r1.started.json').is_file(), 'the records live in the checkout')
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
        self.assertIn('was used before (.hil-remote/r3.started.json)', e.exception.code)

    def test_options_and_run_ids_are_checked(self):
        for argv, why in ((['wait', '../x'], 'a run id is'), (['--run-id', '.hidden'], 'a run id is'),
                          (['-v', '--run-id'], '--run-id needs a value'), (['--receipt='], '--receipt needs a value'),
                          (['--run-id', ''], '--run-id needs a value')):
            with self.subTest(argv=argv), self.assertRaises(SystemExit) as e:
                hil_remote.main(argv)
            self.assertIn(why, e.exception.code)
        with contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit) as e:
            hil_remote.main(['wait', 'x', '--timeout', '571'])
        self.assertEqual(e.exception.code, 2, 'one wait ends inside the Bash tool\'s cap')


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
        # tools/build.py as unverified_examples() imports it: cmake has configured cdc alone
        self.registered = {'cdc'}
        self.tools_build = types.SimpleNamespace(cmake_registered_targets=lambda d: self.registered)
        for patch in (mock.patch.dict(sys.modules, {'build': self.tools_build}), mock.patch.object(sys, 'path', list(sys.path))):
            patch.start()
            self.addCleanup(patch.stop)
        (self.root / 'test/hil').mkdir(parents=True)
        (self.root / 'test/hil/hil_test.py').write_text('')
        (self.root / 'test/hil/tinyusb.json').write_text(json.dumps(self.CONFIG))
        (self.root / '.gitignore').write_text('cmake-build/\n.hil-remote/\n')
        self.git('init', '-q')
        self.commit('base')
        for v in ('a', 'a-dma', 'b'):
            d = self.dir(v)
            self.example(v, 'device/cdc').write_text(v)
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

    def example(self, variant, example, suffix='.elf'):
        """The firmware path of example role/name in a variant dir, its dir created."""
        d = self.dir(variant) / example
        d.mkdir(parents=True, exist_ok=True)
        return d / f'{Path(example).name}{suffix}'

    def head(self):
        return subprocess.run(['git', '-C', str(self.root), 'rev-parse', 'HEAD'], capture_output=True, text=True).stdout.strip()

    def check(self, *boards):
        dirs = [d for b in boards for d in hil_remote.variant_dirs(self.CONFIG, 'cmake-build', b)]
        hil_remote.check_receipt(self.receipt, (self.root / 'test/hil/tinyusb.json').resolve(), list(boards),
                                 dirs, hil_remote.staged_files([d for d in dirs if d.is_dir()]))

    def write(self, *boards, head=None):
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            rc = hil_remote.main(['receipt', '--out', str(self.receipt), '--head', head or self.head(),
                                  *(x for b in boards for x in ('-b', b))])
        return rc, json.loads(out.getvalue())

    def refused(self, fn, *argv):
        with contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit) as e:
            fn(*argv)
        return e.exception.code

    def test_a_receipt_pins_every_staged_file_of_every_variant_to_head(self):
        rc, out = self.write('a', 'b')
        self.assertEqual(rc, 0)
        receipt = json.loads(self.receipt.read_text())
        self.assertEqual((receipt['head'], receipt['boards'], out['files']), (self.head(), ['a', 'b'], 6))
        self.assertEqual(sorted(receipt['files']), sorted(f'cmake-build/cmake-build-{v}/{f}' for v in ('a', 'a-dma', 'b')
                                                           for f in ('device/cdc/cdc.elf', 'flash_args')), 'build.ninja is not staged')
        self.check('a', 'b')
        self.check('b')  # a retry of a subset

    def test_an_unbuilt_variant_gets_no_receipt(self):
        self.example('a-dma', 'device/cdc').unlink()
        self.assertIn('not built: cmake-build/cmake-build-a-dma', self.refused(self.write, 'a'), 'flash_args alone is no firmware')
        probe = self.dir('a-dma') / 'CMakeFiles/4.1.2/CMakeDetermineCompilerABI_C.bin'
        probe.parent.mkdir(parents=True)
        probe.write_text('compiler probe')
        self.assertIn('not built: cmake-build/cmake-build-a-dma', self.refused(self.write, 'a'), 'a configure alone is no firmware')
        shutil.rmtree(self.dir('a-dma'))
        self.assertIn('not built: cmake-build/cmake-build-a-dma', self.refused(self.write, 'a'))

    def test_firmware_of_an_example_the_build_does_not_build_gets_no_receipt(self):
        self.example('b', 'device/old').write_text('left by an earlier configure')
        self.assertIn('firmware of an example the build does not build: cmake-build/cmake-build-b/device/old',
                      self.refused(self.write, 'b'))
        self.registered = {'cdc', 'old'}
        self.assertEqual(self.write('b')[0], 0)

    def test_a_dir_cmake_cannot_list_gets_no_receipt(self):
        self.registered = None
        self.assertIn('cmake cannot list the targets of cmake-build/cmake-build-b', self.refused(self.write, 'b'))

    def test_espressif_firmware_is_checked_against_the_examples_its_build_attempts(self):
        (self.root / 'hw/bsp/espressif/boards/b').mkdir(parents=True)
        self.tools_build.get_examples = lambda family: ['device/cdc', 'device/skipped']
        self.tools_build.build_utils = types.SimpleNamespace(skip_example=lambda e, board: e == 'device/skipped')
        self.tools_build.cmake_registered_targets = None  # idf lists no <name>.elf target
        for f in ('bootloader/bootloader.elf', 'partition_table/partition-table.bin'):
            (self.dir('b') / 'device/cdc' / f).parent.mkdir()
            (self.dir('b') / 'device/cdc' / f).write_text('idf')
        self.assertEqual(self.write('b')[0], 0)
        self.example('b', 'device/skipped').write_text('skipped this build')
        self.assertIn('does not build: cmake-build/cmake-build-b/device/skipped', self.refused(self.write, 'b'))

    def test_a_commit_during_the_build_gets_no_receipt(self):
        began = self.head()
        (self.root / 'README').write_text('x')
        self.commit('landed during the build')
        self.assertIn(f'not {began[:12]} the build began at', self.refused(lambda: self.write('b', head=began)))
        self.assertFalse(self.receipt.exists())
        self.assertIn('receipt needs --head', self.refused(hil_remote.main, ['receipt', '--out', str(self.receipt), '-b', 'b']))

    def test_a_tree_that_is_not_clean_gets_no_receipt(self):
        (self.root / 'src.h').write_text('untracked source a build could read')
        self.assertIn('the tree is not clean:\n?? src.h', self.refused(self.write, 'b'))
        (self.root / 'src.h').unlink()
        (self.root / 'test/hil/hil_test.py').write_text('changed')
        self.assertIn('M test/hil/hil_test.py', self.refused(self.write, 'b'))

    def test_an_untracked_file_gets_no_receipt_whatever_status_shows(self):
        self.git('config', 'status.showUntrackedFiles', 'no')
        (self.root / 'src.h').write_text('untracked source a build could read')
        self.assertIn('the tree is not clean:\n?? src.h', self.refused(self.write, 'b'))

    def test_a_rebuilt_or_removed_artifact_is_refused(self):
        self.write('a', 'b')
        self.example('a', 'device/cdc').write_text('rebuilt')
        (self.dir('b') / 'flash_args').unlink()
        why = self.refused(self.check, 'a', 'b')
        self.assertIn('2 staged file(s) differ from it, first cmake-build/cmake-build-a/device/cdc/cdc.elf, '
                      'cmake-build/cmake-build-b/flash_args', why)

    def test_a_variant_dir_removed_since_is_refused(self):
        self.write('a')
        shutil.rmtree(self.dir('a-dma'))
        self.assertIn('2 staged file(s) differ from it, first cmake-build/cmake-build-a-dma/', self.refused(self.check, 'a'))

    def test_a_harness_edited_since_is_refused(self):
        self.write('b')
        (self.root / 'test/hil/hil_test.py').write_text('edited after the build')
        self.assertIn('tracked files changed since HEAD: test/hil/hil_test.py', self.refused(self.check, 'b'))

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
        self.example('b', 'device/cdc').write_text('rebuilt')
        self.assertIn('staged file(s) differ', self.refused(hil_remote.main, ['--receipt', str(self.receipt), '-b', 'b']))

    def test_a_run_stages_the_bytes_the_receipt_checked(self):
        self.write('b')
        pinned = json.loads(self.receipt.read_text())['files']
        sent = {}

        @contextlib.contextmanager
        def lease(remote, remote_dir, root, mkdirs=()):
            self.example('b', 'device/cdc').write_text('rebuilt while the rig was set up')
            yield TREE

        def rsync(tree, *args, optional=False, staging=False):
            if '--prune-empty-dirs' not in args:
                return 0
            for src in (a for a in args if isinstance(a, Path)):   # local files are Paths
                sent.update({f.relative_to(src.parent.parent).as_posix(): hil_remote.core.digest(f) for f in src.rglob('*')
                             if f.suffix in hil_remote.STAGED_SUFFIXES or f.name in hil_remote.STAGED_NAMES})
            return 1  # stop the run once the firmware transfer is seen
        with mock.patch.object(hil_remote.core, 'remote_lease', lease), mock.patch.object(hil_remote.core, 'rsync', rsync), \
                contextlib.redirect_stdout(io.StringIO()):
            self.assertIn('could not stage the firmware', self.refused(hil_remote.main, ['--receipt', str(self.receipt), '-b', 'b']))
        self.assertEqual(sent, pinned)

    def staged_run(self, change, *argv):
        """A run whose lease set-up is overlapped by change(): (the SystemExit code, or None when
        hil_test.py started; the lease ended; the snapshot dirs the firmware transfer sent)."""
        state = {'leased': False, 'sources': []}

        @contextlib.contextmanager
        def lease(remote, remote_dir, root, mkdirs=()):
            change()
            state['leased'] = True
            try:
                yield TREE
            finally:
                state['leased'] = False

        def rsync(tree, *args, optional=False, staging=False):
            if '--prune-empty-dirs' in args:
                state['sources'] += [a for a in args if isinstance(a, Path)]
            return 0

        def run_remote(*a, **k):
            state['started'] = True
            return 0
        with mock.patch.object(hil_remote.core, 'remote_lease', lease), mock.patch.object(hil_remote.core, 'rsync', rsync), \
                mock.patch.object(hil_remote.core, 'run_remote', run_remote), \
                mock.patch.object(hil_remote, 'copy_back', lambda *a: ([], True)), \
                contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
            try:
                hil_remote.main([*argv, '-b', 'b'])
                code = None
            except SystemExit as e:
                code = e.code
        self.assertEqual(code is None, state.get('started', False), 'hil_test.py starts only when the run is not refused')
        return code, state['leased'], state['sources']

    def test_a_harness_or_roster_changed_while_the_rig_is_set_up_is_refused(self):
        def edit(path, text):
            return lambda: (self.root / path).write_text(text)
        for what, change, why in (
                ('harness', edit('test/hil/hil_test.py', 'checked out during the set-up'),
                 'tracked files changed since HEAD: test/hil/hil_test.py'),
                ('roster', edit('test/hil/tinyusb.json', json.dumps({**self.CONFIG, 'note': 'edited during the set-up'})),
                 'is not the one it was built from'),
                ('head', lambda: self.commit('landed during the set-up'), 'it is for')):
            with self.subTest(what):
                self.git('reset', '-q', '--hard', 'HEAD')
                self.write('b')
                run_id = f'set-up-{what}'
                code, leased, sources = self.staged_run(change, '--run-id', run_id, '--receipt', str(self.receipt))
                self.assertIn(why, code or '')
                self.assertIn('what was staged is not what it checked', code)
                self.assertFalse(leased, 'the refusal ends the lease')
                self.assertTrue(sources and not any(s.exists() for s in sources), 'the refusal removes the snapshot')
                done = json.loads(hil_remote.core.run_paths(self.root / hil_remote.RUNS_DIR, run_id)[1].read_text())
                self.assertEqual((done['exit'], done['reports']), (1, []))
                self.git('reset', '-q', '--hard', 'HEAD@{1}' if what == 'head' else 'HEAD')

    def test_a_run_without_a_receipt_stages_the_checkout_as_it_is(self):
        code, _, _ = self.staged_run(lambda: (self.root / 'test/hil/hil_test.py').write_text('a CI artifact run'))
        self.assertIsNone(code)


class CheckBuildReceipt(unittest.TestCase):
    """check_build.py --receipt: the receipt is written by the build that made the firmware."""

    @classmethod
    def setUpClass(cls):
        spec = importlib.util.spec_from_file_location('check_build', TEST_DIR.parents[2] / '.claude/skills/build/scripts/check_build.py')
        cls.cb = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(cls.cb)

    def main(self, *argv, status='', receipt=None, built='ok'):
        written, self.order = [], []
        cb = self.cb
        with mock.patch.object(cb, 'git_status', return_value=status), \
                mock.patch.object(cb, 'git_head', lambda: self.order.append('head') or 'h0'), \
                mock.patch.object(cb, 'build_one', lambda b, *a, **k: self.order.append('build') or {'board': b, 'status': built}), \
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
        self.assertEqual(written, [('r.json', 'h0', ['raspberry_pi_pico'], 'test/hil/tinyusb.json')])
        self.assertEqual(self.order, ['head', 'build'], 'the receipt checks HEAD against the one the build began at')

    def test_no_receipt_without_every_example_or_from_a_dirty_tree(self):
        for argv, status, why in ((['-e', 'device/cdc_msc'], '', 'every example'), ([], ' M src/tusb.c', 'clean tree before the build')):
            rc, out, written = self.main('--receipt', 'r.json', *argv, status=status)
            self.assertEqual((rc, written), (2, []))
            self.assertIn(why, out['error'])

    def test_the_clean_tree_check_sees_untracked_files_whatever_status_shows(self):
        with TemporaryDirectory() as tmp, mock.patch.object(self.cb, 'ROOT', Path(tmp)), mock.patch.dict(os.environ):
            for k in [k for k in os.environ if k.startswith('GIT_')]:
                del os.environ[k]  # a pre-commit hook's GIT_DIR would point git at the real checkout
            subprocess.run(['git', '-C', tmp, 'init', '-q'], check=True)
            subprocess.run(['git', '-C', tmp, 'config', 'status.showUntrackedFiles', 'no'], check=True)
            (Path(tmp) / 'src.h').write_text('untracked source a build could read')
            self.assertEqual(self.cb.git_status(), '?? src.h')

    def test_a_refused_receipt_or_a_failed_build_fails_the_run(self):
        rc, out, _ = self.main('--receipt', 'r.json', receipt={'error': 'the tree is not clean: M hw/bsp/family.json'})
        self.assertEqual((rc, out['pass'], out['receipt']['error'][:18]), (2, False, 'the tree is not cl'))
        rc, out, written = self.main('--receipt', 'r.json', built='failed')
        self.assertEqual((rc, out['pass'], written), (1, False, []))


class CopyBack(unittest.TestCase):
    """The copy-back's decisions over what one fetch brought: the core's transport is stubbed
    by a rig dir whose members it copies, with the outcome a test names per member."""

    def setUp(self):
        tmp = TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        self.local, self.rig = Path(tmp.name) / 'checkout', Path(tmp.name) / 'rig'
        self.local.mkdir()
        self.rig.mkdir()
        self.calls, self.broken = [], {}

    def publish(self, d, aid, cell='pass', previous=False, termination='completed'):
        """One attempt's report in d, as hil_test.py publishes it through the core."""
        c, s = report.core(), report.schema()
        attempt = c.start_attempt(d, s, previous, aid)
        meta = c.make_meta(s, attempt, termination=termination, invocation=['hil_test.py'], selection=['b'],
                           host='rig', roster={'name': 'tinyusb.json', 'sha256': '0' * 64})
        c.publish(d, s, {'rows': [{'board': 'b', 'cells': {'t': cell}, 'duration': '1s'}], 'scope': '',
                         'caveat': ''}, meta)

    def copy_back(self, tree, dest, groups=(), optional=()):
        self.calls.append(([list(g) for g in groups], list(optional)))
        members = [*(m for g in groups for m in g), *optional]
        outcomes = {m: self.broken.get(m) or ('ok' if (self.rig / m).exists() else 'absent') for m in members}

        def put(m):
            src, dst = self.rig / m.rstrip('/'), Path(dest) / m.rstrip('/')
            shutil.copytree(src, dst) if m.endswith('/') else shutil.copyfile(src, dst)
        copied, incomplete = [], []
        for g in groups:
            if all(outcomes[m] == 'ok' for m in g):
                for m in g:
                    put(m)
                copied += g
            else:
                incomplete.append(list(g))
        for m in optional:
            if outcomes[m] == 'ok':
                put(m)
                copied.append(m)
        return hil_remote.core.CopyBack(copied, incomplete, outcomes)

    def collect(self):
        err = io.StringIO()
        with mock.patch.object(hil_remote, 'ROOT', self.local), mock.patch.object(hil_remote.core, 'copy_back', self.copy_back), \
                contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(err):
            copied, ok = hil_remote.copy_back(TREE, 'tinyusb.json')
        return copied, ok, err.getvalue()

    def reading(self, d=None):
        return report.core().read_document((d or self.local) / report.REPORT_JSON, report.schema())

    def kept(self):
        return sorted((self.local / hil_remote.RUNS_DIR).glob('collect-*'))

    def retry_on_the_rig(self):
        """The rig accumulates onto the uploaded local report, as an --accumulate run does."""
        self.publish(self.local, 'a1', 'fail')
        shutil.copyfile(self.local / report.REPORT_JSON, self.rig / report.REPORT_JSON)
        self.publish(self.rig, 'a2', 'pass', previous=True)
        (self.rig / 'tinyusb.json.failed').write_text('--accumulate -b b')

    def test_one_fetch_brings_the_pair_the_records_and_the_spec(self):
        self.retry_on_the_rig()
        copied, ok, err = self.collect()
        self.assertEqual((copied, ok, err), (['tinyusb.json.failed', 'hil_report.json', 'hil_report.md'], True, ''))
        self.assertEqual(self.calls, [([['hil_report.md', 'hil_report.json']],
                                       ['tinyusb.json.failed', f'{report.REPORT_ATTEMPTS}/'])])
        self.assertEqual([c.attempt_id for c in report.core().read_chain(self.local, report.schema())], ['a1', 'a2'])
        self.assertEqual(self.kept(), [], 'nothing to keep when everything came home')

    def test_a_report_that_did_not_all_arrive_withdraws_the_local_one(self):
        self.retry_on_the_rig()
        self.broken = {'hil_report.json': 'transport'}
        (self.rig / 'tinyusb.json.failed').unlink()
        (self.local / 'tinyusb.json.failed').write_text('stale')
        copied, ok, err = self.collect()
        self.assertEqual((copied, ok), ([], False))
        self.assertIn('error: report copy-back: hil_report.json transport', err)
        r = self.reading()
        self.assertEqual(r.outcome, 'withdrawn', 'the previous run\'s report is never left as this run\'s')
        self.assertEqual(r.body['archived'], 'a1')
        self.assertTrue((self.local / report.REPORT_MD).read_text().startswith(report.core().NOTICE))
        self.assertTrue((self.local / report.REPORT_ATTEMPTS / 'a1' / report.REPORT_JSON).is_file(), 'the old one is kept')
        self.assertFalse((self.local / 'tinyusb.json.failed').exists(), 'no stale re-run spec')
        self.assertEqual(len(self.kept()), 1, 'what did arrive is kept')

    def test_a_local_report_that_cannot_be_archived_is_left_alone_and_never_prepared(self):
        self.retry_on_the_rig()
        held = self.local / report.REPORT_ATTEMPTS / 'a1'
        held.mkdir(parents=True)
        (held / report.REPORT_JSON).write_text('{"a different": "record"}')
        (self.local / report.REPORT_MD).write_text('the local table')
        before = (self.local / report.REPORT_JSON).read_bytes()
        copied, ok, err = self.collect()
        self.assertEqual((copied, ok), (['tinyusb.json.failed'], False))
        self.assertIn('cannot be archived', err)
        self.assertEqual((self.local / report.REPORT_JSON).read_bytes(), before)
        self.assertEqual((self.local / report.REPORT_MD).read_text(), 'the local table', 'never prepared as this run\'s')
        kept, = self.kept()
        self.assertEqual(self.reading(kept).meta['attempt_id'], 'a2', 'the rig\'s report is kept apart')

    def test_an_unrelated_conflicting_record_is_named_but_does_not_stop_the_report(self):
        self.retry_on_the_rig()
        self.publish(self.local, 'old', 'pass')   # archives a1 locally; local current is old
        other = self.rig / report.REPORT_ATTEMPTS / 'old'
        other.mkdir()
        (other / report.REPORT_JSON).write_text('{"rows": [], "scope": "", "caveat": "other"}')
        copied, ok, err = self.collect()
        self.assertTrue(ok)
        self.assertIn('old on the rig differ', err)
        self.assertEqual(self.reading().meta['attempt_id'], 'a2')
        self.assertEqual(len(self.kept()), 1, 'the rig\'s differing record is kept apart')

    def test_a_collection_withdrawal_names_the_copy_back(self):
        self.retry_on_the_rig()
        self.broken = {'hil_report.json': 'transport'}
        self.collect()
        self.assertTrue(self.reading().body['reason'].startswith('report copy-back failed: the rig report did not all arrive'))

    def test_a_failed_spec_fetch_is_reported(self):
        self.retry_on_the_rig()
        self.broken = {'tinyusb.json.failed': 'truncated'}
        copied, ok, err = self.collect()
        self.assertFalse(ok)
        self.assertIn('tinyusb.json.failed truncated', err)
        self.assertNotIn('tinyusb.json.failed', copied)

    def test_a_run_whose_report_did_not_come_home_never_exits_0(self):
        for rc, came_home, want in ((0, False, 1), (0, True, 0), (3, False, 3)):
            with self.subTest(rc=rc, came_home=came_home), \
                    mock.patch.object(hil_remote.core, 'stage_files'), \
                    mock.patch.object(hil_remote.core, 'rsync', return_value=0), \
                    mock.patch.object(hil_remote.core, 'run_remote', return_value=rc), \
                    mock.patch.object(hil_remote, 'copy_back', return_value=(['x'], came_home)), \
                    contextlib.redirect_stdout(io.StringIO()):
                args = types.SimpleNamespace(accumulate=False, build_dir='cmake-build')
                got = hil_remote.stage_and_run(TREE, [], args, {}, Path('tinyusb.json'), [])
                self.assertEqual(got, (want, ['x']))


if __name__ == '__main__':
    unittest.main()
