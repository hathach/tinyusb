#!/usr/bin/env python3
"""Unit tests for tools/metrics_compare_base.py's generate_membrowse_sizes() guard.

Exercises the "elfs found but no symbols matched filters" failure mode with
glob.glob and membrowse_compare.report_for_elf monkeypatched, so no build and
no real membrowse CLI invocation is needed.
"""
import contextlib
import io
import os
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), '../../..'))
sys.path.insert(0, os.path.join(REPO, 'tools'))
import membrowse_compare  # noqa: E402
import metrics_compare_base as mcb  # noqa: E402


def _run_capturing_stdout(*args, **kwargs):
    buf = io.StringIO()
    with contextlib.redirect_stdout(buf):
        result = mcb.generate_membrowse_sizes(*args, **kwargs)
    return result, buf.getvalue()


class GenerateMembrowseSizes(unittest.TestCase):
    def test_no_elfs_errors_and_returns_none(self):
        with mock.patch('glob.glob', return_value=[]):
            result, out = _run_capturing_stdout('/fake/build', ['/fake/src/'])
        self.assertIsNone(result)
        self.assertIn('no .elf files', out)

    def test_elfs_found_but_nothing_matches_filters_errors_and_returns_none(self):
        # a filter typo, or a membrowse report-shape change breaking per_file_sizes()
        # matching: must not silently produce an empty/degenerate table
        fake_report = {'symbols': [
            {'name': 'x', 'size': 4, 'section': '.text',
             'object_file': 'build/x.c.obj', 'source_file': 'x.c'},
        ]}
        with mock.patch('glob.glob', return_value=['/fake/build/ex/ex.elf']), \
             mock.patch.object(membrowse_compare, 'report_for_elf', return_value=fake_report):
            result, out = _run_capturing_stdout('/fake/build', ['/no/such/prefix/'])
        self.assertIsNone(result)
        self.assertIn('no symbols matched filters', out)
        self.assertIn('--engine linkermap', out)

    def test_membrowse_cli_missing_errors_and_returns_none(self):
        # subprocess.run(['membrowse', ...]) raises FileNotFoundError when the
        # CLI isn't installed - must not surface as a bare traceback after the
        # base+branch builds already ran (minutes of work).
        with mock.patch('glob.glob', return_value=['/fake/build/ex/ex.elf']), \
             mock.patch.object(membrowse_compare, 'report_for_elf',
                                side_effect=FileNotFoundError('membrowse')):
            result, out = _run_capturing_stdout('/fake/build', ['/fake/src/'])
        self.assertIsNone(result)
        self.assertIn('pip install membrowse', out)
        self.assertIn('--engine linkermap', out)

    def test_membrowse_report_failure_errors_and_returns_none(self):
        # report_for_elf() raises RuntimeError when `membrowse report` itself
        # exits non-zero - must not surface as a bare traceback after the
        # base+branch builds already ran (minutes of work).
        with mock.patch('glob.glob', return_value=['/fake/build/ex/ex.elf']), \
             mock.patch.object(membrowse_compare, 'report_for_elf',
                                side_effect=RuntimeError('membrowse report failed for /fake/build/ex/ex.elf: boom')):
            result, out = _run_capturing_stdout('/fake/build', ['/fake/src/'])
        self.assertIsNone(result)
        self.assertIn('membrowse report failed', out)

    def test_elfs_found_and_filters_match_returns_sizes(self):
        fake_report = {'symbols': [
            {'name': 'x', 'size': 4, 'section': '.text',
             'object_file': 'build/x.c.obj', 'source_file': 'x.c'},
        ]}
        with mock.patch('glob.glob', return_value=['/fake/build/ex/ex.elf']), \
             mock.patch.object(membrowse_compare, 'report_for_elf', return_value=fake_report):
            result = mcb.generate_membrowse_sizes('/fake/build', ['build/'])
        self.assertIsNotNone(result)
        self.assertIn('x.c', result)
        self.assertEqual(result['x.c']['flash'], 4)

    def test_all_examples_scope_averages_a_file_shared_across_elfs(self):
        # No `example` arg (the -b-with-no-e, all-examples scope): a file linked
        # by more than one elf must be AVERAGED across them, matching
        # metrics.py's compute_avg() semantics from the legacy linkermap engine
        # this replaces - not summed, or a file linked by N examples would
        # report ~N times its real size and Flash/RAM wouldn't be a real
        # binary's size any more.
        report_a = {'symbols': [
            {'name': 'x', 'size': 100, 'section': '.text',
             'object_file': 'build/x.c.obj', 'source_file': 'x.c'},
        ]}
        report_b = {'symbols': [
            {'name': 'x', 'size': 200, 'section': '.text',
             'object_file': 'build/x.c.obj', 'source_file': 'x.c'},
        ]}
        with mock.patch('glob.glob', return_value=['/fake/build/ex1/ex1.elf',
                                                    '/fake/build/ex2/ex2.elf']), \
             mock.patch.object(membrowse_compare, 'report_for_elf',
                                side_effect=[report_a, report_b]):
            result = mcb.generate_membrowse_sizes('/fake/build', ['build/'])
        self.assertIsNotNone(result)
        self.assertEqual(result['x.c']['flash'], 150)  # (100+200)/2, not 300

    def test_single_example_scope_sums_across_its_own_elfs(self):
        # `example` given (-e): must stay byte-identical to before averaging was
        # added - an example with more than one of its own elf (e.g. app +
        # bootloader) sums, it does not average.
        report_a = {'symbols': [
            {'name': 'x', 'size': 100, 'section': '.text',
             'object_file': 'build/x.c.obj', 'source_file': 'x.c'},
        ]}
        report_b = {'symbols': [
            {'name': 'x', 'size': 200, 'section': '.text',
             'object_file': 'build/x.c.obj', 'source_file': 'x.c'},
        ]}
        with mock.patch('glob.glob', return_value=['/fake/build/ex/app.elf',
                                                    '/fake/build/ex/loader.elf']), \
             mock.patch.object(membrowse_compare, 'report_for_elf',
                                side_effect=[report_a, report_b]):
            result = mcb.generate_membrowse_sizes('/fake/build', ['build/'], example='ex')
        self.assertIsNotNone(result)
        self.assertEqual(result['x.c']['flash'], 300)  # sum, not average


class BuildBoardLinkermap(unittest.TestCase):
    def _build(self, tmp, with_map):
        build_dir = os.path.join(tmp, 'build')
        map_dir = os.path.join(build_dir, 'device', 'x')
        ok = subprocess.CompletedProcess([], 0, '', '')
        failed = subprocess.CompletedProcess([], 1, '', 'no such target')

        def run(cmd, **_kwargs):
            # the plain build (a base tree's POST_BUILD hook) writes map.json
            if with_map and '--target' in cmd and cmd[-1] == 'x':
                os.makedirs(map_dir)
                open(os.path.join(map_dir, 'x.map.json'), 'w').close()
            return failed if cmd[-1] == 'x-linkermap' else ok

        with mock.patch.object(mcb, 'run', side_effect=run):
            return mcb.build_board(tmp, build_dir, 'b', 'device/x', linkermap=True)

    def test_failed_target_without_map_fails(self):
        with tempfile.TemporaryDirectory() as tmp:
            self.assertFalse(self._build(tmp, with_map=False))

    def test_failed_target_accepts_map_from_plain_build(self):
        with tempfile.TemporaryDirectory() as tmp:
            self.assertTrue(self._build(tmp, with_map=True))

    def test_map_lookup_survives_glob_metachars_in_the_path(self):
        # a checkout at .../pr[1]/... must not report the linkermap target fatal
        # when the map.json is right there
        with tempfile.TemporaryDirectory() as tmp:
            bracketed = os.path.join(tmp, 'pr[1]')
            os.makedirs(bracketed)
            self.assertTrue(self._build(bracketed, with_map=True))


class MainFailure(unittest.TestCase):
    def _main(self, tmp, build_board=True, sizes=None):
        ok = subprocess.CompletedProcess([], 0, '', '')
        board_patch = (dict(side_effect=build_board) if callable(build_board)
                       else dict(return_value=build_board))
        with mock.patch.object(sys, 'argv', ['metrics_compare_base.py', '-b', 'b']), \
             mock.patch.object(mcb, 'METRICS_DIR', tmp), \
             mock.patch.object(mcb, 'run', return_value=ok), \
             mock.patch.object(mcb, 'symlink_deps'), \
             mock.patch.object(mcb, 'build_board', **board_patch), \
             mock.patch.object(mcb, 'generate_membrowse_sizes', return_value=sizes):
            return mcb.main()

    def test_build_dirs_are_cleaned_before_build(self):
        with tempfile.TemporaryDirectory() as tmp:
            stale_paths = [os.path.join(tmp, 'b', side, 'removed', 'removed.elf')
                           for side in ('base', 'build')]
            for path in stale_paths:
                os.makedirs(os.path.dirname(path))
                with open(path, 'w') as f:
                    f.write('stale')

            def build_board(*_args, **_kwargs):
                self.assertFalse(any(os.path.exists(path) for path in stale_paths))
                return True

            self.assertEqual(self._main(tmp, build_board, {'x.c': {'flash': 1, 'ram': 1}}), 0)

    def test_failed_report_removes_stale_output_and_returns_nonzero(self):
        with tempfile.TemporaryDirectory() as tmp:
            board_dir = os.path.join(tmp, 'b')
            os.makedirs(board_dir)
            stale = os.path.join(board_dir, 'metrics_compare.md')
            with open(stale, 'w') as f:
                f.write('stale')
            self.assertEqual(self._main(tmp, sizes=None), 1)
            self.assertFalse(os.path.exists(stale))

    def test_success_returns_zero(self):
        with tempfile.TemporaryDirectory() as tmp:
            os.makedirs(os.path.join(tmp, 'b'))
            self.assertEqual(self._main(tmp, sizes={'x.c': {'flash': 1, 'ram': 1}}), 0)

    def _main_combined(self, tmp, fail_out=None, metrics=True, build_ok=True, example=None):
        """main() with --combined --engine linkermap and metrics.py stubbed out.

        `fail_out` is the metrics.py `-o` path (relative to METRICS_DIR) whose run
        returns 1; `metrics` False fails per-board metric generation instead;
        `build_ok` False fails every board build; `example` adds -e (whose per-board
        JSONs carry a suffix the combined step doesn't read).
        Returns (rc, stdout, the argv lists run() saw).
        """
        ok = subprocess.CompletedProcess([], 0, '', '')
        bad = subprocess.CompletedProcess([], 1, '', 'boom')
        cmds = []

        def run(cmd, **_kwargs):
            cmds.append(cmd)
            if '-o' not in cmd:
                return ok
            return bad if os.path.relpath(cmd[cmd.index('-o') + 1], tmp) == fail_out else ok

        def generate_metrics(_build_dir, out_basename, _filters, example=None):
            if not metrics:
                return None
            os.makedirs(os.path.dirname(out_basename), exist_ok=True)
            with open(f'{out_basename}.json', 'w') as f:
                f.write('{}')
            return f'{out_basename}.json'

        argv = ['metrics_compare_base.py', '-b', 'b', '--combined', '--engine', 'linkermap']
        if example:
            argv += ['-e', example]
        with mock.patch.object(sys, 'argv', argv), \
             mock.patch.object(mcb, 'METRICS_DIR', tmp), \
             mock.patch.object(mcb, 'run', side_effect=run), \
             mock.patch.object(mcb, 'symlink_deps'), \
             mock.patch.object(mcb, 'build_board', return_value=build_ok), \
             mock.patch.object(mcb, 'generate_metrics', side_effect=generate_metrics):
            buf = io.StringIO()
            with contextlib.redirect_stdout(buf):
                rc = mcb.main()
        return rc, buf.getvalue(), cmds

    def test_combined_success_returns_zero_and_reports(self):
        with tempfile.TemporaryDirectory() as tmp:
            rc, out, _cmds = self._main_combined(tmp)
            self.assertEqual(rc, 0)
            self.assertIn('combined report', out)

    def test_combined_base_combine_failure_returns_nonzero(self):
        with tempfile.TemporaryDirectory() as tmp:
            rc, out, _cmds = self._main_combined(
                tmp, fail_out=os.path.join('_combined', 'base_metrics'))
            self.assertEqual(rc, 1)
            self.assertNotIn('combined report', out)

    def test_combined_current_combine_failure_returns_nonzero(self):
        with tempfile.TemporaryDirectory() as tmp:
            rc, out, _cmds = self._main_combined(
                tmp, fail_out=os.path.join('_combined', 'build_metrics'))
            self.assertEqual(rc, 1)
            self.assertNotIn('combined report', out)

    def test_combined_compare_failure_returns_nonzero(self):
        with tempfile.TemporaryDirectory() as tmp:
            rc, out, _cmds = self._main_combined(
                tmp, fail_out=os.path.join('_combined', 'metrics_compare'))
            self.assertEqual(rc, 1)
            self.assertNotIn('combined report', out)
            self.assertIn('boom', out)
            self.assertFalse(os.path.isfile(os.path.join(tmp, '_combined',
                                                         'metrics_compare.md')))

    def test_failed_metrics_drop_the_board_from_the_combined_set(self):
        # a previous run's JSONs must not stand in for the ones this run failed
        # to generate - the board simply drops out of the combined comparison
        with tempfile.TemporaryDirectory() as tmp:
            board_dir = os.path.join(tmp, 'b')
            os.makedirs(board_dir)
            stale = [os.path.join(board_dir, name)
                     for name in ('base_metrics.json', 'build_metrics.json')]
            for path in stale:
                with open(path, 'w') as f:
                    f.write('{}')
            rc, out, cmds = self._main_combined(tmp, metrics=False)
            self.assertEqual(rc, 1)
            self.assertFalse(any(os.path.exists(path) for path in stale))
            self.assertFalse(any(path in cmd for cmd in cmds for path in stale))
            self.assertNotIn('combined report', out)

    def _seed_combined_report(self, tmp):
        """A previous run's combined report, left behind in the gitignored
        cmake-metrics/ tree that nothing else wipes."""
        os.makedirs(os.path.join(tmp, '_combined'))
        stale = os.path.join(tmp, '_combined', 'metrics_compare.md')
        with open(stale, 'w') as f:
            f.write('| previous run | 100 | 200 |')
        return stale

    def test_combined_compare_failure_drops_the_previous_report(self):
        with tempfile.TemporaryDirectory() as tmp:
            stale = self._seed_combined_report(tmp)
            rc, out, _cmds = self._main_combined(
                tmp, fail_out=os.path.join('_combined', 'metrics_compare'))
            self.assertEqual(rc, 1)
            self.assertFalse(os.path.exists(stale))

    def test_no_board_built_drops_the_previous_combined_report(self):
        # every board fails to build, so the combined step is skipped entirely
        with tempfile.TemporaryDirectory() as tmp:
            stale = self._seed_combined_report(tmp)
            rc, out, _cmds = self._main_combined(tmp, build_ok=False)
            self.assertEqual(rc, 1)
            self.assertFalse(os.path.exists(stale))

    def test_no_per_board_metrics_drops_the_previous_combined_report(self):
        # -e with --combined finds no whole-board JSONs and just prints, so this
        # run exits 0: the stale report must already be gone, or a reader
        # following the exit code takes a previous run's numbers for this one's.
        with tempfile.TemporaryDirectory() as tmp:
            stale = self._seed_combined_report(tmp)
            rc, out, _cmds = self._main_combined(tmp, example='device/cdc_msc')
            self.assertEqual(rc, 0)
            self.assertIn('no per-board metrics found', out)
            self.assertFalse(os.path.exists(stale))


class GlobMetacharsInBuildDir(unittest.TestCase):
    """A build dir under a path with glob metachars - a worktree or CI workspace
    named after e.g. `pr[1]` - must still find the files a good build produced.
    """

    def _tree(self, tmp):
        build_dir = os.path.join(tmp, 'pr[1]', 'build')
        ex_dir = os.path.join(build_dir, 'device', 'cdc_msc')
        os.makedirs(ex_dir)
        elf = os.path.join(ex_dir, 'cdc_msc.elf')
        map_json = os.path.join(ex_dir, 'cdc_msc.map.json')
        for path in (elf, map_json, elf + '.map'):
            open(path, 'w').close()
        return build_dir, elf, map_json

    def test_map_json_found(self):
        with tempfile.TemporaryDirectory() as tmp:
            build_dir, _elf, map_json = self._tree(tmp)
            cmds = []

            def run(cmd, **_kwargs):
                cmds.append(cmd)
                return subprocess.CompletedProcess([], 0, '', '')

            with mock.patch.object(mcb, 'run', side_effect=run):
                out = mcb.generate_metrics(build_dir, os.path.join(tmp, 'm'), ['src/'])
            self.assertIsNotNone(out)
            self.assertIn(map_json, cmds[0])

    def test_map_json_found_for_a_single_example(self):
        with tempfile.TemporaryDirectory() as tmp:
            build_dir, _elf, map_json = self._tree(tmp)
            cmds = []

            def run(cmd, **_kwargs):
                cmds.append(cmd)
                return subprocess.CompletedProcess([], 0, '', '')

            with mock.patch.object(mcb, 'run', side_effect=run):
                out = mcb.generate_metrics(build_dir, os.path.join(tmp, 'm'), ['src/'],
                                           example='device/cdc_msc')
            self.assertIsNotNone(out)
            self.assertIn(map_json, cmds[0])

    def test_elf_found(self):
        fake_report = {'symbols': [
            {'name': 'x', 'size': 4, 'section': '.text',
             'object_file': 'build/x.c.obj', 'source_file': 'x.c'},
        ]}
        with tempfile.TemporaryDirectory() as tmp:
            build_dir, _elf, _map_json = self._tree(tmp)
            with mock.patch.object(membrowse_compare, 'report_for_elf',
                                   return_value=fake_report):
                result = mcb.generate_membrowse_sizes(build_dir, ['build/'])
            self.assertIsNotNone(result)
            self.assertEqual(result['x.c']['flash'], 4)


if __name__ == '__main__':
    unittest.main()
