"""tools/family_json.py: the join of a configure's outputs into a hw/bsp/family.json row,
the probes that validate it, the merge that writes it, and the check pre-commit runs.
No cmake here: the build dirs are fixtures. The last test is the check itself over the
real tree, which is what the pre-commit hook guarantees."""
import contextlib
import io
import json
import os
import pathlib
import sys
import tempfile
import unittest
from unittest import mock

ROOT = pathlib.Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / 'tools'))
import family_json as fj  # noqa: E402

HEADER = '''
#if TU_CHECK_MCU(OPT_MCU_LPC54)
  #if !defined(LPC54114_cm4_SERIES) && !defined(LPC54114_cm0plus_SERIES)
    #define TUP_USBIP_OHCI
  #endif
#elif defined(STM32F105x8) || defined(STM32F105xB) || \\
      defined(STM32F107xB)
  #define TUP_USBIP_DWC2
#endif
#ifdef CORE_CM4
#endif
#ifndef CFG_TUD_WCH_USBIP_USBFS
  #define CFG_TUD_WCH_USBIP_USBFS 0
#endif
#if CFG_TUSB_MCU == OPT_MCU_NRF5X && 1
#endif
'''


def entry(directory, file, defines, compiler='arm-none-eabi-gcc', extra=()):
    d = ' '.join(f'-D{k}' if v is None else f'-D{k}={v}' for k, v in defines.items())
    return {'directory': str(directory), 'file': file,
            'command': f'{compiler} {d} -Isrc {" ".join(extra)} -MD -MF x.d -o x.obj -c {file}',
            'output': 'x.obj'}


def fixture_build(root, name, board='stm32f407disco', family='stm32f4', mcus=('STM32F4',),
                  options=None, entries=None):
    d = root / 'cmake-build' / name
    d.mkdir(parents=True, exist_ok=True)
    part = {'board': board, 'family': family, 'family_mcus': list(mcus)}
    if options is not None:
        part['options'] = options
    (d / fj.PART).write_text(json.dumps(part))
    src = root / 'src'
    if entries is None:
        entries = [
            entry(d, str(src / 'tusb.c'), {'CFG_TUSB_MCU': 'OPT_MCU_STM32F4', 'STM32F407xx': None, 'NDEBUG': None}),
            entry(d, str(src / 'portable/synopsys/dwc2/dcd_dwc2.c'), {'CFG_TUSB_MCU': 'OPT_MCU_STM32F4', 'STM32F407xx': None}),
            entry(d, str(root / 'hw/bsp/board.c'), {'CFG_TUSB_MCU': 'OPT_MCU_NRF5X'}),  # not a TinyUSB TU
        ]
    (d / 'compile_commands.json').write_text(json.dumps(entries))
    return d


def fixture_root(d):
    """A repo skeleton: tusb_mcu.h keying on STM32F407xx, one portable driver."""
    root = pathlib.Path(d)
    (root / 'src' / 'common').mkdir(parents=True)
    (root / 'src' / 'common' / 'tusb_mcu.h').write_text('#if defined(STM32F407xx) || defined(LPC54114_cm4_SERIES)\n#endif\n')
    (root / 'src' / 'tusb_option.h').write_text('#define OPT_MCU_STM32F4 300\n#define OPT_MCU_NRF5X 800\n')
    (root / 'src' / 'portable' / 'synopsys' / 'dwc2').mkdir(parents=True)
    for f in ('dcd_dwc2.c', 'hcd_dwc2.c'):
        (root / 'src' / 'portable' / 'synopsys' / 'dwc2' / f).write_text('')
    (root / 'src' / 'tusb.c').write_text('')
    (root / 'hw' / 'bsp' / 'stm32f4' / 'boards' / 'stm32f407disco').mkdir(parents=True)
    (root / 'hw' / 'bsp' / 'stm32f4' / 'family.cmake').write_text('')
    # examples/<group>/CMakeLists.txt is what makes a directory a group, as it is what
    # examples/CMakeLists.txt adds; build_system has none and is not one.
    for group, names in (('device', ('cdc_msc', 'board_test')), ('dual', ('host_hid_to_device_cdc',)),
                         ('host', ('cdc_msc_hid',)), ('typec', ('power_delivery',))):
        (root / 'examples' / group).mkdir(parents=True)
        (root / 'examples' / group / 'CMakeLists.txt').write_text('')
        for n in names:
            (root / 'examples' / group / n / 'src').mkdir(parents=True)
    (root / 'examples' / 'build_system' / 'cmake').mkdir(parents=True)
    return root


def example_entry(directory, root, group, name, file='src/main.c', relative=False):
    """A compile command for one example's own source, as the configure records it."""
    path = root / 'examples' / group / name / file
    spelled = os.path.relpath(path, directory) if relative else str(path)
    return entry(directory, spelled, {'CFG_TUSB_MCU': 'OPT_MCU_STM32F4'})


ROW = {'defines': {'STM32F407xx': '1'}, 'family_mcus': ['STM32F4'], 'mcu': 'OPT_MCU_STM32F4',
       'options': {}, 'portable': ['synopsys/dwc2/dcd_dwc2.c'], 'roles': []}


class Extract(unittest.TestCase):
    def test_tested_identifiers_are_the_names_tusb_mcu_h_branches_on(self):
        names = fj.tested_identifiers(HEADER)
        self.assertEqual(names, {'TU_CHECK_MCU', 'LPC54114_cm4_SERIES', 'LPC54114_cm0plus_SERIES',
                                 'STM32F105x8', 'STM32F105xB', 'STM32F107xB', 'CORE_CM4', 'CFG_TUD_WCH_USBIP_USBFS'})

    def test_preprocess_argv_keeps_the_context_and_drops_the_outputs(self):
        with tempfile.TemporaryDirectory() as d:
            e = entry(d, 'src/tusb.c', {'CFG_TUSB_MCU': 'OPT_MCU_STM32F4', 'X': None}, extra=('-mthumb', '@resp.rsp'))
            (pathlib.Path(d) / 'resp.rsp').write_text('-Wall -DFROM_RSP\n')
            argv = fj.preprocess_argv(e)
        self.assertEqual(argv, ['arm-none-eabi-gcc', '-DCFG_TUSB_MCU=OPT_MCU_STM32F4', '-DX', '-Isrc', '-mthumb',
                                '-Wall', '-DFROM_RSP'])
        self.assertEqual(fj.defines_of(argv), {'CFG_TUSB_MCU': 'OPT_MCU_STM32F4', 'X': '1', 'FROM_RSP': '1'})
        args = {'directory': d, 'file': 'a.c', 'arguments': ['cc', '-D', 'A=2', '-o', 'a.o', '-c', 'a.c'], 'output': 'a.o'}
        self.assertEqual(fj.preprocess_argv(args), ['cc', '-D', 'A=2'])
        self.assertEqual(fj.defines_of(fj.preprocess_argv(args)), {'A': '2'})


class Observe(unittest.TestCase):
    def test_a_configured_dir_yields_the_row(self):
        with tempfile.TemporaryDirectory() as d:
            root = fixture_root(d)
            b = fixture_build(root, 'x')
            family, board, row, entries = fj.observe(b, root)
        self.assertEqual((family, board), ('stm32f4', 'stm32f407disco'))
        self.assertEqual(row, ROW)
        self.assertEqual(len(entries), 2, 'hw/bsp/board.c is not a TinyUSB translation unit')

    def test_disagreeing_translation_units_are_a_failure_naming_both(self):
        with tempfile.TemporaryDirectory() as d:
            root = fixture_root(d)
            src = root / 'src'
            for defines, marker in (
                ([{'CFG_TUSB_MCU': 'OPT_MCU_STM32F4'}, {'CFG_TUSB_MCU': 'OPT_MCU_NRF5X'}], 'OPT_MCU_STM32F4 and OPT_MCU_NRF5X'),
                ([{'CFG_TUSB_MCU': 'OPT_MCU_STM32F4', 'STM32F407xx': '1'}, {'CFG_TUSB_MCU': 'OPT_MCU_STM32F4', 'STM32F407xx': '2'}], 'STM32F407xx differs'),
                ([{'X': None}], 'without CFG_TUSB_MCU'),
            ):
                b = fixture_build(root, 'y', entries=[entry(root, str(src / f'{i}.c'), ds) for i, ds in enumerate(defines)])
                with self.assertRaises(fj.Failure) as cm:
                    fj.observe(b, root)
                self.assertIn(marker, str(cm.exception))
            b = fixture_build(root, 'z', entries=[])
            with self.assertRaises(fj.Failure) as cm:
                fj.observe(b, root)
            self.assertIn('no TinyUSB translation unit', str(cm.exception))
            (b / fj.PART).unlink()
            with self.assertRaises(fj.Failure) as cm:
                fj.observe(b, root)
            self.assertIn(fj.PART, str(cm.exception))


class Roles(unittest.TestCase):
    def groups_of(self, root, build, extra):
        b = fixture_build(root, build, entries=[
            entry(b_dir := root / 'cmake-build' / build, str(root / 'src' / 'tusb.c'), {'CFG_TUSB_MCU': 'OPT_MCU_STM32F4'}),
            *extra(b_dir),
        ])
        return fj.observe(b, root)[2]['roles']

    def test_roles_are_the_groups_whose_example_sources_the_configure_compiled(self):
        with tempfile.TemporaryDirectory() as d:
            root = fixture_root(d)
            cases = {
                'device only': (lambda b: [example_entry(b, root, 'device', 'cdc_msc')], ['device']),
                'host only': (lambda b: [example_entry(b, root, 'host', 'cdc_msc_hid')], ['host']),
                'dual stays dual': (lambda b: [example_entry(b, root, 'dual', 'host_hid_to_device_cdc')], ['dual']),
                'every group, sorted': (lambda b: [example_entry(b, root, g, n) for g, n in
                                                   (('host', 'cdc_msc_hid'), ('device', 'cdc_msc'),
                                                    ('typec', 'power_delivery'), ('dual', 'host_hid_to_device_cdc'))],
                                        ['device', 'dual', 'host', 'typec']),
                'two units of one example are one role': (
                    lambda b: [example_entry(b, root, 'device', 'cdc_msc'),
                               example_entry(b, root, 'device', 'cdc_msc', file='src/usb_descriptors.c')], ['device']),
                'none configured': (lambda b: [], []),
            }
            for name, (extra, expected) in cases.items():
                self.assertEqual(self.groups_of(root, f'r{abs(hash(name))}', extra), expected, name)

    def test_a_path_that_is_not_an_examples_group_example_source_is_no_role(self):
        with tempfile.TemporaryDirectory() as d:
            root = fixture_root(d)
            (root / 'examples' / 'CMakeLists.txt').write_text('')  # the tree's own file, not a group
            for name, extra in {
                'a group dir with no CMakeLists.txt': lambda b: [example_entry(b, root, 'build_system', 'cmake')],
                'a file directly under the group': lambda b: [entry(b, str(root / 'examples' / 'device' / 'x.c'), {'CFG_TUSB_MCU': 'OPT_MCU_STM32F4'})],
                'an example dir that does not exist': lambda b: [example_entry(b, root, 'device', 'gone')],
            }.items():
                self.assertEqual(self.groups_of(root, f'n{abs(hash(name))}', extra), [], name)

    def test_a_relative_file_path_is_resolved_against_its_directory(self):
        with tempfile.TemporaryDirectory() as d:
            root = fixture_root(d)
            spelled = example_entry(root / 'cmake-build' / 'rel', root, 'device', 'cdc_msc', relative=True)['file']
            self.assertFalse(os.path.isabs(spelled), 'the fixture must spell the file relative, as a real database may')
            roles = self.groups_of(root, 'rel', lambda b: [example_entry(b, root, 'device', 'cdc_msc', relative=True)])
            self.assertEqual(roles, ['device'], 'compile_commands.json may spell file relative to directory')

    def test_join_unions_roles_over_the_espressif_example_trees(self):
        with tempfile.TemporaryDirectory() as d:
            root = fixture_root(d)
            dirs = []
            for group, name in (('device', 'cdc_msc'), ('host', 'cdc_msc_hid')):
                b = root / 'cmake-build' / f'esp-{group}'
                b.mkdir(parents=True)
                dirs.append(fixture_build(root, f'esp-{group}', entries=[
                    entry(b, str(root / 'src' / 'tusb.c'), {'CFG_TUSB_MCU': 'OPT_MCU_STM32F4'}),
                    example_entry(b, root, group, name)]))
            with mock.patch.object(fj, 'ROOT', root):
                _, _, row, _ = fj.join(dirs)
            self.assertEqual(row['roles'], ['device', 'host'], 'one IDF dir per example, the board is their union')


def runner(real, host):
    """A subprocess.run stand-in: the real compiler answers `real`, cc answers `host`;
    None means the run fails."""
    def run(argv, **kw):
        answer = host if argv[0] == 'cc' else real
        if answer is None:
            return mock.Mock(returncode=1, stdout='', stderr='fatal error: fsl_device_registers.h: No such file\n')
        return mock.Mock(returncode=0, stdout=''.join(f'#define {n}\n' for n in answer), stderr='')
    return run


class Validate(unittest.TestCase):
    def setUp(self):
        self.entries = [entry('/b', '/b/src/tusb.c', {'CFG_TUSB_MCU': 'OPT_MCU_STM32F4'})]

    def test_both_probes_agreeing_is_valid_and_host_unavailable_is_a_note(self):
        self.assertEqual(fj.validate(ROW, self.entries, runner({'TUP_USBIP_DWC2'}, {'TUP_USBIP_DWC2'})), '')
        note = fj.validate(ROW, self.entries, runner({'TUP_USBIP_OHCI'}, None))
        self.assertIn('host probe unavailable', note)
        self.assertIn('fsl_device_registers.h', note)

    def test_a_failed_real_probe_or_a_mismatch_is_a_failure(self):
        with self.assertRaises(fj.Failure) as cm:
            fj.validate(ROW, self.entries, runner(None, {'TUP_USBIP_DWC2'}))
        self.assertIn('real compiler probe failed', str(cm.exception))
        with self.assertRaises(fj.Failure) as cm:
            fj.validate(ROW, self.entries, runner({'TUP_USBIP_DWC2'}, {'TUP_USBIP_FSDEV'}))
        self.assertIn('TUP_USBIP_DWC2', str(cm.exception))
        self.assertIn('TUP_USBIP_FSDEV', str(cm.exception))
        with self.assertRaises(fj.Failure):
            fj.validate(ROW, self.entries, runner(None, None))  # two failures never compare equal

    def test_the_probes_read_the_same_synthetic_config(self):
        argv = fj.real_probe_argv(self.entries[0])
        self.assertEqual(argv[0], 'arm-none-eabi-gcc')
        self.assertEqual(argv[-7:], ['-E', '-dM', *fj.SYNTHETIC_CONFIG, '-x', 'c', fj.OPTION_H])
        self.assertIn('-DCFG_TUSB_CONFIG_FILE=<stdint.h>', argv)
        self.assertNotIn('-c', argv)
        host = fj.host_probe_argv(ROW)
        self.assertEqual(host[:4], ['cc', '-E', '-dM', '-DCFG_TUSB_MCU=OPT_MCU_STM32F4'])
        self.assertIn('-DSTM32F407xx=1', host)
        self.assertIn('-DCFG_TUSB_CONFIG_FILE=<stdint.h>', host)


class Merge(unittest.TestCase):
    def test_merge_writes_canonical_text_and_leaves_an_equal_row_alone(self):
        with tempfile.TemporaryDirectory() as d:
            path = pathlib.Path(d) / 'family.json'
            with mock.patch.object(fj, 'ROOT', pathlib.Path(d)):
                self.assertEqual(fj.merge('stm32f4', 'stm32f407disco', ROW, path), 'updated')
                text = path.read_text()
                self.assertEqual(text, fj.canonical_text({'stm32f4': {'stm32f407disco': {'cmake': ROW}}}))
                self.assertEqual(fj.merge('stm32f4', 'stm32f407disco', dict(ROW), path), 'unchanged')
                self.assertEqual(fj.merge('nrf', 'nrf52840dk', dict(ROW, mcu='OPT_MCU_NRF5X'), path), 'updated')
                data = json.loads(path.read_text())
                self.assertEqual(sorted(data), ['nrf', 'stm32f4'], 'other rows are kept')
                path.write_text('{not json')
                with self.assertRaises(fj.Failure):
                    fj.merge('stm32f4', 'stm32f407disco', ROW, path)
                self.assertEqual(path.read_text(), '{not json', 'a malformed catalog is never replaced')

    def test_update_joins_example_trees_and_reports_one_line(self):
        with tempfile.TemporaryDirectory() as d:
            root = fixture_root(d)
            path = root / 'family.json'
            src = root / 'src'
            a = fixture_build(root, 'a', entries=[entry(root, str(src / 'portable/synopsys/dwc2/dcd_dwc2.c'), {'CFG_TUSB_MCU': 'OPT_MCU_STM32F4', 'STM32F407xx': None})])
            b = fixture_build(root, 'b', entries=[entry(root, str(src / 'portable/synopsys/dwc2/hcd_dwc2.c'), {'CFG_TUSB_MCU': 'OPT_MCU_STM32F4', 'STM32F407xx': None})])
            with mock.patch.object(fj, 'ROOT', root):
                line = fj.update([a, b], runner({'TUP_USBIP_DWC2'}, None), path)
                self.assertEqual(line, 'family.json: updated stm32f407disco (host probe unavailable (fatal error: fsl_device_registers.h: No such file))')
                row = json.loads(path.read_text())['stm32f4']['stm32f407disco']['cmake']
                self.assertEqual(row['portable'], ['synopsys/dwc2/dcd_dwc2.c', 'synopsys/dwc2/hcd_dwc2.c'])
                c = fixture_build(root, 'c', mcus=('STM32F4', 'MAX3421'), options={'MAX3421_HOST': '1'})
                line = fj.update([a, c], runner({'TUP_USBIP_DWC2'}, None), path)
                self.assertIn('not updated: family_mcus differs between example trees', line)
                self.assertEqual(json.loads(path.read_text())['stm32f4']['stm32f407disco']['cmake'], row, 'a failed join touches nothing')
                line = fj.update([a], runner(None, None), path)
                self.assertIn('not updated: real compiler probe failed', line)
                # the CLI's observe is the same join: two boards never print one row
                n = fixture_build(root, 'n', board='nrf52840dk', family='nrf', mcus=('NRF5X',))
                with contextlib.redirect_stderr(io.StringIO()) as err:
                    self.assertEqual(fj.main(['observe', str(a), str(n)]), 1)
                self.assertIn('configured nrf52840dk, not stm32f407disco', err.getvalue())


class Check(unittest.TestCase):
    def catalog(self, root, data):
        path = root / 'hw' / 'bsp' / 'family.json'
        path.write_text(fj.canonical_text(data))
        return path

    def test_a_complete_catalog_passes_and_every_gap_is_named_with_its_remedy(self):
        with tempfile.TemporaryDirectory() as d:
            root = fixture_root(d)
            (root / 'hw' / 'bsp' / 'pic32mz' / 'boards' / 'olimex_emz64').mkdir(parents=True)
            (root / 'hw' / 'bsp' / 'pic32mz' / 'boards' / 'olimex_hmz144').mkdir(parents=True)
            good = {'stm32f4': {'stm32f407disco': {'cmake': ROW}},
                    'pic32mz': {'olimex_emz64': {'cmake': None}, 'olimex_hmz144': {'cmake': None}}}
            path = self.catalog(root, good)
            self.assertEqual(fj.check(path, root), [])
            for data, marker in (
                ({'stm32f4': {}, 'pic32mz': good['pic32mz']}, 'stm32f4: no row for any of its 1 boards'),
                ({'stm32f4': {}, 'pic32mz': {'olimex_emz64': {'cmake': None}}}, 'pic32mz/olimex_hmz144: no row'),
                ({**good, 'stm32f4': {**good['stm32f4'], 'ghost': {'cmake': ROW}}}, 'stm32f4/ghost: row for a board dir that does not exist'),
                ({**good, 'stm32f4': {'stm32f407disco': {'cmake': None}}}, 'stm32f4/stm32f407disco: cmake is null but the board is not Make-only'),
                ({**good, 'pic32mz': {**good['pic32mz'], 'olimex_emz64': {'cmake': ROW}}}, 'pic32mz/olimex_emz64: is Make-only (NULL_BOARDS) but carries a cmake row'),
                ({**good, 'stm32f4': {'stm32f407disco': {'cmake': dict(ROW, family_mcus=['MAX3421', 'STM32F4'])}}}, 'MAX3421 belongs in family_mcus exactly when'),
                ({**good, 'stm32f4': {'stm32f407disco': {'cmake': dict(ROW, extra=1)}}}, 'fields must be exactly'),
                ({**good, 'stm32f4': {'stm32f407disco': {'cmake': dict(ROW, portable=['synopsys/dwc2/hcd_dwc2.c', 'synopsys/dwc2/dcd_dwc2.c'])}}}, 'portable must be a sorted list'),
                ({**good, 'stm32f4': {'stm32f407disco': {'cmake': dict(ROW, portable=['none/dcd_none.c'])}}}, 'is not a relative path to a file under src/portable/'),
                ({**good, 'stm32f4': {'stm32f407disco': {'cmake': {k: v for k, v in ROW.items() if k != 'roles'}}}}, 'fields must be exactly'),
                ({**good, 'stm32f4': {'stm32f407disco': {'cmake': dict(ROW, roles=['host', 'device'])}}}, 'roles must be a sorted list'),
                ({**good, 'stm32f4': {'stm32f407disco': {'cmake': dict(ROW, roles=['device', 'device'])}}}, 'roles must be a sorted list'),
                ({**good, 'stm32f4': {'stm32f407disco': {'cmake': dict(ROW, roles=['gadget'])}}}, 'roles: gadget is not a group under examples/'),
                ({**good, 'stm32f4': {'stm32f407disco': {'cmake': dict(ROW, roles=[1])}}}, 'roles must be a sorted list'),
                ({**good, 'stm32f4': {'stm32f407disco': {'cmake': dict(ROW, roles=[{}])}}}, 'roles must be a sorted list'),
                ({**good, 'stm32f4': {'stm32f407disco': {'cmake': dict(ROW, portable=[str(root / 'src' / 'portable' / 'synopsys/dwc2/dcd_dwc2.c')])}}}, 'is not a relative path'),
                ({**good, 'stm32f4': {'stm32f407disco': {'cmake': dict(ROW, portable=['../portable/synopsys/dwc2/dcd_dwc2.c'])}}}, 'is not a relative path'),
                ({**good, 'stm32f4': {'stm32f407disco': {'cmake': dict(ROW, mcu='OPT_MCU_BOGUS')}}}, 'is not an OPT_MCU_* defined in src/tusb_option.h'),
                ({**good, 'stm32f4': {'stm32f407disco': {'cmake': ROW, 'make': 1}}}, 'is {"cmake": ...} and nothing else'),
                ({**good, 'nrf': {'x': {'cmake': ROW}}}, 'nrf: rows for a family dir that does not exist'),
            ):
                path = self.catalog(root, data)
                problems = fj.check(path, root)
                self.assertTrue(any(marker in p for p in problems), f'{marker!r} not in {problems}')
            path.write_text(json.dumps(good))  # valid, not canonical
            self.assertTrue(any('not in canonical form' in p for p in fj.check(path, root)))
            path.unlink()
            self.assertEqual(len(fj.check(path, root)), 1)

    def test_fix_repairs_what_observation_can_and_reports_the_rest(self):
        with tempfile.TemporaryDirectory() as d:
            root = fixture_root(d)
            (root / 'hw' / 'bsp' / 'pic32mz' / 'boards' / 'olimex_emz64').mkdir(parents=True)
            (root / 'hw' / 'bsp' / 'pic32mz' / 'boards' / 'olimex_hmz144').mkdir(parents=True)
            (root / 'hw' / 'bsp' / 'nrf' / 'boards' / 'nrf52840dk').mkdir(parents=True)
            path = root / 'hw' / 'bsp' / 'family.json'
            path.write_text(json.dumps({'stm32f4': {'ghost': {'cmake': ROW}}, 'gone': {'x': {'cmake': ROW}},
                                        'pic32mz': {'olimex_emz64': {'cmake': ROW}}}))
            calls = []

            def run(argv, **kw):
                # tools/build.py's stand-in: writes the stm32 row, cannot configure nrf
                calls.append(argv)
                board = argv[argv.index('-b') + 1]
                if board == 'stm32f407disco':
                    fj.merge('stm32f4', board, dict(ROW), path)
                    return mock.Mock(returncode=0, stderr='family.json: updated stm32f407disco\n')
                return mock.Mock(returncode=1, stderr='family.json: not updated: nrf deps not at their pins: hw/mcu/nordic/nrfx\n')
            with contextlib.redirect_stdout(io.StringIO()) as out:
                changed, problems = fj.fix(path, root, run)
            self.assertTrue(changed)
            self.assertEqual(problems, ['nrf: no row for any of its 1 boards'])
            self.assertEqual([a[a.index('-b') + 1] for a in calls], ['nrf52840dk', 'stm32f407disco'])
            self.assertEqual(calls[1][-2], '--build-name')
            self.assertTrue(calls[1][-1].startswith('fj-stm32f407disco-'), 'a private dir of this process, never the shared one')
            self.assertIn('not updated: nrf deps not at their pins', out.getvalue())
            data = json.loads(path.read_text())
            self.assertEqual(sorted(data), ['pic32mz', 'stm32f4'], 'gone rows dropped, null rows written')
            self.assertEqual(data['pic32mz'], {'olimex_emz64': {'cmake': None}, 'olimex_hmz144': {'cmake': None}},
                             'a Make-only board with a cmake row is set to null, not skipped')
            self.assertEqual(path.read_text(), fj.canonical_text(data))
            # nothing to do: unchanged, and the one report stands
            with contextlib.redirect_stdout(io.StringIO()):
                changed, problems = fj.fix(path, root, run)
            self.assertEqual((changed, problems), (False, ['nrf: no row for any of its 1 boards']))

    def test_refresh_reobserves_every_cmake_board_and_fails_on_any_it_could_not(self):
        with tempfile.TemporaryDirectory() as d:
            root = fixture_root(d)
            (root / 'hw' / 'bsp' / 'pic32mz' / 'boards' / 'olimex_emz64').mkdir(parents=True)
            (root / 'hw' / 'bsp' / 'nrf' / 'boards' / 'nrf52840dk').mkdir(parents=True)
            (root / 'hw' / 'bsp' / 'nrf' / 'boards' / 'nrf5340dk').mkdir(parents=True)
            (root / 'hw' / 'bsp' / 'espressif' / 'boards' / 'esp32s3_devkitm').mkdir(parents=True)
            path = root / 'hw' / 'bsp' / 'family.json'
            # a row to keep, a null cmake row that is not Make-only, a missing row, and
            # a file that is not canonical: refresh accepts a dirty catalog
            path.write_text(json.dumps({'stm32f4': {'stm32f407disco': {'cmake': ROW}},
                                        'nrf': {'nrf52840dk': {'cmake': None}}}))
            dirs = []

            def run(argv, **kw):
                board = argv[argv.index('-b') + 1]
                dirs.append(root / 'cmake-build' / f'cmake-build-{argv[-1]}')
                dirs[-1].mkdir(parents=True)  # what a configure leaves; refresh must remove it
                if board == 'stm32f407disco':
                    return mock.Mock(returncode=0, stderr='family.json: unchanged stm32f407disco\n')
                if board == 'nrf52840dk':
                    fj.merge('nrf', board, dict(ROW, mcu='OPT_MCU_NRF5X'), path)
                    return mock.Mock(returncode=0, stderr='family.json: updated nrf52840dk\n')
                if board == 'nrf5340dk':
                    return mock.Mock(returncode=1, stderr='family.json: not updated: nrf deps not at their pins: x\n')
                return mock.Mock(returncode=0, stderr='')  # an Espressif example that did not configure: no line
            with contextlib.redirect_stdout(io.StringIO()) as out:
                asked, failed, problems = fj.refresh(path, root, run)
            self.assertEqual(asked, [('espressif', 'esp32s3_devkitm'), ('nrf', 'nrf52840dk'), ('nrf', 'nrf5340dk'),
                                     ('stm32f4', 'stm32f407disco')], 'every board but the Make-only one, rows or not')
            self.assertEqual([(f, b) for f, b, _ in failed], [('espressif', 'esp32s3_devkitm'), ('nrf', 'nrf5340dk')],
                             'no line and a refusal both fail; unchanged and updated both count')
            self.assertEqual(failed[0][2], 'tools/build.py exit 0')
            self.assertIn('not at their pins', failed[1][2])
            self.assertEqual(len(dirs), 4)
            self.assertFalse(any(x.exists() for x in dirs), 'every private dir removed, failures included')
            data = json.loads(path.read_text())
            self.assertEqual(data['stm32f4']['stm32f407disco']['cmake'], ROW, 'a row not re-observed is kept')
            self.assertEqual(data['nrf']['nrf52840dk']['cmake']['mcu'], 'OPT_MCU_NRF5X')
            self.assertEqual(data['pic32mz'], {'olimex_emz64': {'cmake': None}})
            self.assertEqual(path.read_text(), fj.canonical_text(data))
            self.assertEqual(problems, ['espressif: no row for any of its 1 boards', 'nrf/nrf5340dk: no row'])

    def test_refresh_cli_exits_1_on_a_kept_row_it_could_not_observe_and_0_when_all_observed(self):
        with tempfile.TemporaryDirectory() as d:
            root = fixture_root(d)
            path = root / 'hw' / 'bsp' / 'family.json'
            path.write_text(fj.canonical_text({'stm32f4': {'stm32f407disco': {'cmake': ROW}}}))
            answers = {}

            def run(argv, **kw):
                return mock.Mock(**answers)
            with mock.patch.object(fj, 'CATALOG', path), mock.patch.object(fj, 'ROOT', root), \
                    mock.patch('subprocess.run', run):
                # the old row still passes check(): only the missing observation fails the sweep
                for answers in ({'returncode': 1, 'stderr': ''},
                                {'returncode': 0, 'stderr': ''},
                                {'returncode': 0, 'stderr': 'family.json: not updated: cmake-build existed before\n'}):
                    with contextlib.redirect_stdout(io.StringIO()) as out:
                        self.assertEqual(fj.main(['refresh']), 1, answers)
                    self.assertIn('0 of 1 boards re-observed', out.getvalue())
                    self.assertIn('stm32f4/stm32f407disco: not observed:', out.getvalue())
                answers = {'returncode': 0, 'stderr': 'family.json: unchanged stm32f407disco\n'}
                with contextlib.redirect_stdout(io.StringIO()) as out:
                    self.assertEqual(fj.main(['refresh']), 0)
                self.assertIn('1 of 1 boards re-observed', out.getvalue())
                # fix keeps its own contract: a changed file is exit 1
                path.write_text(json.dumps({'stm32f4': {'stm32f407disco': {'cmake': ROW}}}))
                with contextlib.redirect_stdout(io.StringIO()) as out:
                    self.assertEqual(fj.main(['fix']), 1)
                self.assertIn('updated: stage it and commit again', out.getvalue())

    def test_a_stale_row_whose_reobservation_failed_is_reported_and_exits_1(self):
        with tempfile.TemporaryDirectory() as d:
            root = fixture_root(d)
            path = root / 'hw' / 'bsp' / 'family.json'
            path.write_text(fj.canonical_text({'stm32f4': {'stm32f407disco': {'cmake': ROW}}}))
            changed = ['hw/bsp/stm32f4/boards/stm32f407disco/board.cmake']
            answers = {}

            def run(argv, **kw):
                return mock.Mock(**answers)
            # the board's own cmake changed, so its row is stale; the configure fails
            answers = {'returncode': 1, 'stderr': 'family.json: not updated: stm32f4 deps not at their pins: x\n'}
            with contextlib.redirect_stdout(io.StringIO()):
                changed_file, problems = fj.fix(path, root, run, changed)
            self.assertFalse(changed_file, 'the row it could not observe is kept')
            self.assertEqual(problems, ['stm32f4/stm32f407disco: row kept from before the change that made it stale: '
                                        'family.json: not updated: stm32f4 deps not at their pins: x'])
            self.assertEqual(json.loads(path.read_text())['stm32f4']['stm32f407disco']['cmake'], ROW)
            with mock.patch.object(fj, 'CATALOG', path), mock.patch.object(fj, 'ROOT', root), \
                    mock.patch.object(fj, 'changed_cmake', return_value=changed), \
                    mock.patch('subprocess.run', run), contextlib.redirect_stdout(io.StringIO()) as out:
                self.assertEqual(fj.main(['fix']), 1)
            self.assertIn('row kept from before the change that made it stale', out.getvalue())
            # observed again, unchanged: nothing to report
            answers = {'returncode': 0, 'stderr': 'family.json: unchanged stm32f407disco\n'}
            with contextlib.redirect_stdout(io.StringIO()):
                self.assertEqual(fj.fix(path, root, run, changed), (False, []))

    def test_a_malformed_entry_keeps_its_structural_diagnostic_and_nothing_else(self):
        with tempfile.TemporaryDirectory() as d:
            root = fixture_root(d)
            path = root / 'hw' / 'bsp' / 'family.json'
            path.write_text(json.dumps({'stm32f4': {'stm32f407disco': ['malformed']}}))

            def run(argv, **kw):
                return mock.Mock(returncode=1, stderr='family.json: not updated: no toolchain\n')
            with contextlib.redirect_stdout(io.StringIO()):
                _, problems = fj.fix(path, root, run, ['hw/bsp/stm32f4/boards/stm32f407disco/board.cmake'])
            self.assertEqual(problems, ['stm32f4/stm32f407disco: an entry is {"cmake": ...} and nothing else'],
                             'a row that is not a mapping is check()\'s to report, and repair must not trip over it')

    def test_a_board_with_no_row_at_all_gets_only_its_own_diagnostic(self):
        with tempfile.TemporaryDirectory() as d:
            root = fixture_root(d)
            path = root / 'hw' / 'bsp' / 'family.json'
            path.write_text(fj.canonical_text({}))

            def run(argv, **kw):
                return mock.Mock(returncode=1, stderr='family.json: not updated: no toolchain\n')
            with contextlib.redirect_stdout(io.StringIO()):
                _, problems = fj.fix(path, root, run, ['hw/bsp/stm32f4/boards/stm32f407disco/board.cmake'])
            self.assertEqual(problems, ['stm32f4: no row for any of its 1 boards'],
                             'a board with nothing to keep is reported once, by check()')

    def test_the_catalog_in_this_tree_is_complete(self):
        # what the pre-commit hook guarantees: every board dir has a row, every row a board
        problems = fj.check()
        self.assertEqual(problems, [], '\n'.join(problems))


if __name__ == '__main__':
    unittest.main()
