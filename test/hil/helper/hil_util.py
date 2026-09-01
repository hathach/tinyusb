#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Bottom layer of the HIL harness: the bounded command runner plus the shared helpers and
# data every other module needs. Stays stdlib-only; its one local dependency is
# tools/rtt.py (the RTT console, loaded by path below) -- everything
# else imports this, including the unit tests on GitHub's bare runner; never import them
# from here. Callers set the module global `verbose`.

from __future__ import annotations

import glob
import os
import signal
import subprocess
import unicodedata
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
# Post-SIGKILL reap, spent ON TOP of a run_cmd timeout whenever the child has to be killed.
# A caller budgeting several bounded steps must add one of these PER STEP, or its own outer
# bound fires mid-step -- for a flasher, orphaning it on the probe.
REAP_GRACE = 10

TINYUSB_ROOT = Path(__file__).resolve().parents[3]  # test/hil/helper/ -> repo root


def display_width(s: str) -> int:
    """Terminal COLUMNS, not characters.

    The status marks the reports use -- ✅ ❌ ⚪ ⚠ 🔒 -- are one Python character and TWO
    columns wide. Measuring with len() pads every cell containing one a column short, so
    the pipes drift out of line against the header rule for the whole table.
    """
    return sum(2 if unicodedata.east_asian_width(c) in 'WF' else 1 for c in s)


def pad(s: str, width: int, center: bool = False) -> str:
    """str.ljust/center, measured in display columns. See display_width."""
    room = max(0, width - display_width(s))
    if not center:
        return s + ' ' * room
    left = room // 2
    return ' ' * left + s + ' ' * (room - left)


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


SYSFS_READ_GRACE = 2.0        # default bound on one attribute read; see read_sysfs

# path -> the kernfs inode the node had when its bounded read gave up. Keyed by INODE, not
# by path alone: a busport does not change when a board returns to the same physical port,
# so a path-only blacklist outlives the wedge -- hil_pool_check resets or reflashes the
# board, wait_device polls that busport for the new inode, and the scan it polls through
# would never look at the device again. A re-enumeration destroys the kernfs node and makes
# a new one, so a CHANGED inode is the all-clear. os.stat is safe on a wedged device: it
# does not call ->show(), so it cannot block on the lock the reader is stuck behind.
_stranded: dict = {}
_strand_hits: dict = {}       # path -> how many times it has stranded, ever
_refused: set = set()         # paths answered None WITHOUT reading, once past _STRAND_MAX
_strand_lock = threading.Lock()
_ever_stranded = False

# Each strand costs a thread AND an fd for the life of the process -- on sysfs the open()
# SUCCEEDS and only the read blocks. Two ceilings, because they bound different things:
#
# _PATH_STRAND_MAX -- a device that FLAPS while still wedged re-enumerates, clears the
#   inode memo, and strands again. Per path, so one sick board cannot leak without bound.
#   After this many it stays memoised whatever its inode says.
# _STRAND_MAX -- a whole-process backstop against RLIMIT_NOFILE or the thread ceiling,
#   which would raise inside a worker and lose every board's result. Counted PER PATH, not
#   per reader: hil_pool_check runs four poll threads over one bus, and counting each
#   reader let four threads on ONE wedged device spend four credits between them. With
#   per-path counting a 27-board rig cannot approach this.
_PATH_STRAND_MAX = 4
_STRAND_MAX = 64


def sysfs_stranded() -> bool:
    """True once any bounded read has given up, and it STAYS true.

    A sticky, process-wide fact, so it answers exactly one question: "could anything in
    this process's output be the tool losing sight of healthy hardware?" -- which is what
    hil_pool_check's footer needs. It canNOT answer "is THIS device unreadable" for a
    caller deciding what a single missing device means; use path_stranded() for that.
    """
    return _ever_stranded


def strand_note() -> str:
    """Suffix for an absence claim, so "not found" never reads as proven absence.

    Lives here because every caller that can say "not found" needs the same sentence, and
    the one that had to re-invent it got missed: a wedged-but-enumerated printer was
    reported as an enumeration failure, sending a maintainer after firmware.
    """
    return (' (a bounded sysfs read gave up, so "not found" here means "could not tell"'
            ' -- see the usb-kernel-recover skill)') if sysfs_stranded() else ''


def path_stranded(path: str) -> bool:
    """Whether THIS attribute is currently memoised as unreadable.

    The per-device question sysfs_stranded() cannot answer. usbtest uses it to tell a DUT
    whose `serial` is held under device_lock from one that genuinely left the bus, because
    the difference decides whether it performs driver-registry writes that take the
    UNINTERRUPTIBLE device_lock.
    """
    with _strand_lock:
        return path in _stranded or path in _refused


def read_sysfs(path: str, timeout: float = SYSFS_READ_GRACE) -> str | None:
    """A sysfs attribute's value, or None when it did not answer.

    BOUNDED BY DEFAULT, and it has to be. `serial` is served by usb_string_attr, which
    takes usb_lock_device_interruptible (v6.12.96 sysfs.c:141-143) -- the same lock a
    wedged usbfs ioctl holds. Every OTHER attribute the harness reads (idVendor, idProduct,
    bcdDevice, busnum, devnum, speed) is a lock-free sysfs_emit from a cached field and
    cannot block.

    "Only the wedged board's own worker pays" is FALSE, which is why the bound is not
    opt-in: usb_scan reads `serial` on every device matching the VID to find the one it
    wants, so resolving MY board touches every peer's locked attribute. hil_lock's
    controller_of does that from controller_permit, on essentially every board -- one
    wedged DUT would stall every worker, not one. hil_pool_check has no guard at all.

    A give-up reads as None, the same as unreadable: there is no third value and no
    per-attribute blindness. The memo is keyed by inode so the cost stays on the device
    that is actually wedged; path_stranded() tells a caller which device that was.
    """
    with _strand_lock:
        was = _stranded.get(path)
        stuck_for_good = _strand_hits.get(path, 0) >= _PATH_STRAND_MAX
        budget_spent = len(_stranded) >= _STRAND_MAX
    if was is not None:
        try:
            if os.stat(path).st_ino == was:
                return None                   # same kernfs node, still wedged
        except OSError:
            pass                              # gone: let the read below report it
        if stuck_for_good:
            return None       # flapped too many times; see _PATH_STRAND_MAX
        with _strand_lock:
            _stranded.pop(path, None)         # a different inode is the all-clear
    elif budget_spent:
        # see _STRAND_MAX. Recorded, not just returned: usbtest fails CLOSED on
        # path_stranded() before the lock-taking cleanup, and a path we declined to read
        # is exactly the case it must not be told is readable-and-absent.
        with _strand_lock:
            _refused.add(path)
        return None

    # BEFORE the read, not after: a node that re-enumerates DURING the grace would
    # otherwise have its brand-new HEALTHY inode recorded as the wedged one, and only a
    # second re-enumeration could ever clear it. If it cannot be stat'd there is no key to
    # memoise against, so the path is simply re-read next time -- the open fails fast.
    try:
        ino = os.stat(path).st_ino
    except OSError:
        ino = None
    out: dict = {}

    def _read():
        try:
            with open(path) as f:
                out['v'] = f.read().strip()
        except (OSError, ValueError):
            pass

    t = threading.Thread(target=_read, daemon=True)
    t.start()
    t.join(timeout)
    # `out` FIRST: a reader can deposit its value and still be alive for a moment
    # afterwards, and counting that as a strand blacklists a healthy attribute forever
    if 'v' in out:
        # a path that answered is not refused any more: _refused feeds path_stranded(),
        # and a stale entry makes usbtest read a LATER genuine disconnect as "cannot tell"
        with _strand_lock:
            _refused.discard(path)
    if t.is_alive() and 'v' not in out:
        global _ever_stranded
        announce = False
        if ino is None:
            # the pre-read stat lost a race the open then won -- the node was replaced
            # between them. Re-stat now: the reader is blocked on whatever node exists,
            # so this is the key it is stuck on. Without a key nothing is memoised and
            # every later poll starts another permanent thread and fd for this path.
            try:
                ino = os.stat(path).st_ino
            except OSError:
                pass
        with _strand_lock:
            _ever_stranded = True
            if ino is not None:
                first = path not in _stranded     # count the PATH once, not each reader
                _stranded[path] = ino
                if first:
                    _strand_hits[path] = _strand_hits.get(path, 0) + 1
                    announce = len(_stranded) == _STRAND_MAX
            else:
                _refused.add(path)    # unkeyable: at least do not vouch for it
        if announce:
            print(f'warning: {_STRAND_MAX} devices have unreadable sysfs attributes; '
                  f'refusing to start more bounded readers, so later reads answer None '
                  f'without looking. Find the wedged device (usb-kernel-recover skill).',
                  file=sys.stderr, flush=True)
        return None
    return out.get('v')


def usb_scan(vid_pid=None, serial=None, vid=None, timeout=SYSFS_READ_GRACE) -> list:
    """Enumerated USB devices matching the filters: [{busport, dir, vid, pid, serial}].

    Three rules, one implementation for every caller:

    * Root hubs excluded (glob `*-*`): no DUT is one, and scans including them measured
      seconds slower (observation, no mechanism -- the "autosuspend wake" explanation was
      wrong; usb_string_attr reads a cached string, sysfs.c:141-143).
    * idVendor/idProduct first: lock-free `sysfs_emit` from udev->descriptor
      (sysfs.c:688-705), so they rule out nearly every device for free.
    * `serial` LAST and BOUNDED: it is the only attribute here served under the device
      lock, so it is the only one that can block. Filtering on the lock-free pair first
      keeps most devices out of it, but a scan for ONE board still reads the serial of
      every peer that shares its VID -- so the bound is what stops one wedged DUT from
      stalling every caller (see read_sysfs).
    """
    out = []
    for d in glob.glob('/sys/bus/usb/devices/*-*'):
        # `in`, not endswith: an interface is '<busport>:<cfg>.<ifnum>' (2-4:1.0), which
        # CONTAINS the colon rather than ending with it. Screening them out here is worth
        # real time -- they were 31 of 44 matches on this rig.
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
        sn = read_sysfs(os.path.join(d, 'serial'), timeout)
        if sn is None:
            continue          # no serial attribute
        if serial is not None and sn.lower() != serial.lower():
            continue
        out.append({'busport': os.path.basename(d), 'dir': d,
                    'vid': dev_vid, 'pid': dev_pid, 'serial': sn})
    return out


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
                out, err = p.communicate(timeout=REAP_GRACE)
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


# The RTT console implementation lives in tools/rtt.py (importable classes + CLI,
# stdlib-only, harness-critical — see its module docstring). Loaded by file path so
# no sys.path entry for tools/ can shadow other imports; re-exported here so the
# harness keeps addressing hil_util.JlinkRtt.
import importlib.util as _ilu

_rtt_path = TINYUSB_ROOT / 'tools' / 'rtt.py'
if not _rtt_path.exists():
    # name the real cause: a bare FileNotFoundError out of an exec_module here reads
    # as a harness bug, when the actual problem is an incompletely staged tree
    raise ImportError(f'{_rtt_path} is missing — the RTT console lives there and the '
                      f'harness depends on it; stage it alongside test/hil (hil_ci.sh does)')
_rtt_spec = _ilu.spec_from_file_location('tinyusb_tools_rtt', _rtt_path)
_rtt = _ilu.module_from_spec(_rtt_spec)
sys.modules[_rtt_spec.name] = _rtt   # registered: RttError must be picklable across the fork Pool
_rtt_spec.loader.exec_module(_rtt)
JlinkRtt = _rtt.JlinkRtt
OpenocdRtt = _rtt.OpenocdRtt
RttError = _rtt.RttError
RTT_BANNER_RE = _rtt.RTT_BANNER_RE
strip_banner = _rtt.strip_banner


def _cmd_label(cmd) -> str:
    """A one-line name for a banner. An argv whose payload is a `python3 -c` program would
    otherwise dump the whole body into the CI log, where run_cmd's banners are already the
    noisiest thing in a failing row."""
    if isinstance(cmd, str):
        return cmd
    parts = [a if len(a) <= 60 else f'<{len(a)}-char program>' for a in cmd]
    return ' '.join(parts)


def run_cmd(cmd: str | list, cwd: str | None = None, timeout: int | None = None,
            binary: bool = False, split_stderr: bool = False,
            quiet: bool = False) -> subprocess.CompletedProcess:
    """Bounded subprocess: own session, killpg on expiry, rc 124 when it had to be killed.

    `cmd` is a shell STRING or an argv LIST. argv exists for a program that cannot survive
    a trip through the shell -- a multi-line `python3 -c` body -- which is how the harness
    runs a library call that no in-process bound can contain. A daemon thread cannot bound
    a C call that holds the GIL, so for those the child process IS the bound.
    """
    if timeout is None:
        timeout = CMD_TIMEOUT
    # binary: raw bytes (text mode's errors='replace' mangles non-UTF-8 file content).
    # split_stderr: keep stderr out of stdout, for callers that parse stdout. quiet: no
    # COMMAND FAILED banner, for retry loops that report failures themselves (timeouts
    # still print: a killed child is always noteworthy).
    popen_kwargs = {
        'cwd': cwd,
        # a list goes straight to execve; only a string needs a shell to parse it
        'shell': isinstance(cmd, str),
        'stdout': subprocess.PIPE,
        'stderr': subprocess.PIPE if split_stderr else subprocess.STDOUT,
    }
    if not binary:
        popen_kwargs.update({'text': True, 'encoding': 'utf-8', 'errors': 'replace'})
    # C-level setsid, same process-group semantics as preexec_fn=os.setsid but safe when
    # called from threads (pool_check runs flashes from a thread pool)
    popen_kwargs['start_new_session'] = True

    p = subprocess.Popen(cmd, **popen_kwargs)
    try:
        out, err = p.communicate(timeout=timeout)
        r = subprocess.CompletedProcess(args=cmd, returncode=p.returncode, stdout=out, stderr=err)
    except subprocess.TimeoutExpired as ex:
        try:
            os.killpg(p.pid, signal.SIGKILL)
        except OSError:
            # ProcessLookupError: already gone. PermissionError: an all-root group refuses
            # the group kill -- letting either escape would skip the bounded reap, the pipe
            # close and the rc-124 return this handler exists for.
            pass
        try:
            out, err = p.communicate(timeout=REAP_GRACE)
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
        _print_banner(f'COMMAND TIMEOUT ({timeout}s): {_cmd_label(cmd)}', timeout_out, timeout_err)
        return subprocess.CompletedProcess(args=cmd, returncode=124, stdout=timeout_out, stderr=timeout_err)
    except BaseException:
        # BaseException, not Exception (as in CPython's own subprocess.run):
        # KeyboardInterrupt is the case that matters, and start_new_session put the child in
        # its OWN group, so it never got the terminal's SIGINT -- without this, Ctrl-C
        # leaves the flasher or testusb holding the probe and its usbfs node. Kill and
        # close, never wait: this path must not add a hang of its own.
        try:
            os.killpg(p.pid, signal.SIGKILL)
        except OSError:
            pass
        _close_pipes(p)
        raise

    if r.returncode != 0 and not quiet:
        _print_banner(f'COMMAND FAILED: {_cmd_label(cmd)}', r.stdout, r.stderr)
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
