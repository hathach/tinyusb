#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Bottom layer of the HIL harness: the bounded command runner (agentrc's, wrapped with this
# harness's bounds) plus the shared helpers and data every other module needs. Stays
# stdlib-only at import -- everything else imports this, including the unit tests on GitHub's
# bare runner; never import them from here. Callers set the module global `verbose`.

from __future__ import annotations

import glob
import os
import subprocess
import sys
from pathlib import Path
from typing import Any


# -------------------------------------------------------------
# HIL example test lists, shared by hil_test.py (runner) and change_impact.py (PR-diff
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


# agentrc's flash provider (the `hil` skill's Flashers): its hil_flash, whose runner is
# `.hil_process`. Loaded on first use, never at import: change_impact imports this module on
# a bare checkout before CI fetches lib/agentrc. Kept for the process because
# helper/agentrc.load makes a new module per call, and a second load is a second runner.
_provider = None


def provider():
    """agentrc's hil_flash; ImportError, before any tool runs, when lib/agentrc is missing,
    off the pin, or lacks a file the provider loads or launches (a staged tree included)."""
    global _provider
    if _provider is None:
        from . import agentrc
        # literal paths: RemoteStaging reads them to check the rig gets each one
        _provider = agentrc.load('skills/hil/scripts/hil_flash.py', 'tinyusb_hil_flash_provider',
                                 requires=('skills/hil/scripts/hil_process.py',
                                           'skills/hil/scripts/pyocd/run_pyocd.py',
                                           'skills/hil/scripts/pyocd/user_script.py'))
    return _provider


_openocd_esp = None


def openocd_esp():
    """agentrc's `openocd_esp` backend (hil_openocd_esp), loaded once like provider(): its own
    hil_flash and usb_observe instances, and target-debug's esp_session for the managed session."""
    global _openocd_esp
    if _openocd_esp is None:
        from . import agentrc
        _openocd_esp = agentrc.load('skills/hil/scripts/hil_openocd_esp.py', 'tinyusb_hil_openocd_esp',
                                    requires=('skills/hil/scripts/hil_flash.py',
                                              'skills/hil/scripts/hil_process.py',
                                              'skills/hil/scripts/usb_observe.py',
                                              'skills/target-debug/scripts/esp_session.py'))
    return _openocd_esp


def run_cmd(cmd: str | list, cwd: str | None = None, timeout: int | None = None,
            binary: bool = False, split_stderr: bool = False,
            quiet: bool = False) -> subprocess.CompletedProcess:
    """The provider's bounded runner (hil_process.run_cmd: own session, killpg on expiry, rc 124
    when it had to be killed) with this harness's CMD_TIMEOUT default, REAP_GRACE and
    `verbose`, read on every call."""
    return provider().hil_process.run_cmd(
        cmd, cwd, timeout=CMD_TIMEOUT if timeout is None else timeout, binary=binary,
        split_stderr=split_stderr, quiet=quiet, reap_grace=REAP_GRACE, verbose=verbose)


def run_alongside(argv: list, work, timeout: int) -> subprocess.CompletedProcess:
    """The provider's hil_process.run_alongside, reaping for this harness's REAP_GRACE."""
    return provider().hil_process.run_alongside(argv, work, timeout, reap_grace=REAP_GRACE)


def cmd_stdout_text(out: Any) -> str:
    return provider().hil_process.cmd_stdout_text(out)


def strip_workflow_markers(line: str) -> str:
    return provider().hil_process.strip_workflow_markers(line)


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


def ramp_gaps(samples: list, start: int) -> list:
    """(index, delta) of every sample after `start` that does not continue a +1 uint16 ramp;
    delta is got - expected, wrapped to [-32768, 32767]."""
    deltas = (((b - a - 1 + 0x8000) & 0xFFFF) - 0x8000 for a, b in zip(samples[start:], samples[start + 1:]))
    return [(i, d) for i, d in enumerate(deltas, start + 1) if d]
