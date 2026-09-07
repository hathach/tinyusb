#!/usr/bin/env python3
"""Unit tests for tools/membrowse_compare.py (pure functions, no build needed)."""
import os
import subprocess
import sys
import unittest

REPO = subprocess.run(['git', 'rev-parse', '--show-toplevel'],
                      capture_output=True, text=True, check=True).stdout.strip()
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


if __name__ == '__main__':
    unittest.main()
