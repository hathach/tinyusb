#!/usr/bin/env python3
"""Tests for tools/membrowse_onboard.py's argv composition.

The wrapper exists so a backfill cannot diverge from CI's target-name
convention (`<board>/<cmake-target>` basename) - these tests pin exactly that.
"""
import os
import subprocess
import sys
import tempfile
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

    def test_ld_scripts_and_defsyms_passed_through(self):
        # so a backfill computes over the same regions CI's own upload for this
        # target name used - not membrowse's DEFAULT Code/Data regions.
        cmd = mo.compose('b', 'device/x', 5, False, 'k', [],
                         ld_scripts=['/a.ld', '/b.ld'], defsyms=['FOO=1', 'BAR=2'])
        i = cmd.index('--ld-scripts')
        self.assertEqual(cmd[i + 1], '/a.ld /b.ld')
        self.assertEqual(cmd.count('--def'), 2)
        self.assertIn('FOO=1', cmd)
        self.assertIn('BAR=2', cmd)

    def test_no_ld_scripts_or_defsyms_omits_flags(self):
        cmd = mo.compose('b', 'device/x', 5, False, 'k', [])
        self.assertNotIn('--ld-scripts', cmd)
        self.assertNotIn('--def', cmd)


class MainExtractionFailure(unittest.TestCase):
    """End-to-end: a configured build dir whose ninja graph has no linker script
    must abort before ever invoking `membrowse onboard`, not silently backfill
    against membrowse's DEFAULT regions."""

    def test_no_ld_scripts_exits_before_calling_membrowse(self):
        with tempfile.TemporaryDirectory() as tmp:
            build_dir = os.path.join(tmp, 'examples', 'cmake-build-b')
            os.makedirs(build_dir)
            stub_dir = os.path.join(tmp, 'stubbin')
            os.mkdir(stub_dir)
            # stub ninja: succeeds, but its ninja-graph "commands" have no
            # linker script or --defsym for extract_ld_scripts()/extract_defsyms()
            # to find.
            ninja_stub = os.path.join(stub_dir, 'ninja')
            with open(ninja_stub, 'w') as f:
                f.write('#!/usr/bin/env python3\nprint("cc -o out.elf")\n')
            os.chmod(ninja_stub, 0o755)
            # main()'s dirty-tree check runs before the extraction under test, and
            # a non-repo cwd is now fatal there (see WorktreeGuard) - so make `tmp`
            # a real, clean repo: `.gitignore` of `*` leaves porcelain empty
            # (ignored files, including itself, are not reported).
            with open(os.path.join(tmp, '.gitignore'), 'w') as f:
                f.write('*\n')
            # Strip GIT_* (GIT_DIR/GIT_WORK_TREE/GIT_INDEX_FILE/...) for BOTH the
            # init and the run below: under a pre-commit hook those are set, and
            # they would send `git init` and the wrapper's own status check at
            # this repo instead of `tmp`.
            env = {k: v for k, v in os.environ.items() if not k.startswith('GIT_')}
            subprocess.run(['git', 'init', '-q', tmp], check=True,
                           capture_output=True, env=env)
            env['PATH'] = stub_dir + os.pathsep + env.get('PATH', '')
            script = os.path.join(REPO, 'tools', 'membrowse_onboard.py')
            r = subprocess.run(
                [sys.executable, script, 'b', 'device/x', '-n', '1'],
                capture_output=True, text=True, cwd=tmp, env=env)
            self.assertNotEqual(r.returncode, 0)
            self.assertIn('linker script', r.stderr)
            self.assertNotIn('Traceback', r.stderr)


class WorktreeGuard(unittest.TestCase):
    """`membrowse onboard` checks out past commits in place, so the wrapper
    refuses to start unless it can prove the worktree is clean."""

    def test_failed_git_status_is_fatal_not_clean(self):
        # A failed `git status` leaves stdout empty, which read as "clean" and
        # let onboard check out over uncommitted work - the very thing the
        # guard exists to prevent.
        with tempfile.TemporaryDirectory() as tmp:
            env = {k: v for k, v in os.environ.items() if not k.startswith('GIT_')}
            r = subprocess.run(
                [sys.executable, os.path.join(REPO, 'tools', 'membrowse_onboard.py'),
                 'b', 'device/x', '-n', '1'],
                capture_output=True, text=True, cwd=tmp, env=env)
            self.assertNotEqual(r.returncode, 0)
            self.assertIn('git status', r.stderr)
            self.assertNotIn('Traceback', r.stderr)


if __name__ == '__main__':
    unittest.main()
