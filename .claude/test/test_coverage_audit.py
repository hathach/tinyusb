"""Tests for the build skill's coverage_audit.py: the parsers that turn ninja and readelf
output into a per-example file set, and the replay judgement against a build view."""
import importlib.util
import json
import os
import tempfile
import unittest
from pathlib import Path

SCRIPT = Path(__file__).resolve().parents[1] / 'skills' / 'build' / 'scripts' / 'coverage_audit.py'
spec = importlib.util.spec_from_file_location('coverage_audit', SCRIPT)
ca = importlib.util.module_from_spec(spec)
spec.loader.exec_module(ca)

DEPS = """\
device/cdc_msc/CMakeFiles/cdc_msc.dir/src/main.c.obj: #deps 3, deps mtime 1 (VALID)
    /repo/examples/device/cdc_msc/src/main.c
    /repo/src/tusb.h
    /opt/gcc/include/stdint.h

device/cdc_msc/CMakeFiles/cdc_msc.dir/__/__/__/src/class/dfu/dfu_device.c.obj: #deps 1, deps mtime 1 (VALID)
    /repo/src/class/dfu/dfu_device.c
"""


class ParseTest(unittest.TestCase):
    def test_ninja_deps_groups_paths_under_their_object(self):
        d = ca.parse_ninja_deps(DEPS)
        self.assertEqual(d['device/cdc_msc/CMakeFiles/cdc_msc.dir/src/main.c.obj'],
                         ['/repo/examples/device/cdc_msc/src/main.c', '/repo/src/tusb.h',
                          '/opt/gcc/include/stdint.h'])
        self.assertEqual(len(d), 2)

    def test_defined_func_or_object_is_live(self):
        sym = '     5: 00000000    24 FUNC    GLOBAL DEFAULT    3 tud_cdc_n_write\n'
        self.assertTrue(ca.defines_symbols(sym))

    def test_undefined_or_section_only_symbols_are_dead(self):
        txt = ('     1: 00000000     0 SECTION LOCAL  DEFAULT    1 .text\n'
               '     2: 00000000     0 NOTYPE  GLOBAL DEFAULT  UND tu_fifo_write\n'
               '     3: 00000000     0 FUNC    GLOBAL DEFAULT  UND memcpy\n')
        self.assertFalse(ca.defines_symbols(txt))

    def test_gcc_nm_defined_symbols_are_live_undefined_are_not(self):
        self.assertTrue(ca.nm_defines_symbols('00000000 T tud_midi_n_stream_write\n'))
        self.assertFalse(ca.nm_defines_symbols('         U tu_fifo_write\n'))

    def test_compiler_of_reads_either_compile_commands_form(self):
        self.assertEqual(ca.compiler_of({'command': '/x/riscv-none-elf-gcc -c a.c'}), '/x/riscv-none-elf-gcc')
        self.assertEqual(ca.compiler_of({'arguments': ['arm-none-eabi-gcc', '-c']}), 'arm-none-eabi-gcc')

    def test_example_of_maps_example_objects_only(self):
        self.assertEqual(ca.example_of('host/bare_api/CMakeFiles/bare_api.dir/src/main.c.obj'), 'host/bare_api')
        self.assertIsNone(ca.example_of('CMakeFiles/board.dir/family.c.obj'))
        self.assertIsNone(ca.example_of('device/cdc_msc/cdc_msc.elf'))

    def test_query_inputs_strips_implicit_and_order_only_markers(self):
        q = ('device/x/x.elf:\n  input: C_EXECUTABLE_LINKER__x\n    a.obj\n'
             '    | /repo/hw/bsp/f4/linker.ld\n    || cmake_object_order_depends_target_x\n'
             '  outputs:\n    all\n')
        self.assertEqual(ca.query_inputs(q), ['a.obj', '/repo/hw/bsp/f4/linker.ld',
                                              'cmake_object_order_depends_target_x', 'all'])

    def test_rel_drops_outside_and_generated_paths_and_resolves_against_base(self):
        self.assertEqual(ca.rel('/repo/src/tusb.h', '/repo'), 'src/tusb.h')
        self.assertIsNone(ca.rel('/opt/gcc/include/stdint.h', '/repo'))
        self.assertIsNone(ca.rel('/repo/cmake-build/cmake-build-x/gen.h', '/repo'))
        self.assertEqual(ca.rel('../../src/tusb.c', '/repo', '/repo/cmake-build/b'), 'src/tusb.c')


class ScopeTest(unittest.TestCase):
    def test_example_cmake_reaches_its_example_only(self):
        own, shared = ca.scope_cmake_inputs(
            ['examples/device/cdc_msc/CMakeLists.txt', 'examples/device/CMakeLists.txt',
             'examples/host/gone/CMakeLists.txt', 'hw/bsp/stm32f4/family.cmake'],
            {'device/cdc_msc': [], 'host/bare_api': []})
        self.assertEqual(own, {'device/cdc_msc': ['examples/device/cdc_msc/CMakeLists.txt'], 'host/bare_api': []})
        self.assertEqual(shared, ['examples/device/CMakeLists.txt', 'hw/bsp/stm32f4/family.cmake'])

    def test_load_index_skips_failed_boards_and_scopes_cmake(self):
        with tempfile.TemporaryDirectory() as d:
            for rec in ({'board': 'b1', 'family': 'f1', 'status': 'ok',
                         'examples': {'device/a': ['src/a.c'], 'device/b': ['src/b.c']},
                         'cmake_inputs': ['examples/device/a/CMakeLists.txt', 'hw/bsp/f1/family.cmake']},
                        {'board': 'b2', 'family': 'f2', 'status': 'failed'}):
                with open(os.path.join(d, rec['board'] + '.json'), 'w') as fh:
                    json.dump(rec, fh)
            index, family, status = ca.load_index(d)
        self.assertEqual(status, {'b1': 'ok', 'b2': 'failed'})
        self.assertEqual(family, {'b1': 'f1', 'b2': 'f2'})
        self.assertEqual(index['examples/device/a/CMakeLists.txt'], {('b1', 'device/a')})
        self.assertEqual(index['hw/bsp/f1/family.cmake'], {('b1', 'device/a'), ('b1', 'device/b')})
        self.assertEqual(index['src/b.c'], {('b1', 'device/b')})


class ExtractBoardTest(unittest.TestCase):
    def test_tree_without_any_build_ninja_is_a_failed_build(self):
        with tempfile.TemporaryDirectory() as d:
            os.makedirs(os.path.join(d, 'device', 'cdc_msc'))
            self.assertIsNone(ca.extract_board(d, d))

    def test_per_example_trees_are_extracted_as_their_example(self):
        seen = []
        orig = ca.extract
        ca.extract = lambda b, r, ex=None: seen.append(ex) or {'examples': {ex: ['src/tusb.c']}, 'cmake_inputs': []}
        try:
            with tempfile.TemporaryDirectory() as d:
                for ex in ('device/cdc_msc', 'host/cdc_msc_hid', 'device/unbuilt'):
                    os.makedirs(os.path.join(d, ex))
                for ex in ('device/cdc_msc', 'host/cdc_msc_hid'):
                    open(os.path.join(d, ex, 'build.ninja'), 'w').close()
                g = ca.extract_board(d, d)
        finally:
            ca.extract = orig
        self.assertEqual(seen, ['device/cdc_msc', 'host/cdc_msc_hid'])
        self.assertEqual(set(g['examples']), {'device/cdc_msc', 'host/cdc_msc_hid'})


class JudgeTest(unittest.TestCase):
    INDEX = {'src/a.c': {('b1', 'device/a'), ('b2', 'device/a')}, 'src/b.c': {('b1', 'device/b')}}
    FAMILY = {'b1': 'f1', 'b2': 'f2'}

    def view(self, full=False, families=(), family_examples=None):
        return {'full': full, 'families': set(families), 'family_examples': family_examples or {}}

    def test_full_covers_everything(self):
        v = ca.judge(['src/a.c'], [], self.INDEX, self.FAMILY, self.view(full=True))
        self.assertEqual((v['required'], v['missed']), (2, []))

    def test_unselected_family_is_missed(self):
        v = ca.judge(['src/a.c'], [], self.INDEX, self.FAMILY, self.view(families=['f1']))
        self.assertEqual(v['missed'], [('b2', 'device/a')])

    def test_example_filter_misses_examples_outside_it(self):
        v = ca.judge(['src/a.c', 'src/b.c'], [], self.INDEX, self.FAMILY,
                     self.view(families=['f1', 'f2'], family_examples={'f1': ['device/a']}))
        self.assertEqual(v['missed'], [('b1', 'device/b')])

    def test_unseen_and_deleted_paths_are_reported(self):
        v = ca.judge(['README.md', 'src/gone.c'], ['src/gone.c'], self.INDEX, self.FAMILY, self.view())
        self.assertEqual((v['required'], v['unseen'], v['deleted']), (0, ['README.md', 'src/gone.c'], ['src/gone.c']))

    def test_no_files_requires_nothing(self):
        self.assertEqual(ca.judge([], [], self.INDEX, self.FAMILY, self.view())['required'], 0)


if __name__ == '__main__':
    unittest.main()
