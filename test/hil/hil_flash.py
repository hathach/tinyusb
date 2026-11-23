#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Firmware flashing for the TinyUSB HIL rig: board-shaped flash_*/reset_* over agentrc's flash
# provider (hil_util.provider(), the `hil` skill's Flashers), dispatched by roster name through
# flash_primitive/reset_primitive, plus find_firmware. tinyusb supplies the bounds, the runner
# (a caller's keyword-only `run=`, else hil_util.run_cmd read per call; for openocd_esp it runs
# only the esptool children), retries and per-board settings; openocd_esp's server is the
# provider's own managed session (hil_util.openocd_esp()), never a runner's. The provider
# loads on first use, so change_impact imports this on a bare checkout; never import hil_test
# here. Callers set `build_dir`. `from __future__ import annotations` keeps the Board hints
# unevaluated.

from __future__ import annotations

import copy
import functools
import json
import shutil
import subprocess
import tempfile
from pathlib import Path

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))  # PYTHONSAFEPATH drops it
from helper import hil_util

build_dir = 'cmake-build'

# libjlinkarm.so crashes on on-board J-Link probes before target access (the provider's
# JLINK_CRASH_RCS); rerun the whole script this often within the caller's one bound
JLINK_CRASH_RETRIES = 2
# pyocd's project dir: relative roster args (--pack...) resolve from here, and it holds no
# pyocd config, which discovery would load (the provider's pyocd launcher refuses one)
PYOCD_PROJECT_DIR = hil_util.TINYUSB_ROOT / 'test/hil/pyocd'


def _bound(timeout):
    return hil_util.CMD_TIMEOUT if timeout is None else timeout


def _runner(run):
    return hil_util.run_cmd if run is None else run


# -------------------------------------------------------------
# Flashing firmware
# -------------------------------------------------------------
def judged(op, problem: str):
    """`op` when it exited non-zero or `problem` is '', else a shallow copy failing with exit 1,
    `.problem` the judgment and `.operation` the provider's Operation; attempts, cleanup and
    receipts kept, so every caller's returncode check and report see the failure. Printed for
    the callers that read only the returncode; farm_profile reports `.problem`."""
    if op.returncode or not problem:
        return op
    print(f'judged failed: {problem}', flush=True)
    failed = copy.copy(op)
    failed.returncode, failed.problem, failed.operation = 1, problem, op
    return failed


def flash_jlink(board: Board, firmware: str, timeout=None, *, run=None) -> subprocess.CompletedProcess:
    p = hil_util.provider()
    op = p.flash_jlink(board['flasher'], firmware, timeout=_bound(timeout), crash_retries=JLINK_CRASH_RETRIES,
                       run=_runner(run))
    return judged(op, p.jlink_problem(op))


def reset_jlink(board: Board, timeout=None, *, run=None) -> subprocess.CompletedProcess:
    p = hil_util.provider()
    op = p.reset_jlink(board['flasher'], timeout=_bound(timeout), crash_retries=JLINK_CRASH_RETRIES,
                       run=_runner(run))
    return judged(op, p.jlink_problem(op))


def flash_stlink(board, firmware, timeout=None, *, run=None):
    # --verify is opt-in per roster entry ("verify": true): the provider's flash_stlink
    return hil_util.provider().flash_stlink(board['flasher'], firmware, timeout=_bound(timeout),
                                            run=_runner(run))


def reset_stlink(board, timeout=None, *, run=None):
    return hil_util.provider().reset_stlink(board['flasher'], timeout=_bound(timeout), run=_runner(run))


def _openocd_cmd_base(flasher):
    # the provider refuses a malformed explicit vid_pid with ValueError before openocd runs
    return hil_util.provider().openocd_cmd_base(flasher)


# `verify` is on by default, opted out per board with "verify": false (the provider's
# flash_openocd holds which boards and why)
def flash_openocd(board, firmware, timeout=None, *, run=None):
    return hil_util.provider().flash_openocd(board['flasher'], firmware, timeout=_bound(timeout),
                                             run=_runner(run))


def reset_openocd(board, timeout=None, *, run=None):
    # timeout: usbtest's post-hang recovery bounds this (RECOVER_RESET_TIMEOUT)
    return hil_util.provider().reset_openocd(board['flasher'], timeout=_bound(timeout), run=_runner(run))


def rescue_openocd(board, flash_out: str = '', timeout=None) -> bool:
    """The provider's rescue_openocd; the caller retries the flash afterwards either way."""
    return hil_util.provider().rescue_openocd(board['flasher'], flash_out, timeout=_bound(timeout),
                                              run=hil_util.run_cmd)


def valid_vid_pid(value) -> bool:
    return hil_util.provider().valid_vid_pid(value)


def flashers(board: dict, name: str) -> list:
    """The board's primary and recovery entries (recover_flasher, the primary's uid) named
    `name`, case-folded; the primary twice when it has no `flasher_recover`."""
    return [f for f in (board['flasher'], recover_flasher(board)) if (f.get('name') or '').lower() == name]


def recover_flasher(board: dict) -> dict:
    """The flasher that delivers RECOVERY for this board.

    Optional key `flasher_recover` (from agentrc's board registry for a board with a probe, else
    the roster's), else the primary. It exists because delivery and
    normal flashing have different requirements: a board flashed by jlink/lm4flash
    cannot reach its probe past a poisoned usbfs node, but the same probe driven by openocd
    often can (see convoy_safe). Keeping it a separate key rather than a list means the
    primary's shape never changes, so nothing that reads board['flasher'] has to care.
    The entry drives the primary's probe, so its uid is the primary's.
    """
    rec = board.get('flasher_recover')
    return {**rec, 'uid': board['flasher'].get('uid')} if rec else board['flasher']


def convoy_safe(flasher: dict) -> bool:
    """The provider's convoy_safe: can it deliver a recovery past a poisoned usbfs node?"""
    return hil_util.provider().convoy_safe(flasher)


class ProbeCheckFailed(RuntimeError):
    """probe_busy could not rule out a user of the board's probe."""


def probe_busy(board: dict) -> list:
    """The board_task busy check of every HIL entry point and the farm check: the provider's
    probe_users for the board's flasher and, when it differs, its recovery flasher (same probe,
    another tool); [{'pid', 'cmd'}]. ProbeCheckFailed, naming the cause, when the provider is
    inconclusive (ProbeCheckInconclusive) or has no lookup for the flasher (ValueError,
    wch_uart_loader); any other exception propagates. Probe users only: a DUT-side process (a
    stray testusb) is the run's recovery, never a refusal."""
    provider, users = hil_util.provider(), {}
    try:
        for f in {(f['name'], f.get('uid')): f for f in (board['flasher'], recover_flasher(board))}.values():
            users.update((u['pid'], u) for u in provider.probe_users(f))
    except (provider.ProbeCheckInconclusive, ValueError) as e:
        raise ProbeCheckFailed(f'{type(e).__name__}: {e}') from e
    return sorted(users.values(), key=lambda u: u['pid'])


def busy_refusal(e) -> str | None:
    """Why a board task's busy check refused admission (hil_lock.BoardBusy or ProbeCheckFailed),
    or None for any other refusal."""
    from helper import hil_lock   # agentrc's, loaded on first use like the provider
    if isinstance(e, hil_lock.BoardBusy):
        return f'probe in use by {hil_lock.users_text(e.users):.500}'
    if isinstance(e, ProbeCheckFailed):
        return f'probe check: {e}'
    return None


def _idf_target(fw_dir: Path) -> str:
    with (fw_dir / 'config.env').open() as f:
        return json.load(f)['IDF_TARGET']


def _esptool_inputs(fw_dir: Path) -> tuple:
    """The build's (IDF_TARGET, flash_args on one line), as flash_esptool takes them."""
    return _idf_target(fw_dir), (fw_dir / 'flash_args').read_text().strip().replace('\n', ' ')


@functools.cache
def esptool_runtime() -> dict:
    """The provider's esptool_runtime of the esptool on PATH, checked once per process: hil_test
    checks it before its forked workers start, which inherit the answer."""
    return hil_util.provider().esptool_runtime()


def flash_esptool(board: Board, firmware: str, timeout=None, *, run=None) -> subprocess.CompletedProcess:
    flasher, p = board['flasher'], hil_util.provider()
    rt = esptool_runtime()
    port = p.esptool_port(flasher['uid'])
    fw_dir = Path(firmware).parent
    chip, flash_args = _esptool_inputs(fw_dir)
    op = p.flash_esptool(flasher, chip=chip, port=port, flash_args=flash_args, cwd=str(fw_dir),
                         timeout=_bound(timeout), run=_runner(run), esptool=rt['exe'])
    return judged(op, p.esptool_problem(op))


# no reset_esptool: hil_test resets with the board's console open on the same serial port.
def rom_reset_esptool(board: Board, timeout=None, *, run=None) -> subprocess.CompletedProcess:
    """The provider's ROM-loader reset, for a caller that holds no console on the port."""
    flasher = board['flasher']
    port = hil_util.provider().esptool_port(flasher['uid'])
    return hil_util.provider().reset_esptool(flasher, port=port, timeout=_bound(timeout), run=_runner(run))


# -------------------------------------------------------------
# openocd_esp: the chip's built-in USB-Serial/JTAG through openocd-esp32 (the provider's
# hil_openocd_esp, the `hil` skill's Flashers). Not convoy-safe, so usbtest never recovers
# through it.
# -------------------------------------------------------------
# What an operation leaves not proven ended is listed in the provider's per-USJ pending record
# (pending(serial)), which refuses every later flash, reset and security read on that USJ. The
# record clears an entry once it ends, so this run keeps its own latch too: USJ serial ->
# pending_entries()-shaped evidence of what an operation here left, which stops the board for
# the rest of the run whatever the record later says.
left_pending = {}
OPENOCD_ESP_DIR = None   # the sessions' dir; None: one per user under the temp dir, named when used


def openocd_esp_serial(board: dict) -> str:
    """The USJ serial keying the provider's pending record of this board ('' without an
    openocd_esp flasher; a recovery entry's uid is the primary's)."""
    return board['flasher']['uid'] if flashers(board, 'openocd_esp') else ''


def openocd_esp_refused(board: dict, *, retains_lock: bool) -> str:
    """Why an entry point refuses this board's openocd_esp flasher, or '' (none, or admitted):
    the provider's refusal, under a CI job or to a caller that does not keep the board lock
    until its task logs a session left running (`retains_lock`). Checked before any board access."""
    if not flashers(board, 'openocd_esp'):
        return ''
    return hil_util.openocd_esp().refusal(retains_lock=retains_lock)


def openocd_esp_entry_problem(flasher: dict) -> str:
    """Why the provider refuses this roster entry ('' when it admits it)."""
    try:
        hil_util.openocd_esp().admit(flasher)
    except ValueError as e:
        return str(e)
    return ''


def _openocd_esp_session(board, operation):
    """operation(provider), which refuses and lists what it leaves pending itself, unless the CI
    job, the USJ's pending record or this run's latch refuses first, before any host-side esptool.
    What it leaves not proven ended, returned or on a raised `.operation` (a failed listing's
    included), goes in left_pending."""
    p, serial = hil_util.openocd_esp(), board['flasher']['uid']
    if why := p.refusal(retains_lock=True) or p.pending_refusal(serial) or left_refusal(p, serial):
        raise RuntimeError(why)
    op = None
    try:
        op = operation(p)
    except BaseException as e:
        op = getattr(e, 'operation', None)
        raise
    finally:
        if op is not None and op.cleanup != 'done':
            _note_left(p, serial, op)
    if op.returncode != 0:
        print(f'openocd_esp: {op.session["failure"]} (cleanup {op.cleanup}, log {op.session["log"]})', flush=True)
    elif op.cleanup == 'done':
        shutil.rmtree(op.session['dir'], ignore_errors=True)   # read-back copies, a few hundred KB per flash
    return op


def _note_left(p, serial, op):
    """Add what `op` left to left_pending[serial], by the provider's receipt kinds; for an operation
    that left no receipt, the unreceipted entries the provider holds for the USJ (latch's), else one
    naming it."""
    evidence = left_pending.setdefault(serial, {'receipts': [], 'retained': [], 'unrecheckable': []})
    left = op.receipts or p.pending_entries(serial)['unrecheckable'] or \
        [{'unreceipted': f'openocd_esp operation: cleanup {op.cleanup}', 'serial': serial}]
    for r in left:
        kept = evidence[{'tracked': 'receipts', 'session': 'retained'}.get(p.receipt_kind(r), 'unrecheckable')]
        if r not in kept:
            kept.append(r)


def left_evidence(p, serial) -> dict:
    """The provider's pending_entries of the USJ with what this run left there (left_pending), each
    entry once: {'receipts', 'retained', 'unrecheckable'}."""
    evidence = p.pending_entries(serial)
    for kind, left in left_pending.get(serial, {}).items():
        evidence[kind] += [r for r in left if r not in evidence[kind]]
    return evidence


def left_refusal(p, serial) -> str:
    """Why this run starts nothing more on the USJ ('' when nothing was left here)."""
    if not (evidence := left_pending.get(serial)):
        return ''
    left = [r for kind in evidence.values() for r in kind]
    return (f'USJ {serial}: {len(left)} openocd_esp operation(s) earlier in this run were not proven ended: '
            + '; '.join(map(p.describe, left[:3])) + '; nothing more runs on it in this run')


def _openocd_esp_dir() -> str:
    path = OPENOCD_ESP_DIR or Path(tempfile.gettempdir()) / f'tinyusb-hil-openocd-esp-{os.getuid()}'
    path.mkdir(mode=0o700, exist_ok=True)
    return str(path)


def flash_openocd_esp(board, firmware, timeout=None, *, run=None):
    """The build's flash_args as the provider's ordered segments, the chip's security state read
    over its USJ tty (the esptool package of the `esptool` on PATH, the contract's runtime), then
    the flash, which takes that read's Operation; the provider spaces its session BOOT_SETTLE from
    the security read's hard reset. `run` runs the merge-bin; the security read always takes the
    provider's own run_tracked, which lists its state dir before the child starts (write-ahead, lost
    to a caller's runner), with this harness's REAP_GRACE and verbosity."""
    flasher = board['flasher']

    def operation(p):
        chip, _ = p.admit(flasher)
        segments = p.segments_from_flash_args(Path(firmware).parent, chip, timeout=_bound(timeout),
                                              run=_runner(run))
        read = p.read_security(flasher['uid'], chip=chip, workdir=_openocd_esp_dir(), timeout=_bound(timeout),
                               retains_lock=True, reap_grace=hil_util.REAP_GRACE, verbose=hil_util.verbose)
        return p.flash_openocd_esp(flasher, segments, security=read, workdir=_openocd_esp_dir(),
                                   timeout=_bound(timeout), retains_lock=True)
    return _openocd_esp_session(board, operation)


def reset_openocd_esp(board, timeout=None, *, run=None):
    """`run` is accepted as every adapter's is; the reset starts no process of its own."""
    return _openocd_esp_session(board, lambda p: p.reset_openocd_esp(board['flasher'], workdir=_openocd_esp_dir(),
                                                                     timeout=_bound(timeout), retains_lock=True))


def flash_lm4flash(board, firmware, timeout=None, *, run=None):
    return hil_util.provider().flash_lm4flash(board['flasher'], firmware, timeout=_bound(timeout),
                                              run=_runner(run))


# no reset_lm4flash: lm4flash has no reset-only mode; it resets+runs on flash


def pyocd_vid_pid(flasher: dict) -> list | None:
    """The provider's pyocd_vid_pid."""
    return hil_util.provider().pyocd_vid_pid(flasher)


def unfiltered_pyocd(board: dict) -> bool:
    """Is the board's primary or recovery flasher a pyocd entry without its VID/PID filter?"""
    return any(pyocd_vid_pid(f) is None for f in flashers(board, 'pyocd'))


def flash_pyocd(board, firmware, timeout=None, *, run=None):
    return hil_util.provider().flash_pyocd(board['flasher'], firmware, project_dir=str(PYOCD_PROJECT_DIR),
                                           timeout=_bound(timeout), run=_runner(run))


def reset_pyocd(board, timeout=None, *, run=None):
    return hil_util.provider().reset_pyocd(board['flasher'], project_dir=str(PYOCD_PROJECT_DIR),
                                           timeout=_bound(timeout), run=_runner(run))


def flash_primitive(flasher_name: str):
    """The flasher's flash_* callable, case-folded from the roster name.

    An unknown flasher raises AttributeError.
    """
    name = flasher_name.lower()
    if name == 'primitive':   # this lookup shares the flash_* namespace but is no flasher
        raise AttributeError(f'{flasher_name!r} is not a flasher')
    return getattr(sys.modules[__name__], f'flash_{name}')


def reset_primitive(flasher_name: str):
    """The flasher's probe-reset callable, or None when it has no reset-only mode.

    An unknown flasher raises AttributeError, as flash_primitive does.
    """
    flash_primitive(flasher_name)
    return getattr(sys.modules[__name__], f'reset_{flasher_name.lower()}', None)


# The one place a flasher's firmware extension is decided. A flasher with no entry falls
# back to .elf-or-.bin and can be handed the wrong file — test_change_impact's
# TestRosterFlashersDispatch fails if a roster names one.
FLASHER_SUFFIX = {
    'esptool': '.bin',
    'jlink': '.elf',
    'lm4flash': '.bin',
    'openocd': '.elf',
    'openocd_esp': '.bin',
    'pyocd': '.elf',
    'stlink': '.elf',
}


def find_firmware(variant: str, example: str, flasher: str | None = None):
    """Locate a built example's firmware under <build_dir>/cmake-build-<variant>/<example>/.
    `flasher` is the roster flasher name and selects which extension counts (FLASHER_SUFFIX),
    so a build that produced only the other one is reported missing — a clean "Skip (no
    binary)" — instead of being handed to the flasher, which would fail opaquely and burn
    every retry plus the board lock.
    Accepts the single-config layout (firmware directly in the example dir) or Ninja
    Multi-Config (a per-config subdir like RelWithDebInfo/).
    Returns the full Path INCLUDING extension, or None if not built."""
    base = Path(example).name
    suffixes = [FLASHER_SUFFIX.get(flasher.lower())] if flasher else []
    if not suffixes or suffixes == [None]:
        suffixes = ['.elf', '.bin']
    fw_dir = hil_util.TINYUSB_ROOT / build_dir / f'cmake-build-{variant}' / example
    for cand in [fw_dir / base, fw_dir / 'RelWithDebInfo' / base,
                 *(p.with_suffix('') for s in suffixes for p in sorted(fw_dir.glob(f'*/{base}{s}')))]:
        for s in suffixes:
            if cand.with_suffix(s).exists():
                return cand.with_suffix(s)
    return None
