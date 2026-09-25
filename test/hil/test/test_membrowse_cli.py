#!/usr/bin/env python3
"""Unit tests for tools/membrowse_cli.py: report argv/key handling and onboard's
argv composition, which pins CI's target-name convention (`<board>/<cmake-target>`
basename)."""
import argparse
import os
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), '../../..'))
SCRIPT = os.path.join(REPO, 'tools', 'membrowse_cli.py')
sys.path.insert(0, os.path.join(REPO, 'tools'))
import membrowse_cli as cli  # noqa: E402


def _write_stub(directory, name, body):
    path = os.path.join(directory, name)
    with open(path, 'w') as f:
        f.write(body)
    os.chmod(path, 0o755)
    return path


class Regexes(unittest.TestCase):
    def test_ld_script_extraction_all_forms_deduped(self):
        text = ('cc -Wl,--script=a.ld -o out.elf\ncc -T b.ld -o out2.elf\n'
                'cc -Tc.ld -o out3.elf\ncc -T b.ld -o out4.elf\n')
        self.assertEqual(cli.extract_ld_scripts(text), ['a.ld', 'b.ld', 'c.ld'])

    def test_ld_script_extraction_handles_drive_letters_and_spaces(self):
        text = 'cc -T C:/work/tinyusb/board.ld -o a.elf\ncc -T "/work/a b/board.ld" -o b.elf\n'
        self.assertEqual(cli.extract_ld_scripts(text),
                         ['C:/work/tinyusb/board.ld', '/work/a b/board.ld'])

    def test_defsym_extraction_both_separators(self):
        text = 'cc -Wl,--defsym=FOO=0x10 -Wl,--defsym,BAR=1 -o out.elf\n'
        self.assertEqual(cli.DEFSYM_RE.findall(text), ['FOO=0x10', 'BAR=1'])

    def test_defsym_extraction_dedupes_preserving_first_seen_order(self):
        text = 'cc -Wl,--defsym=FOO=0x10 -Wl,--defsym,BAR=1 -Wl,--defsym=FOO=0x10 -o out.elf\n'
        self.assertEqual(cli.extract_defsyms(text), ['FOO=0x10', 'BAR=1'])


class LinkCommand(unittest.TestCase):
    def test_asks_ninja_for_only_the_elfs_final_command(self):
        # -s: without it, a target's commands include helper executables' links
        # (pico-sdk's boot_stage2) whose linker scripts are not the elf's
        with tempfile.TemporaryDirectory() as tmp:
            fake_ninja = _write_stub(tmp, 'fake_ninja.sh', '#!/bin/sh\necho "$@"\n')
            self.assertEqual(cli.link_command(fake_ninja, tmp, os.path.join(tmp, 'd', 'x.elf')),
                             f'-C {tmp} -t commands -s d/x.elf\n')

    def test_no_command_is_an_error(self):
        # ninja exits 0 with no output for a phony target or an unknown file
        with tempfile.TemporaryDirectory() as tmp:
            with self.assertRaisesRegex(RuntimeError, 'no command builds'):
                cli.link_command('true', tmp, os.path.join(tmp, 'x.elf'))


class BuildMembrowseCmd(unittest.TestCase):
    def _args(self, elf, **kw):
        base = dict(target_name='board/example', upload=False, ld=None, option='')
        base.update(kw)
        return argparse.Namespace(elf=elf, **base)

    def test_local_report_includes_elf_and_ld_scripts(self):
        with tempfile.TemporaryDirectory() as tmp:
            elf = os.path.join(tmp, 'x.elf')
            open(elf, 'w').close()
            commands = 'cc -Wl,--script=/a/b.ld -o x.elf\n'
            cmd = cli.build_membrowse_cmd(self._args(elf), commands)
            self.assertEqual(cmd, ['membrowse', 'report', elf, '/a/b.ld'])

    def test_map_file_appended_when_present(self):
        with tempfile.TemporaryDirectory() as tmp:
            elf = os.path.join(tmp, 'x.elf')
            open(elf, 'w').close()
            open(elf + '.map', 'w').close()
            cmd = cli.build_membrowse_cmd(self._args(elf, ld=['/fake.ld']), '')
            self.assertIn('--map-file', cmd)
            self.assertEqual(cmd[cmd.index('--map-file') + 1], elf + '.map')

    def test_defsym_becomes_def_args(self):
        with tempfile.TemporaryDirectory() as tmp:
            elf = os.path.join(tmp, 'x.elf')
            open(elf, 'w').close()
            commands = 'cc -Wl,--defsym=FOO=0x10 -Wl,--defsym,BAR=1 -o x.elf\n'
            cmd = cli.build_membrowse_cmd(self._args(elf, ld=['/fake.ld']), commands)
            self.assertEqual(cmd[cmd.index('--def') + 1], 'FOO=0x10')
            self.assertIn('BAR=1', cmd)

    def test_ld_override_skips_ninja_extraction(self):
        with tempfile.TemporaryDirectory() as tmp:
            elf = os.path.join(tmp, 'x.elf')
            open(elf, 'w').close()
            commands = 'cc -Wl,--script=/should/not/be-used.ld -o x.elf\n'
            cmd = cli.build_membrowse_cmd(
                self._args(elf, ld=['/override/a.ld', '/override/b.ld']), commands)
            self.assertIn('/override/a.ld /override/b.ld', cmd)
            self.assertNotIn('/should/not/be-used.ld', ' '.join(cmd))


    def test_option_split_inserted_after_report(self):
        with tempfile.TemporaryDirectory() as tmp:
            elf = os.path.join(tmp, 'x.elf')
            open(elf, 'w').close()
            cmd = cli.build_membrowse_cmd(
                self._args(elf, ld=['/fake.ld'], option='--json --all-symbols'), '')
            self.assertEqual(cmd[:4], ['membrowse', 'report', '--json', '--all-symbols'])

    def test_option_split_preserves_quoted_values(self):
        with tempfile.TemporaryDirectory() as tmp:
            elf = os.path.join(tmp, 'x.elf')
            open(elf, 'w').close()
            cmd = cli.build_membrowse_cmd(
                self._args(elf, ld=['/fake.ld'], option='--label "two words"'), '')
            self.assertEqual(cmd[:4], ['membrowse', 'report', '--label', 'two words'])


class Redaction(unittest.TestCase):
    def test_masks_the_key_wherever_it_sits(self):
        # membrowse report puts it after --api-key, membrowse onboard as a positional
        # (compose()), so the rule is by value
        self.assertEqual(cli.redacted(['membrowse', 'report', '--api-key', 's3cret'], 's3cret'),
                         ['membrowse', 'report', '--api-key', '***'])
        self.assertEqual(cli.redacted(['membrowse', 'onboard', 'b/e', 's3cret'], 's3cret'),
                         ['membrowse', 'onboard', 'b/e', '***'])

    def test_no_key_leaves_the_argv_alone(self):
        for secret in (None, ''):
            self.assertEqual(cli.redacted(['membrowse', 'report', 'x'], secret),
                             ['membrowse', 'report', 'x'])


class ReportCli(unittest.TestCase):
    """End-to-end CLI tests: real `python3 tools/membrowse_cli.py report` subprocess,
    with a stub `membrowse` injected into PATH so the real tool is never invoked."""

    def _run(self, tmp, args, env_extra=None):
        stub_dir = os.path.join(tmp, 'stubbin')
        os.mkdir(stub_dir)
        _write_stub(stub_dir, 'membrowse', '#!/usr/bin/env python3\n'
                                           'import os, sys\n'
                                           'print("STUB_ARGV:" + " ".join(sys.argv[1:]))\n'
                                           'print("STUB_CWD:" + os.getcwd())\n')
        env = dict(os.environ, PATH=stub_dir + os.pathsep + os.environ.get('PATH', ''))
        env.pop('MEMBROWSE_API_KEY', None)
        env.update(env_extra or {})
        return subprocess.run([sys.executable, SCRIPT, 'report',
                               '--target-name', 'board/example'] + args,
                              capture_output=True, text=True, env=env)

    def _elf(self, tmp):
        elf = os.path.join(tmp, 'fake.elf')
        open(elf, 'w').close()
        return elf

    def _ninja(self, tmp, link='cc -o fake.elf'):
        """Fake ninja answering `-t commands -s fake.elf` with `link`, nothing otherwise."""
        return _write_stub(tmp, 'fake_ninja.sh',
                           '#!/bin/sh\n[ "$3 $4 $5 $6" = "-t commands -s fake.elf" ] && '
                           f"echo '{link}'\n")

    def _upload(self, tmp, env_extra):
        # --ld replaces the extracted scripts: these cover upload/key handling only
        return self._run(tmp, ['--build-dir', tmp, '--ninja', self._ninja(tmp),
                               '--elf', self._elf(tmp), '--ld', '/fake.ld', '--upload'], env_extra)

    def test_reports_with_the_elfs_link_from_the_build_dir(self):
        # the build dir is the link's working dir: pico-sdk's `-L .` INCLUDE
        # (pico_flash_region.ld) resolves there, as it does for the linker
        with tempfile.TemporaryDirectory() as tmp:
            ninja = self._ninja(tmp, 'cc -Wl,--script=/sdk/memmap.ld -o fake.elf')
            r = self._run(tmp, ['--build-dir', tmp, '--ninja', ninja, '--elf', self._elf(tmp)])
            self.assertEqual(r.returncode, 0, r.stderr)
            self.assertIn(' /sdk/memmap.ld', r.stdout)
            self.assertIn(f'STUB_CWD:{os.path.realpath(tmp)}', r.stdout)

    def test_relative_elf_still_names_the_elf_from_the_build_dir(self):
        with tempfile.TemporaryDirectory() as tmp:
            ninja = self._ninja(tmp, 'cc -Wl,--script=/sdk/memmap.ld -o fake.elf')
            elf = os.path.relpath(self._elf(tmp))
            r = self._run(tmp, ['--build-dir', tmp, '--ninja', ninja, '--elf', elf])
            self.assertEqual(r.returncode, 0, r.stderr)
            self.assertIn(f'STUB_ARGV:report {os.path.abspath(elf)} ', r.stdout)

    def test_upload_without_key_goes_tokenless(self):
        # No MEMBROWSE_API_KEY in the environment (fork PR): the CLI must still
        # invoke membrowse - with --github but no --api-key - instead of exiting.
        with tempfile.TemporaryDirectory() as tmp:
            r = self._upload(tmp, {})
            self.assertEqual(r.returncode, 0, r.stderr)
            logged_line = r.stdout.splitlines()[0]
            self.assertIn('--github', logged_line)
            self.assertNotIn('--api-key', logged_line)

    def test_upload_redacts_logged_key_and_passes_real_key(self):
        with tempfile.TemporaryDirectory() as tmp:
            r = self._upload(tmp, {'MEMBROWSE_API_KEY': 'dummysecret'})
            self.assertEqual(r.returncode, 0, r.stderr)
            logged_line = r.stdout.splitlines()[0]
            self.assertNotIn('dummysecret', logged_line)
            self.assertIn('***', logged_line)
            stub_line = [l for l in r.stdout.splitlines() if l.startswith('STUB_ARGV:')]
            self.assertTrue(stub_line, r.stdout)
            self.assertIn('dummysecret', stub_line[0])

    def test_no_ld_scripts_without_override_errors_cleanly_no_traceback(self):
        with tempfile.TemporaryDirectory() as tmp:
            r = self._run(tmp, ['--build-dir', tmp, '--ninja', self._ninja(tmp),
                                '--elf', self._elf(tmp)])
            self.assertNotEqual(r.returncode, 0)
            self.assertNotIn('Traceback', r.stderr)
            self.assertIn('linker script', r.stderr)

    def test_identical_via_cli_needs_no_build_dir_or_ninja(self):
        # a no-code-change CI run has neither a configured build dir nor an elf
        with tempfile.TemporaryDirectory() as tmp:
            r = self._run(tmp, ['--build-dir', os.path.join(tmp, 'no-such-build-dir'),
                                '--ninja', '/no/such/ninja',
                                '--elf', os.path.join(tmp, 'never-built.elf')])
            self.assertEqual(r.returncode, 0, r.stderr)
            self.assertIn('STUB_ARGV:report --identical', r.stdout)

    def test_report_rejects_unknown_arguments(self):
        # only onboard forwards extra arguments (to `membrowse onboard`)
        with tempfile.TemporaryDirectory() as tmp:
            r = self._run(tmp, ['--build-dir', tmp, '--ninja', self._ninja(tmp),
                                '--elf', self._elf(tmp), '--no-such-flag'])
            self.assertEqual(r.returncode, 2)
            self.assertIn('unrecognized arguments: --no-such-flag', r.stderr)

    def test_failed_ninja_query_errors_cleanly_no_traceback(self):
        with tempfile.TemporaryDirectory() as tmp:
            fake_ninja = _write_stub(tmp, 'fake_ninja.sh', '#!/bin/sh\necho "boom" >&2\nexit 3\n')
            r = self._run(tmp, ['--build-dir', tmp, '--ninja', fake_ninja, '--elf', self._elf(tmp)])
            self.assertNotEqual(r.returncode, 0)
            self.assertNotIn('Traceback', r.stderr)
            self.assertIn('boom', r.stderr)


class Compose(unittest.TestCase):
    def test_target_name_uses_basename_not_role_path(self):
        cmd = cli.compose('stm32f407disco', 'device/cdc_msc', 30, False, 'k', [])
        self.assertIn('stm32f407disco/cdc_msc', cmd)
        self.assertNotIn('stm32f407disco/device/cdc_msc', cmd)

    def test_ci_uploads_use_the_same_target_name(self):
        # membrowse history is keyed on it: a rename orphans every series
        with open(os.path.join(REPO, 'hw', 'bsp', 'family_support.cmake')) as f:
            args = [line.split('#', 1)[0].strip() for line in f]
        self.assertEqual([a for a in args if a.startswith('--target-name')],
                         ['--target-name ${BOARD}/${TARGET}'])

    def test_paths_and_build_script_are_repo_root_relative(self):
        cmd = cli.compose('stm32f407disco', 'device/cdc_msc', 30, False, 'k', [])
        self.assertIn('cmake -S examples -B examples/cmake-build-stm32f407disco '
                      '-DBOARD=stm32f407disco -G Ninja -DCMAKE_BUILD_TYPE=MinSizeRel', cmd[3])
        self.assertIn('cmake --build examples/cmake-build-stm32f407disco '
                      '--target cdc_msc', cmd[3])
        self.assertEqual(cmd[4], 'examples/cmake-build-stm32f407disco/'
                                 'device/cdc_msc/cdc_msc.elf')

    def test_build_script_reconfigures_before_building(self):
        # `membrowse onboard` runs `git clean -fdx` before every historical
        # build, which deletes the ignored build_dir - the build script must
        # reconfigure it rather than assume it survives.
        cmd = cli.compose('b', 'device/x', 5, False, 'k', [])
        self.assertIn('cmake -S examples -B examples/cmake-build-b -DBOARD=b', cmd[3])
        self.assertIn(' && cmake --build examples/cmake-build-b --target x', cmd[3])

    def test_dry_run_by_default_upload_drops_it(self):
        self.assertIn('--dry-run', cli.compose('b', 'device/x', 5, False, 'k', []))
        self.assertNotIn('--dry-run', cli.compose('b', 'device/x', 5, True, 'k', []))

    def test_build_dirs_scope_and_extra_passthrough(self):
        # the example's own dir is in scope too - its sources (e.g. src/main.c)
        # link into the same elf as src/ and hw/, so a change there must also
        # trigger a rebuild rather than an --identical skip.
        cmd = cli.compose('b', 'host/y', 5, False, 'k', ['--initial-commit', 'HEAD~5'])
        i = cmd.index('--build-dirs')
        self.assertEqual(cmd[i + 1:i + 9], [
            'src/', 'hw/', 'lib/', 'examples/build_system/', 'examples/CMakeLists.txt',
            'examples/host/CMakeLists.txt', 'examples/host/y/', 'tools/get_deps.py'])
        self.assertEqual(cmd[-2:], ['--initial-commit', 'HEAD~5'])

    def test_espressif_uses_idf_build_and_generated_linker_scripts(self):
        cmd = cli.compose('espressif_s3_devkitm', 'device/cdc_msc_freertos',
                         5, False, 'k', [], family='espressif')
        build = cmd[3]
        self.assertIn('idf.py -C examples/device/cdc_msc_freertos', build)
        self.assertIn('-DBOARD=espressif_s3_devkitm build', build)
        self.assertIn('linker-shim ninja examples/cmake-build-espressif_s3_devkitm '
                      'examples/cmake-build-espressif_s3_devkitm/cdc_msc_freertos.elf ', build)
        self.assertIn('esp-idf/esp_system/ld/memory.ld', build)
        self.assertEqual(cmd[4], 'examples/cmake-build-espressif_s3_devkitm/'
                                 'cdc_msc_freertos.elf')

    def test_binary_search_omits_mutually_exclusive_build_dirs(self):
        cmd = cli.compose('b', 'device/x', 5, False, 'k', ['--binary-search'])
        self.assertNotIn('--build-dirs', cmd)

    def test_explicit_commits_omit_mutually_exclusive_count(self):
        cmd = cli.compose('b', 'device/x', 5, False, 'k', ['--commits', 'a b'])
        self.assertNotIn('5', cmd[:4])

    def test_each_historical_build_fetches_its_own_deps_and_linker_settings(self):
        cmd = cli.compose('b', 'device/x', 5, False, 'k', [])
        i = cmd.index('--ld-scripts')
        self.assertEqual(cmd[i + 1], 'examples/cmake-build-b/.membrowse-onboard.ld')
        self.assertIn('tools/get_deps.py -b b', cmd[3])
        # the shim reads that build's own link of the elf
        self.assertIn('membrowse_cli.py linker-shim ninja examples/cmake-build-b '
                      'examples/cmake-build-b/device/x/x.elf '
                      'examples/cmake-build-b/.membrowse-onboard.ld', cmd[3])
        self.assertNotIn('--relink-deps', cmd[3])


class WriteLinkerShim(unittest.TestCase):
    def test_uses_each_builds_scripts_and_defsyms(self):
        with tempfile.TemporaryDirectory() as tmp:
            out = os.path.join(tmp, 'settings.ld')
            commands = 'cc -Wl,--script=/tree/board.ld -Wl,--defsym=FLASH_SIZE=256K -o x.elf\n'
            with mock.patch.object(cli, 'link_command', return_value=commands), \
                 mock.patch.object(cli.os.path, 'isfile', return_value=True):
                cli.write_linker_shim('ninja', 'build', 'x.elf', out)
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
            with mock.patch.object(cli, 'link_command', return_value='cc -o x.elf\n'):
                cli.write_linker_shim(
                    'ninja', build, 'x.elf', out,
                    'esp-idf/esp_system/ld/memory.ld',
                    'esp-idf/esp_system/ld/sections.ld')
            with open(out) as f:
                self.assertEqual(f.read(),
                                 f'INCLUDE "{script_dir}/memory.ld"\n'
                                 f'INCLUDE "{script_dir}/sections.ld"\n')

    def test_subcommand_runs_as_onboards_build_script_calls_it(self):
        # the historical build script runs `<python> <abs membrowse_cli.py> linker-shim ninja ...`
        with tempfile.TemporaryDirectory() as tmp:
            ld = os.path.join(tmp, 'board.ld')
            open(ld, 'w').close()
            fake_ninja = _write_stub(tmp, 'fake_ninja.sh',
                                     f'#!/bin/sh\necho "cc -Wl,--script={ld} '
                                     f'-Wl,--defsym=FLASH_SIZE=256K -o x.elf"\n')
            out = os.path.join(tmp, 'settings.ld')
            r = subprocess.run([sys.executable, SCRIPT, 'linker-shim', fake_ninja, tmp, 'x.elf', out],
                               capture_output=True, text=True)
            self.assertEqual(r.returncode, 0, r.stderr)
            with open(out) as f:
                self.assertEqual(f.read(), f'FLASH_SIZE = 256K;\nINCLUDE "{ld}"\n')

    def test_subcommand_is_hidden_from_help(self):
        r = subprocess.run([sys.executable, SCRIPT, '--help'], capture_output=True, text=True)
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertIn('onboard', r.stdout)
        self.assertNotIn('linker-shim', r.stdout)


class DisposableWorktree(unittest.TestCase):
    """`membrowse onboard` checks out and `git clean -fdx`s every historical
    commit unconditionally, in whatever directory it runs (membrowse/utils/
    git.py) - onboard() must run it in a disposable worktree, never repo_root."""

    def test_membrowse_runs_in_a_disposable_worktree_then_cleans_it_up(self):
        with tempfile.TemporaryDirectory() as tmp:
            env = {k: v for k, v in os.environ.items() if not k.startswith('GIT_')}
            env.update(GIT_AUTHOR_NAME='t', GIT_AUTHOR_EMAIL='t@t',
                      GIT_COMMITTER_NAME='t', GIT_COMMITTER_EMAIL='t@t')
            subprocess.run(['git', 'init', '-q', tmp], check=True,
                           capture_output=True, env=env)
            # Ignore everything below, but track .gitignore itself so HEAD has
            # a real commit to fork the disposable worktree from.
            with open(os.path.join(tmp, '.gitignore'), 'w') as f:
                f.write('*\n')
            subprocess.run(['git', '-C', tmp, 'add', '-f', '.gitignore'],
                           check=True, capture_output=True, env=env)
            subprocess.run(['git', '-C', tmp, '-c', 'commit.gpgsign=false',
                            'commit', '-q', '-m', 'init'],
                           check=True, capture_output=True, env=env)
            with open(os.path.join(tmp, '.gitignore'), 'a') as f:
                f.write('# caller stays dirty\n')

            stub_dir = os.path.join(tmp, 'stubbin')
            os.mkdir(stub_dir)

            # Records the cwd `membrowse` actually ran in, instead of doing
            # anything a real historical backfill would.
            cwd_marker = os.path.join(tmp, 'membrowse_cwd.txt')
            _write_stub(stub_dir, 'membrowse', '#!/usr/bin/env python3\nimport os\n'
                        f'open({cwd_marker!r}, "w").write(os.getcwd())\n')

            env['PATH'] = stub_dir + os.pathsep + env.get('PATH', '')
            r = subprocess.run(
                [sys.executable, SCRIPT, 'onboard', 'b', 'device/x', '-n', '1'],
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
            status = subprocess.run(['git', '-C', tmp, 'status', '--porcelain'],
                                    capture_output=True, text=True, env=env)
            self.assertEqual(status.stdout.strip(), 'M .gitignore')


if __name__ == '__main__':
    unittest.main()
