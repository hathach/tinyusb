#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# The hil_lock adapter: callers get agentrc's board-lock primitive at the commit tools/get_deps.py
# pins (agentrc's tests/test_hil_lock.py covers the primitive itself), hil_test refuses a board
# whose lock is unavailable before flashing, `hold --all` takes the roster, and a missing
# tools/agentrc refuses at import (test_hil_agentrc.py covers the loader's pin check). Every
# lock lives in a temp dir under a name no rig uses. Run directly:
#   python3 test/hil/test/test_hil_lock.py
import importlib.util
import json
import os
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

HIL_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
REPO = os.path.dirname(os.path.dirname(HIL_DIR))
sys.path.insert(0, HIL_DIR)

from usbtest_harness import patch   # noqa: E402 - stubs pyserial before hil_test imports it
import hil_flash         # noqa: E402
import hil_test          # noqa: E402
from helper import hil_lock   # noqa: E402

BOARD = 'hil-lock-selftest-board'
PROVIDER = 'tools/agentrc/skills/hil/scripts/hil_lock.py'
FETCH = 'python3 tools/get_deps.py tools/agentrc'


def pinned_commit():
    spec = importlib.util.spec_from_file_location('get_deps_under_test', os.path.join(REPO, 'tools/get_deps.py'))
    get_deps = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(get_deps)
    return get_deps.deps_optional['tools/agentrc'][1]


class LockDir(unittest.TestCase):
    def setUp(self):
        td = tempfile.TemporaryDirectory()
        self.addCleanup(td.cleanup)
        self.dir = td.name
        patch(self, hil_lock, 'BOARD_LOCK_DIR', self.dir)
        env = mock.patch.dict(os.environ)
        env.start()
        self.addCleanup(env.stop)
        os.environ.pop('HIL_NO_BOARD_LOCK', None)

    def lock_dir_is_a_file(self):
        path = os.path.join(self.dir, 'not-a-dir')
        open(path, 'w').close()
        patch(self, hil_lock, 'BOARD_LOCK_DIR', path)


class RunnerRefusesBeforeFlash(LockDir):
    """hil_test.test_board on an unusable lock dir: the board fails as lock-unavailable and
    returns before anything flashes."""

    def setUp(self):
        super().setUp()
        self.lock_dir_is_a_file()
        self.addCleanup(setattr, hil_test, 'board_wedged', hil_test.board_wedged)
        self.log = []
        patch(self, hil_test, 'log_line', self.log.append)
        patch(self, hil_flash, 'flash_primitive', lambda *a: self.fail('flashed'))
        self.board = {'name': BOARD, 'uid': 'ABC123', 'flasher': {'name': 'jlink', 'uid': 'P'},
                      'tests': {'only': ['device/cdc_msc']}}

    def test_lock_unavailable_fails_the_board_unflashed(self):
        name, err, failed, rows, _ = hil_test.test_board(self.board)
        self.assertEqual((name, err, failed), (BOARD, 1, []))
        cells = rows[0][1]
        self.assertEqual(list(cells), [hil_test.hil_report.LOCK_UNAVAILABLE_CELL])
        cell = cells[hil_test.hil_report.LOCK_UNAVAILABLE_CELL]
        self.assertTrue(cell.startswith(f'{hil_test.hil_report.REPORT_CELL["fail"]} board lock unavailable: '),
                        cell)
        self.assertEqual(hil_test.hil_report.cell_state(cell), 'fail')
        self.assertIn(f'{hil_test.STATUS_FAILED}: board lock unavailable: ', self.log[0])

    def test_contention_keeps_its_own_cell(self):
        def held(name):
            raise RuntimeError('board locked: {"pid": 1}')
        patch(self, hil_lock, 'acquire_board_lock', held)
        _, err, _, rows, _ = hil_test.test_board(self.board)
        self.assertEqual(err, 1)
        self.assertEqual(rows[0][1], {hil_test.hil_report.LOCKED_CELL: 'fail'})


class Adapter(LockDir):
    def test_callers_get_the_pinned_provider(self):
        self.assertEqual(os.path.realpath(hil_lock.__file__), os.path.realpath(os.path.join(REPO, PROVIDER)))
        self.assertIs(hil_test.hil_lock, hil_lock)

    def test_a_patched_lock_dir_reaches_the_provider(self):
        self.assertEqual(hil_lock.lock_path(BOARD), os.path.join(self.dir, f'{BOARD}.lock'))


class PinnedDependency(unittest.TestCase):
    """The adapter in a scratch tree: refused at import through helper/agentrc.py unless
    tools/agentrc is at the pin; then its CLI runs, against a lock dir of its own."""

    def setUp(self):
        td = tempfile.TemporaryDirectory()
        self.addCleanup(td.cleanup)
        self.root = td.name
        for rel in ('test/hil/helper/__init__.py', 'test/hil/helper/agentrc.py', 'test/hil/helper/hil_lock.py',
                    'tools/get_deps.py'):
            os.makedirs(os.path.join(self.root, os.path.dirname(rel)), exist_ok=True)
            shutil.copy(os.path.join(REPO, rel), os.path.join(self.root, rel))
        self.pin = pinned_commit()

    def provide(self, head, lock_dir='/tmp/tinyusb-hil-locks'):
        os.makedirs(os.path.join(self.root, os.path.dirname(PROVIDER)), exist_ok=True)
        with open(os.path.join(REPO, PROVIDER)) as f:
            text = f.read()
        moved = text.replace("BOARD_LOCK_DIR = '/tmp/tinyusb-hil-locks'", f'BOARD_LOCK_DIR = {lock_dir!r}')
        self.assertTrue(lock_dir == '/tmp/tinyusb-hil-locks' or moved != text, 'BOARD_LOCK_DIR line moved')
        with open(os.path.join(self.root, PROVIDER), 'w') as f:
            f.write(moved)
        os.makedirs(os.path.join(self.root, 'tools/agentrc/.git'), exist_ok=True)
        with open(os.path.join(self.root, 'tools/agentrc/.git/HEAD'), 'w') as f:
            f.write(head + '\n')

    def load(self):
        code = 'import sys; sys.path.insert(0, "test/hil"); from helper import hil_lock; print(hil_lock.__file__)'
        return subprocess.run([sys.executable, '-c', code], cwd=self.root, capture_output=True, text=True, timeout=30)

    def cli(self, *argv):
        # -P: the script must find helper/agentrc.py without the script-dir sys.path entry
        r = subprocess.run([sys.executable, '-P', 'test/hil/helper/hil_lock.py', *argv], cwd=self.root,
                           capture_output=True, text=True, timeout=30)
        return r.returncode, r.stdout + r.stderr

    def assert_refused(self, r):
        self.assertNotEqual(r.returncode, 0)
        last = r.stderr.strip().splitlines()[-1]
        self.assertTrue(last.startswith('ImportError: '), last)
        self.assertIn(self.pin, last)
        self.assertIn(FETCH, last)

    def test_missing_dependency_refuses(self):
        self.assert_refused(self.load())

    def test_hold_all_takes_the_roster_with_boards_skip(self):
        lock_dir = os.path.join(self.root, 'locks')
        self.provide(self.pin, lock_dir)
        names = [f'hil-lock-selftest-{n}' for n in ('a', 'b', 'parked', 'other')]
        with open(os.path.join(self.root, 'test/hil/tinyusb.json'), 'w') as f:
            json.dump({'boards': [{'name': names[0]}, {'name': names[1]}], 'boards-skip': [{'name': names[2]}]}, f)
        other = os.path.join(self.root, 'other.json')
        with open(other, 'w') as f:
            json.dump({'boards': [{'name': names[3]}]}, f)
        self.addCleanup(self.cli, 'release', '--all')
        self.assertEqual(self.cli('hold', '--all', '--reason', 'selftest'), (0, f'held: {", ".join(names[:3])}\n'))
        self.assertEqual(self.cli('hold', '--all', '--config', other, '--reason', 'selftest'),
                         (0, f'held: {names[3]}\n'))
        rc, out = self.cli('status')
        self.assertEqual(sorted(line.split(':')[0] for line in out.splitlines()), sorted(names))
        rc, _ = self.cli('release', '--all')
        self.assertEqual(rc, 0)
        self.assertEqual(self.cli('status'), (0, 'no locks\n'))


if __name__ == '__main__':
    unittest.main()
