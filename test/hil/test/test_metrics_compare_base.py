#!/usr/bin/env python3
"""Unit tests for tools/metrics_compare_base.py's generate_membrowse_sizes() guard.

Exercises the "elfs found but no symbols matched filters" failure mode with
glob.glob and membrowse_compare.report_for_elf monkeypatched, so no build and
no real membrowse CLI invocation is needed.
"""
import contextlib
import io
import os
import subprocess
import sys
import unittest
from unittest import mock

REPO = subprocess.run(['git', 'rev-parse', '--show-toplevel'],
                      capture_output=True, text=True, check=True).stdout.strip()
sys.path.insert(0, os.path.join(REPO, 'tools'))
import membrowse_compare  # noqa: E402
import metrics_compare_base as mcb  # noqa: E402


def _run_capturing_stdout(*args, **kwargs):
    buf = io.StringIO()
    with contextlib.redirect_stdout(buf):
        result = mcb.generate_membrowse_sizes(*args, **kwargs)
    return result, buf.getvalue()


class GenerateMembrowseSizes(unittest.TestCase):
    def test_no_elfs_errors_and_returns_none(self):
        with mock.patch('glob.glob', return_value=[]):
            result, out = _run_capturing_stdout('/fake/build', ['/fake/src/'])
        self.assertIsNone(result)
        self.assertIn('no .elf files', out)

    def test_elfs_found_but_nothing_matches_filters_errors_and_returns_none(self):
        # The exact failure mode fix round 1 diagnosed: elfs exist, membrowse
        # reports real symbols, but none match `filters` (a filter typo, or a
        # future membrowse report-shape change breaking per_file_sizes()
        # matching again). Must not silently produce an empty/degenerate table.
        fake_report = {'symbols': [
            {'name': 'x', 'size': 4, 'section': '.text',
             'object_file': 'build/x.c.obj', 'source_file': 'x.c'},
        ]}
        with mock.patch('glob.glob', return_value=['/fake/build/ex/ex.elf']), \
             mock.patch.object(membrowse_compare, 'report_for_elf', return_value=fake_report):
            result, out = _run_capturing_stdout('/fake/build', ['/no/such/prefix/'])
        self.assertIsNone(result)
        self.assertIn('no symbols matched filters', out)
        self.assertIn('--engine linkermap', out)

    def test_elfs_found_and_filters_match_returns_sizes(self):
        fake_report = {'symbols': [
            {'name': 'x', 'size': 4, 'section': '.text',
             'object_file': 'build/x.c.obj', 'source_file': 'x.c'},
        ]}
        with mock.patch('glob.glob', return_value=['/fake/build/ex/ex.elf']), \
             mock.patch.object(membrowse_compare, 'report_for_elf', return_value=fake_report):
            result = mcb.generate_membrowse_sizes('/fake/build', ['build/'])
        self.assertIsNotNone(result)
        self.assertIn('x.c', result)
        self.assertEqual(result['x.c']['flash'], 4)


if __name__ == '__main__':
    unittest.main()
