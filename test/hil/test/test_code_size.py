#!/usr/bin/env python3
"""Unit tests for tools/code_size.py.

glob.glob, the engines and the build steps are monkeypatched in the generate_sizes
and main tests, so no build and no real membrowse CLI invocation is needed.
"""
import concurrent.futures
import contextlib
import errno
import functools
import hashlib
import io
import json
import ntpath
import os
import re
import shutil
import signal
import struct
import subprocess
import sys
import tempfile
import time
import types
import unittest
from unittest import mock

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), '../../..'))
sys.path.insert(0, os.path.join(REPO, 'tools'))
import code_size as sd  # noqa: E402

posix_only = unittest.skipIf(os.name == 'nt', 'needs a POSIX shell and signals')
no_esp_on_windows = unittest.skipIf(os.name == 'nt', 'code_size.py refuses ESP-IDF on Windows')


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
            with mock.patch.object(sd, 'link_command', return_value=commands) as link, \
                 mock.patch.object(sd.subprocess, 'run', return_value=done) as run:
                sd.report_for_elf(elf)
            link.assert_called_once_with('ninja', build, elf)
            cmd = run.call_args.args[0]
            self.assertEqual(cmd[2:4], [elf, '/sdk/memmap.ld'])
            self.assertEqual(cmd[cmd.index('--def') + 1], 'X=1')
            self.assertEqual(run.call_args.kwargs['cwd'], build)

    @no_esp_on_windows
    def test_an_esp_idf_build_uses_its_generated_scripts(self):
        with tempfile.TemporaryDirectory() as build:
            open(os.path.join(build, 'build.ninja'), 'w').close()
            ld = os.path.join(build, 'esp-idf', 'esp_system', 'ld')
            os.makedirs(ld)
            commands = 'cc -T memory.ld -T sections.ld -T esp32s3.rom.ld -Wl,--defsym=X=1 -o x.elf\n'
            open(os.path.join(ld, 'memory.ld'), 'w').close()
            with mock.patch.object(sd, 'link_command', return_value=commands), \
                 self.assertRaisesRegex(RuntimeError, 'lacks one of'):
                sd._link_settings(os.path.join(build, 'x.elf'))
            open(os.path.join(ld, 'sections.ld'), 'w').close()
            with mock.patch.object(sd, 'link_command', return_value=commands):
                self.assertEqual(sd._link_settings(os.path.join(build, 'x.elf')),
                                 (build, [os.path.join(ld, 'memory.ld'), os.path.join(ld, 'sections.ld')], ['X=1']))

    def test_malformed_output_is_a_runtime_error(self):
        done = subprocess.CompletedProcess([], 0, 'not json', '')
        with mock.patch.object(sd, '_link_settings', return_value=('/b', ['m.ld'], [])), \
             mock.patch.object(sd.subprocess, 'run', return_value=done):
            with self.assertRaisesRegex(RuntimeError, 'malformed membrowse report'):
                sd.report_for_elf('/b/x.elf')


def unpad(md):
    """`md` with md_table's column padding removed, so tests match cells, not widths."""
    return re.sub(r' +\|', ' |', re.sub(r'\| +', '| ', md))


class MdTable(unittest.TestCase):
    def test_columns_line_up_as_plain_text(self):
        md = sd.md_table(['File', 'size Δ'], [['a/long/path.c', '+1'], ['└ f', '-120']])
        self.assertEqual(md, '| File          | size Δ |\n'
                             '|---------------|-------:|\n'
                             '| a/long/path.c |     +1 |\n'
                             '| └ f           |   -120 |')


class CompareReports(unittest.TestCase):
    def test_delta_table(self):
        md = unpad(sd.compare_reports({'portable/dcd_dwc2.c': {'flash': 3426, 'ram': 64}},
                                      {'portable/dcd_dwc2.c': {'flash': 3414, 'ram': 64}}))
        self.assertIn('| File (base → new) | Flash | Flash Δ | RAM | RAM Δ |', md)
        self.assertIn('| portable/dcd_dwc2.c | 3426 → 3414 | -12 | 64 → 64 | 0 |', md)
        self.assertIn('| TOTAL | 3426 → 3414 | -12 | 64 → 64 | 0 |', md)

    def test_base_and_new_values_line_up_down_each_column(self):
        md = sd.compare_reports({'a.c': {'flash': 1046, 'ram': 188}, 'b.c': {'flash': 3426, 'ram': 212}},
                                {'a.c': {'flash': 1082, 'ram': 220}, 'b.c': {'flash': 3414, 'ram': 212}})
        self.assertEqual(md, '| File (base → new) |       Flash | Flash Δ |       RAM | RAM Δ |\n'
                             '|-------------------|------------:|--------:|----------:|------:|\n'
                             '| a.c               | 1046 → 1082 |     +36 | 188 → 220 |   +32 |\n'
                             '| b.c               | 3426 → 3414 |     -12 | 212 → 212 |     0 |\n'
                             '|-------------------|-------------|---------|-----------|-------|\n'
                             '| TOTAL             | 4472 → 4496 |     +24 | 400 → 432 |   +32 |\n')

    def test_base_and_new_are_each_padded_to_their_own_widest(self):
        md = sd.compare_reports({'a.c': {'flash': 5, 'ram': 0}, 'b.c': {'flash': 12345, 'ram': 0}},
                                {'a.c': {'flash': 12000, 'ram': 0}, 'b.c': {'flash': 6, 'ram': 0}})
        rows = md.split('\n')
        self.assertIn('|     5 → 12000 |', rows[3])
        self.assertIn('| 12345 →     6 |', rows[2])
        self.assertIn('| 12350 → 12006 |', rows[5])  # TOTAL, under its rule

    def test_labels_name_the_sides_in_the_header(self):
        md = sd.compare_reports({'x.c': {'flash': 1, 'ram': 0}}, {'x.c': {'flash': 2, 'ram': 0}}, ('abc1234', 'def5678'))
        self.assertTrue(md.startswith('| File (abc1234 → def5678) |'))

    def test_the_total_row_sits_under_a_rule_in_every_table(self):
        base, cur = elf({'x.c': (1, 0)}), elf({'x.c': (3, 0)})
        for md in (sd.compare_reports(base['files'], cur['files']), sd.delta_table(base, cur, False, 'membrowse'),
                   sd.size_table(cur, False, 'membrowse')):
            lines = md.rstrip('\n').split('\n')
            self.assertEqual(lines[-2], lines[1].replace(':', '-'))
            self.assertTrue(lines[-1].startswith('| TOTAL '))

    def test_no_per_file_changes_row_spans_the_columns(self):
        md = unpad(sd.compare_reports({'x.c': {'flash': 1, 'ram': 0}}, {'x.c': {'flash': 1, 'ram': 0}}))
        self.assertIn('| _no per-file changes_ | | | | |', md)


def elf(files, all_syms=None, symbols=None):
    """An engine's sizes: files as {path: (flash, ram)}; the all total defaults to their sum.
    Flash is one .text symbol `f`, RAM one .bss symbol `v`, unless `symbols` gives
    {path: {section: {name: size}}}; sections are the symbols' sums."""
    if symbols is None:
        symbols = {p: {s: {n: v} for s, n, v in (('.text', 'f', f), ('.bss', 'v', r)) if v}
                   for p, (f, r) in files.items()}
    files = {p: {'flash': f, 'ram': r} for p, (f, r) in files.items()}
    if all_syms is None:
        all_syms = (sum(v['flash'] for v in files.values()), sum(v['ram'] for v in files.values()))
    sections = {p: {s: sum(names.values()) for s, names in secs.items()} for p, secs in symbols.items()}
    return {'files': files, 'all': {'flash': all_syms[0], 'ram': all_syms[1]},
            'sections': sections, 'symbols': symbols}


def pair_ids(n, board='b'):
    return [(board, f'device/ex{i}/ex{i}.elf') for i in range(n)]


class PairElfs(unittest.TestCase):
    def test_unmatched_and_failed_elfs_are_not_paired(self):
        a, b, c, d = pair_ids(4)
        base = {a: elf({'x.c': (1, 0)}), b: elf({}), c: None}
        cur = {a: elf({'x.c': (1, 0)}), c: elf({}), d: elf({})}
        pairs, base_only, cur_only = sd.pair_elfs(base, cur)
        self.assertEqual(list(pairs), [a])     # c's base report failed
        self.assertEqual(base_only, [b])
        self.assertEqual(cur_only, [d])


class RenderReport(unittest.TestCase):
    def report(self, *args, **kwargs):
        return unpad(sd.render_report(*args, **kwargs))

    def test_one_elf_is_a_linkermap_table_with_percent_of_the_filtered_total(self):
        a, = pair_ids(1)
        md = self.report({a: elf({'x.c': (60, 20), 'y.c': (20, 0)}, all_syms=(500, 40))}, 'membrowse')
        self.assertIn('**Coverage (complete, membrowse):** 1 of 1 elfs sized', md)
        self.assertIn('TinyUSB Flash 80, RAM 20; all symbols Flash 500, RAM 40', md)
        self.assertIn('| File | .text | .bss | size | % |', md)
        self.assertLess(md.index('| x.c | 60 | 20 | 80 | 80.0% |'), md.index('| y.c | 20 | 0 | 20 | 20.0% |'))
        self.assertIn('| TOTAL | 80 | 20 | 100 | 100.0% |', md)
        self.assertNotIn('<details>', md)

    def test_symbols_list_each_files_symbols_under_it(self):
        a, = pair_ids(1)
        syms = {'x.c': {'.text': {'f': 10, 'g': 30}, '.bss': {'v': 10}}}
        md = self.report({a: elf({'x.c': (40, 10)}, symbols=syms)}, 'membrowse', symbols=True)
        self.assertIn('| File / symbol | .text | .bss | size | % |', md)
        # a tie sorts by (section, name)
        rows = ['| x.c | 40 | 10 | 50 | 100.0% |', '| └ g | 30 | 0 | 30 | 60.0% |',
                '| └ v | 0 | 10 | 10 | 20.0% |', '| └ f | 10 | 0 | 10 | 20.0% |']
        self.assertEqual([md.index(r) for r in rows], sorted(md.index(r) for r in rows))

    def test_linkermap_symbols_are_labelled_input_sections(self):
        a, = pair_ids(1)
        md = self.report({a: elf({'x.c': (1, 0)})}, 'linkermap', symbols=True)
        self.assertIn('| File / input section | .text | size | % |', md)

    def test_many_elfs_get_a_summary_and_a_table_each_in_details(self):
        a, b = pair_ids(2)
        md = self.report({a: elf({'x.c': (1, 0)}), b: elf({'x.c': (2, 0)})}, 'membrowse', boards=['b'])
        self.assertIn('- boards: `b`', md)
        self.assertIn('| b: device/ex1/ex1.elf | 2 | 0 | 2 | 0 |', md)
        self.assertEqual(md.count('<details>'), 2)
        self.assertLess(md.index('| Elf |'), md.index('<details>'))

    def test_a_failed_elf_makes_the_report_incomplete(self):
        a, b = pair_ids(2)
        md = self.report({a: elf({'x.c': (1, 0)}), b: None}, 'membrowse', [(b, 'report', 'boom')])
        self.assertIn('**Coverage (INCOMPLETE, membrowse):** 1 of 2 elfs sized', md)
        self.assertIn('- FAILED `b: device/ex1/ex1.elf` report: boom', md)

    def test_nothing_sized_is_incomplete(self):
        md = self.report({}, 'membrowse', [(('b', None), 'build', 'build failed, see log')])
        self.assertIn('INCOMPLETE', md)
        self.assertIn('_no sized elfs_', md)


class RenderPairs(unittest.TestCase):
    def render(self, base, cur, failures=(), symbols=False, engine='membrowse'):
        pairs, base_only, cur_only = sd.pair_elfs(base, cur)
        return unpad(sd.render_pairs(pairs, len(base.keys() & cur.keys()), engine, base_only, cur_only,
                                     failures, symbols=symbols))

    def test_a_pair_gets_a_section_delta_table(self):
        a, = pair_ids(1)
        md = self.render({a: elf({'x.c': (100, 8), 'y.c': (5, 0)})}, {a: elf({'x.c': (120, 4), 'y.c': (5, 0)})})
        self.assertIn('| File | .text | .bss | size Δ |', md)
        self.assertIn('| x.c | +20 | -4 | +16 |', md)
        self.assertIn('| TOTAL | +20 | -4 | +16 |', md)
        self.assertNotIn('| y.c |', md)

    def test_sections_cancelling_in_flash_still_mark_the_pair_changed(self):
        # +10 .text / -10 .rodata: flash is unchanged, the sections are not
        a, b = pair_ids(2)
        syms = lambda t, r: {'x.c': {'.text': {'f': t}, '.rodata': {'k': r}}}
        base = {a: elf({'x.c': (110, 0)}, symbols=syms(100, 10)), b: elf({'x.c': (1, 0)})}
        cur = {a: elf({'x.c': (110, 0)}, symbols=syms(110, 0)), b: elf({'x.c': (1, 0)})}
        md = self.render(base, cur)
        self.assertIn('2 of 2 matched elf pairs compared, 1 changed', md)
        self.assertIn('| x.c | +10 | -10 | 0 |', md)

    def test_symbols_cancelling_in_a_section_show_only_with_symbols(self):
        a, = pair_ids(1)
        syms = lambda f, g: {'x.c': {'.text': {'f': f, 'g': g}}}
        base = {a: elf({'x.c': (30, 0)}, symbols=syms(10, 20))}
        cur = {a: elf({'x.c': (30, 0)}, symbols=syms(20, 10))}
        self.assertIn('1 of 1 matched elf pairs compared, 0 changed', self.render(base, cur))
        md = self.render(base, cur, symbols=True)
        self.assertIn('1 of 1 matched elf pairs compared, 1 changed', md)
        # the file row stays, zero, as the parent of its changed symbols
        self.assertIn('| File / symbol | .text | size Δ |', md)
        self.assertIn('| x.c | 0 | 0 |', md)
        self.assertIn('| └ f | +10 | +10 |', md)
        self.assertIn('| └ g | -10 | -10 |', md)

    def test_linkermap_symbols_are_labelled_input_sections(self):
        a, = pair_ids(1)
        md = self.render({a: elf({'x.c': (1, 0)})}, {a: elf({'x.c': (2, 0)})}, symbols=True, engine='linkermap')
        self.assertIn('| File / input section | .text | size Δ |', md)

    def test_many_pairs_put_the_tables_of_changed_pairs_only_in_details(self):
        a, b = pair_ids(2)
        md = self.render({a: elf({'x.c': (1, 0)}), b: elf({'x.c': (1, 0)})},
                         {a: elf({'x.c': (3, 0)}), b: elf({'x.c': (1, 0)})})
        self.assertEqual(md.count('| File | .text | size Δ |'), 1)
        self.assertIn('<details><summary>b: device/ex0/ex0.elf</summary>', md)

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
        # nothing compared is INCOMPLETE even without a failure or an unmatched elf
        for base, cur, failures in (({}, {}, [(('b', None), 'current', 'build', 'failed')]),
                                    ({a: elf({})}, {b: elf({})}, []), ({}, {}, [])):
            md = self.render(base, cur, failures)
            self.assertIn('Coverage (INCOMPLETE, membrowse):** 0 of 0 matched', md)
            self.assertIn('_no comparable pairs_', md)
            self.assertNotIn('_no changes_', md)
            if failures:
                self.assertIn('FAILED `b` current build: failed', md)
            self.assertEqual(sd.compare_sides(base, cur, 'membrowse', failures)[3]['status'], 'INCOMPLETE')

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

    def test_single_pair_keeps_the_delta_table_and_incomplete_status(self):
        a, b = pair_ids(2)
        base = {a: elf({'x.c': (100, 0)}), b: None}
        cur = {a: elf({'x.c': (120, 0)}), b: elf({})}
        md = self.render(base, cur, failures=[(b, 'base', 'report', 'boom')])
        self.assertIn('Coverage (INCOMPLETE, membrowse):** 1 of 2 matched elf pairs compared, 1 changed', md)
        self.assertIn('FAILED `b: device/ex1/ex1.elf` base report: boom', md)
        self.assertIn('all symbols: Flash Δ +20, RAM Δ 0', md)
        self.assertIn('| x.c | 100 → 120 | +20 | 0 → 0 | 0 |', md)


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
            return sd.section_buckets(elf, elf + '.map')

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

    def test_an_esp_idf_app_is_split_by_its_flash_mapped_sections(self):
        # every section runs where it loads; .flash* is flash-mapped, the rest the bootloader
        # copies from the image into RAM (real S3 names, incl. writable .flash.rodata)
        sections = [('.flash.appdesc', PROGBITS, A, 0x3c020020, 0x100),
                    ('.flash.rodata', PROGBITS, WA, 0x3c020120, 0x10),
                    ('.flash.text', PROGBITS, AX, 0x42000020, 0x10),
                    ('.flash_rodata_dummy', NOBITS, WA, 0x3c000020, 0x20000),
                    ('.iram0.text', PROGBITS, AX, 0x40374404, 0x10),
                    ('.dram0.data', PROGBITS, WA, 0x3fc88000, 0x10),
                    ('.rtc.force_slow', PROGBITS, WA, 0x50000000, 0x10),
                    ('.dram0.bss', NOBITS, WA, 0x3fc96d20, 0x10)]
        with tempfile.TemporaryDirectory() as tmp:
            elf = os.path.join(tmp, 'x.elf')
            write_elf(elf, sections, [(0, 0, 0xffffffff)])
            b = sd.section_buckets(elf, os.path.join(tmp, 'no.map'))  # an IDF app never reads the map
        self.assertEqual({n: set(v) for n, v in b.items() if n != '.shstrtab'},
                         {'.flash.appdesc': {'flash'}, '.flash.rodata': {'flash'}, '.flash.text': {'flash'},
                          '.flash_rodata_dummy': set(), '.iram0.text': {'flash', 'ram'},
                          '.dram0.data': {'flash', 'ram'}, '.rtc.force_slow': {'flash', 'ram'},
                          '.dram0.bss': {'ram'}})

    def test_overlapping_flash_and_ram_regions_still_fail_outside_esp_idf(self):
        regions = MAP_REGIONS.replace('m_text           0x00000000         0x00010000         xr',
                                      'drom0_0_seg      0x3c000020         0x01ffffe0         r\n'
                                      'extern_ram_seg   0x3c000020         0x01ffffe0         xrw')
        with self.assertRaisesRegex(RuntimeError, 'no map region recognized'):
            self.buckets([('.ext_ram.data', PROGBITS, WA, 0x3c040020, 0x10)],
                         [(0x3c040020, 0x3c040020, 0x200)], regions)

    def test_an_esp_idf_map_is_named_after_the_project(self):
        with tempfile.TemporaryDirectory() as tmp:
            elf = os.path.join(tmp, 'x.elf')
            self.assertEqual(sd._map_path(elf), elf + '.map')  # neither: the usual name, reported missing
            open(os.path.join(tmp, 'x.map'), 'w').close()
            self.assertEqual(sd._map_path(elf), os.path.join(tmp, 'x.map'))
            open(elf + '.map', 'w').close()
            self.assertEqual(sd._map_path(elf), elf + '.map')

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
            self.assertEqual(sd.elf_layout(elf), ([('.data', PROGBITS, WA, 0x80000000, 0x10),
                                                   ('.shstrtab', 3, 0, 0, 17)],
                                                  [(0x80000000, 0x1000, 0x10)]))

    def test_unreadable_elf_is_a_runtime_error_not_a_missing_tool(self):
        with tempfile.TemporaryDirectory() as tmp:
            truncated = os.path.join(tmp, 'x.elf')
            with open(truncated, 'wb') as f:
                f.write(b'\x7fELF\x01\x01\x01')
            for path in (truncated, os.path.join(tmp, 'missing.elf')):
                with self.assertRaisesRegex(RuntimeError, 'cannot read ELF headers'):
                    sd.elf_layout(path)

    def test_read_only_in_place_is_flash(self):
        b = self.buckets([('.text', PROGBITS, AX, 0x10000000, 0x10)], [(0x10000000, 0x10000000, 0x10)])
        self.assertEqual(b['.text'], {'flash'})

    def test_allocated_section_outside_every_load_segment_fails(self):
        with self.assertRaisesRegex(RuntimeError, 'no PT_LOAD'):
            self.buckets([('.text', PROGBITS, AX, 0x10000000, 0x10)], [])


def fake_map_section(section, children):
    """A linkermap Objectfile stand-in: `children` [(object path or None, size[, input section])]."""
    kids = [mock.Mock(path=(c[0], None), size=c[1], section=c[2] if len(c) > 2 else section)
            for c in children]
    return mock.Mock(section=section, children=kids)


class LinkermapSizes(unittest.TestCase):
    def sizes(self, parsed, filters=('/abs/src/',)):
        with tempfile.TemporaryDirectory() as tmp:
            elf = elf_with_map(tmp, [('.text', PROGBITS, AX, 0x10000000, 0x400),
                                     ('.data', PROGBITS, WA, 0x20000000, 0x10),
                                     ('.comment', PROGBITS, 0, 0, 0x40)],
                               [(0x10000000, 0x10000000, 0x400), (0x20000000, 0x10000400, 0x10)])
            parser = mock.Mock(parseSections=mock.Mock(return_value=parsed))
            with mock.patch.object(sd, '_linkermap', return_value=parser):
                return sd.linkermap_sizes(elf, list(filters))

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
        # the non-allocated .comment is in no bucket, so in no section total either
        self.assertEqual(s['sections'], {'class/a/x.c': {'.text': 0x100, '.data': 0x10},
                                         'portable/b/x.c': {'.text': 0x80}})

    def test_symbols_are_input_sections_counted_once(self):
        # one input section holding several labels is one row; the same name adds up
        s = self.sizes([fake_map_section('.text', [('o/abs/src/x.c.o', 0x20, '.text.a'),
                                                   ('o/abs/src/x.c.o', 0x10, '.text.a'),
                                                   ('o/abs/src/x.c.o', 0x08, '.text.b'),
                                                   ('lib.a', 0x04, '.text.c')])])
        self.assertEqual(s['symbols'], {'x.c': {'.text': {'.text.a': 0x30, '.text.b': 0x08}}})

    def test_output_section_missing_from_the_elf_fails(self):
        with self.assertRaisesRegex(RuntimeError, r'\.gone is not in'):
            self.sizes([fake_map_section('.gone', [('a.o', 4)])])

    def test_missing_linkermap_is_reported_as_a_missing_tool(self):
        with mock.patch('os.path.isfile', return_value=False):
            sd._linkermap.cache_clear()
            try:
                with self.assertRaises(FileNotFoundError):
                    sd._linkermap()
            finally:
                sd._linkermap.cache_clear()

    @unittest.skipUnless(os.path.isfile(os.path.join(REPO, 'tools', 'linkermap', 'linkermap.py')),
                         'tools/linkermap not fetched')
    def test_the_real_parser_reads_a_gnu_map(self):
        with tempfile.TemporaryDirectory() as tmp:
            elf = elf_with_map(tmp, [('.text', PROGBITS, AX, 0x10000000, 0x180)], [(0x10000000, 0x10000000, 0x180)],
                               MAP_REGIONS + '.text           0x10000000      0x180\n'
                               ' .text.a        0x10000000      0x100 obj/abs/src/class/a/x.c.o\n'
                               ' .text.b        0x10000100       0x80 obj/abs/src/portable/b/x.c.o\n')
            s = sd.linkermap_sizes(elf, ['/abs/src/'])
        self.assertEqual(s['files'], {'class/a/x.c': {'flash': 0x100, 'ram': 0},
                                      'portable/b/x.c': {'flash': 0x80, 'ram': 0}})


# .text in flash, .data copied from flash to RAM, .bss in RAM
THREE_SECTIONS = ([('.text', PROGBITS, AX, 0x10000000, 0x400),
                   ('.data', PROGBITS, WA, 0x20000000, 0x10),
                   ('.bss', NOBITS, WA, 0x20000010, 0x20)],
                  [(0x10000000, 0x10000000, 0x400), (0x20000000, 0x10000400, 0x30)])


class DwarfSources(unittest.TestCase):
    @unittest.skipUnless(shutil.which('gcc') and os.name != 'nt', 'needs a gcc that builds an elf with DWARF')
    def test_compile_units_are_found_by_basename(self):
        with tempfile.TemporaryDirectory() as tmp:
            for rel, code in (('src/device/usbd.c', 'int usbd(void) { return 1; }\n'),
                              ('main.c', 'int usbd(void);\nint main(void) { return usbd(); }\n')):
                os.makedirs(os.path.dirname(os.path.join(tmp, rel)), exist_ok=True)
                with open(os.path.join(tmp, rel), 'w') as f:
                    f.write(code)
            subprocess.run(['gcc', '-g', '-o', 'x.elf', 'main.c', 'src/device/usbd.c'], cwd=tmp, check=True)
            sources = sd._dwarf_sources(os.path.join(tmp, 'x.elf'))
            self.assertEqual(sources['usbd.c'], {os.path.join(os.path.realpath(tmp), 'src', 'device', 'usbd.c')})


class SourcePath(unittest.TestCase):
    def path(self, sym, sources):
        with mock.patch.object(sd, '_dwarf_sources', return_value=sources):
            return sd._source_path(sym, 'x.elf', ['/c/src/'])

    def test_an_object_path_or_full_source_is_kept(self):
        self.assertEqual(self.path({'object_file': '/c/src/tusb.c.obj', 'source_file': 'tusb.c'}, {}),
                         '/c/src/tusb.c.obj')
        self.assertEqual(self.path({'source_file': '/c/src/tusb.c'}, {}), '/c/src/tusb.c')

    def test_a_bare_source_name_takes_its_one_compile_unit(self):
        # ESP-IDF archive member: membrowse keeps only the basename
        self.assertEqual(self.path({'source_file': 'usbd.c'}, {'usbd.c': {'/c/src/device/usbd.c'}}),
                         '/c/src/device/usbd.c')

    def test_an_ambiguous_name_fails_only_when_one_candidate_is_filtered(self):
        self.assertEqual(self.path({'source_file': 'port.c'}, {'port.c': {'/idf/a/port.c', '/idf/b/port.c'}}),
                         'port.c')
        with self.assertRaisesRegex(RuntimeError, 'several compile units'):
            self.path({'source_file': 'x.c', 'name': 'f'}, {'x.c': {'/c/src/x.c', '/idf/x.c'}})


class MembrowseSizes(unittest.TestCase):
    def sizes(self, symbols, filters=('/co/src/',)):
        with tempfile.TemporaryDirectory() as tmp:
            elf = elf_with_map(tmp, *THREE_SECTIONS)
            with mock.patch.object(sd, 'report_for_elf', return_value=fake_report(symbols)):
                return sd.membrowse_sizes(elf, list(filters))

    def test_object_paths_are_keyed_after_the_filter_and_buckets_come_from_the_elf(self):
        s = self.sizes(SYMS_BASE)
        self.assertEqual(s['files'], {'portable/synopsys/dwc2/dcd_dwc2.c': {'flash': 100, 'ram': 64}})
        self.assertEqual(s['all'], {'flash': 100 + 999, 'ram': 64})

    def test_symbols_are_per_section_and_same_names_add_up(self):
        s = self.sizes([{'name': 'f', 'size': 10, 'section': '.text', 'object_file': 'o/co/src/x.c.obj'},
                        {'name': 'f', 'size': 6, 'section': '.text', 'object_file': 'o/co/src/x.c.obj'},
                        {'name': 'v', 'size': 8, 'section': '.data', 'object_file': 'o/co/src/x.c.obj'},
                        {'name': 'u', 'size': 4, 'section': '.text', 'object_file': 'o/elsewhere/u.c.obj'}])
        self.assertEqual(s['symbols'], {'x.c': {'.text': {'f': 16}, '.data': {'v': 8}}})

    def test_keys_are_shared_across_checkout_prefixes(self):
        self.assertEqual(set(self.sizes(SYMS_BASE, ['/co/src/'])['files']),
                         set(self.sizes(SYMS_CUR, ['/co2/src/'])['files']))

    def test_data_counts_its_flash_load_image_and_ram(self):
        s = self.sizes([{'name': 'v', 'size': 8, 'section': '.data', 'object_file': 'o/co/src/x.c.obj'}])
        self.assertEqual(s['files'], {'x.c': {'flash': 8, 'ram': 8}})
        self.assertEqual(s['sections'], {'x.c': {'.data': 8}})

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

    @staticmethod
    def pins(text):
        return re.findall(r'pip3? install [^\n]*?\bmembrowse\b(==[^\s\'"]+)?', text.replace('\\\n', ' '))

    def test_pins_reports_unpinned_installs_in_every_shell_form(self):
        self.assertEqual(self.pins('pip3 install membrowse\n'
                                   'pip install \\\n  --only-binary :all: \\\n  membrowse\n'
                                   'python -m pip install membrowse\n'), ['', '', ''])

    def test_every_ci_install_pins_the_version_the_install_hint_names(self):
        self.assertEqual(self.pins("pip install membrowse==1.2.12rc1 'x'; pip install membrowse >/dev/null"),
                         ['==1.2.12rc1', ''])
        installs = []
        for root, _, files in os.walk(os.path.join(REPO, '.github')):
            for name in files:
                with open(os.path.join(root, name), errors='replace') as f:
                    installs += self.pins(f.read())
        self.assertGreaterEqual(len(installs), 4)
        self.assertEqual(set(installs), {f'=={sd.MEMBROWSE_CI_VERSION}'})
        self.assertIn(f'membrowse=={sd.MEMBROWSE_CI_VERSION}', sd.ENGINES['membrowse'].install)


class BloatySizes(unittest.TestCase):
    def sizes(self, csv_text, returncode=0):
        with tempfile.TemporaryDirectory() as tmp:
            elf = elf_with_map(tmp, *THREE_SECTIONS)
            done = subprocess.CompletedProcess([], returncode, stdout=csv_text, stderr='boom')
            with mock.patch.object(sd.subprocess, 'run', return_value=done):
                return sd.bloaty_sizes(elf, ['/abs/src/'])

    def test_compile_units_are_keyed_and_bucketed_by_the_elf(self):
        s = self.sizes('compileunits,sections,symbols,vmsize,filesize\n'
                       '/abs/src/class/a/x.c,.text,f,200,200\n'
                       '/abs/src/class/a/x.c,.text,g,56,56\n'
                       '/abs/src/class/a/x.c,.data,v,16,16\n'
                       '/abs/src/class/a/x.c,.bss,b,32,0\n'
                       '/abs/src/class/a/x.c,.debug_info,[section .debug_info],0,900\n'
                       '/other/y.c,.text,y,64,64\n'
                       '[section .text],.text,[section .text],8,8\n'
                       '[LOAD #0 [RX]],,[LOAD #0 [RX]],100,100\n'
                       '[ELF Header],,[ELF Header],52,52\n')
        self.assertEqual(s['files'], {'class/a/x.c': {'flash': 256 + 16, 'ram': 16 + 32}})
        self.assertEqual(s['all'], {'flash': 256 + 16 + 64 + 8, 'ram': 48})
        self.assertEqual(s['sections'], {'class/a/x.c': {'.text': 256, '.data': 16, '.bss': 32}})
        self.assertEqual(s['symbols'], {'class/a/x.c': {'.text': {'f': 200, 'g': 56},
                                                        '.data': {'v': 16}, '.bss': {'b': 32}}})

    def test_missing_columns_fail(self):
        with self.assertRaisesRegex(RuntimeError, 'unexpected bloaty csv columns'):
            self.sizes('symbols,vmsize\n')

    def test_malformed_row_fails(self):
        with self.assertRaisesRegex(RuntimeError, 'malformed bloaty csv row'):
            self.sizes('compileunits,sections,symbols,vmsize,filesize\n/abs/src/a.c,.text,a,n/a,4\n')

    def test_bloaty_failure_is_reported(self):
        with self.assertRaisesRegex(RuntimeError, 'bloaty failed.*boom'):
            self.sizes('', returncode=1)

    def test_unknown_section_fails(self):
        with self.assertRaisesRegex(RuntimeError, r'\.gone is not in'):
            self.sizes('compileunits,sections,symbols,vmsize,filesize\n/abs/src/a.c,.gone,g,4,4\n')


def _run_capturing_stdout(*args, **kwargs):
    buf = io.StringIO()
    with contextlib.redirect_stdout(buf):
        result = sd.generate_sizes(*args, **kwargs)
    return result, buf.getvalue()


def _elf(flash):
    return elf({'x.c': (flash, 0)})


def _engine(name, sizes):
    """Patch ENGINES[name] to size elfs with `sizes` (a Mock or its side_effect)."""
    if not isinstance(sizes, mock.Mock):
        sizes = mock.Mock(side_effect=sizes)
    return mock.patch.dict(sd.ENGINES,
                           {name: sd.ENGINES[name]._replace(sizes=sizes)})


class GenerateSizes(unittest.TestCase):
    def test_no_elfs_errors_and_returns_nothing(self):
        with mock.patch('glob.glob', return_value=[]):
            (sizes, errors), out = _run_capturing_stdout('/fake/build', ['/fake/src/'])
        self.assertEqual(sizes, {})
        self.assertEqual(errors[0][0], None)
        self.assertIn('no .elf files', errors[0][1])
        self.assertEqual(out, '')

    def test_membrowse_cli_missing_errors_and_returns_nothing(self):
        # subprocess.run(['membrowse', ...]) raises FileNotFoundError when the
        # CLI isn't installed - must not surface as a bare traceback after the
        # base+branch builds already ran (minutes of work).
        with mock.patch('glob.glob', return_value=['/fake/build/ex/ex.elf']), \
             _engine('membrowse', mock.Mock(side_effect=FileNotFoundError('membrowse'))):
            (sizes, errors), out = _run_capturing_stdout('/fake/build', ['/fake/src/'])
        self.assertEqual(sizes, {})
        self.assertEqual(errors[0][0], None)
        self.assertIn('pip install membrowse', errors[0][1])
        self.assertIn('another --engine', errors[0][1])
        self.assertEqual(out, '')

    def test_fetched_tools_under_deps_are_not_examples(self):
        # picotool's enc_bootloader.elf has no linker map, so sizing it fails every rp2 board
        sizer = mock.Mock(return_value=_elf(7))
        with tempfile.TemporaryDirectory() as build:
            _touch(build, 'device/ex/ex.elf', '_deps/picotool/enc_bootloader.elf')
            with _engine('bloaty', sizer):
                sizes, errors = sd.generate_sizes(build, ['src/'], engine='bloaty')
        self.assertEqual((list(sizes), errors), (['device/ex/ex.elf'], []))

    def test_the_selected_engine_sizes_each_elf(self):
        bloaty = mock.Mock(return_value=_elf(7))
        with mock.patch('glob.glob', return_value=['/fake/build/ex/ex.elf']), \
             _engine('bloaty', bloaty):
            sizes, errors = sd.generate_sizes('/fake/build', ['src/'], engine='bloaty')
        bloaty.assert_called_once_with('/fake/build/ex/ex.elf', ['src/'])
        self.assertEqual((sizes, errors), ({'ex/ex.elf': _elf(7)}, []))

    def test_a_missing_engine_tool_names_its_install(self):
        with mock.patch('glob.glob', return_value=['/fake/build/ex/ex.elf']), \
             _engine('bloaty', mock.Mock(side_effect=FileNotFoundError('bloaty'))):
            (sizes, errors), out = _run_capturing_stdout('/fake/build', ['src/'], engine='bloaty')
        self.assertEqual(sizes, {})
        (elf_path, message), = errors
        self.assertIsNone(elf_path)
        self.assertIn('bloaty not found', message)
        self.assertIn('github.com/google/bloaty', message)
        self.assertEqual(out, '')

    def test_an_absent_engine_tool_is_missing(self):
        with mock.patch.object(sd.shutil, 'which', side_effect=lambda cmd: None if cmd == 'bloaty' else '/bin/x'):
            self.assertTrue(sd.engine_missing('bloaty'))
            self.assertFalse(sd.engine_missing('membrowse'))
        sd._linkermap.cache_clear()
        try:
            with mock.patch('os.path.isfile', return_value=False):
                self.assertTrue(sd.engine_missing('linkermap'))
        finally:
            sd._linkermap.cache_clear()

    def test_an_exit_while_sizing_cancels_the_elfs_not_yet_started(self):
        """Executor.map cancels its pending calls when its results stop being read, so
        exit_on_termination() waits only for the elfs being sized."""
        elfs = [f'/fake/build/ex{i}/ex{i}.elf' for i in range(200)]
        sized = []

        def sizes(elf, _filters):
            if elf == elfs[0]:
                raise SystemExit(143)  # as exit_on_termination()'s handler, surfacing from the map
            time.sleep(0.02)
            sized.append(elf)
            return _elf(4)
        # one worker: elfs[0]'s exit is read long before the others could all be sized
        one_worker = functools.partial(concurrent.futures.ThreadPoolExecutor, max_workers=1)
        with mock.patch('glob.glob', return_value=elfs), _engine('membrowse', sizes), \
             mock.patch('concurrent.futures.ThreadPoolExecutor', one_worker), self.assertRaises(SystemExit):
            sd.generate_sizes('/fake/build', ['build/'])
        self.assertLess(len(sized), len(elfs) - 1)

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
        self.assertIn('membrowse report failed', errors[0][1])
        self.assertEqual(out, '')

    def test_each_elf_is_kept_by_its_relative_path(self):
        # no averaging or summing across elfs: every elf, including two of one
        # example (app + bootloader), is its own pairing identity
        # keyed by elf: the reports run on a thread pool, in no fixed order
        flash = {'/fake/build/ex/app.elf': 100, '/fake/build/ex/loader.elf': 200,
                 '/fake/build/ex2/ex2.elf': 300}
        with mock.patch('glob.glob', return_value=list(flash)), \
             _engine('membrowse', lambda elf, _filters: _elf(flash[elf])):
            sizes, errors = sd.generate_sizes('/fake/build', ['build/'])
        self.assertEqual(errors, [])
        self.assertEqual({rel: s['files']['x.c']['flash'] for rel, s in sizes.items()},
                         {'ex/app.elf': 100, 'ex/loader.elf': 200, 'ex2/ex2.elf': 300})
        self.assertEqual(sizes['ex/app.elf']['all'], {'flash': 100, 'ram': 0})


needs_proc = unittest.skipUnless(os.path.exists('/proc/self/stat'), 'needs /proc')


class BuildOutput(unittest.TestCase):
    @staticmethod
    def excerpt(stdout, stderr='', lines=20):
        return sd.output_excerpt(subprocess.CompletedProcess([], 1, stdout, stderr), lines)

    def test_a_compile_error_on_stdout_is_shown_when_stderr_is_empty(self):
        # ninja reports the failing compile on stdout; stderr stays empty
        self.assertEqual(self.excerpt('a\nbad.c:1: error: x undeclared\nninja: build stopped\n', lines=2),
                         ['bad.c:1: error: x undeclared', 'ninja: build stopped'])

    def test_ninja_progress_lines_are_dropped(self):
        # the jobs running when the build failed finish after it, burying the error
        out = 'FAILED: bad.c.obj\nbad.c:1: error: x undeclared\n[2/9] Building C object a.c.obj\n' \
              '[3/9] Building C object b.c.obj\nninja: build stopped: subcommand failed.\n'
        self.assertEqual(self.excerpt(out, lines=3),
                         ['FAILED: bad.c.obj', 'bad.c:1: error: x undeclared', 'ninja: build stopped: subcommand failed.'])

    def test_a_long_diagnostic_keeps_its_first_error(self):
        out = 'FAILED: bad.c.obj\ngcc -c bad.c\nbad.c:1: error: first\n' + 'note: more\n' * 30
        self.assertEqual(self.excerpt(out, lines=3), ['FAILED: bad.c.obj', 'gcc -c bad.c', 'bad.c:1: error: first'])

    def test_without_a_failed_block_the_tail_is_shown(self):
        # cmake configure errors have no ninja FAILED: block
        self.assertEqual(self.excerpt(''.join(f'line {i}\n' for i in range(30)), lines=2), ['line 28', 'line 29'])

    def test_both_streams_are_shown(self):
        self.assertEqual(self.excerpt('out\n', 'err\n'), ['out', 'err'])

    @staticmethod
    def error(stdout, stderr='', src_dir='/co'):
        return sd.build_error(subprocess.CompletedProcess([], 1, stdout, stderr), src_dir)

    def test_the_error_skips_the_command_and_its_werror_flags(self):
        out = 'FAILED: x.c.obj\ncc -Wno-error=cast-align -Werror -c /co/src/x.c\n/co/src/x.c: In function f:\n' \
              '/co/src/x.c:17:3: error: y undeclared\nninja: build stopped: subcommand failed.\n'
        self.assertEqual(self.error(out), 'src/x.c:17:3: error: y undeclared')

    def test_the_error_is_found_beyond_the_console_excerpt(self):
        out = 'FAILED: x.c.obj\ncc -c x.c\n' + 'In file included from a.h:1,\n' * 30 + 'x.c:1:1: error: deep\n'
        self.assertEqual(self.error(out), 'x.c:1:1: error: deep')

    def test_base_side_paths_are_relative_to_their_own_checkout(self):
        self.assertEqual(self.error('/co/cmake-code-size/_worktree/src/x.c:1:1: error: y\n',
                                    src_dir='/co/cmake-code-size/_worktree'), 'src/x.c:1:1: error: y')

    def test_a_cmake_error_takes_its_message(self):
        self.assertEqual(self.error('', 'CMake Error at /co/hw/a.cmake:44 (message):\n  BOARD x not found\n'),
                         'CMake Error at hw/a.cmake:44 (message): BOARD x not found')

    def test_without_a_diagnostic_the_last_line_is_used(self):
        self.assertEqual(self.error('something\nKilled\n[3/9] Building C object a.o\n'
                                    'ninja: build stopped: subcommand failed.\n'), 'Killed')
        self.assertEqual(self.error(''), 'no output')

    def test_ninjas_own_error_is_kept(self):
        # `ninja -C <dir> <ex>` of an unknown target: its error is the only output
        self.assertEqual(self.error('', "ninja: error: unknown target 'foo', did you mean '.'?\n"),
                         "ninja: error: unknown target 'foo', did you mean '.'?")

    def test_markdown_escapes_the_backticks_of_a_diagnostic(self):
        md = sd.render_report({}, 'membrowse', [(('b', None), 'build', "build failed: ld: region `FLASH' overflowed")])
        self.assertIn("build failed: ld: region \\`FLASH' overflowed", md)

    def test_trailing_blank_lines_are_not_shown(self):
        self.assertEqual(self.excerpt('', 'CMake Error: x\n\n\n'), ['CMake Error: x'])

    def test_a_failed_build_prints_its_phase_and_output_and_returns_the_error(self):
        failed = subprocess.CompletedProcess([], 1, 'FAILED: bad.c.obj\nbad.c:1: error: x undeclared\n', '')
        ok = subprocess.CompletedProcess([], 0, '', '')
        with tempfile.TemporaryDirectory() as tmp, \
             mock.patch.object(sd, 'run', side_effect=[ok, failed]), \
             contextlib.redirect_stdout(io.StringIO()) as out:
            error = sd.build_board(tmp, os.path.join(tmp, 'b'), 'b', 'device/ex', 'build master')
        self.assertEqual(error, 'bad.c:1: error: x undeclared')
        self.assertRegex(out.getvalue(), r'^  build master… FAILED after \d+\.\ds\n'
                                         r'    FAILED: bad.c.obj\n    bad.c:1: error: x undeclared\n$')

    def test_a_successful_build_returns_none(self):
        ok = subprocess.CompletedProcess([], 0, '', '')
        with tempfile.TemporaryDirectory() as tmp, mock.patch.object(sd, 'run', return_value=ok), \
             contextlib.redirect_stdout(io.StringIO()):
            self.assertIsNone(sd.build_board(tmp, os.path.join(tmp, 'b'), 'b', None, 'build'))

    def test_the_build_step_runs_ninja_for_the_example_with_a_timeout(self):
        ok = subprocess.CompletedProcess([], 0, '', '')
        with tempfile.TemporaryDirectory() as tmp, \
             mock.patch.object(sd, 'run', return_value=ok) as run, \
             contextlib.redirect_stdout(io.StringIO()):
            sd.build_board(tmp, os.path.join(tmp, 'b'), 'b', 'device/ex', 'build')
        self.assertEqual(run.call_args, mock.call(['ninja', '-C', os.path.join(tmp, 'b'), 'ex'], timeout=600))

    def _esp_src(self, tmp):
        """A checkout with one espressif board, one example and a dependency linked in two
        hops, as a base worktree's: src -> checkout -> main checkout."""
        src = os.path.join(tmp, 'src')
        os.makedirs(os.path.join(src, 'hw', 'bsp', 'espressif', 'boards', 'esp'))
        # an ESP-IDF component, which get_examples('espressif') requires
        _touch(src, 'examples/device/a_freertos/src/CMakeLists.txt')
        os.makedirs(os.path.join(src, 'tools'))
        os.makedirs(os.path.join(src, 'lib'))
        os.makedirs(os.path.join(tmp, 'main', 'lib', 'dep'))
        os.makedirs(os.path.join(tmp, 'checkout', 'lib'))
        os.symlink(os.path.join(tmp, 'main', 'lib', 'dep'), os.path.join(tmp, 'checkout', 'lib', 'dep'))
        os.symlink(os.path.join(tmp, 'checkout', 'lib', 'dep'), os.path.join(src, 'lib', 'dep'))
        with open(os.path.join(src, 'tools', 'get_deps.py'), 'w') as f:
            f.write("deps_all = {'lib/dep': None, 'lib/absent': None}\n")
        return src

    def _build_esp(self, tmp, example='device/a_freertos', which=('idf.py',), inspect=0, rc=0, runs=None):
        """(build_board's error, its run() calls but the image check, also into `runs`); `rc`
        an exception class raises it from the build."""
        runs = [] if runs is None else runs
        def run(cmd, timeout=None):
            if cmd[:3] == ['docker', 'image', 'inspect']:
                return subprocess.CompletedProcess([], inspect, '', '')
            runs.append((cmd, timeout))
            if isinstance(rc, type) and 'app' in cmd:
                raise rc
            return subprocess.CompletedProcess([], rc, '', 'x')
        with mock.patch.object(sd, 'run', run), mock.patch.object(sd, 'CODE_SIZE_DIR', tmp), \
             mock.patch.object(sd.shutil, 'which', lambda name: f'/bin/{name}' if name in which else None), \
             mock.patch.object(sd.build_utils, 'skip_example', return_value=False), \
             contextlib.redirect_stdout(io.StringIO()):
            error = sd.build_board(self._esp_src(tmp), os.path.join(tmp, 'b'), 'esp', example, 'build')
        return error, runs

    @no_esp_on_windows
    def test_an_espressif_board_builds_each_examples_app_as_an_idf_project(self):
        with tempfile.TemporaryDirectory() as tmp:
            error, runs = self._build_esp(tmp)
        self.assertIsNone(error)
        self.assertEqual(runs, [(['idf.py', '-C', os.path.join(tmp, 'src', 'examples', 'device', 'a_freertos'),
                                  '-B', os.path.join(tmp, 'b', 'device', 'a_freertos'), '-GNinja', '-DBOARD=esp',
                                  'app'], 600)])

    @no_esp_on_windows
    def test_without_an_exported_idf_it_builds_in_cis_image_as_this_user(self):
        with tempfile.TemporaryDirectory() as tmp:
            error, runs = self._build_esp(tmp, which=('docker',))
            dep, cache = os.path.realpath(os.path.join(tmp, 'main', 'lib', 'dep')), os.path.join(tmp, '_ccache')
            mounts = [f'{os.path.realpath(tmp)}/_ccache:{cache}', f'{os.path.realpath(tmp)}/b:{tmp}/b',
                      f'{dep}:{tmp}/checkout/lib/dep', f'{dep}:{tmp}/main/lib/dep',
                      f'{os.path.realpath(tmp)}/src:{tmp}/src']
            self.assertTrue(os.path.isdir(cache))
        self.assertIsNone(error)
        cmd = runs[0][0]
        self.assertEqual(cmd[:11], ['docker', 'run', '--rm', '--name', f'tinyusb-code-size-{os.getpid()}',
                                    '--user', f'{os.getuid()}:{os.getgid()}', '-e', 'HOME=/tmp',
                                    '-e', f'CCACHE_DIR={cache}'])
        self.assertEqual([cmd[i + 1] for i, a in enumerate(cmd) if a == '-v'], mounts)
        self.assertEqual(cmd[cmd.index(sd.ESP_IDF_IMAGE):][:3], [sd.ESP_IDF_IMAGE, 'idf.py', '-C'])

    def test_without_idf_or_its_image_the_espressif_build_fails_saying_how_to_get_one(self):
        for which, inspect in (((), 0), (('docker',), 1)):
            with tempfile.TemporaryDirectory() as tmp:
                error, runs = self._build_esp(tmp, which=which, inspect=inspect)
            self.assertEqual(runs, [])
            self.assertIn('esp needs ESP-IDF: source $IDF_PATH/export.sh', error)

    @mock.patch.object(sd, 'WINDOWS', new=False)
    def test_without_idf_report_and_diff_refuse_an_espressif_board_before_building(self):
        pinned = ['stm32f407disco', 'espressif_s3_devkitm']
        for argv in (['report', '-b', pinned[0], '-b', pinned[1]], ['diff', '-b', pinned[0], '-b', pinned[1]],
                     ['diff', '--ci']):
            err = io.StringIO()
            with mock.patch.object(sys, 'argv', ['code_size.py'] + argv), \
                 mock.patch.object(sd, 'ci_pinned_boards', return_value=pinned), \
                 mock.patch.object(sd, 'engine_missing', return_value=False), \
                 mock.patch.object(sd.shutil, 'which', return_value=None), \
                 mock.patch.object(sd, 'run') as run, contextlib.redirect_stderr(err):
                with self.assertRaises(SystemExit):
                    sd.main()
            run.assert_not_called()
            self.assertIn('espressif_s3_devkitm need ESP-IDF: source $IDF_PATH/export.sh', err.getvalue())

    def test_an_example_the_tree_does_not_build_for_espressif_fails_the_build(self):
        # device/board_test: an espressif example absent from this tree
        for example in ('device/cdc_msc', 'device/board_test'):
            with tempfile.TemporaryDirectory() as tmp:
                error, runs = self._build_esp(tmp, example=example)
            self.assertEqual((error, runs), (f'esp builds no {example}', []))

    @no_esp_on_windows
    def test_a_timed_out_or_interrupted_docker_build_removes_its_container(self):
        rm = ['docker', 'rm', '-f', f'tinyusb-code-size-{os.getpid()}']
        for which, rc, removed in ((('docker',), 124, True), (('docker',), 2, False), (('idf.py',), 124, False)):
            with tempfile.TemporaryDirectory() as tmp:
                _error, runs = self._build_esp(tmp, which=which, rc=rc)
            self.assertEqual(runs[-1][0] == rm, removed, (which, rc))
        runs = []
        with tempfile.TemporaryDirectory() as tmp, self.assertRaises(KeyboardInterrupt):
            self._build_esp(tmp, which=('docker',), rc=KeyboardInterrupt, runs=runs)
        self.assertEqual(runs[-1][0], rm)

    def test_a_timeout_returns_124_and_keeps_the_output_as_text(self):
        ret = sd.run(['sh', '-c', 'printf err >&2; echo out; exec sleep 30'], timeout=1)
        self.assertEqual(ret.returncode, 124)
        self.assertEqual(ret.stdout, 'out\n')
        self.assertTrue(ret.stderr.startswith('err\nCommand timed out after 1s'))

    def test_output_that_is_not_utf8_is_decoded_with_replacement(self):
        for timeout, rc, tail in ((None, 0, ''), (0.3, 124, 'exec sleep 30')):
            with self.subTest(timeout=timeout):
                ret = sd.run(['sh', '-c', f"printf 'w: \\251\\n'; printf '\\377' >&2; {tail}"], timeout=timeout)
                self.assertEqual((ret.returncode, ret.stdout), (rc, 'w: \ufffd\n'))
                self.assertTrue(ret.stderr.startswith('\ufffd'))

    @posix_only
    def test_sigterm_to_the_script_alone_stops_its_command_and_runs_cleanup(self):
        """Popen's exit waits for the command, so the handler passes the signal on, and
        kills a command ignoring it after the grace."""
        for action, forwarded in (('echo got > {got}; exit', True), ('', False)):
            with self.subTest(forwarded=forwarded), tempfile.TemporaryDirectory() as tmp:
                pidfile, got = os.path.join(tmp, 'pid'), os.path.join(tmp, 'got')
                # the pid is written once the trap is in place, so the signal meets it
                command = f"trap '{action.format(got=got)}' TERM; echo $$ > {pidfile}; while :; do sleep 0.1; done"
                script = ('import code_size as c\n'
                          'c.TERMINATE_GRACE = 0.2\n'
                          'c.exit_on_termination()\n'
                          'try:\n'
                          f'    c.run(["sh", "-c", {command!r}])\n'
                          'finally:\n'
                          '    print("cleaned up", flush=True)\n')
                proc = subprocess.Popen([sys.executable, '-c', script], stdout=subprocess.PIPE, text=True,
                                        cwd=os.path.dirname(sd.__file__))
                child = None
                try:
                    deadline = time.monotonic() + 5
                    while not (os.path.exists(pidfile) and os.path.getsize(pidfile)):
                        self.assertLess(time.monotonic(), deadline)
                        time.sleep(0.05)
                    with open(pidfile) as f:
                        child = int(f.read())
                    proc.send_signal(signal.SIGTERM)
                    out, _ = proc.communicate(timeout=5)
                    self.assertEqual((proc.returncode, out), (128 + signal.SIGTERM, 'cleaned up\n'))
                    self.assertEqual(os.path.exists(got), forwarded)
                    with self.assertRaises(ProcessLookupError):
                        os.kill(child, 0)
                finally:
                    proc.kill()
                    proc.wait()
                    if child:
                        with contextlib.suppress(ProcessLookupError):
                            os.kill(child, signal.SIGKILL)

    def test_a_command_ignoring_sigterm_is_killed_after_the_grace(self):
        with mock.patch.object(sd, 'TERMINATE_GRACE', 0.5):
            ret = sd.run(['sh', '-c', "trap '' TERM; while :; do sleep 0.1; done"], timeout=1)
        self.assertEqual(ret.returncode, 124)

    @posix_only
    def test_a_child_holding_the_pipes_after_the_kill_does_not_hang_it(self):
        start = time.monotonic()
        with mock.patch.object(sd, 'TERMINATE_GRACE', 0.5):
            ret = sd.run(['sh', '-c', "trap '' TERM; sleep 30 & echo $!; while :; do sleep 0.1; done"], timeout=1)
        with contextlib.suppress(ProcessLookupError):
            os.kill(int(ret.stdout), signal.SIGKILL)  # the pid printed before the kill is kept
        self.assertLess(time.monotonic() - start, 5)
        self.assertEqual(ret.returncode, 124)
        self.assertIn('Command timed out after 1s', ret.stderr)

    @posix_only
    def test_a_child_holding_the_pipes_after_sigterm_does_not_hang_it(self):
        start = time.monotonic()
        with mock.patch.object(sd, 'TERMINATE_GRACE', 0.5), mock.patch.object(sd, 'KILL_DRAIN', 0.2):
            ret = sd.run(['sh', '-c', 'sleep 30 & echo $!; exec sleep 30'], timeout=0.5)
        child = int(ret.stdout)
        try:
            self.assertLess(time.monotonic() - start, 5)
            self.assertEqual(ret.returncode, 124)
            self.assertIn('Command timed out after 0.5s', ret.stderr)
        finally:
            with contextlib.suppress(ProcessLookupError):
                os.kill(child, signal.SIGKILL)

    @needs_proc
    @unittest.skipUnless(shutil.which('ninja'), 'needs ninja')
    def test_a_timed_out_ninja_stops_its_jobs(self):
        # ninja runs each job in a process group of its own, as it does the compilers
        with tempfile.TemporaryDirectory() as tmp:
            with open(os.path.join(tmp, 'build.ninja'), 'w') as f:
                f.write('rule job\n  command = sh -c \'echo $$$$ > $out.pid; exec sleep 30\'\n'
                        'build a: job\nbuild b: job\n')
            ret = sd.run(['ninja', '-C', tmp, '-j', '2'], timeout=1)
            pids = []
            for target in 'ab':
                with open(os.path.join(tmp, f'{target}.pid')) as f:
                    pids.append(int(f.read()))
        try:
            self.assertEqual(ret.returncode, 124)
            self.assertIn('Command timed out after 1s', ret.stderr)
            self.assertTrue(all(_dies(pid) for pid in pids))
        finally:
            for pid in pids:
                with contextlib.suppress(ProcessLookupError):
                    os.kill(pid, signal.SIGKILL)


def _dies(pid, timeout=5):
    """Whether `pid` ends within `timeout` seconds, by the last /proc read: a reaped
    process passes zombie (Z), dead (X), gone."""
    deadline = time.monotonic() + timeout
    while True:
        try:
            with open(f'/proc/{pid}/stat') as f:
                if f.read().rpartition(')')[2].split()[0] in ('Z', 'X'):
                    return True
        except OSError:  # reaped
            return True
        if time.monotonic() > deadline:
            return False
        time.sleep(0.05)


class ShortHash(unittest.TestCase):
    def hash(self, ret):
        with mock.patch.object(sd, 'run', return_value=ret) as run:
            return sd.short_hash('/co'), run.call_args.args[0]

    def test_the_describe_output_names_the_commit(self):
        self.assertEqual(self.hash(subprocess.CompletedProcess([], 0, 'abc1234-dirty\n', '')),
                         ('abc1234-dirty', ['git', '-C', '/co', 'describe', '--always', '--dirty', '--exclude=*']))

    def test_a_git_error_is_none_not_dirty(self):
        self.assertIsNone(self.hash(subprocess.CompletedProcess([], 128, '', 'fatal: not a git repository'))[0])
        self.assertIsNone(self.hash(subprocess.CompletedProcess([], 0, '\n', ''))[0])

    def test_a_real_checkout_has_a_hash(self):
        self.assertRegex(sd.short_hash(sd.TINYUSB_ROOT), r'^[0-9a-f]{7,}(-dirty)?$')


@unittest.skipIf(os.name == 'nt', 'the Windows filter drops the drive')
class SymlinkedCheckout(unittest.TestCase):
    def test_the_root_matches_its_own_filter_through_a_symlink(self):
        with tempfile.TemporaryDirectory() as tmp:
            link = os.path.join(tmp, 'tinyusb')
            os.symlink(REPO, link)
            out = subprocess.run([sys.executable, '-c', 'import code_size as sd; '
                                  'print(sd.tinyusb_src_filter(sd.TINYUSB_ROOT).startswith(sd.TINYUSB_ROOT + "/"))'],
                                 cwd=tmp, env={**os.environ, 'PYTHONPATH': os.path.join(link, 'tools')},
                                 capture_output=True, text=True, check=True).stdout
        self.assertEqual(out, 'True\n')


def cmake_shortened(obj_dir, obj_name):
    """CMake v4.1.2 cmLocalGeneratorCheckObjectName and cmLocalGeneratorShortenObjectName
    (Source/cmLocalGenerator.cxx) at Windows' CMAKE_OBJECT_PATH_MAX of 250."""
    max_len = 250 - len(obj_dir)
    if len(obj_name) <= max_len:
        return obj_name
    pos = obj_name.find('/', len(obj_name) - max_len + 32)
    return hashlib.md5(obj_name[:pos].encode()).hexdigest() + obj_name[pos:]


@mock.patch.object(sd, 'WINDOWS', new=True)
class CMakeShortenedObject(unittest.TestCase):
    CHECKOUT = 'C:/Users/username/code/tinyusb/.worktrees/claude/code-size-windows/cmake-code-size/_worktree'
    BUILD = 'C:/Users/username/code/tinyusb/.worktrees/claude/code-size-windows/cmake-code-size/base/'
    TARGET = 'device/audio_4_channel_mic_freertos/CMakeFiles/audio_4_channel_mic_freertos.dir/'
    FILTER = CHECKOUT[2:] + '/src/'
    USBD = CHECKOUT + '/src/device/usbd.c'
    STARTUP = CHECKOUT + '/hw/mcu/st/cmsis_device_f4/Source/Templates/gcc/startup_stm32f407xx.s'

    def setUp(self):
        tmp = tempfile.TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        self.build_dir = tmp.name
        self.elf = os.path.join(tmp.name, 'device', 'audio_4_channel_mic_freertos', 'audio_4_channel_mic_freertos.elf')

    def obj(self, source):
        """The map path of source's object, and its build.ninja compile edge."""
        obj = self.TARGET + cmake_shortened(self.BUILD + self.TARGET, source.replace(':', '_') + '.obj')
        self.assertRegex(obj, r'\.dir/[0-9a-f]{32}/')
        self.assertNotIn(self.FILTER, obj)
        edge = f'build {obj}: C_COMPILER__audio_4_channel_mic_freertos_unscanned_MinSizeRel {source.replace(":", "$:")}' \
               ' || cmake_object_order_depends_target_audio_4_channel_mic_freertos\n'
        return obj.replace('/', '\\'), edge

    def sizes(self, edges):
        with open(os.path.join(self.build_dir, 'build.ninja'), 'w') as f:
            f.writelines(edges)
        return sd._Sizes([self.FILTER], self.elf)

    def test_a_shortened_vendor_object_counts_in_all_only(self):
        obj, edge = self.obj(self.STARTUP)
        sizes = self.sizes([edge])
        sizes.add(obj, '.isr_vector', {'flash'}, 4, 'g_pfnVectors')
        self.assertEqual(sizes.result()['files'], {})
        self.assertEqual(sizes.result()['all'], {'flash': 4, 'ram': 0})

    def test_a_shortened_tinyusb_object_keeps_its_source_key(self):
        obj, edge = self.obj(self.USBD)
        sizes = self.sizes([edge])
        sizes.add(obj, '.text', {'flash'}, 4, 'tud_task_ext')
        self.assertEqual(sizes.result()['files'], {'device/usbd.c': {'flash': 4, 'ram': 0}})

    def test_a_shortened_object_build_ninja_does_not_build_fails(self):
        obj, _ = self.obj(self.USBD)
        _, edge = self.obj(self.STARTUP)
        with self.assertRaisesRegex(RuntimeError, 'CMAKE_OBJECT_PATH_MAX .* no compile edge'):
            self.sizes([edge]).add(obj, '.text', {'flash'}, 4, 'tud_task_ext')

    def test_a_build_ninja_not_in_utf_8_fails(self):
        obj, edge = self.obj(self.USBD)
        with open(os.path.join(self.build_dir, 'build.ninja'), 'wb') as f:
            f.write(b'# Calf\xe9\n' + edge.encode())
        sizes = sd._Sizes([self.FILTER], self.elf)
        with self.assertRaisesRegex(RuntimeError, r'build\.ninja.*utf-8'):
            sizes.add(obj, '.text', {'flash'}, 4, 'tud_task_ext')

    def test_linux_does_not_resolve_a_shortened_object(self):
        obj, _ = self.obj(self.USBD)
        with mock.patch.object(sd, 'WINDOWS', new=False):
            sizes = self.sizes([])
            sizes.add(obj.replace('\\', '/'), '.text', {'flash'}, 4, 'tud_task_ext')
        self.assertEqual(sizes.result()['files'], {})


@mock.patch.object(sd, 'WINDOWS', new=True)
class WindowsHost(unittest.TestCase):
    def test_a_backslash_object_path_matches_the_forward_slash_filter(self):
        for f in ('/a/tinyusb/src/', '\\a\\tinyusb\\src\\'):
            self.assertEqual(sd._relative_key(r'cdc_msc.dir\D_\a\tinyusb\src\device\usbd.c.obj', [sd.filter_arg(f)]),
                             'device/usbd.c')

    def test_a_drive_is_stripped_from_a_filter(self):
        with mock.patch.object(sd.os, 'path', ntpath):
            self.assertEqual(sd.filter_arg('D:\\a\\tinyusb\\src\\'), '/a/tinyusb/src/')

    def test_a_backslash_example_names_the_forward_slash_elf_id(self):
        self.assertEqual(sd.example_arg('device\\cdc_msc\\'), 'device/cdc_msc')

    def test_a_filter_matches_regardless_of_case_and_the_key_keeps_the_recorded_case(self):
        self.assertEqual(sd._relative_key(r'cdc_msc.dir\D_\A\TinyUSB\src\Device\usbd.c.obj',
                                          ['/nomatch/', '/a/tinyusb/src/']), 'Device/usbd.c')

    def test_espressif_boards_are_refused(self):
        self.assertRegex(sd.esp_without_idf(['espressif_s3_devkitc', 'stm32f407disco']),
                         r'^espressif_s3_devkitc need ESP-IDF, .* not support on Windows')
        self.assertIsNone(sd.esp_without_idf(['stm32f407disco']))

    def test_a_timeout_kills_the_command_tree(self):
        with mock.patch('subprocess.run') as taskkill:
            ret = sd.run([sys.executable, '-c', 'import time; time.sleep(30)'], timeout=0.5)
        self.assertEqual(ret.returncode, 124)
        argv = taskkill.call_args.args[0]
        self.assertEqual(argv[:2] + argv[3:], ['taskkill', '/PID', '/T', '/F'])

    @unittest.skipUnless(os.name == 'nt', 'a real Windows process tree')
    def test_a_timeout_stops_a_grandchild_holding_the_pipes(self):
        # as ninja's compilers do: terminate() alone stops only the direct child
        parent = ('import subprocess, sys, time; '
                  'p = subprocess.Popen([sys.executable, "-c", "import time; time.sleep(60)"], '
                  'stdout=sys.stdout, stderr=sys.stderr); '
                  'print(p.pid, flush=True); time.sleep(60)')
        start = time.monotonic()
        ret = sd.run([sys.executable, '-c', parent], timeout=5)
        elapsed = time.monotonic() - start
        pid = ret.stdout.strip()
        try:
            self.assertEqual(ret.returncode, 124)
            self.assertTrue(pid.isdigit(), ret.stdout)
            tasks = subprocess.run(['tasklist', '/FI', f'PID eq {pid}', '/FO', 'CSV', '/NH'],
                                   capture_output=True, text=True, check=True, timeout=10).stdout
            self.assertNotIn(f'"{pid}"', tasks)
            self.assertLess(elapsed, 30)  # well before the grandchild's own exit
        finally:
            if pid.isdigit():
                subprocess.run(['taskkill', '/PID', pid, '/F'], capture_output=True)

    def test_a_symlink_without_the_privilege_names_developer_mode(self):
        denied = OSError(22, 'A required privilege is not held by the client')
        denied.winerror = 1314
        with tempfile.TemporaryDirectory() as main, tempfile.TemporaryDirectory() as wt:
            os.makedirs(os.path.join(main, 'lib/lwip'))
            os.makedirs(os.path.join(wt, 'tools'))
            with open(os.path.join(wt, 'tools', 'get_deps.py'), 'w') as f:
                f.write("deps_all = {'lib/lwip': []}\n")
            with mock.patch('os.symlink', side_effect=denied), \
                    self.assertRaisesRegex(SystemExit, 'Developer Mode, or use --base-source ci'):
                sd.symlink_deps(main, wt)

    def test_a_path_on_another_drive_is_shown_absolute(self):
        with mock.patch('os.path.relpath', side_effect=ValueError('path is on mount C:, start on mount D:')):
            self.assertEqual(sd._shown('C:/x/report.md'), 'C:/x/report.md')


# main() tests stub the builds and sizing, so need no engine tool
@mock.patch.object(sd, 'engine_missing', new=lambda _engine: False)
class MainFailure(unittest.TestCase):
    def _run_main(self, tmp, argv, build_board, generate=None, run=None):
        """main() with the build, sizing and command steps stubbed. `build_board`
        is its side_effect (it must create the board dir, as the real one does);
        `generate` the generate_sizes() side_effect. Returns (rc, stdout)."""
        ok = subprocess.CompletedProcess([], 0, 'c0ffee\n', '')
        if '--base-source' not in argv:  # the default would look up a CI base
            argv = argv + ['--base-source', 'local']
        with mock.patch.object(sys, 'argv', ['code_size.py', 'diff'] + argv), \
             mock.patch.object(sd, 'CODE_SIZE_DIR', tmp), \
             mock.patch.object(sd, 'run', side_effect=run or (lambda *_a, **_k: ok)), \
             mock.patch.object(sd, 'symlink_deps'), \
             mock.patch.object(sd, 'build_board', side_effect=build_board), \
             mock.patch.object(sd, 'generate_sizes', side_effect=generate):
            buf = io.StringIO()
            with contextlib.redirect_stdout(buf):
                rc = sd.main()
        return rc, unpad(buf.getvalue())

    def _main(self, tmp, build_board=None, sizes=None, cur_sizes=None):
        """main() for board `b`; `sizes` is what generate_sizes() returns for
        the base side and, unless `cur_sizes` is given, the current side too."""
        def build(_src, _build_dir, board, *_args, **_kwargs):
            os.makedirs(os.path.join(tmp, board), exist_ok=True)
            return build_board() if build_board else None
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
                return None

            rc, _out = self._main(tmp, build_board, ({'ex/ex.elf': _elf(1)}, []))
            self.assertEqual(rc, 0)

    def test_failed_report_replaces_stale_output_and_returns_nonzero(self):
        with tempfile.TemporaryDirectory() as tmp:
            board_dir = os.path.join(tmp, 'b')
            os.makedirs(board_dir)
            stale = os.path.join(board_dir, 'diff.md')
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
            rc, out = self._main(tmp, sizes=({'ex/ex.elf': _elf(1)}, []),
                                 cur_sizes=({'ex/ex.elf': _elf(3)}, []))
            self.assertEqual(rc, 0)
            with open(os.path.join(tmp, 'b', 'diff.md')) as f:
                self.assertIn('| x.c | 1 → 3 | +2 | 0 → 0 | 0 |', unpad(f.read()))
            # no -e: the console gets the summary line, the tables stay in the .md
            self.assertIn('  1 pair, 1 changed; TinyUSB Flash Δ +2, RAM Δ 0\n', out)
            self.assertNotIn('| x.c |', out)

    def test_a_filter_mismatch_fails_the_size_and_compare_phase(self):
        empty = elf({}, all_syms=(4, 0))
        with tempfile.TemporaryDirectory() as tmp:
            rc, out = self._main(tmp, sizes=({'ex/ex.elf': empty}, []))
            self.assertEqual(rc, 1)
            self.assertRegex(out, r'  size and compare… FAILED after \d+\.\ds\n  INCOMPLETE: 1 pair')

    def test_a_failed_worktree_cleanup_fails_the_run(self):
        ok = subprocess.CompletedProcess([], 0, 'c0ffee\n', '')
        stuck = subprocess.CompletedProcess([], 1, '', 'fatal: busy')

        def run(cmd, **_kwargs):
            if cmd[3:5] == ['worktree', 'add']:
                os.makedirs(cmd[-2])
            return stuck if cmd[3:5] == ['worktree', 'remove'] else ok
        with tempfile.TemporaryDirectory() as tmp:
            def build(_src, _build_dir, board, *_args):
                os.makedirs(os.path.join(tmp, board), exist_ok=True)
                return None
            side_sizes = iter([({'ex/ex.elf': _elf(1)}, [])] * 2)
            rc, out = self._run_main(tmp, ['-b', 'b'], build, lambda *_a, **_k: next(side_sizes), run)
        self.assertEqual(rc, 1)
        self.assertIn('Error removing worktree', out)

    def test_a_leftover_worktree_is_removed_even_when_locked(self):
        # a killed `git worktree add` leaves it locked, which one --force refuses
        failed = subprocess.CompletedProcess([], 128, '', 'fatal: stop after the add')
        cmds = []

        def run(cmd, **_kwargs):
            if 'rev-parse' in cmd:
                return subprocess.CompletedProcess([], 0, 'c0ffee\n', '')
            cmds.append(cmd)
            return failed
        with tempfile.TemporaryDirectory() as tmp:
            worktree = os.path.join(tmp, '_worktree')
            os.makedirs(worktree)
            with self.assertRaises(SystemExit):
                self._run_main(tmp, ['-b', 'b'], mock.Mock(), run=run)
        self.assertEqual(cmds[0], ['git', '-C', sd.TINYUSB_ROOT, 'worktree', 'remove', '--force', '--force', worktree])
        self.assertEqual(cmds[1][3:5], ['worktree', 'add'])

    def test_several_elfs_of_one_example_label_their_tables(self):
        with tempfile.TemporaryDirectory() as tmp:
            two = lambda f: ({'ex/app.elf': _elf(f), 'ex/boot.elf': _elf(f)}, [])
            side_sizes = iter([two(1), two(3)])

            def build(_src, _build_dir, board, *_args):
                os.makedirs(os.path.join(tmp, board), exist_ok=True)
                return None
            rc, out = self._run_main(tmp, ['-b', 'b', '-e', 'ex'], build, lambda *_a, **_k: next(side_sizes))
            self.assertEqual(rc, 0)
            self.assertLess(out.index('\n  b: ex/app.elf:\n'), out.index('\n  b: ex/boot.elf:\n'))

    def test_the_one_changed_elf_of_several_is_labelled(self):
        with tempfile.TemporaryDirectory() as tmp:
            side_sizes = iter([({'ex/app.elf': _elf(1), 'ex/boot.elf': _elf(1)}, []),
                               ({'ex/app.elf': _elf(1), 'ex/boot.elf': _elf(3)}, [])])

            def build(_src, _build_dir, board, *_args):
                os.makedirs(os.path.join(tmp, board), exist_ok=True)
                return None
            rc, out = self._run_main(tmp, ['-b', 'b', '-e', 'ex'], build, lambda *_a, **_k: next(side_sizes))
            self.assertEqual(rc, 0)
            self.assertIn('  2 pairs, 1 changed\n', out)
            self.assertIn('\n  b: ex/boot.elf:\n', out)
            self.assertNotIn('b: ex/app.elf:', out)

    def test_the_table_names_the_sides_by_hash_or_falls_back(self):
        for short, header in (('abc1234\n', 'File (abc1234 → abc1234) |'), ('', 'File (base → new) |')):
            with tempfile.TemporaryDirectory() as tmp:
                side_sizes = iter([({'ex/ex.elf': _elf(1)}, []), ({'ex/ex.elf': _elf(3)}, [])])

                def build(_src, _build_dir, board, *_args):
                    os.makedirs(os.path.join(tmp, board), exist_ok=True)
                    return None

                def run(cmd, **_kwargs):
                    out = short if 'describe' in cmd else 'c0ffee\n'
                    return subprocess.CompletedProcess(cmd, 0 if out else 128, out, '')
                rc, out = self._run_main(tmp, ['-b', 'b', '-e', 'ex'], build, lambda *_a, **_k: next(side_sizes), run)
                self.assertEqual(rc, 0)
                self.assertIn(header, out)
                with open(os.path.join(tmp, 'b', 'diff_ex.md')) as f:
                    self.assertIn(header, unpad(f.read()))

    def test_a_single_example_prints_its_phases_summary_and_changed_tables(self):
        with tempfile.TemporaryDirectory() as tmp:
            side_sizes = iter([({'ex/ex.elf': _elf(1)}, []), ({'ex/ex.elf': _elf(3)}, [])])

            def build(_src, _build_dir, board, *_args):
                os.makedirs(os.path.join(tmp, board), exist_ok=True)
                return None
            rc, out = self._run_main(tmp, ['-b', 'b', '-e', 'ex'], build, lambda *_a, **_k: next(side_sizes))
            self.assertEqual(rc, 0)
            self.assertRegex(out, r'^diff master \(c0ffee\) vs working tree \(c0ffee\) · membrowse\n\[1/1\] b / ex\n'
                                  r'  size and compare… \d+\.\ds\n  1 pair, 1 changed; TinyUSB Flash Δ \+2')
            # indented tables; unpad() folds their indent to one space
            self.assertIn('\n | x.c | 1 → 3 | +2 | 0 → 0 | 0 |', out)
            self.assertIn('\n | x.c | +2 | +2 |', out)
            self.assertNotIn('**Coverage', out)

    def test_unmatched_elf_is_incomplete_but_not_a_failure(self):
        with tempfile.TemporaryDirectory() as tmp:
            rc, out = self._main(tmp, sizes=({'ex/ex.elf': _elf(1)}, []),
                                 cur_sizes=({'ex/ex.elf': _elf(1), 'new/new.elf': _elf(5)}, []))
            self.assertEqual(rc, 0)
            self.assertIn('INCOMPLETE', out)
            self.assertIn('  INCOMPLETE: 1 pair, 0 changed', out)
            self.assertIn('    current-only: b: new/new.elf', out)

    def test_no_symbols_matched_filters_fails(self):
        # a filter typo, or an engine output change breaking source-path
        # matching: must not silently produce an empty/degenerate table
        empty = elf({}, all_syms=(4, 0))
        with tempfile.TemporaryDirectory() as tmp:
            rc, out = self._main(tmp, sizes=({'ex/ex.elf': empty}, []))
            self.assertEqual(rc, 1)
            self.assertIn('no membrowse sizes matched filters', out)
            self.assertIn('another --engine', out)

    def test_one_side_without_matched_files_is_a_valid_removal(self):
        empty = elf({}, all_syms=(4, 0))
        with tempfile.TemporaryDirectory() as tmp:
            rc, out = self._main(tmp, sizes=({'ex/ex.elf': _elf(8)}, []),
                                 cur_sizes=({'ex/ex.elf': empty}, []))
            self.assertEqual(rc, 0)
            self.assertIn('1 pair, 1 changed; TinyUSB Flash Δ -8', out)
            with open(os.path.join(tmp, 'b', 'diff.md')) as f:
                self.assertIn('| x.c | 8 → 0 | -8 | 0 → 0 | 0 |', unpad(f.read()))

    def test_a_custom_filter_is_not_called_tinyusb(self):
        def build(_src, _build_dir, board, *_args, **_kwargs):
            os.makedirs(os.path.join(tmp, board), exist_ok=True)
        side_sizes = iter([({'ex/ex.elf': _elf(1)}, []), ({'ex/ex.elf': _elf(3)}, [])])
        with tempfile.TemporaryDirectory() as tmp:
            rc, out = self._run_main(tmp, ['-b', 'b', '-f', 'lib/'], build, lambda *_a, **_k: next(side_sizes))
        self.assertEqual(rc, 0)
        self.assertIn('1 pair, 1 changed; filtered Flash Δ +2', out)

    def _main_combined(self, tmp, sizes, build_ok=('b1', 'b2'), example=None):
        """main() with -b b1 -b b2 --combined. `sizes` maps
        (board, side) to what generate_sizes() returns; boards not in
        `build_ok` fail their base build. Returns (rc, stdout, combined md or None)."""
        def build_board(_src, _build_dir, board, *_args, **_kwargs):
            os.makedirs(os.path.join(tmp, board), exist_ok=True)
            return None if board in build_ok else 'boom'

        def generate(build_dir, _filters, _example=None, _engine='membrowse'):
            board, side = os.path.relpath(build_dir, tmp).split(os.sep)
            return sizes[(board, 'base' if side == 'base' else 'current')]

        argv = ['-b', 'b1', '-b', 'b2', '--combined'] + (['-e', example] if example else [])
        rc, out = self._run_main(tmp, argv, build_board, generate)
        combined = os.path.join(tmp, '_combined', 'diff.md')
        md = None
        if os.path.isfile(combined):
            with open(combined) as f:
                md = unpad(f.read())
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
        empty = elf({}, all_syms=(4, 0))
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
        for example, name in ((None, 'diff.md'),
                              ('device/cdc_msc', 'diff_device_cdc_msc.md')):
            with tempfile.TemporaryDirectory() as tmp:
                def build_board(*_args, **_kwargs):
                    os.makedirs(os.path.join(tmp, 'b'), exist_ok=True)
                    return 'boom'
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
                return None if example == 'device/a' else 'boom'
            cmds = []

            def run(cmd, **_kwargs):
                cmds.append(cmd)
                return subprocess.CompletedProcess([], 0, '', '')
            generate = mock.Mock()
            with mock.patch.object(sd.shutil, 'which', return_value='/usr/bin/bloaty'):
                rc, _out = self._run_main(tmp, ['-b', 'b', '-e', 'device/a', '-e', 'device/b',
                                                '--bloaty', '--combined'],
                                          build_board, generate, run)
            self.assertEqual(rc, 1)
            generate.assert_not_called()
            self.assertFalse(any('bloaty' in cmd for cmd in cmds))
            for name in ('diff_device_a.md', 'diff_device_b.md'):
                with open(os.path.join(tmp, 'b', name)) as f:
                    self.assertIn('FAILED `b` base build: build failed (device/b): boom', f.read())
            with open(os.path.join(tmp, '_combined', 'diff.md')) as f:
                self.assertEqual(f.read().count('FAILED `b`'), 1)

    def test_combined_with_an_example(self):
        sizes = {(b, side): ({'device/cdc_msc/cdc_msc.elf': _elf(5)}, [])
                 for b in ('b1', 'b2') for side in ('base', 'current')}
        with tempfile.TemporaryDirectory() as tmp:
            rc, _out, md = self._main_combined(tmp, sizes, example='device/cdc_msc')
            self.assertTrue(os.path.isfile(os.path.join(tmp, 'b1', 'diff_device_cdc_msc.md')))
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
            return None if build_ok else 'boom'
        generate = mock.Mock(side_effect=lambda *_a, **_k: sizes)
        rc, _out = self._run_main(tmp, ['-b', 'b'] + argv, build, generate)
        return rc, generate

    def _read(self, *path):
        with open(os.path.join(*path)) as f:
            return unpad(f.read())

    def test_engine_is_passed_to_sizing_and_named_in_the_report(self):
        with tempfile.TemporaryDirectory() as tmp:
            rc, generate = self._main_json(tmp, ['--engine', 'linkermap'], ({'ex/ex.elf': _elf(1)}, []))
            self.assertEqual(rc, 0)
            self.assertEqual({c.args[3] for c in generate.call_args_list}, {'linkermap'})
            self.assertIn('Coverage (complete, linkermap)', self._read(tmp, 'b', 'diff.md'))

    def test_json_holds_the_paired_sizes_and_provenance(self):
        with tempfile.TemporaryDirectory() as tmp:
            rc, _ = self._main_json(tmp, ['--json', '--combined', '-f', 'src/'],
                                    ({'ex/ex.elf': _elf(1)}, []))
            self.assertEqual(rc, 0)
            for path in ((tmp, 'b', 'diff.json'), (tmp, '_combined', 'diff.json')):
                data = json.loads(self._read(*path))
                self.assertEqual(data['engine'], 'membrowse')
                self.assertEqual((data['base_ref'], data['base_sha'], data['current_rev']), ('master', 'c0ffee', 'c0ffee'))
                self.assertEqual(data['filters'], {'base': ['src/'], 'current': ['src/']})
                no_symbols = {k: v for k, v in _elf(1).items() if k != 'symbols'}
                self.assertEqual(data['pairs'], [{'board': 'b', 'elf': 'ex/ex.elf',
                                                  'base': no_symbols, 'current': no_symbols}])
                self.assertEqual((data['base_only'], data['current_only'], data['failures']), ([], [], []))
                self.assertEqual(data['status'], 'complete')

    def test_symbols_reach_the_report_and_the_json(self):
        with tempfile.TemporaryDirectory() as tmp:
            rc, _ = self._main_json(tmp, ['--json', '--symbols'], ({'ex/ex.elf': _elf(1)}, []))
            self.assertEqual(rc, 0)
            self.assertIn('_no section changes_', self._read(tmp, 'b', 'diff.md'))
            pair, = json.loads(self._read(tmp, 'b', 'diff.json'))['pairs']
            self.assertEqual(pair['base']['symbols'], {'x.c': {'.text': {'f': 1}}})

    def test_json_of_a_failed_run_matches_its_report(self):
        with tempfile.TemporaryDirectory() as tmp:
            os.makedirs(os.path.join(tmp, 'b'))
            for name in ('diff.md', 'diff.json'):
                with open(os.path.join(tmp, 'b', name), 'w') as f:
                    f.write('stale')
            rc, _ = self._main_json(tmp, ['--json'], None, build_ok=False)
            self.assertEqual(rc, 1)
            data = json.loads(self._read(tmp, 'b', 'diff.json'))
            self.assertEqual(data['status'], 'INCOMPLETE')
            self.assertEqual(data['failures'], [{'board': 'b', 'elf': None, 'side': 'base', 'stage': 'build',
                                                 'message': 'build failed: boom'}])
            self.assertIn('FAILED `b` base build: build failed: boom',
                          self._read(tmp, 'b', 'diff.md'))

    def test_bloaty_missing_is_refused_before_any_worktree_work(self):
        with tempfile.TemporaryDirectory() as tmp, \
             mock.patch.object(sd.shutil, 'which', return_value=None), \
             contextlib.redirect_stderr(io.StringIO()) as err:
            run = mock.Mock()
            with self.assertRaises(SystemExit):
                self._run_main(tmp, ['-b', 'b', '-e', 'device/a', '--bloaty'], mock.Mock(), run=run)
        run.assert_not_called()
        self.assertIn('--bloaty requires bloaty on PATH', err.getvalue())

    def test_a_missing_engine_is_refused_before_any_worktree_work(self):
        with tempfile.TemporaryDirectory() as tmp, \
             mock.patch.object(sd, 'engine_missing', return_value=True), \
             contextlib.redirect_stderr(io.StringIO()) as err:
            run = mock.Mock()
            with self.assertRaises(SystemExit):
                self._run_main(tmp, ['-b', 'b', '--engine', 'bloaty'], mock.Mock(), run=run)
        run.assert_not_called()
        self.assertIn('bloaty not found - install bloaty on PATH (https://github.com/google/bloaty), '
                      'or pick another --engine', err.getvalue())

    def test_a_bloaty_failure_is_printed_and_fails_the_run(self):
        for rc, printed in ((0, '\nsection diff\n'), (1, '\n  bloaty FAILED (exit 1)\n    bloaty: missing debug info\n')):
            with self.subTest(rc=rc), tempfile.TemporaryDirectory() as tmp:
                def build(_src, build_dir, board, example, *_args):
                    os.makedirs(os.path.join(build_dir, example))
                    open(os.path.join(build_dir, example, 'ex.elf'), 'w').close()
                    return None
                ok = subprocess.CompletedProcess([], 0, 'c0ffee\n', '')
                bloaty = subprocess.CompletedProcess([], rc, 'section diff\n' if rc == 0 else '',
                                                     'bloaty: missing debug info\n' if rc else '')
                sizes = ({'ex/ex.elf': _elf(1)}, [])
                with mock.patch.object(sd.shutil, 'which', return_value='/usr/bin/bloaty'):
                    code, out = self._run_main(tmp, ['-b', 'b', '-e', 'ex', '--bloaty'], build,
                                               lambda *_a, **_k: sizes,
                                               lambda cmd, **_k: bloaty if cmd[0] == 'bloaty' else ok)
                self.assertEqual(code, rc)
                self.assertEqual(out.count(printed), 2)

    def test_a_failed_base_setup_still_removes_the_worktree(self):
        ok = subprocess.CompletedProcess([], 0, '', '')
        cmds = []

        def run(cmd, **_kwargs):
            cmds.append(cmd[3:5])
            if cmd[3:5] == ['worktree', 'add']:
                os.makedirs(cmd[-2])
            return ok
        with tempfile.TemporaryDirectory() as tmp, \
             mock.patch.object(sys, 'argv', ['code_size.py', 'diff', '-b', 'b', '--base-source', 'local']), \
             mock.patch.object(sd, 'CODE_SIZE_DIR', tmp), \
             mock.patch.object(sd, 'run', side_effect=run), \
             mock.patch.object(sd, 'symlink_deps', side_effect=FileNotFoundError('tools/get_deps.py')), \
             contextlib.redirect_stdout(io.StringIO()):
            with self.assertRaises(FileNotFoundError):
                sd.main()
        self.assertEqual(cmds, [['rev-parse', '--verify'], ['worktree', 'add'], ['worktree', 'remove']])

    def test_a_worktree_add_stopped_by_a_signal_is_removed(self):
        cmds = []

        def run(cmd, **_kwargs):
            cmds.append(cmd[3:])
            if cmd[3:5] == ['worktree', 'add']:
                os.makedirs(cmd[-2])  # git has created it, locked `initializing`
                raise SystemExit(143)  # exit_on_termination(), through run()
            return subprocess.CompletedProcess([], 0, '', '')
        with tempfile.TemporaryDirectory() as tmp, \
             mock.patch.object(sys, 'argv', ['code_size.py', 'diff', '-b', 'b', '--base-source', 'local']), \
             mock.patch.object(sd, 'CODE_SIZE_DIR', tmp), \
             mock.patch.object(sd, 'run', side_effect=run), \
             contextlib.redirect_stdout(io.StringIO()):
            with self.assertRaises(SystemExit):
                sd.main()
            self.assertEqual(cmds[-1], ['worktree', 'remove', '--force', '--force', os.path.join(tmp, '_worktree')])

    def test_a_failed_worktree_setup_leaves_no_previous_report(self):
        with tempfile.TemporaryDirectory() as tmp:
            os.makedirs(os.path.join(tmp, 'b'))
            stale = [os.path.join(tmp, 'b', f'diff.{ext}') for ext in ('md', 'json')]
            for path in stale:
                with open(path, 'w') as f:
                    f.write('stale')
            cmds = []

            def run(cmd, **_kwargs):
                cmds.append(cmd[3:5])
                if cmd[3:5] == ['worktree', 'add']:
                    return subprocess.CompletedProcess(cmd, 128, '', 'fatal: invalid reference')
                return subprocess.CompletedProcess(cmd, 0, 'c0ffee\n', '')
            with self.assertRaises(SystemExit) as stop:
                self._run_main(tmp, ['-b', 'b'], mock.Mock(), run=run)
            self.assertEqual(stop.exception.code, 1)  # 'Error creating worktree', not a usage error
            self.assertIn(['worktree', 'add'], cmds)
            self.assertFalse(any(os.path.exists(path) for path in stale))

    def test_a_run_without_json_drops_a_previous_json(self):
        with tempfile.TemporaryDirectory() as tmp:
            os.makedirs(os.path.join(tmp, 'b'))
            stale = os.path.join(tmp, 'b', 'diff.json')
            with open(stale, 'w') as f:
                f.write('{}')
            rc, _ = self._main_json(tmp, [], ({'ex/ex.elf': _elf(1)}, []))
            self.assertEqual(rc, 0)
            self.assertFalse(os.path.exists(stale))

    def _seed_combined_report(self, tmp):
        """A previous run's combined report, left behind in the gitignored
        cmake-code-size/ tree that nothing else wipes."""
        os.makedirs(os.path.join(tmp, '_combined'))
        stale = os.path.join(tmp, '_combined', 'diff.md')
        with open(stale, 'w') as f:
            f.write('| previous run | 100 | 200 |')
        return stale


@mock.patch.object(sd, 'engine_missing', new=lambda _engine: False)
class MainReport(unittest.TestCase):
    def _run(self, tmp, argv, sizes, build_ok=lambda _example: True):
        """`code_size.py report -b b` plus `argv`, building and sizing stubbed;
        `build_ok(example)` is each build's result, `sizes` what generate_sizes()
        returns. Returns (rc, build mock, generate mock)."""
        def build_board(_src, _build_dir, board, example, _label):
            os.makedirs(os.path.join(tmp, board), exist_ok=True)
            return None if build_ok(example) else 'boom'
        build = mock.Mock(side_effect=build_board)
        generate = mock.Mock(side_effect=lambda *_a, **_k: sizes)
        with mock.patch.object(sys, 'argv', ['code_size.py', 'report', '-b', 'b'] + argv), \
             mock.patch.object(sd, 'CODE_SIZE_DIR', tmp), \
             mock.patch.object(sd, 'run') as run, \
             mock.patch.object(sd, 'build_board', build), \
             mock.patch.object(sd, 'generate_sizes', generate), \
             contextlib.redirect_stdout(io.StringIO()) as out:
            rc = sd.main()
        self.out = unpad(out.getvalue())
        run.assert_not_called()  # no base worktree
        return rc, build, generate

    def _read(self, *path):
        with open(os.path.join(*path)) as f:
            return unpad(f.read())

    def test_a_missing_engine_is_refused_before_any_build(self):
        with tempfile.TemporaryDirectory() as tmp, \
             mock.patch.object(sd, 'engine_missing', side_effect=lambda engine: engine == 'linkermap'), \
             contextlib.redirect_stderr(io.StringIO()) as err:
            with self.assertRaises(SystemExit):
                self._run(tmp, ['--engine', 'linkermap'], ({}, []))
            self.assertEqual(os.listdir(tmp), [])  # no build dir
        self.assertIn('linkermap not found - install `python3 tools/get_deps.py tools/linkermap`', err.getvalue())

    def test_builds_and_sizes_the_working_tree_only(self):
        with tempfile.TemporaryDirectory() as tmp:
            rc, build, generate = self._run(tmp, ['-e', 'device/cdc_msc', '--engine', 'bloaty'],
                                            ({'device/cdc_msc/cdc_msc.elf': _elf(4)}, []))
            self.assertEqual(rc, 0)
            build.assert_called_once_with(sd.TINYUSB_ROOT, os.path.join(tmp, 'b', 'build'), 'b', 'device/cdc_msc',
                                          'build')
            self.assertEqual(generate.call_args.args[1:], ([sd.tinyusb_src_filter(sd.TINYUSB_ROOT)],
                                                          'device/cdc_msc', 'bloaty'))
            md = self._read(tmp, 'b', 'report_device_cdc_msc.md')
            self.assertIn('Coverage (complete, bloaty)', md)
            self.assertIn('| x.c | 4 | 4 | 100.0% |', md)
            self.assertRegex(self.out, r'^report working tree · bloaty\n\[1/1\] b / device/cdc_msc\n'
                                       r'  size… \d+\.\ds\n  1 of 1 elfs sized; TinyUSB Flash 4, RAM 0\n')
            self.assertIn('\n | x.c | 4 | 4 | 100.0% |', self.out)

    def test_an_examples_trailing_slash_is_dropped(self):
        with tempfile.TemporaryDirectory() as tmp:
            rc, build, generate = self._run(tmp, ['-e', 'device/cdc_msc/'],
                                            ({'device/cdc_msc/cdc_msc.elf': _elf(4)}, []))
            self.assertEqual(rc, 0)
            self.assertEqual(build.call_args.args[3], 'device/cdc_msc')
            self.assertEqual(generate.call_args.args[2], 'device/cdc_msc')
            self.assertTrue(os.path.exists(os.path.join(tmp, 'b', 'report_device_cdc_msc.md')))

    def test_an_example_naming_nothing_is_refused_before_any_build(self):
        for example in ('/', ''):
            with self.subTest(example=example), tempfile.TemporaryDirectory() as tmp, \
                 contextlib.redirect_stderr(io.StringIO()) as err:
                with self.assertRaises(SystemExit) as exit_:
                    self._run(tmp, ['-e', example], ({}, []))
                self.assertEqual(exit_.exception.code, 2)
                self.assertEqual(os.listdir(tmp), [])
                self.assertIn(f"argument -e/--example: {example!r} names no example", err.getvalue())

    def test_json_holds_the_sizes_and_symbols_only_with_symbols(self):
        with tempfile.TemporaryDirectory() as tmp:
            for flag, has_symbols in (([], False), (['--symbols'], True)):
                rc, _, _ = self._run(tmp, ['--json', '-f', 'src/'] + flag,
                                     ({'ex/ex.elf': _elf(1)}, []))
                self.assertEqual(rc, 0)
                data = json.loads(self._read(tmp, 'b', 'report.json'))
                self.assertEqual((data['engine'], data['boards'], data['filters'], data['status']),
                                 ('membrowse', ['b'], ['src/'], 'complete'))
                (record,) = data['elfs']
                self.assertEqual((record['board'], record['elf']), ('b', 'ex/ex.elf'))
                self.assertEqual('symbols' in record['sizes'], has_symbols)

    def test_a_failed_build_replaces_a_stale_report_and_fails(self):
        with tempfile.TemporaryDirectory() as tmp:
            os.makedirs(os.path.join(tmp, 'b'))
            for ext in ('md', 'json'):
                with open(os.path.join(tmp, 'b', f'report.{ext}'), 'w') as f:
                    f.write('stale')
            rc, _, generate = self._run(tmp, [], None, lambda _example: False)
            self.assertEqual(rc, 1)
            generate.assert_not_called()
            self.assertIn('FAILED `b` build: build failed: boom', self._read(tmp, 'b', 'report.md'))
            self.assertIn('  INCOMPLETE: 0 of 0 elfs sized\n    FAILED b build: build failed: boom\n', self.out)
            self.assertFalse(os.path.exists(os.path.join(tmp, 'b', 'report.json')))

    def test_one_examples_build_failure_spares_the_others(self):
        with tempfile.TemporaryDirectory() as tmp:
            rc, _, generate = self._run(tmp, ['-e', 'device/a', '-e', 'device/b'],
                                        ({'device/a/a.elf': _elf(1)}, []), lambda ex: ex == 'device/a')
            self.assertEqual(rc, 1)
            self.assertEqual(generate.call_count, 1)
            self.assertIn('Coverage (complete, membrowse)', self._read(tmp, 'b', 'report_device_a.md'))
            self.assertIn('FAILED `b` build: build failed (device/b): boom', self._read(tmp, 'b', 'report_device_b.md'))

    def test_a_failed_or_unmatched_elf_is_incomplete_and_fails(self):
        empty = elf({}, all_syms=(4, 0))
        for sizes, message in ((({'a/a.elf': _elf(1), 'b/b.elf': None}, [('b/b.elf', 'boom')]), 'boom'),
                               (({'a/a.elf': empty}, []), 'no membrowse sizes matched filters')):
            with tempfile.TemporaryDirectory() as tmp:
                rc, _, _ = self._run(tmp, [], sizes)
                self.assertEqual(rc, 1)
                md = self._read(tmp, 'b', 'report.md')
                self.assertIn('INCOMPLETE', md)
                self.assertIn(message, md)

    def test_reports_that_all_failed_add_no_filter_failure(self):
        with tempfile.TemporaryDirectory() as tmp:
            rc, _, _ = self._run(tmp, [], ({'a/a.elf': None}, [('a/a.elf', 'boom')]))
            self.assertEqual(rc, 1)
            self.assertNotIn('matched filters', self._read(tmp, 'b', 'report.md'))

    def test_an_elf_without_tinyusb_code_is_complete_when_another_matched(self):
        with tempfile.TemporaryDirectory() as tmp:
            rc, _, _ = self._run(tmp, [], ({'a/a.elf': _elf(1), 'board_test/board_test.elf': elf({}, all_syms=(4, 0))},
                                           []))
            self.assertEqual(rc, 0)
            self.assertIn('Coverage (complete, membrowse)', self._read(tmp, 'b', 'report.md'))

    def test_diff_only_options_are_refused(self):
        with self.assertRaises(SystemExit), contextlib.redirect_stderr(io.StringIO()):
            self._run(tempfile.gettempdir(), ['--ci'], None)


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
                sizes, errors = sd.generate_sizes(build_dir, ['build/'])
            self.assertEqual(errors, [])
            self.assertEqual(sizes['device/cdc_msc/cdc_msc.elf']['files']['x.c']['flash'], 4)

    def test_helper_elf_below_an_example_is_skipped(self):
        """pico-sdk's boot stage 2 links its own elf under the role dir; it has no
        TinyUSB file and must not be sized as an example."""
        with tempfile.TemporaryDirectory() as tmp:
            build_dir = self._tree(tmp)
            bs2_dir = os.path.join(build_dir, 'device', 'pico-sdk', 'src', 'rp2040', 'boot_stage2')
            os.makedirs(bs2_dir)
            open(os.path.join(bs2_dir, 'bs2_default.elf'), 'w').close()
            with _engine('membrowse', mock.Mock(return_value=_elf(4))):
                sizes, errors = sd.generate_sizes(build_dir, ['build/'])
            self.assertEqual(errors, [])
            self.assertEqual(list(sizes), ['device/cdc_msc/cdc_msc.elf'])


@mock.patch.object(sd, 'engine_missing', new=lambda _engine: False)
class CiBoardSet(unittest.TestCase):
    def _boards_built(self, tmp, pinned_json):
        """Boards main() builds for `--ci -b extra -b b1 -b extra`, with `pinned_json` as the pinned file."""
        pinned = os.path.join(tmp, 'ci-pinned-boards.json')
        with open(pinned, 'w') as f:
            f.write(pinned_json)
        built = []

        def build(_src, _build_dir, board, *_args, **_kwargs):
            built.append(board)
            os.makedirs(os.path.join(tmp, board), exist_ok=True)
            return 'boom'  # stop at the base build: only the board set matters
        ok = subprocess.CompletedProcess([], 0, '', '')
        with mock.patch.object(sys, 'argv', ['x', 'diff', '--ci', '-b', 'extra', '-b', 'b1', '-b', 'extra']), \
             mock.patch.object(sd, 'CODE_SIZE_DIR', tmp), \
             mock.patch.object(sd, 'CI_PINNED_BOARDS', pinned), \
             mock.patch.object(sd, 'run', return_value=ok), \
             mock.patch.object(sd, 'symlink_deps'), \
             mock.patch.object(sd, 'build_board', side_effect=build), \
             contextlib.redirect_stdout(io.StringIO()):
            sd.main()
        return built

    def test_ci_adds_the_pinned_boards_after_the_named_ones_once_each(self):
        with tempfile.TemporaryDirectory() as tmp:
            built = self._boards_built(tmp, '{"boards": [{"board": "b1"}, {"board": "b2"}], '
                                            '"uncovered": []}')
            self.assertEqual(built, ['extra', 'b1', 'b2'])

    @mock.patch.object(sd, 'WINDOWS', new=False)
    def test_ci_builds_pinned_espressif_boards_too(self):
        with tempfile.TemporaryDirectory() as tmp:
            built = self._boards_built(tmp, '{"boards": [{"board": "b1"}, '
                                            '{"board": "espressif_s3_devkitm"}], "uncovered": []}')
        self.assertEqual(built, ['extra', 'b1', 'espressif_s3_devkitm'])


def _touch(root, *rels):
    for rel in rels:
        os.makedirs(os.path.dirname(os.path.join(root, rel)), exist_ok=True)
        open(os.path.join(root, rel), 'w').close()


class Snapshot(unittest.TestCase):
    SHA = [c * 40 for c in 'abc']

    def test_a_missing_build_dir_is_a_build_failure(self):
        elfs, failures = sd.board_snapshot('b1', '/nonexistent/cmake-build-b1', None, ['src/'])
        self.assertEqual(elfs, {})
        self.assertEqual([(f['elf'], f['stage']) for f in failures], [(None, 'build')])

    def test_sizes_every_elf_and_records_failed_ones(self):
        with tempfile.TemporaryDirectory() as tmp:
            _touch(tmp, 'device/a/a.elf', 'device/a/loader.elf', 'host/bad/bad.elf')

            def sizes(path, _filters):
                if 'bad' in path:
                    raise RuntimeError('membrowse report failed')
                return _elf(5)
            with _engine('membrowse', sizes):
                elfs, failures = sd.board_snapshot('b1', tmp, None, ['src/'])
        self.assertEqual(sorted(elfs), ['device/a/a.elf', 'device/a/loader.elf'])
        self.assertEqual([(f['elf'], f['stage']) for f in failures], [('host/bad/bad.elf', 'report')])

    def test_scoped_examples_skip_what_the_board_skips_and_fail_a_missing_one(self):
        with tempfile.TemporaryDirectory() as tmp:
            _touch(tmp, 'device/a/a.elf')
            with _engine('membrowse', lambda _p, _f: _elf(5)), \
                 mock.patch.object(sd.build_utils, 'skip_example', side_effect=lambda e, b, _d: e == 'device/skipped'):
                elfs, failures = sd.board_snapshot('b1', tmp, ['device/a', 'device/skipped', 'device/gone'],
                                                   ['src/'])
        self.assertEqual(list(elfs), ['device/a/a.elf'])
        self.assertEqual(len(failures), 1)
        self.assertEqual(failures[0]['stage'], 'build')
        self.assertIn('device/gone', failures[0]['message'])

    def test_no_tinyusb_file_in_any_elf_fails_the_filter(self):
        with tempfile.TemporaryDirectory() as tmp:
            _touch(tmp, 'device/a/a.elf')
            with _engine('membrowse', lambda _p, _f: elf({})):
                elfs, failures = sd.board_snapshot('b1', tmp, None, ['src/'])
        self.assertEqual(list(elfs), ['device/a/a.elf'])
        self.assertEqual([f['stage'] for f in failures], ['filter'])

    def test_compiler_comes_from_cmake_not_the_host(self):
        with tempfile.TemporaryDirectory() as tmp:
            os.makedirs(os.path.join(tmp, 'CMakeFiles', '4.1.2'))
            with open(os.path.join(tmp, 'CMakeFiles', '4.1.2', 'CMakeCCompiler.cmake'), 'w') as f:
                f.write('set(CMAKE_C_COMPILER "/opt/arm/bin/arm-none-eabi-gcc")\n'
                        'set(CMAKE_C_COMPILER_ID "GNU")\nset(CMAKE_C_COMPILER_VERSION "13.3.1")\n')
            with open(os.path.join(tmp, 'CMakeCache.txt'), 'w') as f:
                f.write('CMAKE_BUILD_TYPE:STRING=MinSizeRel\n')
            self.assertEqual(sd._cmake_compiler(tmp), {'id': 'GNU', 'version': '13.3.1',
                                                       'name': 'arm-none-eabi-gcc', 'build_type': 'MinSizeRel'})
        self.assertEqual(sd._cmake_compiler('/nonexistent'), {'id': '', 'version': '', 'name': '', 'build_type': ''})

    def test_a_windows_compiler_is_named_without_exe(self):
        with tempfile.TemporaryDirectory() as tmp:
            os.makedirs(os.path.join(tmp, 'CMakeFiles', '4.1.2'))
            with open(os.path.join(tmp, 'CMakeFiles', '4.1.2', 'CMakeCCompiler.cmake'), 'w') as f:
                f.write('set(CMAKE_C_COMPILER "C:/arm/bin/arm-none-eabi-gcc.EXE")\n')
            self.assertEqual(sd._cmake_compiler(tmp)['name'], 'arm-none-eabi-gcc')

    def test_an_esp_idf_board_reads_its_first_example_project(self):
        with tempfile.TemporaryDirectory() as tmp:
            for ex, ver in (('device/b', '14.2.0'), ('device/a', '14.2.0'), ('device/a/bootloader', '1.0')):
                os.makedirs(os.path.join(tmp, ex, 'CMakeFiles', '3.30'))
                with open(os.path.join(tmp, ex, 'CMakeFiles', '3.30', 'CMakeCCompiler.cmake'), 'w') as f:
                    f.write(f'set(CMAKE_C_COMPILER "/idf/xtensa-esp32s3-elf-gcc")\nset(CMAKE_C_COMPILER_VERSION "{ver}")\n')
            with open(os.path.join(tmp, 'device', 'a', 'CMakeCache.txt'), 'w') as f:
                f.write('CMAKE_BUILD_TYPE:STRING=\n')
            self.assertEqual(sd._cmake_compiler(tmp), {'id': '', 'version': '14.2.0',
                                                       'name': 'xtensa-esp32s3-elf-gcc', 'build_type': ''})

    def test_a_pull_request_reads_base_and_head_from_the_merge_commit(self):
        ret = subprocess.CompletedProcess([], 0, '\n'.join(self.SHA) + '\n', '')
        with mock.patch.object(sd, 'run', return_value=ret) as run:
            self.assertEqual(sd._git_shas('pull_request'), tuple(self.SHA))
        self.assertEqual(run.call_args[0][0][-3:], ['HEAD', 'HEAD^1', 'HEAD^2'])
        with mock.patch.object(sd, 'run', return_value=subprocess.CompletedProcess([], 0, self.SHA[0], '')):
            self.assertEqual(sd._git_shas('push'), (self.SHA[0],) * 3)

    def test_unresolvable_commits_raise(self):
        # a shallow pull_request checkout has no HEAD^2: never a snapshot with a guessed base
        for ret in (subprocess.CompletedProcess([], 128, '', 'unknown revision HEAD^2'),
                    subprocess.CompletedProcess([], 0, 'HEAD^2\n', '')):
            with mock.patch.object(sd, 'run', return_value=ret), self.assertRaises(RuntimeError):
                sd._git_shas('pull_request')

    def test_boards_are_the_pinned_ones_of_the_leg(self):
        with tempfile.TemporaryDirectory() as tmp:
            pinned = os.path.join(tmp, 'pinned.json')
            with open(pinned, 'w') as f:
                f.write('{"boards": [{"board": "p1"}, {"board": "p2"}]}')
            import build
            with mock.patch.object(sd, 'CI_PINNED_BOARDS', pinned), \
                 mock.patch.object(build, 'builds_any', return_value=True), \
                 mock.patch.object(build, 'resolve_ci_boards', return_value=['p2']) as resolve:
                boards = sd.snapshot_boards(['fam'], ['p1', 'notpinned', 'p2'], ['device/a'])
        self.assertEqual(boards, ['p1', 'p2'])
        resolve.assert_called_once_with(pinned, 'fam', True, ['device/a'], extra_defines=())

    def test_a_pinned_board_that_builds_none_of_the_examples_is_not_expected(self):
        # build.py skips it before configuring: no build dir, and no failure either
        with tempfile.TemporaryDirectory() as tmp:
            pinned = os.path.join(tmp, 'pinned.json')
            with open(pinned, 'w') as f:
                f.write('{"boards": [{"board": "p1"}, {"board": "p2"}]}')
            import build
            with mock.patch.object(sd, 'CI_PINNED_BOARDS', pinned), \
                 mock.patch.object(build, 'builds_any', side_effect=lambda b, _e, _d: b == 'p2'):
                self.assertEqual(sd.snapshot_boards([], ['p1', 'p2'], ['host/device_info']), ['p2'])

    def test_non_integer_sizes_fail_the_elf(self):
        for bad in (1.5, True):
            sizes = _elf(5)
            sizes['files']['x.c']['flash'] = bad
            with tempfile.TemporaryDirectory() as tmp:
                _touch(tmp, 'device/a/a.elf')
                with _engine('membrowse', lambda _p, _f, s=sizes: s):
                    elfs, failures = sd.board_snapshot('b1', tmp, None, ['src/'])
            self.assertEqual(elfs, {})
            self.assertEqual([(f['elf'], f['stage']) for f in failures], [('device/a/a.elf', 'report')])

    def test_a_repeated_example_is_sized_once(self):
        with tempfile.TemporaryDirectory() as tmp:
            _touch(tmp, 'device/a/a.elf')
            sizer = mock.Mock(return_value=_elf(5))
            with _engine('membrowse', sizer), mock.patch.object(sd.build_utils, 'skip_example', return_value=False):
                elfs, failures = sd.board_snapshot('b1', tmp, ['device/a', 'device/a'], ['src/'])
        self.assertEqual((list(elfs), failures, sizer.call_count), (['device/a/a.elf'], [], 1))

    def test_writes_one_json_per_board_symbols_only_on_request(self):
        with tempfile.TemporaryDirectory() as tmp:
            build_root, out = os.path.join(tmp, 'cmake-build'), os.path.join(tmp, 'out')
            _touch(build_root, 'cmake-build-b1/device/a/a.elf')
            for symbols in (False, True):
                args = mock.Mock(event='push', families=[], board=['b1'], example=None, output=out,
                                 build_root=build_root, build_outcome='failure', symbols=symbols, filter=None,
                                 build_name=None, define_symbol=[])
                with _engine('membrowse', lambda _p, _f: _elf(5)), \
                     mock.patch.object(sd, '_git_shas', return_value=(self.SHA[0],) * 3), \
                     mock.patch.object(sd, 'snapshot_boards', return_value=['b1']), \
                     contextlib.redirect_stdout(io.StringIO()):
                    self.assertEqual(sd.run_snapshot(args), 0)
                with open(os.path.join(out, 'code-size-b1.json')) as f:
                    data = json.load(f)
                self.assertEqual((data['schema'], data['board']), (sd.SNAPSHOT_SCHEMA, 'b1'))
                self.assertEqual(data['elfs']['device/a/a.elf']['files'], {'x.c': {'flash': 5, 'ram': 0}})
                self.assertEqual('symbols' in data['elfs']['device/a/a.elf'], symbols)
                with open(os.path.join(out, 'leg.json')) as f:
                    leg = json.load(f)
                self.assertEqual((leg['boards'], leg['sha'], leg['build_outcome']), (['b1'], self.SHA[0], 'failure'))

    def test_a_leg_without_pinned_boards_still_leaves_its_marker(self):
        with tempfile.TemporaryDirectory() as tmp:
            args = mock.Mock(event='push', families=['fam'], board=[], example=['device/a'], output=tmp,
                             build_root=tmp, build_outcome='success', symbols=False, filter=None, build_name=None,
                             define_symbol=[])
            with mock.patch.object(sd, '_git_shas', return_value=(self.SHA[0],) * 3), \
                 mock.patch.object(sd, 'snapshot_boards', return_value=[]), \
                 contextlib.redirect_stdout(io.StringIO()):
                self.assertEqual(sd.run_snapshot(args), 0)
            self.assertEqual(os.listdir(tmp), ['leg.json'])
            with open(os.path.join(tmp, 'leg.json')) as f:
                leg = json.load(f)
        self.assertEqual((leg['boards'], leg['examples']), ([], ['device/a']))

    def test_a_variant_reports_its_board_under_the_build_name(self):
        with tempfile.TemporaryDirectory() as tmp:
            build_root, out = os.path.join(tmp, 'cmake-build'), os.path.join(tmp, 'out')
            _touch(build_root, 'cmake-build-b1/device/a/a.elf', 'cmake-build-b1-DMA/device/a/a.elf')
            args = mock.Mock(event='push', families=[], board=['b1'], example=['device/a'], output=out,
                             build_root=build_root, build_outcome='success', symbols=False, filter=None,
                             build_name='b1-DMA', define_symbol=['MAX3421_HOST=1'])
            sized = []
            with _engine('membrowse', lambda p, _f: sized.append(p) or _elf(5)), \
                 mock.patch.object(sd, '_git_shas', return_value=(self.SHA[0],) * 3), \
                 mock.patch.object(sd, 'snapshot_boards', return_value=['b1']) as boards, \
                 mock.patch.object(sd.build_utils, 'skip_example', return_value=False) as skip, \
                 contextlib.redirect_stdout(io.StringIO()):
                self.assertEqual(sd.run_snapshot(args), 0)
            self.assertEqual(sorted(os.listdir(out)), ['code-size-b1-DMA.json', 'leg.json'])
            with open(os.path.join(out, 'leg.json')) as f:
                self.assertEqual(json.load(f)['boards'], ['b1-DMA'])
            with open(os.path.join(out, 'code-size-b1-DMA.json')) as f:
                self.assertEqual(json.load(f)['board'], 'b1-DMA')
        self.assertEqual([os.path.relpath(p, build_root) for p in sized], [os.path.join('cmake-build-b1-DMA', 'device', 'a', 'a.elf')])
        # the board's own skip rules, with the leg's defines as build.py applied them
        boards.assert_called_once_with([], ['b1'], ['device/a'], ('MAX3421_HOST=1',))
        skip.assert_called_with('device/a', 'b1', ('MAX3421_HOST=1',))

    def test_a_variant_needs_one_board_and_a_build_name(self):
        for argv in (['-b', 'b1', '-b', 'b2', '--build-name', 'x'], ['fam', '--build-name', 'x'],
                     ['-b', 'b1', '--cflag=-DX=1'], ['-b', 'b1', '-DX=1'], ['-b', 'b1', '--build-name', '../x']):
            with mock.patch.object(sd, 'engine_missing', return_value=False), \
                 mock.patch.object(sd, 'run_snapshot') as run, \
                 mock.patch.object(sys, 'argv', ['code_size.py', 'snapshot', '-o', 'out'] + argv), \
                 contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit):
                sd.main()
            run.assert_not_called()

    def test_a_variant_takes_build_py_defines(self):
        argv = ['-b', 'b1', '--build-name', 'b1-X', '-DX=1', '--define-symbol', 'Y=2', '--cflag=-DZ=3']
        with mock.patch.object(sd, 'engine_missing', return_value=False), \
             mock.patch.object(sd, 'run_snapshot', return_value=0) as run, \
             mock.patch.object(sys, 'argv', ['code_size.py', 'snapshot', '-o', 'out'] + argv):
            self.assertEqual(sd.main(), 0)
        self.assertEqual((run.call_args[0][0].define_symbol, run.call_args[0][0].cflag), (['X=1', 'Y=2'], ['-DZ=3']))


def _shard(board, elfs, sha='a', base='b', head='c', failures=(), examples=None, compiler='gcc 14'):
    return {'schema': 1, 'board': board, 'engine': 'membrowse', 'membrowse_version': '1.2.9',
            'compiler': compiler, 'sha': sha * 40, 'base_sha': base * 40, 'head_sha': head * 40,
            'build_outcome': 'success', 'examples': examples, 'elfs': elfs, 'failures': list(failures)}


def _write(root, rel, data):
    os.makedirs(os.path.dirname(os.path.join(root, rel)), exist_ok=True)
    with open(os.path.join(root, rel), 'w') as f:
        json.dump(data, f)


LEG_KEYS = ('sha', 'base_sha', 'head_sha', 'examples', 'build_outcome')


def _run_dir(root, shards, legs=None, scope=None):
    """A downloaded run: one artifact dir per leg ({artifact: [boards]}), each leg taking its
    commits, examples and build outcome from its first board's _shard(), and the shards,
    without those, spread over them."""
    legs = legs if legs is not None else {'code-size-arm-gcc-fam': [s['board'] for s in shards]}
    by_board = {s['board']: s for s in shards}
    for artifact, boards in legs.items():
        first = by_board.get(boards[0], _shard('x', {})) if boards else _shard('x', {})
        _write(root, f'{artifact}/leg.json', {'schema': 1, 'boards': boards, **{k: first[k] for k in LEG_KEYS}})
        for b in boards:
            if b in by_board:
                _write(root, f'{artifact}/code-size-{b}.json',
                       {k: v for k, v in by_board[b].items() if k not in LEG_KEYS})
    if scope is not None:
        _write(root, 'code-size-scope/scope.json', {'schema': 1, **scope})
    return sd.load_snapshots(root)


class Compare(unittest.TestCase):
    SCOPE = {'code_changed': True, 'legs': [{'toolchain': 'arm-gcc', 'arg': 'fam'}]}

    def compare(self, base_shards, cur_shards, cur_legs=None, scope=SCOPE, baseline=None, symbols=False):
        with tempfile.TemporaryDirectory() as b, tempfile.TemporaryDirectory() as c:
            return sd.compare_runs(_run_dir(b, base_shards, scope=self.SCOPE),
                                   _run_dir(c, cur_shards, cur_legs, scope), baseline, symbols)

    def failed(self, data):
        return {(f['board'], f['side'], f['stage']) for f in data['failures']}

    def test_pairs_boards_and_reports_the_change(self):
        md, comment, data = self.compare([_shard('b1', {'device/a/a.elf': _elf(10)}, sha='b', head='b')],
                                         [_shard('b1', {'device/a/a.elf': _elf(14)})])
        self.assertEqual(data['status'], 'complete')
        self.assertIn('+4', md)
        self.assertTrue(comment.startswith('## Code size'))
        self.assertIn('bbbbbbbbbb', md)  # sides labelled by the merge commit's parents

    def test_base_elfs_of_examples_the_pr_did_not_build_are_outside_coverage(self):
        base = _shard('b1', {'device/a/a.elf': _elf(10), 'device/z/z.elf': _elf(3)})
        cur = _shard('b1', {'device/a/a.elf': _elf(10)}, examples=['device/a'])
        scope = {**self.SCOPE, 'family_examples': {'fam': ['device/a']}}
        _md, _c, data = self.compare([base], [cur], scope=scope)
        self.assertEqual(data['status'], 'complete')
        self.assertEqual(data['outside'], 1)
        self.assertEqual(data['base_only'], [])

    def test_a_leg_measuring_other_examples_than_the_scope_fails(self):
        cur = _shard('b1', {'device/z/z.elf': _elf(1)}, examples=['device/z'])
        scope = {**self.SCOPE, 'family_examples': {'fam': ['device/a']}}
        _md, _c, data = self.compare([_shard('b1', {'device/z/z.elf': _elf(1)})], [cur], scope=scope)
        self.assertIn(('code-size-arm-gcc-fam', 'current', 'scope'), self.failed(data))

    def test_an_esp_leg_matches_its_artifact_and_its_own_examples(self):
        arg = '-b espressif_s3_devkitm -e device/cdc_msc_freertos'
        scope = {'code_changed': True, 'legs': [{'toolchain': 'esp-idf', 'arg': arg}]}
        legs = {'code-size-esp-idf--b espressif_s3_devkitm': ['espressif_s3_devkitm']}
        elfs = {'device/cdc_msc_freertos/cdc_msc_freertos.elf': _elf(1)}
        _md, _c, data = self.compare([_shard('espressif_s3_devkitm', elfs)],
                                     [_shard('espressif_s3_devkitm', elfs, examples=['device/cdc_msc_freertos'])],
                                     cur_legs=legs, scope=scope)
        self.assertEqual(data['status'], 'complete')
        _md, _c, data = self.compare([], [_shard('espressif_s3_devkitm', elfs, examples=['device/other'])],
                                     cur_legs=legs, scope=scope)
        self.assertIn(('code-size-esp-idf--b espressif_s3_devkitm', 'current', 'scope'), self.failed(data))

    def test_a_leg_outside_the_scope_and_a_shard_outside_its_leg_fail(self):
        shards = [_shard('b1', {'device/a/a.elf': _elf(1)}), _shard('b2', {'device/a/a.elf': _elf(1)})]
        with tempfile.TemporaryDirectory() as b, tempfile.TemporaryDirectory() as c:
            cur = _run_dir(c, shards, {'code-size-arm-gcc-fam': ['b1'], 'code-size-arm-gcc-extra': ['b2']},
                           self.SCOPE)
            _write(c, 'code-size-arm-gcc-fam/code-size-b3.json', _shard('b3', {'device/a/a.elf': _elf(1)}))
            _md, _c, data = sd.compare_runs(_run_dir(b, shards + [_shard('b3', {})]), sd.load_snapshots(c))
        self.assertIn(('code-size-arm-gcc-extra', 'current', 'scope'), self.failed(data))
        self.assertIn(('b3', 'current', 'snapshot'), self.failed(data))
        self.assertEqual(data['boards'], ['b1', 'b2'])

    def test_a_new_elf_is_current_only(self):
        md, comment, data = self.compare([_shard('b1', {'device/a/a.elf': _elf(10), 'device/o/o.elf': _elf(1)})],
                                         [_shard('b1', {'device/a/a.elf': _elf(10), 'device/n/n.elf': _elf(1)})])
        self.assertEqual(data['current_only'], [{'board': 'b1', 'elf': 'device/n/n.elf'}])
        self.assertEqual(data['status'], 'INCOMPLETE')
        self.assertIn('- missing in this PR: `b1: device/o/o.elf`\n'
                      '- new in this PR (no master size yet): `b1: device/n/n.elf`\n', comment)
        self.assertIn('current-only: `b1: device/n/n.elf`', md)  # the full report keeps its terms

    def test_a_board_without_baseline_leaves_coverage_incomplete(self):
        md, comment, data = self.compare([_shard('b1', {'device/a/a.elf': _elf(10)})],
                                         [_shard('b1', {'device/a/a.elf': _elf(10)}), _shard('b2', {'device/a/a.elf': _elf(1)})])
        self.assertEqual(data['no_baseline'], ['b2'])
        self.assertEqual(data['status'], 'INCOMPLETE')
        self.assertIn('FAILED `b2` base snapshot: no baseline snapshot', md)
        self.assertEqual(len(data['pairs']), 1)
        self.assertIn('1 builds on 1 boards compared, 0 changed\n\nNot compared:\n\n'
                      '- FAILED `b2` master snapshot: no baseline snapshot\n', comment)
        self.assertNotIn('INCOMPLETE', comment)

    def test_a_missing_scope_or_a_failed_build_leaves_coverage_incomplete(self):
        broken = {**_shard('b1', {'device/a/a.elf': _elf(1)}), 'build_outcome': 'failure'}
        _md, _c, data = self.compare([_shard('b1', {'device/a/a.elf': _elf(1)})], [broken], scope=None)
        self.assertEqual(self.failed(data), {('scope', 'current', 'snapshot'), ('b1', 'current', 'build')})

    def test_a_leg_or_board_that_left_no_snapshot_fails(self):
        scope = {'code_changed': True, 'legs': [{'toolchain': 'arm-gcc', 'arg': 'fam -e device/a'},
                                               {'toolchain': 'riscv-gcc', 'arg': 'other'}]}
        _md, _c, data = self.compare([_shard('b1', {'device/a/a.elf': _elf(1)})],
                                     [_shard('b1', {'device/a/a.elf': _elf(1)}, examples=['device/a'])],
                                     cur_legs={'code-size-arm-gcc-fam': ['b1', 'b2']}, scope=scope)
        self.assertEqual(self.failed(data), {('code-size-riscv-gcc-other', 'current', 'snapshot'),
                                             ('b2', 'current', 'snapshot')})

    def test_a_failed_baseline_elf_is_a_base_failure_not_a_new_elf(self):
        base = _shard('b1', {'device/a/a.elf': _elf(1)},
                      failures=[{'elf': 'device/n/n.elf', 'stage': 'report', 'message': 'boom'}])
        _md, _c, data = self.compare([base], [_shard('b1', {'device/a/a.elf': _elf(1), 'device/n/n.elf': _elf(2)})])
        self.assertEqual(data['current_only'], [])
        self.assertEqual([(f['elf'], f['side']) for f in data['failures']], [('device/n/n.elf', 'base')])

    def test_a_compiler_mismatch_is_a_warning(self):
        md, _c, data = self.compare([_shard('b1', {'device/a/a.elf': _elf(1)}, compiler='gcc 13')],
                                    [_shard('b1', {'device/a/a.elf': _elf(1)})])
        self.assertEqual(data['status'], 'complete')
        self.assertIn('compiler differs', md)

    def test_current_snapshots_of_different_commits_are_not_compared(self):
        _md, _c, data = self.compare([_shard('b1', {'device/a/a.elf': _elf(1)}), _shard('b2', {'device/a/a.elf': _elf(1)})],
                                     [_shard('b1', {'device/a/a.elf': _elf(1)}),
                                      _shard('b2', {'device/a/a.elf': _elf(1)}, head='d')],
                                     cur_legs={'code-size-arm-gcc-fam': ['b1'], 'code-size-arm-gcc-x': ['b2']},
                                     scope={'code_changed': True, 'legs': [{'toolchain': 'arm-gcc', 'arg': 'fam'},
                                                                           {'toolchain': 'arm-gcc', 'arg': 'x'}]})
        self.assertIn(('commits', 'current', 'snapshot'), self.failed(data))
        self.assertEqual(data['pairs'], [])

    def test_baseline_snapshots_of_another_commit_than_the_selected_run_are_dropped(self):
        _md, _c, data = self.compare([_shard('b1', {'device/a/a.elf': _elf(1)}, sha='d')],
                                     [_shard('b1', {'device/a/a.elf': _elf(1)})],
                                     baseline={'sha': 'b' * 40, 'url': 'u', 'exact': True})
        self.assertIn(('commits', 'base', 'snapshot'), self.failed(data))
        self.assertEqual(data['pairs'], [])

    def test_a_run_with_no_leg_to_measure_says_so(self):
        for changed in (False, True):  # e.g. a HIL-harness-only PR selects no pinned leg
            md, comment, data = self.compare([], [], cur_legs={}, scope={'code_changed': changed, 'legs': []})
            self.assertEqual(data, {'status': 'nothing measured'})
            self.assertTrue(comment.startswith('## Code size\n\nNothing to measure'))

    def test_snapshots_a_scope_without_legs_did_not_expect_are_reported(self):
        _md, _c, data = self.compare([_shard('b1', {'device/a/a.elf': _elf(1)})],
                                     [_shard('b1', {'device/a/a.elf': _elf(41)})],
                                     scope={'code_changed': True, 'legs': []})
        self.assertEqual(data['status'], 'INCOMPLETE')
        self.assertIn(('code-size-arm-gcc-fam', 'current', 'scope'), self.failed(data))

    def test_the_comment_has_a_row_per_changed_file_however_small(self):
        def msc(n, ram=0):
            return elf({'class/msc/msc_device.c': (n, 0), 'device/usbd.c': (500, ram)})
        base = [_shard('b1', {'device/msc/msc.elf': msc(4000), 'device/hid/hid.elf': elf({'class/hid/hid_device.c': (1200, 128)}),
                              'device/n/n.elf': elf({'device/usbd.c': (500, 0)}, all_syms=(900, 9))}),
                _shard('b2', {'device/msc/msc.elf': msc(3800)})]
        cur = [_shard('b1', {'device/msc/msc.elf': msc(4200, 4), 'device/hid/hid.elf': elf({'class/hid/hid_device.c': (1224, 136)}),
                             'device/n/n.elf': elf({'device/usbd.c': (500, 0)}, all_syms=(904, 9))}),
               _shard('b2', {'device/msc/msc.elf': msc(3980)})]
        md, comment, _data = self.compare(base, cur)
        self.assertIn('4 builds on 2 boards compared, 4 changed. Whole firmware¹ Flash Δ +4 → +200, '
                      'RAM Δ 0 → +8', comment)
        rows = [line for line in comment.splitlines() if line.startswith('| ') and 'File' not in line]
        self.assertEqual([[c.strip() for c in r.strip('|').split('|')] for r in rows],
                         [['class/msc/msc_device.c', '+180 → +200', '0'],
                          ['class/hid/hid_device.c', '+24', '+8'],
                          ['device/usbd.c', '0', '0 → +4']])
        self.assertIn('1 other build changed without a TinyUSB file-size change.', comment)
        self.assertNotIn('boards: ', comment)
        self.assertNotIn('Not compared', comment)
        self.assertNotIn('| b1: ', comment)
        self.assertNotIn('<details>', comment)
        self.assertIn('<details>', md)
        self.assertIn('| b1: device/hid/hid.elf ', md)  # the full report keeps every pair

    def test_a_comment_over_the_limit_is_cut_at_a_line(self):
        base = _shard('b1', {'device/a/a.elf': elf({f'f{j}.c': (10, 0) for j in range(50)})})
        cur = _shard('b1', {'device/a/a.elf': elf({f'f{j}.c': (11, 0) for j in range(50)})})
        with mock.patch.object(sd, 'COMMENT_LIMIT', 800):
            _md, comment, _data = self.compare([base], [cur])
        self.assertLessEqual(len(comment), 800)
        self.assertIn('Whole firmware¹', comment)
        self.assertTrue(comment.endswith('_Truncated: see the full report._\n\n'
                                         '¹ Sum of all symbols measured by membrowse, not the exact image size.\n'))

    def test_a_symbol_only_change_counts_under_symbols(self):
        sizes = lambda a, b: elf({'x.c': (10, 0)}, symbols={'x.c': {'.text': {'f': a, 'g': b}}})  # noqa: E731
        for symbols, head in ((False, '1 builds on 1 boards compared, 0 changed'),
                              (True, '1 builds on 1 boards compared, 1 changed')):
            _md, comment, _data = self.compare([_shard('b1', {'device/a/a.elf': sizes(4, 6)})],
                                               [_shard('b1', {'device/a/a.elf': sizes(6, 4)})], symbols=symbols)
            self.assertIn(head, comment)
        self.assertIn('1 other build changed without a TinyUSB file-size change.', comment)

    def test_pr_controlled_names_cannot_inject_markdown(self):
        evil = 'x`|<img src=x>@team\n\n# FORGED\r\n'
        sizes = lambda n: elf({f'{evil}.c': (n, 0)}, symbols={f'{evil}.c': {evil: {evil: n}}})  # noqa: E731
        fail = [{'elf': None, 'stage': 'build', 'message': '<script>@maintainers `x`'}]
        base = _shard('b1', {'device/a/a.elf': sizes(1)})
        with tempfile.TemporaryDirectory() as b, tempfile.TemporaryDirectory() as c:
            md, comment, _data = sd.compare_runs(
                _run_dir(b, [base]), _run_dir(c, [_shard('b1', {'device/a/a.elf': sizes(2)}, failures=fail)],
                                              scope=self.SCOPE), symbols=True)
        for text in (md, comment):
            self.assertNotIn('<img', text)
            self.assertNotIn('<script', text)
            self.assertIsNone(re.search(r'@\w', text))
            self.assertNotIn('`|', text)
            self.assertIsNone(re.search(r'^# FORGED', text, re.M))  # stays inside its cell
        self.assertIn('&#60;img', md)  # escaped, still shown

    def test_names_that_escape_alike_stay_distinct(self):
        files = lambda a, b: elf({'x@.c': (a, 0), 'x#.c': (b, 0)})  # noqa: E731
        _md, _c, data = self.compare([_shard('b1', {'device/a/a.elf': files(1, 2)})],
                                     [_shard('b1', {'device/a/a.elf': files(1, 5)})])
        self.assertEqual(sorted(data['pairs'][0]['current']['files']), ['x#.c', 'x@.c'])

    def test_invalid_shards_are_errors_not_data(self):
        good = _shard('b0', {})
        bad = [_shard('b1', {'../x.elf': _elf(1)}), _shard('b2', {'device/a/a.elf': {'files': {}}}),
               {**_shard('b3', {}), 'engine': 'bloaty'}, {**_shard('b4', {}), 'schema': 2},
               _shard('b5', {}, failures=[1]),
               _shard('b7', {'device/a/a.elf': {**_elf(1), 'files': 1}}),
               _shard('b8', {'device/a/a.elf': {**_elf(1), 'sections': {'x.c': {'.text': True}}}}),
               _shard('b10', {}, failures=[{'stage': 'build', 'message': 'no elf key'}])]
        with tempfile.TemporaryDirectory() as c:
            run = _run_dir(c, [good] + bad)
            _write(c, 'code-size-scope/scope.json', {'schema': 1, 'code_changed': True, 'legs': None})
            run = sd.load_snapshots(c)
            sd.compare_runs(run, run)  # malformed input never crashes
        self.assertEqual(list(run['shards']), ['b0'])
        self.assertIsNone(run['scope'])
        self.assertEqual(len(run['errors']), len(bad) + 1)

    def test_invalid_legs_are_errors_not_data(self):
        good = {'schema': 1, 'boards': [], 'examples': None, 'build_outcome': 'success',
                'sha': 'a' * 40, 'base_sha': 'a' * 40, 'head_sha': 'a' * 40}
        bad = [{k: v for k, v in good.items() if k != 'examples'}, {**good, 'examples': 1},
               {**good, 'sha': 'nothex'}, {**good, 'boards': None}, {**good, 'build_outcome': None}]
        with tempfile.TemporaryDirectory() as c:
            for i, leg in enumerate([good] + bad):
                _write(c, f'code-size-arm-gcc-f{i}/leg.json', leg)
            run = sd.load_snapshots(c)
        self.assertEqual(list(run['legs']), ['code-size-arm-gcc-f0'])
        self.assertEqual(len(run['errors']), len(bad))

    def test_an_invalid_baseline_shard_of_a_board_the_pr_did_not_build_is_ignored(self):
        with tempfile.TemporaryDirectory() as b, tempfile.TemporaryDirectory() as c:
            base = _run_dir(b, [_shard('b1', {'device/a/a.elf': _elf(1)})])
            _write(b, 'code-size-arm-gcc-fam/code-size-other.json', {'schema': 1})
            _md, _c, data = sd.compare_runs(sd.load_snapshots(b), _run_dir(
                c, [_shard('b1', {'device/a/a.elf': _elf(1)})], scope=self.SCOPE))
        self.assertEqual(data['status'], 'complete')

    def test_symbols_are_omitted_when_a_snapshot_lacks_them(self):
        bare = {k: v for k, v in _elf(1).items() if k != 'symbols'}
        with tempfile.TemporaryDirectory() as b, tempfile.TemporaryDirectory() as c:
            md, _c, _d = sd.compare_runs(_run_dir(b, [_shard('b1', {'device/a/a.elf': bare})]),
                                         _run_dir(c, [_shard('b1', {'device/a/a.elf': _elf(2)})], scope=self.SCOPE),
                                         symbols=True)
        self.assertIn('symbols omitted', md)

    def test_a_board_in_two_artifacts_is_rejected(self):
        with tempfile.TemporaryDirectory() as c:
            run = _run_dir(c, [_shard('b1', {})], legs={'code-size-arm-gcc-x': ['b1'], 'code-size-riscv-gcc-y': ['b1']})
        self.assertEqual(list(run['shards']), ['b1'])
        self.assertEqual(len(run['errors']), 1)

    def test_an_approximate_baseline_is_labelled_with_its_note(self):
        md, _c, _d = self.compare([_shard('b1', {'device/a/a.elf': _elf(1)}, sha='d')],
                                  [_shard('b1', {'device/a/a.elf': _elf(1)})],
                                  baseline={'sha': 'd' * 40, 'url': 'https://github.com/x/y/actions/runs/1',
                                            'exact': False, 'note': '2 master commits before this build'})
        self.assertIn('Baseline: dddddddddd (approximate) https://github.com/x/y/actions/runs/1 - 2 master '
                      'commits before this build', md)

    def test_an_unavailable_baseline_is_said_so(self):
        md, _c, data = self.compare([], [_shard('b1', {'device/a/a.elf': _elf(1)})],
                                    baseline={'sha': None, 'url': None, 'exact': False, 'note': 'base x is not on master'})
        self.assertIn('Baseline: unavailable - base x is not on master', md)
        self.assertEqual(data['no_baseline'], ['b1'])

    def test_cli_writes_the_three_reports(self):
        with tempfile.TemporaryDirectory() as b, tempfile.TemporaryDirectory() as c, \
             tempfile.TemporaryDirectory() as out:
            _run_dir(b, [_shard('b1', {'device/a/a.elf': _elf(1)})])
            _run_dir(c, [_shard('b1', {'device/a/a.elf': _elf(2)})], scope=self.SCOPE)
            with mock.patch.object(sys, 'argv', ['code_size.py', 'compare', c, b, '-o', out]), \
                 contextlib.redirect_stdout(io.StringIO()):
                self.assertEqual(sd.main(), 0)
            self.assertEqual(sorted(os.listdir(out)), ['code-size.json', 'code-size.md', 'comment.md'])


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
            sd.symlink_deps(main, wt)
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


def _fake_ci(info=None, attempts=(1, 1), error=None):
    """A code_size_ci stand-in: lookup_sha() answers `info` (a found run 7 of 'b'*40 by
    default) or raises `error`; each run lookup returns the next of `attempts`."""
    attempts = iter(attempts)

    def lookup_sha(_repo, _sha, _whose):
        if error:
            raise error
        return info or {'sha': 'b' * 40, 'url': 'https://example/runs/7', 'exact': True, 'run_id': 7}
    return types.SimpleNamespace(
        lookup_sha=lookup_sha, gh=lambda _path: {'run_attempt': next(attempts)})


class CiBaseline(unittest.TestCase):
    def baseline(self, fake, downloads=None, rc=0):
        """ci_baseline() of 'b'*40 with `fake` as code_size_ci and gh run download stubbed
        (exit `rc`): `downloads` collects each -D dir, which gets a scope artifact."""
        def run(cmd, **_kwargs):
            if downloads is not None:
                downloads.append(cmd[-1])
            _write(cmd[-1], 'code-size-scope/scope.json', {'schema': 1, 'code_changed': True, 'legs': []})
            return subprocess.CompletedProcess(cmd, rc, '', 'HTTP 410: artifact expired' if rc else '')
        with mock.patch.object(sd, '_load_module', return_value=fake), \
             mock.patch.object(sd.shutil, 'which', return_value='/usr/bin/gh'), \
             mock.patch.object(sd, 'run', side_effect=run):
            return sd.ci_baseline('o/r', 'b' * 40)

    def test_a_found_run_is_downloaded_once_and_its_manifest_kept_apart(self):
        with tempfile.TemporaryDirectory() as tmp, mock.patch.object(sd, 'BASELINE_CACHE_DIR', tmp):
            downloads = []
            path, info = self.baseline(_fake_ci(), downloads)
            self.assertEqual(info['run_id'], 7)
            cache = os.path.join(tmp, 'o_r', '7-1')
            self.assertEqual(path, os.path.join(cache, 'snapshots'))
            with open(os.path.join(cache, 'manifest.json')) as f:
                self.assertEqual(json.load(f), {'repo': 'o/r', 'run_id': 7, 'run_attempt': 1})
            self.assertEqual(sd.load_snapshots(path)['errors'], [])  # the manifest is no snapshot file
            self.assertEqual(self.baseline(_fake_ci(attempts=(1,)), downloads)[0], path)
            self.assertEqual(len(downloads), 1)  # the second lookup reused the published copy

    def test_a_rerun_during_the_download_is_retried_under_its_attempt(self):
        with tempfile.TemporaryDirectory() as tmp, mock.patch.object(sd, 'BASELINE_CACHE_DIR', tmp):
            downloads = []
            path, _info = self.baseline(_fake_ci(attempts=(1, 2, 2)), downloads)
            self.assertEqual(os.path.basename(os.path.dirname(path)), '7-2')
            self.assertEqual(len(downloads), 2)
            self.assertEqual(os.listdir(os.path.join(tmp, 'o_r')), ['7-2'])  # the first copy discarded

    def test_what_makes_the_ci_base_unavailable(self):
        cases = {'no master Build run with snapshots': _fake_ci(info={'sha': None, 'note': 'no master Build run with '
                                                                                          'snapshots'}),
                 'the baseline lookup failed: gh api x: HTTP 401': _fake_ci(error=RuntimeError('gh api x: HTTP 401')),
                 'running gh failed: Too many open files':
                     _fake_ci(error=OSError(errno.EMFILE, 'Too many open files'))}
        for why, fake in cases.items():
            with self.subTest(why), tempfile.TemporaryDirectory() as tmp, mock.patch.object(sd, 'BASELINE_CACHE_DIR', tmp):
                with self.assertRaisesRegex(sd.BaselineUnavailable, re.escape(why)):
                    self.baseline(fake)
        with mock.patch.object(sd.shutil, 'which', return_value=None), \
             self.assertRaisesRegex(sd.BaselineUnavailable, 'gh not found'):
            sd.ci_baseline('o/r', 'b' * 40)

    def test_a_failed_download_publishes_nothing(self):
        with tempfile.TemporaryDirectory() as tmp, mock.patch.object(sd, 'BASELINE_CACHE_DIR', tmp):
            with self.assertRaisesRegex(sd.BaselineUnavailable, 'artifact expired'):
                self.baseline(_fake_ci(), rc=1)
            self.assertEqual(os.listdir(os.path.join(tmp, 'o_r')), [])

    def test_a_failed_publish_is_unavailable_and_leaves_no_temp_copy(self):
        cases = {'rename': mock.patch.object(sd.os, 'rename', side_effect=OSError(errno.EXDEV, 'Cross-device link')),
                 'manifest': mock.patch.object(sd, 'open', create=True,
                                               side_effect=OSError(errno.ENOSPC, 'No space left on device'))}
        for why, patch in cases.items():
            with self.subTest(why), tempfile.TemporaryDirectory() as tmp, \
                 mock.patch.object(sd, 'BASELINE_CACHE_DIR', tmp), patch:
                with self.assertRaisesRegex(sd.BaselineUnavailable, 'caching run 7'):
                    self.baseline(_fake_ci())
                self.assertEqual(os.listdir(os.path.join(tmp, 'o_r')), [])

    def test_a_failed_rerun_check_leaves_no_temp_copy(self):
        fake = _fake_ci()
        fake.gh = mock.Mock(side_effect=[{'run_attempt': 1}, RuntimeError('gh api x: HTTP 502')])
        with tempfile.TemporaryDirectory() as tmp, mock.patch.object(sd, 'BASELINE_CACHE_DIR', tmp):
            with self.assertRaisesRegex(sd.BaselineUnavailable, 'HTTP 502'):
                self.baseline(fake)
            self.assertEqual(os.listdir(os.path.join(tmp, 'o_r')), [])

    def test_a_copy_published_first_by_another_run_is_used(self):
        def publish_first(_src, dst):
            _write(dst, 'snapshots/theirs.json', {})
            _write(dst, 'manifest.json', {'repo': 'o/r', 'run_id': 7, 'run_attempt': 1})
            raise OSError(errno.ENOTEMPTY, 'Directory not empty')
        with tempfile.TemporaryDirectory() as tmp, mock.patch.object(sd, 'BASELINE_CACHE_DIR', tmp), \
             mock.patch.object(sd.os, 'rename', side_effect=publish_first):
            path, _info = self.baseline(_fake_ci())
            self.assertEqual(path, os.path.join(tmp, 'o_r', '7-1', 'snapshots'))
            self.assertTrue(os.path.isfile(os.path.join(path, 'theirs.json')))
            self.assertEqual(os.listdir(os.path.join(tmp, 'o_r')), ['7-1'])


class BaselineShards(unittest.TestCase):
    BASELINE = {'sha': 'b' * 40}

    def shards(self, shards, boards=('b1',), legs=None):
        with tempfile.TemporaryDirectory() as tmp:
            return sd.baseline_shards(_run_dir(tmp, shards, legs), self.BASELINE, list(boards))

    def test_only_built_boards_count(self):
        shards, failures = self.shards([_shard('b1', {'device/a/a.elf': _elf(1)}, sha='b'),
                                        _shard('b1-DMA', {'device/a/a.elf': _elf(2)}, sha='b')])
        self.assertEqual((list(shards), failures), (['b1'], []))

    def test_unusable_baselines_are_base_failures_never_zero(self):
        cases = {
            'no baseline snapshot': [_shard('b2', {}, sha='b')],
            'the build step ended failure': [{**_shard('b1', {'device/a/a.elf': _elf(1)}, sha='b'),
                                              'build_outcome': 'failure'}],
            'the snapshots are not of the selected baseline commit': [_shard('b1', {'device/a/a.elf': _elf(1)}, sha='d')],
        }
        for why, shards in cases.items():
            with self.subTest(why):
                usable, failures = self.shards(shards)
                self.assertEqual(usable, {})
                self.assertIn(why, [f[3] for f in failures])
                self.assertTrue(all(f[1] == 'base' for f in failures))

    def test_a_shard_outside_its_legs_boards_is_a_base_failure(self):
        with tempfile.TemporaryDirectory() as tmp:
            _run_dir(tmp, [_shard('b9', {}, sha='b')])
            _write(tmp, 'code-size-arm-gcc-fam/code-size-b1.json',
                   {k: v for k, v in _shard('b1', {'device/a/a.elf': _elf(1)}).items() if k not in LEG_KEYS})
            usable, failures = sd.baseline_shards(sd.load_snapshots(tmp), self.BASELINE, ['b1'])
        self.assertEqual(usable, {})
        self.assertEqual(failures, [(('b1', None), 'base', 'snapshot', 'not a board of its leg\'s build')])


class PairBaseShard(unittest.TestCase):
    def test_examples_scope_the_base_and_a_failed_base_elf_is_not_new(self):
        base = _shard('b1', {'device/a/a.elf': _elf(1), 'device/z/z.elf': _elf(2)},
                      failures=[{'elf': 'device/a/n.elf', 'stage': 'report', 'message': 'boom'},
                                {'elf': 'device/z/y.elf', 'stage': 'report', 'message': 'other scope'}])
        cur = {'device/a/a.elf': _elf(3), 'device/a/n.elf': _elf(1)}
        sizes, failures, current, outside = sd._pair_base_shard('b1', base, cur, ['device/a'])
        self.assertEqual(sizes, {('b1', 'device/a/a.elf'): _elf(1)})
        self.assertEqual(failures, [(('b1', 'device/a/n.elf'), 'base', 'report', 'boom')])
        self.assertEqual(current, {('b1', 'device/a/a.elf'): _elf(3)})
        self.assertEqual(outside, 1)


class MetadataWarnings(unittest.TestCase):
    GCC = {'id': 'GNU', 'name': 'arm-none-eabi-gcc', 'version': '14.2.1', 'build_type': 'MinSizeRel'}

    def warnings(self, base, cur, base_m='1.2.12', cur_m='1.2.12'):
        return sd.metadata_warnings('b1', {'compiler': base, 'membrowse_version': base_m}, cur, cur_m)

    def test_matching_metadata_warns_nothing(self):
        self.assertEqual(self.warnings(self.GCC, dict(self.GCC)), [])
        esp = {**self.GCC, 'build_type': ''}  # ESP-IDF records no build type on either side
        self.assertEqual(self.warnings(esp, dict(esp)), [])

    def test_each_kind_of_difference_shows_both_sides(self):
        self.assertEqual(self.warnings(self.GCC, {**self.GCC, 'version': '13.3.1'}),
                         ['b1: base built with GNU arm-none-eabi-gcc 14.2.1 MinSizeRel, current with GNU '
                          'arm-none-eabi-gcc 13.3.1 MinSizeRel: deltas may include the toolchain change'])
        self.assertIn('base built with GNU arm-none-eabi-gcc 14.2.1 MinSizeRel, current with Clang arm-none-eabi-gcc',
                      self.warnings(self.GCC, {**self.GCC, 'id': 'Clang'})[0])
        self.assertEqual(self.warnings(self.GCC, self.GCC, cur_m='1.2.9'),
                         ['b1: base measured with membrowse 1.2.12, current with 1.2.9: per-file attribution may differ'])

    def test_unknown_metadata_names_every_unknown_side(self):
        self.assertEqual(self.warnings({}, {}, base_m='', cur_m=''),
                         ['b1: compiler unknown on the base and current side: comparability cannot be established',
                          'b1: membrowse version unknown on the base and current side: comparability cannot be '
                          'established'])
        self.assertIn('compiler unknown on the current side', self.warnings(self.GCC, {**self.GCC, 'version': ''})[0])
        self.assertIn('compiler unknown on the base side', self.warnings('gcc 14', self.GCC)[0])
        self.assertEqual(self.warnings(self.GCC, self.GCC, cur_m=''),
                         ['b1: membrowse version unknown on the current side: comparability cannot be established'])


class DiffBaseSource(unittest.TestCase):
    """main()'s diff with the base from CI snapshots, from a local build, or the fallback."""
    GCC = MetadataWarnings.GCC

    def base(self, elfs=None, **kwargs):
        """A baseline shard of b1 measured like the current build."""
        return {**_shard('b1', elfs or {'device/a/a.elf': _elf(1)}, sha='b', **kwargs),
                'compiler': self.GCC, 'membrowse_version': '1.2.9'}

    def diff(self, tmp, argv, unavailable=None, cur=None, base_shards=None, compiler=None, extra=None,
             note=None, subject="ci: base's commit (#1)"):
        """main()'s diff of `-b b1 --json` + `argv`: ci_baseline() returns `base_shards` (plus
        `extra` files) as run 7 of 'b'*40, approximate with `note`, or raises `unavailable`; git
        knows the base commit as `subject`, not at all when None; the current side sizes as
        `cur`. Returns rc, out, builds as (src, board), git commands, md, data, looked_up."""
        builds, cmds = [], []
        cur = cur if cur is not None else {'device/a/a.elf': _elf(3)}

        def ci_baseline(_repo, _sha):
            if unavailable:
                raise sd.BaselineUnavailable(unavailable)
            root = os.path.join(tmp, '_dl')
            _run_dir(root, base_shards or [self.base()])
            for rel, text in (extra or {}).items():
                os.makedirs(os.path.dirname(os.path.join(root, rel)), exist_ok=True)
                with open(os.path.join(root, rel), 'w') as f:
                    f.write(text)
            info = {'sha': 'b' * 40, 'url': 'https://example/runs/7', 'exact': note is None, 'run_id': 7}
            return root, {**info, 'note': note} if note else info

        def build(src, _build_dir, board, *_a):
            builds.append((src, board))
            os.makedirs(os.path.join(tmp, board), exist_ok=True)

        def run(cmd, **_kwargs):
            cmds.append(cmd[3:5])
            if cmd[3:5] == ['worktree', 'add']:
                os.makedirs(cmd[-2])
            if cmd[3] == 'log':
                return subprocess.CompletedProcess(cmd, 128 if subject is None else 0, f'{subject}\n', '')
            return subprocess.CompletedProcess(cmd, 0, 'c0ffee\n', '')

        def generate(build_dir, *_a):
            return (cur if build_dir.endswith('build') else {'device/a/a.elf': _elf(1)}), []

        with mock.patch.object(sys, 'argv', ['code_size.py', 'diff', '-b', 'b1', '--json'] + argv), \
             mock.patch.object(sd, 'CODE_SIZE_DIR', tmp), mock.patch.object(sd, 'run', side_effect=run), \
             mock.patch.object(sd, 'symlink_deps'), mock.patch.object(sd, 'build_board', side_effect=build), \
             mock.patch.object(sd, 'generate_sizes', side_effect=generate), \
             mock.patch.object(sd, 'ci_baseline', side_effect=ci_baseline) as lookup, \
             mock.patch.object(sd, '_cmake_compiler', return_value=compiler or self.GCC), \
             mock.patch.object(sd, '_membrowse_version', return_value='1.2.9'), \
             mock.patch.object(sd, 'engine_missing', return_value=False):
            buf = io.StringIO()
            with contextlib.redirect_stdout(buf):
                rc = sd.main()
        md = data = None
        reports = sorted(f for f in os.listdir(os.path.join(tmp, 'b1')) if f.startswith('diff')) \
            if os.path.isdir(os.path.join(tmp, 'b1')) else []
        if reports:
            with open(os.path.join(tmp, 'b1', reports[0].rsplit('.', 1)[0] + '.md')) as f:
                md = unpad(f.read())
            with open(os.path.join(tmp, 'b1', reports[0].rsplit('.', 1)[0] + '.json')) as f:
                data = json.load(f)
        return types.SimpleNamespace(rc=rc, out=unpad(buf.getvalue()), builds=builds, cmds=cmds, md=md, data=data,
                                     looked_up=lookup.called)

    def test_the_default_compares_against_the_ci_snapshots_without_a_base_build(self):
        with tempfile.TemporaryDirectory() as tmp:
            r = self.diff(tmp, [], base_shards=[self.base(), _shard('b1-DMA', {'device/a/a.elf': _elf(9)}, sha='b')])
        self.assertEqual(r.rc, 0)
        self.assertEqual(r.builds, [(sd.TINYUSB_ROOT, 'b1')])
        self.assertNotIn(['worktree', 'add'], r.cmds)
        self.assertIn(f'Base: CI snapshots of {"b" * 10} "ci: base\'s commit (#1)" https://example/runs/7\n', r.out)
        self.assertNotIn('b1-DMA', r.out)  # snapshots of boards not diffed go unmentioned
        self.assertIn('1 pair, 1 changed; TinyUSB Flash Δ +2', r.out)
        self.assertTrue(r.md.startswith(f'Base: CI snapshots of {"b" * 10} "ci: base\'s commit (#1)" https://'))
        self.assertEqual(r.data['base_source'], {'requested': 'default', 'effective': 'ci', 'reason': None,
                                                 'requested_sha': 'c0ffee', 'base_sha': 'b' * 40,
                                                 'baseline': {'sha': 'b' * 40, 'url': 'https://example/runs/7',
                                                              'exact': True, 'run_id': 7}})
        self.assertIsNone(r.data['filters']['base'])
        self.assertEqual(r.data['warnings'], [])

    def test_an_ancestor_base_says_how_far_back_it_is(self):
        note = '2 master commits before this build\'s base c0ffee: their changes count as yours'
        with tempfile.TemporaryDirectory() as tmp:
            r = self.diff(tmp, [], note=note, subject=None)  # not fetched: no subject
        self.assertIn(f'Base: CI snapshots of {"b" * 10}, {note} https://example/runs/7\n', r.out)

    def test_a_metadata_difference_warns_and_still_compares(self):
        with tempfile.TemporaryDirectory() as tmp:
            r = self.diff(tmp, [], compiler={**self.GCC, 'version': '13.3.1'})
        warning = ('b1: base built with GNU arm-none-eabi-gcc 14.2.1 MinSizeRel, current with GNU arm-none-eabi-gcc '
                   '13.3.1 MinSizeRel: deltas may include the toolchain change')
        self.assertEqual(r.rc, 0)
        self.assertIn(f'  WARNING {warning}', r.out)
        self.assertIn(f'- {warning}', r.md)
        self.assertEqual(r.data['warnings'], [warning])
        self.assertEqual(len(r.data['pairs']), 1)

    def test_an_unavailable_ci_base_falls_back_by_default_and_fails_when_required(self):
        with tempfile.TemporaryDirectory() as tmp:
            r = self.diff(tmp, [], unavailable='gh not found')
            self.assertEqual(r.rc, 0)
            self.assertEqual(r.builds, [(os.path.join(tmp, '_worktree'), 'b1'), (sd.TINYUSB_ROOT, 'b1')])
            self.assertIn(['worktree', 'add'], r.cmds)
            self.assertIn('Base: local build of c0ffee (CI base unavailable: gh not found)', r.out)
            self.assertTrue(r.md.startswith('Base: local build of c0ffee (CI base unavailable: gh not found)'))
            self.assertEqual({k: r.data['base_source'][k] for k in ('requested', 'effective', 'reason', 'base_sha')},
                             {'requested': 'default', 'effective': 'local', 'reason': 'gh not found', 'base_sha': 'c0ffee'})
        with tempfile.TemporaryDirectory() as tmp:
            r = self.diff(tmp, ['--base-source', 'ci'], unavailable='gh not found')
        self.assertEqual((r.rc, r.builds, r.md), (1, [], None))
        self.assertNotIn(['worktree', 'add'], r.cmds)
        self.assertIn('Error: CI base unavailable: gh not found (--base-source local builds it)', r.out)

    def test_options_a_ci_base_cannot_serve_use_a_local_base_or_are_refused(self):
        with tempfile.TemporaryDirectory() as tmp:
            r = self.diff(tmp, ['-f', 'src/'])
        self.assertEqual((r.rc, r.looked_up, len(r.builds)), (0, False, 2))
        self.assertEqual(r.data['base_source']['reason'], '-f needs a local base')
        with tempfile.TemporaryDirectory() as tmp, contextlib.redirect_stderr(io.StringIO()), \
             self.assertRaises(SystemExit):
            self.diff(tmp, ['--base-source', 'ci', '--engine', 'linkermap'])

    def test_an_explicit_local_base_makes_no_github_lookup(self):
        with tempfile.TemporaryDirectory() as tmp:
            r = self.diff(tmp, ['--base-source', 'local'])
        self.assertEqual((r.rc, r.looked_up, len(r.builds)), (0, False, 2))
        self.assertNotIn('Base:', r.out)
        self.assertEqual(r.data['base_source']['effective'], 'local')

    def test_a_failed_baseline_elf_stays_a_failure(self):
        base = self.base(failures=[{'elf': 'device/n/n.elf', 'stage': 'report', 'message': 'boom'}])
        with tempfile.TemporaryDirectory() as tmp:
            r = self.diff(tmp, [], base_shards=[base], cur={'device/a/a.elf': _elf(3), 'device/n/n.elf': _elf(1)})
        self.assertEqual(r.rc, 1)
        self.assertEqual(r.data['current_only'], [])
        self.assertIn('FAILED `b1: device/n/n.elf` base report: boom', r.md)

    def test_a_board_without_a_baseline_is_a_failure_not_new_elfs(self):
        with tempfile.TemporaryDirectory() as tmp:
            r = self.diff(tmp, [], base_shards=[_shard('b9', {}, sha='b')])
        self.assertEqual(r.rc, 1)
        self.assertEqual((r.data['pairs'], r.data['current_only']), ([], []))
        self.assertIn('FAILED `b1` base snapshot: no baseline snapshot', r.md)

    def test_a_snapshot_without_symbols_omits_them_with_a_warning_never_zero(self):
        base = self.base()
        base['elfs']['device/a/a.elf'].pop('symbols')
        for argv in ([], ['-e', 'device/a', '--combined']):  # -e prints the changed tables too
            with self.subTest(argv), tempfile.TemporaryDirectory() as tmp:
                r = self.diff(tmp, ['--symbols'] + argv, base_shards=[base])
                self.assertEqual(r.rc, 0)
                self.assertIn('  WARNING b1: symbols omitted: the CI snapshot has none', r.out)
                self.assertIn('b1: symbols omitted: the CI snapshot has none', r.data['warnings'])
                if argv:
                    with open(os.path.join(tmp, '_combined', 'diff.md')) as f:
                        self.assertIn('- b1: symbols omitted: the CI snapshot has none', f.read())

    def test_a_failure_of_the_downloaded_run_itself_is_in_every_report(self):
        with tempfile.TemporaryDirectory() as tmp:
            r = self.diff(tmp, ['--combined'], extra={'code-size-arm-gcc-x/code-size-b1.json': '{broken'})
            with open(os.path.join(tmp, '_combined', 'diff.md')) as f:
                combined = f.read()
        self.assertEqual(r.rc, 1)
        self.assertEqual(r.data['status'], 'INCOMPLETE')
        self.assertIn('unreadable JSON', r.md)
        self.assertIn('unreadable JSON', combined)


if __name__ == '__main__':
    unittest.main()
