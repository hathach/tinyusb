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

    def test_membrowse_cli_missing_errors_and_returns_none(self):
        # subprocess.run(['membrowse', ...]) raises FileNotFoundError when the
        # CLI isn't installed - must not surface as a bare traceback after the
        # base+branch builds already ran (minutes of work).
        with mock.patch('glob.glob', return_value=['/fake/build/ex/ex.elf']), \
             mock.patch.object(membrowse_compare, 'report_for_elf',
                                side_effect=FileNotFoundError('membrowse')):
            result, out = _run_capturing_stdout('/fake/build', ['/fake/src/'])
        self.assertIsNone(result)
        self.assertIn('pip install membrowse', out)
        self.assertIn('--engine linkermap', out)

    def test_membrowse_report_failure_errors_and_returns_none(self):
        # report_for_elf() raises RuntimeError when `membrowse report` itself
        # exits non-zero - must not surface as a bare traceback after the
        # base+branch builds already ran (minutes of work).
        with mock.patch('glob.glob', return_value=['/fake/build/ex/ex.elf']), \
             mock.patch.object(membrowse_compare, 'report_for_elf',
                                side_effect=RuntimeError('membrowse report failed for /fake/build/ex/ex.elf: boom')):
            result, out = _run_capturing_stdout('/fake/build', ['/fake/src/'])
        self.assertIsNone(result)
        self.assertIn('membrowse report failed', out)

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

    def test_all_examples_scope_averages_a_file_shared_across_elfs(self):
        # No `example` arg (the -b-with-no-e, all-examples scope): a file linked
        # by more than one elf must be AVERAGED across them, matching
        # metrics.py's compute_avg() semantics from the legacy linkermap engine
        # this replaces - not summed, or a file linked by N examples would
        # report ~N times its real size and Flash/RAM wouldn't be a real
        # binary's size any more.
        report_a = {'symbols': [
            {'name': 'x', 'size': 100, 'section': '.text',
             'object_file': 'build/x.c.obj', 'source_file': 'x.c'},
        ]}
        report_b = {'symbols': [
            {'name': 'x', 'size': 200, 'section': '.text',
             'object_file': 'build/x.c.obj', 'source_file': 'x.c'},
        ]}
        with mock.patch('glob.glob', return_value=['/fake/build/ex1/ex1.elf',
                                                    '/fake/build/ex2/ex2.elf']), \
             mock.patch.object(membrowse_compare, 'report_for_elf',
                                side_effect=[report_a, report_b]):
            result = mcb.generate_membrowse_sizes('/fake/build', ['build/'])
        self.assertIsNotNone(result)
        self.assertEqual(result['x.c']['flash'], 150)  # (100+200)/2, not 300

    def test_single_example_scope_sums_across_its_own_elfs(self):
        # `example` given (-e): must stay byte-identical to before averaging was
        # added - an example with more than one of its own elf (e.g. app +
        # bootloader) sums, it does not average.
        report_a = {'symbols': [
            {'name': 'x', 'size': 100, 'section': '.text',
             'object_file': 'build/x.c.obj', 'source_file': 'x.c'},
        ]}
        report_b = {'symbols': [
            {'name': 'x', 'size': 200, 'section': '.text',
             'object_file': 'build/x.c.obj', 'source_file': 'x.c'},
        ]}
        with mock.patch('glob.glob', return_value=['/fake/build/ex/app.elf',
                                                    '/fake/build/ex/loader.elf']), \
             mock.patch.object(membrowse_compare, 'report_for_elf',
                                side_effect=[report_a, report_b]):
            result = mcb.generate_membrowse_sizes('/fake/build', ['build/'], example='ex')
        self.assertIsNotNone(result)
        self.assertEqual(result['x.c']['flash'], 300)  # sum, not average


if __name__ == '__main__':
    unittest.main()
