#!/usr/bin/env python3
"""Tests for tools/membrowse_onboard.py's argv composition.

The wrapper exists so a backfill cannot diverge from CI's target-name
convention (`<board>/<cmake-target>` basename) - these tests pin exactly that.
"""
import os
import subprocess
import sys
import unittest

REPO = subprocess.run(['git', 'rev-parse', '--show-toplevel'],
                      capture_output=True, text=True, check=True).stdout.strip()
sys.path.insert(0, os.path.join(REPO, 'tools'))
import membrowse_onboard as mo  # noqa: E402


class Compose(unittest.TestCase):
    def test_target_name_uses_basename_not_role_path(self):
        cmd = mo.compose('stm32f407disco', 'device/cdc_msc', 30, False, 'k', [])
        self.assertIn('stm32f407disco/cdc_msc', cmd)
        self.assertNotIn('stm32f407disco/device/cdc_msc', cmd)

    def test_paths_and_build_script_are_repo_root_relative(self):
        cmd = mo.compose('stm32f407disco', 'device/cdc_msc', 30, False, 'k', [])
        self.assertEqual(cmd[3], 'cmake --build examples/cmake-build-stm32f407disco '
                                 '--target cdc_msc')
        self.assertEqual(cmd[4], 'examples/cmake-build-stm32f407disco/'
                                 'device/cdc_msc/cdc_msc.elf')

    def test_dry_run_by_default_upload_drops_it(self):
        self.assertIn('--dry-run', mo.compose('b', 'device/x', 5, False, 'k', []))
        self.assertNotIn('--dry-run', mo.compose('b', 'device/x', 5, True, 'k', []))

    def test_build_dirs_scope_and_extra_passthrough(self):
        cmd = mo.compose('b', 'host/y', 5, False, 'k', ['--binary-search'])
        i = cmd.index('--build-dirs')
        self.assertEqual(cmd[i + 1:i + 3], ['src/', 'hw/'])
        self.assertEqual(cmd[-1], '--binary-search')


if __name__ == '__main__':
    unittest.main()
