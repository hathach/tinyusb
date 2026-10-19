#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Firmware flashing for the TinyUSB HIL rig: board-shaped flash_*/reset_* over agentrc's
# flash provider (hil_util.provider(), the `hil` skill's Flashers), one per flasher type
# (dispatched by config name through flash_primitive), a reset_* taking timeout= where the
# tool can reset without flashing (looked up through reset_primitive), plus find_firmware.
# Every tool runs through hil_util.run_cmd, read per call so a patched runner reaches it
# (never import hil_test here). Callers set the module global `build_dir`. The provider
# loads on first use: change_impact's suites import this on a bare checkout. `from
# __future__ import annotations` keeps the Board hints below unevaluated: the type is not
# defined in this module.

from __future__ import annotations

import json
import subprocess
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


# -------------------------------------------------------------
# Flashing firmware
# -------------------------------------------------------------
def flash_jlink(board: Board, firmware: str, timeout=None) -> subprocess.CompletedProcess:
    return hil_util.provider().flash_jlink(board['flasher'], firmware, timeout=_bound(timeout),
                                           crash_retries=JLINK_CRASH_RETRIES, run=hil_util.run_cmd)


def reset_jlink(board: Board, timeout=None) -> subprocess.CompletedProcess:
    return hil_util.provider().reset_jlink(board['flasher'], timeout=_bound(timeout),
                                           crash_retries=JLINK_CRASH_RETRIES, run=hil_util.run_cmd)


def flash_stlink(board, firmware, timeout=None):
    # --verify is opt-in per roster entry ("verify": true): the provider's flash_stlink
    return hil_util.provider().flash_stlink(board['flasher'], firmware, timeout=_bound(timeout),
                                            run=hil_util.run_cmd)


def reset_stlink(board, timeout=None):
    return hil_util.provider().reset_stlink(board['flasher'], timeout=_bound(timeout), run=hil_util.run_cmd)


def _openocd_cmd_base(flasher):
    # the provider refuses a malformed explicit vid_pid with ValueError before openocd runs
    return hil_util.provider().openocd_cmd_base(flasher)


# `verify` is on by default, opted out per board with "verify": false (the provider's
# flash_openocd holds which boards and why)
def flash_openocd(board, firmware, timeout=None):
    return hil_util.provider().flash_openocd(board['flasher'], firmware, timeout=_bound(timeout),
                                             run=hil_util.run_cmd)


def reset_openocd(board, timeout=None):
    # timeout: usbtest's post-hang recovery bounds this (RECOVER_RESET_TIMEOUT)
    return hil_util.provider().reset_openocd(board['flasher'], timeout=_bound(timeout), run=hil_util.run_cmd)


def rescue_openocd(board, flash_out: str = '', timeout=None) -> bool:
    """The provider's rescue_openocd; the caller retries the flash afterwards either way."""
    return hil_util.provider().rescue_openocd(board['flasher'], flash_out, timeout=_bound(timeout),
                                              run=hil_util.run_cmd)


def valid_vid_pid(value) -> bool:
    return hil_util.provider().valid_vid_pid(value)


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


def flash_esptool(board: Board, firmware: str, timeout=None) -> subprocess.CompletedProcess:
    flasher = board['flasher']
    port = hil_util.get_serial_dev(flasher["uid"], None, None, 0)
    fw_dir = Path(firmware).parent
    with (fw_dir / 'config.env').open() as f:
        idf_target = json.load(f)['IDF_TARGET']
    with (fw_dir / 'flash_args').open() as f:
        flash_args = f.read().strip().replace('\n', ' ')
    return hil_util.provider().flash_esptool(flasher, chip=idf_target, port=port, flash_args=flash_args,
                                             cwd=str(fw_dir), timeout=_bound(timeout), run=hil_util.run_cmd)


# no reset_esptool: esptool's reset would be `--after hard_reset`, which is not wired here


def flash_lm4flash(board, firmware, timeout=None):
    return hil_util.provider().flash_lm4flash(board['flasher'], firmware, timeout=_bound(timeout),
                                              run=hil_util.run_cmd)


# no reset_lm4flash: lm4flash has no reset-only mode; it resets+runs on flash


def pyocd_vid_pid(flasher: dict) -> list | None:
    """The provider's pyocd_vid_pid."""
    return hil_util.provider().pyocd_vid_pid(flasher)


def unfiltered_pyocd(board: dict) -> bool:
    """Is the board's primary or recovery flasher a pyocd entry without its VID/PID filter?"""
    return any((f.get('name') or '').lower() == 'pyocd' and pyocd_vid_pid(f) is None
               for f in (board['flasher'], recover_flasher(board)))


def flash_pyocd(board, firmware, timeout=None):
    return hil_util.provider().flash_pyocd(board['flasher'], firmware, project_dir=str(PYOCD_PROJECT_DIR),
                                           timeout=_bound(timeout), run=hil_util.run_cmd)


def reset_pyocd(board, timeout=None):
    return hil_util.provider().reset_pyocd(board['flasher'], project_dir=str(PYOCD_PROJECT_DIR),
                                           timeout=_bound(timeout), run=hil_util.run_cmd)


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
