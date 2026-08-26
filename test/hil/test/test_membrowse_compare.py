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
        syms = [{'name': 'd', 'size': 8, 'section': '.data', 'source_file': 'x.c',
                 'object_file': 'device/cdc_msc/CMakeFiles/cdc_msc.dir/co/src/x.c.obj'}]
        by_file = mc.per_file_sizes(fake_report(syms), ['/co/src/'])
        sizes = by_file[next(iter(by_file))]
        self.assertEqual(sizes['flash'], 8)
        self.assertEqual(sizes['ram'], 8)

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
