#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Firmware flashing for the TinyUSB HIL rig: one flash_*/reset_* pair per flasher type
# (dispatched by config name via getattr) plus find_firmware. The bounded runner run_cmd
# lives in hil_util (never import hil_test here). Callers set the module global
# `build_dir`. `from __future__ import annotations` keeps the Board hints below
# unevaluated: the type is not defined in this module.

from __future__ import annotations

import json
import re
import subprocess
from pathlib import Path

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))  # PYTHONSAFEPATH drops it
from helper import hil_util

build_dir = 'cmake-build'

# flasher names (dispatch key, board['flasher']['name'].lower()) whose reset_* is a no-op
RESET_NOOP = {'esptool', 'lm4flash'}

# extra parents find_firmware ALSO searches after build_dir. Empty by default so
# hil_test's -B stays authoritative: a board missing there must report "Skip (no
# binary)", never silently flash a stale binary from another tree.
EXTRA_BUILD_DIRS: list = []

_VID_PID_WARNED: set = set()   # one warning per probe, not per command


# -------------------------------------------------------------
# Flashing firmware
# -------------------------------------------------------------
def flash_jlink(board: Board, firmware: str, timeout=None) -> subprocess.CompletedProcess:
    flasher = board['flasher']
    script = ['halt', 'r', f'loadfile {firmware}', 'r', 'go', 'exit']
    f_jlink = Path(f'{board["name"]}_{Path(firmware).name}.jlink')
    with f_jlink.open('w') as f:
        f.writelines(f'{s}\n' for s in script)
    ret = hil_util.run_cmd(f'JLinkExe -USB {flasher["uid"]} {flasher["args"]} -if swd -JTAGConf -1,-1 -speed auto -NoGui 1 -ExitOnError 1 -CommandFile {f_jlink}',
                           timeout=timeout)
    f_jlink.unlink(missing_ok=True)
    return ret


def reset_jlink(board: Board) -> subprocess.CompletedProcess:
    flasher = board['flasher']
    script = ['halt', 'r', 'go', 'exit']
    f_jlink = Path(f'{board["name"]}_reset.jlink')
    if not f_jlink.exists():
        with f_jlink.open('w') as f:
            f.writelines(f'{s}\n' for s in script)
    ret = hil_util.run_cmd(f'JLinkExe -USB {flasher["uid"]} {flasher["args"]} -if swd -JTAGConf -1,-1 -speed auto -NoGui 1 -ExitOnError 1 -CommandFile {f_jlink}')
    return ret


def flash_stlink(board, firmware, timeout=None):
    # --verify catches the partial/corrupt write that exits 0 and sends the test phase
    # off to exercise bad firmware. Opt-IN here ("verify": true), unlike flash_openocd's
    # opt-out: a default-on read-back silently changes every roster entry that lacks the
    # key, including boards on rigs this was never validated against.
    flasher = board['flasher']
    verify = ' --verify' if flasher.get('verify', False) else ''
    return hil_util.run_cmd(f'STM32_Programmer_CLI --connect port=swd sn={flasher["uid"]} --write {firmware}{verify} --go',
                            timeout=timeout)


def reset_stlink(board):
    flasher = board['flasher']
    return hil_util.run_cmd(f'STM32_Programmer_CLI --connect port=swd sn={flasher["uid"]} --rst --go')


def _openocd_cmd_base(flasher):
    # Optional roster field vid_pid, openocd-verbatim (e.g. "0x1a86 0x8010"), pins probe
    # discovery to the probe's IDs so openocd never opens foreign usbfs nodes to read
    # strings -- a wedged node makes that open hang unkillably (the 2026-08-10 convoy).
    # BEFORE args, because the rescue cfgs run `init` internally and reject (or never see)
    # a config command that follows it.
    vid_pid = ''
    if 'vid_pid' in flasher:
        # Validated HERE too, not just in convoy_safe: openocd only warns ("incomplete
        # vid_pid configuration directive") and exits 0 on a malformed value, so the pin
        # silently does not apply and discovery goes back to opening every usbfs node --
        # the convoy this field exists to stop. The same key name carries a DIFFERENT
        # syntax under tests.dev_attached ('1a86_55d4'), so the typo is one copy away.
        if valid_vid_pid(flasher['vid_pid']):
            vid_pid = f'-c "adapter usb vid_pid {flasher["vid_pid"]}" '
        else:
            # stderr + once-per-probe, like the missing-pin branch below: stdout here is
            # captured by test_example's redirect_stdout (shown only when the test FAILS)
            # and by hil_pool_check's StringIO spool, so on a PASSING run the operator
            # would never learn the pin was silently dropped.
            uid = flasher.get('uid', '?')
            if uid not in _VID_PID_WARNED:
                _VID_PID_WARNED.add(uid)
                print(f'warning: {uid} has a malformed vid_pid {flasher["vid_pid"]!r} '
                      f'(want "0xVVVV 0xPPPP"); probe pin DROPPED, so discovery will open '
                      f'foreign usbfs nodes', file=sys.stderr, flush=True)
    elif flasher.get('uid') not in _VID_PID_WARNED:
        # stderr, once per probe: test_example captures stdout, so a passing run would
        # swallow this and the operator would never learn discovery still opens every
        # usbfs node
        _VID_PID_WARNED.add(flasher.get('uid'))
        print(f'warning: openocd flasher {flasher.get("uid", "?")} has no vid_pid pin; '
              f'probe discovery will open every usbfs node (hangs on a wedged one)',
              file=sys.stderr, flush=True)
    return (f'openocd -c "tcl_port disabled" -c "gdb_port disabled" -c "telnet_port disabled" '
            f'-c "adapter serial {flasher["uid"]}" {vid_pid}{flasher["args"]}')


# `verify` is on by default, opted out per board with "verify": false. WCH targets must
# opt out: read-back over the WCH-Link sdi transport returns a repeated word instead of
# memory contents, so verification always mismatches (measured on ch32v103r and ch32v307v,
# 2026-07-30). Do NOT drop verify fleet-wide for them — every other openocd board reads
# back, and without it a partial or corrupt write exits 0 and the tests run bad firmware.
def flash_openocd(board, firmware, timeout=None):
    flasher = board['flasher']
    verify = ' verify' if flasher.get('verify', True) else ''
    ret = hil_util.run_cmd(f'{_openocd_cmd_base(flasher)} -c "program {firmware}{verify} reset exit"',
                           timeout=timeout)
    return ret


def reset_openocd(board, timeout=None):
    # timeout: usbtest's post-hang recovery bounds this (RECOVER_RESET_TIMEOUT); an
    # unbounded reset there would outlive the caller's outer kill and orphan openocd on
    # the probe, which is the stray the recovery exists to avoid.
    flasher = board['flasher']
    ret = hil_util.run_cmd(f'{_openocd_cmd_base(flasher)} -c "init; reset run; exit"',
                           timeout=timeout)
    return ret


# OpenOCD's messages for "the target's debug port did not answer". The probe is fine when
# these appear ("CMSIS-DAP: Interface ready" is still logged); the chip's debug clock is
# gone, which no probe-driven reset fixes -- the CMSIS-DAP probe has no nRESET line. Which
# message appears depends on DAP topology, not the board, so both are accepted for both
# chips; RESCUE_CFG below picks the rescue.
DAP_WEDGED = ('Failed to connect multidrop', 'Error connecting DP: cannot read IDR')

# How each RP target reaches its Rescue DP, keyed by the target cfg named in flasher args:
# (cfg substitution, pre args, post args). rp2040.cfg drives the Rescue DP behind a RESCUE
# flag and init/shutdowns itself; rp2350-rescue.cfg never shuts down, so it needs an
# explicit one or it sits in the server loop until CMD_TIMEOUT.
RESCUE_CFG = {
    'target/rp2040.cfg': ('target/rp2040.cfg', '-c "set RESCUE 1" ', ''),
    'target/rp2350.cfg': ('target/rp2350-rescue.cfg', '', ' -c "shutdown"'),
}


def rescue_openocd(board, flash_out: str = '', timeout=None) -> bool:
    """Power-on-reset a wedged RP2040/RP2350 through its Rescue DP, the one debug port not
    gated by the system clock (RP2040 datasheet 2.3.4.2): CDBGPWRUPREQ hard-resets the
    chip and the bootrom halts it ready to be flashed. Without it the board needs a
    physical replug -- the probe carries no reset line.

    No-op (False) unless this is an openocd RP board AND the flash output shows the wedge,
    so a flash that failed for any other reason still just retries. True when a rescue was
    attempted; the caller should retry the flash afterwards."""
    flasher = board['flasher']
    if flasher['name'].lower() != 'openocd' or not any(m in flash_out for m in DAP_WEDGED):
        return False
    for cfg, (rescue_cfg, pre, post) in RESCUE_CFG.items():
        if cfg in flasher['args']:
            args = flasher['args'].replace(cfg, rescue_cfg)
            return hil_util.run_cmd(f'{_openocd_cmd_base({**flasher, "args": pre + args})}{post}',
                                    timeout=timeout).returncode == 0
    return False


# openocd's own syntax: one or more "0xVVVV 0xPPPP" pairs. Validated rather than merely
# tested for truthiness -- `vid_pid` is a hand-edited roster field whose NAME is also used,
# with a different syntax, by tests.dev_attached, and convoy_safe reads a non-empty value
# as PROOF the flasher can deliver a recovery past a poisoned node. A typo there silently
# promised a recovery that openocd would reject at startup.
_VID_PID_RE = re.compile(r'^0x[0-9a-fA-F]{4}(\s+0x[0-9a-fA-F]{4})+$')


def valid_vid_pid(value) -> bool:
    return isinstance(value, str) and bool(_VID_PID_RE.match(value.strip()))


def recover_flasher(board: dict) -> dict:
    """The flasher that delivers RECOVERY for this board.

    Optional roster key `flasher_recover`, else the primary. It exists because delivery and
    normal flashing have different requirements: a board flashed by jlink/stlink/lm4flash
    cannot reach its probe past a poisoned usbfs node, but the same probe driven by openocd
    often can (see convoy_safe). Keeping it a separate key rather than a list means the
    primary's shape never changes, so nothing that reads board['flasher'] has to care.
    """
    return board.get('flasher_recover') or board['flasher']


def convoy_safe(flasher: dict) -> bool:
    """Can this flasher DELIVER a recovery while a usbfs node on the rig is poisoned?

    A post-HUNG reflash only helps if the flasher reaches its probe without opening the
    wedged node. Two shapes qualify:

    * openocd pinned with the roster's `vid_pid` -- the match is made from the cached
      descriptor and the loop `continue`s BEFORE libusb_open, so a foreign node is never
      opened. On 2026-08-12 it was the only flasher that still reached its probe.
    * esptool -- delivery is `-p <ttyACM>`, a named port; it never enumerates usbfs.

    Everything else enumerates by OPENING nodes, would block in D state on the poisoned
    one, survive SIGKILL and become a second stray. JLinkExe cannot be pinned: selection
    is serial-only (-USB/-SelectEmuBySN) and reading a serial requires the open (J-Link
    Commander V9.66 exposes no VID/PID filter), so those boards can only become
    convoy-safe by moving to openocd.

    Verified against openocd 0ce743125 (the rig's build), because the INVERSE is what
    bites: cmsis_dap_usb_bulk.c:107 skips on `id_filter && !id_match`, and `id_filter` is
    only `vids[0] || pids[0]` -- so without the pin nothing is skipped and every device on
    the bus is opened, which the code itself expects to mostly fail. Enumeration cannot
    block: libusb reads the `descriptors` sysfs attribute, and descriptors_read (v6.12.101
    drivers/usb/core/sysfs.c) is a memcpy from udev->rawdescriptors under no lock.

    The pin gates the BULK backend, which is the one that runs: `auto` tries usb_bulk ->
    hid -> tcp (cmsis_dap.c:62) and stops at the first that opens, so a CMSIS-DAP v2 probe
    never reaches the rest. It does NOT cover the HID fallback that a v1 probe or a failed
    bulk open takes -- cmsis_dap_usb_hid.c:91 calls hid_enumerate(0x0, 0x0), pin ignored,
    and filters afterwards, while hidapi's hidraw backend reads `manufacturer` and
    `product` for every HID device it lists (linux/hid.c:744), both usb_string_attr and so
    served under the device lock. A wedged DUT running hid_generic_inout,
    hid_boot_interface or hid_composite_freertos is a HID device and would stall that walk
    -- interruptibly, so it hangs rather than joining the D-state convoy and run_cmd's
    timeout ends it, but "never opens a foreign node" is true of the bulk path, not of
    every path openocd can take.
    """
    name = (flasher.get('name') or '').lower()
    if name == 'esptool':
        return True
    # EXACT, not startswith: rescue_openocd and usbtest's
    # getattr(hil_flash, f'flash_{name}') both require the exact name, so an
    # 'openocd_wch'-style entry would pass this gate, reserve the Rescue-DP legs,
    # and then find no recovery path at all -- paying for a path that cannot fire, which
    # is the precise cost this gate exists to avoid.
    if name != 'openocd':
        return False
    if valid_vid_pid(flasher.get('vid_pid')):
        return True
    # openocd over the JLINK driver is safe WITHOUT a pin, and cannot use one: jlink.c
    # never reads adapter_usb_get_vids/pids (selection is adapter serial / usb address /
    # usb location), but libjaylink's discovery returns early unless idVendor == 0x1366 and
    # the PID is in its table, and only THEN calls libusb_open (discovery_usb.c). So it
    # never opens a foreign node -- which is exactly what JLinkExe, SEGGER's own tool,
    # does do. Verified against openocd 0ce743125 and libjaylink master.
    return 'interface/jlink.cfg' in (flasher.get('args') or '')


def flash_esptool(board: Board, firmware: str, timeout=None) -> subprocess.CompletedProcess:
    flasher = board['flasher']
    port = hil_util.get_serial_dev(flasher["uid"], None, None, 0)
    fw_dir = Path(firmware).parent
    with (fw_dir / 'config.env').open() as f:
        idf_target = json.load(f)['IDF_TARGET']
    with (fw_dir / 'flash_args').open() as f:
        flash_args = f.read().strip().replace('\n', ' ')
    command = (f'esptool --chip {idf_target} -p {port} {flasher["args"]} '
               f'--before=default_reset --after=hard_reset write_flash {flash_args}')
    ret = hil_util.run_cmd(command, cwd=str(fw_dir), timeout=timeout)
    return ret


def reset_esptool(board):
    # NO-OP, and marked as one: esptool's reset would be `--after hard_reset`, which is not
    # wired here. Returning rc 0 without resetting is why callers must never read the exit
    # code as proof -- usbtest's recovery skips a primitive carrying `no_op`.
    return subprocess.CompletedProcess(args=['dummy'], returncode=0)


reset_esptool.no_op = True


def flash_lm4flash(board, firmware, timeout=None):
    # TI Tiva-C / Stellaris ICDI: lightweight lm4flash, resets and runs after write
    flasher = board['flasher']
    ret = hil_util.run_cmd(f'lm4flash -s {flasher["uid"]} {flasher["args"]} {firmware}',
                           timeout=timeout)
    return ret


def reset_lm4flash(board):
    # lm4flash has no reset-only mode; it resets+runs on flash, so reset is a no-op
    return subprocess.CompletedProcess(args=['dummy'], returncode=0)


reset_lm4flash.no_op = True


# The one place a flasher's firmware extension is decided. A flasher with no entry falls
# back to .elf-or-.bin and can be handed the wrong file — test_ci_select's
# TestRosterFlashersDispatch fails if a roster names one.
FLASHER_SUFFIX = {
    'esptool': '.bin',
    'jlink': '.elf',
    'lm4flash': '.bin',
    'openocd': '.elf',
    'stlink': '.elf',
}


def find_firmware(variant: str, example: str, roots: list | None = None, flasher: str | None = None):
    """Locate a built example's firmware under <build_dir>/cmake-build-<variant>/<example>/,
    then under EXTRA_BUILD_DIRS. `roots` overrides that search list entirely for one call
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
    for bd in dict.fromkeys(roots if roots is not None else [build_dir, *EXTRA_BUILD_DIRS]):
        fw_dir = hil_util.TINYUSB_ROOT / bd / f'cmake-build-{variant}' / example
        if not fw_dir.is_dir():
            continue
        for cand in [fw_dir / base, fw_dir / 'RelWithDebInfo' / base,
                     *(p.with_suffix('') for s in suffixes for p in sorted(fw_dir.glob(f'*/{base}{s}')))]:
            for s in suffixes:
                if cand.with_suffix(s).exists():
                    return cand.with_suffix(s)
    return None
