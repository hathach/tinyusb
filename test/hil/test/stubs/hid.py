# SPDX-License-Identifier: MIT
"""Scripted stand-in for cython-hidapi, for the HID_ECHO child tests.

A real wedge cannot be manufactured on demand, so the failure modes are scripted here and
selected with FAKE_HID_MODE. Mirrors test/stubs/pymtp.py, which does the same for libmtp.
"""
import ctypes
import ctypes.util
import os
import time

_MODE = os.environ.get('FAKE_HID_MODE', 'ok')
_UID = os.environ.get('FAKE_HID_UID', 'CAFE01')


def _gil_stall():
    """Block forever WITHOUT releasing the GIL -- the shape cython-hidapi's bare
    hid_open()/hid_close() calls have, and the one an in-process bound cannot touch.

    PyDLL, not CDLL: CDLL releases the GIL around the call, which would make this the
    easy case instead of the hard one. Resolved through find_library so a non-glibc libc
    still works; PyDLL(None) is not usable here (its `sleep` returns immediately).
    """
    ctypes.PyDLL(ctypes.util.find_library('c') or 'libc.so.6').sleep(3600)
_PID = int(os.environ.get('FAKE_HID_PID', '0x4012'), 16)


def enumerate(vid=0, pid=0):
    """Real hid.enumerate(vid, pid) filters on both ids -- 0 means "any" -- and returns a
    'path' key too. The filters are applied BEFORE the locked manufacturer/product reads,
    which is why passing both narrows what a wedged peer can stall."""
    if _MODE == 'wedged_enumerate':
        # hidapi's hidraw backend reads `manufacturer`/`product` for every device it
        # lists, both served under the device lock -- this is that stall.
        while True:
            time.sleep(3600)
    if _MODE == 'absent':
        return []
    if vid not in (0, 0xCafe) or pid not in (0, _PID):
        return []
    return [{'serial_number': _UID, 'vendor_id': 0xCafe, 'product_id': _PID,
             'path': b'/dev/hidraw0'}]


class device:
    def __init__(self):
        self._last = b''

    def open(self, vid, pid, serial):
        # HID_ECHO really does call this, and usb_autopm/hidraw can block in it, so the
        # child must be bounded here too -- exercised by test_a_wedged_open_is_killed.
        if _MODE == 'wedged_open':
            while True:
                time.sleep(3600)
        if _MODE == 'wedged_open_gil':
            # a thread-based bound is inert against this; only killing the process works
            _gil_stall()

    def write(self, report):
        self._last = bytes(report)

    def read(self, size, timeout_ms):
        if _MODE == 'wedged_read':
            while True:
                time.sleep(3600)
        if _MODE == 'short_read':
            return list(self._last[1:4])
        if _MODE == 'wrong_data':
            return list(bytes(b ^ 0xFF for b in self._last[1:]))
        return list(self._last[1:])       # the device echoes the payload, minus report ID

    def close(self):
        if _MODE == 'wedged_close':
            # also GIL-holding in cython-hidapi, and it runs in HID_ECHO's finally on
            # every failure path
            _gil_stall()
