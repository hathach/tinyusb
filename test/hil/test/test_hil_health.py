#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Unit tests for hil_health.py — pure logic against a synthetic /proc, no hardware. A real
# wedge cannot be manufactured on demand, so the detectors are exercised against fabricated
# inputs. hil_health is stdlib-only on purpose, so all of this runs on a bare CI runner
# with nothing skipped. Run directly:
#   python3 test/hil/test/test_hil_health.py
import os
import signal
import sys
import threading
import time
import subprocess
import unittest
from multiprocessing import Pool
from pathlib import Path
from tempfile import TemporaryDirectory

# the module under test lives in the parent dir (test/hil), not here
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from helper import hil_health

REAL_PROC = hil_health.PROC


def make_proc(root: Path, procs: dict, with_pid1: bool = True) -> None:
    """Build a synthetic /proc. `procs` maps pid -> (comm, state, cmdline); a None comm or
    cmdline omits that file. `with_pid1=False` simulates a restricted /proc (hidepid=2),
    where an empty scan must not be read as an all-clear."""
    for pid, (comm, state, cmdline) in procs.items():
        d = root / str(pid)
        d.mkdir()
        if comm is not None:
            (d / 'comm').write_text(comm + '\n')
        if cmdline is not None:
            (d / 'cmdline').write_bytes(cmdline)
        # field 2 is comm in parens; the state letter follows it. Deliberately use a comm
        # containing ')' so a naive split() would pick the wrong field.
        (d / 'stat').write_text(f'{pid} (we)ird) {state} 1 1 0 0 -1 0 0\n')
    if with_pid1 and 1 not in procs:
        d = root / '1'
        d.mkdir()
        (d / 'comm').write_text('systemd\n')
        (d / 'cmdline').write_bytes(b'/sbin/init\0')
        (d / 'stat').write_text('1 (systemd) S 0 1 0 0 -1 0 0\n')
    (root / 'not-a-pid').mkdir()


class PatchCase(unittest.TestCase):
    """For classes that patch PROCESS-GLOBAL state (os.kill, time.sleep, subprocess.Popen).

    addCleanup, never tearDown: tearDown does NOT run when setUp raises, so a no-op
    os.kill or time.sleep would survive into every later test in this blocking pre-commit
    suite -- turning one setUp failure into a cascade of nonsense results."""

    def patch(self, obj, name, value):
        self.addCleanup(setattr, obj, name, getattr(obj, name))
        setattr(obj, name, value)

    def restore(self, obj, name):
        """Same guarantee for state a TEST BODY assigns directly: register the restore
        from setUp so it holds even when the assert between fails."""
        self.addCleanup(setattr, obj, name, getattr(obj, name))


class ProcCase(unittest.TestCase):
    """Every subclass repoints hil_health.PROC at a temp tree; restore it so a later test
    cannot silently keep scanning a deleted directory."""

    def tearDown(self):
        hil_health.PROC = REAL_PROC


class ShutdownPool(unittest.TestCase):
    def test_returns_true_when_the_pool_terminates(self):
        pool = Pool(processes=1)
        try:
            self.assertTrue(hil_health.shutdown_pool(pool, grace=30))
        finally:
            pool.terminate()

    def test_returns_false_instead_of_blocking_forever(self):
        """The real failure is a worker in uninterruptible sleep, which cannot be created
        from userspace. What matters is that shutdown_pool gives up on the deadline rather
        than hanging, because the caller must then abandon the pool to free the job slot."""
        # Cancellable, not time.sleep(3600): shutdown_pool returns while its daemon thread
        # is still inside terminate(), and an uninterruptible sleep there outlives the test.
        # The next test alphabetically forks a real Pool, so the leaked thread made it
        # fork-from-multithreaded ('DeprecationWarning: ... may lead to deadlocks in the
        # child') and its result order-dependent. addCleanup releases it either way.
        release = threading.Event()
        self.addCleanup(release.set)

        class NeverDies:
            def terminate(self):
                release.wait(3600)

        start = time.monotonic()
        self.assertFalse(hil_health.shutdown_pool(NeverDies(), grace=0.5))
        self.assertLess(time.monotonic() - start, 10)

    def test_a_raising_terminate_counts_as_failure(self):
        """The thread dies on the exception, so is_alive() goes False -- which would report
        success for a pool that is just as alive as if terminate() had hung."""
        class Explodes:
            def terminate(self):
                raise RuntimeError('boom')

        self.assertFalse(hil_health.shutdown_pool(Explodes(), grace=5))


class ChildProcs(ProcCase):
    """A pool worker's own group is OUR group (multiprocessing never setpgid's), so its
    children can only be found by walking ppid -> pgrp in /proc."""

    def test_grandchildren_are_swept_too(self):
        """usbtest.py (child, own session) spawns its recovery reflash via run_cmd (own
        session again): the flasher is a GRANDCHILD no direct-child walk covers, and a
        pool-guard kill mid-recovery would orphan it on the probe."""
        got = self.scan([100], {
            100: ('worker', 1, 4242),
            200: ('usbtest.py', 100, 200),      # child, own session
            300: ('openocd', 200, 300),         # grandchild flasher, own session
            999: ('unrelated', 1, 999),
        })
        self.assertEqual(sorted(got.get(100, [])), [(200, 200), (300, 300)])

    def scan(self, pids, procs):
        """`procs` maps pid -> (comm, ppid, pgrp); a None comm omits the stat file."""
        with TemporaryDirectory() as td:
            root = Path(td)
            for p, (comm, ppid, pgrp) in procs.items():
                d = root / str(p)
                d.mkdir()
                if comm is not None:
                    (d / 'stat').write_text(f'{p} ({comm}) S {ppid} {pgrp} 0 0 -1 0 0\n')
            (root / 'not-a-pid').mkdir()
            hil_health.PROC = root
            return hil_health.child_procs(pids)

    def test_finds_direct_children_only(self):
        got = self.scan([100], {
            100: ('python3', 1, 4242),        # the worker itself
            201: ('openocd', 100, 201),       # its detached flasher
            202: ('usbtest.py', 100, 202),    # a second detached session
            303: ('unrelated', 7, 303),       # someone else's child
        })
        self.assertEqual({100: [(201, 201), (202, 202)]},
                         {k: sorted(v) for k, v in got.items()})

    def test_covers_every_parent_in_one_walk(self):
        """One pass for all workers, not one pass each: this runs on the free-the-runner
        path, and per-parent walks would also see different snapshots."""
        got = self.scan([100, 101], {
            201: ('openocd', 100, 201),
            202: ('JLinkExe', 101, 202),
        })
        self.assertEqual(got, {100: [(201, 201)], 101: [(202, 202)]})

    def test_parses_a_comm_containing_spaces_and_parens(self):
        """A naive split() on the whole line would read the wrong fields."""
        got = self.scan([100], {500: ('we ) ird', 100, 500)})
        self.assertEqual(got, {100: [(500, 500)]})

    def test_reports_a_child_that_shares_our_group(self):
        """subprocess.run children (arecord, iperf) get no new session, so they land in
        our group. They must still be REPORTED -- kill_pool_children signals them by pid,
        since killpg on that group would take down the run itself."""
        got = self.scan([100], {201: ('arecord', 100, 4242)})
        self.assertEqual(got, {100: [(201, 4242)]})

    def test_tolerates_unreadable_and_truncated_entries(self):
        got = self.scan([100], {
            201: (None, 0, 0),                # stat missing (exited mid-scan)
            202: ('openocd', 100, 202),       # still found
        })
        self.assertEqual(got, {100: [(202, 202)]})

    def test_returns_empty_when_proc_is_unreadable(self):
        hil_health.PROC = Path('/nonexistent-proc-for-test')
        self.assertEqual(hil_health.child_procs([100]), {})


class FakeProc:
    """Stands in for a multiprocessing worker: kill_pool_children goes through
    is_alive() and Process.kill(), whose internal returncode guard is what protects
    against signalling a recycled pid."""

    def __init__(self, pid, alive=True, wedged=False):
        self.pid = pid
        self._alive = alive
        self._wedged = wedged      # D state: ignores SIGKILL, so is_alive() stays True
        self.killed = False

    def is_alive(self):
        return self._alive

    def kill(self):
        self.killed = True
        # A signalled worker DIES unless it is wedged. Modelling every worker as an
        # unkillable survivor sent all of them down the confirm/sudo ladder, which is
        # what let literal pids reach the real os.kill.
        if not self._wedged:
            self._alive = False


class KillWorkerChildren(PatchCase):
    # os.getpgid/killpg are stubbed for the whole class: FakeProc pids are literals like
    # 101, which are live pids on a real machine, so an unstubbed killpg SIGKILLs a real
    # process GROUP. That happened while writing this and killed the test run itself.
    """What the workers spawned, killed while their parents are still alive.

    Verified premise: Pool.terminate() reaps a worker that is merely waiting in
    communicate() on a wedged flasher, reparenting that flasher to init -- so this must
    run BEFORE shutdown_pool(), or the ppid link is gone and a successful terminate()
    skips the cleanup entirely."""

    OWN_PGID = 4242

    def setUp(self):
        # _kill_and_confirm's grace poll must not touch the real /proc: fake pid
        # 900 can be a live process on the host, which stalls the poll for the full grace
        # and prints a false survivor warning into the blocking pre-commit hook.
        self.proc_tmp = TemporaryDirectory()
        self.addCleanup(self.proc_tmp.cleanup)
        self.patch(hil_health, 'PROC', Path(self.proc_tmp.name))  # empty: unreadable -> gone
        self.patch(hil_health, 'CONFIRM_KILL_GRACE', 0.05)
        # the sweep runs two passes with a real gap; the fakes never respawn, so
        # stub the wait rather than pay it in every test
        self.patch(hil_health.time, 'sleep', lambda _s: None)
        self.groups, self.pids = [], []
        self.children = {}                              # worker pid -> [(pid, pgid), ...]
        self.patch(hil_health, 'child_procs', lambda pids: self.children)
        self.patch(os, 'killpg', lambda pgid, sig: self.groups.append((pgid, sig)))
        self.patch(os, 'kill', lambda pid, sig: self.pids.append((pid, sig)))
        self.patch(os, 'getpgid', lambda pid: self.OWN_PGID)
        # overwritten directly by some test bodies below (eperm/boom fakes)
        self.restore(hil_health, '_kill_and_confirm')

    def test_kills_a_detached_child_by_group(self):
        """Flashers are spawned with start_new_session=True, so one killpg also reaps
        whatever they spawned; a plain kill would leave them holding the probe with no
        timeout enforcer left alive."""
        w = FakeProc(101)
        self.children = {101: [(900, 900)]}

        class FakePool:
            _pool = [w]
        self.assertEqual(hil_health.kill_worker_children(FakePool()), 0)  # none survived
        # by GROUP, so whatever the flasher spawned dies with it
        self.assertEqual(self.groups, [(900, signal.SIGKILL)])
        # and then confirmed by pid: killpg reports success when it reached ANY member,
        # so the group kill alone is not evidence this one died
        self.assertIn((900, 0), self.pids)
        self.assertFalse(w.killed)                      # the WORKER is not this one's job

    def test_a_root_owned_group_is_still_confirmed_and_reported(self):
        """killpg on an all-root session raises EPERM: the sudo wrapper died and only its
        root members remain. That is the one case this handler exists for, so it must
        still reach the confirm step -- otherwise the holder that strands the NEXT job is
        the one holder the report never names."""
        w = FakeProc(101)
        self.children = {101: [(900, 900)]}

        def eperm(pgid, sig):
            raise PermissionError
        os.killpg = eperm

        class FakePool:
            _pool = [w]
        hil_health.kill_worker_children(FakePool())
        # confirmed by pid: a liveness probe on the member killpg could not touch
        self.assertIn((900, 0), self.pids)

    def test_kills_a_same_group_child_by_pid(self):
        """arecord/iperf/gio go through plain subprocess.run and stay in OUR group, where
        killpg would take down the run itself -- but they must still die, or a blocked
        arecord keeps holding the wedged device."""
        w = FakeProc(101)
        self.children = {101: [(900, self.OWN_PGID)]}

        class FakePool:
            _pool = [w]
        hil_health.kill_worker_children(FakePool())
        self.assertEqual(self.groups, [])               # never our own group
        # SIGKILL, then a (pid, 0) probe: signalling is not dying, so the kill is always
        # confirmed -- see _kill_and_confirm.
        self.assertIn((900, signal.SIGKILL), self.pids)
        self.assertIn((900, 0), self.pids)


    def test_signals_pids_only_when_our_group_is_unknown(self):
        """If getpgid(0) fails we cannot tell our group from a detached one, so killpg is
        never safe -- fall back to per-pid signals rather than guessing."""
        def boom(pid):
            raise OSError('no pgid')
        os.getpgid = boom
        w = FakeProc(101)
        self.children = {101: [(900, 900)]}

        class FakePool:
            _pool = [w]
        hil_health.kill_worker_children(FakePool())
        self.assertEqual(self.groups, [])
        self.assertIn((900, signal.SIGKILL), self.pids)

    def test_covers_a_dead_workers_orphans(self):
        """A worker reaped between the snapshot and now leaves its flasher running. The
        children are keyed off the snapshot, not off is_alive(), so they still die."""
        w = FakeProc(101, alive=False)
        self.children = {101: [(900, 900)]}

        class FakePool:
            _pool = [w]
        self.assertEqual(hil_health.kill_worker_children(FakePool()), 0)  # none survived
        self.assertEqual(self.groups, [(900, signal.SIGKILL)])

    def test_a_survivor_is_returned_so_the_report_can_say_the_rig_is_dirty(self):
        """A stray that ignores SIGKILL is in D state on a usbfs node or holds a probe, and
        it persists into the NEXT job. The count used to be discarded by the caller (the
        return was the signalled-child count, which nothing read), so the only trace was a
        line in the log -- and the run still published a table that looks clean."""
        w = FakeProc(101)
        # TWO strays, only ONE unkillable: signalled=2, survivors=1, so this cannot pass
        # by accident on the old return value
        self.children = {101: [(900, 900), (901, 901)]}
        self.patch(hil_health, '_kill_and_confirm', lambda pids: [p for p in pids if p == 901])

        class FakePool:
            _pool = [w]
        self.assertEqual(hil_health.kill_worker_children(FakePool()), 1)

    def test_no_signal_when_the_workers_spawned_nothing(self):
        w = FakeProc(101)                               # no self.children entry

        class FakePool:
            _pool = [w]
        self.assertEqual(hil_health.kill_worker_children(FakePool()), 0)
        self.assertEqual((self.groups, self.pids), ([], []))

    def test_includes_the_managers_children(self):
        mgr_proc = FakeProc(402)
        self.children = {402: [(900, 900)]}

        class FakePool:
            _pool = []

        class FakeManager:
            _process = mgr_proc
        self.assertEqual(hil_health.kill_worker_children(FakePool(), FakeManager()), 0)


class ConfirmTailIsOneGrace(PatchCase):
    """The grace is ONE window for the whole set, not one per pid. Paid serially it
    scaled with stray count: 30 strays x 16 workers spent ~154s inside the path whose
    only job is to free the runner's single job slot -- which is exactly the
    'multi-stray convoy tail is minutes' the CI ceilings budget +30 min for."""

    def setUp(self):
        self.proc_tmp = TemporaryDirectory()
        self.addCleanup(self.proc_tmp.cleanup)
        root = Path(self.proc_tmp.name)
        # every fake pid is alive and NOT a zombie, so all of them outlast the grace
        make_proc(root, {900 + i: ('flasher', 'D', b'openocd\x00') for i in range(20)})
        self.patch(hil_health, 'PROC', root)
        self.patch(hil_health, 'CONFIRM_KILL_GRACE', 0.3)
        self.patch(os, 'kill', lambda pid, sig: None)   # never signal a real pid

    def test_twenty_survivors_cost_one_grace_not_twenty(self):
        pids = [900 + i for i in range(20)]
        t0 = time.monotonic()
        still = hil_health._kill_and_confirm(pids)
        elapsed = time.monotonic() - t0
        self.assertEqual(sorted(still), pids)          # all reported, none lost
        self.assertLess(elapsed, 0.3 * 4,
                        f'the grace is paid per pid ({elapsed:.2f}s for 20)')


class KillPoolChildren(PatchCase):
    """The worker processes themselves.

    Verified premise: an orphaned pool worker keeps the CI runner's stdout pipe open, so a
    reader never sees EOF even after the parent exits.

    Fakes throughout: FakeProc.kill() only sets a flag, so nothing here can signal a real
    process. That matters historically -- an earlier revision drove this through
    os.pidfd_open with literal pids (101, 102), which exist on a real machine, so the suite
    was asking the kernel to signal unrelated system processes and was saved only by EPERM.
    Keep the fake in charge of kill(); never let a test reach os.kill/os.killpg with a
    live pid. FakeProc.kill() alone is NOT enough for that: it leaves is_alive() True, so
    the pid reaches the confirm/sudo ladder, which signals for real. Stub that too."""

    def setUp(self):
        # Pids 101/102/201 are ordinary user processes on a container or a fresh runner --
        # and pre-commit.yml runs this suite on GitHub's. Unstubbed, the ladder ran
        # os.kill(101, SIGKILL) and forked `sudo -n kill -9 101` on an account with
        # passwordless sudo, and the assertions passed only because those pids happen to
        # be unkillable kernel threads here.
        self.signals = []
        self.patch(os, 'kill', lambda pid, sig: self.signals.append((pid, sig)))
        self.patch(os, 'killpg', lambda pgid, sig: self.signals.append((pgid, sig)))
        self.proc_tmp = TemporaryDirectory()
        self.addCleanup(self.proc_tmp.cleanup)
        self.patch(hil_health, 'PROC', Path(self.proc_tmp.name))  # empty: unreadable -> gone
        self.patch(hil_health, 'CONFIRM_KILL_GRACE', 0.05)

    def test_a_healthy_worker_never_reaches_the_signalling_ladder(self):
        """The premise every assertion below rests on. Process.kill() is the fake's job;
        only a worker that SURVIVES it goes on to raw os.kill/sudo, and these pids are
        literals that belong to somebody else."""
        a, b = FakeProc(101), FakeProc(102)

        class FakePool:
            _pool = [a, b]
        hil_health.kill_pool_children(FakePool())
        self.assertEqual(self.signals, [], 'a literal pid reached the raw-signal ladder')

    def test_signals_every_live_worker(self):
        a, b = FakeProc(101), FakeProc(102)

        class FakePool:
            _pool = [a, b]
        # 0, not 2: the RETURN is confirmed survivors, and workers that die to SIGKILL are
        # not survivors. The operator verdict ("power-cycle the host") hangs off this.
        self.assertEqual(hil_health.kill_pool_children(FakePool()), 0)
        self.assertTrue(a.killed and b.killed)

    def test_a_wedged_worker_is_reported_as_a_survivor(self):
        """The number the power-cycle verdict is worded on."""
        make_proc(Path(self.proc_tmp.name), {301: ('python3', 'D', b'python3 hil_test.py\x00')})
        wedged = FakeProc(301, wedged=True)

        class FakePool:
            _pool = [wedged]
        self.assertEqual(hil_health.kill_pool_children(FakePool()), 1)

    def test_skips_a_reaped_worker(self):
        """Process.kill() re-checks returncode internally, but skipping a dead child keeps
        the harness from signalling a pid the OS may have recycled."""
        live, dead = FakeProc(201), FakeProc(202, alive=False)

        class FakePool:
            _pool = [live, dead]
        self.assertEqual(hil_health.kill_pool_children(FakePool()), 0)
        self.assertTrue(live.killed)
        self.assertFalse(dead.killed)

    def test_also_kills_the_manager(self):
        """Manager() is a separate child holding the same descriptors, and os._exit skips
        its finalizer, so leaving it behind defeats the whole purpose. The RETURN is the
        confirmed-survivor count (the caller words a power-cycle verdict on it), so a
        clean kill of both reports 0."""
        worker, mgr_proc = FakeProc(401), FakeProc(402)

        class FakePool:
            _pool = [worker]

        class FakeManager:
            _process = mgr_proc
        self.assertEqual(hil_health.kill_pool_children(FakePool(), FakeManager()), 0)
        self.assertTrue(worker.killed and mgr_proc.killed)
        self.assertTrue(mgr_proc.killed)

    def test_tolerates_a_pool_without_workers(self):
        class NoPool:
            _pool = None
        self.assertEqual(hil_health.kill_pool_children(NoPool()), 0)


class WorkerSweepsItsOwnChildren(unittest.TestCase):
    """maxtasksperchild=1 makes a worker exit the moment its task returns, so by the time
    main()'s finally sweeps, the strays have been reparented to init and are off the pool's
    ppid tree entirely. Measured over 4 tasks: pool._pool held two FRESH workers with zero
    overlap with the four that ran, child_procs() returned {}, the sweep reported 0, and all
    four strays were alive. Inside the worker the ppid link is still there."""

    def test_a_detached_child_is_killed_and_confirmed(self):
        kid = subprocess.Popen(['sleep', '120'], start_new_session=True)
        self.addCleanup(lambda: kid.poll() is None and kid.kill())
        time.sleep(0.3)                       # let it appear in /proc

        stray = hil_health.kill_own_children()

        self.assertEqual(stray, 0, 'a killable stray was reported as a survivor')
        kid.wait(timeout=5)                   # TimeoutExpired here means it outlived us
        self.assertIsNotNone(kid.poll())

    def test_no_children_is_not_an_error(self):
        self.assertEqual(hil_health.kill_own_children(), 0)


class PermitReleasesOnlyWhatItTook(unittest.TestCase):
    """The bounded acquire skips a slot it could not get ('proceeding over-subscribed') and
    deliberately leaves it out of `taken`, but __exit__ released every slot in self.slots.
    multiprocessing.Semaphore is unbounded, so each timeout permanently widened that
    controller's permit -- the throttle this branch NARROWED (FLASH_PARALLEL 8->4,
    USBTEST_PARALLEL 4->2) for xHCI bandwidth margin."""

    def test_a_timed_out_slot_is_not_released_on_exit(self):
        from helper import hil_lock
        import multiprocessing

        sems = [multiprocessing.Semaphore(1)]
        sems[0].acquire()                      # width 1, already held: the next wait times out
        self.addCleanup(setattr, hil_lock, 'PERMIT_TIMEOUT', hil_lock.PERMIT_TIMEOUT)
        hil_lock.PERMIT_TIMEOUT = 0.1

        permit = hil_lock.controller_permit(sems, 'UID')
        permit.slots = [0]
        with permit:
            pass

        # one holder still holds it, so a correct exit leaves it unavailable
        self.assertFalse(sems[0].acquire(timeout=0.1),
                         'the permit released a slot it never acquired: width grew')


class RecoveryUsesAResetOnlyWhenThereIsARealOne(unittest.TestCase):
    """usbtest's recovery runs the reset unconditionally before the reflash -- it is
    non-destructive (the wedged firmware survives for autopsy), writes no flash, cannot
    brick SWD the way a bad park image has (mimxrt1064_evk, max32666fthr), and is measured
    at 128-129 ms against a full erase+program.

    Two things still gate it, and both are what this pins: a flasher may have no reset
    primitive at all, and reset_esptool/reset_lm4flash return rc 0 WITHOUT resetting
    anything. Running those makes the log say "resetting <board> via <flasher>" for a step
    that did nothing. wedged_pids() arbitrates either way, so behaviour was always right --
    the record was not, and a false record is what keeps having to be unpicked."""

    def setUp(self):
        import usbtest          # test/hil is already on sys.path (see top of file)
        # PRODUCTION, not a copy: re-implementing the screen here let the real gate be
        # deleted with the suite still green, which is the failure mode this pins.
        self._reset_fn = usbtest.reset_primitive

    def test_a_stub_that_resets_nothing_is_not_claimed(self):
        for name in ('esptool', 'lm4flash'):
            self.assertIsNone(self._reset_fn(name),
                              f'reset_{name} returns rc 0 without resetting; claiming it '
                              f'puts a step that did nothing in the record')

    def test_a_real_reset_primitive_is_used(self):
        for name in ('openocd', 'jlink', 'stlink'):
            self.assertIsNotNone(self._reset_fn(name))

    def test_a_flasher_with_no_reset_primitive_goes_straight_to_the_reflash(self):
        self.assertIsNone(self._reset_fn('nosuchflasher'))

    def test_the_reset_is_attempted_before_the_reflash(self):
        """Order matters and now lives only in main()'s inline ladder, where no test
        reaches it -- swapping the two blocks kept the suite green. Reset first is
        non-destructive: the firmware under test survives for autopsy, no flash is
        written, and it cannot brick SWD the way a bad park image has on mimxrt1064_evk
        and max32666fthr."""
        import ast
        import usbtest
        src = Path(usbtest.__file__).read_text()
        fn = next(n for n in ast.walk(ast.parse(src))
                  if isinstance(n, ast.FunctionDef) and n.name == 'main')
        seg = ast.get_source_segment(src, fn)
        reset_at = seg.index('reset_fn = reset_primitive(')
        flash_at = seg.index("flash_fn(board, args.recover_fw")
        self.assertLess(reset_at, flash_at,
                        'the reflash is attempted before the non-destructive reset')



class SudoSoftNeverRaises(unittest.TestCase):
    """Two of its four call sites are inside run_case's timeout handler, where ANY raise
    costs the HUNG verdict, the recovery and the JSON report -- and sudo() sys.exit()s on
    'a password is required', which is a raise like any other."""

    def setUp(self):
        import usbtest
        self.u = usbtest
        self.addCleanup(setattr, usbtest, 'sudo', usbtest.sudo)

    def _check(self, exc):
        def boom(*a, **k):
            raise exc
        self.u.sudo = boom
        r = self.u._sudo_soft(['dmesg'])            # must not propagate
        self.assertEqual(r.returncode, 1)

    def test_systemexit_from_a_password_prompt_is_contained(self):
        self._check(SystemExit('sudo needs a password'))

    def test_oserror_is_contained(self):
        self._check(OSError('no such binary'))

    def test_subprocess_error_is_contained(self):
        self._check(subprocess.SubprocessError('timed out'))


if __name__ == '__main__':
    unittest.main(verbosity=1)
