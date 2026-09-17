#!/usr/bin/env python3
"""Unit tests for tools/membrowse_report.py (pure functions, no build needed)."""
import argparse
import os
import subprocess
import sys
import tempfile
import unittest

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), '../../..'))
SCRIPT = os.path.join(REPO, 'tools', 'membrowse_report.py')
sys.path.insert(0, os.path.join(REPO, 'tools'))
import membrowse_report as mr  # noqa: E402


def _write_stub(directory, name, body):
    path = os.path.join(directory, name)
    with open(path, 'w') as f:
        f.write(body)
    os.chmod(path, 0o755)
    return path


class NinjaCommands(unittest.TestCase):
    def test_success_returns_stdout(self):
        with tempfile.TemporaryDirectory() as tmp:
            fake_ninja = _write_stub(tmp, 'fake_ninja.sh',
                                     '#!/bin/sh\necho "cc -Wl,--script=a.ld -o out.elf"\n')
            self.assertEqual(mr.ninja_commands(fake_ninja, tmp, 'x'),
                              'cc -Wl,--script=a.ld -o out.elf\n')

class Regexes(unittest.TestCase):
    def test_ld_script_extraction_all_forms_deduped(self):
        text = ('cc -Wl,--script=a.ld -o out.elf\ncc -T b.ld -o out2.elf\n'
                'cc -Tc.ld -o out3.elf\ncc -T b.ld -o out4.elf\n')
        self.assertEqual(mr.extract_ld_scripts(text), ['a.ld', 'b.ld', 'c.ld'])

    def test_ld_script_extraction_handles_drive_letters_and_spaces(self):
        text = 'cc -T C:/work/tinyusb/board.ld -o a.elf\ncc -T "/work/a b/board.ld" -o b.elf\n'
        self.assertEqual(mr.extract_ld_scripts(text),
                         ['C:/work/tinyusb/board.ld', '/work/a b/board.ld'])

    def test_defsym_extraction_both_separators(self):
        text = 'cc -Wl,--defsym=FOO=0x10 -Wl,--defsym,BAR=1 -o out.elf\n'
        self.assertEqual(mr.DEFSYM_RE.findall(text), ['FOO=0x10', 'BAR=1'])

    def test_defsym_extraction_dedupes_preserving_first_seen_order(self):
        text = 'cc -Wl,--defsym=FOO=0x10 -Wl,--defsym,BAR=1 -Wl,--defsym=FOO=0x10 -o out.elf\n'
        self.assertEqual(mr.extract_defsyms(text), ['FOO=0x10', 'BAR=1'])


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
            cmd, key = mr.build_membrowse_cmd(self._args(elf), commands)
            self.assertEqual(cmd, ['membrowse', 'report', elf, '/a/b.ld'])
            self.assertIsNone(key)

    def test_map_file_appended_when_present(self):
        with tempfile.TemporaryDirectory() as tmp:
            elf = os.path.join(tmp, 'x.elf')
            open(elf, 'w').close()
            open(elf + '.map', 'w').close()
            cmd, _key = mr.build_membrowse_cmd(self._args(elf, ld=['/fake.ld']), '')
            self.assertIn('--map-file', cmd)
            self.assertEqual(cmd[cmd.index('--map-file') + 1], elf + '.map')

    def test_defsym_becomes_def_args(self):
        with tempfile.TemporaryDirectory() as tmp:
            elf = os.path.join(tmp, 'x.elf')
            open(elf, 'w').close()
            commands = 'cc -Wl,--defsym=FOO=0x10 -Wl,--defsym,BAR=1 -o x.elf\n'
            cmd, _key = mr.build_membrowse_cmd(self._args(elf, ld=['/fake.ld']), commands)
            self.assertEqual(cmd[cmd.index('--def') + 1], 'FOO=0x10')
            self.assertIn('BAR=1', cmd)

    def test_ld_override_skips_ninja_extraction(self):
        with tempfile.TemporaryDirectory() as tmp:
            elf = os.path.join(tmp, 'x.elf')
            open(elf, 'w').close()
            commands = 'cc -Wl,--script=/should/not/be-used.ld -o x.elf\n'
            cmd, _key = mr.build_membrowse_cmd(
                self._args(elf, ld=['/override/a.ld', '/override/b.ld']), commands)
            self.assertIn('/override/a.ld /override/b.ld', cmd)
            self.assertNotIn('/should/not/be-used.ld', ' '.join(cmd))


    def test_option_split_inserted_after_report(self):
        with tempfile.TemporaryDirectory() as tmp:
            elf = os.path.join(tmp, 'x.elf')
            open(elf, 'w').close()
            cmd, _key = mr.build_membrowse_cmd(
                self._args(elf, ld=['/fake.ld'], option='--json --all-symbols'), '')
            self.assertEqual(cmd[:4], ['membrowse', 'report', '--json', '--all-symbols'])

    def test_option_split_preserves_quoted_values(self):
        with tempfile.TemporaryDirectory() as tmp:
            elf = os.path.join(tmp, 'x.elf')
            open(elf, 'w').close()
            cmd, _key = mr.build_membrowse_cmd(
                self._args(elf, ld=['/fake.ld'], option='--label "two words"'), '')
            self.assertEqual(cmd[:4], ['membrowse', 'report', '--label', 'two words'])


class CliKeyHandling(unittest.TestCase):
    """End-to-end CLI tests: real `python3 tools/membrowse_report.py` subprocess,
    with a stub `membrowse` injected into PATH so the real tool is never invoked."""

    def _run(self, tmp, args, env_extra=None):
        stub_dir = os.path.join(tmp, 'stubbin')
        os.mkdir(stub_dir)
        _write_stub(stub_dir, 'membrowse', '#!/usr/bin/env python3\n'
                                           'import sys\n'
                                           'print("STUB_ARGV:" + " ".join(sys.argv[1:]))\n')
        env = dict(os.environ, PATH=stub_dir + os.pathsep + os.environ.get('PATH', ''))
        env.pop('MEMBROWSE_API_KEY', None)
        env.update(env_extra or {})
        return subprocess.run([sys.executable, SCRIPT, '--target', 'x',
                               '--target-name', 'board/example'] + args,
                              capture_output=True, text=True, env=env)

    def _elf(self, tmp):
        elf = os.path.join(tmp, 'fake.elf')
        open(elf, 'w').close()
        return elf

    def _upload(self, tmp, env_extra):
        # --ld bypasses ninja-graph extraction: these cover upload/key handling only
        return self._run(tmp, ['--build-dir', tmp, '--ninja', 'true', '--elf', self._elf(tmp),
                               '--ld', '/fake.ld', '--upload'], env_extra)

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
        # `--ninja true` emits no commands, so no linker script is found
        with tempfile.TemporaryDirectory() as tmp:
            r = self._run(tmp, ['--build-dir', tmp, '--ninja', 'true', '--elf', self._elf(tmp)])
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

    def test_failed_ninja_query_errors_cleanly_no_traceback(self):
        with tempfile.TemporaryDirectory() as tmp:
            fake_ninja = _write_stub(tmp, 'fake_ninja.sh', '#!/bin/sh\necho "boom" >&2\nexit 3\n')
            r = self._run(tmp, ['--build-dir', tmp, '--ninja', fake_ninja, '--elf', self._elf(tmp)])
            self.assertNotEqual(r.returncode, 0)
            self.assertNotIn('Traceback', r.stderr)
            self.assertIn('boom', r.stderr)


if __name__ == '__main__':
    unittest.main()
