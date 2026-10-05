#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Board locks for the TinyUSB HIL rig, plus the DUT -> host-controller resolver.

Board locks are kernel flocks in BOARD_LOCK_DIR arbitrating hardware access
between dev sessions and CI's hil_test.py (never stop the actions-runner).
controller_of feeds hil_test's dispatch-order cache and has no CLI meaning. The
CLI below (hold/release/status) manages board locks only.
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


# -------------------------------------------------------------
# DUT topology
# -------------------------------------------------------------
controller_map = None   # shared dict: 'uid:<uid>' -> pci addr, read back into the dispatch cache


def init_topology(cmap):
    """Install the shared topology map (called from hil_test.init_worker)."""
    global controller_map
    controller_map = cmap


def controller_of(uid: str):
    """Resolve a DUT uid to its root host controller's PCI address, or None when it cannot
    be resolved — the device is not enumerated (e.g. parked in board_test firmware with USB
    off), or sysfs would not answer. Successful resolutions are cached — cabling does not
    change mid-run. Dual-port parts (e.g. CH32V307 usbhs/usbfs variants) share one uid and
    one cache entry, the first port seen."""
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
