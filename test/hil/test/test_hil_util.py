#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Unit tests for hil_util.run_cmd's binary/split_stderr/quiet modes — real subprocesses, no
# hardware. Stdlib + hil_util only (hil_util is stdlib-only), so the pre-commit hil-test
# hook can run this on GitHub's bare runner. Run directly:
#   python3 test/hil/test/test_hil_util.py
import io
import os
import shutil
import tempfile
import sys
import time
import unittest
from contextlib import redirect_stdout
from pathlib import Path

# the module under test lives in the parent dir's helper/ package
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from helper import hil_util


@unittest.skipIf(os.name == 'nt', 'POSIX shell commands')
class RunCmdModes(unittest.TestCase):
    def test_default_mode_unchanged(self):
        r = hil_util.run_cmd('printf out; printf err >&2')
        self.assertEqual(r.returncode, 0)
        self.assertIsInstance(r.stdout, str)
        # stderr merged into stdout, as every existing caller expects
        self.assertIn('out', r.stdout)
        self.assertIn('err', r.stdout)

    def test_binary_stdout_is_exact_bytes(self):
        # \xff is not valid UTF-8: text mode would mangle it via errors='replace'
        r = hil_util.run_cmd(r"printf 'a\377\000b'", binary=True)
        self.assertEqual(r.returncode, 0)
        self.assertEqual(r.stdout, b'a\xff\x00b')

    def test_split_stderr_keeps_stdout_clean(self):
        r = hil_util.run_cmd('printf out; printf err >&2', split_stderr=True)
        self.assertEqual(r.returncode, 0)
        self.assertEqual(r.stdout, 'out')
        self.assertEqual(r.stderr, 'err')

    def test_binary_split_stderr_timeout_returns_124(self):
        t0 = time.monotonic()
        r = hil_util.run_cmd(r"printf 'p\377re'; printf warn >&2; sleep 30",
                              binary=True, split_stderr=True, timeout=1)
        self.assertEqual(r.returncode, 124)
        # killpg + bounded communicate: well under sleep 30
        self.assertLess(time.monotonic() - t0, 15)
        self.assertIn(b'p\xffre', r.stdout or b'')
        # stderr collected before the timeout must survive the kill
        self.assertIn(b'warn', r.stderr or b'')

    def test_text_mode_timeout_stdout_stays_str(self):
        r = hil_util.run_cmd('sleep 30', timeout=1)
        self.assertEqual(r.returncode, 124)
        # a text-mode caller must never get bytes back, even empty
        self.assertIsInstance(r.stdout, str)

    def test_failed_banner_includes_split_stderr(self):
        # with split_stderr the diagnostic is in .stderr; the banner must not go blank.
        # The text travels via env, not the command string — the banner title echoes the
        # command, which would make a literal assertion pass vacuously.
        os.environ['RUN_CMD_TEST_ERR'] = 'diagnostic-xyzzy'
        self.addCleanup(os.environ.pop, 'RUN_CMD_TEST_ERR', None)
        cap = io.StringIO()
        with redirect_stdout(cap):
            r = hil_util.run_cmd('printf "$RUN_CMD_TEST_ERR" >&2; exit 3', split_stderr=True)
        self.assertEqual(r.returncode, 3)
        self.assertIn('COMMAND FAILED', cap.getvalue())
        self.assertIn('diagnostic-xyzzy', cap.getvalue())

    def test_no_group_markers_when_stdout_is_captured(self):
        # GitHub folds ::group:: only at line start of the JOB's real stdout. Pool
        # workers run tests under redirect_stdout and compact the capture into one
        # row line, where the markers land mid-line and render as literal noise.
        saved_ci = os.environ.get('CI')  # pre-exists on GitHub runners: restore, not pop
        os.environ['CI'] = '1'
        self.addCleanup(lambda: os.environ.update({'CI': saved_ci}) if saved_ci is not None
                        else os.environ.pop('CI', None))
        cap = io.StringIO()
        with redirect_stdout(cap):
            r = hil_util.run_cmd('printf boom; exit 3')
        self.assertEqual(r.returncode, 3)
        self.assertIn('COMMAND FAILED', cap.getvalue())
        self.assertNotIn('::group::', cap.getvalue())
        self.assertNotIn('::endgroup::', cap.getvalue())

    def test_quiet_suppresses_failed_banner(self):
        # retry-loop callers report failures themselves; per-poll banners are noise
        cap = io.StringIO()
        with redirect_stdout(cap):
            r = hil_util.run_cmd('printf boom >&2; exit 3', quiet=True)
        self.assertEqual(r.returncode, 3)
        self.assertNotIn('COMMAND FAILED', cap.getvalue())


class BottomLayer(unittest.TestCase):
    def test_bad_timeout_env_falls_back(self):
        # ci_select (the PR-diff selector) imports hil_util for the example rosters;
        # a malformed HIL_CMD_TIMEOUT must not crash the selector at import and knock
        # CI back to the full-matrix fallback
        import subprocess
        r = subprocess.run(
            [sys.executable, '-c', 'from helper import hil_util; print(hil_util.CMD_TIMEOUT)'],
            cwd=os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
            env={**os.environ, 'HIL_CMD_TIMEOUT': 'bogus'},
            capture_output=True, text=True, timeout=30)
        self.assertEqual(r.returncode, 0, r.stderr)
        # the warning must NOT be on stdout: ci_select's stdout is machine-read JSON
        self.assertEqual(r.stdout.strip(), '180')
        self.assertIn('warning', r.stderr)  # but a silent fallback hides the misconfiguration

    def test_tinyusb_root_is_the_repo_root(self):
        # the constant is derived from __file__ parents[N]; moving hil_util.py without
        # adjusting N silently re-points every firmware/build path (it happened)
        self.assertTrue((hil_util.TINYUSB_ROOT / 'examples').is_dir(), hil_util.TINYUSB_ROOT)
        self.assertTrue((hil_util.TINYUSB_ROOT / 'test' / 'hil').is_dir(), hil_util.TINYUSB_ROOT)

    def test_hil_util_is_a_single_module_instance(self):
        # helper modules must be imported via the helper package everywhere: a plain
        # `import hil_util` from inside helper/ creates a SECOND module object, and
        # state like `verbose` set on one copy never reaches the other
        import hil_flash
        from helper import hil_pool_check
        self.assertIs(hil_flash.hil_util, hil_util)
        self.assertIs(hil_pool_check.hil_util, hil_util)
        self.assertIs(hil_pool_check.hil_flash, hil_flash)

    def test_bare_runner_modules_stay_stdlib_only(self):
        # hil_examples.py used to make this structural (a list of strings cannot grow a
        # dependency); with the rosters folded into hil_util the invariant needs teeth:
        # everything the bare GitHub runner imports (selector + this suite) must stay
        # stdlib + local. Adding pyserial/pymtp here breaks ci_select on CI.
        import ast
        hil_dir = Path(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
        # ONLY the modules the bare runner can import -- not every stem in the tree.
        # Globbing the directory allowed `import pymtp` (and hil_test, usbtest,
        # mtp_test) through, so the pymtp case this test names could never fail: that
        # module runs ctypes.CDLL(find_library('mtp')) at import and raises where there
        # is no libmtp, taking ci_select down with it.
        local = {'helper', 'hil_util', 'ci_select', 'hil_flash',
                 'hil_health', 'hil_lock', 'hil_pool_check', 'build', 'build_utils'}
        allowed = set(sys.stdlib_module_names) | local
        # hil_pool_check included: test_hil_util_is_a_single_module_instance imports it
        # on the bare runner, and its `import serial` is function-local for exactly
        # this reason -- hoisting it must fail HERE, not on every PR's pre-commit CI
        for mod in ('helper/hil_util', 'hil_flash', '../../tools/ci_select',
                    'helper/hil_health', 'helper/hil_lock', 'helper/hil_pool_check',
                    '../../tools/build', '../../tools/build_utils'):
            tree = ast.parse((hil_dir / f'{mod}.py').read_text())
            # module level only: a deferred import inside a function cannot break
            # importability (hil_pool_check keeps `import serial` function-local
            # for exactly that reason)
            for node in tree.body:
                roots = []
                if isinstance(node, ast.Import):
                    roots = [a.name.split('.')[0] for a in node.names]
                elif isinstance(node, ast.ImportFrom) and node.module:
                    roots = [node.module.split('.')[0]]
                for root in roots:
                    self.assertIn(root, allowed,
                                  f'{mod}.py imports {root}, not stdlib/local - breaks the bare CI runner')


class BoundedReadBookkeeping(unittest.TestCase):
    """Two ways the strand accounting lied, both of which cost a blindness credit -- and
    the process goes blind after four."""

    def test_a_value_that_arrived_at_the_deadline_is_not_a_strand(self):
        """join() returns, is_alive() is still True, but the reader HAS deposited its
        value. read_sysfs booked a strand from is_alive() alone, so a merely-slow healthy
        read was memoised as unreadable forever. bounded_open already gets this right."""
        import threading, time as _t
        before = hil_util._sysfs_stuck
        self.addCleanup(setattr, hil_util, '_sysfs_stuck', before)
        real_thread = threading.Thread

        class Lingering(real_thread):
            """Deposits the value, then outlives the join by a hair."""
            def run(self):
                super().run()
                _t.sleep(0.6)          # still alive when join(grace) returns

        self.addCleanup(setattr, threading, 'Thread', real_thread)
        threading.Thread = Lingering
        with tempfile.NamedTemporaryFile('w', suffix='_attr', delete=False) as fh:
            fh.write('cafe\n')
            path = fh.name
        self.addCleanup(os.unlink, path)
        hil_util.read_sysfs(path, grace=0.2)
        self.assertEqual(hil_util._sysfs_stuck, before,
                         'a value that arrived was still counted as a strand')

    def test_bounded_open_does_not_re_strand_a_known_path(self):
        """Same rule read_sysfs has: re-opening a path known to hang costs another thread,
        another fd and another blindness credit to learn what we already know. The printer
        test re-opens ONE lp node on every retry."""
        d = tempfile.mkdtemp()
        self.addCleanup(shutil.rmtree, d, True)
        fifo = os.path.join(d, 'lp0')
        os.mkfifo(fifo)                       # open() blocks: no writer, ever
        self.addCleanup(setattr, hil_util, '_sysfs_stuck', hil_util._sysfs_stuck)
        self.addCleanup(setattr, hil_util, '_sysfs_stranded', dict(hil_util._sysfs_stranded))
        before = hil_util._sysfs_stuck
        for _ in range(3):
            hil_util.bounded_open(fifo, os.O_WRONLY, 0.3)
        self.assertLessEqual(hil_util._sysfs_stuck - before, 1,
                             'each retry spent another blindness credit on the same path')


class RunAlongsideKeepsStderrOffThePayload(unittest.TestCase):
    """test_device_printer_to_cdc byte-compares run_alongside's stdout against the payload
    it wrote. Merging stderr into that stream turns any stray child stderr byte -- a
    PYTHONWARNINGS chirp, a sitecustomize print, a venv .pth deprecation -- into
    'CDC->Printer wrong data', sending a maintainer after the printer class driver for an
    interpreter warning. hil_ci.sh runs python3 with no isolating flags."""

    def test_child_stderr_does_not_contaminate_stdout(self):
        from helper import hil_util
        argv = [sys.executable, '-c',
                'import sys; sys.stderr.write("noise\\n"); sys.stdout.write("PAYLOAD")']
        r = hil_util.run_alongside(argv, lambda: time.sleep(0.2), timeout=20)
        self.assertEqual(r.returncode, 0)
        self.assertEqual(r.stdout, b'PAYLOAD',
                         'child stderr leaked into the payload stream')


if __name__ == '__main__':
    unittest.main()
