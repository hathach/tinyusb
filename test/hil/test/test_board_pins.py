#!/usr/bin/env python3
"""Tests for build.py --board-pins resolution against the real targets file."""
import os
import subprocess
import sys
import unittest

REPO = subprocess.run(['git', 'rev-parse', '--show-toplevel'],
                      capture_output=True, text=True, check=True).stdout.strip()
sys.path.insert(0, os.path.join(REPO, 'tools'))
os.chdir(REPO)  # build.py resolves hw/bsp relative to cwd
import build  # noqa: E402

PINS = os.path.join(REPO, '.github', 'membrowse-targets.json')


class BoardPins(unittest.TestCase):
    def test_pinned_family_returns_pins(self):
        boards = build.resolve_pinned_boards(PINS, 'rp2040', pins_only=False, ci=True)
        self.assertIn('raspberry_pi_pico', boards)
        self.assertIn('feather_rp2040_max3421', boards)  # pin overrides ci_skip_boards

    def test_unpinned_family_falls_back_to_one_first(self):
        # stm32f0 has no pinned board; expect exactly the one-first pick
        expected = build.get_family_boards('stm32f0', one_random=False,
                                           one_first=True, ci=True)
        boards = build.resolve_pinned_boards(PINS, 'stm32f0', pins_only=False, ci=True)
        self.assertEqual(boards, expected)

    def test_pins_only_skips_unpinned_family(self):
        boards = build.resolve_pinned_boards(PINS, 'stm32f0', pins_only=True, ci=True)
        self.assertEqual(boards, [])

    def test_pins_only_keeps_pinned_family(self):
        boards = build.resolve_pinned_boards(PINS, 'stm32f4', pins_only=True, ci=True)
        self.assertEqual(boards, ['stm32f407disco'])


if __name__ == '__main__':
    unittest.main()
