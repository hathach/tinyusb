"""Tests for the build-doc skill's build_doc.py: it runs sphinx-build strictly
by default, forwards the flags it documents, and propagates the build's exit
code without inventing success. The docs and build dirs are temp dirs, never
the checkout's docs/_build, which --clean removes."""
import importlib.util
import io
import os
import shutil
import subprocess
import sys
import tempfile
import types
import unittest
from contextlib import redirect_stderr, redirect_stdout
from pathlib import Path
from unittest import mock

SCRIPT = Path(__file__).resolve().parents[1] / 'skills' / 'build-doc' / 'scripts' / 'build_doc.py'
spec = importlib.util.spec_from_file_location('build_doc', SCRIPT)
build_doc = importlib.util.module_from_spec(spec)
spec.loader.exec_module(build_doc)


class BuildDocTest(unittest.TestCase):
    def setUp(self):
        tmp = tempfile.TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        self.docs = Path(tmp.name) / 'docs'
        self.build = self.docs / '_build'
        for name, value in (('DOCS', self.docs), ('BUILD', self.build)):
            patch = mock.patch.object(build_doc, name, value)
            patch.start()
            self.addCleanup(patch.stop)

    def run_main(self, args, rc=0):
        """build_doc.py's main() with its sphinx-build call recorded and answered with rc, in-process:
        a stub on PATH that a host cannot execute would let the real sphinx-build run."""
        calls = []
        out, err = io.StringIO(), io.StringIO()

        def call(cmd):
            calls.append(cmd)
            self.build_existed = self.build.exists()
            return rc
        with mock.patch.object(build_doc.subprocess, 'call', call), \
                mock.patch.object(sys, 'argv', [str(SCRIPT), *args]), redirect_stdout(out), redirect_stderr(err):
            try:
                code = build_doc.main()
            except SystemExit as e:
                code = e.code
        if calls:
            assert calls[0][0] == 'sphinx-build', calls
        return types.SimpleNamespace(returncode=code, stdout=out.getvalue(), stderr=err.getvalue()), \
            calls[0][1:] if calls else None

    def test_default_is_html_strict_from_docs_into_docs_build(self):
        r, argv = self.run_main([])
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertEqual(argv, ['-b', 'html', '-W', str(self.docs), str(self.build)])
        self.assertIn('Docs built:', r.stdout)

    def test_no_strict_drops_W_and_nothing_else(self):
        r, argv = self.run_main(['--no-strict'])
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertNotIn('-W', argv)
        self.assertEqual(argv[:2], ['-b', 'html'])

    def test_clean_removes_the_build_dir_first(self):
        (self.build / 'stale').mkdir(parents=True)
        r, argv = self.run_main(['-c'])
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertFalse(self.build_existed, 'removed before sphinx-build ran')

    def test_sphinx_failure_is_the_exit_code_and_no_success_line(self):
        r, argv = self.run_main([], rc=2)
        self.assertEqual(r.returncode, 2)
        self.assertIsNotNone(argv)
        self.assertNotIn('Docs built:', r.stdout)

    def test_unknown_flag_is_refused_before_sphinx_runs(self):
        r, argv = self.run_main(['-W'])
        self.assertEqual(r.returncode, 2)
        self.assertIsNone(argv)


@unittest.skipIf(os.name == 'nt', 'the stub is a #! script Windows cannot exec: a miss would run the real sphinx-build')
class ProcessTest(unittest.TestCase):
    """The script as a process, copied into a temp checkout so its docs/_build is a temp dir:
    main()'s status is the exit status."""

    def test_the_exit_status_is_mains(self):
        for rc in (0, 3):
            with self.subTest(rc=rc), tempfile.TemporaryDirectory() as d:
                script = Path(d) / '.claude' / 'skills' / 'build-doc' / 'scripts' / 'build_doc.py'
                script.parent.mkdir(parents=True)
                shutil.copy(SCRIPT, script)
                stub = Path(d) / 'bin' / 'sphinx-build'
                stub.parent.mkdir()
                stub.write_text(f'#!/bin/sh\nprintf "%s\\n" "$@" > {d}/argv\nexit {rc}\n')
                stub.chmod(0o755)
                env = dict(os.environ, PATH=f'{stub.parent}{os.pathsep}{os.environ["PATH"]}')
                r = subprocess.run([sys.executable, str(script)], capture_output=True, text=True, env=env, timeout=60)
                self.assertEqual(r.returncode, rc, r.stderr)
                self.assertEqual((Path(d) / 'argv').read_text().split('\n')[:-1],
                                 ['-b', 'html', '-W', str(Path(d).resolve() / 'docs'), str(Path(d).resolve() / 'docs' / '_build')])
                self.assertEqual('Docs built:' in r.stdout, rc == 0)


if __name__ == '__main__':
    unittest.main()
