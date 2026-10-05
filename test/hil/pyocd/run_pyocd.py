#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Run pyocd for the HIL harness: probe discovery filtered to one USB VID/PID, and
user_script.py loaded as the session's user script.

Usage: <pyocd's python> run_pyocd.py 0xVVVV 0xPPPP <pyocd args...>

pyocd has no VID/PID filter: its CMSIS-DAP discovery opens every device whose class could
be CMSIS-DAP to read its interface string, and checks the serial last (pyocd 0.45.1
pyusb_v2_backend.py HasCmsisDapv2Interface). On a rig one wedged node then hangs every flash,
and each flash sends control requests to other boards' probes and DUTs. pyusb's find() tests
keyword filters before custom_match, from the cached device descriptor, so injecting
idVendor/idProduct keeps every other device unopened. Only the CMSIS-DAP probe plugin is
left registered: the J-Link one enumerates through SEGGER's DLL, where the filter cannot reach.
"""
import functools
import os
import sys
from pathlib import Path


def _fail(msg: str):
    print(f'run_pyocd.py: {msg}', file=sys.stderr)
    sys.exit(2)


def _filter_vid_pid(find, vid: int, pid: int):
    @functools.wraps(find)
    def filtered(*args, **kwargs):
        for key, want in (('idVendor', vid), ('idProduct', pid)):
            if kwargs.setdefault(key, want) != want:
                raise RuntimeError(f'run_pyocd.py: a caller asked for {key}={kwargs[key]:#06x}')
        return find(*args, **kwargs)
    return filtered


def main(argv: list) -> int:
    if not sys.platform.startswith('linux'):
        _fail(f'the VID/PID filter is only verified on Linux, not {sys.platform}')
    try:
        vid, pid = (int(v, 16) for v in argv[:2])
    except ValueError:
        _fail(f'want "0xVVVV 0xPPPP" before the pyocd args, got {argv[:2]}')
    # hidapi would bypass pyusb; pyocd reads this when its interface package is imported
    os.environ['PYOCD_USB_BACKEND'] = 'pyusb'

    # before any pyocd import: its backends bind `find` at import time
    import usb.core
    usb.core.find = _filter_vid_pid(usb.core.find, vid, pid)
    filtered = [usb.core.find]
    try:
        import libusb_package
        libusb_package.find = _filter_vid_pid(libusb_package.find, vid, pid)
        filtered.append(libusb_package.find)
    except ImportError:
        pass

    from pyocd.probe.pydapaccess.interface import pyusb_backend, pyusb_v2_backend
    for mod in (pyusb_backend, pyusb_v2_backend):
        if getattr(mod, 'usb_find', None) not in filtered:
            _fail(f'{mod.__name__}.usb_find is not the filtered find; pyocd changed how it imports it')

    from pyocd.probe.aggregator import PROBE_CLASSES
    if 'cmsisdap' not in PROBE_CLASSES:
        _fail(f'pyocd has no cmsisdap probe plugin: {sorted(PROBE_CLASSES)}')
    for name in [k for k in PROBE_CLASSES if k != 'cmsisdap']:
        del PROBE_CLASSES[name]

    from pyocd.__main__ import main as pyocd_main
    # last, so they win over the args; no pyocd.yaml from whatever cwd the harness runs in
    sys.argv = ['pyocd', *argv[2:], '--no-config',
                '--script', str(Path(__file__).resolve().with_name('user_script.py'))]
    return pyocd_main()


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
