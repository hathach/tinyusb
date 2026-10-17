"""Tests for the build-doc skill's build_doc.py: it runs sphinx-build strictly
by default, forwards the flags it documents, and propagates the build's exit
code without inventing success."""
import importlib.util
import io
import sys
import types
import unittest
from contextlib import redirect_stderr, redirect_stdout
from pathlib import Path
from unittest import mock

SCRIPT = Path(__file__).resolve().parents[1] / 'skills' / 'build-doc' / 'scripts' / 'build_doc.py'
ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('build_doc', SCRIPT)
build_doc = importlib.util.module_from_spec(spec)
spec.loader.exec_module(build_doc)


def run(args, rc=0):
    """build_doc.py's main() with its sphinx-build call recorded and answered with rc, in-process:
    a stub on PATH that a host cannot execute would let the real sphinx-build run."""
    calls = []
    out, err = io.StringIO(), io.StringIO()
    with mock.patch.object(build_doc.subprocess, 'call', lambda cmd: calls.append(cmd) or rc), \
            mock.patch.object(sys, 'argv', [str(SCRIPT), *args]), redirect_stdout(out), redirect_stderr(err):
        try:
            code = build_doc.main()
        except SystemExit as e:
            code = e.code
    if calls:
        assert calls[0][0] == 'sphinx-build', calls
    return types.SimpleNamespace(returncode=code, stdout=out.getvalue(), stderr=err.getvalue()), \
        calls[0][1:] if calls else None


class BuildDocTest(unittest.TestCase):
    def test_default_is_html_strict_from_docs_into_docs_build(self):
        r, argv = run([])
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertEqual(argv, ['-b', 'html', '-W', str(ROOT / 'docs'), str(ROOT / 'docs' / '_build')])
        self.assertIn('Docs built:', r.stdout)

    def test_no_strict_drops_W_and_nothing_else(self):
        r, argv = run(['--no-strict'])
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertNotIn('-W', argv)
        self.assertEqual(argv[:2], ['-b', 'html'])

    def test_sphinx_failure_is_the_exit_code_and_no_success_line(self):
        r, argv = run([], rc=2)
        self.assertEqual(r.returncode, 2)
        self.assertIsNotNone(argv)
        self.assertNotIn('Docs built:', r.stdout)

    def test_unknown_flag_is_refused_before_sphinx_runs(self):
        r, argv = run(['-W'])
        self.assertEqual(r.returncode, 2)
        self.assertIsNone(argv)


if __name__ == '__main__':
    unittest.main()
