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
    """One `memory_layout` region in the real membrowse (1.2.9) --json shape
    (verified against `membrowse report <elf> --json --all-symbols` on a real
    stm32f407disco build: 'type' is 'UNKNOWN' for every region built from a
    parsed linker script - see _classify_region()'s docstring)."""
    used = sum(s['size'] for s in sections)
    return {'address': address, 'limit_size': limit_size, 'type': 'UNKNOWN',
            'used_size': used, 'free_size': limit_size - used,
            'utilization_percent': (used / limit_size * 100) if limit_size else 0.0,
            'sections': sections}


def fake_section(name, address, size, section_type):
    return {'name': name, 'address': address, 'size': size, 'type': section_type,
           'end_address': address + size}


# membrowse (>=1.2.9) truncates `source_file` to a bare basename before it reaches
# the JSON report, so per_file_sizes() filters/keys on `object_file` instead - it
# mirrors CMake's `<target>.dir/<abs-source-path>.obj` layout and still carries the
# full path. `source_file` stays as the (unused-for-matching) basename it really is.
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
        # distinct from test_data_counts_both (no-layout, name-table path): here the
        # 'both' answer comes from the layout itself - '.data' listed under both
        # FLASH (its load/LMA copy) and RAM (its run/VMA copy), exactly how membrowse
        # reports an AT()-relocated section (see _section_regions()'s docstring).
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

    def test_ccmram_style_symbol_lands_in_ram_via_layout(self):
        # stm32f4's core-coupled RAM: FLASH_SECTIONS/RAM_SECTIONS/BOTH_SECTIONS does
        # not list '.ccmram', so before this rule it fell through the old catch-all
        # and was misreported as flash. The region is named CCMRAM (verified against
        # a real stm32f407disco report), recognized directly by _classify_region().
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
        self.assertEqual(sizes['flash'], 0)

    def test_noncacheable_style_symbol_lands_in_ram_via_unrecognized_region(self):
        # imxrt's CFG_TUSB_MEM_SECTION EHCI/DMA buffers: placed in a region named
        # m_data2 (NXP's own naming - see hw/bsp/imxrt/boards/metro_m7_1011/
        # metro_m7_1011.ld) that _classify_region() does not recognize, so this
        # exercises the ELF-section-type tiebreak instead: a writable ('data'-typed)
        # section with no separate flash-side load copy is runtime-only RAM.
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

    def test_noncacheable_style_symbol_without_layout_still_lands_in_ram(self):
        # same symbol as test_noncacheable_style_symbol_lands_in_ram_via_unrecognized_
        # region, but with no memory_layout at all (a report from an unsupported
        # generator, or a caller that never asked for one). 'NonCacheable' is now in
        # RAM_SECTIONS itself (see the module's fallback-table comment), so the
        # fallback gets this one right too, without needing a layout at all.
        syms = [{'name': 'dma_buf', 'size': 256, 'section': 'NonCacheable', 'source_file': 'x.c',
                'object_file': 'device/cdc_msc/CMakeFiles/cdc_msc.dir/co/src/x.c.obj'}]
        by_file = mc.per_file_sizes(fake_report(syms), ['/co/src/'])
        sizes = by_file[next(iter(by_file))]
        self.assertEqual(sizes['ram'], 256)
        self.assertEqual(sizes['flash'], 0)

    def test_fallback_table_knows_a_verified_vendor_flash_name(self):
        # no memory_layout in this fixture at all: this is the fallback table itself,
        # not the layout rule. '.flash_config' is imxrt's FlexSPI NOR boot-header
        # output section (hw/bsp/imxrt/boards/metro_m7_1011/metro_m7_1011.ld) - flash.
        syms = [{'name': 'cfg', 'size': 16, 'section': '.flash_config', 'source_file': 'x.c',
                'object_file': 'device/cdc_msc/CMakeFiles/cdc_msc.dir/co/src/x.c.obj'}]
        by_file = mc.per_file_sizes(fake_report(syms), ['/co/src/'])
        sizes = by_file[next(iter(by_file))]
        self.assertEqual(sizes['flash'], 16)
        self.assertEqual(sizes['ram'], 0)

    def test_fallback_table_knows_a_verified_vendor_ram_name(self):
        # no memory_layout in this fixture at all: this is the fallback table itself,
        # not the layout rule. 'm_usb_global' is LPC55's USB-controller SRAM output
        # section (hw/mcu/nxp/mcux-devices-lpc/LPC5500/LPC55S28/gcc/LPC55S28_flash.ld,
        # placed NOLOAD into the m_usb_sram MEMORY region) - ram.
        syms = [{'name': 'usb_buf', 'size': 32, 'section': 'm_usb_global', 'source_file': 'x.c',
                'object_file': 'device/cdc_msc/CMakeFiles/cdc_msc.dir/co/src/x.c.obj'}]
        by_file = mc.per_file_sizes(fake_report(syms), ['/co/src/'])
        sizes = by_file[next(iter(by_file))]
        self.assertEqual(sizes['ram'], 32)
        self.assertEqual(sizes['flash'], 0)

    def test_fallback_table_knows_ccmram_counts_both(self):
        # no memory_layout: .ccmram moved from unlisted (old catch-all: flash-only,
        # wrong) to BOTH_SECTIONS, verified against
        # hw/bsp/stm32f4/boards/stm32f407disco/STM32F407VGTx_FLASH.ld, which loads it
        # `AT> FLASH` exactly like .data - a flash-resident load image plus its
        # CCMRAM run address.
        syms = [{'name': 'buf', 'size': 20, 'section': '.ccmram', 'source_file': 'x.c',
                'object_file': 'device/cdc_msc/CMakeFiles/cdc_msc.dir/co/src/x.c.obj'}]
        by_file = mc.per_file_sizes(fake_report(syms), ['/co/src/'])
        sizes = by_file[next(iter(by_file))]
        self.assertEqual(sizes['flash'], 20)
        self.assertEqual(sizes['ram'], 20)

    def test_unlisted_section_in_flash_region_lands_in_flash_via_layout(self):
        # a section name FLASH_SECTIONS does not enumerate, but the layout still
        # correctly places it: region name 'FLASH' is recognized directly.
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
        # base and current checkouts have different absolute prefixes (/co vs /co2);
        # both must relativize to the same key so compare_reports can match them up.
        base = mc.per_file_sizes(fake_report(SYMS_BASE), ['/co/src/'])
        cur = mc.per_file_sizes(fake_report(SYMS_CUR), ['/co2/src/'])
        self.assertEqual(set(base), set(cur))

    def test_falls_back_to_source_file_when_object_file_missing(self):
        # archive-linked symbols (libc/libgcc) carry no object_file; membrowse puts
        # the path info in `archive` instead, which per_file_sizes() doesn't read -
        # falling back to source_file is a deliberate no-op for those (basename-only
        # can't match an absolute-path filter), not a crash.
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
