#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Bottom layer of the HIL harness: the bounded command runner plus the shared helpers and
# data every other module needs. Stays stdlib-only and imports nothing local -- everything
# else imports this, including the unit tests on GitHub's bare runner; never import them
# from here. Callers set the module global `verbose`.

from __future__ import annotations

import glob
import os
import signal
import subprocess
import threading
import sys
from pathlib import Path
from typing import Any


# -------------------------------------------------------------
# HIL example test lists, shared by hil_test.py (runner) and ci_select.py (PR-diff
# selector). Run order is shuffled per board (see test_board); every example carries a
# unique hardcoded idProduct (see its usb_descriptors.c).
# -------------------------------------------------------------

# device tests
device_tests = [
    'device/cdc_dual_ports',
    'device/cdc_msc',
    'device/dfu',
    'device/cdc_msc_throughput',
    'device/audio_test_freertos',
    'device/dfu_runtime',
    'device/cdc_msc_freertos',
    'device/hid_boot_interface',
    'device/msc_dual_lun',
    'device/hid_generic_inout',
    'device/printer_to_cdc',
    'device/midi_test',
    'device/mtp',
    'device/usbtest',  # cafe:4010, unique PID; runs the Linux testusb tier-4 battery via usbtest.py
    # 'device/net_lwip_webserver',  # disabled for PR #3605: USB net iface enum is flaky on the CI HIL host
]

dual_tests = [
    'dual/host_info_to_device_cdc',
]

host_test = [
    'host/cdc_msc_hid',
    'host/msc_file_explorer',
    'host/msc_file_explorer_freertos',
    'host/device_info',
]

verbose = False

def pos_int_env(name: str, default: int) -> int:
    # One parsing policy for every HIL_* knob: a bare int() crashes every run at import
    # on a malformed value, and 0/negative silently removes the bound the knob enforces.
    try:
        v = int(os.getenv(name, str(default)))
    except ValueError:
        print(f'warning: {name} is not an integer; using {default}',
              file=sys.stderr, flush=True)
        return default
    if v <= 0:
        print(f'warning: {name}={v} is not usable; using {default}',
              file=sys.stderr, flush=True)
        return default
    return v


def pos_float_env(name: str, default: float) -> float:
    try:
        v = float(os.getenv(name, str(default)))
    except ValueError:
        print(f'warning: {name} is not a number; using {default}',
              file=sys.stderr, flush=True)
        return default
    # float() accepts 'inf'/'nan': an infinite serial timeout is an unbounded read, the
    # very thing these knobs exist to prevent, and nan fails every comparison silently
    if not (v > 0 and v < float('inf')):
        print(f'warning: {name}={v} is not usable; using {default}',
              file=sys.stderr, flush=True)
        return default
    return v


CMD_TIMEOUT = pos_int_env('HIL_CMD_TIMEOUT', 180)

TINYUSB_ROOT = Path(__file__).resolve().parents[3]  # test/hil/helper/ -> repo root


def cmd_stdout_text(out: Any) -> str:
    if out is None:
        return ''
    if isinstance(out, bytes):
        return out.decode('utf-8', errors='ignore')
    return str(out)


def _banner_body(out: Any, err: Any) -> str:
    # split_stderr callers keep the diagnostic in stderr — a banner of stdout alone
    # would be blank exactly when something went wrong
    body = cmd_stdout_text(out)
    err_text = cmd_stdout_text(err)
    if err_text:
        body = f'{body}\n{err_text}' if body else err_text
    return body


# Shared with compact_output's stripper in hil_test: duplicated literals let the two
# layers drift and reintroduce literal marker noise mid-row in the GitHub log.
GROUP_MARK, ENDGROUP_MARK = '::group::', '::endgroup::'


def strip_workflow_markers(line: str) -> str:
    # run_cmd only ever emits markers at line start; mid-line is not a real case.
    return line.removeprefix(GROUP_MARK).removeprefix(ENDGROUP_MARK)


def _ci_log_groups() -> bool:
    # GitHub folds ::group::/::endgroup:: only at line start of the JOB's real stdout; a
    # pool worker's capture is compacted into one row line, where they render literally.
    return bool(os.getenv('CI')) and sys.stdout is sys.__stdout__


def _print_banner(title: str, out: Any, err: Any) -> None:
    print()
    if _ci_log_groups():
        print(f'{GROUP_MARK}{title}')
        print(_banner_body(out, err))
        print(ENDGROUP_MARK)
    else:
        print(title)
        print(_banner_body(out, err))


SYSFS_READ_GRACE = 2.0   # bound on one attribute read of a possibly-wedged device
SYSFS_STUCK_MAX = 4      # stranded readers tolerated before read_sysfs goes blind
_sysfs_stuck = 0         # each costs a thread + an fd for the life of the process
_sysfs_stuck_lock = threading.Lock()
_sysfs_blind_logged = False


class _SysfsUnknown:
    """Sentinel: the read did not answer. NOT "the attribute is absent" -- reading it as
    absence turns a healthy board into a firmware regression in the report."""
    __slots__ = ()

    def __bool__(self) -> bool:
        return False

    def __repr__(self) -> str:
        return 'SYSFS_UNKNOWN'


SYSFS_UNKNOWN = _SysfsUnknown()


def sysfs_blind() -> bool:
    """True once this process has stranded SYSFS_STUCK_MAX readers: every later read
    answers SYSFS_UNKNOWN, so nothing it reports about a device is a fact any more."""
    return _sysfs_stuck >= SYSFS_STUCK_MAX


def sysfs_blind_note() -> str:
    """Suffix for a failure message, so a blind worker's verdict never reads as hardware."""
    return (f' (this worker is blind: {SYSFS_STUCK_MAX} sysfs reads stranded on a wedged '
            f'device, so the check could not see the bus)') if sysfs_blind() else ''


def read_sysfs(path: str, grace: float = SYSFS_READ_GRACE) -> str | None | _SysfsUnknown:
    """Read a sysfs attribute with a WALL-CLOCK bound.

    The value, None when the attribute is genuinely unreadable (OSError), or SYSFS_UNKNOWN
    when the read did not answer -- it timed out, or this process is already blind. Callers
    MUST keep those apart: absence is a fact, unknown is not.

    usb_string_attr (serial/product/manufacturer) is served under the device lock a wedged
    usbfs ioctl holds, so a plain open().read() blocks for as long as the wedge lasts, on
    exactly the board an incident is about. The reader sleeps INTERRUPTIBLY (every read
    takes usb_lock_device_interruptible, v6.12.96 sysfs.c:124-139 -- uninterruptible is the
    ioctl holder, not us), so it dies with a SIGKILLed worker; what it costs meanwhile is a
    thread and an fd for this process's life, because on sysfs the open() SUCCEEDS and only
    the read blocks. Measured: 20 blocking reads leave 20 live threads.

    Hence the cap: callers rescan (hil_lock's controller_of re-reads every unresolved
    device on EVERY permit), and hitting RLIMIT_NOFILE or the thread ceiling raises inside
    the worker and loses every board's result -- worse than the hang this prevents.
    """
    if sysfs_blind():
        return SYSFS_UNKNOWN
    # Known-stranded? Re-reading costs another permanent thread+fd and a blindness credit
    # to learn what we already know. Lives HERE, not at the call sites: a call-site memo
    # has to be remembered by every new scanner, and twice it was not.
    was = _sysfs_stranded.get(path, _STRAND_MISS)
    if was is not _STRAND_MISS:
        if was is None:
            return SYSFS_UNKNOWN          # stranded, inode unknown: never re-read it
        try:
            if os.stat(path).st_ino == was:
                return SYSFS_UNKNOWN          # same node, still wedged
        except OSError:
            pass                              # gone: fall through, the read reports it
        _sysfs_stranded.pop(path, None)       # replaced or gone -> re-read it
    out: dict = {}

    def _read():
        try:
            with open(path) as f:
                out['v'] = f.read().strip()
        except (OSError, ValueError):
            pass      # no such attribute, or not text: unreadable, and that IS a fact

    t = threading.Thread(target=_read, daemon=True)
    t.start()
    t.join(grace)
    # `out` FIRST, not is_alive() alone: a reader can deposit its value and still be alive
    # for a moment afterwards, and counting that as a strand memoises a healthy attribute as
    # unreadable and spends one of four blindness credits. bounded_open has always checked
    # its box for the same reason.
    if t.is_alive() and 'v' not in out:
        # Count the PATH once, not once per reader. hil_pool_check runs -j4 by default,
        # which equals SYSFS_STUCK_MAX, so four threads hitting ONE wedged device used to
        # spend the entire blindness budget between them -- latching blind on the single
        # wedge the tool was run to find. The strand is real for each thread, but the
        # DEVICE is what the cap is about.
        # Under the SAME lock as the counter: check-then-act here is a race, and
        # hil_pool_check runs a ThreadPoolExecutor of exactly SYSFS_STUCK_MAX workers in
        # ONE process, so four threads on one wedged path could each see `first` before any
        # of them recorded it -- spending the whole blindness budget on a single device,
        # which is what this memo exists to prevent. note_sysfs_strand takes the lock
        # itself, so call it after releasing.
        with _sysfs_stuck_lock:
            first = path not in _sysfs_stranded
            if first:
                try:
                    # stat, never the thread's own open(): stat does not call ->show(), so
                    # it cannot block on the device lock the reader is stuck behind
                    _sysfs_stranded[path] = os.stat(path).st_ino
                except OSError:
                    _sysfs_stranded[path] = None   # unstattable, but still known-stranded
        if first:
            note_sysfs_strand()
        return SYSFS_UNKNOWN
    return out.get('v')


def note_sysfs_strand() -> None:
    """Record ONE stranded sysfs reader. Shared by read_sysfs and bounded_open so both
    account against a single counter -- the report caveat keys off it."""
    global _sysfs_stuck, _sysfs_blind_logged
    with _sysfs_stuck_lock:
        _sysfs_stuck += 1
        announce = sysfs_blind() and not _sysfs_blind_logged
        _sysfs_blind_logged = _sysfs_blind_logged or announce
    if announce:
        # once per process, on stderr: a worker's stdout is compacted into one report
        # row, where this would be lost among the test output
        print(f'warning: {SYSFS_STUCK_MAX} sysfs reads stranded on a wedged device; '
              f'this process is now blind and answers SYSFS_UNKNOWN for every '
              f'attribute -- its verdicts about device presence are not evidence',
              file=sys.stderr, flush=True)


# path -> the inode it had when its read stranded. A stranded attribute stays
# stranded until the DEVICE is replaced, and a re-enumeration destroys the kernfs
# node and makes a new one -- so a changed inode is the all-clear. Keyed by path
# alone it would outlive the wedge: a busport does not change when a board comes
# back on the same port, so the HUNG reflash this branch performs would recover a
# board the harness could then never see again.
_sysfs_stranded: dict = {}
# A stranded path whose inode could not be read is stored as None, so a plain .get() cannot
# tell 'known stranded, inode unknown' from 'never seen' -- and treating the first as the
# second re-reads it, stranding another permanent thread and fd every call. Distinct miss
# sentinel, so None keeps its own meaning.
_STRAND_MISS = object()


def usb_scan(vid_pid=None, serial=None, vid=None) -> tuple[list, bool]:
    """Enumerated USB devices matching the filters, and whether anything is unknown.

    Returns ([{busport, dir, vid, pid, serial}], unknown). `unknown` True means a bounded
    read did not answer, so absence is NOT proven -- the same contract as read_sysfs.

    Three rules, one implementation for every caller:

    * Root hubs excluded (glob `*-*`): no DUT is one, and scans including them measured
      seconds slower (observation, no mechanism -- the "autosuspend wake" explanation was
      wrong; usb_string_attr reads a cached string, sysfs.c:124-139).
    * idVendor/idProduct first: lock-free `sysfs_emit` from udev->descriptor
      (sysfs.c:688-705), so they rule out nearly every device for free.
    * `serial` last and bounded: it is served under the lock a wedged ioctl holds, and a
      path that already stranded is never re-read (each strand costs a thread and an fd
      for this process's life).
    """
    out = []
    unknown = False
    for d in glob.glob('/sys/bus/usb/devices/*-*'):
        # Interfaces are '<busport>:<cfg>.<ifnum>' (e.g. 2-4:1.0) -- they CONTAIN the
        # colon, they do not end with it, so the original endswith() never fired and every
        # scan opened idVendor/idProduct on all of them (measured: 31 of 44 matches).
        if ':' in os.path.basename(d):
            continue
        try:
            with open(os.path.join(d, 'idVendor')) as f:
                dev_vid = f.read().strip()
            with open(os.path.join(d, 'idProduct')) as f:
                dev_pid = f.read().strip()
        except OSError:
            continue          # vanished mid-walk, or not a device dir: a fact, not unknown
        if vid_pid is not None and (dev_vid, dev_pid) != tuple(vid_pid):
            continue          # ruled out for free, without touching the locked attribute
        if vid is not None and dev_vid != vid:
            continue          # same, for callers that know the VID but not the PID
        sn = read_sysfs(os.path.join(d, 'serial'))
        if sn is SYSFS_UNKNOWN:
            unknown = True    # read_sysfs memoises it; a repeat scan costs nothing
            continue
        if sn is None:
            continue          # no serial attribute: a fact
        if serial is not None and sn.lower() != serial.lower():
            continue
        out.append({'busport': os.path.basename(d), 'dir': d,
                    'vid': dev_vid, 'pid': dev_pid, 'serial': sn})
    return out, unknown


def bounded_open(path: str, flags: int, timeout: float = SYSFS_READ_GRACE):
    """os.open() with a wall-clock bound.

    The fd, None when the open genuinely FAILED (OSError: EBUSY, ENOENT, EACCES), or
    SYSFS_UNKNOWN when it did not answer -- the same three-valued contract as read_sysfs,
    and for the same reason: folding a fact into an unknown made an ordinary EBUSY read as
    a wedged device and sent the operator hunting hardware that is healthy.

    An open CAN block on a wedged device -- not on O_NONBLOCK, which usblp_open never
    consults, but on usb_autopm_get_interface(), a runtime-PM resume that does I/O
    (v6.12.96 drivers/usb/class/usblp.c). It holds usblp_mutex while it waits, and that
    mutex is driver-GLOBAL, so one wedged printer blocks opens of every usblp node.

    Unlike read_sysfs the stranded thread cleans up after itself: if we have given up it
    closes the fd it eventually got, so only the thread leaks. Both sides take `handoff`
    -- "store or close" and "abandon and drain" are a check-then-act pair that can
    interleave into an fd stored after the box was drained, which would leak it into a
    node that allows a SINGLE opener (usblp_open returns -EBUSY when usblp->used).
    """
    # Same short-circuit as read_sysfs: once blind, another stranded thread buys nothing
    # and the cap exists precisely to stop them accumulating.
    if sysfs_blind():
        return SYSFS_UNKNOWN
    # Known-stranded? Re-opening costs another thread, another fd and another blindness
    # credit to learn what we already know -- and the printer test re-opens ONE lp node on
    # every retry. Same memo and same inode check as read_sysfs.
    was = _sysfs_stranded.get(path, _STRAND_MISS)
    if was is not _STRAND_MISS:
        if was is None:
            return SYSFS_UNKNOWN          # stranded, inode unknown: never re-read it
        try:
            if os.stat(path).st_ino == was:
                return SYSFS_UNKNOWN
        except OSError:
            pass
        _sysfs_stranded.pop(path, None)
    box: dict = {}
    done, abandoned = threading.Event(), threading.Event()
    handoff = threading.Lock()

    def _open():
        try:
            fd = os.open(path, flags)
        except OSError:
            done.set()
            return
        with handoff:
            stored = not abandoned.is_set()
            if stored:
                box['fd'] = fd
        if not stored:
            try:
                os.close(fd)
            except OSError:
                pass
        done.set()

    threading.Thread(target=_open, daemon=True).start()
    if not done.wait(timeout):
        with handoff:
            abandoned.set()
            fd = box.pop('fd', None)  # completed in the gap between timeout and flag
        if fd is not None:
            # It DID open, just after our deadline -- the thread finished, so nothing is
            # stranded. Report unknown (we already gave up on it) but do not spend a
            # blindness credit, and do not call a merely-slow node wedged.
            try:
                os.close(fd)
            except OSError:
                pass
            return SYSFS_UNKNOWN
        # counted like a stranded read_sysfs: the thread and (eventually) its fd are gone
        # for the life of the process, and the cap exists to stop that reaching the
        # thread/fd ceiling -- an exception there escapes the worker and loses every board.
        # Memoised by inode so a retry of the same node does not pay again.
        # same lock as read_sysfs, same reason
        with _sysfs_stuck_lock:
            first = path not in _sysfs_stranded
            if first:
                try:
                    _sysfs_stranded[path] = os.stat(path).st_ino
                except OSError:
                    _sysfs_stranded[path] = None
        if first:
            note_sysfs_strand()
        return SYSFS_UNKNOWN
    return box.get('fd')


def _close_pipes(p: subprocess.Popen) -> None:
    """Close OUR ends of an abandoned child's pipes. Never raises."""
    for pipe in (p.stdout, p.stderr, p.stdin):
        try:
            if pipe is not None:
                pipe.close()
        except OSError:
            pass


def run_alongside(argv: list, work, timeout: int) -> subprocess.CompletedProcess:
    """Run `argv` alongside `work()`, which runs in THIS thread, then reap it -- bounded.

    The read-while-we-write shape run_cmd cannot express: the caller needs the child
    RUNNING while it does something else. Everything else about the contract is run_cmd's
    -- own session, killpg, bounded reap, our pipe ends closed, rc 124 on the kill.

    A PROCESS, not a thread: an abandoned thread keeps the fd, and usblp_open returns
    -EBUSY while usblp->used (v6.12.96 usblp.c), so every later open in this long-lived
    worker would read as a wedged device. A killed process takes its fd with it.

    stdout is captured as BYTES and kept CLEAN -- a caller byte-compares it against the
    payload it sent, so a single stderr byte (a PYTHONWARNINGS chirp, a sitecustomize
    print, a .pth deprecation from a venv) would read as USB data corruption. stderr gets
    its own pipe; communicate() drains both, so the split cannot deadlock.
    `work` runs even if the child dies immediately -- the caller's own asserts decide.
    """
    p = subprocess.Popen(argv, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                         start_new_session=True)

    def _reap() -> subprocess.CompletedProcess:
        try:
            out, err = p.communicate(timeout=timeout)
            return subprocess.CompletedProcess(argv, p.returncode, out, err)
        except subprocess.TimeoutExpired:
            try:
                os.killpg(p.pid, signal.SIGKILL)
            except OSError:
                p.kill()
            try:
                out, err = p.communicate(timeout=5)
            except subprocess.TimeoutExpired:
                # Outlasted SIGKILL: uninterruptible, still holding whatever it opened.
                # Abandoned like any other stray -- but as a real child in its own
                # session, so the containment sweep FINDS it (child_procs walks the ppid
                # tree) and the report names it. That is the whole difference from a
                # blocked thread, which no sweep can see and no signal can reach.
                out, err = b'', b''
                _close_pipes(p)     # our own fds must not leak either
            return subprocess.CompletedProcess(argv, 124, out, err)

    try:
        work()
    except BaseException:
        # Reap first so the child never outlives us, then let the caller's error through.
        # A `return` inside a `finally` would SWALLOW it -- an assert in `work` would
        # vanish and the caller would compare data it never finished sending.
        _reap()
        raise
    return _reap()


def run_cmd(cmd: str, cwd: str | None = None, timeout: int | None = None,
            binary: bool = False, split_stderr: bool = False,
            quiet: bool = False) -> subprocess.CompletedProcess:
    if timeout is None:
        timeout = CMD_TIMEOUT
    # binary: raw bytes (text mode's errors='replace' mangles non-UTF-8 file content).
    # split_stderr: keep stderr out of stdout, for callers that parse stdout. quiet: no
    # COMMAND FAILED banner, for retry loops that report failures themselves (timeouts
    # still print: a killed child is always noteworthy).
    popen_kwargs = {
        'cwd': cwd,
        'shell': True,
        'stdout': subprocess.PIPE,
        'stderr': subprocess.PIPE if split_stderr else subprocess.STDOUT,
    }
    if not binary:
        popen_kwargs.update({'text': True, 'encoding': 'utf-8', 'errors': 'replace'})
    if os.name != 'nt':
        # C-level setsid, same process-group semantics as preexec_fn=os.setsid but
        # safe when called from threads (pool_check runs flashes from a thread pool)
        popen_kwargs['start_new_session'] = True

    p = subprocess.Popen(cmd, **popen_kwargs)
    try:
        out, err = p.communicate(timeout=timeout)
        r = subprocess.CompletedProcess(args=cmd, returncode=p.returncode, stdout=out, stderr=err)
    except subprocess.TimeoutExpired as ex:
        if os.name != 'nt':
            try:
                os.killpg(p.pid, signal.SIGKILL)
            except OSError:
                # ProcessLookupError: already gone. PermissionError: an all-root group
                # refuses the group kill -- letting either escape would skip the bounded
                # reap, the pipe close and the rc-124 return this handler exists for.
                pass
        else:
            p.kill()
        try:
            out, err = p.communicate(timeout=10)
        except subprocess.TimeoutExpired:
            # Something in the group outlived SIGKILL: D state (truly unkillable), or
            # root-owned because sudo FORKS rather than execs, so the wrapper dies and its
            # root child does not. Abandon it and let the report name it; the harness never
            # sudo-kills its way out. Our ends of its pipes must not leak, though: a pool
            # worker lives for the whole run, so every wedged command would cost it two fds.
            out, err = None, None
            _close_pipes(p)
        # prefer the post-kill buffers (supersets of the exception's), falling back to ex.*
        # when the child was unkillable. TimeoutExpired carries BYTES even for a text-mode
        # Popen, so the fallbacks must be decoded or a text-mode caller gets bytes exactly
        # when the child wedged in D state.
        def _typed(v):
            if not binary and isinstance(v, bytes):
                return v.decode('utf-8', errors='replace')
            return v

        timeout_out = _typed(out or ex.stdout) or (b'' if binary else '')
        # ...and never None: with split_stderr the SUCCESS path always yields a str/bytes,
        # so a caller that does `r.stderr.strip()` works everywhere except the timeout --
        # the one path it was written for. Without split_stderr stderr stays None, as on
        # the success path (it was merged into stdout).
        timeout_err = _typed(err if err is not None else ex.stderr)
        if split_stderr and timeout_err is None:
            timeout_err = b'' if binary else ''
        _print_banner(f'COMMAND TIMEOUT ({timeout}s): {cmd}', timeout_out, timeout_err)
        return subprocess.CompletedProcess(args=cmd, returncode=124, stdout=timeout_out, stderr=timeout_err)
    except BaseException:
        # BaseException, not Exception (as in CPython's own subprocess.run):
        # KeyboardInterrupt is the case that matters, and start_new_session put the child in
        # its OWN group, so it never got the terminal's SIGINT -- without this, Ctrl-C
        # leaves the flasher or testusb holding the probe and its usbfs node. Kill and
        # close, never wait: this path must not add a hang of its own.
        if os.name != 'nt':
            try:
                os.killpg(p.pid, signal.SIGKILL)
            except OSError:
                pass
        else:
            p.kill()
        _close_pipes(p)
        raise

    if r.returncode != 0 and not quiet:
        _print_banner(f'COMMAND FAILED: {cmd}', r.stdout, r.stderr)
    elif verbose:
        print(cmd)
        print(cmd_stdout_text(r.stdout))
    return r


# get usb serial by id
def get_serial_dev(id, vendor_str, product_str, ifnum):
    if vendor_str and product_str:
        # known vendor and product
        vendor_str = vendor_str.replace(' ', '_')
        product_str = product_str.replace(' ', '_')
        return f'/dev/serial/by-id/usb-{vendor_str}_{product_str}_{id}-if{ifnum:02d}'
    else:
        # just use id: mostly for cp210x/ftdi flasher
        pattern = f'/dev/serial/by-id/usb-*_{id}-if*'
        port_list = glob.glob(pattern)
        if len(port_list) == 0:
            raise RuntimeError(f'No serial device found for {pattern}')
        return port_list[0]
