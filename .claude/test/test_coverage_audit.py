"""Tests for the build skill's coverage_audit.py: the parsers that turn ninja and readelf
output into a per-example file set, extraction over a canned build, and the replay
judgement against a build view."""
import importlib.util
import json
import os
import subprocess
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

    def test_global_assembly_label_is_live_local_one_is_not(self):
        self.assertTrue(ca.defines_symbols('    25: 00000000     2 NOTYPE  GLOBAL DEFAULT    7 Default_Handler\n'))
        self.assertFalse(ca.defines_symbols('     4: 00000000     0 NOTYPE  LOCAL  DEFAULT    1 $t\n'))

    def test_query_keeps_explicit_and_implicit_inputs_only(self):
        q = ('device/x/x.elf:\n  input: C_EXECUTABLE_LINKER__x\n    a.obj\n'
             '    | lib/libboard.a\n    || cmake_object_order_depends_target_x\n'
             '  outputs:\n    all\nlib/libboard.a:\n  input: C_STATIC_LIBRARY_LINKER__board\n'
             '    lib/CMakeFiles/board.dir/family.c.obj\n  outputs:\n    device/x/x.elf\n')
        self.assertEqual(ca.parse_query(q), {'device/x/x.elf': ['a.obj', 'lib/libboard.a'],
                                             'lib/libboard.a': ['lib/CMakeFiles/board.dir/family.c.obj']})

    def test_link_files_finds_scripts_given_as_options(self):
        with tempfile.TemporaryDirectory() as root:
            for f in ('hw/a.ld', 'hw/b.ld', 'hw/c.ld'):
                os.makedirs(os.path.join(root, 'hw'), exist_ok=True)
                open(os.path.join(root, f), 'w').close()
            cmd = (f'gcc -Wl,--script={root}/hw/a.ld -T{root}/hw/b.ld -Wl,-T,{root}/hw/c.ld '
                   f'-Wl,-Map={root}/cmake-build/x.map -L{root}/hw x.obj -o x.elf')
            self.assertEqual(ca.link_files(cmd, root, os.path.join(root, 'cmake-build')),
                             {'hw/a.ld', 'hw/b.ld', 'hw/c.ld'})

    def test_example_elves_from_targets(self):
        targets = ('device/cdc_msc/cdc_msc.elf: C_EXECUTABLE_LINKER\nlib/libboard.a: C_STATIC\n'
                   'device/cdc_msc/cdc_msc.elf.map: phony\nhost/bare_api/bare_api.elf: C_EXECUTABLE_LINKER\n')
        self.assertEqual(ca.example_elves(targets), {'device/cdc_msc': 'device/cdc_msc/cdc_msc.elf',
                                                     'host/bare_api': 'host/bare_api/bare_api.elf'})
        self.assertEqual(ca.example_elves('cdc_msc.elf: L\n', 'device/cdc_msc'), {'device/cdc_msc': 'cdc_msc.elf'})

    def test_walk_follows_archives_and_phony_groups_once(self):
        graph = {'x.elf': ['a.obj', 'lib/libboard.a', 'lib/libos.a', '/opt/libc.a'],
                 'lib/libboard.a': ['b.obj', 'lib/libos.a'], 'lib/libos.a': ['port_group', 'os.obj'],
                 'port_group': ['port.obj']}
        asked = []

        def query(ts):
            asked.extend(ts)
            return {t: graph[t] for t in ts}
        objs, others = ca.walk_link_inputs('x.elf', query, set(graph))
        self.assertEqual(objs, {'a.obj', 'b.obj', 'os.obj', 'port.obj'})
        self.assertEqual(others, {'/opt/libc.a'})
        self.assertEqual(sorted(asked), sorted(graph))

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
            with self.assertRaises(ca.ExtractError):
                ca.extract_board(d, d)

    def test_per_example_trees_are_extracted_as_their_example(self):
        seen = []
        orig = ca.extract
        ca.extract = lambda b, r, ex=None: seen.append(ex) or {'examples': {ex: ['src/tusb.c']}, 'cmake_inputs': [], 'nodeps': []}
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


class FakeBuild:
    """A built tree on disk (sources, compile_commands.json) whose ninja and readelf answers
    are canned: two examples, each linking a different library archive."""
    def __init__(self, root, fail=None):
        self.root, self.bd, self.fail = root, os.path.join(root, 'cmake-build', 'b'), fail
        for f in ('examples/device/a/main.c', 'examples/device/b/main.c', 'lib/os/os.c', 'lib/os/os.h',
                  'lib/usb/usb.c', 'hw/bsp/f/start.S', 'hw/bsp/f/mem.ld', 'src/dead.c', 'src/prog.pio'):
            os.makedirs(os.path.dirname(os.path.join(root, f)), exist_ok=True)
            open(os.path.join(root, f), 'w').close()
        os.makedirs(self.bd)
        with open(os.path.join(root, 'hw/bsp/f/link.ld'), 'w') as fh:
            fh.write('INCLUDE mem.ld\nSECTIONS {}\n')
        self.objs = {'device/a/CMakeFiles/a.dir/main.c.obj': 'examples/device/a/main.c',
                     'device/b/CMakeFiles/b.dir/main.c.obj': 'examples/device/b/main.c',
                     'lib/CMakeFiles/os.dir/os.c.obj': 'lib/os/os.c',
                     'lib/CMakeFiles/usb.dir/usb.c.obj': 'lib/usb/usb.c',
                     'device/a/CMakeFiles/a.dir/start.S.obj': 'hw/bsp/f/start.S',
                     'device/a/CMakeFiles/a.dir/dead.c.obj': 'src/dead.c'}
        with open(os.path.join(self.bd, 'compile_commands.json'), 'w') as fh:
            json.dump([{'directory': self.bd, 'command': 'gcc -c', 'file': os.path.join(root, s),
                        'output': o} for o, s in self.objs.items()], fh)
        self.query = {'device/a/a.elf': ['device/a/CMakeFiles/a.dir/main.c.obj',
                                         'device/a/CMakeFiles/a.dir/start.S.obj',
                                         'device/a/CMakeFiles/a.dir/dead.c.obj', 'lib/libos.a'],
                      'device/b/b.elf': ['device/b/CMakeFiles/b.dir/main.c.obj', 'lib/libusb.a'],
                      'lib/libos.a': ['lib/CMakeFiles/os.dir/os.c.obj'],
                      'lib/libusb.a': ['lib/CMakeFiles/usb.dir/usb.c.obj'],
                      'gen/prog.h': [os.path.join(root, 'src/prog.pio')],
                      'build.ninja': [os.path.join(root, 'examples/device/a/CMakeLists.txt')]}

    def __call__(self, cmd, cwd, **kw):
        out, rc = '', 0
        if cmd[0] == 'readelf':
            dead = cmd[-1].endswith(('dead.c.obj', 'start.S.obj'))
            out = '' if dead else '     5: 00000000    24 FUNC    GLOBAL DEFAULT    3 f\n'
        elif cmd[4] == self.fail:
            rc = 1
        elif cmd[4] == 'targets':
            out = ''.join(f'{t}: x\n' for t in self.query)
        elif cmd[4] == 'deps':
            out = ''.join(f'{o}: #deps 2, deps mtime 1 (VALID)\n    {os.path.join(self.root, s)}\n'
                          f'    {os.path.join(self.root, "lib/os/os.h")}\n'
                          + (f'    {self.bd}/gen/prog.h\n' if o.endswith('usb.c.obj') else '') + '\n'
                          for o, s in self.objs.items() if not o.endswith('start.S.obj'))
        elif cmd[4] == 'query':
            out = ''.join(f'{t}:\n  input: R\n' + ''.join(f'    {i}\n' for i in self.query[t])
                          + '  outputs:\n    all\n' for t in cmd[5:])
        elif cmd[4] == 'commands':
            out = f'gcc -c x\ngcc -Wl,--script={self.root}/hw/bsp/f/link.ld -o {cmd[5]}\n'
        return subprocess.CompletedProcess(cmd, rc, out, 'boom' if rc else '')


class ExtractTest(unittest.TestCase):
    def extract(self, fail=None):
        orig = ca.run
        with tempfile.TemporaryDirectory() as root:
            fb = FakeBuild(root, fail)
            ca.run = fb
            try:
                return ca.extract(fb.bd, root)
            finally:
                ca.run = orig

    def test_each_example_gets_its_own_archives_linker_script_and_asm(self):
        g = self.extract()
        self.assertEqual(g['examples']['device/a'], ['examples/device/a/main.c', 'hw/bsp/f/link.ld', 'hw/bsp/f/mem.ld',
                                                     'hw/bsp/f/start.S', 'lib/os/os.c', 'lib/os/os.h'])
        self.assertEqual(g['examples']['device/b'], ['examples/device/b/main.c', 'hw/bsp/f/link.ld', 'hw/bsp/f/mem.ld',
                                                     'lib/os/os.h', 'lib/usb/usb.c', 'src/prog.pio'])
        self.assertEqual(g['nodeps'], ['device/a/CMakeFiles/a.dir/start.S.obj'])
        self.assertEqual(g['cmake_inputs'], ['examples/device/a/CMakeLists.txt'])

    def test_a_failing_ninja_tool_fails_the_extraction(self):
        for tool in ('targets', 'deps', 'query', 'commands'):
            with self.assertRaises(ca.ExtractError, msg=tool):
                self.extract(fail=tool)


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
