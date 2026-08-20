#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Unit tests for hil_test's usbtest result accounting — no hardware. A case that never ran is
# not a pass and not a failure: it must stay in the denominator and out of the failed list, or
# a partial battery reads as a clean sweep. Run directly:
#   python3 test/hil/test/test_hil_usbtest_totals.py
import os
import sys
import unittest

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
import hil_test


class UsbtestTotals(unittest.TestCase):
    def test_full_pass(self):
        self.assertEqual((30, 0, 0, 0, 30),
                         hil_test.usbtest_totals({'passed': 30, 'failed': 0}))

    def test_quirk_skipped_cases_stay_in_the_denominator(self):
        """25 ran and passed, 5 the device advertises as unsupported: 25/30, never 25/25."""
        p, f, n, s, total = hil_test.usbtest_totals(
            {'passed': 25, 'failed': 0, 'skipped': 5})
        self.assertEqual((25, 0, 0, 5), (p, f, n, s))
        self.assertEqual(30, total)

    def test_notrun_and_skipped_both_count(self):
        _, _, n, s, total = hil_test.usbtest_totals(
            {'passed': 20, 'failed': 1, 'notrun': 4, 'skipped': 5})
        self.assertEqual((4, 5), (n, s))
        self.assertEqual(30, total)

    def test_missing_keys_default_to_zero(self):
        """Output from a usbtest.py predating either key must still add up."""
        self.assertEqual((30, 0, 0, 0, 30),
                         hil_test.usbtest_totals({'passed': 30, 'failed': 0, 'cases': []}))


class UsbtestBadCases(unittest.TestCase):
    def test_only_real_failures_are_listed(self):
        data = {'cases': [{'num': n, 'status': 'SKIP'} for n in (9, 10, 13)]
                         + [{'num': 4, 'status': 'BUDGET'}]
                         + [{'num': 7, 'status': 'FAIL'}]
                         + [{'num': 1, 'status': 'PASS'}]}
        self.assertEqual([7], hil_test.usbtest_bad_cases(data))

    def test_no_cases_key(self):
        self.assertEqual([], hil_test.usbtest_bad_cases({}))


if __name__ == '__main__':
    unittest.main(verbosity=1)
