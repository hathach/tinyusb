#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""hil_test.py's command line lives in helper/hil_args.py so the remote wrapper parses
exactly what the rig will; these pin the forms both sides rely on."""
import ast
import contextlib
import io
import os
import sys
import unittest
from pathlib import Path

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from helper import hil_args

HIL_DIR = Path(__file__).resolve().parents[1]


def parse(*argv):
    return hil_args.build_parser().parse_args([*argv, 'cfg.json'])


class BuildParser(unittest.TestCase):
    def test_defaults(self):
        a = parse()
        self.assertEqual((a.config_file, a.board, a.board_test, a.build_dir, a.retry),
                         ('cfg.json', [], [], 'cmake-build', 1))
        self.assertFalse(a.accumulate or a.build or a.verbose or a.skip_flash)

    def test_every_board_spelling(self):
        for argv in (['-b', 'x'], ['--board', 'x'], ['--board=x'], ['-bx']):
            self.assertEqual(parse(*argv).board, ['x'], argv)
        self.assertEqual(parse('-b', 'x', '-by').board, ['x', 'y'])

    def test_board_test_forms(self):
        for argv in (['-bt', 'x:t'], ['-bt=x:t'], ['--board-test', 'x:t'], ['--board-test=x:t']):
            a = parse(*argv)
            self.assertEqual((a.board_test, a.board), (['x:t'], []), argv)

    def test_glued_bt_is_a_board(self):
        """argparse resolves -btVALUE as -b tVALUE; the roster check then refuses the name."""
        a = parse('-btx:t')
        self.assertEqual((a.board, a.board_test), (['tx:t'], []))

    def test_every_accumulate_spelling(self):
        for s in ('--accumulate', '-a', '-av', '-va', '--accum', '--acc'):
            self.assertTrue(parse(s).accumulate, s)
        self.assertFalse(parse('-v').accumulate)

    def test_build_dir(self):
        self.assertEqual(parse('-B', 'examples').build_dir, 'examples')
        self.assertEqual(parse('--build-dir=out').build_dir, 'out')

    def test_rejects_unknown_option_and_missing_config(self):
        for argv in (['--nope', 'cfg.json'], []):
            with self.assertRaises(SystemExit), contextlib.redirect_stderr(io.StringIO()):
                hil_args.build_parser().parse_args(argv)

    def test_hil_test_builds_no_parser_of_its_own(self):
        tree = ast.parse((HIL_DIR / 'hil_test.py').read_text())
        calls = {ast.unparse(n.func) for n in ast.walk(tree) if isinstance(n, ast.Call)}
        self.assertIn('hil_args.build_parser', calls)
        self.assertNotIn('argparse.ArgumentParser', calls)


if __name__ == '__main__':
    unittest.main()
