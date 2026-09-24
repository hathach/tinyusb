#!/usr/bin/env python3
"""Unit tests for tools/membrowse_compare.py (pure functions, no build needed)."""
import os
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), '../../..'))
sys.path.insert(0, os.path.join(REPO, 'tools'))
import membrowse_compare as mc  # noqa: E402


def fake_report(symbols):
    return {'symbols': symbols}


def fake_report_with_layout(symbols, memory_layout):
    return {'symbols': symbols, 'memory_layout': memory_layout}


def fake_region(address, limit_size, sections):
    """One membrowse 1.2.9 memory-layout region."""
    used = sum(s['size'] for s in sections)
    return {'address': address, 'limit_size': limit_size, 'type': 'UNKNOWN',
            'used_size': used, 'free_size': limit_size - used,
            'utilization_percent': (used / limit_size * 100) if limit_size else 0.0,
            'sections': sections}


def fake_section(name, address, size, section_type):
    return {'name': name, 'address': address, 'size': size, 'type': section_type,
           'end_address': address + size}


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


class PerFileSizes(unittest.TestCase):
    def test_filters_and_buckets(self):
        by_file = mc.per_file_sizes(fake_report(SYMS_BASE), ['/co/src/'])
        self.assertEqual(len(by_file), 1)  # vendor_thing filtered out (hw/mcu, not src/)
        (path, sizes), = by_file.items()
        self.assertIn('dcd_dwc2.c', path)
        self.assertEqual(sizes['flash'], 100)   # .text
        self.assertEqual(sizes['ram'], 64)      # .bss

    def test_data_counts_both(self):
        # no memory_layout: the name-table fallback's BOTH_SECTIONS answer
        syms = [{'name': 'd', 'size': 8, 'section': '.data', 'source_file': 'x.c',
                 'object_file': 'device/cdc_msc/CMakeFiles/cdc_msc.dir/co/src/x.c.obj'}]
        by_file = mc.per_file_sizes(fake_report(syms), ['/co/src/'])
        sizes = by_file[next(iter(by_file))]
        self.assertEqual(sizes['flash'], 8)
        self.assertEqual(sizes['ram'], 8)

    def test_data_counts_both_via_layout_split(self):
        # `.data` has a flash LMA and RAM VMA.
        layout = {
            'FLASH': fake_region(0x08000000, 0x100000,
                                 [fake_section('.data', 0x08000000, 8, 'data')]),
            'RAM': fake_region(0x20000000, 0x20000,
                               [fake_section('.data', 0x20000000, 8, 'data')]),
        }
        syms = [{'name': 'd', 'size': 8, 'section': '.data', 'source_file': 'x.c',
                'object_file': 'device/cdc_msc/CMakeFiles/cdc_msc.dir/co/src/x.c.obj'}]
        by_file = mc.per_file_sizes(fake_report_with_layout(syms, layout), ['/co/src/'])
        sizes = by_file[next(iter(by_file))]
        self.assertEqual(sizes['flash'], 8)
        self.assertEqual(sizes['ram'], 8)

    def test_data_single_ram_region_still_counts_flash_load_image(self):
        # RP2040 reports only `.data`'s RAM VMA, not its flash load image.
        layout = {
            'RAM': fake_region(0x20000000, 0x40000,
                               [fake_section('.data', 0x20000000, 8, 'data')]),
        }
        syms = [{'name': 'd', 'size': 8, 'section': '.data', 'source_file': 'x.c',
                'object_file': 'device/cdc_msc/CMakeFiles/cdc_msc.dir/co/src/x.c.obj'}]
        by_file = mc.per_file_sizes(fake_report_with_layout(syms, layout), ['/co/src/'])
        sizes = by_file[next(iter(by_file))]
        self.assertEqual(sizes['flash'], 8)
        self.assertEqual(sizes['ram'], 8)

    def test_relocate_single_ram_region_still_counts_flash_load_image(self):
        layout = {
            'RAM': fake_region(0x20000000, 0x40000,
                               [fake_section('.relocate', 0x20000000, 8, 'data')]),
        }
        syms = [{'name': 'd', 'size': 8, 'section': '.relocate', 'source_file': 'x.c',
                'object_file': 'device/cdc_msc/CMakeFiles/cdc_msc.dir/co/src/x.c.obj'}]
        sizes = mc.per_file_sizes(fake_report_with_layout(syms, layout), ['/co/src/'])['x.c']
        self.assertEqual(sizes, {'flash': 8, 'ram': 8})

    def test_time_critical_code_counts_flash_load_image_and_ram(self):
        layout = {
            'RAM': fake_region(0x20000000, 0x40000,
                               [fake_section('.time_critical.tinyusb', 0x20000000,
                                             8, 'code')]),
        }
        syms = [{'name': 'dcd_event_handler', 'size': 8,
                 'section': '.time_critical.tinyusb', 'source_file': 'usbd.c',
                 'object_file': 'device/cdc_msc/CMakeFiles/cdc_msc.dir/co/src/device/usbd.c.obj'}]
        sizes = mc.per_file_sizes(fake_report_with_layout(syms, layout),
                                  ['/co/src/'])['device/usbd.c']
        self.assertEqual(sizes, {'flash': 8, 'ram': 8})

    def test_fast_code_in_ilm_counts_flash_load_image_and_ram(self):
        layout = {
            'ILM': fake_region(0x00080000, 0x20000,
                               [fake_section('.fast', 0x00080000, 8, 'code')]),
        }
        syms = [{'name': 'dcd_event_handler', 'size': 8, 'section': '.fast',
                 'source_file': 'usbd.c',
                 'object_file': 'device/cdc_msc/CMakeFiles/cdc_msc.dir/co/src/device/usbd.c.obj'}]
        sizes = mc.per_file_sizes(fake_report_with_layout(syms, layout),
                                  ['/co/src/'])['device/usbd.c']
        self.assertEqual(sizes, {'flash': 8, 'ram': 8})

    def test_split_across_two_ram_regions_stays_ram_only(self):
        # Multiple regions can all be RAM banks.
        layout = {
            'RAM_D1': fake_region(0x24000000, 0x80000,
                                  [fake_section('.bss', 0x24000000, 32, 'bss')]),
            'RAM_D2': fake_region(0x30000000, 0x48000,
                                  [fake_section('.bss', 0x30000000, 32, 'bss')]),
        }
        syms = [{'name': 'b', 'size': 32, 'section': '.bss', 'source_file': 'x.c',
                'object_file': 'device/cdc_msc/CMakeFiles/cdc_msc.dir/co/src/x.c.obj'}]
        by_file = mc.per_file_sizes(fake_report_with_layout(syms, layout), ['/co/src/'])
        sizes = by_file[next(iter(by_file))]
        self.assertEqual(sizes['ram'], 32)
        self.assertEqual(sizes['flash'], 0)

    def test_ccmram_style_symbol_lands_in_ram_via_layout(self):
        # STM32F4 `.ccmram` has a flash load image and CCMRAM run address.
        layout = {
            'FLASH': fake_region(0x08000000, 0x100000, []),
            'CCMRAM': fake_region(0x10000000, 0x10000,
                                  [fake_section('.ccmram', 0x10000000, 64, 'data')]),
        }
        syms = [{'name': 'buf', 'size': 64, 'section': '.ccmram', 'source_file': 'x.c',
                'object_file': 'device/cdc_msc/CMakeFiles/cdc_msc.dir/co/src/x.c.obj'}]
        by_file = mc.per_file_sizes(fake_report_with_layout(syms, layout), ['/co/src/'])
        sizes = by_file[next(iter(by_file))]
        self.assertEqual(sizes['ram'], 64)
        self.assertEqual(sizes['flash'], 64)

    def test_noncacheable_style_symbol_lands_in_ram_via_unrecognized_region(self):
        # NXP's `m_data2` relies on the ELF section-type fallback.
        layout = {
            'm_text': fake_region(0x6000C400, 0x800000, []),
            'm_data2': fake_region(0x20200000, 0x10000,
                                   [fake_section('NonCacheable', 0x20200000, 256, 'data')]),
        }
        syms = [{'name': 'dma_buf', 'size': 256, 'section': 'NonCacheable', 'source_file': 'x.c',
                'object_file': 'device/cdc_msc/CMakeFiles/cdc_msc.dir/co/src/x.c.obj'}]
        by_file = mc.per_file_sizes(fake_report_with_layout(syms, layout), ['/co/src/'])
        sizes = by_file[next(iter(by_file))]
        self.assertEqual(sizes['ram'], 256)
        self.assertEqual(sizes['flash'], 0)

    def test_data_in_vendor_named_ram_still_counts_flash_load_image(self):
        layout = {
            'm_data': fake_region(0x1ffff000, 0x4000,
                                  [fake_section('.data', 0x1ffff000, 8, 'data')]),
        }
        syms = [{'name': 'd', 'size': 8, 'section': '.data', 'source_file': 'x.c',
                'object_file': 'device/cdc_msc/CMakeFiles/cdc_msc.dir/co/src/x.c.obj'}]
        sizes = mc.per_file_sizes(fake_report_with_layout(syms, layout), ['/co/src/'])['x.c']
        self.assertEqual(sizes, {'flash': 8, 'ram': 8})

    def test_noncacheable_style_symbol_without_layout_still_lands_in_ram(self):
        # The name fallback covers reports without a memory layout.
        syms = [{'name': 'dma_buf', 'size': 256, 'section': 'NonCacheable', 'source_file': 'x.c',
                'object_file': 'device/cdc_msc/CMakeFiles/cdc_msc.dir/co/src/x.c.obj'}]
        by_file = mc.per_file_sizes(fake_report(syms), ['/co/src/'])
        sizes = by_file[next(iter(by_file))]
        self.assertEqual(sizes['ram'], 256)
        self.assertEqual(sizes['flash'], 0)

    def test_fallback_table_knows_a_verified_vendor_flash_name(self):
        # imxrt's FlexSPI `.flash_config` is flash-resident.
        syms = [{'name': 'cfg', 'size': 16, 'section': '.flash_config', 'source_file': 'x.c',
                'object_file': 'device/cdc_msc/CMakeFiles/cdc_msc.dir/co/src/x.c.obj'}]
        by_file = mc.per_file_sizes(fake_report(syms), ['/co/src/'])
        sizes = by_file[next(iter(by_file))]
        self.assertEqual(sizes['flash'], 16)
        self.assertEqual(sizes['ram'], 0)

    def test_fallback_table_knows_a_verified_vendor_ram_name(self):
        # LPC55's `m_usb_global` is USB SRAM.
        syms = [{'name': 'usb_buf', 'size': 32, 'section': 'm_usb_global', 'source_file': 'x.c',
                'object_file': 'device/cdc_msc/CMakeFiles/cdc_msc.dir/co/src/x.c.obj'}]
        by_file = mc.per_file_sizes(fake_report(syms), ['/co/src/'])
        sizes = by_file[next(iter(by_file))]
        self.assertEqual(sizes['ram'], 32)
        self.assertEqual(sizes['flash'], 0)

    def test_fallback_table_knows_ccmram_counts_both(self):
        # The name fallback also counts `.ccmram`'s flash load image.
        syms = [{'name': 'buf', 'size': 20, 'section': '.ccmram', 'source_file': 'x.c',
                'object_file': 'device/cdc_msc/CMakeFiles/cdc_msc.dir/co/src/x.c.obj'}]
        by_file = mc.per_file_sizes(fake_report(syms), ['/co/src/'])
        sizes = by_file[next(iter(by_file))]
        self.assertEqual(sizes['flash'], 20)
        self.assertEqual(sizes['ram'], 20)

    def test_unlisted_section_in_flash_region_lands_in_flash_via_layout(self):
        # Layout classification handles unlisted section names.
        layout = {
            'FLASH': fake_region(0x08000000, 0x100000,
                                 [fake_section('.custom_vendor_code', 0x08000000, 40, 'code')]),
            'RAM': fake_region(0x20000000, 0x20000, []),
        }
        syms = [{'name': 'f', 'size': 40, 'section': '.custom_vendor_code', 'source_file': 'x.c',
                'object_file': 'device/cdc_msc/CMakeFiles/cdc_msc.dir/co/src/x.c.obj'}]
        by_file = mc.per_file_sizes(fake_report_with_layout(syms, layout), ['/co/src/'])
        sizes = by_file[next(iter(by_file))]
        self.assertEqual(sizes['flash'], 40)
        self.assertEqual(sizes['ram'], 0)

    def test_key_strips_obj_suffix(self):
        by_file = mc.per_file_sizes(fake_report(SYMS_BASE), ['/co/src/'])
        (path, _), = by_file.items()
        self.assertFalse(path.endswith('.obj'), path)
        self.assertTrue(path.endswith('dcd_dwc2.c'), path)

    def test_relativized_keys_shared_across_checkout_prefixes(self):
        # Different checkout roots must normalize to the same key.
        base = mc.per_file_sizes(fake_report(SYMS_BASE), ['/co/src/'])
        cur = mc.per_file_sizes(fake_report(SYMS_CUR), ['/co2/src/'])
        self.assertEqual(set(base), set(cur))

    def test_falls_back_to_source_file_when_object_file_missing(self):
        # Symbols without object files fall back to `source_file`.
        syms = [{'name': 'archived_thing', 'size': 12, 'section': '.text',
                 'source_file': '/co/src/x.c', 'object_file': ''}]
        by_file = mc.per_file_sizes(fake_report(syms), ['/co/src/'])
        self.assertEqual(len(by_file), 1)
        (path, sizes), = by_file.items()
        self.assertEqual(path, 'x.c')
        self.assertEqual(sizes['flash'], 12)


class CompareReports(unittest.TestCase):
    def test_delta_table(self):
        base = mc.per_file_sizes(fake_report(SYMS_BASE), ['/co/src/'])
        cur = mc.per_file_sizes(fake_report(SYMS_CUR), ['/co2/src/'])
        md = mc.compare_reports(base, cur)
        self.assertIn('dcd_dwc2.c', md)
        self.assertIn('+20', md)          # flash grew 100 -> 120
        self.assertIn('TOTAL', md)


class AllSymbolSizes(unittest.TestCase):
    def test_unattributed_symbol_counts_in_all_symbols_only(self):
        syms = SYMS_BASE + [{'name': 'anon', 'size': 7, 'section': '.text',
                             'source_file': '', 'object_file': ''}]
        sizes = mc.elf_sizes(fake_report(syms), ['/co/src/'])
        self.assertEqual(sizes['all'], {'flash': 100 + 999 + 7, 'ram': 64})
        self.assertEqual(sizes['files'], {'portable/synopsys/dwc2/dcd_dwc2.c':
                                          {'flash': 100, 'ram': 64}})


def elf(files, all_syms=None):
    """elf_sizes() shape: files as {path: (flash, ram)}; all-symbols defaults to their sum."""
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
        return mc.render_pairs(pairs, len(base.keys() & cur.keys()), base_only, cur_only, failures)

    def test_different_elf_sets_give_zero_on_shared_pairs(self):
        a, b, extra = pair_ids(3)
        base = {a: elf({'x.c': (100, 8)}), b: elf({'x.c': (200, 8)})}
        cur = {a: elf({'x.c': (100, 8)}), b: elf({'x.c': (200, 8)}), extra: elf({'x.c': (900, 8)})}
        md = self.render(base, cur)
        self.assertIn('Coverage (INCOMPLETE):** 2 of 2 matched elf pairs compared, 0 changed', md)
        self.assertIn('current-only: `b: device/ex2/ex2.elf`', md)
        self.assertIn('_no changes_', md)

    def test_single_pair_with_an_unmatched_elf_is_incomplete(self):
        a, extra = pair_ids(2)
        md = self.render({a: elf({'x.c': (1, 0)})}, {a: elf({'x.c': (1, 0)}), extra: elf({})})
        self.assertIn('Coverage (INCOMPLETE):** 1 of 1 matched', md)
        self.assertIn('current-only: `b: device/ex1/ex1.elf`', md)

    def test_zero_pairs_are_not_reported_as_no_changes(self):
        a, b = pair_ids(2)
        for base, cur, failures in (({}, {}, [(('b', None), 'current', 'build', 'failed')]),
                                    ({a: elf({})}, {b: elf({})}, [])):
            md = self.render(base, cur, failures)
            self.assertIn('Coverage (INCOMPLETE):** 0 of 0 matched', md)
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
        self.assertIn('Coverage (INCOMPLETE):** 1 of 2 matched elf pairs compared, 1 changed', md)
        self.assertIn('FAILED `b: device/ex1/ex1.elf` base report: boom', md)
        self.assertIn('all symbols: Flash Δ +20, RAM Δ 0', md)
        self.assertIn('| x.c | 100 | 120 | +20 |', md)


if __name__ == '__main__':
    unittest.main()
