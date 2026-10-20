#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Firmware flashing for the TinyUSB HIL rig: board-shaped flash_*/reset_* over agentrc's flash
# provider (hil_util.provider(), the `hil` skill's Flashers), dispatched by roster name through
# flash_primitive/reset_primitive, plus find_firmware. tinyusb supplies the bounds, the runner
# (a caller's keyword-only `run=`, else hil_util.run_cmd read per call; openocd_esp takes none),
# retries and per-board settings; openocd_esp's server is the provider's own managed session
# (hil_util.openocd_esp()). The provider loads on first use, so
# change_impact imports this on a bare checkout; never import hil_test here. Callers set
# `build_dir`. `from __future__ import annotations` keeps the Board hints unevaluated.

from __future__ import annotations

import json
import shlex
import shutil
import subprocess
import tempfile
import time
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
def flash_jlink(board: Board, firmware: str, timeout=None, *, run=None) -> subprocess.CompletedProcess:
    return hil_util.provider().flash_jlink(board['flasher'], firmware, timeout=_bound(timeout),
                                           crash_retries=JLINK_CRASH_RETRIES, run=_runner(run))


def reset_jlink(board: Board, timeout=None, *, run=None) -> subprocess.CompletedProcess:
    return hil_util.provider().reset_jlink(board['flasher'], timeout=_bound(timeout),
                                           crash_retries=JLINK_CRASH_RETRIES, run=_runner(run))


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


def flash_esptool(board: Board, firmware: str, timeout=None, *, run=None) -> subprocess.CompletedProcess:
    flasher = board['flasher']
    port = hil_util.get_serial_dev(flasher["uid"], None, None, 0)
    fw_dir = Path(firmware).parent
    chip, flash_args = _esptool_inputs(fw_dir)
    return hil_util.provider().flash_esptool(flasher, chip=chip, port=port, flash_args=flash_args,
                                             cwd=str(fw_dir), timeout=_bound(timeout), run=_runner(run))


def esptool_input_problem(firmware) -> str:
    """Why flash_esptool could not flash this build ('' when it could): its dir's config.env
    and flash_args, judged by the provider's esptool_input_problem. Checked before a board is
    locked, so a flash cannot fail on its inputs."""
    fw_dir = Path(firmware).parent
    try:
        chip, flash_args = _esptool_inputs(fw_dir)
    except (OSError, ValueError, KeyError, TypeError) as e:
        return f'{fw_dir}: {e!r}'[:200]
    why = hil_util.provider().esptool_input_problem(chip, flash_args, str(fw_dir))
    return f'{fw_dir}: {why}' if why else ''


# no reset_esptool: esptool's reset would be `--after hard_reset`, which is not wired here


# -------------------------------------------------------------
# openocd_esp: the chip's built-in USB-Serial/JTAG through openocd-esp32 (the provider's
# hil_openocd_esp, the `hil` skill's Flashers). Not convoy-safe, so usbtest never recovers
# through it.
# -------------------------------------------------------------
# board name -> receipts of an openocd_esp session the provider left running (cleanup
# 'pending'). No later session starts on that board, and hil_test hands its lock to a cleanup
# holder instead of releasing it.
cleanup_pending = {}
OPENOCD_ESP_DIR = None   # the sessions' dir; None: one per user under the temp dir, named when used
# flash_args options esptool applies to the bootloader header; anything else (--encrypt...) is refused
ESP_HEADER_OPTIONS = {f'--flash{s}{k}' for s in '-_' for k in ('mode', 'freq', 'size')}
# s from the security read's hard reset to the provider's start; within ms every P4 v1.0 reset init failed (agentrc#160)
OPENOCD_ESP_BOOT_SETTLE = 1


def retained_lock_only(board: dict) -> str:
    """Why an entry point without hil_test's retained-lock path refuses this board, or ''."""
    if flashers(board, 'openocd_esp'):
        return ('openocd_esp: only hil_test.py keeps the board locked through a session the provider '
                'leaves running; refused here before any board access')
    return ''


def openocd_esp_refusal(flasher: dict) -> str:
    """Why the provider refuses this roster entry ('' when it admits it)."""
    try:
        hil_util.openocd_esp().admit(flasher)
    except ValueError as e:
        return str(e)
    return ''


def _esp_segments(fw_dir: Path, chip: str, timeout) -> list:
    """The build's flash_args as esptool's write_flash reads it, as explicit ordered
    {offset, file, size}. openocd programs each file as-is, so esptool's own merge-bin with the
    same options must leave every file's bytes unchanged (no bootloader header rewrite)."""
    target = _idf_target(fw_dir)
    if target != chip:
        raise ValueError(f'{fw_dir} is built for {target}, the openocd_esp args name {chip}')
    tokens = shlex.split((fw_dir / 'flash_args').read_text())
    options = []
    while tokens and tokens[0].startswith('-'):
        opt = tokens.pop(0)
        if opt.split('=', 1)[0] not in ESP_HEADER_OPTIONS:
            raise ValueError(f'flash_args option {opt} is not applied by openocd_esp')
        options += [opt] if '=' in opt else [opt, tokens.pop(0)]
    if not tokens or len(tokens) % 2:
        raise ValueError(f'{fw_dir}/flash_args: want "<offset> <file>" pairs after the options')
    pairs = list(zip(tokens[::2], tokens[1::2]))
    segments = [{'offset': int(o, 0), 'file': str(fw_dir / f), 'size': (fw_dir / f).stat().st_size}
                for o, f in pairs]
    with tempfile.TemporaryDirectory() as td:
        merged = Path(td) / 'merged.bin'
        cmd = (f'NO_COLOR=1 esptool --chip {chip} merge-bin -o {shlex.quote(str(merged))} '
               f'{shlex.join(options)} {shlex.join(x for p in pairs for x in p)}')
        ret = hil_util.run_cmd(cmd, cwd=str(fw_dir), timeout=_bound(timeout))
        if ret.returncode != 0:
            raise RuntimeError(f'esptool merge-bin exited {ret.returncode}: cannot tell what write_flash would write')
        data = merged.read_bytes()   # raw, from offset 0
    for seg in segments:
        o, n = seg['offset'], seg['size']
        if data[o:o + n] != Path(seg['file']).read_bytes():
            raise ValueError(f'esptool would rewrite {seg["file"]} at 0x{o:x} (its header lacks the flash_args '
                             f'settings); openocd_esp programs files as-is')
    return segments


def _tty_busport(port: str) -> str:
    # /sys/class/tty/<tty>/device is the USB interface (<busport>:<config>.<ifnum>)
    iface = os.path.realpath(f'/sys/class/tty/{os.path.basename(os.path.realpath(port))}/device')
    return os.path.basename(os.path.dirname(iface))


def _esp_security(flasher: dict, chip: str, timeout) -> dict:
    """The chip's flash encryption and secure boot state, read before programming with the
    esptool tinyusb already runs, over the same USJ's serial port: get-security-info from the ROM
    (--no-stub), which reports SPI_BOOT_CRYPT_CNT's parity and SECURE_BOOT_EN (ESP32-P4 TRM
    Rev 0.7 pp. 944-945). Refuses anything but both reported Disabled."""
    port = hil_util.get_serial_dev(flasher['uid'], None, None, 0)
    busport = hil_util.openocd_esp().usj_location(flasher['uid'])
    if _tty_busport(port) != busport:
        raise RuntimeError(f'{port} is not on the USJ at {busport}: no security evidence from this chip')
    cmd = (f'NO_COLOR=1 esptool --chip {chip} -p {shlex.quote(port)} --no-stub --before=default-reset '
           f'--after=hard-reset get-security-info')
    ret = hil_util.run_cmd(cmd, timeout=_bound(timeout))
    lines = [ln.strip() for ln in hil_util.cmd_stdout_text(ret.stdout).splitlines()]
    want = ('Secure Boot: Disabled', 'Flash Encryption: Disabled')
    if ret.returncode != 0 or not all(w in lines for w in want):
        raise RuntimeError(f'esptool get-security-info on {port} (rc {ret.returncode}) did not report '
                           f'{" and ".join(want)}: openocd_esp refuses to program')
    return {'flashEncryption': 'off', 'secureBoot': 'off',
            'source': f'`{cmd}` on USJ {busport}: {"; ".join(want)}'}


def _openocd_esp_session(board, operation):
    """Run operation(provider) unless the board has an outstanding session, latching a new one
    (cleanup 'pending', returned or on an exception's `.operation`) into cleanup_pending."""
    name, provider = board['name'], hil_util.openocd_esp()
    if why := provider.ci_refusal():    # before this adapter's own esptool sessions too
        raise RuntimeError(why)
    if name in cleanup_pending:
        raise RuntimeError(f'{name}: openocd_esp cleanup pending ({", ".join(map(str, cleanup_pending[name]))}); '
                           f'no session starts until its cleanup holder is recovered')
    op = None
    try:
        op = operation(provider)
    except BaseException as e:
        op = getattr(e, 'operation', None)
        raise
    finally:
        if op is not None and op.cleanup == 'pending':
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


def flash_openocd_esp(board, firmware, timeout=None):
    flasher = board['flasher']

    def operation(p):
        chip, _ = p.admit(flasher)
        segments = _esp_segments(Path(firmware).parent, chip, timeout)
        security = _esp_security(flasher, chip, timeout)
        time.sleep(OPENOCD_ESP_BOOT_SETTLE)
        return p.flash_openocd_esp(flasher, segments, security=security, workdir=_openocd_esp_dir(),
                                   timeout=_bound(timeout))
    return _openocd_esp_session(board, operation)


def reset_openocd_esp(board, timeout=None):
    return _openocd_esp_session(board, lambda p: p.reset_openocd_esp(board['flasher'], workdir=_openocd_esp_dir(),
                                                                     timeout=_bound(timeout)))


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


def find_firmware(variant: str, example: str, roots: list | None = None, flasher: str | None = None):
    """Locate a built example's firmware under <build_dir>/cmake-build-<variant>/<example>/.
    `roots` replaces build_dir with that search list for one call
    (e.g. a build just produced by tools/build.py in its fixed cmake-build/ layout)
    without widening the global policy. `flasher` is the roster flasher name and selects
    which extension counts (FLASHER_SUFFIX), so a build that produced only the other one
    is reported missing — a clean "Skip (no binary)" — instead of being handed to the
    flasher, which would fail opaquely and burn every retry plus the board lock.
    Accepts the single-config layout (firmware directly in the example dir) or Ninja
    Multi-Config (a per-config subdir like RelWithDebInfo/).
    Returns the full Path INCLUDING extension, or None if not built."""
    base = Path(example).name
    suffixes = [FLASHER_SUFFIX.get(flasher.lower())] if flasher else []
    if not suffixes or suffixes == [None]:
        suffixes = ['.elf', '.bin']
    for bd in dict.fromkeys(roots if roots is not None else [build_dir]):
        fw_dir = hil_util.TINYUSB_ROOT / bd / f'cmake-build-{variant}' / example
        if not fw_dir.is_dir():
            continue
        for cand in [fw_dir / base, fw_dir / 'RelWithDebInfo' / base,
                     *(p.with_suffix('') for s in suffixes for p in sorted(fw_dir.glob(f'*/{base}{s}')))]:
            for s in suffixes:
                if cand.with_suffix(s).exists():
                    return cand.with_suffix(s)
    return None
