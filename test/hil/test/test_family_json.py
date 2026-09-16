"""tools/family_json.py: the join of a configure's outputs into a hw/bsp/family.json row,
the probes that validate it, the merge that writes it, and the check pre-commit runs.
No cmake here: the build dirs are fixtures. The last test is the check itself over the
real tree, which is what the pre-commit hook guarantees."""
import contextlib
import io
import json
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
    return root


ROW = {'defines': {'STM32F407xx': '1'}, 'family_mcus': ['STM32F4'], 'mcu': 'OPT_MCU_STM32F4',
       'options': {}, 'portable': ['synopsys/dwc2/dcd_dwc2.c']}


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

    def test_the_catalog_in_this_tree_is_complete(self):
        # what the pre-commit hook guarantees: every board dir has a row, every row a board
        problems = fj.check()
        self.assertEqual(problems, [], '\n'.join(problems))


if __name__ == '__main__':
    unittest.main()
