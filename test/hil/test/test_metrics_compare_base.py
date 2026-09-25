#!/usr/bin/env python3
"""Unit tests for tools/metrics_compare_base.py.

glob.glob, the metrics_compare engines and the build steps are
monkeypatched, so no build and no real membrowse CLI invocation is needed.
"""
import contextlib
import io
import json
import os
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), '../../..'))
sys.path.insert(0, os.path.join(REPO, 'tools'))
import metrics_compare  # noqa: E402
import metrics_compare_base as mcb  # noqa: E402


def _run_capturing_stdout(*args, **kwargs):
    buf = io.StringIO()
    with contextlib.redirect_stdout(buf):
        result = mcb.generate_sizes(*args, **kwargs)
    return result, buf.getvalue()


def _elf(flash):
    return {'files': {'x.c': {'flash': flash, 'ram': 0}}, 'all': {'flash': flash, 'ram': 0}}


def _engine(name, sizes):
    """Patch ENGINES[name] to size elfs with `sizes` (a Mock or its side_effect)."""
    if not isinstance(sizes, mock.Mock):
        sizes = mock.Mock(side_effect=sizes)
    return mock.patch.dict(metrics_compare.ENGINES,
                           {name: metrics_compare.ENGINES[name]._replace(sizes=sizes)})


class GenerateSizes(unittest.TestCase):
    def test_no_elfs_errors_and_returns_nothing(self):
        with mock.patch('glob.glob', return_value=[]):
            (sizes, errors), out = _run_capturing_stdout('/fake/build', ['/fake/src/'])
        self.assertEqual(sizes, {})
        self.assertEqual(errors[0][0], None)
        self.assertIn('no .elf files', out)

    def test_membrowse_cli_missing_errors_and_returns_nothing(self):
        # subprocess.run(['membrowse', ...]) raises FileNotFoundError when the
        # CLI isn't installed - must not surface as a bare traceback after the
        # base+branch builds already ran (minutes of work).
        with mock.patch('glob.glob', return_value=['/fake/build/ex/ex.elf']), \
             _engine('membrowse', mock.Mock(side_effect=FileNotFoundError('membrowse'))):
            (sizes, errors), out = _run_capturing_stdout('/fake/build', ['/fake/src/'])
        self.assertEqual(sizes, {})
        self.assertEqual(errors[0][0], None)
        self.assertIn('pip install membrowse', out)
        self.assertIn('another --engine', out)

    def test_the_selected_engine_sizes_each_elf(self):
        bloaty = mock.Mock(return_value=_elf(7))
        with mock.patch('glob.glob', return_value=['/fake/build/ex/ex.elf']), \
             _engine('bloaty', bloaty):
            sizes, errors = mcb.generate_sizes('/fake/build', ['src/'], engine='bloaty')
        bloaty.assert_called_once_with('/fake/build/ex/ex.elf', ['src/'])
        self.assertEqual((sizes, errors), ({'ex/ex.elf': _elf(7)}, []))

    def test_a_missing_engine_tool_names_its_install(self):
        with mock.patch('glob.glob', return_value=['/fake/build/ex/ex.elf']), \
             _engine('bloaty', mock.Mock(side_effect=FileNotFoundError('bloaty'))):
            (sizes, errors), out = _run_capturing_stdout('/fake/build', ['src/'], engine='bloaty')
        self.assertEqual((sizes, errors), ({}, [(None, 'bloaty not found')]))
        self.assertIn('github.com/google/bloaty', out)

    def test_a_failed_elf_marks_only_that_elf(self):
        # an engine raises RuntimeError for one elf (`membrowse report` exiting
        # non-zero, malformed output): that elf fails, the others are still sized
        def sizes(elf, _filters):
            if 'bad' in elf:
                raise RuntimeError(f'membrowse report failed for {elf}: boom')
            return _elf(4)
        with mock.patch('glob.glob', return_value=['/fake/build/bad/bad.elf',
                                                    '/fake/build/ok/ok.elf']), \
             _engine('membrowse', sizes):
            (sizes, errors), out = _run_capturing_stdout('/fake/build', ['build/'])
        self.assertIsNone(sizes['bad/bad.elf'])
        self.assertEqual(sizes['ok/ok.elf']['files']['x.c']['flash'], 4)
        self.assertEqual([rel for rel, _ in errors], ['bad/bad.elf'])
        self.assertIn('membrowse report failed', out)

    def test_each_elf_is_kept_by_its_relative_path(self):
        # no averaging or summing across elfs: every elf, including two of one
        # example (app + bootloader), is its own pairing identity
        # keyed by elf: the reports run on a thread pool, in no fixed order
        flash = {'/fake/build/ex/app.elf': 100, '/fake/build/ex/loader.elf': 200,
                 '/fake/build/ex2/ex2.elf': 300}
        with mock.patch('glob.glob', return_value=list(flash)), \
             _engine('membrowse', lambda elf, _filters: _elf(flash[elf])):
            sizes, errors = mcb.generate_sizes('/fake/build', ['build/'])
        self.assertEqual(errors, [])
        self.assertEqual({rel: s['files']['x.c']['flash'] for rel, s in sizes.items()},
                         {'ex/app.elf': 100, 'ex/loader.elf': 200, 'ex2/ex2.elf': 300})
        self.assertEqual(sizes['ex/app.elf']['all'], {'flash': 100, 'ram': 0})


class MainFailure(unittest.TestCase):
    def _run_main(self, tmp, argv, build_board, generate=None, run=None):
        """main() with the build, sizing and command steps stubbed. `build_board`
        is its side_effect (it must create the board dir, as the real one does);
        `generate` the generate_sizes() side_effect. Returns (rc, stdout)."""
        ok = subprocess.CompletedProcess([], 0, 'c0ffee\n', '')
        with mock.patch.object(sys, 'argv', ['metrics_compare_base.py'] + argv), \
             mock.patch.object(mcb, 'METRICS_DIR', tmp), \
             mock.patch.object(mcb, 'run', side_effect=run or (lambda *_a, **_k: ok)), \
             mock.patch.object(mcb, 'symlink_deps'), \
             mock.patch.object(mcb, 'build_board', side_effect=build_board), \
             mock.patch.object(mcb, 'generate_sizes', side_effect=generate):
            buf = io.StringIO()
            with contextlib.redirect_stdout(buf):
                rc = mcb.main()
        return rc, buf.getvalue()

    def _main(self, tmp, build_board=None, sizes=None, cur_sizes=None):
        """main() for board `b`; `sizes` is what generate_sizes() returns for
        the base side and, unless `cur_sizes` is given, the current side too."""
        def build(_src, _build_dir, board, *_args, **_kwargs):
            os.makedirs(os.path.join(tmp, board), exist_ok=True)
            return build_board() if build_board else True
        side_sizes = iter([sizes, sizes if cur_sizes is None else cur_sizes])
        return self._run_main(tmp, ['-b', 'b'], build, lambda *_a, **_k: next(side_sizes))

    def test_build_dirs_are_cleaned_before_build(self):
        with tempfile.TemporaryDirectory() as tmp:
            stale_paths = [os.path.join(tmp, 'b', side, 'removed', 'removed.elf')
                           for side in ('base', 'build')]
            for path in stale_paths:
                os.makedirs(os.path.dirname(path))
                with open(path, 'w') as f:
                    f.write('stale')

            def build_board():
                self.assertFalse(any(os.path.exists(path) for path in stale_paths))
                return True

            rc, _out = self._main(tmp, build_board, ({'ex/ex.elf': _elf(1)}, []))
            self.assertEqual(rc, 0)

    def test_failed_report_replaces_stale_output_and_returns_nonzero(self):
        with tempfile.TemporaryDirectory() as tmp:
            board_dir = os.path.join(tmp, 'b')
            os.makedirs(board_dir)
            stale = os.path.join(board_dir, 'metrics_compare.md')
            with open(stale, 'w') as f:
                f.write('stale')
            rc, _out = self._main(tmp, sizes=({}, [(None, 'boom')]))
            self.assertEqual(rc, 1)
            with open(stale) as f:
                md = f.read()
            self.assertIn('INCOMPLETE', md)
            self.assertIn('_no comparable pairs_', md)
            self.assertIn('boom', md)

    def test_success_returns_zero(self):
        with tempfile.TemporaryDirectory() as tmp:
            rc, _out = self._main(tmp, sizes=({'ex/ex.elf': _elf(1)}, []),
                                  cur_sizes=({'ex/ex.elf': _elf(3)}, []))
            self.assertEqual(rc, 0)
            with open(os.path.join(tmp, 'b', 'metrics_compare.md')) as f:
                self.assertIn('| x.c | 1 | 3 | +2 |', f.read())

    def test_unmatched_elf_is_incomplete_but_not_a_failure(self):
        with tempfile.TemporaryDirectory() as tmp:
            rc, out = self._main(tmp, sizes=({'ex/ex.elf': _elf(1)}, []),
                                 cur_sizes=({'ex/ex.elf': _elf(1), 'new/new.elf': _elf(5)}, []))
            self.assertEqual(rc, 0)
            self.assertIn('INCOMPLETE', out)
            self.assertIn('current-only: `b: new/new.elf`', out)

    def test_no_symbols_matched_filters_fails(self):
        # a filter typo, or an engine output change breaking source-path
        # matching: must not silently produce an empty/degenerate table
        empty = {'files': {}, 'all': {'flash': 4, 'ram': 0}}
        with tempfile.TemporaryDirectory() as tmp:
            rc, out = self._main(tmp, sizes=({'ex/ex.elf': empty}, []))
            self.assertEqual(rc, 1)
            self.assertIn('no membrowse sizes matched filters', out)
            self.assertIn('another --engine', out)

    def test_one_side_without_matched_files_is_a_valid_removal(self):
        empty = {'files': {}, 'all': {'flash': 4, 'ram': 0}}
        with tempfile.TemporaryDirectory() as tmp:
            rc, out = self._main(tmp, sizes=({'ex/ex.elf': _elf(8)}, []),
                                 cur_sizes=({'ex/ex.elf': empty}, []))
            self.assertEqual(rc, 0)
            self.assertIn('| x.c | 8 | 0 | -8 |', out)

    def _main_combined(self, tmp, sizes, build_ok=('b1', 'b2'), example=None):
        """main() with -b b1 -b b2 --combined. `sizes` maps
        (board, side) to what generate_sizes() returns; boards not in
        `build_ok` fail their base build. Returns (rc, stdout, combined md or None)."""
        def build_board(_src, _build_dir, board, *_args, **_kwargs):
            os.makedirs(os.path.join(tmp, board), exist_ok=True)
            return board in build_ok

        def generate(build_dir, _filters, _example=None, _engine='membrowse'):
            board, side = os.path.relpath(build_dir, tmp).split(os.sep)
            return sizes[(board, 'base' if side == 'base' else 'current')]

        argv = ['-b', 'b1', '-b', 'b2', '--combined'] + (['-e', example] if example else [])
        rc, out = self._run_main(tmp, argv, build_board, generate)
        combined = os.path.join(tmp, '_combined', 'metrics_compare.md')
        md = None
        if os.path.isfile(combined):
            with open(combined) as f:
                md = f.read()
        return rc, out, md

    def test_combined_pairs_every_board(self):
        sizes = {('b1', 'base'): ({'ex/ex.elf': _elf(10)}, []),
                 ('b1', 'current'): ({'ex/ex.elf': _elf(10)}, []),
                 ('b2', 'base'): ({'ex/ex.elf': _elf(10)}, []),
                 ('b2', 'current'): ({'ex/ex.elf': _elf(50)}, [])}
        with tempfile.TemporaryDirectory() as tmp:
            rc, _out, md = self._main_combined(tmp, sizes)
        self.assertEqual(rc, 0)
        self.assertIn('Coverage (complete, membrowse):** 2 of 2 matched elf pairs compared, 1 changed', md)
        self.assertIn('- boards: `b1`, `b2`', md)
        self.assertIn('| x.c | 1/2 | 0 | +40 (b2: ex/ex.elf) |', md)

    def test_combined_records_a_failed_board(self):
        sizes = {('b1', 'base'): ({'ex/ex.elf': _elf(10)}, []),
                 ('b1', 'current'): ({'ex/ex.elf': _elf(12)}, [])}
        with tempfile.TemporaryDirectory() as tmp:
            rc, _out, md = self._main_combined(tmp, sizes, build_ok=('b1',))
        self.assertEqual(rc, 1)
        self.assertIn('Coverage (INCOMPLETE, membrowse):** 1 of 1 matched', md)
        self.assertIn('FAILED `b2` base build', md)
        self.assertIn('- boards: `b1`, `b2`', md)

    def test_combined_keeps_one_boards_filter_failure(self):
        # b1 matches files, b2 matches none: the combined report still names b2
        empty = {'files': {}, 'all': {'flash': 4, 'ram': 0}}
        sizes = {('b1', 'base'): ({'ex/ex.elf': _elf(10)}, []),
                 ('b1', 'current'): ({'ex/ex.elf': _elf(10)}, []),
                 ('b2', 'base'): ({'ex/ex.elf': empty}, []),
                 ('b2', 'current'): ({'ex/ex.elf': empty}, [])}
        with tempfile.TemporaryDirectory() as tmp:
            rc, _out, md = self._main_combined(tmp, sizes)
        self.assertEqual(rc, 1)
        self.assertIn('Coverage (INCOMPLETE, membrowse)', md)
        self.assertEqual(md.count('FAILED `b2` both filter'), 1)

    def test_failed_build_writes_a_per_board_report_for_each_scope(self):
        for example, name in ((None, 'metrics_compare.md'),
                              ('device/cdc_msc', 'metrics_compare_device_cdc_msc.md')):
            with tempfile.TemporaryDirectory() as tmp:
                def build_board(*_args, **_kwargs):
                    os.makedirs(os.path.join(tmp, 'b'), exist_ok=True)
                    return False
                argv = ['-b', 'b'] + (['-e', example] if example else [])
                rc, _out = self._run_main(tmp, argv, build_board)
                self.assertEqual(rc, 1)
                with open(os.path.join(tmp, 'b', name)) as f:
                    md = f.read()
                self.assertIn('INCOMPLETE', md)
                self.assertIn('FAILED `b` base build', md)
                self.assertIn('_no comparable pairs_', md)

    def test_later_scope_build_failure_skips_collection_and_bloaty(self):
        # -e a builds, -e b fails: both scopes' reports carry the failure, no elf
        # is collected or bloaty-diffed, and the combined report lists it once
        with tempfile.TemporaryDirectory() as tmp:
            def build_board(_src, _build_dir, board, example, *_args, **_kwargs):
                os.makedirs(os.path.join(tmp, board), exist_ok=True)
                return example == 'device/a'
            cmds = []

            def run(cmd, **_kwargs):
                cmds.append(cmd)
                return subprocess.CompletedProcess([], 0, '', '')
            generate = mock.Mock()
            rc, _out = self._run_main(tmp, ['-b', 'b', '-e', 'device/a', '-e', 'device/b',
                                            '--bloaty', '--combined'],
                                      build_board, generate, run)
            self.assertEqual(rc, 1)
            generate.assert_not_called()
            self.assertFalse(any('bloaty' in cmd for cmd in cmds))
            for name in ('metrics_compare_device_a.md', 'metrics_compare_device_b.md'):
                with open(os.path.join(tmp, 'b', name)) as f:
                    self.assertIn('FAILED `b` base build: build failed --target b', f.read())
            with open(os.path.join(tmp, '_combined', 'metrics_compare.md')) as f:
                self.assertEqual(f.read().count('FAILED `b`'), 1)

    def test_combined_with_an_example(self):
        sizes = {(b, side): ({'device/cdc_msc/cdc_msc.elf': _elf(5)}, [])
                 for b in ('b1', 'b2') for side in ('base', 'current')}
        with tempfile.TemporaryDirectory() as tmp:
            rc, _out, md = self._main_combined(tmp, sizes, example='device/cdc_msc')
            self.assertTrue(os.path.isfile(os.path.join(tmp, 'b1', 'metrics_compare_device_cdc_msc.md')))
        self.assertEqual(rc, 0)
        self.assertIn('2 of 2 matched elf pairs compared, 0 changed', md)

    def test_combined_replaces_the_previous_report_when_no_board_builds(self):
        with tempfile.TemporaryDirectory() as tmp:
            self._seed_combined_report(tmp)
            rc, _out, md = self._main_combined(tmp, {}, build_ok=())
        self.assertEqual(rc, 1)
        self.assertNotIn('previous run', md)
        self.assertIn('_no comparable pairs_', md)

    def _main_json(self, tmp, argv, sizes, build_ok=True):
        """main() for board `b` with `argv` added; `sizes` is what generate_sizes()
        returns for each side. Returns (rc, generate mock)."""
        def build(_src, _build_dir, board, *_args, **_kwargs):
            os.makedirs(os.path.join(tmp, board), exist_ok=True)
            return build_ok
        generate = mock.Mock(side_effect=lambda *_a, **_k: sizes)
        rc, _out = self._run_main(tmp, ['-b', 'b'] + argv, build, generate)
        return rc, generate

    def _read(self, *path):
        with open(os.path.join(*path)) as f:
            return f.read()

    def test_engine_is_passed_to_sizing_and_named_in_the_report(self):
        with tempfile.TemporaryDirectory() as tmp:
            rc, generate = self._main_json(tmp, ['--engine', 'linkermap'], ({'ex/ex.elf': _elf(1)}, []))
            self.assertEqual(rc, 0)
            self.assertEqual({c.args[3] for c in generate.call_args_list}, {'linkermap'})
            self.assertIn('Coverage (complete, linkermap)', self._read(tmp, 'b', 'metrics_compare.md'))

    def test_json_holds_the_paired_sizes_and_provenance(self):
        with tempfile.TemporaryDirectory() as tmp:
            rc, _ = self._main_json(tmp, ['--json', '--combined', '-f', 'src/'],
                                    ({'ex/ex.elf': _elf(1)}, []))
            self.assertEqual(rc, 0)
            for path in ((tmp, 'b', 'metrics_compare.json'), (tmp, '_combined', 'metrics_compare.json')):
                data = json.loads(self._read(*path))
                self.assertEqual(data['engine'], 'membrowse')
                self.assertEqual((data['base_ref'], data['base_sha']), ('master', 'c0ffee'))
                self.assertEqual(data['filters'], {'base': ['src/'], 'current': ['src/']})
                self.assertEqual(data['pairs'], [{'board': 'b', 'elf': 'ex/ex.elf',
                                                  'base': _elf(1), 'current': _elf(1)}])
                self.assertEqual((data['base_only'], data['current_only'], data['failures']), ([], [], []))
                self.assertEqual(data['status'], 'complete')

    def test_json_of_a_failed_run_matches_its_report(self):
        with tempfile.TemporaryDirectory() as tmp:
            os.makedirs(os.path.join(tmp, 'b'))
            for name in ('metrics_compare.md', 'metrics_compare.json'):
                with open(os.path.join(tmp, 'b', name), 'w') as f:
                    f.write('stale')
            rc, _ = self._main_json(tmp, ['--json'], None, build_ok=False)
            self.assertEqual(rc, 1)
            data = json.loads(self._read(tmp, 'b', 'metrics_compare.json'))
            self.assertEqual(data['status'], 'INCOMPLETE')
            self.assertEqual(data['failures'], [{'board': 'b', 'elf': None, 'side': 'base', 'stage': 'build',
                                                 'message': 'build failed, see log'}])
            self.assertIn('FAILED `b` base build: build failed, see log',
                          self._read(tmp, 'b', 'metrics_compare.md'))

    def test_a_failed_worktree_setup_leaves_no_previous_report(self):
        with tempfile.TemporaryDirectory() as tmp:
            os.makedirs(os.path.join(tmp, 'b'))
            stale = [os.path.join(tmp, 'b', f'metrics_compare.{ext}') for ext in ('md', 'json')]
            for path in stale:
                with open(path, 'w') as f:
                    f.write('stale')
            failed = subprocess.CompletedProcess([], 128, '', 'fatal: invalid reference')
            with self.assertRaises(SystemExit):
                self._run_main(tmp, ['-b', 'b'], mock.Mock(), run=lambda *_a, **_k: failed)
            self.assertFalse(any(os.path.exists(path) for path in stale))

    def test_a_run_without_json_drops_a_previous_json(self):
        with tempfile.TemporaryDirectory() as tmp:
            os.makedirs(os.path.join(tmp, 'b'))
            stale = os.path.join(tmp, 'b', 'metrics_compare.json')
            with open(stale, 'w') as f:
                f.write('{}')
            rc, _ = self._main_json(tmp, [], ({'ex/ex.elf': _elf(1)}, []))
            self.assertEqual(rc, 0)
            self.assertFalse(os.path.exists(stale))

    def _seed_combined_report(self, tmp):
        """A previous run's combined report, left behind in the gitignored
        cmake-metrics/ tree that nothing else wipes."""
        os.makedirs(os.path.join(tmp, '_combined'))
        stale = os.path.join(tmp, '_combined', 'metrics_compare.md')
        with open(stale, 'w') as f:
            f.write('| previous run | 100 | 200 |')
        return stale


class GlobMetacharsInBuildDir(unittest.TestCase):
    """A build dir under a path with glob metachars - a worktree or CI workspace
    named after e.g. `pr[1]` - must still find the files a good build produced.
    """

    def _tree(self, tmp):
        build_dir = os.path.join(tmp, 'pr[1]', 'build')
        ex_dir = os.path.join(build_dir, 'device', 'cdc_msc')
        os.makedirs(ex_dir)
        open(os.path.join(ex_dir, 'cdc_msc.elf'), 'w').close()
        return build_dir

    def test_elf_found(self):
        with tempfile.TemporaryDirectory() as tmp:
            build_dir = self._tree(tmp)
            with _engine('membrowse', mock.Mock(return_value=_elf(4))):
                sizes, errors = mcb.generate_sizes(build_dir, ['build/'])
            self.assertEqual(errors, [])
            self.assertEqual(sizes['device/cdc_msc/cdc_msc.elf']['files']['x.c']['flash'], 4)


class CiBoardSet(unittest.TestCase):
    def _boards_built(self, tmp, pinned_json):
        """Boards main() builds for `--ci -b extra -b b1`, with `pinned_json` as the pinned file."""
        pinned = os.path.join(tmp, 'ci-pinned-boards.json')
        with open(pinned, 'w') as f:
            f.write(pinned_json)
        built = []

        def build(_src, _build_dir, board, *_args, **_kwargs):
            built.append(board)
            os.makedirs(os.path.join(tmp, board), exist_ok=True)
            return False  # stop at the base build: only the board set matters
        ok = subprocess.CompletedProcess([], 0, '', '')
        with mock.patch.object(sys, 'argv', ['x', '--ci', '-b', 'extra', '-b', 'b1']), \
             mock.patch.object(mcb, 'METRICS_DIR', tmp), \
             mock.patch.object(mcb, 'CI_PINNED_BOARDS', pinned), \
             mock.patch.object(mcb, 'run', return_value=ok), \
             mock.patch.object(mcb, 'symlink_deps'), \
             mock.patch.object(mcb, 'build_board', side_effect=build), \
             contextlib.redirect_stdout(io.StringIO()):
            mcb.main()
        return built

    def test_ci_adds_the_pinned_boards_after_the_named_ones(self):
        with tempfile.TemporaryDirectory() as tmp:
            built = self._boards_built(tmp, '{"boards": [{"board": "b1"}, {"board": "b2"}], '
                                            '"uncovered": []}')
            self.assertEqual(built, ['extra', 'b1', 'b2'])


class SymlinkDeps(unittest.TestCase):
    def test_links_each_dep_path_of_the_worktrees_own_manifest(self):
        deps = {'hw/mcu/broadcom': [], 'hw/mcu/raspberry_pi/Pico-PIO-USB': [],
                'hw/mcu/wch/ch58x': [], 'lib/lwip': [], 'lib/not_fetched': []}
        with tempfile.TemporaryDirectory() as main, tempfile.TemporaryDirectory() as wt:
            for rel in ('hw/mcu/broadcom/broadcom', 'hw/mcu/raspberry_pi/Pico-PIO-USB',
                        'hw/mcu/wch/ch58x', 'hw/mcu/wch/ch583', 'lib/lwip',
                        'hw/mcu/raspberry_pi/tracked'):
                os.makedirs(os.path.join(main, rel))
            open(os.path.join(main, 'hw/mcu/broadcom/core_ca72.h'), 'w').close()
            os.makedirs(os.path.join(wt, 'lib/lwip'))  # already present: left alone
            os.makedirs(os.path.join(wt, 'tools'))
            with open(os.path.join(wt, 'tools', 'get_deps.py'), 'w') as f:
                f.write(f'deps_all = {deps!r}\n')
            mcb.symlink_deps(main, wt)
            # a whole-vendor-dep keeps its root files
            self.assertTrue(os.path.isfile(os.path.join(wt, 'hw/mcu/broadcom/core_ca72.h')))
            self.assertTrue(os.path.islink(os.path.join(wt, 'hw/mcu/broadcom')))
            self.assertTrue(os.path.islink(os.path.join(wt, 'hw/mcu/raspberry_pi/Pico-PIO-USB')))
            # the base's path, not the current checkout's rename
            self.assertTrue(os.path.islink(os.path.join(wt, 'hw/mcu/wch/ch58x')))
            self.assertFalse(os.path.lexists(os.path.join(wt, 'hw/mcu/wch/ch583')))
            self.assertFalse(os.path.islink(os.path.join(wt, 'lib/lwip')))
            self.assertFalse(os.path.lexists(os.path.join(wt, 'lib/not_fetched')))
            self.assertFalse(os.path.lexists(os.path.join(wt, 'hw/mcu/raspberry_pi/tracked')))


if __name__ == '__main__':
    unittest.main()
