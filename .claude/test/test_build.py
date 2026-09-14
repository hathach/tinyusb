"""Tests for the build skill's build.py: scope resolution through ci_select, the
dependency preflight, and the verdict it derives from tools/build.py's rows.
The build itself is stubbed; a real board build is verified by running the script."""
import importlib.util
import json
import sys
import unittest
from pathlib import Path
from unittest import mock

SCRIPT = Path(__file__).resolve().parents[1] / 'skills' / 'build' / 'scripts' / 'build.py'
spec = importlib.util.spec_from_file_location('build_skill', SCRIPT)
build = importlib.util.module_from_spec(spec)
spec.loader.exec_module(build)

OK, FAILED, SKIPPED = '\033[32mOK\033[0m', '\033[31mFailed\033[0m', '\033[33mSkipped\033[0m'


def row(board, target, status):
    return f'| {board:30} | {target:40} | {status:16} | 1.00s |\n'


class ResolveTest(unittest.TestCase):
    def test_full_matrix_uses_the_representative_pair_plus_changed_boards(self):
        boards, how = build.boards_for({'build': {'full': True, 'families': ['all']}, 'boards': {}})
        self.assertEqual((boards, how), (build.FULL_MATRIX_BOARDS, 'full matrix'))
        boards, how = build.boards_for({'build': {'full': True, 'families': ['all']}, 'boards': {}},
                                       ['src/tusb.c', 'hw/bsp/stm32f4/boards/stm32f411blackpill/board.h'])
        self.assertEqual(boards, build.FULL_MATRIX_BOARDS + ['stm32f411blackpill'])

    def test_base_mode_classifies_with_ci_select_base_and_keeps_changed_boards(self):
        # ci_select --base sees dependency revision changes that a path list cannot;
        # the diff paths serve board preservation only
        with mock.patch.object(build, 'changed_paths', return_value=['hw/bsp/stm32f4/boards/stm32f411blackpill/board.h']), \
             mock.patch.object(build, 'select', return_value=({'build': {'full': False, 'families': ['stm32f4']}, 'boards': {}}, [])) as sel, \
             mock.patch.object(build, 'build_one', return_value={'status': 'ok'}) as b1, mock.patch('sys.stdout'):
            build.main(['--base', 'master'])
        self.assertEqual(sel.call_args.kwargs['base'], 'master')
        self.assertIsNone(sel.call_args.kwargs['scope'])
        self.assertEqual(b1.call_args[0][0], 'stm32f411blackpill')

    def test_select_base_passes_base_to_ci_select(self):
        with mock.patch.object(build.subprocess, 'run',
                               return_value=mock.Mock(returncode=0, stdout='{"build": {}}', stderr='')) as run:
            build.select(base='master')
        self.assertEqual(run.call_args[0][0][2:4], ['--base', 'master'])

    def test_no_family_resolves_to_no_board(self):
        boards, how = build.boards_for({'build': {'full': False, 'families': []}, 'boards': {}},
                                       ['docs/index.rst'])
        self.assertEqual((boards, how), ([], 'no build family'))

    def test_a_scope_no_board_builds_is_exit_3_with_the_reason_per_path(self):
        # docs and .claude are nothing to verify; a class no example enables, a lib
        # nothing builds and a port mapping to no family are unverified firmware. The
        # script reports ci_select's own words for each rather than judging.
        for scope, marker in (
            (['docs/index.rst', '.claude/agents/builder.md'], 'non-code'),
            (['src/class/bth/bth_device.c'], 'enabled by no example'),
            (['lib/SEGGER_RTT/RTT/SEGGER_RTT.c'], 'built by no example'),
            (['src/portable/no_vendor/no_driver/dcd_bogus.c'], 'families []'),
        ):
            with mock.patch.object(build, 'build_one') as b1, mock.patch('sys.stdout') as out:
                self.assertEqual(build.main(['--scope', *scope]), 3, scope)
            b1.assert_not_called()
            printed = json.loads(out.write.call_args_list[0][0][0])
            self.assertEqual((printed['pass'], printed['boards']), (False, []))
            self.assertTrue(any(marker in r for r in printed['nothingToBuild']),
                            f'{scope}: {printed["nothingToBuild"]}')

    def test_rig_board_of_the_family_is_preferred_over_the_first_bsp_board(self):
        sel = {'build': {'full': False, 'families': ['stm32f4']}, 'boards': {'stm32f407disco': 'all'}}
        self.assertEqual(build.boards_for(sel)[0], ['stm32f407disco'])
        sel['boards'] = {}
        self.assertEqual(build.boards_for(sel)[0], [build.family_boards('stm32f4')[0]])

    def test_a_changed_board_dir_selects_that_board_over_the_rig_sample(self):
        sel = {'build': {'full': False, 'families': ['stm32f4']}, 'boards': {'stm32f407disco': 'all'}}
        boards, how = build.boards_for(sel, ['hw/bsp/stm32f4/boards/stm32f411blackpill/board.h'])
        self.assertEqual(boards, ['stm32f411blackpill'])
        self.assertIn('changed boards', how)

    def test_unknown_board_is_an_error(self):
        with self.assertRaises(SystemExit) as cm:
            build.family_of('no_such_board')
        self.assertEqual(cm.exception.code, 2)


class VerdictTest(unittest.TestCase):
    def build(self, rc, out, fetch=False):
        with mock.patch.object(build, 'run', return_value=(rc, out)) as run, \
             mock.patch.object(build, 'missing_deps', return_value=[]):
            r = build.build_one('stm32f407disco', ['device/cdc_msc'], [], [], [], False, fetch, False)
        cmd = run.call_args[0][0]
        self.assertIn('--build-name', cmd)
        self.assertEqual(cmd[cmd.index('-e') + 1], 'device/cdc_msc')
        return r

    def test_ok_rows_pass(self):
        r = self.build(0, row('stm32f407disco', 'all', OK))
        self.assertEqual((r['status'], r['firstError']), ('ok', ''))
        self.assertEqual(r['buildDir'], f'cmake-build/cmake-build-agent-{build.os.getpid()}-stm32f407disco')
        self.assertEqual(r['built'], 0)  # stubbed build wrote no elf

    def test_built_counts_only_elfs_this_run_wrote(self):
        import os, shutil, time
        d = build.ROOT / 'cmake-build' / 'cmake-build-agent-test-stm32f407disco'
        shutil.rmtree(d, ignore_errors=True); d.mkdir(parents=True)
        try:
            stale, fresh = d / 'old.elf', d / 'new.elf'
            stale.write_bytes(b''); os.utime(stale, (1, 1))
            def fake_run(cmd, verbose):
                time.sleep(0.01); fresh.write_bytes(b''); return 0, row('stm32f407disco', 'all', OK)
            with mock.patch.object(build, 'run', fake_run), mock.patch.object(build, 'missing_deps', return_value=[]), \
                 mock.patch.object(build.os, 'getpid', return_value='test'):
                r = build.build_one('stm32f407disco', [], [], [], [], False, False, False)
        finally:
            shutil.rmtree(d)
        self.assertEqual(r['built'], 1)

    def test_failed_row_reports_the_first_error_line(self):
        out = ('FAILED: device/a/a.o\nCommand Error: cc failed\nsrc/a.c:3:5: error: expected ;\n'
               'ninja: build stopped\n' + row('stm32f407disco', 'all', FAILED))
        r = self.build(1, out)
        self.assertEqual(r['status'], 'failed')
        self.assertEqual(r['firstError'], 'src/a.c:3:5: error: expected ;')

    def test_cmake_error_is_the_first_error_when_nothing_compiled(self):
        out = 'CMake Error at hw/bsp/nrf/family.cmake:59 (add_library):\n  No SOURCES\n' + row('b', 'all', FAILED)
        self.assertEqual(self.build(1, out)['firstError'], 'CMake Error at hw/bsp/nrf/family.cmake:59 (add_library):')

    def test_all_skipped_is_not_a_pass(self):
        r = self.build(0, row('stm32f407disco', 'examples (PR filter)', SKIPPED))
        self.assertEqual(r['status'], 'skipped')

    def test_no_rows_surfaces_the_tool_message(self):
        r = self.build(2, "build.py: error: -e/--example 'device/nope': no such example\n")
        self.assertEqual(r['status'], 'error')
        self.assertIn('device/nope', r['firstError'])

    def test_defines_and_cflags_reach_tools_build(self):
        with mock.patch.object(build, 'run', return_value=(0, row('stm32f407disco', 'all', OK))) as run, \
             mock.patch.object(build, 'missing_deps', return_value=[]):
            build.build_one('stm32f407disco', ['host/cdc_msc_hid'], [], ['LOG=2'],
                            ['-DCFG_TUH_CDC_FTDI_LATENCY=16'], False, False, False)
        cmd = run.call_args[0][0]
        self.assertEqual(cmd[cmd.index('-D') + 1], 'LOG=2')
        self.assertIn('--cflag=-DCFG_TUH_CDC_FTDI_LATENCY=16', cmd)

    def test_a_define_on_an_espressif_board_is_refused_not_dropped(self):
        esp = build.family_boards('espressif')[0]
        with mock.patch.object(build, 'run') as run, mock.patch.object(sys, 'stderr') as err:
            with self.assertRaises(SystemExit) as cm:
                build.build_one(esp, [], [], ['LOG=2'], [], False, False, False)
        self.assertEqual(cm.exception.code, 2)
        self.assertIn('idf.py', err.write.call_args[0][0])
        self.assertNotIn('instead', err.write.call_args[0][0])  # no non-equivalent replacement offered
        run.assert_not_called()

    def test_shared_uses_the_canonical_hil_dir(self):
        with mock.patch.object(build, 'run', return_value=(0, row('stm32f407disco', 'all', OK))) as run, \
             mock.patch.object(build, 'missing_deps', return_value=[]):
            r = build.build_one('stm32f407disco', [], [], [], [], True, False, False)
        self.assertNotIn('--build-name', run.call_args[0][0])
        self.assertEqual(r['buildDir'], 'cmake-build/cmake-build-stm32f407disco')


class DepsTest(unittest.TestCase):
    def test_missing_deps_fail_with_the_remedy_unless_fetching(self):
        with mock.patch.object(build, 'missing_deps', return_value=['hw/mcu/nordic/nrfx']), \
             mock.patch.object(build, 'run', return_value=(0, '')) as run, \
             mock.patch.object(sys, 'stderr') as err:
            with self.assertRaises(SystemExit) as cm:
                build.ensure_deps('nrf', False, False)
            self.assertEqual(cm.exception.code, 2)
            self.assertIn('nrfx', err.write.call_args[0][0])
            run.assert_not_called()

    def test_fetch_runs_get_deps_once_then_rechecks(self):
        with mock.patch.object(build, 'missing_deps', side_effect=[['hw/mcu/nordic/nrfx'], []]), \
             mock.patch.object(build, 'run', return_value=(0, '')) as run:
            build.ensure_deps('nrf', True, False)
        self.assertEqual(run.call_args[0][0][-2:], [str(build.ROOT / 'tools' / 'get_deps.py'), 'nrf'])

    def test_family_deps_come_from_get_deps_table(self):
        self.assertIn('hw/mcu/nordic/nrfx', [d for d, e in build.get_deps.deps_optional.items() if 'nrf' in e[2].split()])


class MainTest(unittest.TestCase):
    def test_main_prints_json_and_exit_reflects_pass(self):
        results = [{'board': 'b', 'family': 'f', 'buildDir': 'd', 'status': 'ok', 'firstError': ''}]
        with mock.patch.object(build, 'build_one', return_value=results[0]), \
             mock.patch('sys.stdout') as out:
            self.assertEqual(build.main(['--board', 'b']), 0)
        printed = json.loads(out.write.call_args_list[0][0][0])
        self.assertEqual(printed, {'pass': True, 'boards': results, 'resolution': 'named boards'})
        results[0]['status'] = 'failed'
        with mock.patch.object(build, 'build_one', return_value=results[0]), mock.patch('sys.stdout'):
            self.assertEqual(build.main(['--board', 'b']), 1)


if __name__ == '__main__':
    unittest.main()
