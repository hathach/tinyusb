"""Tests for the build-doc skill's build_doc.py: it runs sphinx-build strictly
by default, forwards the flags it documents, and propagates the build's exit
code without inventing success."""
import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

SCRIPT = Path(__file__).resolve().parents[1] / 'skills' / 'build-doc' / 'scripts' / 'build_doc.py'
ROOT = Path(__file__).resolve().parents[2]


def run(args, rc=0):
    """Run build_doc.py with a stub sphinx-build on PATH that logs its argv and exits rc."""
    with tempfile.TemporaryDirectory() as d:
        stub = Path(d) / 'sphinx-build'
        log = Path(d) / 'argv'
        stub.write_text(f'#!/bin/sh\nprintf "%s\\n" "$@" > {log}\nexit {rc}\n')
        stub.chmod(0o755)
        env = dict(os.environ, PATH=f'{d}:{os.environ["PATH"]}')
        r = subprocess.run([sys.executable, str(SCRIPT), *args], capture_output=True, text=True, env=env)
        argv = log.read_text().split('\n')[:-1] if log.exists() else None
    return r, argv


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
