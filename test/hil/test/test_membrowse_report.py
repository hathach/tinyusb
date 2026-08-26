#!/usr/bin/env python3
"""Unit tests for tools/membrowse_report.py (pure functions, no build needed)."""
import argparse
import os
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

REPO = subprocess.run(['git', 'rev-parse', '--show-toplevel'],
                      capture_output=True, text=True, check=True).stdout.strip()
SCRIPT = os.path.join(REPO, 'tools', 'membrowse_report.py')
sys.path.insert(0, os.path.join(REPO, 'tools'))
import membrowse_report as mr  # noqa: E402


class ResolveIncludes(unittest.TestCase):
    def test_include_relative_to_script_dir(self):
        with tempfile.TemporaryDirectory() as tmp:
            a = os.path.join(tmp, 'a.ld')
            b = os.path.join(tmp, 'b.ld')
            with open(a, 'w') as f:
                f.write('INCLUDE "b.ld"\n')
            with open(b, 'w') as f:
                f.write('/* leaf */\n')
            self.assertEqual(mr.resolve_includes([a]), [a, b])

    def test_include_resolved_as_is_before_relative_join(self):
        # the include name is a path already valid on its own (here: absolute, and
        # in a directory OTHER than the including script's) - must resolve via the
        # as-is check, not by joining with the including script's directory.
        with tempfile.TemporaryDirectory() as tmp:
            sub1 = os.path.join(tmp, 'sub1')
            sub2 = os.path.join(tmp, 'sub2')
            os.mkdir(sub1)
            os.mkdir(sub2)
            a = os.path.join(sub1, 'a.ld')
            b = os.path.join(sub2, 'b.ld')
            with open(a, 'w') as f:
                f.write(f'INCLUDE "{b}"\n')
            with open(b, 'w') as f:
                f.write('/* leaf */\n')
            self.assertEqual(mr.resolve_includes([a]), [a, b])

    def test_dedup_and_cycle_safe(self):
        with tempfile.TemporaryDirectory() as tmp:
            a = os.path.join(tmp, 'a.ld')
            b = os.path.join(tmp, 'b.ld')
            with open(a, 'w') as f:
                f.write('INCLUDE "b.ld"\n')
            with open(b, 'w') as f:
                f.write('INCLUDE "a.ld"\n')  # cycle back to a.ld
            result = mr.resolve_includes([a])
            self.assertEqual(result, [a, b])  # each script appears exactly once

    def test_missing_include_target_skipped(self):
        with tempfile.TemporaryDirectory() as tmp:
            a = os.path.join(tmp, 'a.ld')
            with open(a, 'w') as f:
                f.write('INCLUDE "does_not_exist.ld"\n')
            self.assertEqual(mr.resolve_includes([a]), [a])


class Regexes(unittest.TestCase):
    def test_ld_script_extraction_both_forms(self):
        text = 'cc -Wl,--script=a.ld -o out.elf\ncc -T b.ld -o out2.elf\ncc -Tb.ld -o out3.elf\n'
        self.assertEqual(mr.LD_SCRIPT_RE.findall(text), ['a.ld', 'b.ld', 'b.ld'])

    def test_defsym_extraction_both_separators(self):
        text = 'cc -Wl,--defsym=FOO=0x10 -Wl,--defsym,BAR=1 -o out.elf\n'
        self.assertEqual(mr.DEFSYM_RE.findall(text), ['FOO=0x10', 'BAR=1'])


class BuildMembrowseCmd(unittest.TestCase):
    def setUp(self):
        patcher = mock.patch.object(mr.shutil, 'which', return_value=None)
        self.addCleanup(patcher.stop)
        patcher.start()

    def _args(self, elf, **kw):
        base = dict(target_name='board/example', upload=False, ld=None, option='')
        base.update(kw)
        return argparse.Namespace(elf=elf, **base)

    def test_identical_fallback_when_elf_missing(self):
        cmd, key = mr.build_membrowse_cmd(self._args('/no/such/elf'), '')
        self.assertEqual(cmd, ['membrowse', 'report', '--identical'])
        self.assertIsNone(key)

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
            cmd, _key = mr.build_membrowse_cmd(self._args(elf), '')
            self.assertIn('--map-file', cmd)
            self.assertEqual(cmd[cmd.index('--map-file') + 1], elf + '.map')

    def test_defsym_becomes_def_args(self):
        with tempfile.TemporaryDirectory() as tmp:
            elf = os.path.join(tmp, 'x.elf')
            open(elf, 'w').close()
            commands = 'cc -Wl,--defsym=FOO=0x10 -Wl,--defsym,BAR=1 -o x.elf\n'
            cmd, _key = mr.build_membrowse_cmd(self._args(elf), commands)
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
                self._args(elf, option='--json --all-symbols'), '')
            self.assertEqual(cmd[:4], ['membrowse', 'report', '--json', '--all-symbols'])

    def test_upload_requires_key_env(self):
        with tempfile.TemporaryDirectory() as tmp:
            elf = os.path.join(tmp, 'x.elf')
            open(elf, 'w').close()
            env = dict(os.environ)
            env.pop('MEMBROWSE_API_KEY', None)
            with mock.patch.dict(os.environ, env, clear=True):
                with self.assertRaises(SystemExit):
                    mr.build_membrowse_cmd(self._args(elf, upload=True), '')

    def test_upload_appends_key_and_target_name(self):
        with tempfile.TemporaryDirectory() as tmp:
            elf = os.path.join(tmp, 'x.elf')
            open(elf, 'w').close()
            with mock.patch.dict(os.environ, {'MEMBROWSE_API_KEY': 'dummysecret'}):
                cmd, key = mr.build_membrowse_cmd(
                    self._args(elf, upload=True, target_name='board/ex'), '')
            self.assertEqual(key, 'dummysecret')
            self.assertEqual(cmd[cmd.index('--api-key') + 1], 'dummysecret')
            self.assertEqual(cmd[cmd.index('--target-name') + 1], 'board/ex')


class CliKeyHandling(unittest.TestCase):
    """End-to-end CLI tests: real `python3 tools/membrowse_report.py` subprocess,
    with a stub `membrowse` injected into PATH so the real tool is never invoked."""

    def _stub_path_dir(self, tmp):
        stub_dir = os.path.join(tmp, 'stubbin')
        os.mkdir(stub_dir)
        stub = os.path.join(stub_dir, 'membrowse')
        with open(stub, 'w') as f:
            f.write('#!/usr/bin/env python3\n'
                     'import sys\n'
                     'print("STUB_ARGV:" + " ".join(sys.argv[1:]))\n')
        os.chmod(stub, 0o755)
        return stub_dir

    def _run(self, tmp, extra_args, env_extra):
        elf = os.path.join(tmp, 'fake.elf')
        open(elf, 'w').close()
        stub_dir = self._stub_path_dir(tmp)
        env = dict(os.environ)
        env['PATH'] = stub_dir + os.pathsep + env.get('PATH', '')
        env.pop('MEMBROWSE_API_KEY', None)
        env.update(env_extra)
        args = [sys.executable, SCRIPT, '--build-dir', tmp, '--ninja', 'true',
                '--target', 'x', '--elf', elf, '--target-name', 'board/example'] + extra_args
        return subprocess.run(args, capture_output=True, text=True, env=env)

    def test_missing_api_key_errors_cleanly_no_traceback(self):
        with tempfile.TemporaryDirectory() as tmp:
            r = self._run(tmp, ['--upload'], {})
            self.assertNotEqual(r.returncode, 0)
            self.assertNotIn('Traceback', r.stderr)
            self.assertIn('MEMBROWSE_API_KEY', r.stderr)

    def test_upload_redacts_key_in_logged_line(self):
        with tempfile.TemporaryDirectory() as tmp:
            r = self._run(tmp, ['--upload'], {'MEMBROWSE_API_KEY': 'dummysecret'})
            self.assertEqual(r.returncode, 0, r.stderr)
            logged_line = r.stdout.splitlines()[0]
            self.assertNotIn('dummysecret', logged_line)
            self.assertIn('***', logged_line)

    def test_upload_passes_real_key_to_membrowse(self):
        with tempfile.TemporaryDirectory() as tmp:
            r = self._run(tmp, ['--upload'], {'MEMBROWSE_API_KEY': 'dummysecret'})
            self.assertEqual(r.returncode, 0, r.stderr)
            stub_line = [l for l in r.stdout.splitlines() if l.startswith('STUB_ARGV:')]
            self.assertTrue(stub_line, r.stdout)
            self.assertIn('dummysecret', stub_line[0])

    def test_local_report_no_key_needed(self):
        with tempfile.TemporaryDirectory() as tmp:
            r = self._run(tmp, [], {})
            self.assertEqual(r.returncode, 0, r.stderr)
            self.assertIn('STUB_ARGV:', r.stdout)


if __name__ == '__main__':
    unittest.main()
