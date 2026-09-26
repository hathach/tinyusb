"""Tests for the build skill's check_build.py: scope resolution through ci_select, the
dependency preflight, and the verdict it derives from tools/build.py's rows.
The build itself is stubbed; a real board build is verified by running the script."""
import importlib.util
import json
import os
import sys
import unittest
from pathlib import Path
from unittest import mock

SCRIPT = Path(__file__).resolve().parents[1] / 'skills' / 'build' / 'scripts' / 'check_build.py'
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
             mock.patch.object(build, 'build_one', return_value={'family': 'stm32f4', 'status': 'ok'}) as b1, mock.patch('sys.stdout'):
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

    def _main_scope(self, scope, built=None):
        built = built or {'board': 'b', 'family': 'f', 'buildDir': 'd', 'status': 'ok', 'firstError': '',
                          'okExamples': ['cdc_msc', 'cdc_msc_hid']}
        # the scopes below name paths that exist nowhere, to drive ci_select's reasons:
        # let them through the existence check as tracked deletions would be
        with mock.patch.object(build, 'build_one', return_value=built) as b1, mock.patch('sys.stdout') as out, \
             mock.patch.object(build, 'tracked', return_value=True):
            rc = build.main(['--scope', *scope])
        return rc, json.loads(out.write.call_args_list[0][0][0]), b1

    def test_a_scope_of_only_non_code_paths_passes_with_nothing_to_build(self):
        rc, printed, b1 = self._main_scope(['docs/index.rst', '.claude/skills/build/SKILL.md'])
        b1.assert_not_called()
        self.assertEqual((rc, printed['pass'], printed['boards'], printed['uncovered']), (0, True, [], []))
        self.assertTrue(all('non-code' in r for r in printed['nothingToBuild']), printed)

    def test_firmware_no_build_compiles_is_exit_3_with_the_reason_per_path(self):
        # a class no example enables, a lib nothing builds and a port mapping to no
        # family are unverified firmware, in ci_select's own words
        for scope, marker in (
            (['src/class/bth/bth_device.c'], 'enabled by no example'),
            (['lib/SEGGER_RTT/RTT/SEGGER_RTT.c'], 'built by no example'),
            (['src/portable/no_vendor/no_driver/dcd_bogus.c'], 'families []'),
        ):
            rc, printed, b1 = self._main_scope(scope)
            b1.assert_not_called()
            self.assertEqual((rc, printed['pass'], printed['boards']), (3, False, []), scope)
            self.assertTrue(any(marker in r for r in printed['uncovered']), f'{scope}: {printed}')

    def test_an_uncovered_path_fails_the_scope_even_when_the_rest_built(self):
        # the common mixed shape: a class driver plus the core file that registers it.
        # tusb.c resolves to the full matrix and builds green; bth stays a gap.
        rc, printed, b1 = self._main_scope(['src/class/bth/bth_device.c', 'src/tusb.c'])
        self.assertEqual(b1.call_count, len(build.FULL_MATRIX_BOARDS))
        self.assertEqual((rc, printed['pass']), (3, False))
        self.assertTrue(all(r['status'] == 'ok' for r in printed['boards']))
        self.assertTrue(any('bth' in r for r in printed['uncovered']), printed)
        # a build failure outranks the gap: exit 1, and the gap is still listed
        rc, printed, _ = self._main_scope(['src/class/bth/bth_device.c', 'src/tusb.c'],
                                          built={'board': 'b', 'family': 'f', 'status': 'failed', 'firstError': 'x',
                                                 'okExamples': ['cdc_msc']})
        self.assertEqual((rc, printed['pass'], len(printed['uncovered'])), (1, False, 1))

    def test_a_family_pruned_from_the_selection_is_a_gap_not_a_pass(self):
        # _prune_buildable drops a family whose dir is gone, whose boards are unreadable
        # or whose every example is filtered (that one without a reason line); the
        # path that named it must not vanish into a green empty result
        rc, printed, b1 = self._main_scope(['hw/bsp/no_such_family/family.c'])
        b1.assert_not_called()
        self.assertEqual((rc, printed['pass'], printed['nothingToBuild']), (3, False, []))
        self.assertTrue(any(r.startswith('hw/bsp/no_such_family/family.c:') for r in printed['uncovered']), printed)
        reasons = ['hw/bsp/lpc18/family.c: bsp family lpc18',
                   "src/portable/x/y/dcd_y.c: port x/y -> families ['lpc18']",
                   'hw/bsp/stm32f4/family.c: bsp family stm32f4']
        built = [{'family': 'stm32f4', 'okExamples': ['cdc_msc']}]
        benign, gaps = build.coverage(reasons, [r.split(':')[0] for r in reasons], built)
        self.assertEqual((benign, gaps), ([], reasons[:2]))
        # nothing built and no reason line at all: still a gap, never a pass
        _, gaps = build.coverage([], ['src/portable/x/y/dcd_y.c'], [])
        self.assertEqual(gaps, ['src/portable/x/y/dcd_y.c: no build reason from ci_select'])

    def test_full_matrix_builds_a_board_for_a_port_family_the_pair_lacks(self):
        # the representative pair stands in for the matrix on core code, not on a port
        # it does not contain: nordic dcd + tusb.c must compile the nordic port
        rc, printed, b1 = self._main_scope(['src/portable/nordic/nrf5x/dcd_nrf5x.c', 'src/tusb.c'])
        called = [c[0][0] for c in b1.call_args_list]
        self.assertEqual(called[:2], build.FULL_MATRIX_BOARDS)
        self.assertIn('nrf', {build.family_of(b) for b in called})
        self.assertIn('named families', printed['resolution'])
        # a family with no boards dir cannot be added either: no crash, still uncovered
        rc, printed, b1 = self._main_scope(['hw/bsp/no_such_family/family.c', 'src/tusb.c'])
        self.assertEqual(([c[0][0] for c in b1.call_args_list], rc), (build.FULL_MATRIX_BOARDS, 3))
        self.assertTrue(any('no_such_family' in r for r in printed['uncovered']), printed)
        # a port with no family at all cannot be added, so it stays uncovered
        rc, printed, _ = self._main_scope(['src/portable/no_vendor/no_driver/dcd_bogus.c', 'src/tusb.c'])
        self.assertEqual((rc, printed['pass']), (3, False))
        self.assertTrue(any('dcd_bogus' in r for r in printed['uncovered']), printed)

    def test_an_example_no_built_board_wrote_an_elf_for_is_a_gap(self):
        # the family survives for the bsp path while the example is skipped on every
        # resolved board; only the elf inventory can tell
        reasons = ['examples/host/cdc_msc_hid/src/main.c: example host/cdc_msc_hid',
                   'hw/bsp/stm32f4/family.c: bsp family stm32f4',
                   "src/class/cdc/cdc_host.c: class cdc -> ['host/cdc_msc_hid']"]
        scope = [r.split(':')[0] for r in reasons]
        without = [{'family': 'stm32f4', 'okExamples': ['cdc_msc']}]
        self.assertEqual(build.coverage(reasons, scope, without)[1], [reasons[0], reasons[2]])
        with_it = [{'family': 'stm32f4', 'okExamples': ['cdc_msc_hid']}]
        self.assertEqual(build.coverage(reasons, scope, with_it)[1], [])
        # -e or -T: the caller chose what to build, so the narrowing is theirs, not a gap
        self.assertEqual(build.coverage(reasons, scope, without, chosen=True)[1], [])

    def test_espressif_verifies_only_the_example_trees_this_run_attempted(self):
        # one idf tree per example; a shared dir keeps the tree of an example skipped
        # this run, and `cmake --build --target help` does not list idf's <name>.elf
        d = build.ROOT / 'cmake-build' / 'cmake-build-agent-test-espressif_s3_devkitm'
        elfs = [d / 'device' / ex / f'{ex}.elf' for ex in ('cdc_msc_freertos', 'hid_composite_freertos')]
        with mock.patch.object(build.tools_build, 'get_examples',
                               return_value=['device/cdc_msc_freertos', 'device/hid_composite_freertos']), \
             mock.patch.object(build.tools_build.build_utils, 'skip_example',
                               side_effect=lambda e, b, defs: e == 'device/hid_composite_freertos'), \
             mock.patch.object(build.tools_build, 'cmake_registered_targets') as reg:
            got = build.configured('espressif_s3_devkitm', 'espressif', [], [], str(d.relative_to(build.ROOT)), elfs, [])
        self.assertEqual(got, elfs[:1])
        reg.assert_not_called()

    def test_a_green_build_of_nothing_does_not_cover_a_core_path(self):
        # `-T help` succeeds and compiles nothing; core/infra names no example, so the
        # only evidence is that some elf came out
        r = 'src/tusb.c: core/infra -> full build matrix'
        nothing = [{'family': 'stm32f4', 'okExamples': []}]
        self.assertEqual(build.coverage([r], ['src/tusb.c'], nothing)[1], [r])
        self.assertEqual(build.coverage([r], ['src/tusb.c'], nothing, chosen=True)[1], [])
        self.assertEqual(build.coverage([r], ['src/tusb.c'], [{'family': 'stm32f4', 'okExamples': ['cdc_msc']}])[1], [])

    def test_a_membrowse_script_change_needs_its_own_target_not_a_default_sweep(self):
        # examples-membrowse-upload is a plain add_custom_target: `all` never runs
        # tools/membrowse_report.py, so a green sweep is no evidence for it
        r = 'tools/membrowse_report.py: membrowse build-time script -> full build matrix'
        built = [{'family': 'stm32f4', 'okExamples': ['cdc_msc']}]
        gap = (f'{r} (the default sweep builds `all`, which does not run '
               f'examples-membrowse-upload: rerun with -T all -T examples-membrowse-upload)')
        self.assertEqual(build.coverage([r], ['tools/membrowse_report.py'], built)[1], [gap])
        # another -e/-T does not stand in for the target
        self.assertEqual(build.coverage([r], ['tools/membrowse_report.py'], built,
                                        chosen=True, targets=('all',))[1], [gap])
        self.assertEqual(build.coverage([r], ['tools/membrowse_report.py'], built, chosen=True,
                                        targets=('all', 'examples-membrowse-upload'))[1], [])

    def test_a_core_stack_path_needs_a_built_example_of_its_role(self):
        reasons = ['src/host/usbh.c: core host stack', 'src/device/usbd.c: core device stack']
        scope = [r.split(':')[0] for r in reasons]
        device_only = [{'family': 'stm32f4', 'okExamples': ['cdc_msc']}]
        self.assertEqual(build.coverage(reasons, scope, device_only)[1], [reasons[0]])
        both = [{'family': 'stm32f4', 'okExamples': ['cdc_msc', 'cdc_msc_hid']}]
        self.assertEqual(build.coverage(reasons, scope, both)[1], [])
        dual = [{'family': 'stm32f4', 'okExamples': ['host_hid_to_device_cdc']}]
        self.assertEqual(build.coverage(reasons, scope, dual)[1], [])

    def test_get_deps_edit_changing_no_entry_is_nothing_to_build(self):
        r = 'tools/get_deps.py: no dep entry changed, no contribution'
        self.assertEqual(build.coverage([r], ['tools/get_deps.py'], []), ([r], []))

    def test_non_code_paths_beside_code_do_not_fail_the_scope(self):
        rc, printed, b1 = self._main_scope(['docs/index.rst', 'src/tusb.c'])
        self.assertEqual((rc, printed['pass'], printed['uncovered']), (0, True, []))
        self.assertEqual(len(printed['nothingToBuild']), 1)

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

    def test_an_unguarded_nrf_driver_selects_each_compiling_mcu_variant(self):
        path = 'src/portable/nordic/nrf5x/dcd_nrf5x.c'
        reasons = [f"{path}: port nordic/nrf5x -> families ['nrf']"]
        sel = {'build': {'full': False, 'families': ['nrf']}, 'boards': {'nrf52840dk': 'all'}}
        boards, _ = build.boards_for(sel, [path], reasons)
        self.assertEqual(boards, ['nrf52840dk', 'nrf52833dk', 'nrf5340dk'])
        sel['build']['full'] = True
        boards, _ = build.boards_for(sel, [path, 'src/tusb.c'], reasons)
        self.assertEqual(boards, build.FULL_MATRIX_BOARDS + ['nrf52840dk', 'nrf52833dk', 'nrf5340dk'])

    def test_a_guarded_driver_does_not_add_boards_per_mcu_variant(self):
        path = 'src/portable/synopsys/dwc2/dcd_dwc2.c'
        reasons = [f"{path}: port synopsys/dwc2 -> families ['stm32f4']"]
        sel = {'build': {'full': False, 'families': ['stm32f4']}, 'boards': {'stm32f407disco': 'all'}}
        self.assertEqual(build.boards_for(sel, [path], reasons)[0], ['stm32f407disco'])

    def test_variant_selection_keeps_changed_boards_and_the_example_filter(self):
        pool = ['nrf52840dk', 'nrf52840dongle', 'nrf52833dk', 'nrf5340dk', 'nrf54h20dk']
        driver = 'nordic/nrf5x/dcd_nrf5x.c'
        keep = pool[:2]
        self.assertEqual(build.representatives(pool, None, [driver], keep), keep + ['nrf52833dk', 'nrf5340dk'])
        with mock.patch.object(build.tools_build.build_utils, 'skip_example',
                               side_effect=lambda e, b: b != 'nrf52833dk'):
            self.assertEqual(build.representatives(pool, ['device/cdc_msc'], [driver], keep),
                             keep + ['nrf52833dk'])
        with mock.patch.object(build.tools_build.build_utils, 'skip_example', return_value=True):
            self.assertEqual(build.representatives(pool, ['device/cdc_msc'], [driver]),
                             ['nrf52840dk', 'nrf52833dk', 'nrf5340dk'])

    def test_boards_without_mcu_variant_share_one_group(self):
        pool = build.family_boards('rp2040')
        self.assertEqual(build.representatives(pool, None, ['raspberrypi/rp2040/dcd_rp2040.c']), [pool[0]])

    def test_a_directory_scope_expands_to_its_tracked_files(self):
        # a bare board directory matches neither ci_select's rules nor the changed-board
        # rule, and would resolve to the family's sample instead of the board edited
        files = build.expand_scope(['hw/bsp/stm32f4/boards/stm32f411blackpill'])
        self.assertIn('hw/bsp/stm32f4/boards/stm32f411blackpill/board.h', files)
        sel = {'build': {'full': False, 'families': ['stm32f4']}, 'boards': {}}
        self.assertEqual(build.boards_for(sel, files)[0], ['stm32f411blackpill'])

    def test_expansion_includes_a_new_untracked_file(self):
        # scope verification covers uncommitted work: a new board or driver file is untracked
        d = build.ROOT / 'hw' / 'bsp' / 'stm32f4' / 'boards' / 'stm32f411blackpill'
        new = d / 'probe_new_file.h'
        new.write_text('')
        try:
            files = build.expand_scope([str(d.relative_to(build.ROOT))])
        finally:
            new.unlink()
        self.assertIn('hw/bsp/stm32f4/boards/stm32f411blackpill/probe_new_file.h', files)

    def test_a_file_scope_is_left_alone_and_an_empty_directory_is_an_error(self):
        self.assertEqual(build.expand_scope(['src/tusb.c']), ['src/tusb.c'])
        # a path in neither the tree nor the index would classify as the full matrix and
        # come back green; a tracked file deleted from the tree is a real change
        with self.assertRaises(SystemExit) as cm, mock.patch('sys.stdout'), mock.patch.object(sys, 'stderr') as err:
            build.expand_scope(['src/tusb.c', 'no/such/path.c'])
        self.assertEqual(cm.exception.code, 2)
        self.assertIn('no/such/path.c does not exist and is not a tracked file', err.write.call_args[0][0])
        # a deletion counts whether or not it is staged, and a pathspec is not a path
        import os, subprocess, tempfile
        # under a git hook GIT_DIR/GIT_INDEX_FILE point at THIS repository: scrub them, or
        # the temporary repository's commit lands on the branch running the suite
        env = {k: v for k, v in os.environ.items() if not k.startswith('GIT_')}
        with tempfile.TemporaryDirectory() as d, mock.patch.dict(os.environ, env, clear=True):
            root = Path(d)
            git = lambda *args: subprocess.run(['git', '-C', d, *args], check=True, env=env)
            git('init', '-q'); git('config', 'user.email', 't@t'); git('config', 'user.name', 't')
            (root / 'src').mkdir(); (root / 'src' / 'gone.c').write_text(''); (root / 'src' / 'kept.c').write_text('')
            git('add', '.'); git('commit', '-q', '--no-verify', '-m', 'x')
            (root / 'src' / 'gone.c').unlink()
            with mock.patch.object(build, 'ROOT', root):
                self.assertEqual(build.expand_scope(['src/gone.c']), ['src/gone.c'])
                git('rm', '-q', '--cached', 'src/gone.c')
                self.assertEqual(build.expand_scope(['src/gone.c']), ['src/gone.c'])
                with self.assertRaises(SystemExit), mock.patch('sys.stdout'), mock.patch.object(sys, 'stderr'):
                    build.expand_scope(['src/*.c'])
        with mock.patch.object(build.subprocess, 'run', return_value=mock.Mock(returncode=0, stdout='')), \
             mock.patch.object(build.Path, 'is_dir', return_value=True), mock.patch.object(sys, 'stderr'):
            with self.assertRaises(SystemExit) as cm:
                build.expand_scope(['docs'])
        self.assertEqual(cm.exception.code, 2)

    def test_unknown_board_is_an_error(self):
        with self.assertRaises(SystemExit) as cm, mock.patch('sys.stdout') as out:
            build.family_of('no_such_board')
        self.assertEqual(cm.exception.code, 2)
        # exit 2 still ends stdout with a JSON line, so a caller reading only that can quote it
        printed = json.loads(out.write.call_args_list[0][0][0])
        self.assertEqual((printed['pass'], printed['boards']), (False, []))
        self.assertIn('no_such_board', printed['error'])
        with self.assertRaises(SystemExit) as cm, mock.patch('sys.stdout') as out, mock.patch('sys.stderr'):
            build.main(['--scope', 'src/tusb.c', '--base', 'HEAD'])  # a usage error, the same way
        self.assertEqual(cm.exception.code, 2)
        self.assertIn('not allowed with', json.loads(out.write.call_args_list[0][0][0])['error'])


class CatalogTest(unittest.TestCase):
    """Board selection and port coverage read hw/bsp/family.json rows and the
    preprocessed driver body, not the family cmake files."""
    DWC2, FSDEV = 'synopsys/dwc2/dcd_dwc2.c', 'st/stm32_fsdev/dcd_stm32_fsdev.c'
    ROWS = {'stm32l4': {'stm32l476disco': {'cmake': {'portable': [DWC2]}},
                        'stm32l412nucleo': {'cmake': {'portable': [FSDEV]}},
                        'stm32l4r5nucleo': {'cmake': {'portable': [DWC2]}}}}
    IPS = {'stm32l476disco': frozenset({'TUP_USBIP_DWC2'}), 'stm32l412nucleo': frozenset({'TUP_USBIP_FSDEV'}),
           'stm32l4r5nucleo': None}

    def setUp(self):
        for name, value in (('catalog', lambda: self.ROWS), ('family_of', lambda b: 'stm32l4'),
                            ('board_usbips', lambda b: self.IPS[b]),
                            ('source_usbips', lambda src: frozenset({'TUP_USBIP_DWC2'} if 'dwc2' in src else {'TUP_USBIP_FSDEV'}))):
            patcher = mock.patch.object(build, name, value)
            patcher.start()
            self.addCleanup(patcher.stop)

    def test_selection_adds_a_board_per_changed_driver_its_row_compiles(self):
        pool = ['stm32l476disco', 'stm32l412nucleo']
        # one board per driver: the dwc2 pick cannot stand in for fsdev
        self.assertEqual(build.representatives(pool, None, {self.DWC2, self.FSDEV}), ['stm32l412nucleo', 'stm32l476disco'])
        # a kept board that compiles the driver needs no second pick
        self.assertEqual(build.representatives(pool, None, {self.DWC2}, keep=['stm32l476disco']), ['stm32l476disco'])
        # a driver no row lists adds nothing: coverage() reports the gap after the build
        self.assertEqual(build.representatives(pool, None, {'analog/max3421/hcd_max3421.c'}), ['stm32l476disco'])
        # a row whose USB-IP probe cannot say (no host cc, SDK header) counts by its portable list
        self.assertEqual(build.representatives(['stm32l4r5nucleo', 'stm32l412nucleo'], None, {self.DWC2}), ['stm32l4r5nucleo'])
        # a listed driver whose guard needs an IP the board does not select is not compiled there
        self.ROWS['stm32l4']['stm32l412nucleo']['cmake']['portable'].append(self.DWC2)
        try:
            self.assertFalse(build.compiles('stm32l412nucleo', self.DWC2))
        finally:
            self.ROWS['stm32l4']['stm32l412nucleo']['cmake']['portable'].remove(self.DWC2)

    def test_port_gap_is_the_preprocessed_body_of_an_instance_this_run_built(self):
        import tempfile
        driver = 'src/portable/nordic/nrf5x/dcd_nrf5x.c'
        reason = f"{driver}: port nordic/nrf5x -> families ['nrf']"
        with tempfile.TemporaryDirectory() as d:
            src = str(build.ROOT / driver)
            entries = [{'directory': d, 'file': src, 'command': f'gcc -o {ex}/x.o -c {src}',
                        'output': f'{d}/device/{ex}/CMakeFiles/x.dir/dcd_nrf5x.c.o'} for ex in ('cdc_msc', 'hid')]
            Path(d, 'compile_commands.json').write_text(json.dumps(entries))
            result = {'board': 'nrf52840dk', 'family': 'nrf', 'buildDir': d, 'okExamples': ['cdc_msc']}
            with mock.patch.object(build, 'source_lines', return_value=0):
                self.assertEqual(build.port_gap(reason, [result]),
                                 'nordic/nrf5x/dcd_nrf5x.c: nrf52840dk body preprocessed away')
            with mock.patch.object(build, 'source_lines', return_value=7) as lines:
                self.assertIsNone(build.port_gap(reason, [result]))
                # only the instance of an example this run built is preprocessed
                self.assertEqual([c[0][0]['command'] for c in lines.call_args_list], [entries[0]['command']])
            with mock.patch.object(build, 'source_lines', return_value=None):
                self.assertIn('unverified: preprocessing failed', build.port_gap(reason, [result]))
            self.assertIn('compiled only in examples this run did not build',
                          build.port_gap(reason, [dict(result, okExamples=['msc_dual_lun'])]))
            Path(d, 'compile_commands.json').write_text('[]')
            self.assertIn('not compiled in its default configuration', build.port_gap(reason, [result]))
            Path(d, 'compile_commands.json').unlink()
            self.assertIn('unverified: no compile database', build.port_gap(reason, [result]))
        # a family with no built board, a deleted driver, a non-port path
        self.assertIn('no board of its family built', build.port_gap(reason, [{'board': 'x', 'family': 'stm32f4', 'buildDir': '/nowhere'}]))
        self.assertIsNone(build.port_gap("src/portable/x/y/dcd_gone.c: port x/y -> families ['nrf']", []))
        self.assertIsNone(build.port_gap('hw/bsp/nrf/family.c: bsp family nrf', []))

    def test_source_lines_counts_the_files_own_non_directive_lines(self):
        import tempfile
        with tempfile.TemporaryDirectory() as d:
            entry = {'directory': d, 'file': 'dcd.c', 'command': 'gcc -DX=1 -o x.o -c dcd.c'}
            out = f'# 1 "dcd.c"\n\nint a;\n# 1 "tusb.h"\nint from_header;\n# 3 "dcd.c" 2\n#pragma once\nint b;\n'
            with mock.patch.object(build.subprocess, 'run', return_value=mock.Mock(returncode=0, stdout=out)) as run:
                self.assertEqual(build.source_lines(entry), 2)
            self.assertEqual(run.call_args[0][0], ['gcc', '-DX=1', '-E', 'dcd.c'])
            with mock.patch.object(build.subprocess, 'run', return_value=mock.Mock(returncode=1, stdout='')):
                self.assertIsNone(build.source_lines(entry))


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
            (d / 'CMakeCache.txt').write_text('')
            with mock.patch.object(build, 'run', fake_run), mock.patch.object(build, 'missing_deps', return_value=[]), \
                 mock.patch.object(build.os, 'getpid', return_value='test'), \
                 mock.patch.object(build.tools_build, 'cmake_registered_targets', return_value={'new', 'old'}) as reg:
                r = build.build_one('stm32f407disco', [], [], [], [], False, False, False)
            # green: the stale elf is up to date, so verified, while its target is configured
            self.assertEqual((r['built'], r['okExamples']), (1, ['new', 'old']))
            self.assertEqual(reg.call_args[0][0], str(d))
            with mock.patch.object(build, 'run', fake_run), mock.patch.object(build, 'missing_deps', return_value=[]), \
                 mock.patch.object(build.os, 'getpid', return_value='test'), \
                 mock.patch.object(build.tools_build, 'cmake_registered_targets', return_value={'new'}):
                r = build.build_one('stm32f407disco', [], [], [], [], False, False, False)
            self.assertEqual(r['okExamples'], ['new'])  # 'old' was configured away
        finally:
            shutil.rmtree(d)
        d.mkdir(parents=True); stale.write_bytes(b''); os.utime(stale, (1, 1))
        try:
            def fake_fail(cmd, verbose):
                time.sleep(0.01); fresh.write_bytes(b''); return 1, row('stm32f407disco', 'all', FAILED)
            with mock.patch.object(build, 'run', fake_fail), mock.patch.object(build, 'missing_deps', return_value=[]), \
                 mock.patch.object(build.os, 'getpid', return_value='test'):
                r = build.build_one('stm32f407disco', [], [], [], [], False, False, False)
        finally:
            shutil.rmtree(d)
        self.assertEqual((r['status'], r['okExamples']), ('failed', ['new']))  # failed: only what was written

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

    def test_a_define_on_a_build_owned_key_is_refused(self):
        # tools/build.py passes -DBOARD first, so a caller's would win and the artifacts
        # would be named after the board that was asked for, not the one built
        for define in ['BOARD=raspberry_pi_pico', 'CMAKE_BUILD_TYPE:STRING=Debug', 'TOOLCHAIN=clang']:
            with mock.patch.object(build, 'run') as run, mock.patch.object(sys, 'stderr') as err, mock.patch('sys.stdout'):
                with self.assertRaises(SystemExit) as cm:
                    build.build_one('stm32f407disco', [], [], [define], [], False, False, False)
            self.assertEqual(cm.exception.code, 2)
            self.assertIn(f'-D {define.partition("=")[0].partition(":")[0]}', err.write.call_args[0][0])
            run.assert_not_called()

    def test_recorded_options_normalise_a_legacy_typed_entry(self):
        import shutil
        d = build.ROOT / 'cmake-build' / 'cmake-build-agent-test-typed'
        shutil.rmtree(d, ignore_errors=True); d.mkdir(parents=True)
        try:
            (d / build.AGENT_DEFINES).write_text('["LOG:STRING=2", "RHPORT_DEVICE"]\n')
            self.assertEqual(build.recorded_options(str(d.relative_to(build.ROOT))), {'LOG', 'RHPORT_DEVICE'})
        finally:
            shutil.rmtree(d)

    def test_a_define_on_an_espressif_board_is_refused_not_dropped(self):
        esp = build.family_boards('espressif')[0]
        with mock.patch.object(build, 'run') as run, mock.patch.object(sys, 'stderr') as err, mock.patch('sys.stdout'):
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

    def test_a_named_build_in_the_shared_tree_is_its_own_dir(self):
        import tempfile
        with tempfile.TemporaryDirectory() as root, mock.patch.object(build, 'ROOT', Path(root)), \
             mock.patch.object(build, 'family_of', return_value='stm32f7'), \
             mock.patch.object(build, 'run', return_value=(0, row('stm32f723disco', 'all', OK))) as run, \
             mock.patch.object(build, 'missing_deps', return_value=[]), \
             mock.patch.object(build.tools_build, 'cmake_registered_targets', return_value=None):
            r = build.build_one('stm32f723disco', [], [], [], [], True, False, False, 'stm32f723disco-DMA')
        cmd = run.call_args[0][0]
        self.assertEqual(cmd[cmd.index('--build-name') + 1], 'stm32f723disco-DMA')
        self.assertEqual(r['buildDir'], 'cmake-build/cmake-build-stm32f723disco-DMA')


class VariantsTest(unittest.TestCase):
    ROSTER = {'boards': [
        {'name': 'plain'},
        {'name': 'pico', 'variant': [{'name': 'pico', 'flags': '-DA=1 -DB=2'}]},
        {'name': 'ch', 'variant': [{'name': 'ch-fs', 'defines': ['RHPORT_DEVICE=0']},
                                   {'name': 'ch-hs', 'defines': ['RHPORT_DEVICE=1'], 'flags': ''}]},
    ]}

    def setUp(self):
        import tempfile
        d = tempfile.TemporaryDirectory()
        self.addCleanup(d.cleanup)
        self.config = Path(d.name) / 'rig.json'
        self.config.write_text(json.dumps(self.ROSTER))

    def main(self, *args):
        ok = {'board': 'b', 'family': 'f', 'buildDir': 'd', 'status': 'ok', 'firstError': ''}
        with mock.patch.object(build, 'build_one', return_value=ok) as b1, mock.patch('sys.stdout'):
            rc = build.main(list(args))
        return rc, [(c.args[0], c.args[3], c.args[4], c.args[8]) for c in b1.call_args_list]

    def test_each_variant_builds_with_its_name_defines_and_flags(self):
        rc, calls = self.main('--board', 'plain', '--board', 'pico', '--board', 'ch', '--shared',
                              '--variants', str(self.config), '-D', 'LOG=2', '--cflag=-DU=1')
        self.assertEqual(rc, 0)
        self.assertEqual(calls, [
            ('plain', ['LOG=2'], ['-DU=1'], 'plain'),
            ('pico', ['LOG=2'], ['-DU=1', '-DA=1', '-DB=2'], 'pico'),
            ('ch', ['LOG=2', 'RHPORT_DEVICE=0'], ['-DU=1'], 'ch-fs'),
            ('ch', ['LOG=2', 'RHPORT_DEVICE=1'], ['-DU=1'], 'ch-hs'),
        ])

    def test_a_malformed_variant_is_a_resolution_error(self):
        for variant in [[{'name': 'v', 'flags': None}], [{'flags': '-DA=1'}], [{'name': 'v', 'defines': 'X=1'}],
                        [{'name': ''}], ['x'], [None], {'name': 'v'}, 'v', 5]:
            self.config.write_text(json.dumps({'boards': [{'name': 'b', 'variant': variant}]}))
            with mock.patch.object(build, 'build_one') as b1, mock.patch('sys.stdout') as out, \
                 mock.patch.object(sys, 'stderr'), self.assertRaises(SystemExit) as cm:
                build.main(['--board', 'b', '--shared', '--variants', str(self.config)])
            self.assertEqual(cm.exception.code, 2, variant)
            last = json.loads(''.join(c.args[0] for c in out.write.call_args_list).splitlines()[-1])
            self.assertIn('board b variant', last['error'])
            b1.assert_not_called()

    def test_variants_match_the_ci_matrix_builds_of_every_rig_roster(self):
        import shlex
        import subprocess
        for config in sorted((build.ROOT / 'test' / 'hil').glob('*.json')):
            out = subprocess.run([sys.executable, str(build.ROOT / '.github/scripts/hil_ci_set_matrix.py'), str(config)],
                                 capture_output=True, text=True, check=True).stdout
            ci = set()
            for arg in (a for bucket in json.loads(out).values() for a in bucket):
                argv = shlex.split(arg)
                board = argv[argv.index('-b') + 1]
                name = argv[argv.index('--build-name') + 1] if '--build-name' in argv else board
                ci.add((board, name, tuple(a[2:] for a in argv if a.startswith('-D')),
                        tuple(a.partition('=')[2] for a in argv if a.startswith('--cflag='))))
            boards = [b['name'] for b in json.loads(config.read_text())['boards']]
            ours = {(b, n, tuple(d), tuple(f)) for b, n, d, f in build.roster_variants(boards, config)}
            self.assertEqual(ours, ci, config.name)

    def test_refused_without_board_and_shared_or_for_a_board_off_the_roster(self):
        for args, text in [(['--board', 'pico', '--variants', 'x'], '--variants needs --board and --shared'),
                           (['--scope', 'src', '--shared', '--variants', 'x'], '--variants needs --board and --shared'),
                           (['--board', 'nope', '--shared', '--variants', str(self.config)], 'not in '),
                           (['--board', 'pico', '--shared', '--variants', '/nonexistent.json'], 'could not read'),
                           (['--board', 'pico', '--shared', '--variants', ''], 'could not read')]:
            with mock.patch.object(build, 'build_one') as b1, mock.patch('sys.stdout'), \
                 mock.patch.object(sys, 'stderr') as err, self.assertRaises(SystemExit) as cm:
                build.main(args)
            self.assertEqual(cm.exception.code, 2)
            self.assertIn(text, err.write.call_args[0][0])
            b1.assert_not_called()

    def test_a_variant_dir_cached_with_flags_the_variant_lacks_is_refused(self):
        root = self.config.parent
        d = root / 'cmake-build' / 'cmake-build-stm32f723disco-DMA'
        d.mkdir(parents=True)
        (d / 'CMakeCache.txt').write_text('CFLAGS_CLI:UNINITIALIZED=-DCFG_TUD_DWC2_DMA_ENABLE=1\n')
        with mock.patch.object(build, 'ROOT', root), mock.patch.object(build, 'family_of', return_value='stm32f7'), \
             mock.patch.object(build, 'run') as run, \
             mock.patch.object(build, 'missing_deps', return_value=[]), \
             mock.patch.object(sys, 'stderr') as err, mock.patch('sys.stdout'), self.assertRaises(SystemExit):
            build.build_one('stm32f723disco', [], [], [], [], True, False, False, 'stm32f723disco-DMA')
        self.assertIn('CFLAGS_CLI=-DCFG_TUD_DWC2_DMA_ENABLE=1', err.write.call_args[0][0])
        run.assert_not_called()


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

    def test_a_row_is_written_only_from_a_fresh_default_configure(self):
        tb = build.tools_build
        # the runner's own compiler flags would answer for canonical_row before the case does
        env = mock.patch.dict(os.environ, {'CFLAGS': '', 'CXXFLAGS': '', 'ASMFLAGS': ''})
        env.start()
        self.addCleanup(env.stop)
        with mock.patch.object(tb.build_utils, 'missing_deps', return_value=[]), \
             mock.patch.object(tb.family_json, 'update', return_value='family.json: updated b') as update:
            self.assertIn('cmake-build/x existed before this configure', tb.canonical_row('f', ['cmake-build/x'], ['cmake-build/x']))
            update.assert_not_called()
            self.assertEqual(tb.canonical_row('f', ['cmake-build/x'], []), 'family.json: updated b')
        with mock.patch.object(tb.build_utils, 'missing_deps', return_value=['hw/mcu/nordic/nrfx']):
            self.assertIn('deps not at their pins', tb.canonical_row('nrf', ['cmake-build/x'], []))
        # cmake seeds the compile flags from these, so the configure is not the default one
        with mock.patch.object(tb.build_utils, 'missing_deps', return_value=[]), \
             mock.patch.object(tb.family_json, 'update') as update, \
             mock.patch.dict(os.environ, {'CFLAGS': '-DSTM32L476xx'}):
            self.assertIn('CFLAGS set in the environment', tb.canonical_row('f', ['cmake-build/x'], []))
            update.assert_not_called()
        with mock.patch.object(tb.build_utils, 'missing_deps', return_value=[]), \
             mock.patch.object(tb.family_json, 'update', return_value='family.json: updated b'), \
             mock.patch.dict(os.environ, {'CFLAGS': '  '}):
            self.assertEqual(tb.canonical_row('f', ['cmake-build/x'], []), 'family.json: updated b', 'blank is unset')

    def test_family_deps_come_from_get_deps_table(self):
        self.assertIn('hw/mcu/nordic/nrfx', [d for d, e in build.tools_build.build_utils.get_deps.deps_optional.items() if 'nrf' in e[2].split()])


class MainTest(unittest.TestCase):
    def test_main_prints_json_and_exit_reflects_pass(self):
        results = [{'board': 'b', 'family': 'f', 'buildDir': 'd', 'status': 'ok', 'firstError': ''}]
        with mock.patch.object(build, 'build_one', return_value=results[0]), \
             mock.patch('sys.stdout') as out:
            self.assertEqual(build.main(['--board', 'b']), 0)
        printed = json.loads(out.write.call_args_list[0][0][0])
        self.assertEqual(printed, {'pass': True, 'boards': results, 'resolution': 'named boards', 'familyJsonChanged': False})
        results[0]['status'] = 'failed'
        with mock.patch.object(build, 'build_one', return_value=results[0]), mock.patch('sys.stdout'):
            self.assertEqual(build.main(['--board', 'b']), 1)


UTILS = Path(__file__).resolve().parents[2] / 'tools' / 'build_utils.py'
_uspec = importlib.util.spec_from_file_location('build_utils_under_test', UTILS)
utils = importlib.util.module_from_spec(_uspec)
_uspec.loader.exec_module(utils)


class BoardInfoTest(unittest.TestCase):
    """board-info: the J-Link device and reference project of a board, refusing
    every definition the textual cmake parser would otherwise have to guess at."""

    def setUp(self):
        import os
        import tempfile
        self._dir = tempfile.TemporaryDirectory()
        self.addCleanup(self._dir.cleanup)
        self.addCleanup(os.chdir, os.getcwd())
        os.chdir(self._dir.name)

    def board(self, name, board_cmake, family='fam', family_cmake='', jdebugs=()):
        d = Path('hw/bsp') / family / 'boards' / name
        d.mkdir(parents=True)
        (d / 'board.cmake').write_text(board_cmake)
        (d.parent.parent / 'family.cmake').write_text(family_cmake)
        for j in jdebugs:
            (d / 'ozone').mkdir(exist_ok=True)
            (d / 'ozone' / j).write_text('Project.SetDevice ("X");\n')

    def test_literal_and_expanded_definitions(self):
        self.board('literal', 'set(JLINK_DEVICE stm32h743xi)\n')
        self.board('variant', 'set(MCU_VARIANT MK64FN1M0)\nset(JLINK_DEVICE ${MCU_VARIANT}xxx12)\n')
        self.board('fromfamily', 'set(MAX_DEVICE max32650)\n', family='maxim',
                   family_cmake='set(JLINK_DEVICE ${MAX_DEVICE})\n')
        self.board('upper', 'SET(MCU_VARIANT nrf52840)\nSET(JLINK_DEVICE ${MCU_VARIANT}_xxaa)\n')
        self.assertEqual(utils.board_jlink('literal'), 'stm32h743xi')
        self.assertEqual(utils.board_jlink('variant'), 'MK64FN1M0xxx12')
        self.assertEqual(utils.board_jlink('fromfamily'), 'max32650')
        self.assertEqual(utils.board_jlink('upper'), 'nrf52840_xxaa')

    def test_conditional_definitions_are_refused_with_the_candidates(self):
        # rp2040's family.cmake picks JLINK_DEVICE by PICO_PLATFORM
        self.board('pico', '', family='rp2040',
                   family_cmake='if (A)\n  set(JLINK_DEVICE rp2040_m0_0)\nelse ()\n  set(JLINK_DEVICE rp2350_m33_0)\nendif ()\n')
        with self.assertRaises(utils.BoardInfoError) as cm:
            utils.board_jlink('pico')
        self.assertIn('rp2040_m0_0, rp2350_m33_0', str(cm.exception))
        # mimxrt1170_evkb reaches JLINK_CORE, set per core, while expanding
        self.board('rt1170', 'set(MCU_VARIANT MIMXRT1176)\nif (M4 STREQUAL "1")\n  set(JLINK_CORE _M4)\n'
                             'else ()\n  set(JLINK_CORE _M7)\nendif()\nset(JLINK_DEVICE ${MCU_VARIANT}xxxxA${JLINK_CORE})\n')
        with self.assertRaises(utils.BoardInfoError) as cm:
            utils.board_jlink('rt1170')
        self.assertIn('_M4, _M7', str(cm.exception))

    def test_reference_project_settles_a_conditional_device(self):
        # pico2_etm_trace: the rp2040 family picks JLINK_DEVICE by PICO_PLATFORM, but
        # the board's reference Ozone project names the device the capture uses
        fam = 'if (A)\n  set(JLINK_DEVICE rp2040_m0_0)\nelse ()\n  set(JLINK_DEVICE rp2350_m33_0)\nendif ()\n'
        self.board('carrier', '', family='rp2040', family_cmake=fam, jdebugs=('rp2350.jdebug',))
        ref = Path('hw/bsp/rp2040/boards/carrier/ozone/rp2350.jdebug')
        ref.write_text('void OnProjectLoad (void) {\n  // Project.SetDevice ("OLD");\n  Project.SetDevice ("RP2350_M33_0");\n}\n')
        self.assertEqual(utils.board_jlink('carrier'), 'RP2350_M33_0')
        ref.write_text('void OnProjectLoad (void) {\n}\n')
        with self.assertRaises(utils.BoardInfoError) as cm:
            utils.board_jlink('carrier')
        self.assertIn('names no device either', str(cm.exception))

    def test_unresolvable_or_missing_definitions_are_refused(self):
        self.board('nope', 'set(JLINK_DEVICE ${UNSET_ANYWHERE})\n')
        self.board('none', 'set(MCU_VARIANT x)\n')
        self.board('dup', 'set(JLINK_DEVICE d)\n', family='fam2')
        self.board('dup', 'set(JLINK_DEVICE d)\n', family='fam3')
        for name, want in (('nope', 'UNSET_ANYWHERE'), ('none', 'no JLINK_DEVICE'), ('absent', 'unknown board'),
                           ('no*', 'not a board name'), ('dup', 'several families')):
            with self.assertRaises(utils.BoardInfoError) as cm:
                utils.board_jlink(name)
            self.assertIn(want, str(cm.exception))

    def test_reference_project_is_the_sole_jdebug_or_none(self):
        self.board('one', 'set(JLINK_DEVICE d)\n', jdebugs=('ref.jdebug',))
        self.board('zero', 'set(JLINK_DEVICE d)\n')
        self.board('two', 'set(JLINK_DEVICE d)\n', jdebugs=('a.jdebug', 'b.jdebug'))
        self.assertEqual(utils.board_jdebug('one'), 'hw/bsp/fam/boards/one/ozone/ref.jdebug')
        self.assertIsNone(utils.board_jdebug('zero'))
        with self.assertRaises(utils.BoardInfoError):
            utils.board_jdebug('two')

    def test_cli_prints_shell_assignments_or_the_refusal(self):
        self.board('b', 'set(JLINK_DEVICE stm32h743xi)\n', jdebugs=('ref.jdebug',))
        with mock.patch('sys.stdout') as out:
            rc = utils.main(['board-info', 'b'])
        self.assertEqual(rc, 0)
        self.assertEqual(''.join(c[0][0] for c in out.write.call_args_list),
                         'JLINK_DEVICE=stm32h743xi\nJDEBUG=hw/bsp/fam/boards/b/ozone/ref.jdebug\n')
        with mock.patch('sys.stderr') as err:
            rc = utils.main(['board-info', 'missing'])
        self.assertEqual(rc, 1)
        self.assertIn('unknown board', ''.join(c[0][0] for c in err.write.call_args_list))


if __name__ == '__main__':
    unittest.main()
