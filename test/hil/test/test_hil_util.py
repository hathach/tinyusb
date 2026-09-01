#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Unit tests for hil_util.run_cmd's binary/split_stderr/quiet modes — real subprocesses, no
# hardware. Stdlib + hil_util only (hil_util is stdlib-only), so the pre-commit hil-test
# hook can run this on GitHub's bare runner. Run directly:
#   python3 test/hil/test/test_hil_util.py
import io
import os
import sys
import time
import threading
import unittest
from tempfile import TemporaryDirectory
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
        # ../../tools/rtt: hil_util exec_module's it at import (helper/hil_util.py's
        # loader block), so a non-stdlib import THERE kills ci_select on the bare
        # runner just as surely -- and the spec_from_file_location call is invisible to
        # the ast.Import walk below, which is why it must be listed explicitly
        for mod in ('helper/hil_util', 'hil_flash', '../../tools/ci_select',
                    'helper/hil_health', 'helper/hil_lock', 'helper/hil_pool_check',
                    '../../tools/build', '../../tools/build_utils', '../../tools/rtt'):
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
    interpreter warning. hil_ci.sh runs python3 with no isolating flags."""

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


class BoundedReadForGuardlessCallers(unittest.TestCase):
    """`serial` is served under the device lock a wedged usbfs ioctl holds, so the read is
    bounded BY DEFAULT -- not opt-in. usb_scan reads it on every device matching the VID to
    find the one it wants, and hil_lock.controller_of does that from controller_permit on
    essentially every board, so one wedged DUT would stall every worker rather than one.
    hil_pool_check has no guard behind it at all."""

    def setUp(self):
        from helper import hil_util
        self.hil_util = hil_util
        # the pre-commit hook runs all four suites in ONE interpreter, so capture and
        # restore rather than assuming these start (or end) empty
        for name in ('_stranded', '_strand_hits'):
            self.addCleanup(setattr, hil_util, name, dict(getattr(hil_util, name)))
            getattr(hil_util, name).clear()
        self.addCleanup(setattr, hil_util, '_ever_stranded', hil_util._ever_stranded)
        hil_util._ever_stranded = False
        self.td = TemporaryDirectory()
        self.addCleanup(self.td.cleanup)
        self.fifo = os.path.join(self.td.name, 'serial')
        os.mkfifo(self.fifo)          # a read that never answers

    def test_a_wedged_attribute_gives_up_instead_of_hanging(self):
        t0 = time.monotonic()
        self.assertIsNone(self.hil_util.read_sysfs(self.fifo, timeout=0.3))
        self.assertLess(time.monotonic() - t0, 5, 'the bounded read did not give up')

    def test_the_bound_is_the_default_not_an_opt_in(self):
        """usb_scan reads `serial` on every device matching the VID to find the one it
        wants, and hil_lock's controller_of does that from controller_permit on
        essentially every board -- so an opt-in bound that ONE call site forgets lets a
        single wedged DUT stall every worker, not one. Three call sites forgot it once."""
        import inspect
        for fn in (self.hil_util.read_sysfs, self.hil_util.usb_scan):
            default = inspect.signature(fn).parameters['timeout'].default
            self.assertEqual(default, self.hil_util.SYSFS_READ_GRACE,
                             f'{fn.__name__} must be bounded without being asked')
        t0 = time.monotonic()
        self.assertIsNone(self.hil_util.read_sysfs(self.fifo))   # no timeout= passed
        self.assertLess(time.monotonic() - t0, 5, 'the default path did not bound')

    def test_a_node_that_returns_during_the_grace_is_not_memoised_as_wedged(self):
        """The inode must be captured BEFORE the reader starts. Stat it afterwards and a
        board that came back mid-read has its brand-new HEALTHY inode recorded as the
        wedged one -- only a SECOND re-enumeration could ever clear it, and hil_pool_check
        would report a successful recovery as still off the bus."""
        def swap():
            time.sleep(0.15)
            os.unlink(self.fifo)
            Path(self.fifo).write_text('CAFE01\n')

        threading.Thread(target=swap, daemon=True).start()
        self.hil_util.read_sysfs(self.fifo, timeout=0.6)
        self.assertEqual(self.hil_util.read_sysfs(self.fifo, timeout=1), 'CAFE01',
                         'the healthy new inode was recorded as the wedged one')

    def test_concurrent_readers_of_one_path_spend_one_credit(self):
        """hil_pool_check polls one bus from four threads. Counting each READER let four
        threads on ONE wedged device spend four of the process budget between them --
        latching on the single wedge the tool was run to find."""
        ts = [threading.Thread(target=lambda: self.hil_util.read_sysfs(self.fifo, timeout=0.3))
              for _ in range(4)]
        [t.start() for t in ts]
        [t.join() for t in ts]
        self.assertEqual(len(self.hil_util._stranded), 1)
        self.assertEqual(self.hil_util._strand_hits[self.fifo], 1,
                         'four readers of one path spent four credits')

    def test_a_flapping_wedged_device_cannot_leak_without_bound(self):
        """The inode all-clear re-arms on every re-enumeration, so a device that flaps
        while STILL wedged strands again each pass -- a thread and an fd per cycle."""
        for _ in range(self.hil_util._PATH_STRAND_MAX + 4):
            self.hil_util.read_sysfs(self.fifo, timeout=0.2)
            os.unlink(self.fifo)
            os.mkfifo(self.fifo)                      # back on the same path, still wedged
        self.assertEqual(self.hil_util._strand_hits[self.fifo],
                         self.hil_util._PATH_STRAND_MAX,
                         'a flapping device kept stranding past its per-path cap')

    def test_a_value_that_arrived_at_the_deadline_is_not_a_strand(self):
        """`out` is checked BEFORE is_alive(): a reader can deposit its value and still be
        alive for a moment after join() returns. Counting that as a strand blacklists a
        healthy attribute by inode forever AND latches sysfs_stranded for the process."""
        good = Path(self.td.name) / 'idVendor'
        good.write_text('cafe\n')
        real_thread = threading.Thread

        class Lingering(real_thread):        # deposits, then outlives the join
            def run(self):
                super().run()
                time.sleep(2)

        self.hil_util.threading.Thread = Lingering
        self.addCleanup(setattr, self.hil_util.threading, 'Thread', real_thread)
        self.assertEqual(self.hil_util.read_sysfs(str(good), timeout=0.3), 'cafe')
        self.assertNotIn(str(good), self.hil_util._stranded)
        self.assertFalse(self.hil_util.sysfs_stranded())

    def test_path_stranded_answers_per_device_not_per_process(self):
        """usbtest decides whether to run lock-taking cleanup on this result; the sticky
        process-wide flag would let any peer's wedge answer for our board."""
        other = Path(self.td.name) / 'peer'
        other.write_text('PEER\n')
        self.hil_util.read_sysfs(self.fifo, timeout=0.3)
        self.assertTrue(self.hil_util.path_stranded(self.fifo))
        self.assertFalse(self.hil_util.path_stranded(str(other)))
        self.assertTrue(self.hil_util.sysfs_stranded(), 'the process-wide flag is sticky')

    def test_a_refused_read_is_stranded_not_vouched_for(self):
        """usbtest fails CLOSED on path_stranded() before running remove_id/unbind, which
        take the uninterruptible device_lock. Past _STRAND_MAX read_sysfs answers None
        WITHOUT looking -- so answering False there hands that guard a fabricated
        all-clear for a device nobody read, and the lock-taking cleanup runs on a wedge."""
        self.hil_util._stranded.update(
            {f'/sys/fake/{i}': i for i in range(self.hil_util._STRAND_MAX)})
        self.assertIsNone(self.hil_util.read_sysfs(self.fifo, timeout=0.3))
        self.assertTrue(self.hil_util.path_stranded(self.fifo),
                        'a path the reader refused to open was reported readable-and-absent')

    def test_a_stat_that_races_the_reader_still_memoises(self):
        """The pre-read stat is the memo KEY, and it can fail while the open that follows
        succeeds and blocks -- a node replaced between the two. Without a key the give-up
        records nothing, so hil_pool_check's next poll starts another permanent thread and
        fd for the same path, and repeats it every pass."""
        real_stat = self.hil_util.os.stat
        calls = []

        def flaky(path, *a, **kw):
            calls.append(path)
            if len(calls) == 1:          # only the pre-read stat loses the race
                raise OSError('vanished between stat and open')
            return real_stat(path, *a, **kw)

        self.addCleanup(setattr, self.hil_util.os, 'stat', real_stat)
        self.hil_util.os.stat = flaky
        self.assertIsNone(self.hil_util.read_sysfs(self.fifo, timeout=0.3))
        self.assertIn(self.fifo, self.hil_util._stranded,
                      'a lost stat race leaks a fresh reader on every later poll')

    def test_a_successful_read_clears_an_earlier_refusal(self):
        """_refused feeds path_stranded(), which usbtest reads to tell "cannot tell" from
        a real disconnect. Left sticky, a board that recovered and then genuinely left the
        bus is classified as an unrecovered wedge for the rest of the process."""
        good = Path(self.td.name) / 'serial2'
        good.write_text('ABC123\n')
        self.hil_util._refused.add(str(good))
        self.addCleanup(self.hil_util._refused.discard, str(good))
        self.assertEqual(self.hil_util.read_sysfs(str(good), timeout=0.3), 'ABC123')
        self.assertFalse(self.hil_util.path_stranded(str(good)),
                         'a path that answered is still reported unreadable')

    def test_a_recovered_device_is_seen_again_on_the_same_busport(self):
        """THE recovery flow: hil_pool_check resets or reflashes a wedged board, then
        wait_device polls find_device -> scan_usb for the NEW inode. A busport does not
        change when the board comes back on the same physical port, so a path-only
        blacklist would make that poll look at everything except the device it is waiting
        for -- the board recovers physically and the tool reports it gone for the rest of
        the run. A re-enumeration destroys the kernfs node, so a changed inode is the
        all-clear."""
        self.assertIsNone(self.hil_util.read_sysfs(self.fifo, timeout=0.3))
        # re-enumeration: same path, new node
        os.unlink(self.fifo)
        Path(self.fifo).write_text('CAFE01\n')
        self.assertEqual(self.hil_util.read_sysfs(self.fifo, timeout=1), 'CAFE01',
                         'a board that came back on the same busport stayed blacklisted')

    def test_the_caveat_stays_true_after_a_recovery(self):
        """Rows collected while the device was unreadable keep whatever they said, so the
        footer must still warn even once the memo has cleared."""
        self.hil_util.read_sysfs(self.fifo, timeout=0.3)
        os.unlink(self.fifo)
        Path(self.fifo).write_text('CAFE01\n')
        self.hil_util.read_sysfs(self.fifo, timeout=1)
        self.assertTrue(self.hil_util.sysfs_stranded())

    def test_a_stranded_path_is_never_read_twice(self):
        """Each expiry strands a thread and an fd for the life of the process, and
        hil_pool_check POLLS -- wait_device re-scans every 0.5s until its budget runs
        out. Re-reading would leak one pair per poll."""
        self.hil_util.read_sysfs(self.fifo, timeout=0.3)
        t0 = time.monotonic()
        for _ in range(5):
            self.assertIsNone(self.hil_util.read_sysfs(self.fifo, timeout=0.3))
        self.assertLess(time.monotonic() - t0, 0.3,
                        'repeat reads of a known-stranded path paid the grace again')

    def test_the_caller_can_say_the_table_may_be_wrong(self):
        self.assertFalse(self.hil_util.sysfs_stranded())
        self.hil_util.read_sysfs(self.fifo, timeout=0.3)
        self.assertTrue(self.hil_util.sysfs_stranded(),
                        'nothing would tell the operator a missing row may be this tool '
                        'losing sight of healthy hardware')

    def test_a_healthy_attribute_is_not_blacklisted(self):
        good = os.path.join(self.td.name, 'idVendor')
        Path(good).write_text('cafe\n')
        for _ in range(3):
            self.assertEqual(self.hil_util.read_sysfs(good, timeout=1), 'cafe')
        self.assertFalse(self.hil_util.sysfs_stranded())

    def test_without_a_timeout_the_read_stays_plain(self):
        good = os.path.join(self.td.name, 'busnum')
        Path(good).write_text('3\n')
        self.assertEqual(self.hil_util.read_sysfs(good), '3')
        self.assertIsNone(self.hil_util.read_sysfs(os.path.join(self.td.name, 'nope')))


if __name__ == '__main__':
    unittest.main()
