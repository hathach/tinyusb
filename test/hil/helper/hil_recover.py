# SPDX-License-Identifier: MIT
"""Post-pool wedge recovery for hil_test.py (#3947).

A board marked wedged (a confirmed D-state holder on its usbfs node; #3944), by this run's
worker or an earlier run's, is recovered after the worker pool is down, one board at a
time, under ONE reservation of every board in the config: the shield touches the shared
root hub, so nothing else may be flashing or enumerating meanwhile. Per board: shield
(usb-kernel-recover; only when the recovery flasher is not convoy-safe) -> probe reset ->
reflash if the holder survived -> unshield ->
verify (complete holder scan with no holder, and the DUT enumerated again) -> clear the
admission marker with that evidence. Anything less leaves the marker for the next run and
says why.

A shielded board needs a non-root process with passwordless sudo (root ignores the
shield's 000 modes, so JLinkExe would block on the wedged node exactly as before); without
them that board keeps its marker and the others proceed. Refused reservation, --skip-flash:
the whole phase defers. A marker whose identity is not the roster's: left alone.

The phase runs in a SUPERVISOR: a forked child in its own session that takes the flocks,
runs every step, reports its outcomes to hil_test over a pipe, then kills what it spawned
and keeps the flocks until every child is gone before it releases. hil_test being killed
does not end it, so the reservation outlives every action it started; a child that will
not die (a privileged step in D state) keeps the fleet reserved for as long as it lives,
and the report says so. The supervisor's watchdog (SIGALRM at the budget plus one step's
overrun) raises into the phase so the unshield and the release still run, with a second,
hard alarm behind that. hil_test waits the same bound and goes on without the outcomes
when the supervisor does not report.
"""
from __future__ import annotations

import json
import os
import select
import shlex
import signal
import sys
import time
from contextlib import redirect_stdout
from pathlib import Path

from helper import hil_health, hil_lock, hil_util

REPO_ROOT = Path(__file__).resolve().parents[3]
USB_RECOVER = REPO_ROOT / '.claude' / 'skills' / 'usb-kernel-recover' / 'scripts' / 'usb_recover.sh'
RECOVERY_REASON = f'{hil_lock.CI_REASON} wedge recovery'   # CI_REASON prefix: release refuses to kill it
# The whole phase, all boards. One step can overrun it by its own bound plus the reap
# grace, which is what the job timeout above it has to absorb.
PHASE_TIMEOUT = hil_util.pos_int_env('HIL_RECOVERY_TIMEOUT', 600)
SHIELD_TIMEOUT = 60
SETTLE = 5           # after a reset or reflash, for the freed ioctl to unwind and the DUT to enumerate
# Keep at least 5 s: metro_m4_express's UF2 double-tap keeps the bootloader resident (usbtest.RECOVER_SETTLE).
SCAN_ALLOWANCE = 15  # one /proc walk plus the identity scan's bounded serial reads


def overrun() -> int:
    """How far one step can carry the phase past its budget: the longest bounded step, its
    reap grace, and the unshield that follows it."""
    return step_cost(_usbtest().RECOVER_FLASH_TIMEOUT) + step_cost(SHIELD_TIMEOUT)


def _usbtest():
    import usbtest    # hil_test's sys.path; deferred so this helper imports without it
    return usbtest


def _hil_flash():
    import hil_flash
    return hil_flash


class Budget:
    """The phase deadline, handed to every bounded step: `cap(t)` shrinks a step's own
    timeout to what is left after `reserve` (what must still run afterwards, the unshield
    above all), never below one second so the step still runs and reports."""
    def __init__(self, seconds: float, now=time.monotonic):
        self.now = now
        self.deadline = now() + seconds

    def left(self) -> float:
        return self.deadline - self.now()

    def cap(self, timeout: int, reserve: float = 0) -> int:
        return max(1, min(int(timeout), int(self.left() - reserve)))


def step_cost(timeout: int) -> int:
    return timeout + hil_util.REAP_GRACE


def reserve_all(config: dict) -> tuple[dict, str]:
    """Flock every board in the config (boards-skip included, like `hold --all`) in this
    process. ({name: fh}, '') or ({}, why) with nothing held: a partial hold reads as
    protection and is worse than none."""
    names = [b['name'] for b in config.get('boards', []) + config.get('boards-skip', [])]
    held = {}
    for name in names:
        try:
            fh = hil_lock.flock_nb(name)
        except OSError:
            info = hil_lock.read_record(name) or {}
            release_all(held)
            return {}, f'{name} is held by {info.get("reason", "another holder")} (pid {info.get("pid")})'
        hil_lock.write_record(fh, RECOVERY_REASON)
        held[name] = fh
    return held, ''


def release_all(held: dict) -> None:
    for fh in held.values():
        hil_lock.clear_record(fh)
        fh.close()


def preconditions(skip_flash: bool = False) -> str:
    """'' unless the whole phase must defer."""
    if skip_flash:
        return '--skip-flash: this run does not flash, so it does not reset or reflash either'
    return ''


def shield_preconditions() -> str:
    """'' when this process can shield, else why not. Only a board whose recovery flasher is
    not convoy-safe needs the shield; its failure keeps that board's marker alone."""
    if os.geteuid() == 0:
        return 'running as root: the shield does not apply to root (CAP_DAC_OVERRIDE)'
    if not USB_RECOVER.is_file():
        return f'{USB_RECOVER} missing (a staged tree carries no .claude)'
    try:
        if hil_util.run_cmd('sudo -n true', timeout=10, quiet=True).returncode != 0:
            return 'no passwordless sudo for the shield'
    except OSError:
        return 'sudo not installed'
    return ''


def marker_identity(board: dict, marker: dict) -> str:
    """'' when the marker describes THIS roster board's hardware, else why not: the uid
    the worker recorded must be the roster's, and the evidence must name the node and
    the serial the wedge was observed on."""
    if str(marker.get('reason', '')).endswith('; refused'):
        return f'untrusted marker: {marker["reason"]}'
    if marker.get('uid') != board.get('uid'):
        return f'marker uid {marker.get("uid")!r} is not the roster\'s {board.get("uid")!r}'
    ev = marker.get('evidence') or {}
    if not ev.get('node') or not ev.get('serial'):
        return 'marker carries no device node or serial to recover against'
    return ''


def busport_of_node(node: str) -> str:
    """The sysfs busport currently at a usbfs node (/dev/bus/usb/BBB/DDD), '' when none:
    busnum/devnum are lock-free attributes, so this is safe against the wedged device."""
    parts = node.rstrip('/').split('/')
    try:
        bus, dev = int(parts[-2]), int(parts[-1])
    except (ValueError, IndexError):
        return ''
    for d in Path('/sys/bus/usb/devices').glob(f'{bus}-*'):
        if ':' in d.name:
            continue
        try:
            if int((d / 'devnum').read_text()) == dev:
                return d.name
        except (OSError, ValueError):
            continue
    return ''


def _script(action: str, *args: str, timeout: int = SHIELD_TIMEOUT):
    cmd = ' '.join(shlex.quote(a) for a in ['sudo', '-n', str(USB_RECOVER), action, *args])
    r = hil_util.run_cmd(cmd, timeout=timeout, quiet=True)
    return r.returncode, hil_util.cmd_stdout_text(r.stdout).strip()


def recover_board(board: dict, marker: dict, lock_fh, log, budget: Budget) -> dict:
    """One board's recovery under the fleet reservation. Returns the outcome record:
    {'recovered': bool, 'steps': [...], 'why': str, 'identity': str}."""
    ut = _usbtest()
    name = board['name']
    out = {'recovered': False, 'steps': [], 'why': '', 'identity': ''}
    ev = marker.get('evidence') or {}
    node, serial, fw = ev['node'], ev['serial'], marker.get('fw') or ''
    rec_board = {'name': name, 'flasher': _hil_flash().recover_flasher(board)}
    fname = rec_board['flasher']['name']
    me = str(os.getpid())

    def scan(after: str) -> bool:
        stuck, complete = ut.wedged_pids(node)
        out['steps'].append(f'{after}: holders {stuck} complete={complete}')
        return complete and not stuck

    busport = busport_of_node(node)
    if not busport:
        # the node is gone: either the holder let go and the device re-enumerated, or it
        # never came back. The scan decides; there is nothing left to shield.
        if budget.left() < 2 * SCAN_ALLOWANCE:
            out['why'] = 'recovery budget exhausted before this board'
            return out
        if not scan('no device at the node'):
            out['why'] = f'device gone from {node} but a holder or an incomplete scan remains'
            return out
    else:
        # a convoy-safe recovery flasher never reads another device's locking attributes,
        # so only the others need the shield (and root-free sudo for it)
        needs_shield = not _hil_flash().convoy_safe(rec_board['flasher'])
        if needs_shield:
            why = shield_preconditions()
            if why:
                out['why'] = f'{fname} needs the shield, which is unavailable: {why}'
                return out
        shield_cost = step_cost(SHIELD_TIMEOUT) if needs_shield else 0
        # everything this board can cost before its unshield, else it is not started
        need = (shield_cost * 2 + step_cost(ut.RECOVER_RESET_TIMEOUT)
                + SETTLE + 2 * SCAN_ALLOWANCE)
        if budget.left() < need:
            out['why'] = f'recovery budget exhausted before this board ({budget.left():.0f}s left, {need}s needed)'
            return out
        unshield_reserve = shield_cost + SCAN_ALLOWANCE
        cleared = False
        own_shield = needs_shield   # until the script says otherwise: an interrupted shield may have published
        try:
            if needs_shield:
                rc, msg = _script('shield', busport, me, timeout=budget.cap(SHIELD_TIMEOUT, unshield_reserve))
                out['steps'].append(f'shield {busport}: rc {rc} {msg}')
                if rc != 0:
                    # a shield that failed part-way keeps its record ("KEPT"), a timed-out one
                    # (rc 124) may have; a foreign owner's refusal published nothing of ours
                    own_shield = 'KEPT' in msg or rc == 124
                    out['why'] = f'shield refused: {msg}'
                    return out
            else:
                out['steps'].append(f'no shield: {fname} over {rec_board["flasher"].get("args", "")[:40]} is convoy-safe')
            reset_fn = ut.reset_primitive(fname)
            if reset_fn:
                try:
                    with redirect_stdout(sys.stderr):
                        reset_fn(rec_board, timeout=budget.cap(ut.RECOVER_RESET_TIMEOUT, unshield_reserve + SETTLE))
                    out['steps'].append(f'probe reset via {fname}')
                except Exception as e:   # noqa: BLE001 - the scan is the arbiter
                    out['steps'].append(f'probe reset via {fname} raised {type(e).__name__}: {e}')
                time.sleep(SETTLE)
                cleared = scan('after reset')
            if not cleared:
                flash_fn = getattr(_hil_flash(), f'flash_{fname}', None)
                if not fw or not Path(fw).exists():
                    out['steps'].append('no firmware artifact recorded: reflash skipped')
                elif not flash_fn:
                    out['steps'].append(f'no flash_{fname}: reflash skipped')
                elif budget.left() < step_cost(ut.RECOVER_FLASH_TIMEOUT) + SETTLE + unshield_reserve:
                    out['steps'].append('reflash skipped: not enough budget left to reflash and unshield')
                else:
                    try:
                        with redirect_stdout(sys.stderr):
                            r = flash_fn(rec_board, fw, timeout=budget.cap(ut.RECOVER_FLASH_TIMEOUT, unshield_reserve + SETTLE))
                        out['steps'].append(f'reflash via {fname}: rc {getattr(r, "returncode", "?")}')
                    except Exception as e:   # noqa: BLE001
                        out['steps'].append(f'reflash via {fname} raised {type(e).__name__}: {e}')
                    time.sleep(SETTLE)
                    cleared = scan('after reflash')
        finally:
            # ALWAYS, the shield attempt included: a shield interrupted after it published
            # its record must be undone too. The script itself is ownership-aware -- it
            # refuses a live foreign owner's record and reports "no shield record" when
            # ours never published -- so only a failure against OUR record is a failure.
            # Full bound, no budget cap: this is the cleanup the budget reserved for, and
            # it runs on the watchdog path too.
            if own_shield:
                rc, msg = _script('unshield', busport, me, timeout=SHIELD_TIMEOUT)
                out['steps'].append(f'unshield {busport}: rc {rc} {msg}')
                if rc != 0 and 'no shield record' not in msg:
                    out['why'] = (out['why'] + '; ' if out['why'] else '') + f'unshield failed, shield record kept: {msg}'
        if out['why']:
            return out
        if not cleared:
            out['why'] = 'a D-state holder survived the reset and the reflash'
            return out
    # verified: a complete scan found no holder. Now the identity the marker asks for,
    # each serial read bounded so the whole walk stays inside what is left.
    if budget.left() < SCAN_ALLOWANCE:
        out['why'] = 'holder gone, but no budget left for the identity scan; marker kept'
        return out
    per_read = max(0.2, min(hil_util.SYSFS_READ_GRACE, budget.left() / 8))
    devs = [d for d in hil_util.usb_scan(serial=serial, timeout=per_read)
            if d['serial'].lower() == serial.lower()]
    if len(devs) != 1:
        out['why'] = f'holder gone but the DUT ({serial}) is not enumerated exactly once ({len(devs)} found)'
        return out
    out['identity'] = f'{serial}@{devs[0]["busport"]}'
    evidence = {'board': name, 'uid': marker.get('uid', ''), 'holders': [], 'complete': True,
                'identity': out['identity']}
    why = hil_lock.clear_wedged(name, evidence, lock_fh)
    if why:
        out['why'] = f'recovered but the marker stays: {why}'
        return out
    out['recovered'] = True
    return out


class Watchdog(Exception):
    """Raised into the phase by the supervisor's alarm: the finally blocks unshield and
    release, which an os._exit would have skipped."""


def _phase(config: dict, marked: list, log, budget: Budget, report=lambda outcomes: None) -> dict:
    """The reserved part: every board, under the fleet flocks, inside the budget.
    `report(outcomes)` is called BEFORE the children are swept, so the caller learns the
    verdicts even when a child will not die and the flocks must be kept."""
    held, why = reserve_all(config)
    if not held:
        names = ', '.join(b['name'] for b in marked)
        log(f'wedge recovery deferred for {names}: fleet reservation refused ({why}); markers kept')
        report({})
        return {}
    outcomes = {}
    try:
        for board in marked:
            name = board['name']
            if budget.left() <= 0:
                outcomes[name] = {'recovered': False, 'steps': [], 'identity': '',
                                  'why': 'recovery budget exhausted before this board'}
                log(f'{name:25} wedge NOT recovered: {outcomes[name]["why"]}')
                continue
            # re-read under the reservation: another run or operator may have cleared or
            # replaced it while the locks were being taken
            marker = hil_lock.read_wedged(name)
            if marker is None:
                log(f'{name:25} marker gone before the fleet was reserved; nothing to recover')
                continue
            why = marker_identity(board, marker)
            if why:
                outcomes[name] = {'recovered': False, 'steps': [], 'why': why, 'identity': ''}
            else:
                log(f'{name:25} recovering the wedge it is marked with (fleet reserved)')
                outcomes[name] = recover_board(board, marker, held.get(name), log, budget)
            o = outcomes[name]
            log(f'{name:25} wedge {"RECOVERED" if o["recovered"] else "NOT recovered"}'
                f'{": " + o["why"] if o["why"] else ""}; ' + ' | '.join(o['steps']))
    except Watchdog as e:
        log(f'wedge recovery watchdog: {e}; unshield done where a shield was up, releasing')
        outcomes['__error__'] = f'watchdog: {e}'
    finally:
        # a flasher or shield child in its own session must not outlive the reservation.
        # Descendants are snapshotted ONCE, by pid and start time, then killed and watched
        # by identity: a survivor reparented to init is no longer a descendant, and a
        # fresh walk would forget it. While any survives the flocks stay held here -- the
        # caller has its verdicts already -- with the watchdog alarms cancelled, or the
        # retention itself would be what the alarm ends.
        signal.alarm(0)
        tracked = _descendants()
        survivors = _sweep(tracked, log)
        outcomes['__survivors__'] = survivors
        report(outcomes)
        while survivors:
            time.sleep(5)
            survivors = _sweep(tracked, log)
        release_all(held)
    return outcomes


def _start_of(pid: int) -> str:
    """The start time of a live pid, '' once it is gone or a zombie (SIGKILL cannot remove
    a zombie, and it holds nothing)."""
    try:
        stat = Path(f'/proc/{pid}/stat').read_bytes()
        fields = stat[stat.rindex(b')') + 2:].split()
        return '' if fields[0] in (b'Z', b'X') else fields[19].decode()
    except (OSError, ValueError, IndexError):
        return ''


def _descendants() -> dict:
    """{pid: start time} of every live descendant of this process, one /proc walk."""
    out = {}
    try:
        for kids in hil_health.child_procs([os.getpid()]).values():
            for pid, _ in kids:
                start = _start_of(pid)
                if start:
                    out[pid] = start
    except Exception:   # noqa: BLE001 - a failed walk tracks nothing, and says so below
        return {'__unknown__': ''}
    return out


def _alive(pid: int, start: str) -> bool:
    return _start_of(pid) == start


def _sweep(tracked: dict, log) -> int:
    """SIGKILL every tracked identity still alive; return how many are still alive after
    that. A walk that failed counts as a survivor: unknown is not zero."""
    if '__unknown__' in tracked:
        log('wedge recovery: could not enumerate step processes; keeping the fleet reserved')
        return 1
    alive = []
    for pid, start in tracked.items():
        if not _alive(pid, start):
            continue
        try:
            os.kill(pid, signal.SIGKILL)
        except ProcessLookupError:
            continue
        except PermissionError:
            pass
        alive.append(pid)
    time.sleep(0.2)
    n = sum(1 for pid in alive if _alive(pid, tracked[pid]))
    if n:
        log(f'wedge recovery: {n} step process(es) survived SIGKILL ({", ".join(str(p) for p in alive if _alive(p, tracked[p]))}); '
            f'the fleet stays reserved by the supervisor (pid {os.getpid()}) until they end')
    return n


def _supervised(config: dict, marked: list, log) -> dict:
    """Run _phase in a forked child in its own session and read its outcomes back.
    The child's watchdog raises at the budget plus one step's overrun and a hard alarm
    behind it ends a cleanup that stalls; the parent waits for both, then goes on."""
    bound = PHASE_TIMEOUT + overrun()
    cleanup = step_cost(SHIELD_TIMEOUT) + 5
    r_fd, w_fd = os.pipe()
    pid = os.fork()
    if pid == 0:
        os.close(r_fd)
        os.setsid()
        reported = []

        def report(outcomes):
            if not reported:
                reported.append(True)
                try:
                    os.write(w_fd, json.dumps(outcomes).encode())
                    os.close(w_fd)
                except OSError:
                    pass
        try:
            def _watchdog(*_):
                signal.signal(signal.SIGALRM, lambda *_: os._exit(3))   # the hard stop behind the cleanup
                signal.alarm(cleanup)
                raise Watchdog(f'phase exceeded {bound}s')
            signal.signal(signal.SIGALRM, _watchdog)
            signal.alarm(bound)
            _phase(config, marked, log, Budget(PHASE_TIMEOUT), report)
            os._exit(0)
        except BaseException as e:   # noqa: BLE001 - a child that unwinds is no supervisor
            report({'__error__': f'{type(e).__name__}: {e}'})
            os._exit(1)
    os.close(w_fd)
    chunks = []
    deadline = time.monotonic() + bound + cleanup + hil_util.REAP_GRACE
    try:
        while True:
            wait = deadline - time.monotonic()
            if wait <= 0:
                log(f'wedge recovery supervisor (pid {pid}) did not report within {deadline:.0f}s; '
                    'it keeps the fleet reserved until its own watchdog ends it, and the markers stand')
                return {}
            ready, _, _ = select.select([r_fd], [], [], wait)
            if not ready:
                continue
            chunk = os.read(r_fd, 65536)
            if not chunk:
                break
            chunks.append(chunk)
    finally:
        os.close(r_fd)
        try:
            os.waitpid(pid, os.WNOHANG)
        except ChildProcessError:
            pass
    if not chunks:
        log(f'wedge recovery supervisor (pid {pid}) ended without a report; the markers stand')
        return {}
    try:
        outcomes = json.loads(b''.join(chunks))
    except ValueError:
        log('wedge recovery supervisor reported garbage; the markers stand')
        return {}
    if '__error__' in outcomes:
        log(f'wedge recovery supervisor: {outcomes.pop("__error__")}')
    survivors = outcomes.pop('__survivors__', 0)
    if survivors:
        log(f'wedge recovery: {survivors} step process(es) survived; the supervisor (pid {pid}) '
            f'keeps every board reserved until they end')
    return outcomes


def recover_wedged(config: dict, boards: list, log=print, skip_flash: bool = False,
                   supervise: bool = True) -> dict:
    """Recover every board in `boards` (this run's config entries) that carries a marker.
    Returns {name: outcome}; an empty dict when nothing was marked or the phase deferred,
    with the reason logged. Markers of boards not recovered stay in place. `supervise`
    is the forked supervisor (the default); False runs the phase in this process, for
    tests and for a caller that is itself the supervisor."""
    marked = [b for b in boards if hil_lock.read_wedged(b['name']) is not None]
    if not marked:
        return {}
    names = ', '.join(b['name'] for b in marked)
    why = preconditions(skip_flash)
    if why:
        log(f'wedge recovery deferred for {names}: {why}; markers kept')
        return {}
    if supervise:
        return _supervised(config, marked, log)
    outcomes = _phase(config, marked, log, Budget(PHASE_TIMEOUT))
    outcomes.pop('__survivors__', None)
    if '__error__' in outcomes:
        log(f'wedge recovery: {outcomes.pop("__error__")}')
    return outcomes
