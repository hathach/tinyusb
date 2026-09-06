#!/usr/bin/env python3
"""Tests for tools/drivers_coverage_check.py and the real boards/roster files."""
import json
import os
import re
import subprocess
import sys
import tempfile
import unittest

import yaml

REPO = subprocess.run(['git', 'rev-parse', '--show-toplevel'],
                      capture_output=True, text=True, check=True).stdout.strip()
CHECKER = os.path.join(REPO, 'tools', 'drivers_coverage_check.py')
BOARDS_JSON = os.path.join(REPO, '.github', 'ci-pinned-boards.json')

sys.path.insert(0, os.path.dirname(CHECKER))
import drivers_coverage_check as dcc  # noqa: E402


def run_checker(json_path):
    return subprocess.run([sys.executable, CHECKER, json_path],
                         capture_output=True, text=True)


class DriverScan(unittest.TestCase):
    def test_finds_known_drivers(self):
        drivers = dcc.list_drivers(os.path.join(REPO, 'src', 'portable'))
        for d in ('dcd_dwc2', 'hcd_dwc2', 'dcd_rp2040', 'ehci', 'ohci',
                  'dcd_stm32_fsdev', 'hcd_max3421'):
            self.assertIn(d, drivers)

    def test_excludes_template(self):
        drivers = dcc.list_drivers(os.path.join(REPO, 'src', 'portable'))
        self.assertNotIn('dcd_template', drivers)
        self.assertNotIn('hcd_template', drivers)


class CheckerVerdicts(unittest.TestCase):
    def test_real_file_passes(self):
        r = run_checker(BOARDS_JSON)
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertEqual(r.stderr, '')

    def test_real_file_passes_from_a_different_cwd(self):
        # C2 regression: _builds_host_or_dual() chdir'd back to the caller's cwd
        # BEFORE running build_utils.skip_example() (cwd-relative, just like
        # build.get_examples()), so running the checker from outside the repo
        # made every host/dual driver claim look bogus - ~12 FATAL "claims [...]
        # but builds no host/ or dual/ example" errors and exit 1.
        r = subprocess.run([sys.executable, CHECKER, BOARDS_JSON],
                           capture_output=True, text=True, cwd=tempfile.gettempdir())
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertEqual(r.stderr, '')

    def _mutated(self, mutate):
        with open(BOARDS_JSON) as f:
            data = json.load(f)
        mutate(data)
        tmp = tempfile.NamedTemporaryFile('w', suffix='.json', delete=False)
        json.dump(data, tmp)
        tmp.close()
        self.addCleanup(os.unlink, tmp.name)
        return run_checker(tmp.name)

    def test_missing_driver_becomes_a_warning_not_a_failure(self):
        # drop every entry covering dcd_rp2040 and don't add it to uncovered: a
        # coverage gap, not a validity error - it must not fail the run
        def mutate(d):
            for t in d['boards']:
                t['drivers'] = [x for x in t['drivers'] if x != 'dcd_rp2040']
        r = self._mutated(mutate)
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertIn('WARNING: membrowse: dcd_rp2040 has no CI board', r.stdout)
        self.assertEqual(r.stderr, '')

    def test_unknown_driver_name_fails(self):
        r = self._mutated(lambda d: d['boards'][0]['drivers'].append('dcd_nonexistent'))
        self.assertEqual(r.returncode, 1)
        self.assertIn('dcd_nonexistent', r.stderr)

    def test_unknown_board_fails(self):
        r = self._mutated(lambda d: d['boards'][0].update(board='no_such_board'))
        self.assertEqual(r.returncode, 1)
        self.assertIn('no_such_board', r.stderr)

    def test_board_claiming_an_unrelated_driver_fails(self):
        def mutate(d):
            for t in d['boards']:
                if t['board'] == 'stm32f407disco':
                    t['drivers'][0] = 'dcd_nrf5x'
        r = self._mutated(mutate)
        self.assertEqual(r.returncode, 1)
        self.assertIn('stm32f407disco', r.stderr)
        self.assertIn('does not build', r.stderr)

    def test_driver_in_both_lists_fails(self):
        def mutate(d):
            drv = d['boards'][0]['drivers'][0]
            d['uncovered'][drv] = 'also uncovered'
        r = self._mutated(mutate)
        self.assertEqual(r.returncode, 1)

    def test_empty_uncovered_reason_fails(self):
        # a present-but-empty "uncovered" reason is a validity error (malformed
        # entry), not a coverage gap - it must still be fatal
        def mutate(d):
            for t in d['boards']:
                t['drivers'] = [x for x in t['drivers'] if x != 'dcd_rp2040']
            d['uncovered']['dcd_rp2040'] = ''
        r = self._mutated(mutate)
        self.assertEqual(r.returncode, 1)

    def test_hcd_claim_on_a_host_less_board_fails(self):
        # msp_exp430f5529lp only covers a dcd_* driver; host examples are only.txt
        # opt-in and this board builds none, so claiming an hcd_* driver on it is a
        # false claim the checker must catch.
        def mutate(d):
            for t in d['boards']:
                if t['board'] == 'msp_exp430f5529lp':
                    t['drivers'].append('hcd_max3421')
        r = self._mutated(mutate)
        self.assertEqual(r.returncode, 1)
        self.assertIn('msp_exp430f5529lp', r.stderr)
        self.assertIn('host/ or', r.stderr)

    def test_boards_entry_as_a_string_fails_with_one_clear_error(self):
        # a bare string here would otherwise TypeError on t.get() ("'str' object
        # has no attribute 'get'") instead of one clear validity error
        r = self._mutated(lambda d: d['boards'].append('not_an_object'))
        self.assertEqual(r.returncode, 1)
        self.assertIn('must be an object', r.stderr)
        self.assertNotIn('Traceback', r.stderr)

    def test_drivers_as_a_string_fails_with_one_clear_error(self):
        # a string here would otherwise iterate character-by-character below
        # ("d", "c", "d", "_", ... each "matches no driver source file") instead
        # of one clear validity error
        r = self._mutated(lambda d: d['boards'][0].update(drivers='dcd_rp2040'))
        self.assertEqual(r.returncode, 1)
        self.assertIn('"drivers" must be a list', r.stderr)
        self.assertNotIn('matches no driver source file', r.stderr)

    def test_drivers_as_null_fails_instead_of_crashing(self):
        # `for d in None` is an unhandled TypeError, not a validity error
        r = self._mutated(lambda d: d['boards'][0].update(drivers=None))
        self.assertEqual(r.returncode, 1)
        self.assertIn('"drivers" must be a list', r.stderr)
        self.assertNotIn('Traceback', r.stderr)


class MembrowseGaps(unittest.TestCase):
    def test_documented_gap_is_info(self):
        # the real file's "uncovered" entries are exactly this case
        pairs = dcc.membrowse_gaps(BOARDS_JSON)
        self.assertIn(('INFO', 'membrowse: dcd_pic uncovered - pic32mx has no BSP '
                               'family in hw/bsp; XC toolchain not in CI'), pairs)

    def test_undocumented_gap_is_warning(self):
        with open(BOARDS_JSON) as f:
            data = json.load(f)
        for t in data['boards']:
            t['drivers'] = [x for x in t['drivers'] if x != 'dcd_rp2040']
        tmp = tempfile.NamedTemporaryFile('w', suffix='.json', delete=False)
        json.dump(data, tmp)
        tmp.close()
        self.addCleanup(os.unlink, tmp.name)
        pairs = dcc.membrowse_gaps(tmp.name)
        self.assertIn(('WARNING', 'membrowse: dcd_rp2040 has no CI board '
                                  '(and no uncovered entry)'), pairs)


class HilCoverage(unittest.TestCase):
    """Spot checks for the ci_select rule-3/4 reuse: family membership
    (dcd_dwc2, dcd_rp2040) and no rig board at all (dcd_ft9xx)."""

    def test_dwc2_is_covered_by_stm32f4(self):
        gaps = {msg for _, msg in dcc.hil_gaps()}
        self.assertNotIn('hil: dcd_dwc2 has no board on the rig', gaps)

    def test_rp2040_is_covered(self):
        gaps = {msg for _, msg in dcc.hil_gaps()}
        self.assertNotIn('hil: dcd_rp2040 has no board on the rig', gaps)

    def test_ft9xx_has_no_rig_board(self):
        gaps = {msg for _, msg in dcc.hil_gaps()}
        self.assertIn('hil: dcd_ft9xx has no board on the rig', gaps)

    def test_real_run_reports_the_hil_gap_line(self):
        r = run_checker(BOARDS_JSON)
        self.assertIn('INFO: hil: dcd_ft9xx has no board on the rig', r.stdout)


class HilRoleFiltering(unittest.TestCase):
    """Family membership alone isn't coverage: a dcd_* driver needs a
    device-role board, hcd_*/ehci/ohci a host-role one - the board_roles()
    filter rule 3/4 applies. ra6m5_ek is a real 'ra' family board (hcd_rusb2's
    port family), so only its roster-supplied role changes between cases."""

    def _hil_gap_messages(self, boards):
        orig = dcc.build_utils.hil_roster_boards
        dcc.build_utils.hil_roster_boards = lambda repo_root: boards
        try:
            return {msg for _, msg in dcc.hil_gaps()}
        finally:
            dcc.build_utils.hil_roster_boards = orig

    def test_wrong_role_board_is_still_a_gap(self):
        # device-only: hcd_rusb2 (host-only) must not count this as coverage
        board = {'name': 'ra6m5_ek', 'tests': {'device': True, 'host': False, 'dual': False}}
        gaps = self._hil_gap_messages([board])
        self.assertIn('hil: hcd_rusb2 has no board on the rig', gaps)

    def test_matching_role_board_covers_it(self):
        board = {'name': 'ra6m5_ek', 'tests': {'device': False, 'host': True, 'dual': False}}
        gaps = self._hil_gap_messages([board])
        self.assertNotIn('hil: hcd_rusb2 has no board on the rig', gaps)


class BoardsAreSorted(unittest.TestCase):
    def test_boards_sorted_by_board(self):
        with open(BOARDS_JSON) as f:
            data = json.load(f)
        keys = [t['board'] for t in data['boards']]
        self.assertEqual(keys, sorted(keys), 'boards must be sorted by board name')


class HookScope(unittest.TestCase):
    def test_every_checker_input_triggers_the_hook(self):
        with open(os.path.join(REPO, '.pre-commit-config.yaml')) as f:
            hooks = [h for repo in yaml.safe_load(f)['repos'] for h in repo['hooks']]
        pattern = next(h['files'] for h in hooks if h['id'] == 'drivers-coverage')
        for path in ('.github/scripts/ci_set_matrix.py',
                     'examples/CMakeLists.txt',
                     'examples/device/cdc_msc/skip.txt'):
            self.assertRegex(path, re.compile(pattern))


if __name__ == '__main__':
    unittest.main()
