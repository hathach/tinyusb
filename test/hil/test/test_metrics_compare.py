#!/usr/bin/env python3
"""Unit tests for tools/metrics_compare.py (pure functions, no build needed)."""
import os
import struct
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), '../../..'))
sys.path.insert(0, os.path.join(REPO, 'tools'))
import metrics_compare as mc  # noqa: E402


def fake_report(symbols):
    return {'symbols': symbols}


# `source_file` is truncated; CMake's `object_file` retains the source path.
SYMS_BASE = [
    {'name': 'dcd_init', 'size': 100, 'section': '.text', 'source_file': 'dcd_dwc2.c',
     'object_file': 'device/cdc_msc/CMakeFiles/cdc_msc.dir/co/src/portable/synopsys/dwc2/dcd_dwc2.c.obj'},
    {'name': 'dcd_buf', 'size': 64, 'section': '.bss', 'source_file': 'dcd_dwc2.c',
     'object_file': 'device/cdc_msc/CMakeFiles/cdc_msc.dir/co/src/portable/synopsys/dwc2/dcd_dwc2.c.obj'},
    {'name': 'vendor_thing', 'size': 999, 'section': '.text', 'source_file': 'whatever.c',
     'object_file': 'device/cdc_msc/CMakeFiles/cdc_msc.dir/co/hw/mcu/st/whatever.c.obj'},
]
SYMS_CUR = [
    {'name': 'dcd_init', 'size': 120, 'section': '.text', 'source_file': 'dcd_dwc2.c',
     'object_file': 'device/cdc_msc/CMakeFiles/cdc_msc.dir/co2/src/portable/synopsys/dwc2/dcd_dwc2.c.obj'},
    {'name': 'dcd_buf', 'size': 64, 'section': '.bss', 'source_file': 'dcd_dwc2.c',
     'object_file': 'device/cdc_msc/CMakeFiles/cdc_msc.dir/co2/src/portable/synopsys/dwc2/dcd_dwc2.c.obj'},
]


class ReportForElf(unittest.TestCase):
    def test_uses_the_elfs_link_and_runs_from_the_build_dir(self):
        with tempfile.TemporaryDirectory() as build:
            open(os.path.join(build, 'build.ninja'), 'w').close()
            elf = os.path.join(build, 'device', 'x', 'x.elf')
            os.makedirs(os.path.dirname(elf))
            commands = 'cc -Wl,--script=/sdk/memmap.ld -Wl,--defsym=X=1 -o device/x/x.elf\n'
            done = subprocess.CompletedProcess([], 0, '{}', '')
            with mock.patch.object(mc, 'link_command', return_value=commands) as link, \
                 mock.patch.object(mc.subprocess, 'run', return_value=done) as run:
                mc.report_for_elf(elf)
            link.assert_called_once_with('ninja', build, elf)
            cmd = run.call_args.args[0]
            self.assertEqual(cmd[2:4], [elf, '/sdk/memmap.ld'])
            self.assertEqual(cmd[cmd.index('--def') + 1], 'X=1')
            self.assertEqual(run.call_args.kwargs['cwd'], build)

    def test_malformed_output_is_a_runtime_error(self):
        done = subprocess.CompletedProcess([], 0, 'not json', '')
        with mock.patch.object(mc, '_link_settings', return_value=('/b', ['m.ld'], [])), \
             mock.patch.object(mc.subprocess, 'run', return_value=done):
            with self.assertRaisesRegex(RuntimeError, 'malformed membrowse report'):
                mc.report_for_elf('/b/x.elf')


class CompareReports(unittest.TestCase):
    def test_delta_table(self):
        md = mc.compare_reports({'portable/dcd_dwc2.c': {'flash': 100, 'ram': 64}},
                                {'portable/dcd_dwc2.c': {'flash': 120, 'ram': 64}})
        self.assertIn('dcd_dwc2.c', md)
        self.assertIn('+20', md)          # flash grew 100 -> 120
        self.assertIn('TOTAL', md)


def elf(files, all_syms=None):
    """An engine's sizes: files as {path: (flash, ram)}; the all total defaults to their sum."""
    files = {p: {'flash': f, 'ram': r} for p, (f, r) in files.items()}
    if all_syms is None:
        all_syms = (sum(v['flash'] for v in files.values()), sum(v['ram'] for v in files.values()))
    return {'files': files, 'all': {'flash': all_syms[0], 'ram': all_syms[1]}}


def pair_ids(n, board='b'):
    return [(board, f'device/ex{i}/ex{i}.elf') for i in range(n)]


class PairElfs(unittest.TestCase):
    def test_unmatched_and_failed_elfs_are_not_paired(self):
        a, b, c, d = pair_ids(4)
        base = {a: elf({'x.c': (1, 0)}), b: elf({}), c: None}
        cur = {a: elf({'x.c': (1, 0)}), c: elf({}), d: elf({})}
        pairs, base_only, cur_only = mc.pair_elfs(base, cur)
        self.assertEqual(list(pairs), [a])     # c's base report failed
        self.assertEqual(base_only, [b])
        self.assertEqual(cur_only, [d])


class RenderPairs(unittest.TestCase):
    def render(self, base, cur, failures=()):
        pairs, base_only, cur_only = mc.pair_elfs(base, cur)
        return mc.render_pairs(pairs, len(base.keys() & cur.keys()), 'membrowse', base_only, cur_only, failures)

    def test_different_elf_sets_give_zero_on_shared_pairs(self):
        a, b, extra = pair_ids(3)
        base = {a: elf({'x.c': (100, 8)}), b: elf({'x.c': (200, 8)})}
        cur = {a: elf({'x.c': (100, 8)}), b: elf({'x.c': (200, 8)}), extra: elf({'x.c': (900, 8)})}
        md = self.render(base, cur)
        self.assertIn('Coverage (INCOMPLETE, membrowse):** 2 of 2 matched elf pairs compared, 0 changed', md)
        self.assertIn('current-only: `b: device/ex2/ex2.elf`', md)
        self.assertIn('_no changes_', md)

    def test_single_pair_with_an_unmatched_elf_is_incomplete(self):
        a, extra = pair_ids(2)
        md = self.render({a: elf({'x.c': (1, 0)})}, {a: elf({'x.c': (1, 0)}), extra: elf({})})
        self.assertIn('Coverage (INCOMPLETE, membrowse):** 1 of 1 matched', md)
        self.assertIn('current-only: `b: device/ex1/ex1.elf`', md)

    def test_zero_pairs_are_not_reported_as_no_changes(self):
        a, b = pair_ids(2)
        for base, cur, failures in (({}, {}, [(('b', None), 'current', 'build', 'failed')]),
                                    ({a: elf({})}, {b: elf({})}, [])):
            md = self.render(base, cur, failures)
            self.assertIn('Coverage (INCOMPLETE, membrowse):** 0 of 0 matched', md)
            self.assertIn('_no comparable pairs_', md)
            self.assertNotIn('_no changes_', md)
            if failures:
                self.assertIn('FAILED `b` current build: failed', md)

    def test_equal_extremes_pick_the_same_witness_whatever_the_insertion_order(self):
        a, b, c = pair_ids(3)
        base = {i: elf({'x.c': (100, 0)}) for i in (a, b, c)}
        cur = {a: elf({'x.c': (110, 0)}), b: elf({'x.c': (110, 0)}), c: elf({'x.c': (100, 0)})}
        forward = self.render(base, cur)
        backward = self.render(dict(reversed(list(base.items()))), dict(reversed(list(cur.items()))))
        self.assertEqual(forward, backward)
        self.assertIn('| x.c | 2/3 | 0 | +10 (b: device/ex0/ex0.elf) |', forward)

    def test_failed_elfs_of_one_example_stay_distinguishable(self):
        app, loader = ('b', 'device/ex/app.elf'), ('b', 'device/ex/loader.elf')
        md = self.render({app: None, loader: None}, {app: elf({}), loader: elf({})},
                         [(app, 'base', 'report', 'x'), (loader, 'base', 'report', 'y')])
        self.assertIn('FAILED `b: device/ex/app.elf` base report: x', md)
        self.assertIn('FAILED `b: device/ex/loader.elf` base report: y', md)

    def test_one_growing_pair_among_many_stays_visible(self):
        ids = pair_ids(10)
        base = {i: elf({'x.c': (100, 0)}) for i in ids}
        cur = {**base, ids[3]: elf({'x.c': (300, 0)})}
        md = self.render(base, cur)
        self.assertIn('| x.c | 1/10 | 0 | +200 (b: device/ex3/ex3.elf) | 0 | 0 |', md)

    def test_opposite_signs_do_not_cancel(self):
        a, b = pair_ids(2)
        base = {a: elf({'x.c': (100, 0)}), b: elf({'x.c': (100, 0)})}
        cur = {a: elf({'x.c': (110, 0)}), b: elf({'x.c': (90, 0)})}
        md = self.render(base, cur)
        self.assertIn('| x.c | 2/2 | -10 (b: device/ex1/ex1.elf) | +10 (b: device/ex0/ex0.elf) |', md)

    def test_cancelling_files_inside_a_pair_still_mark_it_changed(self):
        a, b = pair_ids(2)
        base = {a: elf({'x.c': (100, 0), 'y.c': (100, 0)}), b: elf({'x.c': (1, 0)})}
        cur = {a: elf({'x.c': (140, 0), 'y.c': (60, 0)}), b: elf({'x.c': (1, 0)})}
        md = self.render(base, cur)
        self.assertIn('2 of 2 matched elf pairs compared, 1 changed', md)
        self.assertIn('| b: device/ex0/ex0.elf | 0 | 0 | 0 | 0 |', md)
        self.assertIn('| x.c | 1/2 |', md)
        self.assertIn('| y.c | 1/1 |', md)

    def test_file_added_and_removed_inside_a_pair(self):
        a, b = pair_ids(2)
        base = {a: elf({'old.c': (50, 0)}), b: elf({})}
        cur = {a: elf({'new.c': (30, 4)}), b: elf({})}
        md = self.render(base, cur)
        # a file only one pair has: its min and max are that pair's delta
        self.assertIn('| old.c | 1/1 | -50 (b: device/ex0/ex0.elf) | -50 (b: device/ex0/ex0.elf) | 0 | 0 |', md)
        self.assertIn('| new.c | 1/1 | +30 (b: device/ex0/ex0.elf) | +30 (b: device/ex0/ex0.elf) '
                      '| +4 (b: device/ex0/ex0.elf) | +4 (b: device/ex0/ex0.elf) |', md)

    def test_elfs_of_one_example_are_separate_pairs(self):
        app, loader = ('b', 'device/ex/app.elf'), ('b', 'device/ex/loader.elf')
        base = {app: elf({'x.c': (100, 0)}), loader: elf({'x.c': (10, 0)})}
        cur = {app: elf({'x.c': (100, 0)}), loader: elf({'x.c': (20, 0)})}
        md = self.render(base, cur)
        self.assertIn('| b: device/ex/loader.elf | +10 |', md)
        self.assertNotIn('| b: device/ex/app.elf |', md)

    def test_pair_totals_are_per_pair_not_summed_file_extremes(self):
        a, b = pair_ids(2)
        base = {a: elf({'x.c': (0, 0), 'y.c': (0, 0)}), b: elf({'x.c': (0, 0), 'y.c': (0, 0)})}
        cur = {a: elf({'x.c': (10, 0), 'y.c': (0, 0)}), b: elf({'x.c': (0, 0), 'y.c': (10, 0)})}
        md = self.render(base, cur)
        self.assertIn('| b: device/ex0/ex0.elf | +10 | 0 |', md)
        self.assertIn('| b: device/ex1/ex1.elf | +10 | 0 |', md)
        self.assertNotIn('+20', md)

    def test_ram_only_change(self):
        a, b = pair_ids(2)
        base = {a: elf({'x.c': (100, 64)}), b: elf({'x.c': (100, 64)})}
        cur = {a: elf({'x.c': (100, 128)}), b: elf({'x.c': (100, 64)})}
        md = self.render(base, cur)
        self.assertIn('| x.c | 1/2 | 0 | 0 | 0 | +64 (b: device/ex0/ex0.elf) |', md)

    def test_all_symbols_change_outside_src_is_reported(self):
        a, b = pair_ids(2)
        base = {a: elf({'x.c': (100, 0)}, all_syms=(1000, 0)), b: elf({'x.c': (1, 0)})}
        cur = {a: elf({'x.c': (100, 0)}, all_syms=(1024, 0)), b: elf({'x.c': (1, 0)})}
        md = self.render(base, cur)
        self.assertIn('| b: device/ex0/ex0.elf | 0 | 0 | +24 | 0 |', md)

    def test_single_pair_keeps_the_detail_table_and_incomplete_status(self):
        a, b = pair_ids(2)
        base = {a: elf({'x.c': (100, 0)}), b: None}
        cur = {a: elf({'x.c': (120, 0)}), b: elf({})}
        md = self.render(base, cur, failures=[(b, 'base', 'report', 'boom')])
        self.assertIn('Coverage (INCOMPLETE, membrowse):** 1 of 2 matched elf pairs compared, 1 changed', md)
        self.assertIn('FAILED `b: device/ex1/ex1.elf` base report: boom', md)
        self.assertIn('all symbols: Flash Δ +20, RAM Δ 0', md)
        self.assertIn('| x.c | 100 | 120 | +20 |', md)


def write_elf(path, sections, loads, bits=32, endian='<'):
    """An ELF with only headers (ELF32 little-endian unless `bits`/`endian` say
    otherwise): `sections` [(name, type, flags, addr, size)], `loads` PT_LOAD
    [(vaddr, paddr, memsz)]."""
    e = endian
    names = b'\0'
    offsets = []
    for name, *_ in sections + [('.shstrtab',)]:
        offsets.append(len(names))
        names += name.encode() + b'\0'
    ehsize, phentsize, shentsize = (52, 32, 40) if bits == 32 else (64, 56, 64)
    stroff = ehsize + phentsize * len(loads)
    shoff = stroff + len(names)
    shnum = len(sections) + 2
    ident = b'\x7fELF' + bytes([1 if bits == 32 else 2, 1 if e == '<' else 2, 1]) + bytes(9)
    addr = 'I' if bits == 32 else 'Q'
    out = ident + struct.pack(e + f'HHI{addr}{addr}{addr}IHHHHHH', 2, 40, 1, 0, ehsize, shoff, 0,
                              ehsize, phentsize, len(loads), shentsize, shnum, shnum - 1)
    for vaddr, paddr, memsz in loads:
        if bits == 32:
            out += struct.pack(e + '8I', 1, 0, vaddr, paddr, memsz, memsz, 0, 4)
        else:
            out += struct.pack(e + 'II6Q', 1, 0, 0, vaddr, paddr, memsz, memsz, 4)
    out += names + bytes(shentsize)
    fmt = e + ('10I' if bits == 32 else 'IIQQQQIIQQ')
    for off, (_, sh_type, flags, sh_addr, size) in zip(offsets, sections):
        out += struct.pack(fmt, off, sh_type, flags, sh_addr, 0, size, 0, 0, 4, 0)
    out += struct.pack(fmt, offsets[-1], 3, 0, 0, stroff, len(names), 0, 0, 1, 0)
    with open(path, 'wb') as f:
        f.write(out)


MAP_REGIONS = """Memory Configuration

Name             Origin             Length             Attributes
FLASH            0x10000000         0x00200000         xr
RAM              0x20000000         0x00040000         xrw
m_text           0x00000000         0x00010000         xr
*default*        0x00000000         0xffffffff

Linker script and memory map

"""
PROGBITS, NOBITS, INIT_ARRAY = 1, 8, 14
A, WA, AX = 0x2, 0x3, 0x6


def elf_with_map(tmp, sections, loads, map_text=MAP_REGIONS):
    """x.elf with only `sections`/`loads` headers, and x.elf.map holding `map_text`."""
    elf = os.path.join(tmp, 'x.elf')
    write_elf(elf, sections, loads)
    with open(elf + '.map', 'w') as f:
        f.write(map_text)
    return elf


class SectionBuckets(unittest.TestCase):
    def buckets(self, sections, loads, regions=MAP_REGIONS):
        with tempfile.TemporaryDirectory() as tmp:
            elf = elf_with_map(tmp, sections, loads, regions)
            return mc.section_buckets(elf, elf + '.map')

    def test_copied_code_counts_its_flash_load_image_and_ram(self):
        # pico .data is AX, not writable, yet loaded from flash to run in RAM
        b = self.buckets([('.data', PROGBITS, AX, 0x200000c0, 0x10)], [(0x200000c0, 0x10004734, 0x10)])
        self.assertEqual(b['.data'], {'flash', 'ram'})

    def test_a_relocation_typed_copy_counts_both(self):
        # metro_m4_express .relocate is SHT_REL
        b = self.buckets([('.relocate', 9, WA, 0x20000000, 0x20)], [(0x20000000, 0x77f0, 0x20)])
        self.assertEqual(b['.relocate'], {'flash', 'ram'})

    def test_a_flash_alias_run_address_is_flash_only(self):
        # xmc4500 links .text to the cached alias of the flash it loads from
        regions = MAP_REGIONS.replace('FLASH            0x10000000', 'FLASH            0x08000000')
        b = self.buckets([('.text', PROGBITS, AX, 0x08000000, 0x10)], [(0x08000000, 0x0c000000, 0x10)], regions)
        self.assertEqual(b['.text'], {'flash'})

    def test_writable_array_in_place_in_an_unrecognized_region_is_flash(self):
        # NXP .init_array: WA at its load address, in m_text
        b = self.buckets([('.init_array', INIT_ARRAY, WA, 0x54a4, 4)], [(0x200, 0x200, 0x6000)])
        self.assertEqual(b['.init_array'], {'flash'})

    def test_writable_data_in_place_takes_its_map_region(self):
        # RAM-only image (raspberrypi_zero): .data runs where it loads, in RAM
        b = self.buckets([('.data', PROGBITS, WA, 0x20000000, 0x10)], [(0x20000000, 0x20000000, 0x10)])
        self.assertEqual(b['.data'], {'ram'})

    def test_writable_data_in_place_in_an_unrecognized_region_fails(self):
        with self.assertRaisesRegex(RuntimeError, 'no map region recognized'):
            self.buckets([('.data', PROGBITS, WA, 0x100, 0x10)], [(0x100, 0x100, 0x10)])

    def test_nobits_is_ram_and_non_alloc_is_not_counted(self):
        b = self.buckets([('.bss', NOBITS, WA, 0x20000000, 0x10), ('.comment', PROGBITS, 0, 0, 0x40)],
                         [(0x20000000, 0x20000000, 0x10)])
        self.assertEqual(b['.bss'], {'ram'})
        self.assertEqual(b['.comment'], frozenset())

    def test_nobits_outside_every_load_segment_fails(self):
        with self.assertRaisesRegex(RuntimeError, 'no PT_LOAD'):
            self.buckets([('.bss', NOBITS, WA, 0x20000000, 0x10)], [])

    def test_elf64_big_endian_is_read(self):
        with tempfile.TemporaryDirectory() as tmp:
            elf = os.path.join(tmp, 'x.elf')
            write_elf(elf, [('.data', PROGBITS, WA, 0x80000000, 0x10)], [(0x80000000, 0x1000, 0x10)],
                      bits=64, endian='>')
            self.assertEqual(mc.elf_layout(elf), ([('.data', PROGBITS, WA, 0x80000000, 0x10),
                                                   ('.shstrtab', 3, 0, 0, 17)],
                                                  [(0x80000000, 0x1000, 0x10)]))

    def test_unreadable_elf_is_a_runtime_error_not_a_missing_tool(self):
        with tempfile.TemporaryDirectory() as tmp:
            truncated = os.path.join(tmp, 'x.elf')
            with open(truncated, 'wb') as f:
                f.write(b'\x7fELF\x01\x01\x01')
            for path in (truncated, os.path.join(tmp, 'missing.elf')):
                with self.assertRaisesRegex(RuntimeError, 'cannot read ELF headers'):
                    mc.elf_layout(path)

    def test_read_only_in_place_is_flash(self):
        b = self.buckets([('.text', PROGBITS, AX, 0x10000000, 0x10)], [(0x10000000, 0x10000000, 0x10)])
        self.assertEqual(b['.text'], {'flash'})

    def test_allocated_section_outside_every_load_segment_fails(self):
        with self.assertRaisesRegex(RuntimeError, 'no PT_LOAD'):
            self.buckets([('.text', PROGBITS, AX, 0x10000000, 0x10)], [])


def fake_map_section(section, children):
    """A linkermap Objectfile stand-in: `children` [(object path or None, size)]."""
    kids = [mock.Mock(path=(p, None), size=n) for p, n in children]
    return mock.Mock(section=section, children=kids)


class LinkermapSizes(unittest.TestCase):
    def sizes(self, parsed, filters=('/abs/src/',)):
        with tempfile.TemporaryDirectory() as tmp:
            elf = elf_with_map(tmp, [('.text', PROGBITS, AX, 0x10000000, 0x400),
                                     ('.data', PROGBITS, WA, 0x20000000, 0x10),
                                     ('.comment', PROGBITS, 0, 0, 0x40)],
                               [(0x10000000, 0x10000000, 0x400), (0x20000000, 0x10000400, 0x10)])
            parser = mock.Mock(parseSections=mock.Mock(return_value=parsed))
            with mock.patch.object(mc, '_linkermap', return_value=parser):
                return mc.linkermap_sizes(elf, list(filters))

    def test_same_named_files_stay_apart_and_buckets_come_from_the_elf(self):
        s = self.sizes([fake_map_section('FLASH', []),
                        fake_map_section('.text', [('d/CMakeFiles/x.dir/abs/src/class/a/x.c.o', 0x100),
                                                   ('d/CMakeFiles/x.dir/abs/src/portable/b/x.c.o', 0x80),
                                                   (None, 0x4)]),
                        fake_map_section('.data', [('d/CMakeFiles/x.dir/abs/src/class/a/x.c.o', 0x10)]),
                        fake_map_section('.comment', [('d/CMakeFiles/x.dir/abs/src/class/a/x.c.o', 0x40)])])
        self.assertEqual(s['files'], {'class/a/x.c': {'flash': 0x110, 'ram': 0x10},
                                      'portable/b/x.c': {'flash': 0x80, 'ram': 0}})
        self.assertEqual(s['all'], {'flash': 0x194, 'ram': 0x10})

    def test_output_section_missing_from_the_elf_fails(self):
        with self.assertRaisesRegex(RuntimeError, r'\.gone is not in'):
            self.sizes([fake_map_section('.gone', [('a.o', 4)])])

    def test_missing_linkermap_is_reported_as_a_missing_tool(self):
        with mock.patch('os.path.isfile', return_value=False):
            mc._linkermap.cache_clear()
            try:
                with self.assertRaises(FileNotFoundError):
                    mc._linkermap()
            finally:
                mc._linkermap.cache_clear()

    @unittest.skipUnless(os.path.isfile(os.path.join(REPO, 'tools', 'linkermap', 'linkermap.py')),
                         'tools/linkermap not fetched')
    def test_the_real_parser_reads_a_gnu_map(self):
        with tempfile.TemporaryDirectory() as tmp:
            elf = elf_with_map(tmp, [('.text', PROGBITS, AX, 0x10000000, 0x180)], [(0x10000000, 0x10000000, 0x180)],
                               MAP_REGIONS + '.text           0x10000000      0x180\n'
                               ' .text.a        0x10000000      0x100 obj/abs/src/class/a/x.c.o\n'
                               ' .text.b        0x10000100       0x80 obj/abs/src/portable/b/x.c.o\n')
            s = mc.linkermap_sizes(elf, ['/abs/src/'])
        self.assertEqual(s['files'], {'class/a/x.c': {'flash': 0x100, 'ram': 0},
                                      'portable/b/x.c': {'flash': 0x80, 'ram': 0}})


# .text in flash, .data copied from flash to RAM, .bss in RAM
THREE_SECTIONS = ([('.text', PROGBITS, AX, 0x10000000, 0x400),
                   ('.data', PROGBITS, WA, 0x20000000, 0x10),
                   ('.bss', NOBITS, WA, 0x20000010, 0x20)],
                  [(0x10000000, 0x10000000, 0x400), (0x20000000, 0x10000400, 0x30)])


class MembrowseSizes(unittest.TestCase):
    def sizes(self, symbols, filters=('/co/src/',)):
        with tempfile.TemporaryDirectory() as tmp:
            elf = elf_with_map(tmp, *THREE_SECTIONS)
            with mock.patch.object(mc, 'report_for_elf', return_value=fake_report(symbols)):
                return mc.membrowse_sizes(elf, list(filters))

    def test_object_paths_are_keyed_after_the_filter_and_buckets_come_from_the_elf(self):
        s = self.sizes(SYMS_BASE)
        self.assertEqual(s['files'], {'portable/synopsys/dwc2/dcd_dwc2.c': {'flash': 100, 'ram': 64}})
        self.assertEqual(s['all'], {'flash': 100 + 999, 'ram': 64})

    def test_keys_are_shared_across_checkout_prefixes(self):
        self.assertEqual(set(self.sizes(SYMS_BASE, ['/co/src/'])['files']),
                         set(self.sizes(SYMS_CUR, ['/co2/src/'])['files']))

    def test_data_counts_its_flash_load_image_and_ram(self):
        s = self.sizes([{'name': 'v', 'size': 8, 'section': '.data', 'object_file': 'o/co/src/x.c.obj'}])
        self.assertEqual(s['files'], {'x.c': {'flash': 8, 'ram': 8}})

    def test_falls_back_to_source_file_when_object_file_missing(self):
        s = self.sizes([{'name': 'a', 'size': 12, 'section': '.text',
                         'source_file': '/co/src/x.c', 'object_file': ''}])
        self.assertEqual(s['files'], {'x.c': {'flash': 12, 'ram': 0}})

    def test_sectionless_linker_symbols_are_not_counted(self):
        # nrf52's __StackLimit: size 16384, no section
        s = self.sizes([{'name': '__StackLimit', 'size': 16384, 'section': '', 'object_file': ''}])
        self.assertEqual(s['all'], {'flash': 0, 'ram': 0})

    def test_a_section_missing_from_the_elf_fails(self):
        with self.assertRaisesRegex(RuntimeError, r'section \.gone, not in'):
            self.sizes([{'name': 'g', 'size': 4, 'section': '.gone', 'object_file': 'o/co/src/g.c.obj'}])


class BloatySizes(unittest.TestCase):
    def sizes(self, csv_text, returncode=0):
        with tempfile.TemporaryDirectory() as tmp:
            elf = elf_with_map(tmp, *THREE_SECTIONS)
            done = subprocess.CompletedProcess([], returncode, stdout=csv_text, stderr='boom')
            with mock.patch.object(mc.subprocess, 'run', return_value=done):
                return mc.bloaty_sizes(elf, ['/abs/src/'])

    def test_compile_units_are_keyed_and_bucketed_by_the_elf(self):
        s = self.sizes('compileunits,sections,vmsize,filesize\n'
                       '/abs/src/class/a/x.c,.text,256,256\n'
                       '/abs/src/class/a/x.c,.data,16,16\n'
                       '/abs/src/class/a/x.c,.bss,32,0\n'
                       '/abs/src/class/a/x.c,.debug_info,0,900\n'
                       '/other/y.c,.text,64,64\n'
                       '[section .text],.text,8,8\n'
                       '[LOAD #0 [RX]],,100,100\n'
                       '[ELF Header],,52,52\n')
        self.assertEqual(s['files'], {'class/a/x.c': {'flash': 256 + 16, 'ram': 16 + 32}})
        self.assertEqual(s['all'], {'flash': 256 + 16 + 64 + 8, 'ram': 48})

    def test_missing_columns_fail(self):
        with self.assertRaisesRegex(RuntimeError, 'unexpected bloaty csv columns'):
            self.sizes('symbols,vmsize\n')

    def test_malformed_row_fails(self):
        with self.assertRaisesRegex(RuntimeError, 'malformed bloaty csv row'):
            self.sizes('compileunits,sections,vmsize,filesize\n/abs/src/a.c,.text,n/a,4\n')

    def test_bloaty_failure_is_reported(self):
        with self.assertRaisesRegex(RuntimeError, 'bloaty failed.*boom'):
            self.sizes('', returncode=1)

    def test_unknown_section_fails(self):
        with self.assertRaisesRegex(RuntimeError, r'\.gone is not in'):
            self.sizes('compileunits,sections,vmsize,filesize\n/abs/src/a.c,.gone,4,4\n')


if __name__ == '__main__':
    unittest.main()
