# SPDX-License-Identifier: MIT
"""Scripted stand-in for cython-hidapi, for the HID_ECHO child tests.

A real wedge cannot be manufactured on demand, so the failure modes are scripted here and
selected with FAKE_HID_MODE. Mirrors test/stubs/pymtp.py, which does the same for libmtp.
"""
import os
import time

_MODE = os.environ.get('FAKE_HID_MODE', 'ok')
_UID = os.environ.get('FAKE_HID_UID', 'CAFE01')


def enumerate(vid=0xCafe):
    """Real hid.enumerate(vid) filters by vendor and returns a 'path' key too."""
    if _MODE == 'wedged_enumerate':
        # hidapi's hidraw backend reads `manufacturer`/`product` for every device it
        # lists, both served under the device lock -- this is that stall.
        while True:
            time.sleep(3600)
    if _MODE == 'absent':
        return []
    if vid not in (0, 0xCafe):
        return []
    return [{'serial_number': _UID, 'vendor_id': 0xCafe, 'product_id': 0x4004,
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
        pass
