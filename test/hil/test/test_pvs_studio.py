#!/usr/bin/env python3
"""Tests for tools/pvs_studio.py: matrix validation, compile-only variants, coverage parsing
and the aggregate verdict. No analyzer or compiler runs here."""
import json
import os
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock

REPO = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(REPO / 'tools'))
import pvs_studio as pvs  # noqa: E402

SRC = str(REPO / 'src')


def entry(file, example='device/cdc_msc', defines=('-DCFG_TUSB_MCU=OPT_MCU_STM32F4',)):
    obj = f'/b/{example}/CMakeFiles/x.dir{file}.obj'
    return {'directory': '/b', 'file': file, 'output': obj,
            'arguments': ['gcc', *defines, f'-I{REPO}/examples/{example}/src', '-o', obj, '-c', file]}


def result(name, findings=(), kept=None, status='ok', error=None):
    r = {'profile': name, 'status': status, 'findings': list(findings), 'kept': kept or {}}
    if error:
        r['error'] = error
    return r


def finding(file='src/tusb.c', line=1, code='V547', level=1):
    return {'file': file, 'line': line, 'code': code, 'level': level, 'message': 'm'}


class LoadMatrix(unittest.TestCase):
    def load(self, data):
        with tempfile.NamedTemporaryFile('w', suffix='.json', delete=False) as f:
            json.dump(data, f)
        self.addCleanup(os.unlink, f.name)
        return pvs.load_matrix(f.name)

    def test_real_matrix_loads(self):
        m = pvs.load_matrix()
        self.assertTrue(m['profiles'])

    def test_profile_needs_one_kind(self):
        with self.assertRaises(pvs.Failure):
            self.load({'profiles': {'a': {}}})
        with self.assertRaises(pvs.Failure):
            self.load({'profiles': {'a': {'board': 'x', 'base': 'y'}}})

    def test_base_must_be_board_profile(self):
        with self.assertRaises(pvs.Failure):
            self.load({'profiles': {'v': {'base': 'nope', 'example': 'device/cdc_msc'}}})
        with self.assertRaises(pvs.Failure):
            self.load({'profiles': {'b': {'board': 'x'}, 'v': {'base': 'b'}}})

    def test_gap_needs_status_and_reason(self):
        with self.assertRaises(pvs.Failure):
            self.load({'profiles': {}, 'gaps': {'src/x.c': {'status': 'skip', 'reason': 'r'}}})
        with self.assertRaises(pvs.Failure):
            self.load({'profiles': {}, 'gaps': {'src/x.c': {'status': 'missing-toolchain'}}})


class Variant(unittest.TestCase):
    def base(self):
        return [entry(f'{SRC}/tusb.c'), entry(f'{SRC}/device/usbd.c'),
                entry(f'{SRC}/tusb.c', example='host/cdc_msc_hid'),
                entry(f'{REPO}/examples/device/cdc_msc/src/main.c')]

    def test_takes_only_the_example_src_entries(self):
        out = pvs.variant_entries({'example': 'device/cdc_msc'}, self.base())
        self.assertEqual([Path(e['file']).name for e in out], ['tusb.c', 'usbd.c'])

    def test_config_file_and_replaced_define(self):
        p = {'example': 'device/cdc_msc', 'config': 'test/pvs/x/tusb_config.h', 'defines': {'CFG_TUSB_MCU': 'OPT_MCU_NONE'}}
        args = pvs.variant_entries(p, self.base())[0]['arguments']
        self.assertNotIn('-DCFG_TUSB_MCU=OPT_MCU_STM32F4', args)
        self.assertIn('-DCFG_TUSB_MCU=OPT_MCU_NONE', args)
        self.assertIn(f'-DCFG_TUSB_CONFIG_FILE="{REPO}/test/pvs/x/tusb_config.h"', args)
        self.assertLess(args.index('-DCFG_TUSB_MCU=OPT_MCU_NONE'), args.index('-c'))
        self.assertIn(f'-I{REPO}/examples/device/cdc_msc/src', args)  # example include dir kept

    def test_twice_defined_is_ambiguous(self):
        base = [entry(f'{SRC}/tusb.c', defines=('-DA=1', '-DA=2'))]
        with self.assertRaises(pvs.Failure):
            pvs.variant_entries({'example': 'device/cdc_msc', 'defines': {'A': '3'}}, base)

    def test_extra_source_cloned_from_tusb(self):
        out = pvs.variant_entries({'example': 'device/cdc_msc', 'sources': ['src/class/bth/bth_device.c']}, self.base())
        extra = out[-1]
        self.assertEqual(extra['file'], str(REPO / 'src/class/bth/bth_device.c'))
        self.assertIn(extra['file'], extra['arguments'])
        self.assertNotIn(f'{SRC}/tusb.c', extra['arguments'])
        self.assertTrue(any(a.endswith('bth_device.c.obj') for a in extra['arguments']))

    def test_unknown_example(self):
        with self.assertRaises(pvs.Failure):
            pvs.variant_entries({'example': 'device/none'}, self.base())


class TusbEntry(unittest.TestCase):
    def test_relative_file_is_normalized(self):
        e = {'directory': SRC, 'file': 'tusb.c'}
        self.assertIs(pvs.tusb_entry([entry(f'{SRC}/usbd.c'), e]), e)
        self.assertIsNone(pvs.tusb_entry([entry('/opt/x/src/tusb.c')]))


class BuildBoard(unittest.TestCase):
    def failure(self, out):
        with mock.patch.object(pvs.subprocess, 'run', return_value=mock.Mock(returncode=1, stdout=out, stderr='')):
            with self.assertRaises(pvs.Failure) as cm:
                pvs.build_board('x', {'board': 'x'})
        return str(cm.exception)

    def test_reports_the_first_error(self):
        msg = self.failure('noise\n' * 500 + 'src/tusb.c:3:1: error: boom\nmore\n')
        self.assertTrue(msg.endswith('src/tusb.c:3:1: error: boom'))
        self.assertNotIn('noise', msg)

    def test_bounded_tail_without_a_diagnostic(self):
        msg = self.failure('x' * 5000 + 'END')
        self.assertTrue(msg.endswith('END'))
        self.assertLess(len(msg), 2200)


class OwnEntries(unittest.TestCase):
    def test_drops_vendored_and_outside_sources(self):
        keep = entry(f'{SRC}/tusb.c')
        got = pvs.own_entries([keep, entry(f'{REPO}/lib/fatfs/ff.c'), entry(f'{REPO}/hw/mcu/st/x.c'),
                               entry('/opt/esp-idf/components/x.c')])
        self.assertEqual(got, [keep])


class KeptLines(unittest.TestCase):
    def test_counts_src_lines_only(self):
        out = '\n'.join([
            f'# 1 "{SRC}/tusb.c"', 'int a;', '', '#pragma x',
            f'# 1 "{SRC}/common/tusb_mcu.h" 1',
            '# 1 "/usr/include/stdint.h" 1', 'typedef int x;',
            f'# 5 "{SRC}/tusb.c" 2', 'int b;'])
        kept = pvs.kept_lines(out, '/b')
        self.assertEqual(kept['src/tusb.c'], ['int a;', 'int b;'])
        self.assertEqual(kept['src/common/tusb_mcu.h'], [])  # included, nothing kept
        self.assertEqual(len(kept), 2)

    def test_relative_marker_resolves_against_its_directory(self):
        self.assertEqual(list(pvs.kept_lines('# 1 "tusb.c"\nint a;', SRC)), ['src/tusb.c'])
        self.assertEqual(pvs.kept_lines('# 1 "tusb.c"\nint a;', '/b'), {})

    def test_macro_only(self):
        self.assertTrue(pvs.macro_only('src/common/tusb_mcu.h'))
        self.assertFalse(pvs.macro_only('src/tusb.c'))

    def test_macro_only_ignores_extern_c_guards(self):
        self.assertTrue(pvs.macro_only('src/portable/valentyusb/eptri/dcd_eptri.h'))
        self.assertFalse(pvs.macro_only('src/portable/synopsys/dwc2/dwc2_nrf.h'))

    def macro_only_text(self, text):
        with tempfile.TemporaryDirectory() as d:
            Path(d, 'h.h').write_text(text)
            with mock.patch.object(pvs, 'ROOT', Path(d)):
                return pvs.macro_only('h.h')

    def test_cplusplus_branch_with_c_alternative_is_code(self):
        self.assertFalse(self.macro_only_text(
            '#ifdef __cplusplus\n#define X 1\n#else\n#if ENABLE_API\nvoid api(void);\n#endif\n#endif\n'))

    def test_cplusplus_only_declaration_after_nested_if_is_code(self):
        self.assertFalse(self.macro_only_text(
            '#ifdef __cplusplus\n#if A\n#define X 1\n#endif\nclass C;\n#endif\n'))


class Verdict(unittest.TestCase):
    MATRIX = {'profiles': {'a': {'board': 'a'}, 'b': {'board': 'b'}},
              'gaps': {'src/portable/x/dcd_x.c': {'status': 'missing-toolchain', 'reason': 'r'}}}
    SOURCES = ['src/tusb.c', 'src/osal/osal_none.h', 'src/portable/x/dcd_x.c']

    def v(self, results, matrix=None, sources=None):
        return pvs.verdict(matrix or self.MATRIX, results, self.SOURCES if sources is None else sources)

    def test_not_applicable_gap_alone_is_clean(self):
        m = dict(self.MATRIX, gaps={'src/portable/x/dcd_x.c': {'status': 'not-applicable', 'reason': 'r'}})
        v = self.v([result('a', kept={'src/tusb.c': 1, 'src/osal/osal_none.h': 1}), result('b')], m)
        self.assertEqual(v['verdict'], 'clean')

    def test_incomplete_when_a_gap_is_used(self):
        v = self.v([result('a', kept={'src/tusb.c': 3}), result('b', kept={'src/osal/osal_none.h': 2})])
        self.assertEqual(v['verdict'], 'incomplete')
        self.assertEqual(list(v['gaps']), ['src/portable/x/dcd_x.c'])

    def test_clean(self):
        m = dict(self.MATRIX, gaps={})
        v = self.v([result('a', kept={'src/tusb.c': 3, 'src/portable/x/dcd_x.c': 1}),
                    result('b', kept={'src/osal/osal_none.h': 2})], matrix=m)
        self.assertEqual(v['verdict'], 'clean')

    def test_findings_dedup_across_profiles(self):
        m = dict(self.MATRIX, gaps={})
        kept = {'src/tusb.c': 3, 'src/portable/x/dcd_x.c': 1, 'src/osal/osal_none.h': 1}
        v = self.v([result('a', [finding(), finding(level=3, code='V1008')], kept),
                    result('b', [finding()], kept)], matrix=m)
        self.assertEqual(v['verdict'], 'incomplete')
        self.assertEqual(v['findings'], {'1': 1, '2': 0, '3': 1})
        self.assertEqual(v['diagnostics'][0]['profiles'], ['a', 'b'])

    def test_failed_on_missing_or_failed_profile(self):
        self.assertEqual(self.v([result('a')])['missingProfiles'], ['b'])
        v = self.v([result('a'), result('b', status='failed', error='boom')])
        self.assertEqual(v['verdict'], 'failed')
        self.assertEqual(v['failedProfiles'], {'b': 'boom'})

    def test_failed_on_unexplained_uncovered(self):
        v = self.v([result('a'), result('b')])
        self.assertEqual(v['verdict'], 'failed')
        self.assertEqual(v['uncovered'], ['src/tusb.c', 'src/osal/osal_none.h'])

    def test_included_macro_only_header_is_covered(self):
        v = self.v([result('a', kept={'src/common/tusb_mcu.h': 0}), result('b')],
                   matrix=dict(self.MATRIX, gaps={}), sources=['src/common/tusb_mcu.h'])
        self.assertEqual(v['verdict'], 'clean')

    def test_gap_on_an_included_macro_only_header_is_stale(self):
        m = dict(self.MATRIX, gaps={'src/common/tusb_mcu.h': {'status': 'not-applicable', 'reason': 'r'}})
        v = self.v([result('a', kept={'src/common/tusb_mcu.h': 0}), result('b')], matrix=m,
                   sources=['src/common/tusb_mcu.h'])
        self.assertEqual(v['staleGaps'], ['src/common/tusb_mcu.h'])
        self.assertEqual(v['verdict'], 'failed')

    def test_gap_on_an_untracked_file_is_stale(self):
        m = dict(self.MATRIX, gaps={'src/gone.c': {'status': 'missing-toolchain', 'reason': 'r'}})
        v = self.v([result('a', kept={'src/tusb.c': 1}), result('b')], matrix=m, sources=['src/tusb.c'])
        self.assertEqual(v['staleGaps'], ['src/gone.c'])

    def test_stale_gap_fails(self):
        v = self.v([result('a', kept={'src/tusb.c': 1, 'src/portable/x/dcd_x.c': 4}),
                    result('b', kept={'src/osal/osal_none.h': 1})])
        self.assertEqual(v['staleGaps'], ['src/portable/x/dcd_x.c'])
        self.assertEqual(v['verdict'], 'failed')


class Select(unittest.TestCase):
    M = {'profiles': {'a': {'board': 'a'}, 'av': {'base': 'a', 'example': 'device/x'},
                      'b': {'board': 'b', 'toolchain': 'riscv-gcc'}, 'c': {'board': 'c'}}}

    def test_named_profile_pulls_its_base(self):
        self.assertEqual(pvs.select(self.M, names=['av']), ['a', 'av'])

    def test_base_runs_before_its_fixture_whatever_the_names_sort(self):
        m = {'profiles': {'cfg': {'base': 'zz', 'example': 'device/x'}, 'zz': {'board': 'zz'}}}
        self.assertEqual(pvs.select(m, names=['cfg']), ['zz', 'cfg'])

    def test_toolchain_and_shards_keep_groups_together(self):
        self.assertEqual(pvs.select(self.M, 'arm-gcc'), ['a', 'av', 'c'])
        self.assertEqual(pvs.select(self.M, 'arm-gcc', '1/2'), ['a', 'av'])
        self.assertEqual(pvs.select(self.M, 'arm-gcc', '2/2'), ['c'])
        with self.assertRaises(pvs.Failure):
            pvs.select(self.M, 'arm-gcc', '3/2')
        with self.assertRaises(pvs.Failure):
            pvs.select(self.M, names=['zz'])

    def test_ci_matrix_covers_every_group_once(self):
        legs = pvs.ci_matrix(self.M, 1)
        self.assertEqual(legs, [{'toolchain': 'arm-gcc', 'shard': '1/2'}, {'toolchain': 'arm-gcc', 'shard': '2/2'},
                                {'toolchain': 'riscv-gcc', 'shard': '1/1'}])
        got = [n for leg in legs for n in pvs.select(self.M, leg['toolchain'], leg['shard'])]
        self.assertEqual(sorted(got), sorted(self.M['profiles']))


class Platform(unittest.TestCase):
    def test_ilp32_but_aarch64(self):
        m = {'profiles': {'a': {'board': 'a'}, 'r': {'board': 'r', 'toolchain': 'riscv-gcc'},
                          'p': {'board': 'p', 'toolchain': 'aarch64-gcc'}, 'v': {'base': 'p', 'example': 'x'}}}
        self.assertEqual([pvs.platform_of(m, n) for n in 'arpv'], ['linux32', 'linux32', 'linux64', 'linux64'])


class Findings(unittest.TestCase):
    def test_keeps_src_positions_only(self):
        warnings = {'warnings': [
            {'code': 'V547', 'level': 1, 'message': 'a', 'positions': [{'file': f'{SRC}/tusb.c', 'line': 3}]},
            {'code': 'V547', 'level': 2, 'message': 'b', 'positions': [{'file': f'{REPO}/examples/x.c', 'line': 1}]},
            {'code': 'V001', 'level': 1, 'message': 'c', 'positions': []}]}
        with tempfile.TemporaryDirectory() as d:
            out = Path(d, 'r.json')

            def fake(argv, cwd):
                out.write_text(json.dumps(warnings))
                return mock.Mock(returncode=0, stdout='', stderr='')
            with mock.patch.object(pvs, 'run_tool', fake):
                got = pvs.findings(Path(d, 'r.log'), out)
        self.assertEqual(got, [{'file': 'src/tusb.c', 'line': 3, 'code': 'V547', 'level': 1, 'message': 'a'}])

    def test_failed_conversion_never_reads_a_stale_report(self):
        with tempfile.TemporaryDirectory() as d:
            out = Path(d, 'r.json')
            out.write_text('{"warnings": []}')
            with mock.patch.object(pvs, 'run_tool', return_value=mock.Mock(returncode=127, stdout='', stderr='')):
                with self.assertRaises(pvs.Failure):
                    pvs.findings(Path(d, 'r.log'), out)

    def test_converter_failure(self):
        with tempfile.TemporaryDirectory() as d:
            with mock.patch.object(pvs, 'run_tool', return_value=mock.Mock(returncode=1, stdout='', stderr='bad')):
                with self.assertRaises(pvs.Failure):
                    pvs.findings(Path(d, 'r.log'), Path(d, 'r.json'))


class Analyze(unittest.TestCase):
    def test_license_notice_exit_with_log_is_ok(self):
        with tempfile.TemporaryDirectory() as d:
            log = Path(d, 'x.log')

            def fake(argv, cwd):
                log.write_text('x')
                return mock.Mock(returncode=2, stdout='Your license will expire in 9 days', stderr='')
            with mock.patch.object(pvs, 'run_tool', fake):
                pvs.analyze(Path(d, 'db.json'), log, 'linux32')

    def test_error_exit_fails(self):
        with tempfile.TemporaryDirectory() as d:
            with mock.patch.object(pvs, 'run_tool', return_value=mock.Mock(returncode=6, stdout='', stderr='no license')):
                with self.assertRaises(pvs.Failure):
                    pvs.analyze(Path(d, 'db.json'), Path(d, 'x.log'), 'linux32')



class Run(unittest.TestCase):
    def test_relative_out_and_one_sarif_per_run(self):
        seen, calls = [], []

        def fake_profile(matrix, name, out_dir, build, jobs):
            seen.append(out_dir)
            Path(out_dir, f'{name}.log').write_text('x')
            return result(name)

        def fake_tool(argv, cwd):
            calls.append(argv)
            return mock.Mock(returncode=0, stdout='', stderr='')
        with tempfile.TemporaryDirectory() as d, mock.patch.object(pvs, 'run_profile', fake_profile), \
                mock.patch.object(pvs, 'run_tool', fake_tool):
            cwd = os.getcwd()
            os.chdir(d)
            try:
                pvs.main(['run', '--profile', 'cfg-device-all', '--out', 'rel'])
            finally:
                os.chdir(cwd)
            out = Path(d, 'rel').resolve()
            self.assertEqual(seen, [out, out])
            sarif = [c for c in calls if 'sarif' in c]
            self.assertEqual(len(sarif), 1)
            self.assertEqual(sarif[0][-2:], [str(out / 'stm32f407disco.log'), str(out / 'cfg-device-all.log')])

    def test_sarif_conversion_failure_fails_the_run(self):
        def fake_profile(matrix, name, out_dir, build, jobs):
            Path(out_dir, f'{name}.log').write_text('x')
            return result(name)
        with tempfile.TemporaryDirectory() as d, mock.patch.object(pvs, 'run_profile', fake_profile), \
                mock.patch.object(pvs, 'run_tool', return_value=mock.Mock(returncode=127, stdout='', stderr='')), \
                mock.patch('builtins.print') as out:
            self.assertEqual(pvs.main(['run', '--profile', 'stm32f407disco', '--out', d]), 1)
        self.assertEqual(json.loads(out.call_args_list[-1].args[0])['verdict'], 'failed')

    def test_stale_log_is_dropped_before_the_profile_runs(self):
        with tempfile.TemporaryDirectory() as d:
            Path(d, 'stm32f407disco.log').write_text('old')
            with mock.patch.object(pvs, 'build_board', side_effect=pvs.Failure('boom')):
                r = pvs.run_profile(pvs.load_matrix(), 'stm32f407disco', Path(d), True, 1)
            self.assertEqual(r['status'], 'failed')
            self.assertFalse(Path(d, 'stm32f407disco.log').exists())


if __name__ == '__main__':
    unittest.main()
