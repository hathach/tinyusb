#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Firmware flashing for the TinyUSB HIL rig: run_cmd, one flash_*/reset_* pair per
# flasher type (dispatched by config name via getattr), find_firmware, and the
# fixture serial-port resolver get_serial_dev (here, not hil_test: flash_esptool
# needs it and helpers must not import hil_test).
# Callers set module globals `build_dir` and `verbose` (hil_test.main from argparse,
# pool_check directly) exactly as they set hil_test's globals today.
#
# from __future__ import annotations (below): some moved function signatures use
# type hints (Any, Board) not defined in this module; postponed evaluation (PEP
# 563) keeps those as unevaluated strings so the verbatim-moved defs still load.

from __future__ import annotations

import glob
import json
import os
import signal
import subprocess
from pathlib import Path

verbose = False
build_dir = 'cmake-build'

CMD_TIMEOUT = int(os.getenv('HIL_CMD_TIMEOUT', '180'))

# flasher names (dispatch key, board['flasher']['name'].lower()) whose reset_* is a no-op
RESET_NOOP = {'esptool', 'lm4flash'}

# extra parents find_firmware ALSO searches after build_dir. Empty by default so
# hil_test's -B stays authoritative (a board missing there must report "Skip (no
# binary)", never silently flash a stale binary from another tree); pool_check
# opts in to cover both standard layouts.
EXTRA_BUILD_DIRS: list = []


def cmd_stdout_text(out: Any) -> str:
    if out is None:
        return ''
    if isinstance(out, bytes):
        return out.decode('utf-8', errors='ignore')
    return str(out)


# -------------------------------------------------------------
# Path
# -------------------------------------------------------------
TINYUSB_ROOT = Path(__file__).resolve().parents[2]

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


# -------------------------------------------------------------
# Flashing firmware
# -------------------------------------------------------------
def run_cmd(cmd: str, cwd: str | None = None, timeout: int = CMD_TIMEOUT) -> subprocess.CompletedProcess:
    popen_kwargs = {
        'cwd': cwd,
        'shell': True,
        'stdout': subprocess.PIPE,
        'stderr': subprocess.STDOUT,
        'text': True,
        'encoding': 'utf-8',
        'errors': 'replace',
    }
    if os.name != 'nt':
        # C-level setsid, same process-group semantics as preexec_fn=os.setsid but
        # safe when called from threads (pool_check runs flashes from a thread pool)
        popen_kwargs['start_new_session'] = True

    p = subprocess.Popen(cmd, **popen_kwargs)
    try:
        out, _ = p.communicate(timeout=timeout)
        r = subprocess.CompletedProcess(args=cmd, returncode=p.returncode, stdout=out)
    except subprocess.TimeoutExpired as ex:
        if os.name != 'nt':
            try:
                os.killpg(p.pid, signal.SIGKILL)
            except ProcessLookupError:
                pass
        else:
            p.kill()
        try:
            out, _ = p.communicate(timeout=10)
        except subprocess.TimeoutExpired:  # unkillable (e.g. D-state on wedged USB)
            out = None
        timeout_out = ex.stdout or out or b''
        title = f'COMMAND TIMEOUT ({timeout}s): {cmd}'
        print()
        if os.getenv('CI'):
            print(f"::group::{title}")
            print(cmd_stdout_text(timeout_out))
            print(f"::endgroup::")
        else:
            print(title)
            print(cmd_stdout_text(timeout_out))
        return subprocess.CompletedProcess(args=cmd, returncode=124, stdout=timeout_out)

    if r.returncode != 0:
        title = f'COMMAND FAILED: {cmd}'
        print()
        if os.getenv('CI'):
            print(f"::group::{title}")
            print(cmd_stdout_text(r.stdout))
            print(f"::endgroup::")
        else:
            print(title)
            print(cmd_stdout_text(r.stdout))
    elif verbose:
        print(cmd)
        print(cmd_stdout_text(r.stdout))
    return r


def flash_jlink(board: Board, firmware: str) -> subprocess.CompletedProcess:
    flasher = board['flasher']
    script = ['halt', 'r', f'loadfile {firmware}', 'r', 'go', 'exit']
    f_jlink = Path(f'{board["name"]}_{Path(firmware).name}.jlink')
    with f_jlink.open('w') as f:
        f.writelines(f'{s}\n' for s in script)
    ret = run_cmd(f'JLinkExe -USB {flasher["uid"]} {flasher["args"]} -if swd -JTAGConf -1,-1 -speed auto -NoGui 1 -ExitOnError 1 -CommandFile {f_jlink}')
    f_jlink.unlink(missing_ok=True)
    return ret


def reset_jlink(board: Board) -> subprocess.CompletedProcess:
    flasher = board['flasher']
    script = ['halt', 'r', 'go', 'exit']
    f_jlink = Path(f'{board["name"]}_reset.jlink')
    if not f_jlink.exists():
        with f_jlink.open('w') as f:
            f.writelines(f'{s}\n' for s in script)
    ret = run_cmd(f'JLinkExe -USB {flasher["uid"]} {flasher["args"]} -if swd -JTAGConf -1,-1 -speed auto -NoGui 1 -ExitOnError 1 -CommandFile {f_jlink}')
    return ret


def flash_stlink(board, firmware):
    flasher = board['flasher']
    return run_cmd(f'STM32_Programmer_CLI --connect port=swd sn={flasher["uid"]} --write {firmware} --go')


def reset_stlink(board):
    flasher = board['flasher']
    return run_cmd(f'STM32_Programmer_CLI --connect port=swd sn={flasher["uid"]} --rst --go')


def _openocd_cmd_base(flasher):
    return (f'openocd -c "tcl_port disabled" -c "gdb_port disabled" -c "telnet_port disabled" '
            f'-c "adapter serial {flasher["uid"]}" {flasher["args"]}')


# `verify` is on by default and opted out per board with "verify": false in the roster.
# WCH targets must opt out: flash read-back over the WCH-Link sdi transport returns a
# repeated word instead of memory contents, so verification always reports a mismatch and
# fails the flash (measured on ch32v103r and ch32v307v, 2026-07-30). Do NOT drop verify
# fleet-wide to accommodate them — every other openocd board can read back, and without it
# a partial or corrupt write exits 0 and the test phase runs bad firmware.
def flash_openocd(board, firmware):
    flasher = board['flasher']
    verify = ' verify' if flasher.get('verify', True) else ''
    ret = run_cmd(f'{_openocd_cmd_base(flasher)} -c "program {firmware}{verify} reset exit"')
    return ret


def reset_openocd(board):
    flasher = board['flasher']
    ret = run_cmd(f'{_openocd_cmd_base(flasher)} -c "init; reset run; exit"')
    return ret


# OpenOCD's messages for "the target's debug port did not answer". The probe is fine when
# these appear (the log still shows "CMSIS-DAP: Interface ready"); the chip's debug clock
# is gone, which no reset the probe can drive would fix -- the CMSIS-DAP Debug Probe has no
# nRESET line at all. Which message you get depends on the DAP topology, NOT on the board:
# rp2040.cfg creates three multidrop DAPs (cores 0/1 and the Rescue DP at instance 0xf) so
# it fails in swd_multidrop_select, while rp2350.cfg creates a single plain ADIv6 DAP that
# fails earlier in swd_connect. A dead RP2040 can also produce the second one if the very
# first DP read never gets through, so both are accepted for both chips -- it is the target
# cfg in the roster args, below, that picks how to rescue.
DAP_WEDGED = ('Failed to connect multidrop', 'Error connecting DP: cannot read IDR')

# How each RP target reaches its Rescue DP, keyed by the target cfg named in flasher args.
# (cfg substitution, extra args): rp2040.cfg drives the Rescue DP itself behind a RESCUE
# flag and calls init/shutdown on its own; rp2350 has a separate cfg that pokes the rescue
# bit via an AP register but never shuts down, so it would sit in the server loop until
# CMD_TIMEOUT without an explicit one.
RESCUE_CFG = {
    'target/rp2040.cfg': ('target/rp2040.cfg', '-c "set RESCUE 1" ', ''),
    'target/rp2350.cfg': ('target/rp2350-rescue.cfg', '', ' -c "shutdown"'),
}


def rescue_openocd(board, flash_out: str = '') -> bool:
    """Power-on-reset a wedged RP2040/RP2350 through its Rescue DP, the one debug port not
    gated by the system clock (RP2040 datasheet 2.3.4.2): setting CDBGPWRUPREQ hard-resets
    the chip, and the bootrom halts it in a safe state ready to be flashed. This is the
    only way back for a target whose cores have stopped answering -- otherwise the board
    needs a physical replug, since the probe carries no reset line.

    No-op (returns False) unless this is an openocd RP board AND the flash output shows the
    wedge, so a flash that failed for any other reason still just retries. Returns True
    when a rescue was attempted; the caller should retry the flash afterwards."""
    flasher = board['flasher']
    if flasher['name'].lower() != 'openocd' or not any(m in flash_out for m in DAP_WEDGED):
        return False
    for cfg, (rescue_cfg, pre, post) in RESCUE_CFG.items():
        if cfg in flasher['args']:
            args = flasher['args'].replace(cfg, rescue_cfg)
            return run_cmd(f'{_openocd_cmd_base({**flasher, "args": pre + args})}{post}').returncode == 0
    return False


def flash_esptool(board: Board, firmware: str) -> subprocess.CompletedProcess:
    flasher = board['flasher']
    port = get_serial_dev(flasher["uid"], None, None, 0)
    fw_dir = Path(firmware).parent
    with (fw_dir / 'config.env').open() as f:
        idf_target = json.load(f)['IDF_TARGET']
    with (fw_dir / 'flash_args').open() as f:
        flash_args = f.read().strip().replace('\n', ' ')
    command = (f'esptool --chip {idf_target} -p {port} {flasher["args"]} '
               f'--before=default_reset --after=hard_reset write_flash {flash_args}')
    ret = run_cmd(command, cwd=str(fw_dir))
    return ret


def reset_esptool(board):
    flasher = board['flasher']
    return subprocess.CompletedProcess(args=['dummy'], returncode=0)


def flash_lm4flash(board, firmware):
    # TI Tiva-C / Stellaris ICDI: lightweight lm4flash, resets and runs after write
    flasher = board['flasher']
    ret = run_cmd(f'lm4flash -s {flasher["uid"]} {flasher["args"]} {firmware}')
    return ret


def reset_lm4flash(board):
    # lm4flash has no reset-only mode; it resets+runs on flash, so reset is a no-op
    flasher = board['flasher']
    return subprocess.CompletedProcess(args=['dummy'], returncode=0)


# The one place a flasher's firmware extension is decided: find_firmware resolves the
# path with it and the flash_* functions pass that path through untouched. A flasher
# added here without an entry falls back to .elf-or-.bin and can be handed the wrong
# file — test_hil_select's TestRosterFlashersDispatch fails if a roster names one.
FLASHER_SUFFIX = {
    'esptool': '.bin',
    'jlink': '.elf',
    'lm4flash': '.bin',
    'openocd': '.elf',
    'stlink': '.elf',
}


def find_firmware(variant: str, example: str, roots: list | None = None, flasher: str | None = None):
    """Locate a built example's firmware under <build_dir>/cmake-build-<variant>/<example>/,
    then under EXTRA_BUILD_DIRS (empty unless the caller opts in — see its comment).
    `roots` overrides that search list entirely for one call (e.g. to find a build just
    produced by tools/build.py in its fixed cmake-build/ layout without widening the
    global policy). `flasher` is the roster flasher name: it selects which extension
    counts (see FLASHER_SUFFIX), so a build that produced only the other one is reported
    missing — a clean "Skip (no binary)" — instead of being handed to the flasher, which
    would fail opaquely on the absent file and burn every retry plus the board lock.
    Accepts the single-config layout (firmware directly in the example dir) or Ninja
    Multi-Config (a per-config subdir like RelWithDebInfo/).
    Returns the full Path INCLUDING extension, or None if not built."""
    base = Path(example).name
    suffixes = [FLASHER_SUFFIX.get(flasher.lower())] if flasher else []
    if not suffixes or suffixes == [None]:
        suffixes = ['.elf', '.bin']
    for bd in dict.fromkeys(roots if roots is not None else [build_dir, *EXTRA_BUILD_DIRS]):
        fw_dir = TINYUSB_ROOT / bd / f'cmake-build-{variant}' / example
        if not fw_dir.is_dir():
            continue
        for cand in [fw_dir / base, fw_dir / 'RelWithDebInfo' / base,
                     *(p.with_suffix('') for s in suffixes for p in sorted(fw_dir.glob(f'*/{base}{s}')))]:
            for s in suffixes:
                if cand.with_suffix(s).exists():
                    return cand.with_suffix(s)
    return None
