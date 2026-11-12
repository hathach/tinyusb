"""tools/hook_steps.py: the shell-free runner of the multi-step pre-commit hooks.

Steps run in order and the first failure stops the hook with its exit code, a leading
`@DIR` runs a step in ROOT/DIR, `python`/`python3` run as the hook's own interpreter
(a language: python hook's venv), and a command that cannot start is 127.
Stdlib only: the module is imported by path, its steps are real child processes
except where subprocess.run is mocked to read the argv it would get."""
import contextlib
import importlib.util
import io
import os
import pathlib
import shlex
import sys
import tempfile
import unittest
from unittest import mock

ROOT = pathlib.Path(__file__).resolve().parents[3]
_spec = importlib.util.spec_from_file_location('hook_steps', ROOT / 'tools' / 'hook_steps.py')
hs = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(hs)

# Writes argv[1] (cwd when argv[2] is 'cwd', else empty) and exits with int(argv[2]) or 0.
MARK = ("import os, sys\n"
        "cwd = sys.argv[2] == 'cwd'\n"
        "with open(sys.argv[1], 'w', encoding='utf-8') as f:\n"
        "    f.write(os.getcwd() if cwd else '')\n"
        "sys.exit(0 if cwd else int(sys.argv[2]))\n")


def step(path, arg, interp='python', at=None):
    """One hook step running MARK; forward slashes so shlex keeps a Windows path whole."""
    words = [interp, '-c', MARK, str(path).replace('\\', '/'), str(arg)]
    return (f'@{at} ' if at else '') + shlex.join(words)


def same(a, b):
    return os.path.normcase(os.path.realpath(a)) == os.path.normcase(os.path.realpath(b))


class StopsAtFirstFailure(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.marks = [pathlib.Path(self.tmp.name, f'm{i}') for i in range(3)]

    def test_returns_first_nonzero_rc_and_skips_later_steps(self):
        rc = hs.main([step(self.marks[0], 0), step(self.marks[1], 3), step(self.marks[2], 0)])
        self.assertEqual(rc, 3)
        self.assertTrue(self.marks[0].exists(), 'the step before the failure ran')
        self.assertTrue(self.marks[1].exists(), 'the failing step ran')
        self.assertFalse(self.marks[2].exists(), 'a step after the failure ran')

    def test_all_steps_pass_returns_zero(self):
        rc = hs.main([step(m, 0) for m in self.marks])
        self.assertEqual(rc, 0)
        for m in self.marks:
            self.assertTrue(m.exists(), f'{m.name} did not run')

    def test_no_steps_is_success(self):
        self.assertEqual(hs.main([]), 0)


class WorkingDirectory(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.out = pathlib.Path(self.tmp.name, 'cwd')

    def test_at_dir_runs_in_root_dir(self):
        self.assertEqual(hs.main([step(self.out, 'cwd', at='test/hil')]), 0)
        self.assertTrue(same(self.out.read_text(encoding='utf-8'), ROOT / 'test' / 'hil'))

    def test_no_at_dir_runs_in_root(self):
        self.assertEqual(hs.main([step(self.out, 'cwd')]), 0)
        self.assertTrue(same(self.out.read_text(encoding='utf-8'), ROOT))

    def test_root_is_the_repo_root(self):
        self.assertTrue(same(hs.ROOT, ROOT))


class CommandResolution(unittest.TestCase):
    def argv_of(self, s):
        with mock.patch.object(hs.subprocess, 'run') as run:
            run.return_value.returncode = 0
            self.assertEqual(hs.run(s), 0)
        run.assert_called_once()
        return run.call_args

    def test_python_and_python3_become_this_interpreter(self):
        for interp in ('python', 'python3'):
            with self.subTest(interp=interp):
                call = self.argv_of(f'{interp} -m unittest -q x')
                self.assertEqual(call.args[0], [sys.executable, '-m', 'unittest', '-q', 'x'])
                self.assertTrue(same(call.kwargs['cwd'], ROOT))

    def test_python_after_at_dir_becomes_this_interpreter(self):
        call = self.argv_of('@test/hil/test python3 -m unittest -q test_hil_util.BottomLayer')
        self.assertEqual(call.args[0][0], sys.executable)
        self.assertTrue(same(call.kwargs['cwd'], ROOT / 'test' / 'hil' / 'test'))

    def test_only_the_command_word_is_substituted(self):
        call = self.argv_of('ceedling python3')
        self.assertEqual(call.args[0][1], 'python3')

    def test_other_commands_resolve_on_path(self):
        with mock.patch.object(hs.shutil, 'which', return_value='/opt/bin/ceedling') as which:
            call = self.argv_of('@test/unit-test ceedling test:all')
        which.assert_called_once_with('ceedling')
        self.assertEqual(call.args[0], ['/opt/bin/ceedling', 'test:all'])
        self.assertTrue(same(call.kwargs['cwd'], ROOT / 'test' / 'unit-test'))


class Errors(unittest.TestCase):
    def test_missing_command_is_127(self):
        err = io.StringIO()
        with contextlib.redirect_stderr(err):
            rc = hs.main(['hook-steps-no-such-command-4550907096 --flag'])
        self.assertEqual(rc, 127)
        self.assertIn('hook-steps-no-such-command-4550907096', err.getvalue())

    def test_missing_command_stops_later_steps(self):
        with tempfile.TemporaryDirectory() as tmp:
            mark = pathlib.Path(tmp, 'm')
            with contextlib.redirect_stderr(io.StringIO()):
                rc = hs.main(['hook-steps-no-such-command-4550907096', step(mark, 0)])
            self.assertEqual(rc, 127)
            self.assertFalse(mark.exists())

    def test_empty_step_exits(self):
        for s in ('', '   ', '@test/hil'):
            with self.subTest(step=s), mock.patch.object(hs.subprocess, 'run') as run:
                with self.assertRaises(SystemExit) as cm:
                    hs.main([s])
                run.assert_not_called()
                self.assertIn('empty step', str(cm.exception.code))


if __name__ == '__main__':
    unittest.main()
