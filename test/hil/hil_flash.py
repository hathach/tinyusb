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

    Optional roster key `flasher_recover`, else the primary. It exists because delivery and
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
    print(f'esptool {rt["version"]} ({rt["exe"]}) on {port}', flush=True)
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
# board name -> receipts of what an openocd_esp operation left not proven ended: a session the
# provider left running (cleanup 'pending'), or a tracked esptool child 'pending' or 'unknown'
# with its receipt. No later session starts on that board, and hil_test hands its lock to a
# cleanup holder instead of releasing it.
cleanup_pending = {}
OPENOCD_ESP_DIR = None   # the sessions' dir; None: one per user under the temp dir, named when used


# hil_process.run_tracked's receipt keys that a native openocd_esp session record (its
# _RECEIPT_KEYS, kept when the session's receipt file could not be written) never has
_TRACKED_KEYS = {'supervisor', 'lock', 'record'}


def tracked_receipt(receipt) -> bool:
    """Whether a cleanup_pending receipt is hil_process.run_tracked's (an esptool child, ended once
    its recheck answers `done`) rather than an openocd_esp session's (its receipt path, or the
    native record when that file could not be written), which only the reviewed `end` ends."""
    return isinstance(receipt, dict) and _TRACKED_KEYS <= receipt.keys()


def receipt_text(receipt) -> str:
    """A cleanup_pending receipt as a cell names it."""
    if tracked_receipt(receipt):
        return f'esptool child, record {receipt["record"]}'
    if isinstance(receipt, dict):
        return (f'openocd_esp session pid {receipt.get("pid")} start {receipt.get("startTime")}, receipt file '
                f'unwritten, log {receipt.get("log")}')
    return str(receipt)


def openocd_esp_refused(board: dict, *, retains_lock: bool) -> str:
    """Why an entry point refuses this board's openocd_esp flasher, or '' (none, or admitted):
    the provider's refusal, under a CI job or to a caller that does not keep the board lock
    through a session left running (`retains_lock`). Checked before any board access."""
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
    """Run operation(provider) unless the board has an outstanding session, latching a new one
    (cleanup 'pending', returned or on an exception's `.operation`) into cleanup_pending."""
    name, provider = board['name'], hil_util.openocd_esp()
    if why := provider.refusal(retains_lock=True):    # refuse before preparatory esptool commands
        raise RuntimeError(why)
    if name in cleanup_pending:
        held = ', '.join(map(receipt_text, cleanup_pending[name]))
        raise RuntimeError(f'{name}: openocd_esp cleanup pending ({held}); no session starts until its cleanup '
                           f'holder is recovered')
    op = None
    try:
        op = operation(provider)
    except BaseException as e:
        op = getattr(e, 'operation', None)
        raise
    finally:
        # 'unknown' with a receipt (a tracked child whose supervisor did not report) is no proof either
        if op is not None and (op.cleanup == 'pending' or op.cleanup == 'unknown' and op.receipts):
            cleanup_pending[name] = op.receipts
    if op.returncode != 0:
        print(f'openocd_esp: {op.session["failure"]} (cleanup {op.cleanup}, log {op.session["log"]})', flush=True)
    elif op.cleanup == 'done':
        shutil.rmtree(op.session['dir'], ignore_errors=True)   # read-back copies, a few hundred KB per flash
    return op


def _openocd_esp_dir() -> str:
    path = OPENOCD_ESP_DIR or Path(tempfile.gettempdir()) / f'tinyusb-hil-openocd-esp-{os.getuid()}'
    path.mkdir(mode=0o700, exist_ok=True)
    return str(path)


def _tracked(cmd, cwd=None, *, timeout, env=None):
    """The security read's runner: the provider's hil_process.run_tracked, whose receipt says
    whether the esptool child is proven ended, with this harness's REAP_GRACE and verbosity and
    its state in the sessions' private dir."""
    state_dir = Path(_openocd_esp_dir()) / 'tracked'
    state_dir.mkdir(mode=0o700, exist_ok=True)
    # settle: s waited for the child's descendants once it ended, as agentrc's farm check engine does
    return hil_util.provider().hil_process.run_tracked(cmd, cwd, timeout=timeout, settle=2.0,
                                                       state_dir=str(state_dir), reap_grace=hil_util.REAP_GRACE,
                                                       verbose=hil_util.verbose, env=env)


def _read_security(p, flasher: dict, chip: str, timeout, run) -> dict:
    """read_security's evidence, once its esptool child is proven ended (cleanup `done`): an
    openocd session started beside a survivor would share the USJ with it. Otherwise raises
    carrying the read's Operation, whose receipt _openocd_esp_session latches."""
    op = p.read_security(flasher['uid'], chip=chip, workdir=_openocd_esp_dir(), timeout=_bound(timeout),
                         retains_lock=True, run=run)
    if op.cleanup != 'done':
        e = RuntimeError(f'esptool security read cleanup {op.cleanup}: its child is not proven ended, so no '
                         f'openocd session starts beside it')
        e.operation = op
        raise e
    return op.security


def flash_openocd_esp(board, firmware, timeout=None, *, run=None):
    """The build's flash_args as the provider's ordered segments, the chip's security state read
    over its USJ tty (the esptool package of the `esptool` on PATH, the contract's runtime), then
    the flash; the provider spaces its session BOOT_SETTLE from the security read's hard reset.
    `run` runs the two esptool children; the security read's runner must report its cleanup
    (run_tracked's), by default _tracked."""
    flasher = board['flasher']

    def operation(p):
        chip, _ = p.admit(flasher)
        segments = p.segments_from_flash_args(Path(firmware).parent, chip, timeout=_bound(timeout),
                                              run=_runner(run))
        security = _read_security(p, flasher, chip, timeout, _tracked if run is None else run)
        return p.flash_openocd_esp(flasher, segments, security=security, workdir=_openocd_esp_dir(),
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
