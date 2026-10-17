#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# helper/agentrc.py, the one loader for agentrc's shared scripts, in a scratch tree: it refuses
# with the fetch command and the pin in the last line a caller's traceback prints unless
# tools/agentrc is at the pin and holds the file, and a module whose exec fails stays out of
# sys.modules. Run directly:
#   python3 test/hil/test/test_hil_agentrc.py
import importlib.util
import os
import shutil
import subprocess
import sys
import tempfile
import unittest

HIL_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
REPO = os.path.dirname(os.path.dirname(HIL_DIR))
RTT_PY = 'skills/rtt/scripts/rtt.py'
FETCH = 'python3 tools/get_deps.py tools/agentrc'


def pinned_commit():
    spec = importlib.util.spec_from_file_location('get_deps_under_test', os.path.join(REPO, 'tools/get_deps.py'))
    get_deps = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(get_deps)
    return get_deps.deps_optional['tools/agentrc'][1]


class PinnedDependency(unittest.TestCase):
    def setUp(self):
        td = tempfile.TemporaryDirectory()
        self.addCleanup(td.cleanup)
        self.root = td.name
        for rel in ('test/hil/helper/__init__.py', 'test/hil/helper/agentrc.py', 'tools/get_deps.py'):
            os.makedirs(os.path.join(self.root, os.path.dirname(rel)), exist_ok=True)
            shutil.copy(os.path.join(REPO, rel), os.path.join(self.root, rel))
        self.pin = pinned_commit()

    def provide(self, head, files=None):
        """tools/agentrc with .git/HEAD at `head`; each file copied from the real checkout, or
        written from its text."""
        for rel, text in (files or {RTT_PY: None}).items():
            path = os.path.join(self.root, 'tools/agentrc', rel)
            os.makedirs(os.path.dirname(path), exist_ok=True)
            if text is None:
                shutil.copy(os.path.join(REPO, 'tools/agentrc', rel), path)
            else:
                with open(path, 'w') as f:
                    f.write(text)
        os.makedirs(os.path.join(self.root, 'tools/agentrc/.git'), exist_ok=True)
        with open(os.path.join(self.root, 'tools/agentrc/.git/HEAD'), 'w') as f:
            f.write(head + '\n')

    def load(self, rel=RTT_PY, then=''):
        code = ('import sys; sys.path.insert(0, "test/hil"); from helper import agentrc; '
                f'print(agentrc.load({rel!r}, "under_test").__file__)' + then)
        return subprocess.run([sys.executable, '-c', code], cwd=self.root, capture_output=True, text=True, timeout=30)

    def assert_refused(self, r, *also):
        self.assertNotEqual(r.returncode, 0)
        last = r.stderr.strip().splitlines()[-1]
        self.assertTrue(last.startswith('ImportError: '), last)
        for s in (self.pin, FETCH, *also):
            self.assertIn(s, last)

    def test_missing_dependency_refuses(self):
        self.assert_refused(self.load())

    def test_off_pin_dependency_refuses(self):
        for head in ('0' * 40, 'ref: refs/heads/main'):
            with self.subTest(head=head):
                self.provide(head)
                self.assert_refused(self.load())

    def test_a_file_missing_at_the_pin_refuses(self):
        self.provide(self.pin)
        self.assert_refused(self.load('skills/none/scripts/none.py'), 'skills/none/scripts/none.py')

    def test_a_failed_exec_leaves_no_module_registered(self):
        self.provide(self.pin, {'broken.py': 'raise RuntimeError("broken")\n'})
        code = ('import sys; sys.path.insert(0, "test/hil"); from helper import agentrc\n'
                'try:\n    agentrc.load("broken.py", "under_test")\n'
                'except RuntimeError:\n    print("under_test" in sys.modules)\n')
        r = subprocess.run([sys.executable, '-c', code], cwd=self.root, capture_output=True, text=True, timeout=30)
        self.assertEqual((r.returncode, r.stdout), (0, 'False\n'), r.stderr)

    def test_the_pinned_commit_loads_and_registers(self):
        self.provide(self.pin)
        r = self.load(then='; print(sys.modules["under_test"].RttError.__module__)')
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertEqual(r.stdout, f'{os.path.join(self.root, "tools/agentrc", RTT_PY)}\nunder_test\n')


if __name__ == '__main__':
    unittest.main()
