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
from unittest import mock

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
        self.assertIn('cmake -S examples -B examples/cmake-build-stm32f407disco '
                      '-DBOARD=stm32f407disco -G Ninja -DCMAKE_BUILD_TYPE=MinSizeRel', cmd[3])
        self.assertIn('cmake --build examples/cmake-build-stm32f407disco '
                      '--target cdc_msc', cmd[3])
        self.assertEqual(cmd[4], 'examples/cmake-build-stm32f407disco/'
                                 'device/cdc_msc/cdc_msc.elf')

    def test_build_script_reconfigures_before_building(self):
        # `membrowse onboard` runs `git clean -fdx` before every historical
        # build, which deletes the ignored build_dir - the build script must
        # reconfigure it rather than assume it survives from the pre-flight
        # check in main().
        cmd = mo.compose('b', 'device/x', 5, False, 'k', [])
        self.assertIn('cmake -S examples -B examples/cmake-build-b -DBOARD=b', cmd[3])
        self.assertIn(' && cmake --build examples/cmake-build-b --target x', cmd[3])

    def test_dry_run_by_default_upload_drops_it(self):
        self.assertIn('--dry-run', mo.compose('b', 'device/x', 5, False, 'k', []))
        self.assertNotIn('--dry-run', mo.compose('b', 'device/x', 5, True, 'k', []))

    def test_build_dirs_scope_and_extra_passthrough(self):
        # the example's own dir is in scope too - its sources (e.g. src/main.c)
        # link into the same elf as src/ and hw/, so a change there must also
        # trigger a rebuild rather than an --identical skip.
        cmd = mo.compose('b', 'host/y', 5, False, 'k', ['--initial-commit', 'HEAD~5'])
        i = cmd.index('--build-dirs')
        self.assertEqual(cmd[i + 1:i + 8], [
            'src/', 'hw/', 'examples/build_system/', 'examples/CMakeLists.txt',
            'examples/host/CMakeLists.txt', 'examples/host/y/', 'tools/get_deps.py'])
        self.assertEqual(cmd[-2:], ['--initial-commit', 'HEAD~5'])

    def test_espressif_uses_idf_build_and_generated_linker_scripts(self):
        cmd = mo.compose('espressif_s3_devkitm', 'device/cdc_msc_freertos',
                         5, False, 'k', [], family='espressif')
        build = cmd[3]
        self.assertIn('idf.py -C examples/device/cdc_msc_freertos', build)
        self.assertIn('-DBOARD=espressif_s3_devkitm build', build)
        self.assertIn('cdc_msc_freertos.elf', build)
        self.assertIn('esp-idf/esp_system/ld/memory.ld', build)
        self.assertEqual(cmd[4], 'examples/cmake-build-espressif_s3_devkitm/'
                                 'cdc_msc_freertos.elf')

    def test_binary_search_omits_mutually_exclusive_build_dirs(self):
        cmd = mo.compose('b', 'device/x', 5, False, 'k', ['--binary-search'])
        self.assertNotIn('--build-dirs', cmd)

    def test_explicit_commits_omit_mutually_exclusive_count(self):
        cmd = mo.compose('b', 'device/x', 5, False, 'k', ['--commits', 'a b'])
        self.assertNotIn('5', cmd[:4])

    def test_each_historical_build_fetches_its_own_deps_and_linker_settings(self):
        cmd = mo.compose('b', 'device/x', 5, False, 'k', [])
        i = cmd.index('--ld-scripts')
        self.assertEqual(cmd[i + 1], 'examples/cmake-build-b/.membrowse-onboard.ld')
        self.assertIn('tools/get_deps.py -b b', cmd[3])
        self.assertIn('--write-linker-shim', cmd[3])
        self.assertNotIn('--relink-deps', cmd[3])


class WriteLinkerShim(unittest.TestCase):
    def test_uses_each_builds_scripts_and_defsyms(self):
        with tempfile.TemporaryDirectory() as tmp:
            out = os.path.join(tmp, 'settings.ld')
            commands = ('cc -Wl,--script=/tree/board.ld '
                        '-Wl,--defsym=FLASH_SIZE=256K -o x.elf\n')
            with mock.patch.object(mo, 'ninja_commands', return_value=commands), \
                 mock.patch.object(mo.os.path, 'isfile', return_value=True):
                mo.write_linker_shim('ninja', 'build', 'x', out)
            with open(out) as f:
                self.assertEqual(f.read(), 'FLASH_SIZE = 256K;\nINCLUDE "/tree/board.ld"\n')

    def test_explicit_scripts_are_resolved_from_the_build_dir(self):
        with tempfile.TemporaryDirectory() as tmp:
            build = os.path.join(tmp, 'build')
            script_dir = os.path.join(build, 'esp-idf', 'esp_system', 'ld')
            os.makedirs(script_dir)
            for name in ('memory.ld', 'sections.ld'):
                open(os.path.join(script_dir, name), 'w').close()
            out = os.path.join(build, 'settings.ld')
            with mock.patch.object(mo, 'ninja_commands', return_value='cc -o x.elf\n'):
                mo.write_linker_shim(
                    'ninja', build, 'x.elf', out,
                    'esp-idf/esp_system/ld/memory.ld',
                    'esp-idf/esp_system/ld/sections.ld')
            with open(out) as f:
                self.assertEqual(f.read(),
                                 f'INCLUDE "{script_dir}/memory.ld"\n'
                                 f'INCLUDE "{script_dir}/sections.ld"\n')


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


class DisposableWorktree(unittest.TestCase):
    """`membrowse onboard` checks out and `git clean -fdx`s every historical
    commit unconditionally, in whatever directory it runs (membrowse/utils/
    git.py) - main() must run it in a disposable worktree, never repo_root."""

    def test_membrowse_runs_in_a_disposable_worktree_then_cleans_it_up(self):
        with tempfile.TemporaryDirectory() as tmp:
            env = {k: v for k, v in os.environ.items() if not k.startswith('GIT_')}
            env.update(GIT_AUTHOR_NAME='t', GIT_AUTHOR_EMAIL='t@t',
                      GIT_COMMITTER_NAME='t', GIT_COMMITTER_EMAIL='t@t')
            subprocess.run(['git', 'init', '-q', tmp], check=True,
                           capture_output=True, env=env)
            # Ignore everything below (build_dir, ld stub, membrowse stub) so
            # the dirty-tree guard sees a clean `git status --porcelain`, same
            # trick as MainExtractionFailure - but track .gitignore itself so
            # HEAD has a real commit to fork the disposable worktree from.
            with open(os.path.join(tmp, '.gitignore'), 'w') as f:
                f.write('*\n')
            subprocess.run(['git', '-C', tmp, 'add', '-f', '.gitignore'],
                           check=True, capture_output=True, env=env)
            subprocess.run(['git', '-C', tmp, '-c', 'commit.gpgsign=false',
                            'commit', '-q', '-m', 'init'],
                           check=True, capture_output=True, env=env)

            build_dir = os.path.join(tmp, 'examples', 'cmake-build-b')
            os.makedirs(build_dir)
            ld_path = os.path.join(tmp, 'fake.ld')
            open(ld_path, 'w').close()

            stub_dir = os.path.join(tmp, 'stubbin')
            os.mkdir(stub_dir)
            ninja_stub = os.path.join(stub_dir, 'ninja')
            with open(ninja_stub, 'w') as f:
                f.write(f'#!/usr/bin/env python3\nprint("cc -Wl,--script={ld_path} -o out.elf")\n')
            os.chmod(ninja_stub, 0o755)

            # Records the cwd `membrowse` actually ran in, instead of doing
            # anything a real historical backfill would.
            cwd_marker = os.path.join(tmp, 'membrowse_cwd.txt')
            membrowse_stub = os.path.join(stub_dir, 'membrowse')
            with open(membrowse_stub, 'w') as f:
                f.write('#!/usr/bin/env python3\nimport os\n'
                       f'open({cwd_marker!r}, "w").write(os.getcwd())\n')
            os.chmod(membrowse_stub, 0o755)

            env['PATH'] = stub_dir + os.pathsep + env.get('PATH', '')
            script = os.path.join(REPO, 'tools', 'membrowse_onboard.py')
            r = subprocess.run(
                [sys.executable, script, 'b', 'device/x', '-n', '1'],
                capture_output=True, text=True, cwd=tmp, env=env)
            self.assertEqual(r.returncode, 0, r.stderr)

            worktree_dir = os.path.join(tmp, 'cmake-metrics', '_onboard_worktree')
            with open(cwd_marker) as f:
                actual_cwd = f.read()
            self.assertEqual(os.path.realpath(actual_cwd), os.path.realpath(worktree_dir))
            self.assertNotEqual(os.path.realpath(actual_cwd), os.path.realpath(tmp))

            # Cleaned up afterward, and the caller's own checkout was never
            # touched (still on its branch, not detached).
            self.assertFalse(os.path.isdir(worktree_dir))
            listing = subprocess.run(['git', '-C', tmp, 'worktree', 'list'],
                                     capture_output=True, text=True, env=env)
            self.assertNotIn('_onboard_worktree', listing.stdout)
            branch = subprocess.run(['git', '-C', tmp, 'symbolic-ref', '-q', 'HEAD'],
                                    capture_output=True, text=True, env=env)
            self.assertEqual(branch.returncode, 0, 'caller checkout ended up detached')


if __name__ == '__main__':
    unittest.main()
