#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""The pyocd flasher's roster checks and its project dir, test/hil/pyocd. The provider's
launcher and user script are agentrc's tests (tests/test_hil_pyocd.py).
"""
import json
import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

HIL_DIR = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(HIL_DIR))

import hil_flash    # noqa: E402


class ProjectDir(unittest.TestCase):
    def test_it_exists_and_holds_no_pyocd_config(self):
        # the provider's launcher refuses a missing dir, and discovery's bare Session would load
        # a config from it despite --no-config
        self.assertTrue(hil_flash.PYOCD_PROJECT_DIR.is_dir())
        for name in ('pyocd.yaml', 'pyocd.yml', '.pyocd.yaml', '.pyocd.yml'):
            self.assertFalse((hil_flash.PYOCD_PROJECT_DIR / name).exists(), name)


class Flasher(unittest.TestCase):
    FLASHER = {'name': 'pyocd', 'uid': 'GSA0CQEQ', 'vid_pid': '0x1fc9 0x0090',
               'args': '-t lpc55s28'}

    def test_an_unfiltered_primary_or_recovery_pyocd_entry_is_found(self):
        jlink = {'name': 'jlink', 'uid': 'X'}
        for board, want in (({'flasher': self.FLASHER}, False),
                            ({'flasher': {**self.FLASHER, 'name': 'PyOCD', 'vid_pid': None}}, True),
                            ({'flasher': jlink, 'flasher_recover': {'name': 'pyocd'}}, True),
                            ({'flasher': jlink}, False)):
            self.assertEqual(hil_flash.unfiltered_pyocd(board), want, board)

    def test_hil_test_refuses_an_unfiltered_entry_before_touching_a_board(self):
        with tempfile.TemporaryDirectory() as td:
            cfg = Path(td) / 'c.json'
            cfg.write_text(json.dumps({'boards': [{'name': 'b', 'uid': 'X', 'tests': {'device': True},
                                                   'flasher': {'name': 'pyocd', 'uid': 'S'}}]}))
            r = subprocess.run([sys.executable, str(HIL_DIR / 'hil_test.py'), str(cfg)],
                               env={**os.environ, 'HIL_REPORT_DIR': td},
                               capture_output=True, text=True, timeout=60)
        self.assertEqual(r.returncode, 1)
        self.assertIn('a pyocd flasher needs a uid and one "vid_pid" pair', r.stdout)


if __name__ == '__main__':
    unittest.main()
