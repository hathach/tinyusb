#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Shutting a wedged HIL run down: kill what the workers spawned.

A device whose usbfs node is held by a D-state process cannot be freed -- SIGKILL is not
delivered in uninterruptible sleep -- so the goal is never to fix the rig from here. It is
to free the runner's single job slot and leave a report naming what survived, instead of
letting the job sit until GitHub cancels it with nothing to show.

Deliberately shallow. We SIGKILL the process groups the workers spawned, wait a grace,
and report whoever is still alive; we do not re-scan groups, prove pid ownership or
escalate through sudo. A root-owned survivor is named in the report for hil_pool_check
and the usb-kernel-recover skill to deal with -- signalling a pid we cannot prove is ours
is the worse failure, and the job ceiling backstops whatever this misses.

Everything here is stdlib-only and reads /proc unprivileged (dmesg is restricted on the
rig), which keeps it importable -- and testable -- on a bare runner.
"""
import os
import signal
import threading
import time
from pathlib import Path

PROC = Path('/proc')


# How long to let a SIGKILL land before calling a process a survivor. Generous enough to
# cover scheduling delay on a loaded rig, short enough that a fleet-wide sweep stays quick.
CONFIRM_KILL_GRACE = 2.0


def _p(*args, **kwargs) -> None:
    # These run on the free-the-runner path, where stdout can already be a dead pipe (a
    # dropped ssh session). An unguarded print would raise BrokenPipeError out of
    # hil_test's inner finally, skipping shutdown_pool AND the report writing.
    try:
        print(*args, **kwargs)
    except (OSError, ValueError):
        # ValueError, not just OSError: printing to a CLOSED stream raises
        # "ValueError: I/O operation on closed file", and both hil_pool_check and
        # hil_test redirect stdout into a StringIO that can be closed under us. Escaping
        # here skips shutdown_pool/kill_pool_children/os._exit -- stranding the runner,
        # the exact failure this wrapper exists to prevent.
        pass


def _state(pid_dir: Path) -> str:
    """The state letter from /proc/<pid>/stat. comm can contain ')', so the field is
    located from the right rather than by splitting."""
    # bytes, not read_text(): read_text decodes with the LOCALE encoding, so under LANG=C
    # (systemd services, self-hosted runners) a non-ASCII comm raises UnicodeDecodeError
    # and the entry silently vanishes from the scan.
    stat = (pid_dir / 'stat').read_bytes()
    return chr(stat[stat.rindex(b')') + 2])


def _pids():
    """/proc pid entries. Yields nothing rather than raising if /proc is unreadable."""
    try:
        entries = list(PROC.iterdir())
    except OSError:
        return
    for entry in entries:
        if entry.name.isdigit():
            yield entry


def d_state_note() -> str:
    """Pids in uninterruptible sleep, for the report. Never aborts, never blocks.

    A D-state process at start-up is NOT a fault on its own -- a healthy in-flight testusb
    looks exactly like this, and the rig supports a dev run alongside CI. It is a hint for
    whoever reads a red cell below. Diagnosis proper is hil_pool_check and the
    usb-kernel-recover skill; this is one line, not a probe."""
    stuck = []
    for d in PROC.glob('[0-9]*'):
        try:
            if _state(d) == 'D':
                stuck.append(d.name)
        except (OSError, ValueError, IndexError):
            pass          # raced with exit, or /proc is restricted: not our problem here
    if not stuck:
        return ''
    return (f'{len(stuck)} process(es) in D state when this run started: '
            f'{sorted(stuck)[:10]}')


def shutdown_pool(pool, grace: float = 30) -> bool:
    """terminate() a worker Pool without ever blocking forever.

    multiprocessing joins its workers unbounded (util.py _exit_function terminate()s the
    daemonic ones, then calls p.join() -- no timeout -- on every remaining active child,
    CPython 3.13.5), and a worker in uninterruptible sleep never
    reaps -- so terminate() itself hangs, taking the runner's only job slot with it. False
    when the pool refuses to die within `grace` (the caller must then abandon it); a
    terminate() that *raises* counts as failure too, the pool being just as alive."""
    outcome = {}

    def _term():
        try:
            pool.terminate()
            outcome['ok'] = True
        except BaseException as e:  # noqa: BLE001 - any failure means the pool is still up
            # Say what happened: Pool._terminate_pool really can raise (CPython:
            # AssertionError 'Cannot have cache with result_handler not alive'), and a
            # swallowed one is indistinguishable from an unkillable D-state worker.
            outcome['err'] = e
            _p(f'warning: Pool.terminate() raised {type(e).__name__}: {e}', flush=True)

    t = threading.Thread(target=_term, daemon=True)
    t.start()
    t.join(grace)
    # Decide on the thread, not the dict: _term may set outcome['ok'] after join(grace)
    # expired, reporting a merely-slow terminate as success on one read and abandoned on
    # another. Still inside terminate() == not shut down.
    if t.is_alive():
        return False
    return outcome.get('ok', False)


def child_procs(pids) -> dict:
    """{ancestor pid in `pids`: [(descendant pid, its pgid), ...]}, from ONE walk of /proc.

    DESCENDANTS, not direct children: a worker's usbtest.py spawns its recovery flasher
    through run_cmd (own session), so it is a GRANDCHILD that a direct-child sweep misses
    and a kill mid-recovery would orphan on the probe. pgid comes back too because the two
    kinds of child need different signals (see kill_pool_children)."""
    wanted = set(pids)
    by_parent: dict = {}                     # ppid -> [(pid, pgid), ...] for EVERY process
    for entry in _pids():
        try:
            stat = (entry / 'stat').read_bytes()
        except OSError:
            continue      # exited between the scan and the read, or not readable
        # comm (field 2) is parenthesised and may contain spaces and ')' -- so split only
        # what follows the LAST ')': state, ppid, pgrp, ...
        try:
            fields = stat[stat.rindex(b')') + 2:].split()
            ppid, pgid = int(fields[1]), int(fields[2])
        except (ValueError, IndexError):
            continue      # truncated or unparsable stat line
        by_parent.setdefault(ppid, []).append((int(entry.name), pgid))
    out: dict = {}
    for root in wanted:
        todo = list(by_parent.get(root, []))
        while todo:
            pid, pgid = todo.pop()
            out.setdefault(root, []).append((pid, pgid))
            todo += by_parent.get(pid, [])
    return out


def _pool_procs(pool, extra) -> list:
    """The pool's worker Process objects, plus each extra's own process.

    Manager() runs in its own child process and inherits the same descriptors as the
    workers, so leaving it behind defeats the point: os._exit skips its finalizer."""
    procs = list(getattr(pool, '_pool', []) or [])
    for e in extra:
        procs.append(getattr(e, '_process', e))
    return procs




def kill_worker_children(pool, *extra) -> int:
    """SIGKILL what the pool's workers spawned; returns how many SURVIVED.

    For the TIMEOUT path only. On the normal path each worker has already run
    kill_own_children() and retired (maxtasksperchild=1), so this walks fresh idle workers
    and finds nothing -- measured: 4 tasks, zero overlap with the pool at sweep time.

    Call it BEFORE shutdown_pool(): terminate() reaps the (interruptible) worker and its
    flasher is reparented to init, erasing the ppid link this matches on. Signalling the
    worker's own group instead cannot work -- a forked pool worker inherits OUR group
    (CPython 3.13.5 multiprocessing never setsid/setpgid) and run_cmd gives every flasher
    a session of its own.

    TWO passes because our SIGKILL can fail an in-flight flash and the worker then retries
    in a fresh session, which one /proc snapshot misses. `seen` stops a pid signalled in
    pass 1 being confirmed twice.
    """
    seen: set = set()
    total = 0
    for i in range(2):
        if i:
            time.sleep(0.5)
        procs = _pool_procs(pool, extra)
        total += _kill_kids(
            child_procs(getattr(p, 'pid', None) for p in procs if p is not None), seen)
    return total


def kill_own_children() -> int:
    """SIGKILL what THIS process spawned. Returns how many survived.

    For the worker to call before it returns. maxtasksperchild=1 retires it the moment the
    task ends, reparenting its children to init, so main()'s sweep walks fresh idle workers
    and finds nothing (measured over 4 tasks: zero overlap, sweep 0, 4 strays alive).
    Inside the worker the ppid link is still live.
    """
    return _kill_kids(child_procs([os.getpid()]), set())


def _kill_kids(kids: dict, seen: set) -> int:
    """SIGKILL every pid in a ppid-tree snapshot; return how many survived.

    Every pid here is a DESCENDANT of a process we own, so it is ours by construction -- no
    argv identity check, because we never signal anything we did not discover through our
    own ppid tree.
    """
    try:
        own = os.getpgid(0)
    except OSError:
        own = None        # cannot tell our own group apart: never killpg, signal pids only
    touched: list = []
    for children in kids.values():
        for cpid, cpgid in children:
            if cpid in seen:
                continue          # a previous pass already signalled it
            seen.add(cpid)
            try:
                if own is not None and cpgid != own:
                    # A run_cmd child: its own session, so one killpg also reaps what it
                    # spawned. Recorded because killpg cannot report a partial kill.
                    os.killpg(cpgid, signal.SIGKILL)
                else:
                    # Shares our group (a plain subprocess.run), so killpg would take
                    # down the whole run -- it is signalled by pid in _kill_and_confirm.
                    pass
                touched.append(cpid)
            except PermissionError:
                # NOT "already gone": the signal did not land, so this pid MUST still be
                # confirmed, or the one case this handler exists for (an all-root session:
                # the sudo wrapper died, its root members did not) is the one case that
                # never reaches the report.
                touched.append(cpid)
            except ProcessLookupError:
                pass          # already gone
            except OSError:
                pass
    # Both paths need confirming: a killpg'd flasher and a same-group mtype blocked on a
    # wedged device are both in D state, and os.kill reported success on either.
    denied = _kill_and_confirm(touched)
    if denied:
        _p(f'warning: could not kill {sorted(denied)}; they still hold whatever they '
              f'had open (probe, usbfs node) into the next job', flush=True)
    # SURVIVORS, not the count we signalled: the caller needs to know the rig is dirty for
    # the next job, and a killpg is counted once per child sharing the group anyway.
    return len(denied)


def _kill_and_confirm(pids) -> list:
    """SIGKILL every pid, then return those STILL alive after ONE grace window.

    SIGKILL is QUEUED, not delivered, for a task in uninterruptible sleep -- and testusb
    waits in a plain wait_for_completion() with no timeout (v6.12.96 usbtest.c:1404;
    usb_sg_wait, message.c:765), so that is the normal state of a healthy in-flight case
    too. os.kill returning success proves nothing; only the recheck does. It is also
    asynchronous, so probing immediately reports a process we just killed as a survivor
    (measured: 11 of 20 plain `sleep`s with no grace).

    Signal all, then poll the set against ONE shared deadline: per-pid windows made this
    scale with stray count, minutes on a convoy. A pid we cannot signal is reported, never
    sudo-killed.
    """
    pending = []
    for pid in pids:
        try:
            os.kill(pid, signal.SIGKILL)
        except ProcessLookupError:
            continue      # already gone
        except OSError:
            pass          # EPERM (root-owned): it stays, and the poll below reports it
        pending.append(pid)

    deadline = time.monotonic() + CONFIRM_KILL_GRACE
    while True:
        alive = []
        for pid in pending:
            try:
                os.kill(pid, 0)
            except ProcessLookupError:
                continue             # ESRCH: genuinely gone
            except OSError:
                pass                 # EPERM: it exists; the state check decides
            try:
                # a ZOMBIE answers kill(pid, 0) too: dead, merely unreaped. Not a survivor.
                if _state(PROC / str(pid)) == 'Z':
                    continue
            except (OSError, ValueError, IndexError):
                continue             # unreadable: assume gone rather than cry wolf
            alive.append(pid)
        pending = alive
        if not pending or time.monotonic() >= deadline:
            return pending           # outlasted SIGKILL: D state, or not ours to kill
        time.sleep(0.02)


def kill_pool_children(pool, *extra) -> int:
    """SIGKILL the pool's worker processes themselves. Returns how many are STILL ALIVE
    after the grace -- not how many were signalled.

    Survivors, not signals: the caller turns this number into "power-cycle the host", so
    counting signals would send someone to a hypervisor over workers that all died.

    Call after a shutdown_pool() that returned False, and after kill_worker_children().
    A D-state worker ignores SIGKILL, but every other worker dies and drops the inherited
    descriptors -- a survivor holds the runner's stdout pipe open and the runner waits for
    EOF even after we exit, so the early exit would not free the job slot."""
    killed_procs: list = []
    for proc in _pool_procs(pool, extra):
        try:
            # Process.kill(), never a raw pid: multiprocessing's _send_signal re-checks
            # `self.returncode is None` first, so once shutdown_pool's thread has reaped a
            # worker this is a no-op instead of signalling a pid the OS may have recycled.
            # os.pidfd_open(proc.pid) is worse: it skips that guard entirely.
            if proc is None or not proc.is_alive():
                continue
            proc.kill()
            killed_procs.append(proc)
        except (OSError, AttributeError, ValueError):
            continue  # already reaped, never started, or not a real process
    # Re-check the Process objects, never the pids collected a moment ago: shutdown_pool's
    # thread is STILL join()ing workers, so a pid killed here can be reaped and RECYCLED
    # before _kill_and_confirm signals it -- and on EPERM that escalates to `sudo -n kill
    # -9 <stale pid>`, killing an unrelated ROOT process as the last act before os._exit.
    killed_pids = []
    for proc in killed_procs:
        try:
            if proc.is_alive() and proc.pid is not None:
                killed_pids.append(proc.pid)
        except (OSError, AttributeError, ValueError):
            continue
    # SIGKILL is asynchronous and a D-state task ignores it: only a confirmed survivor
    # justifies the caller's power-cycle wording
    return len(_kill_and_confirm(killed_pids)) if killed_pids else 0
