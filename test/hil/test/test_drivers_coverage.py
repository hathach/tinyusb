#!/usr/bin/env python3
"""Tests for tools/drivers_coverage_check.py and the real boards/roster files."""
import json
import os
import re
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

import yaml

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), '../../..'))
CHECKER = os.path.join(REPO, 'tools', 'drivers_coverage_check.py')
BOARDS_JSON = os.path.join(REPO, '.github', 'ci-pinned-boards.json')

sys.path.insert(0, os.path.dirname(CHECKER))
import drivers_coverage_check as dcc  # noqa: E402


def run_checker(json_path, *extra):
    return subprocess.run([sys.executable, CHECKER, json_path, *extra],
                         capture_output=True, text=True)


def write_json(tc, data):
    return write_text(tc, json.dumps(data))


def write_text(tc, text):
    tmp = tempfile.NamedTemporaryFile('w', suffix='.json', delete=False)
    tmp.write(text)
    tmp.close()
    tc.addCleanup(os.unlink, tmp.name)
    return tmp.name


class DriverScan(unittest.TestCase):
    def test_finds_known_drivers(self):
        drivers = dcc.list_driver_paths(os.path.join(REPO, 'src', 'portable'))
        for d in ('dcd_dwc2', 'hcd_dwc2', 'dcd_rp2040', 'ehci', 'ohci',
                  'dcd_stm32_fsdev', 'hcd_max3421'):
            self.assertIn(d, drivers)

    def test_excludes_template(self):
        drivers = dcc.list_driver_paths(os.path.join(REPO, 'src', 'portable'))
        self.assertNotIn('dcd_template', drivers)
        self.assertNotIn('hcd_template', drivers)


class RowDrivers(unittest.TestCase):
    def _row(self, roles, *drivers):
        return {'roles': roles,
                'portable': [f'x/{d}.c' for d in drivers] + ['x/common.c']}

    def test_dcd_counts_with_device_or_dual(self):
        for roles in (['device'], ['dual']):
            self.assertEqual(dcc.row_drivers(self._row(roles, 'dcd_dwc2')), {'dcd_dwc2'})
        self.assertEqual(dcc.row_drivers(self._row(['host'], 'dcd_dwc2')), set())

    def test_host_drivers_count_with_host_or_dual(self):
        for roles in (['host'], ['dual']):
            self.assertEqual(dcc.row_drivers(self._row(roles, 'hcd_dwc2', 'ehci', 'ohci')),
                             {'hcd_dwc2', 'ehci', 'ohci'})

    def test_host_driver_on_a_device_only_board_is_not_counted(self):
        self.assertEqual(dcc.row_drivers(self._row(['device'], 'dcd_musb', 'hcd_musb')),
                         {'dcd_musb'})


class CheckerVerdicts(unittest.TestCase):
    """Fixture catalog: two real stm32f4 boards with different rows. Every
    driver the fixture boards do not cover is waived, so a test only has to
    say what it pins and which waiver it drops or adds."""

    def setUp(self):
        self.catalog = {'stm32f4': {
            'stm32f407disco': {'cmake': {'roles': ['device', 'host'], 'portable': [
                'synopsys/dwc2/dcd_dwc2.c', 'synopsys/dwc2/hcd_dwc2.c']}},
            'stm32f411disco': {'cmake': {'roles': ['device'], 'portable': [
                'synopsys/dwc2/dcd_dwc2.c', 'synopsys/dwc2/hcd_dwc2.c', 'ehci/ehci.c',
                'ohci/ohci.c', 'st/stm32_fsdev/dcd_stm32_fsdev.c']}},
            'stm32f412disco': None,
            'stm32f439nucleo': {'cmake': None},
        }, 'pic32mz': {
            'olimex_emz64': {'cmake': {'roles': ['device'], 'portable': [
                'microchip/pic32mz/dcd_pic32mz.c']}},
        }}
        drivers = dcc.list_driver_paths(os.path.join(REPO, 'src', 'portable'))
        self.waivers = {d: 'fixture' for d in drivers
                        if d not in ('dcd_dwc2', 'hcd_dwc2')}

    def _run(self, boards, uncovered=None):
        pinned = {'boards': [{'board': b} if isinstance(b, str) else b for b in boards],
                  'uncovered': self.waivers if uncovered is None else uncovered}
        return run_checker(write_json(self, pinned), write_json(self, self.catalog))

    def test_pinned_board_covers_its_role_filtered_drivers(self):
        r = self._run(['stm32f407disco'])
        self.assertEqual(r.returncode, 0, r.stderr)

    def test_board_does_not_inherit_another_boards_sources(self):
        # same family, but only stm32f411disco's row compiles dcd_stm32_fsdev
        del self.waivers['dcd_stm32_fsdev']
        r = self._run(['stm32f407disco'])
        self.assertEqual(r.returncode, 1)
        self.assertIn('dcd_stm32_fsdev has no CI board and no uncovered entry', r.stderr)

    def test_host_sources_on_a_device_only_board_are_not_coverage(self):
        # stm32f411disco compiles hcd_dwc2/ehci/ohci but is device-only
        del self.waivers['ehci'], self.waivers['ohci']
        r = self._run(['stm32f411disco'])
        self.assertEqual(r.returncode, 1)
        for d in ('hcd_dwc2', 'ehci', 'ohci'):
            self.assertIn(f'{d} has no CI board and no uncovered entry', r.stderr)

    def test_family_no_ci_toolchain_builds_fails(self):
        r = self._run(['stm32f407disco', 'olimex_emz64'])
        self.assertEqual(r.returncode, 1)
        self.assertIn('olimex_emz64): family "pic32mz" is pinned but built by no CI toolchain',
                      r.stderr)

    def test_missing_catalog_row_fails(self):
        del self.catalog['stm32f4']['stm32f411disco']
        r = self._run(['stm32f407disco', 'stm32f411disco'])
        self.assertEqual(r.returncode, 1)
        self.assertIn('stm32f411disco): no row in hw/bsp/family.json', r.stderr)

    def test_null_cmake_row_fails(self):
        for board in ('stm32f412disco', 'stm32f439nucleo'):
            r = self._run(['stm32f407disco', board])
            self.assertEqual(r.returncode, 1)
            self.assertIn(f'{board}): hw/bsp/family.json row has no cmake configure', r.stderr)

    def test_duplicate_board_fails(self):
        r = self._run(['stm32f407disco', 'stm32f407disco'])
        self.assertEqual(r.returncode, 1)
        self.assertIn('duplicate entry', r.stderr)

    def test_unknown_board_fails(self):
        r = self._run(['stm32f407disco', 'no_such_board'])
        self.assertEqual(r.returncode, 1)
        self.assertIn('no_such_board', r.stderr)

    def test_entry_missing_board_fails(self):
        r = self._run(['stm32f407disco', {'note': 'x'}])
        self.assertEqual(r.returncode, 1)
        self.assertIn('missing "board"', r.stderr)

    def test_non_object_entry_fails_with_one_clear_error(self):
        r = self._run(['stm32f407disco', ['not_an_object']])
        self.assertEqual(r.returncode, 1)
        self.assertIn('must be an object', r.stderr)
        self.assertNotIn('Traceback', r.stderr)

    def test_non_string_board_fails_with_one_clear_error(self):
        for value, kind in ((['stm32f407disco'], 'list'),
                            ({'name': 'stm32f407disco'}, 'dict'), (42, 'int')):
            with self.subTest(kind):
                r = self._run(['stm32f407disco', {'board': value}])
                self.assertEqual(r.returncode, 1)
                self.assertNotIn('Traceback', r.stderr)
                self.assertEqual(r.stderr.splitlines(),
                                 [f'boards[1]: "board" must be a string, not {kind}'])

    def test_compound_cases_keep_their_exact_error_list(self):
        # pinned against extraction: an unbuilt family is NOT an early exit (a valid
        # row still enters coverage), a missing row adds its own line after it, and a
        # duplicate is judged against what already entered coverage
        self.catalog['pic32mz'] = {'olimex_emz64': {'cmake': {
            'roles': ['device'], 'portable': ['microchip/pic32mz/dcd_pic32mz.c']}}}
        del self.waivers['dcd_pic32mz']
        r = self._run(['stm32f407disco', 'olimex_emz64'])
        self.assertEqual(r.returncode, 1)
        self.assertEqual(r.stderr.splitlines(), [
            'boards[1] (olimex_emz64): family "pic32mz" is pinned but built by no CI '
            'toolchain (not in ci_set_matrix.family_list), so it covers nothing'])

    def test_a_board_covering_nothing_still_counts_as_entered(self):
        # empty roles/portable is valid coverage: the second entry is a duplicate,
        # not a fresh board, so `if drivers:` would be the wrong test
        self.catalog['stm32f4']['stm32f411disco'] = {'cmake': {'roles': [], 'portable': []}}
        r = self._run(['stm32f411disco', 'stm32f411disco'])
        self.assertIn('boards[1] (stm32f411disco): duplicate entry', r.stderr)

    def test_a_board_that_never_enters_coverage_is_never_a_duplicate(self):
        # the mirror of the case above: a board whose row yields no coverage never
        # enters it, so each occurrence reports its own error and none is a duplicate
        for name, board, expect in (
                ('invalid row', 'stm32f439nucleo',
                 'hw/bsp/family.json row has no cmake configure'),
                ('unknown board', 'no_such_board',
                 'unknown board (no hw/bsp/*/boards/no_such_board)')):
            with self.subTest(name):
                r = self._run(['stm32f407disco', board, board])
                self.assertEqual(r.returncode, 1)
                self.assertEqual(r.stderr.splitlines(),
                                 [f'boards[1] ({board}): {expect}',
                                  f'boards[2] ({board}): {expect}'])

    def test_malformed_catalog_rows_fail_with_one_clear_error(self):
        # hw/bsp/family.json is validated by its own hook, but nothing sets fail_fast:
        # a hand-corrupted catalog must reach this hook as a line, not a traceback
        row = self.catalog['stm32f4']['stm32f407disco']
        for name, family_rows, expect in (
                ('family value is a list', ['stm32f407disco'], 'no row in hw/bsp/family.json'),
                ('board entry is a list', {'stm32f407disco': ['x']}, 'no cmake configure'),
                ('cmake is a list', {'stm32f407disco': {'cmake': ['x']}}, 'no cmake configure'),
                ('no roles', {'stm32f407disco': {'cmake': {'portable': ['a.c']}}},
                 'needs string lists'),
                ('roles of lists',
                 {'stm32f407disco': {'cmake': {'roles': [['device']], 'portable': ['a.c']}}},
                 'needs string lists'),
                ('portable of ints',
                 {'stm32f407disco': {'cmake': {'roles': ['device'], 'portable': [1]}}},
                 'needs string lists')):
            with self.subTest(name):
                self.catalog['stm32f4'] = family_rows
                r = self._run(['stm32f407disco'])
                self.assertEqual(r.returncode, 1)
                self.assertNotIn('Traceback', r.stderr)
                self.assertIn(expect, r.stderr)
        self.catalog['stm32f4'] = {'stm32f407disco': row}

    def test_malformed_roster_json_fails_with_one_clear_error(self):
        r = run_checker(write_text(self, '{ bad json'), write_json(self, self.catalog))
        self.assertEqual(r.returncode, 1)
        self.assertNotIn('Traceback', r.stderr)
        self.assertEqual(len(r.stderr.strip().splitlines()), 1)

    def test_non_object_roster_fails_with_one_clear_error(self):
        r = run_checker(write_text(self, '[]'), write_json(self, self.catalog))
        self.assertEqual(r.returncode, 1)
        self.assertNotIn('Traceback', r.stderr)
        self.assertIn('top level must be an object', r.stderr)

    def test_waiver_overlapping_coverage_is_not_an_error(self):
        r = self._run(['stm32f407disco'], dict(self.waivers, dcd_dwc2='empty build'))
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertIn('membrowse: dcd_dwc2 uncovered - empty build '
                      '(suppresses: stm32f407disco)', r.stdout)
        self.assertIn('membrowse: dcd_nrf5x uncovered - fixture (no candidates)', r.stdout)

    def test_waiver_naming_no_driver_fails(self):
        r = self._run(['stm32f407disco'], dict(self.waivers, dcd_nonexistent='x'))
        self.assertEqual(r.returncode, 1)
        self.assertIn('"dcd_nonexistent" matches no driver source file', r.stderr)

    def test_blank_waiver_reason_fails(self):
        r = self._run(['stm32f407disco'], dict(self.waivers, dcd_nrf5x=' '))
        self.assertEqual(r.returncode, 1)
        self.assertIn('uncovered "dcd_nrf5x": reason must be a non-empty string', r.stderr)


class RealFiles(unittest.TestCase):
    def test_real_file_passes(self):
        r = run_checker(BOARDS_JSON)
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertEqual(r.stderr, '')
        self.assertRegex(r.stdout, r'membrowse: dcd_pio_usb uncovered - .*'
                                   r'\(suppresses: .*raspberry_pi_pico\b')
        self.assertRegex(r.stdout, r'membrowse: hcd_lpc_ip3516 uncovered - .*\(no candidates\)')

    def test_real_file_passes_from_a_different_cwd(self):
        r = subprocess.run([sys.executable, CHECKER, BOARDS_JSON],
                           capture_output=True, text=True, cwd=tempfile.gettempdir())
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertEqual(r.stderr, '')


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
        with mock.patch.object(dcc, '_hil_roster_boards', lambda repo_root: boards):
            return {msg for _, msg in dcc.hil_gaps()}

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
                     'hw/bsp/family.json',
                     'examples/CMakeLists.txt',
                     'examples/device/cdc_msc/skip.txt'):
            self.assertRegex(path, re.compile(pattern))


if __name__ == '__main__':
    unittest.main()
