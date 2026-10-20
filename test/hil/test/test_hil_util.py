#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Unit tests for hil_util: its wrappers over agentrc's runner, the bottom-layer invariants and
# helpers -- real subprocesses, no hardware. Stdlib + hil_util (stdlib-only) + the agentrc
# providers it and helper/usb_observe.py hand out, so the pre-commit hil-test hook can run
# this on GitHub's bare runner once lib/agentrc is fetched at the pin. Run directly:
#   python3 test/hil/test/test_hil_util.py
import os
import sys
import time
import unittest
from pathlib import Path
from unittest import mock

# the module under test lives in the parent dir's helper/ package
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from helper import hil_util


class Wrappers(unittest.TestCase):
    """run_cmd and run_alongside are agentrc's (hil_process; its tests hold the runner's
    contract) with this harness's CMD_TIMEOUT, REAP_GRACE and verbose read on every call:
    callers and tests set them at run time, so a copy taken at import would be stale."""

    def capture(self, name):
        seen = []
        patcher = mock.patch.object(hil_util.provider().hil_process, name,
                                    lambda *a, **kw: seen.append((a, kw)))
        patcher.start()
        self.addCleanup(patcher.stop)
        for knob in ('CMD_TIMEOUT', 'REAP_GRACE', 'verbose'):
            self.addCleanup(setattr, hil_util, knob, getattr(hil_util, knob))
        hil_util.CMD_TIMEOUT, hil_util.REAP_GRACE, hil_util.verbose = 33, 4, True
        return seen

    def test_run_cmd_forwards_every_mode_and_the_knobs(self):
        seen = self.capture('run_cmd')
        hil_util.run_cmd('c', 'd', binary=True, split_stderr=True, quiet=True)
        hil_util.run_cmd(['c'], timeout=5)
        self.assertEqual(seen, [
            (('c', 'd'), dict(timeout=33, binary=True, split_stderr=True, quiet=True, reap_grace=4, verbose=True)),
            ((['c'], None), dict(timeout=5, binary=False, split_stderr=False, quiet=False, reap_grace=4,
                                 verbose=True))])

    def test_run_alongside_reaps_for_reap_grace(self):
        seen = self.capture('run_alongside')
        work = object()
        hil_util.run_alongside(['c'], work, 5)
        self.assertEqual(seen, [((['c'], work, 5), {'reap_grace': 4})])

    def test_a_real_run_merges_stderr_and_a_timeout_is_124(self):
        r = hil_util.run_cmd('printf out; printf err >&2')
        self.assertEqual((r.returncode, r.stdout), (0, 'outerr'))
        r = hil_util.run_cmd('sleep 30', timeout=1)
        self.assertEqual((r.returncode, r.stdout), (124, ''))


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
        from helper import farm_profile
        self.assertIs(hil_flash.hil_util, hil_util)
        self.assertIs(farm_profile.hil_util, hil_util)
        self.assertIs(farm_profile.hil_flash, hil_flash)

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
                 'hil_lock', 'farm_profile', 'hil_farm_check', 'build', 'build_utils', 'get_deps', 'family_json',
                 'hil_report'}
        allowed = set(sys.stdlib_module_names) | local
        # farm_profile included: test_hil_util_is_a_single_module_instance imports it
        # on the bare runner, and its `import serial` is function-local for exactly
        # this reason -- hoisting it must fail HERE, not on every PR's pre-commit CI
        for mod in ('helper/hil_util', 'hil_flash', '../../tools/change_impact',
                    'helper/hil_lock', 'helper/agentrc', 'helper/farm_profile', 'helper/hil_farm_check',
                    '../../tools/build', '../../tools/build_utils',
                    '../../tools/get_deps', '../../tools/family_json',
                    # the selection contract CI calls: check_build.py --select-only
                    '../../.claude/skills/build/scripts/check_build', 'helper/hil_report'):
            tree = ast.parse((hil_dir / f'{mod}.py').read_text())
            # module level only: a deferred import inside a function cannot break
            # importability (farm_profile keeps `import serial` function-local
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


class UsbObserveIsThePinnedModule(unittest.TestCase):
    """helper/usb_observe.py hands callers agentrc's module itself: one strand memo per
    process, and a patched root reaches every function that reads it."""

    def test_callers_get_the_pinned_provider(self):
        from helper import usb_observe
        provider = Path(hil_util.TINYUSB_ROOT) / 'lib/agentrc/skills/hil/scripts/usb_observe.py'
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
