#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Unit tests for hil_util.run_cmd's binary/split_stderr/quiet modes — real subprocesses, no
# hardware. Stdlib + hil_util (stdlib-only) + the agentrc provider helper/usb_observe.py
# hands out, so the pre-commit hil-test hook can run this on GitHub's bare runner once
# tools/agentrc is fetched at the pin. Run directly:
#   python3 test/hil/test/test_hil_util.py
import io
import os
import sys
import time
import unittest
from contextlib import redirect_stdout
from pathlib import Path

# the module under test lives in the parent dir's helper/ package
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from helper import hil_util


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
        # change_impact (the PR-diff selector) imports hil_util for the example rosters;
        # a malformed HIL_CMD_TIMEOUT must not crash the selector at import and knock
        # CI back to the full-matrix fallback
        import subprocess
        r = subprocess.run(
            [sys.executable, '-c', 'from helper import hil_util; print(hil_util.CMD_TIMEOUT)'],
            cwd=os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
            env={**os.environ, 'HIL_CMD_TIMEOUT': 'bogus'},
            capture_output=True, text=True, timeout=30)
        self.assertEqual(r.returncode, 0, r.stderr)
        # the warning must NOT be on stdout: change_impact's stdout is machine-read JSON
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
        # stdlib + local. Adding pyserial/pymtp here breaks change_impact on CI.
        import ast
        hil_dir = Path(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
        # ONLY the modules the bare runner can import -- not every stem in the tree.
        # Globbing the directory allowed `import pymtp` (and hil_test, usbtest,
        # mtp_test) through, so the pymtp case this test names could never fail: that
        # module runs ctypes.CDLL(find_library('mtp')) at import and raises where there
        # is no libmtp, taking change_impact down with it.
        local = {'helper', 'hil_util', 'change_impact', 'hil_flash', 'agentrc',
                 'hil_lock', 'hil_pool_check', 'build', 'build_utils', 'get_deps', 'family_json',
                 'hil_report'}
        allowed = set(sys.stdlib_module_names) | local
        # hil_pool_check included: test_hil_util_is_a_single_module_instance imports it
        # on the bare runner, and its `import serial` is function-local for exactly
        # this reason -- hoisting it must fail HERE, not on every PR's pre-commit CI
        for mod in ('helper/hil_util', 'hil_flash', '../../tools/change_impact',
                    'helper/hil_lock', 'helper/agentrc', 'helper/hil_pool_check',
                    '../../tools/build', '../../tools/build_utils',
                    '../../tools/get_deps', '../../tools/family_json',
                    # the selection contract CI calls: check_build.py --select-only
                    '../../.claude/skills/build/scripts/check_build', 'helper/hil_report'):
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


class RunAlongsideKeepsStderrOffThePayload(unittest.TestCase):
    """test_device_printer_to_cdc byte-compares run_alongside's stdout against the payload
    it wrote. Merging stderr into that stream turns any stray child stderr byte -- a
    PYTHONWARNINGS chirp, a sitecustomize print, a venv .pth deprecation -- into
    'CDC->Printer wrong data', sending a maintainer after the printer class driver for an
    interpreter warning. hil_remote.py runs python3 with no isolating flags."""

    def test_child_stderr_does_not_contaminate_stdout(self):
        from helper import hil_util
        argv = [sys.executable, '-c',
                'import sys; sys.stderr.write("noise\\n"); sys.stdout.write("PAYLOAD")']
        r = hil_util.run_alongside(argv, lambda: time.sleep(0.2), timeout=20)
        self.assertEqual(r.returncode, 0)
        self.assertEqual(r.stdout, b'PAYLOAD',
                         'child stderr leaked into the payload stream')


class RunCmdCleanupShape(unittest.TestCase):
    """run_cmd's two cleanup paths, asserted structurally.

    Both must kill the process GROUP: start_new_session puts the child in its own group, so
    a flasher run through a shell keeps children a p.kill() cannot reach, and on the
    BaseException path the child never receives the terminal's SIGINT either.

    Structural rather than behavioural on purpose. Driving a real SIGINT into a blocked
    communicate() from a unit test is timing-dependent, and a flaky guard on this block is
    worse than none -- while what actually breaks it is an edit that rebinds a branch. Both
    times this block has been mis-edited, an `else:` ended up attached to the `try` instead
    of the `if` it belonged to, so `p.kill()` ran when killpg had SUCCEEDED and its
    ProcessLookupError masked the caller's exception. That is a shape, and shapes are
    exactly what an AST can pin.
    """

    def _run_cmd_ast(self):
        import ast
        src = Path(hil_util.__file__).read_text()
        return next(n for n in ast.walk(ast.parse(src))
                    if isinstance(n, ast.FunctionDef) and n.name == 'run_cmd')

    def test_no_cleanup_try_has_an_else(self):
        import ast
        for n in ast.walk(self._run_cmd_ast()):
            if isinstance(n, ast.Try) and n.orelse:
                self.fail(f'try/else at line {n.lineno}: an else here runs when the kill '
                          f'SUCCEEDED, and its ProcessLookupError masks the caller\'s '
                          f'exception -- this block has been mis-edited that way twice')

    def test_both_cleanup_paths_kill_the_group(self):
        import ast
        fn = self._run_cmd_ast()
        killers = [getattr(c.func, 'attr', '') for c in ast.walk(fn)
                   if isinstance(c, ast.Call) and getattr(c.func, 'attr', '') in
                   ('killpg', 'kill')]
        self.assertEqual(killers.count('killpg'), 2,
                         'both the timeout and the BaseException path must killpg')
        self.assertEqual(killers.count('kill'), 0,
                         'p.kill() reaches only the direct child; a flasher run through a '
                         'shell keeps grandchildren it cannot touch')

    def test_the_interrupt_path_reraises(self):
        import ast
        fn = self._run_cmd_ast()
        base = [h for n in ast.walk(fn) if isinstance(n, ast.Try) for h in n.handlers
                if isinstance(h.type, ast.Name) and h.type.id == 'BaseException']
        self.assertTrue(base, 'the BaseException cleanup path is gone')
        for h in base:
            self.assertTrue(any(isinstance(x, ast.Raise) for x in ast.walk(h)),
                            'the interrupt path must re-raise, or Ctrl-C is swallowed')


class UsbObserveIsThePinnedModule(unittest.TestCase):
    """helper/usb_observe.py hands callers agentrc's module itself: one strand memo per
    process, and a patched root reaches every function that reads it."""

    def test_callers_get_the_pinned_provider(self):
        from helper import usb_observe
        provider = Path(hil_util.TINYUSB_ROOT) / 'tools' / 'agentrc' / 'skills' / 'hil' / 'scripts' / 'usb_observe.py'
        self.assertEqual(Path(usb_observe.__file__).resolve(), provider.resolve())
        self.assertIs(sys.modules['helper.usb_observe'], usb_observe)


class RampGaps(unittest.TestCase):
    def test_clean_ramp_across_the_uint16_wrap(self):
        self.assertEqual(hil_util.ramp_gaps([0xFFFE, 0xFFFF, 0, 1], 0), [])

    def test_forward_and_backward_jumps(self):
        s = [10, 11, 12, 208, 209, 149, 150]
        self.assertEqual(hil_util.ramp_gaps(s, 0), [(3, 195), (5, -61)])

    def test_jump_across_the_wrap_is_small(self):
        self.assertEqual(hil_util.ramp_gaps([0xFFF0, 0x00B4], 0), [(1, 195)])

    def test_gaps_before_start_are_ignored(self):
        self.assertEqual(hil_util.ramp_gaps([5, 0, 1, 2, 9], 1), [(4, 6)])


if __name__ == '__main__':
    unittest.main()
