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
RESET_NOOP = {'esptool', 'lm4flash', 'stflash', 'uniflash'}

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
OPENCOD_ADI_PATH = Path.home() / 'app' / 'openocd_adi'
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
    script = ['halt', 'r', f'loadfile {firmware}.elf', 'r', 'go', 'exit']
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
    return run_cmd(f'STM32_Programmer_CLI --connect port=swd sn={flasher["uid"]} --write {firmware}.elf --go')


def reset_stlink(board):
    flasher = board['flasher']
    return run_cmd(f'STM32_Programmer_CLI --connect port=swd sn={flasher["uid"]} --rst --go')

def flash_stflash(board, firmware):
    flasher = board['flasher']
    ret = run_cmd(f'st-flash --serial {flasher["uid"]} write {firmware}.bin 0x8000000')
    return ret


def reset_stflash(board):
    flasher = board['flasher']
    return subprocess.CompletedProcess(args=['dummy'], returncode=0)


def flash_openocd(board, firmware):
    flasher = board['flasher']
    ret = run_cmd(f'openocd -c "tcl_port disabled" -c "gdb_port disabled" -c "adapter serial {flasher["uid"]}" '
                  f'{flasher["args"]} -c "init; halt; program {firmware}.elf verify; reset; exit"')
    return ret


def reset_openocd(board):
    flasher = board['flasher']
    ret = run_cmd(f'openocd -c "tcl_port disabled" -c "gdb_port disabled" -c "adapter serial {flasher["uid"]}" '
                  f'{flasher["args"]} -c "init; reset run; exit"')
    return ret


def flash_openocd_wch(board, firmware):
    flasher = board['flasher']
    ret = run_cmd(f'openocd -c "tcl_port disabled" -c "gdb_port disabled" -c "telnet_port disabled" '
                  f'-c "adapter serial {flasher["uid"]}" {flasher.get("args", "")} -c "program {firmware}.elf reset exit"')
    return ret


def reset_openocd_wch(board):
    flasher = board['flasher']
    ret = run_cmd(f'openocd -c "tcl_port disabled" -c "gdb_port disabled" -c "telnet_port disabled" '
                  f'-c "adapter serial {flasher["uid"]}" {flasher.get("args", "")} -c "init; reset run; exit"')
    return ret


def flash_openocd_adi(board: Board, firmware: str) -> subprocess.CompletedProcess:
    flasher = board['flasher']
    openocd = OPENCOD_ADI_PATH / 'src' / 'openocd'
    tcl_dir = OPENCOD_ADI_PATH / 'tcl'
    ret = run_cmd(f'{openocd} -c "adapter serial {flasher["uid"]}" -s {tcl_dir} '
                  f'{flasher["args"]} -c "program {firmware}.elf reset exit"')
    return ret


def reset_openocd_adi(board: Board) -> subprocess.CompletedProcess:
    flasher = board['flasher']
    openocd = OPENCOD_ADI_PATH / 'src' / 'openocd'
    tcl_dir = OPENCOD_ADI_PATH / 'tcl'
    ret = run_cmd(f'{openocd} -c "adapter serial {flasher["uid"]}" -s {tcl_dir} '
                  f'{flasher["args"]} -c "program reset exit"')
    return ret


def flash_wlink_rs(board, firmware):
    flasher = board['flasher']
    # wlink use index for probe selection and lacking usb serial support
    ret = run_cmd(f'wlink flash {firmware}.elf')
    return ret


def reset_wlink_rs(board):
    flasher = board['flasher']
    # wlink use index for probe selection and lacking usb serial support
    ret = run_cmd(f'wlink reset')
    return ret


def flash_esptool(board: Board, firmware: str) -> subprocess.CompletedProcess:
    flasher = board['flasher']
    port = get_serial_dev(flasher["uid"], None, None, 0)
    fw_dir = Path(f'{firmware}.bin').parent
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


def flash_uniflash(board, firmware):
    flasher = board['flasher']
    ret = run_cmd(f'dslite.sh {flasher["args"]} -f {firmware}.hex')
    return ret


def reset_uniflash(board):
    flasher = board['flasher']
    return subprocess.CompletedProcess(args=['dummy'], returncode=0)


def flash_lm4flash(board, firmware):
    # TI Tiva-C / Stellaris ICDI: lightweight lm4flash, resets and runs after write
    flasher = board['flasher']
    ret = run_cmd(f'lm4flash -s {flasher["uid"]} {flasher["args"]} {firmware}.bin')
    return ret


def reset_lm4flash(board):
    # lm4flash has no reset-only mode; it resets+runs on flash, so reset is a no-op
    flasher = board['flasher']
    return subprocess.CompletedProcess(args=['dummy'], returncode=0)


def find_firmware(variant: str, example: str, roots: list | None = None):
    """Locate a built example's firmware base path (no extension) under
    <build_dir>/cmake-build-<variant>/<example>/, then under EXTRA_BUILD_DIRS
    (empty unless the caller opts in — see its comment). `roots` overrides that
    search list entirely for one call (e.g. to find a build just produced by
    tools/build.py in its fixed cmake-build/ layout without widening the global
    policy). Accepts the single-config layout (firmware directly in the example
    dir) or Ninja Multi-Config (a per-config subdir like RelWithDebInfo/).
    Returns the base Path, or None if not built."""
    base = Path(example).name
    for bd in dict.fromkeys(roots if roots is not None else [build_dir, *EXTRA_BUILD_DIRS]):
        fw_dir = TINYUSB_ROOT / bd / f'cmake-build-{variant}' / example
        if not fw_dir.is_dir():
            continue
        for cand in [fw_dir / base, fw_dir / 'RelWithDebInfo' / base,
                     *(p.with_suffix('') for p in sorted(fw_dir.glob(f'*/{base}.elf')))]:
            if cand.with_suffix('.elf').exists() or cand.with_suffix('.bin').exists():
                return cand
    return None
