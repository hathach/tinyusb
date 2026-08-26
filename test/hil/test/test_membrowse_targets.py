#!/usr/bin/env python3
"""Tests for .github/scripts/membrowse_targets_check.py and the real targets file."""
import json
import os
import subprocess
import sys
import tempfile
import unittest

REPO = subprocess.run(['git', 'rev-parse', '--show-toplevel'],
                      capture_output=True, text=True, check=True).stdout.strip()
CHECKER = os.path.join(REPO, '.github', 'scripts', 'membrowse_targets_check.py')
TARGETS_JSON = os.path.join(REPO, '.github', 'membrowse-targets.json')

sys.path.insert(0, os.path.dirname(CHECKER))
import membrowse_targets_check as mtc  # noqa: E402


def run_checker(json_path):
    return subprocess.run([sys.executable, CHECKER, json_path],
                         capture_output=True, text=True)


class DriverScan(unittest.TestCase):
    def test_finds_known_drivers(self):
        drivers = mtc.list_drivers(os.path.join(REPO, 'src', 'portable'))
        for d in ('dcd_dwc2', 'hcd_dwc2', 'dcd_rp2040', 'ehci', 'ohci',
                  'dcd_stm32_fsdev', 'hcd_max3421'):
            self.assertIn(d, drivers)

    def test_excludes_template(self):
        drivers = mtc.list_drivers(os.path.join(REPO, 'src', 'portable'))
        self.assertNotIn('dcd_template', drivers)
        self.assertNotIn('hcd_template', drivers)


class CheckerVerdicts(unittest.TestCase):
    def test_real_file_passes(self):
        r = run_checker(TARGETS_JSON)
        self.assertEqual(r.returncode, 0, r.stderr)

    def _mutated(self, mutate):
        with open(TARGETS_JSON) as f:
            data = json.load(f)
        mutate(data)
        tmp = tempfile.NamedTemporaryFile('w', suffix='.json', delete=False)
        json.dump(data, tmp)
        tmp.close()
        self.addCleanup(os.unlink, tmp.name)
        return run_checker(tmp.name)

    def test_missing_driver_fails(self):
        # drop every entry covering dcd_rp2040 and don't add it to uncovered
        def mutate(d):
            for t in d['targets']:
                t['drivers'] = [x for x in t['drivers'] if x != 'dcd_rp2040']
        r = self._mutated(mutate)
        self.assertEqual(r.returncode, 1)
        self.assertIn('dcd_rp2040', r.stderr)

    def test_unknown_driver_name_fails(self):
        r = self._mutated(lambda d: d['targets'][0]['drivers'].append('dcd_nonexistent'))
        self.assertEqual(r.returncode, 1)
        self.assertIn('dcd_nonexistent', r.stderr)

    def test_unknown_board_fails(self):
        r = self._mutated(lambda d: d['targets'][0].update(board='no_such_board'))
        self.assertEqual(r.returncode, 1)
        self.assertIn('no_such_board', r.stderr)

    def test_driver_in_both_lists_fails(self):
        def mutate(d):
            drv = d['targets'][0]['drivers'][0]
            d['uncovered'][drv] = 'also uncovered'
        r = self._mutated(mutate)
        self.assertEqual(r.returncode, 1)

    def test_empty_uncovered_reason_fails(self):
        def mutate(d):
            for t in d['targets']:
                t['drivers'] = [x for x in t['drivers'] if x != 'dcd_rp2040']
            d['uncovered']['dcd_rp2040'] = ''
        r = self._mutated(mutate)
        self.assertEqual(r.returncode, 1)

    def test_hcd_claim_on_a_host_less_board_fails(self):
        # msp_exp430f5529lp only pins a dcd_* driver; host examples are only.txt
        # opt-in and this board builds none, so claiming an hcd_* driver on it is a
        # false claim the checker must catch.
        def mutate(d):
            for t in d['targets']:
                if t['board'] == 'msp_exp430f5529lp':
                    t['drivers'].append('hcd_max3421')
        r = self._mutated(mutate)
        self.assertEqual(r.returncode, 1)
        self.assertIn('msp_exp430f5529lp', r.stderr)
        self.assertIn('host/ or', r.stderr)


if __name__ == '__main__':
    unittest.main()
