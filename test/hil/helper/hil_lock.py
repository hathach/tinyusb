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
import errno
import fcntl
import json
import os
import re
import stat
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


# --- wedged-board markers -----------------------------------------------------
# A board whose usbfs node still had a D-state holder when hil_test.py finished with it
# (a CONFIRMED wedge, usbtest's `wedge_confirmation`) is marked here so the next run
# refuses it in seconds instead of flashing into the poisoned node and paying the pool
# guard again. Same namespace and lifetime as the flocks: /tmp, cleared by the reboot
# that also clears the wedge. Written only under the board's flock by the run that saw
# it; cleared only with recovery evidence, never as a bare delete.
_BOARD_NAME_RE = re.compile(r'^[A-Za-z0-9_.-]+$')
WEDGED_SUFFIX = '.wedged'


def wedged_path(board: str) -> str:
    if not _BOARD_NAME_RE.fullmatch(board or ''):
        raise ValueError(f'not a board name: {board!r}')
    return os.path.join(BOARD_LOCK_DIR, f'{board}{WEDGED_SUFFIX}')


def _refused(board: str, why: str) -> dict:
    return {'board': board, 'reason': f'marker {why}; refused'}


def is_untrusted(marker: dict) -> bool:
    """A marker read_wedged could not trust (see _refused), not one this code wrote."""
    return str(marker.get('reason', '')).endswith('; refused')


def read_wedged(board: str):
    """The marker as a dict, None when the board is not marked. Fail CLOSED: a marker that
    exists but cannot be trusted (a symlink, not a regular file, another owner, unreadable,
    malformed) still counts as marked, with the problem in `reason`, so a broken marker never
    admits a board. Opened O_NOFOLLOW and checked on the open fd: no islink-then-open race."""
    path = wedged_path(board)
    try:
        # O_NONBLOCK: a FIFO planted here would otherwise block the open for ever, before
        # fstat can say it is not a regular file
        fd = os.open(path, os.O_RDONLY | os.O_NOFOLLOW | os.O_NONBLOCK | os.O_CLOEXEC)
    except FileNotFoundError:
        return None
    except OSError as e:
        return _refused(board, 'is a symlink' if e.errno == errno.ELOOP else f'unreadable ({e.strerror})')
    try:
        st = os.fstat(fd)
        if not stat.S_ISREG(st.st_mode):
            return _refused(board, 'is not a regular file')
        if st.st_uid != os.geteuid():
            return _refused(board, f'is owned by uid {st.st_uid}, not us')
        with os.fdopen(fd, 'r') as f:
            fd = -1
            info = json.load(f)
    except (OSError, ValueError) as e:
        return _refused(board, f'unreadable ({type(e).__name__})')
    finally:
        if fd >= 0:
            os.close(fd)
    if not isinstance(info, dict) or not info.get('reason'):
        return _refused(board, 'malformed')
    return info


def holds_board(board: str, lock_fh) -> bool:
    """Is `lock_fh` a reservation of THIS board? The handle must be open, be the board's lock
    file (same device and inode) and take LOCK_EX|LOCK_NB on it: that succeeds for the open
    file description that already holds the flock, and for one that finds it free, which
    holds it from here on -- either way the caller then owns the board. It fails while
    anyone else holds it. A closed or unrelated handle is not a reservation."""
    try:
        if lock_fh is None or lock_fh.closed:
            return False
        own, disk = os.fstat(lock_fh.fileno()), os.stat(lock_path(board))
        if (own.st_dev, own.st_ino) != (disk.st_dev, disk.st_ino):
            return False
        fcntl.flock(lock_fh, fcntl.LOCK_EX | fcntl.LOCK_NB)
        return True
    except (OSError, ValueError):
        return False


def atomic_write(path: str, data: bytes) -> bool:
    """Write `path` through an exclusive temporary file and one replace: never follows a
    planted symlink, never leaves a torn file. False on any failure."""
    tmp = f'{path}.{os.getpid()}.tmp'
    try:
        os.makedirs(os.path.dirname(path), exist_ok=True)
        fd = os.open(tmp, os.O_WRONLY | os.O_CREAT | os.O_EXCL | os.O_NOFOLLOW | os.O_CLOEXEC, 0o644)
        # buffered and closed before the replace: a short write (quota, ENOSPC, a size
        # limit) raises here and the previous target survives untouched
        with os.fdopen(fd, 'wb') as f:
            f.write(data)
        if os.path.getsize(tmp) != len(data):
            raise OSError('short write')
        if os.path.islink(path):
            raise OSError('target is a symlink')
        os.replace(tmp, path)
        return True
    except OSError:
        try:
            os.unlink(tmp)
        except OSError:
            pass
        return False


def write_wedged(board: str, info: dict, lock_fh) -> bool:
    """Mark the board. `lock_fh` must hold the board's flock (holds_board): with none, the
    fail-open or HIL_NO_BOARD_LOCK path, nothing is written and False comes back, because a
    marker written outside a reservation could race a run that just took the board. Atomic
    and symlink-proof: an exclusive temporary file, then replace; a reader sees the old
    marker or the new one, never a torn file."""
    if not holds_board(board, lock_fh):
        return False
    record = dict(info, board=board, since=info.get('since') or time.strftime('%Y-%m-%dT%H:%M:%S%z'))
    return atomic_write(wedged_path(board), json.dumps(record).encode())


def check_evidence(board: str, marker: dict, evidence) -> str:
    """'' when `evidence` verifies recovery of THIS marker, else why not: a complete holder
    scan with no holder, the same board, the marker's device uid, and the re-enumerated
    identity (serial@busport) the recovery observed."""
    if not isinstance(evidence, dict):
        return 'evidence must be a JSON object'
    if evidence.get('board') != board:
        return f'evidence names board {evidence.get("board")!r}, marker is {board}'
    if marker.get('uid') and evidence.get('uid') != marker['uid']:
        return f'evidence uid {evidence.get("uid")!r} is not the marker\'s {marker["uid"]}'
    if evidence.get('holders') not in ([], ()) or evidence.get('complete') is not True:
        return 'evidence must show a complete holder scan that found no holder'
    identity = evidence.get('identity')
    serial, _, busport = identity.partition('@') if isinstance(identity, str) else ('', '', '')
    if not serial or not busport:
        return 'evidence must name the re-enumerated identity as <serial>@<busport>'
    observed = (marker.get('evidence') or {}).get('serial')
    if observed and serial != observed:
        return f'evidence serial {serial!r} is not the one the wedge was observed on ({observed})'
    return ''


def clear_wedged(board: str, evidence: dict, lock_fh=None) -> str:
    """Remove the marker; '' on success, else why not. Needs evidence that verifies THIS
    marker (check_evidence) and the board reserved: `lock_fh` holding the board's flock,
    or none, in which case the flock is taken here and refused while anyone holds it."""
    path = wedged_path(board)
    own = lock_fh is None
    if own:
        try:
            lock_fh = flock_nb(board)
        except OSError:
            info = read_record(board) or {}
            return (f'board held by {info.get("reason", "another holder")} '
                    f'(pid {info.get("pid")}); release it first')
    elif not holds_board(board, lock_fh):
        return 'the given handle does not hold this board\'s lock'
    try:
        marker = read_wedged(board)
        if marker is None:
            return 'not marked'
        if is_untrusted(marker):
            # not a marker this code wrote: nothing to match evidence against, so no
            # evidence verifies it -- a human removes the file after looking at it
            return f'untrusted marker ({marker["reason"]}); inspect and remove it by hand'
        why = check_evidence(board, marker, evidence)
        if why:
            return why
        os.unlink(path)
        return ''
    except OSError as e:
        return f'cannot clear: {e}'
    finally:
        if own:
            lock_fh.close()


def wedged_boards() -> list:
    """(board, info) for every marker in the lock dir."""
    if not os.path.isdir(BOARD_LOCK_DIR):
        return []
    out = []
    for fn in sorted(os.listdir(BOARD_LOCK_DIR)):
        if fn.endswith(WEDGED_SUFFIX):
            board = fn[:-len(WEDGED_SUFFIX)]
            if not _BOARD_NAME_RE.fullmatch(board):
                out.append((board, _refused(board, 'has an invalid name')))
                continue
            out.append((board, read_wedged(board) or {}))
    return out


def cmd_wedged(sub: str, board: str = '', evidence: str = '') -> int:
    if sub == 'status':
        marks = wedged_boards()
        for b, info in marks:
            print(f'{b}: {json.dumps(info)}')
        if not marks:
            print('no board is marked wedged')
        return 0
    try:
        ev = json.loads(evidence) if evidence else {}
    except ValueError:
        print('--evidence is not JSON', file=sys.stderr)
        return 2
    why = clear_wedged(board, ev)
    if why:
        print(f'{board}: not cleared: {why}', file=sys.stderr)
        return 1
    print(f'cleared: {board}')
    return 0


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
  hil_lock.py wedged status
  hil_lock.py wedged clear BOARD --evidence '{"board": "BOARD", "uid": "<marker uid>", "holders": [], "complete": true, "identity": "<serial>@<busport>"}'

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
    p_w = sub.add_parser('wedged', help='markers left by a confirmed wedge; see the hil skill')
    p_w.add_argument('sub', choices=['status', 'clear'])
    p_w.add_argument('board', nargs='?', default='')
    p_w.add_argument('--evidence', default='',
                     help='JSON from the recovery that verified THIS board: {"board": "BOARD", '
                          '"uid": "<marker uid>", "holders": [], "complete": true, '
                          '"identity": "<serial>@<busport>"}')
    a = ap.parse_args()
    if a.cmd == 'wedged':
        if a.sub == 'clear' and not a.board:
            ap.error('wedged clear needs a board')
        sys.exit(cmd_wedged(a.sub, a.board, a.evidence))
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
