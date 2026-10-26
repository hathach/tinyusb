#!/usr/bin/env python3
"""Tests for build.py --ci-pinned-boards resolution against the real boards file."""
import os
import sys
import unittest
from unittest import mock

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), '../../..'))
sys.path.insert(0, os.path.join(REPO, 'tools'))
import build  # noqa: E402

CI_BOARDS = os.path.join(REPO, '.github', 'ci-pinned-boards.json')


def setUpModule():
    unittest.addModuleCleanup(os.chdir, os.getcwd())
    os.chdir(REPO)  # build.py resolves hw/bsp relative to cwd


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
        # examples/device/cdc_dual_ports/skip.txt skips stm32f407disco, stm32f4's CI board
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

    def _captured_build_examples(self, argv):
        """main()'s examples arg to build_boards_list(), without doing a real build."""
        captured = {}

        def fake(boards, build_defines, build_system, build_name, build_cflags, build_targets, examples=None):
            captured['examples'] = examples
            return [0, 0, 0]

        with mock.patch.object(build, 'build_boards_list', fake), \
             mock.patch.object(sys, 'argv', ['build.py'] + argv):
            build.main()
        return captured['examples']

    def test_ci_pinned_boards_only_does_not_scope_the_actual_build(self):
        # the upload step's -e only picks the board; every example of it uploads
        examples = self._captured_build_examples([
            '--ci-pinned-boards', CI_BOARDS, '--ci-pinned-boards-only',
            '-e', 'device/cdc_msc', '--target', 'examples-membrowse-upload', 'stm32f4'])
        self.assertIsNone(examples)

    def test_examples_still_scope_the_build_without_boards_only(self):
        # the Build step (no --ci-pinned-boards-only) compiles only the PR-selected examples
        examples = self._captured_build_examples([
            '--ci-pinned-boards', CI_BOARDS,
            '-e', 'device/cdc_msc', '--target', 'all', 'stm32f4'])
        self.assertEqual(examples, ['device/cdc_msc'])


if __name__ == '__main__':
    unittest.main()
