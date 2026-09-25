#!/usr/bin/env python3
"""Unit tests for tools/code_size.py.

glob.glob, the engines and the build steps are monkeypatched in the generate_sizes
and main tests, so no build and no real membrowse CLI invocation is needed.
"""
import contextlib
import io
import json
import os
import re
import struct
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), '../../..'))
sys.path.insert(0, os.path.join(REPO, 'tools'))
import code_size as sd  # noqa: E402


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
        self.assertIn('filtered Flash 80, RAM 20; all symbols Flash 500, RAM 40', md)
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

    def test_a_timeout_keeps_the_output_as_text(self):
        # TimeoutExpired carries bytes even with text=True
        expired = subprocess.TimeoutExpired(['ninja'], 600, output=b'partial\n', stderr=b'err')
        with mock.patch('subprocess.run', side_effect=expired):
            ret = sd.run(['ninja'], timeout=600)
        self.assertEqual((ret.returncode, ret.stdout), (124, 'partial\n'))
        self.assertTrue(ret.stderr.startswith('err\nCommand timed out after 600s'))


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


class MainFailure(unittest.TestCase):
    def _run_main(self, tmp, argv, build_board, generate=None, run=None):
        """main() with the build, sizing and command steps stubbed. `build_board`
        is its side_effect (it must create the board dir, as the real one does);
        `generate` the generate_sizes() side_effect. Returns (rc, stdout)."""
        ok = subprocess.CompletedProcess([], 0, 'c0ffee\n', '')
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
            self.assertIn('  1 pair, 1 changed; filtered Flash Δ +2, RAM Δ 0\n', out)
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
            return stuck if cmd[3:5] == ['worktree', 'remove'] else ok
        with tempfile.TemporaryDirectory() as tmp:
            def build(_src, _build_dir, board, *_args):
                os.makedirs(os.path.join(tmp, board), exist_ok=True)
                return None
            side_sizes = iter([({'ex/ex.elf': _elf(1)}, [])] * 2)
            rc, out = self._run_main(tmp, ['-b', 'b'], build, lambda *_a, **_k: next(side_sizes), run)
        self.assertEqual(rc, 1)
        self.assertIn('Error removing worktree', out)

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
                                  r'  size and compare… \d+\.\ds\n  1 pair, 1 changed; filtered Flash Δ \+2')
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
            self.assertIn('1 pair, 1 changed; filtered Flash Δ -8', out)
            with open(os.path.join(tmp, 'b', 'diff.md')) as f:
                self.assertIn('| x.c | 8 → 0 | -8 | 0 → 0 | 0 |', unpad(f.read()))

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

    def test_a_failed_worktree_setup_leaves_no_previous_report(self):
        with tempfile.TemporaryDirectory() as tmp:
            os.makedirs(os.path.join(tmp, 'b'))
            stale = [os.path.join(tmp, 'b', f'diff.{ext}') for ext in ('md', 'json')]
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
                                       r'  size… \d+\.\ds\n  1 of 1 elfs sized; filtered Flash 4, RAM 0\n')
            self.assertIn('\n | x.c | 4 | 4 | 100.0% |', self.out)

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
            return 'boom'  # stop at the base build: only the board set matters
        ok = subprocess.CompletedProcess([], 0, '', '')
        with mock.patch.object(sys, 'argv', ['x', 'diff', '--ci', '-b', 'extra', '-b', 'b1']), \
             mock.patch.object(sd, 'CODE_SIZE_DIR', tmp), \
             mock.patch.object(sd, 'CI_PINNED_BOARDS', pinned), \
             mock.patch.object(sd, 'run', return_value=ok), \
             mock.patch.object(sd, 'symlink_deps'), \
             mock.patch.object(sd, 'build_board', side_effect=build), \
             contextlib.redirect_stdout(io.StringIO()):
            sd.main()
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


if __name__ == '__main__':
    unittest.main()
