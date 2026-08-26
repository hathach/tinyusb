#!/usr/bin/env python3
"""Tests for build.py --ci-boards resolution against the real boards file."""
import os
import subprocess
import sys
import unittest

REPO = subprocess.run(['git', 'rev-parse', '--show-toplevel'],
                      capture_output=True, text=True, check=True).stdout.strip()
sys.path.insert(0, os.path.join(REPO, 'tools'))
os.chdir(REPO)  # build.py resolves hw/bsp relative to cwd
import build  # noqa: E402

CI_BOARDS = os.path.join(REPO, '.github', 'ci-boards.json')


class CIBoards(unittest.TestCase):
    def test_ci_board_family_returns_boards(self):
        boards = build.resolve_ci_boards(CI_BOARDS, 'rp2040', boards_only=False)
        self.assertIn('raspberry_pi_pico', boards)
        self.assertIn('adafruit_fruit_jam', boards)  # explicit CI board, not the one-first pick

    def test_family_with_no_ci_board_falls_back_to_one_first(self):
        # stm32f0 has no CI board; expect exactly the one-first pick
        expected = build.get_family_boards('stm32f0', one_random=False, one_first=True)
        boards = build.resolve_ci_boards(CI_BOARDS, 'stm32f0', boards_only=False)
        self.assertEqual(boards, expected)

    def test_boards_only_skips_family_with_no_ci_board(self):
        boards = build.resolve_ci_boards(CI_BOARDS, 'stm32f0', boards_only=True)
        self.assertEqual(boards, [])

    def test_boards_only_keeps_family_with_a_ci_board(self):
        boards = build.resolve_ci_boards(CI_BOARDS, 'stm32f4', boards_only=True)
        self.assertEqual(boards, ['stm32f407disco'])

    def test_ci_board_that_cannot_build_the_filter_falls_back_to_one_first(self):
        # C1: stm32f4's CI board is stm32f407disco, and
        # examples/device/cdc_dual_ports/skip.txt skips that exact board - a PR
        # scoped to that example must not silently build nothing on the CI board,
        # it must fall back exactly like a family with no CI board would.
        examples = ['device/cdc_dual_ports']
        expected = build.get_family_boards('stm32f4', one_random=False, one_first=True,
                                           examples=examples)
        self.assertNotIn('stm32f407disco', expected, 'the fixture must actually be unbuildable')
        boards = build.resolve_ci_boards(CI_BOARDS, 'stm32f4', boards_only=False,
                                         examples=examples)
        self.assertEqual(boards, expected)

    def test_ci_board_that_cannot_build_the_filter_is_dropped_under_boards_only(self):
        # same fixture, but the upload step must not touch a board that was only
        # ever a compile smoke-check substitute for the CI board
        boards = build.resolve_ci_boards(CI_BOARDS, 'stm32f4', boards_only=True,
                                         examples=['device/cdc_dual_ports'])
        self.assertEqual(boards, [])


if __name__ == '__main__':
    unittest.main()
