#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Board locks + controller permits for the TinyUSB HIL rig.

Board locks are kernel flocks in BOARD_LOCK_DIR arbitrating hardware access
between dev sessions and CI's hil_test.py (never stop the actions-runner).
Controller permits are in-process semaphores budgeting flashes and usbtest
batteries per host controller; they have no CLI meaning. The CLI below
(hold/release/status) manages board locks only.
"""
import argparse
import fcntl
import json
import os
import re
import select
import signal
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))  # helper/ scripts import via the test/hil root
from helper import hil_util

BOARD_LOCK_DIR = '/tmp/tinyusb-hil-locks'
CI_REASON = 'hil_test.py'   # release-protected holder tag (release refuses to kill it)
PROTECTED_REASONS = {CI_REASON, 'pool_check'}  # cmd_release refuses to SIGTERM these holders
PROFILE = os.environ.get('HIL_PROFILE') == '1'


def lock_path(board: str) -> str:
    return os.path.join(BOARD_LOCK_DIR, f'{board}.lock')


def flock_nb(board: str):
    """Open-or-create the lock file WITHOUT truncating (a losing racer must not
    wipe the winner's record) and take LOCK_EX|LOCK_NB. Returns the open handle;
    raises OSError when the flock is held elsewhere (handle already closed)."""
    fd = os.open(lock_path(board), os.O_RDWR | os.O_CREAT, 0o666)
    fh = os.fdopen(fd, 'r+')
    try:
        fcntl.flock(fh, fcntl.LOCK_EX | fcntl.LOCK_NB)
    except OSError:
        fh.close()
        raise
    return fh


def write_record(fh, reason: str) -> bool:
    """Holder record; the flock itself is already held. Returns False on a write failure:
    acquire_board_lock stays best-effort (the flock is the authority), but cmd_hold aborts
    -- a hold whose record is missing is invisible to status/release."""
    try:
        fh.truncate(0)
        fh.seek(0)
        json.dump({'pid': os.getpid(), 'reason': reason,
                   'since': time.strftime('%Y-%m-%dT%H:%M:%S%z')}, fh)
        fh.flush()
        return True
    except OSError:
        return False


def clear_record(fh) -> None:
    """Clear our record before dropping the flock so records stay truthful."""
    try:
        fh.truncate(0)
    except OSError:
        pass


def read_record(board: str):
    try:
        with open(lock_path(board)) as f:
            return json.load(f)
    except (OSError, ValueError):
        return None


# --- per-board dev-session locks ------------------------------------------
def acquire_board_lock(board_name, reason=CI_REASON):
    """Take this board's flock for the duration of its flash+test.
    Returns an open file handle (keep it referenced; closing releases it),
    or None when HIL_NO_BOARD_LOCK=1 or the lock dir is unusable (fail-open:
    locking must never break a test run by itself).
    Raises RuntimeError only when another session holds the board."""
    import fcntl
    if os.environ.get('HIL_NO_BOARD_LOCK') == '1':
        return None  # user-authorized bypass — see hil skill
    try:
        os.makedirs(BOARD_LOCK_DIR, exist_ok=True)
        fd = os.open(os.path.join(BOARD_LOCK_DIR, f'{board_name}.lock'),
                     os.O_RDWR | os.O_CREAT, 0o666)
        fh = os.fdopen(fd, 'r+')
    except OSError as e:
        # odd lock dir (perms, path collision): proceed unlocked, but say so —
        # a silent fail-open is indistinguishable from the intentional bypass
        print(f'warning: board lock unavailable for {board_name} ({e}); proceeding unlocked',
              flush=True)
        return None
    try:
        fcntl.flock(fh, fcntl.LOCK_EX | fcntl.LOCK_NB)
    except OSError:
        try:
            info = fh.read(500).strip()
        except (OSError, UnicodeDecodeError):
            info = ''
        fh.close()
        raise RuntimeError(f'board locked: {info or "unknown holder"}')
    # announce ourselves so the other side's conflict message is truthful;
    # best-effort — the flock itself is already held
    try:
        fh.truncate(0)
        fh.seek(0)
        json.dump({'pid': os.getpid(), 'reason': reason,
                   'since': time.strftime('%Y-%m-%dT%H:%M:%S%z')}, fh)
        fh.flush()
    except OSError:
        pass
    return fh


# Per-host-controller concurrency (see controller_of/controller_slot below): a usbtest
# battery saturates its DUT's host controller, so batteries and flashes are budgeted per
# controller. The 4/2 defaults trade ~3.5 min on the usbtest leg for bandwidth margin on
# the shared leaf-hub uplinks, where battery case failures were observed from 12/8
# (profiled 2026-07-13/14: 22.2/14.3/12.5/10.8 min at usbtest width 1/2/3/4, plateau
# after). Raise per run via HIL_FLASH_PARALLEL/HIL_USBTEST_PARALLEL.
# - uPD720201 cards need firmware >= 2.0.2.6 (RAM-uploaded, reloads every power cycle):
#   the ROM firmware dies under battery + re-enumeration churn.
# - a marginal DUT port bouncing during concurrent batteries can kill a uPD720201 ("xHCI
#   host not responding to stop endpoint command"): fix the port/cable or pull the board
#   -- lowering the widths does not fix a bad port (2026-07-16, every death).
FLASH_PARALLEL = hil_util.pos_int_env('HIL_FLASH_PARALLEL', 4)
USBTEST_PARALLEL = hil_util.pos_int_env('HIL_USBTEST_PARALLEL', 2)
CONTROLLER_SLOTS = 12  # lock slots; controllers are assigned to slots on first sight
# Bound on ONE permit wait. Generous: a real queue behind a slow board is normal,
# and this only has to beat the pool guard so a leaked permit cannot consume it.
PERMIT_TIMEOUT = hil_util.pos_int_env('HIL_PERMIT_TIMEOUT', 900)
# CONTROLLER_SLOTS + 1 entries each, built by make_permit_sems: UNKNOWN_SLOT indexes the
# extra one. Sized to CONTROLLER_SLOTS instead, the first unresolved board IndexErrors
# inside a pool worker -- which now surfaces through drain_pool as a worker-raise (the
# finished boards survive), but still loses this board and aborts the run.
usbtest_sems = None     # per-slot usbtest-battery permits
flash_sems = None       # per-slot flash permits
controller_map = None         # shared dict: 'pci:<addr>' -> slot, 'uid:<uid>' -> pci addr cache
controller_meta = None        # guards slot assignment in controller_map
controller_hints = {}         # static uid -> pci from the last run's cache (read-only per worker)


log = print  # hil_test.init_worker points this at log_line via init_scheduling


def init_scheduling(b_sems, f_sems, cmap, cmeta, hints, log_fn=None):
    """Install per-worker scheduling state (called from hil_test.init_worker)."""
    global usbtest_sems, flash_sems, controller_map, controller_meta, controller_hints, log
    usbtest_sems, flash_sems = b_sems, f_sems
    controller_map, controller_meta, controller_hints = cmap, cmeta, hints
    if log_fn is not None:
        log = log_fn


# -------------------------------------------------------------
# Per-controller scheduling
# -------------------------------------------------------------
def controller_of(uid: str):
    """Resolve a DUT uid to its root host controller's PCI address, or None when it cannot
    be resolved — the device is not enumerated (e.g. parked in board_test firmware with USB
    off), or sysfs would not answer. Successful resolutions are cached — cabling does not
    change mid-run. Dual-port parts (e.g. CH32V307 usbhs/usbfs variants) share one uid and
    one cache entry: budgeting is only exact when both ports sit on the same controller
    (true on this rig)."""
    if controller_map is None:
        return None
    cached = controller_map.get(f'uid:{uid}')
    if cached:
        return cached
    # vid='cafe' first: the target is always a TinyUSB DUT, and the VID is a lock-free
    # descriptor field. Without it this reads every probe's and hub's `serial` -- the one
    # attribute served under device_lock -- so a wedged peer would block us here.
    devs = hil_util.usb_scan(vid='cafe', serial=uid)
    for dev in devs:
        busnum = hil_util.read_sysfs(os.path.join(dev['dir'], 'busnum'))
        if busnum is None:
            continue
        try:
            root = os.path.realpath(f'/sys/bus/usb/devices/usb{int(busnum)}')
        except ValueError:
            continue
        m = re.findall(r'[0-9a-f]{4}:[0-9a-f]{2}:[0-9a-f]{2}\.[0-9a-f]', root)
        if m:
            controller_map[f'uid:{uid}'] = m[-1]
            return m[-1]
    return None


def controller_slot(pci: str) -> int:
    """Map a controller PCI address to a lock slot (assigned on first sight)."""
    key = f'pci:{pci}'
    with controller_meta:
        slot = controller_map.get(key)
        if slot is None:
            slot = controller_map.get('nslots', 0)
            if slot >= CONTROLLER_SLOTS:
                slot = 0  # more controllers than slots: overflow shares slot 0 (safe, over-serialized)
            else:
                controller_map['nslots'] = slot + 1
            controller_map[key] = slot
        return slot


# Unresolved boards budget in a slot of their OWN, one past the real ones, and that slot
# holds exactly ONE permit whatever the per-controller width is. Neither neighbour works:
# a permit on every slot (the old fail-closed rule) serialized the whole fleet the moment
# one board could not be resolved, while a full private budget let unknown boards run a second
# controller's worth of batteries on top of the resolved ones -- doubling the load on
# whichever physical controller they actually sit on, which is the saturation the
# uPD720201 deaths above are attributed to. Width 1 caps the over-subscription at +1.
UNKNOWN_SLOT = CONTROLLER_SLOTS


def make_permit_sems(semaphore, width: int) -> list:
    """One semaphore per controller slot at `width`, plus the unknown bucket at 1."""
    return [semaphore(width) for _ in range(CONTROLLER_SLOTS)] + [semaphore(1)]


class controller_permit:
    """Context manager: one permit from `sems` on the board's controller slot. An
    unresolved controller budgets in UNKNOWN_SLOT, which admits one at a time: unresolved
    boards serialize against each other, never against the whole rig, and never add a
    second full budget to a controller. `warn_unknown` logs that fallback (used by
    usbtest, where the device is expected to be enumerated by the caller)."""
    def __init__(self, sems, uid: str, warn_unknown: bool = False):
        self.sems = sems
        self.slots = None
        self.uid = uid
        # what __enter__ actually ACQUIRED. Not the same as self.slots: a bounded acquire
        # that times out is skipped on purpose, and releasing it anyway would add a permit
        # that was never taken -- multiprocessing semaphores are unbounded, so the width
        # grows for the rest of the run, on the controller throttle that exists to keep
        # concurrent batteries from killing the uPD720201 xHCI.
        self.taken: list = []
        if sems is None:
            return
        # Hint FIRST for flash budgeting: a mis-budgeted flash is harmless, and the board
        # is usually parked in board_test with USB off at this point, so controller_of
        # cannot resolve it anyway -- it just walks the whole bus to say so, once per
        # flash permit (~14 examples x ~21 boards a leg), each walk spawning a bounded
        # reader per device. usbtest still resolves for real (warn_unknown), and by then
        # the DUT is enumerated, so that walk succeeds and caches.
        pci = None if warn_unknown else controller_hints.get(uid)
        if pci is None:
            pci = controller_of(uid)
        if pci is None and warn_unknown:
            log(f'warning: cannot resolve {uid} to a host controller; '
                f'budgeting it in the unknown bucket')
        self.slots = [controller_slot(pci) if pci else UNKNOWN_SLOT]

    def __enter__(self):
        if self.slots:
            t0 = time.monotonic()
            taken = self.taken = []
            try:
                for s in self.slots:
                    # BOUNDED. multiprocessing semaphores are NOT released when a holder
                    # dies, and the pool sweep SIGKILLs workers -- so a permit lost that
                    # way would block every later worker on this controller forever, and
                    # boards unrelated to the wedge would burn the whole pool guard. On
                    # expiry proceed over-subscribed and say so: a slower controller is a
                    # far better failure than a hung run.
                    if not self.sems[s].acquire(timeout=PERMIT_TIMEOUT):
                        log(f'warning: waited {PERMIT_TIMEOUT}s for a permit on slot {s} '
                            f'(uid {self.uid}); a holder probably died without releasing '
                            f'it -- proceeding over-subscribed')
                        continue
                    taken.append(s)
                # inside the try: a failed __enter__ never gets its __exit__, so a raise
                # here (e.g. broken stdout) must still release the permits
                if PROFILE and time.monotonic() - t0 > 1.0:
                    log(f'[prof] permit wait {time.monotonic() - t0:.1f}s '
                             f'(uid {self.uid}, slots {self.slots})')
            except BaseException:
                for s in reversed(taken):
                    self.sems[s].release()
                raise
        return self

    def __exit__(self, *exc):
        if self.slots:
            for s in reversed(self.taken):
                self.sems[s].release()
            self.taken = []
        return False


def flash_permit(uid: str) -> controller_permit:
    return controller_permit(flash_sems, uid)


def usbtest_permit(uid: str) -> controller_permit:
    return controller_permit(usbtest_sems, uid, warn_unknown=True)


# --- operator CLI (hold/release/status) ------------------------------------
def boards_from_config(config: str) -> list:
    """All board names, INCLUDING boards-skip: `hold --all` guards rig-wide
    operations, and parked boards can still be touched (pool_check -b names them
    explicitly), so a rig-wide hold that skipped them would leave a gap."""
    try:
        with open(config) as f:
            cfg = json.load(f)
            return [b['name'] for b in cfg['boards'] + cfg.get('boards-skip', [])]
    except (OSError, ValueError, KeyError) as e:
        print(f'ERROR: cannot read board roster {config}: {e}', file=sys.stderr)
        sys.exit(1)


def is_locked(board: str) -> bool:
    """True if the recorded holder process is still alive.

    Deliberately never touches the flock: even a momentary probe lock would
    make a concurrent acquirer's LOCK_NB attempt fail spuriously. The flock
    taken by acquirers themselves stays the only authority."""
    info = read_record(board)
    pid = info.get('pid') if isinstance(info, dict) else None
    if not isinstance(pid, int) or pid <= 0:
        return False
    try:
        os.kill(pid, 0)
    except ProcessLookupError:
        return False
    except PermissionError:
        return True  # alive but owned by another user (e.g. the CI runner)
    return True


def cmd_hold(boards, reason):
    os.makedirs(BOARD_LOCK_DIR, exist_ok=True)
    # No pre-check: the holder's own LOCK_NB flock is the only authority, since a recorded
    # pid may be stale or recycled. The holder signals success through this pipe because a
    # generic is_locked() poll would be fooled by a RIVAL invocation's flock — only the
    # holder knows whether it won every board.
    r_fd, w_fd = os.pipe()
    pid = os.fork()
    if pid > 0:
        os.close(w_fd)
        os.waitpid(pid, 0)  # reap intermediate child
        ready, _, _ = select.select([r_fd], [], [], 10)
        ok = bool(ready) and os.read(r_fd, 1) == b'1'
        os.close(r_fd)
        if ok:
            print(f'held: {", ".join(boards)}')
            return 0
        for b in boards:
            info = read_record(b)
            if info:
                print(f'ERROR: {b} locked: {info}', file=sys.stderr)
        print('ERROR: holder failed to acquire locks', file=sys.stderr)
        return 1
    # intermediate child: detach, then spawn the actual holder
    os.setsid()
    if os.fork() > 0:
        os._exit(0)
    # holder (grandchild): acquire all flocks, signal the parent, sleep until killed
    os.close(r_fd)
    # Keep the success pipe clear of fds 0-2: invoked with stdio closed, os.pipe() can
    # land there and the dup2 loop below would clobber it.
    if w_fd <= 2:
        w_fd = fcntl.fcntl(w_fd, fcntl.F_DUPFD, 3)
    # Detach stdio: a `hold` whose output is captured must see EOF when the front-end
    # exits — the immortal holder must not keep that pipe open.
    devnull = os.open(os.devnull, os.O_RDWR)
    for std_fd in (0, 1, 2):
        os.dup2(devnull, std_fd)
    if devnull > 2:
        os.close(devnull)
    try:
        handles = []
        for b in boards:
            fh = flock_nb(b)
            if not write_record(fh, reason):
                raise OSError(f'cannot write holder record for {b}')
            handles.append(fh)
    except OSError:
        try:
            os.write(w_fd, b'0')
        except OSError:
            pass
        os._exit(1)  # lost a race; parent reports the failure
    os.write(w_fd, b'1')
    os.close(w_fd)

    def _bow_out(*_):
        # clear the records before dying so read_record/status stay truthful (the kernel
        # drops the flocks themselves on exit either way)
        for h in handles:
            clear_record(h)
        os._exit(0)

    signal.signal(signal.SIGTERM, _bow_out)
    while True:
        signal.pause()


def cmd_release(boards):
    rc = 0
    victims = set()
    for b in boards:
        try:
            fd = os.open(lock_path(b), os.O_RDWR)
        except OSError:
            continue  # no lock file (or another user's): nothing we can release
        fh = os.fdopen(fd, 'r+')
        try:
            fcntl.flock(fh, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except OSError:
            # flock genuinely held — never SIGTERM on a mere pid record: the pid may be
            # recycled, or a live worker that already moved on.
            fh.close()
            info = read_record(b) or {}
            pid = info.get('pid')
            reason = info.get('reason')
            if reason in PROTECTED_REASONS:
                print(f'ERROR: {b} is mid-test by {reason} (pid {pid}) — not killing it; '
                      'wait for it to finish', file=sys.stderr)
                rc = 1
            elif isinstance(pid, int) and pid > 0:
                victims.add(pid)
            else:
                print(f'ERROR: {b} is held but its record is unreadable', file=sys.stderr)
                rc = 1
            continue
        # flock was free: only a stale record remained — clear it
        clear_record(fh)
        fh.close()
    for holder in sorted(victims):
        try:
            os.kill(holder, signal.SIGTERM)
            print(f'released holder pid {holder}')
        except ProcessLookupError:
            pass
        except PermissionError:
            print(f'ERROR: holder pid {holder} belongs to another user — cannot signal it',
                  file=sys.stderr)
            rc = 1
    time.sleep(0.3)
    still = [b for b in boards if is_locked(b)]
    if still:
        print(f'ERROR: still locked: {", ".join(still)}', file=sys.stderr)
        return 1
    return rc


def cmd_status():
    if not os.path.isdir(BOARD_LOCK_DIR):
        print('no locks')
        return 0
    any_locked = False
    for fn in sorted(os.listdir(BOARD_LOCK_DIR)):
        if not fn.endswith('.lock'):
            continue
        b = fn[:-5]
        if is_locked(b):
            any_locked = True
            print(f'{b}: {read_record(b)}')
    if not any_locked:
        print('no locks')
    return 0


_CLI_USAGE = """Per-board advisory locks for the HIL rig.

Arbitrates board access between dev sessions and CI's hil_test.py without
stopping the actions-runner. Locks are kernel flocks: the kernel releases
them automatically when the holder process dies, and holders clear their
lock-file record on release so records stay truthful (/tmp also clears on
reboot).

Usage:
  hil_lock.py hold BOARD [BOARD...] --reason TEXT
  hil_lock.py hold --all [--config CONFIG.json] --reason TEXT
  hil_lock.py release BOARD [BOARD...] | release --all
  hil_lock.py status

A holder process holds ALL boards given in one `hold` call; releasing any of
them kills that holder and releases all of its boards.
"""


def main():
    ap = argparse.ArgumentParser(description=_CLI_USAGE,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest='cmd', required=True)
    p_hold = sub.add_parser('hold')
    p_hold.add_argument('boards', nargs='*')
    p_hold.add_argument('--all', action='store_true')
    p_hold.add_argument('--config',
                        default=os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
                                             'tinyusb.json'),
                        help='board roster JSON (default: tinyusb.json in test/hil, one level above this script)')
    p_hold.add_argument('--reason', required=True)
    p_rel = sub.add_parser('release')
    p_rel.add_argument('boards', nargs='*')
    p_rel.add_argument('--all', action='store_true')
    sub.add_parser('status')
    a = ap.parse_args()
    if a.cmd == 'hold':
        boards = boards_from_config(a.config) if a.all else a.boards
        if not boards:
            ap.error('no boards given (name boards or use --all)')
        sys.exit(cmd_hold(boards, a.reason))
    if a.cmd == 'release':
        if a.all:
            boards = ([fn[:-5] for fn in os.listdir(BOARD_LOCK_DIR) if fn.endswith('.lock')]
                      if os.path.isdir(BOARD_LOCK_DIR) else [])
        else:
            boards = a.boards
        if not boards:
            ap.error('no boards given (name boards or use --all)')
        sys.exit(cmd_release(boards))
    sys.exit(cmd_status())


if __name__ == '__main__':
    main()
