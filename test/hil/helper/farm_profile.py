#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""tinyusb's profile for agentrc's farm check engine (the `hil` skill's Farm check; the engine's
module docstring is the contract), run by helper/hil_farm_check.py.

Per board: the flash probe matched by its uid (never a cafe device), then a reset through it
with tinyusb's hil_flash (esptool: rom_reset_esptool) and the engine's tracked runner. A reset
that exits 0 counts only when the provider's reset_problem finds none; lm4flash has no
reset-only mode. An openocd_esp session the provider leaves running joins the engine's ledger
(_retain).

Rosters by hostname: ci -> tinyusb.json, tusb (hifiphile rig) -> hfp.json; any other host
names its roster (a dev PC: test/hil/local.json) or is refused.
"""

import functools
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))  # hil_flash + the helper package
import hil_flash
from helper import hil_util, usb_observe

REPO_ROOT = hil_util.TINYUSB_ROOT
CONFIG_BY_HOST = {'ci': 'tinyusb.json', 'tusb': 'hfp.json'}


# --- roster -----------------------------------------------------------------------------------
def roster(host: str, config, args) -> dict:
    name = config or CONFIG_BY_HOST.get(host)
    if name is None:
        raise LookupError(f'host {host} has no roster here: name one (a dev PC: test/hil/local.json)')
    path = Path(name)
    if not path.exists():
        path = REPO_ROOT / 'test' / 'hil' / name
    with path.open() as f:
        cfg = json.load(f)
    # boards-skip is parked hardware: checked only when -b names it
    return {'path': str(path), 'boards': list(cfg['boards']), 'named_only': list(cfg.get('boards-skip', []))}


def lock_name(board: dict) -> str:
    return board['name']


def probe_name(board: dict) -> str:
    return board['flasher']['name']


# --- the bus ----------------------------------------------------------------------------------
def scan_usb() -> dict:
    """busport -> {'serial', 'vidpid'} for every enumerated USB device, root hubs excluded
    (their 'serial' is a fabricated PCI address). Keyed by busport, not serial: an
    Espressif USB-Serial-JTAG bridge and the cafe device it flashes share one (the MAC)."""
    return {d['busport']: {'serial': d['serial'].lower(), 'vidpid': f"{d['vid']}:{d['pid']}"}
            for d in usb_observe.usb_scan()}


def probe_candidates(board: dict) -> list:
    """Busports of every flasher probe matching the board's flasher uid, best tier only
    (usb_observe.jlink_serial_candidates: J-Link's zero padding of an all-digit serial, never
    for an all-zero uid), excluding VID cafe (TinyUSB DUT firmware): a probe's uid can
    coincidentally equal its DUT's (Espressif USB-Serial-JTAG bridges derive both from the
    same MAC), and the DUT is never the probe."""
    probes = {bp: dev['serial'] for bp, dev in scan_usb().items() if not dev['vidpid'].startswith('cafe:')}
    return usb_observe.jlink_serial_candidates(board['flasher']['uid'], probes)


# --- inside the engine's reservation ----------------------------------------------------------
def _retain(ctx, board: dict) -> None:
    """An openocd_esp session the provider left running (hil_flash.cleanup_pending) goes to the
    engine's ctx.retain, its detail naming the receipts (its receipt path, or its native record
    when that file was not written), which only the contract's recovery ends. The session is
    the provider's own, so ctx.run never saw it; an esptool child's tracked receipt is ctx.run's,
    already on the ledger with its recheck."""
    receipts = [r for r in hil_flash.cleanup_pending.get(board['name'], ()) if not hil_flash.tracked_receipt(r)]
    ctx.retain(f'openocd_esp session, receipt {", ".join(map(hil_flash.receipt_text, receipts))}', receipts)


def reset(ctx, board: dict):
    name = board['flasher']['name'].lower()
    reset_fn = hil_flash.rom_reset_esptool if name == 'esptool' else hil_flash.reset_primitive(name)
    if reset_fn is None:
        return None
    try:
        op = reset_fn(board, run=functools.partial(ctx.run, verbose=ctx.args.verbose))
    finally:
        _retain(ctx, board)
    # hil_flash's judged failure (`.problem`), else a reset that exited 0 without the target's answer
    if why := getattr(op, 'problem', '') or op.returncode == 0 and hil_util.provider().reset_problem(name, op):
        e = RuntimeError(why)
        e.operation = op
        raise e
    return op
