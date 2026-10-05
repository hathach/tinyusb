#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Run pyocd with probe discovery pinned to one USB VID/PID.

Usage: <pyocd's python> pinned.py 0xVVVV 0xPPPP <pyocd args...>

pyocd has no VID/PID filter: its CMSIS-DAP discovery opens every device whose class could
be CMSIS-DAP to read its interface string, and checks the serial last (pyocd 0.45.1
pyusb_v2_backend.py HasCmsisDapv2Interface). On a rig one wedged node then hangs every flash,
and each flash sends control requests to other boards' probes and DUTs. pyusb's find() tests
keyword filters before custom_match, from the cached device descriptor, so injecting
idVendor/idProduct keeps every other device unopened. Only the CMSIS-DAP probe plugin is
left registered: the J-Link one enumerates through SEGGER's DLL, where the pin cannot reach.
"""
import functools
import os
import sys


def _fail(msg: str):
    print(f'pinned.py: {msg}', file=sys.stderr)
    sys.exit(2)


def _pin(find, vid: int, pid: int):
    @functools.wraps(find)
    def pinned(*args, **kwargs):
        for key, want in (('idVendor', vid), ('idProduct', pid)):
            if kwargs.setdefault(key, want) != want:
                raise RuntimeError(f'pinned.py: a caller asked for {key}={kwargs[key]:#06x}')
        return find(*args, **kwargs)
    pinned.hil_pinned = True
    return pinned


def main(argv: list) -> int:
    if not sys.platform.startswith('linux'):
        _fail(f'the pin is only verified on Linux, not {sys.platform}')
    try:
        vid, pid = (int(v, 16) for v in argv[:2])
    except ValueError:
        _fail(f'want "0xVVVV 0xPPPP" before the pyocd args, got {argv[:2]}')
    # hidapi would bypass pyusb; pyocd reads this when its interface package is imported
    os.environ['PYOCD_USB_BACKEND'] = 'pyusb'

    # before any pyocd import: its backends bind `find` at import time
    import usb.core
    usb.core.find = _pin(usb.core.find, vid, pid)
    try:
        import libusb_package
        libusb_package.find = _pin(libusb_package.find, vid, pid)
    except ImportError:
        pass

    from pyocd.probe.pydapaccess.interface import pyusb_backend, pyusb_v2_backend
    for mod in (pyusb_backend, pyusb_v2_backend):
        if not getattr(getattr(mod, 'usb_find', None), 'hil_pinned', False):
            _fail(f'{mod.__name__}.usb_find is not the pinned find; pyocd changed how it imports it')

    from pyocd.probe.aggregator import PROBE_CLASSES
    if 'cmsisdap' not in PROBE_CLASSES:
        _fail(f'pyocd has no cmsisdap probe plugin: {sorted(PROBE_CLASSES)}')
    for name in [k for k in PROBE_CLASSES if k != 'cmsisdap']:
        del PROBE_CLASSES[name]

    from pyocd.__main__ import main as pyocd_main
    sys.argv = ['pyocd', *argv[2:]]
    return pyocd_main()


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
