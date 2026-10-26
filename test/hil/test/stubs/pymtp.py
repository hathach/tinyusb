# SPDX-License-Identifier: MIT
# Fake pymtp for the hil unit tests — stands in both for the import (GitHub's bare
# pre-commit runner has no libmtp/pymtp) and for a scripted MTP device. Behavior is
# driven by env vars so subprocesses (mtp_test.py under run_cmd) can be steered:
#   FAKE_PYMTP_MODE   absent (default) | ok | hang
#   FAKE_PYMTP_UID    serial number the fake device reports
#   FAKE_PYMTP_FILE1  text served as file id 1 (README.TXT)
#   FAKE_PYMTP_LOGO   path to the logo bytes served as file id 2
# File contents come from env, not constants: the test extracts them from the example's
# own sources, so this stub cannot drift out of sync with the firmware.
# 'hang' blocks forever inside detect_devices — the in-process libmtp equivalent of a
# D-state usbfs ioctl on a wedged device.
import ctypes
import os
import time


class NotConnected(Exception):
    pass


class LIBMTP_DeviceEntry(ctypes.Structure):
    """Real pymtp exposes this; mtp_test builds one to open a KNOWN device instead of
    probing every MTP device on the bus."""
    _fields_ = [('vendor', ctypes.c_char_p), ('vendor_id', ctypes.c_uint16),
                ('product', ctypes.c_char_p), ('product_id', ctypes.c_uint16),
                ('device_flags', ctypes.c_uint32)]


class LIBMTP_RawDevice(ctypes.Structure):
    _fields_ = [('device_entry', LIBMTP_DeviceEntry), ('bus_location', ctypes.c_uint32),
                ('devnum', ctypes.c_uint8)]


class _LibShim:
    @staticmethod
    def LIBMTP_Open_Raw_Device(_ref):
        # mtp_test no longer calls detect_devices() (it probed every MTP device on the
        # rig), so the scripted modes have to act here -- this is the only libmtp entry
        # point the marker-based open goes through.
        mode = os.environ.get('FAKE_PYMTP_MODE', 'absent')
        if mode == 'hang':
            time.sleep(10000)
        if mode == 'error':
            raise RuntimeError('CommandFailed: LIBMTP_ERROR_USB_LAYER')
        if mode == 'error_then_ok':
            flag = os.environ.get('FAKE_PYMTP_ERRED_MARKER', '/tmp/.fake_pymtp_erred')
            if not os.path.exists(flag):
                open(flag, 'w').close()
                raise RuntimeError('CommandFailed: LIBMTP_ERROR_PTP_LAYER')
        if mode == 'absent':
            return ctypes.POINTER(ctypes.c_int)()      # NULL: nothing to open
        # the real one has restype POINTER(LIBMTP_MTPDevice): a failed open returns a
        # NULL pointer, which is FALSY but compares unequal to None -- the distinction
        # mtp_test's `if mtp.device:` guards depend on
        if os.environ.get('FAKE_PYMTP_OPEN') == 'null':
            return ctypes.POINTER(ctypes.c_int)()
        return 1


class MTP:
    def __init__(self):
        self.mtp = _LibShim()
        self.device = None
        self._sent = {}
        self._next_id = 3

    def detect_devices(self):
        mode = os.environ.get('FAKE_PYMTP_MODE', 'absent')
        if mode == 'hang':
            time.sleep(10000)
        if mode == 'error':
            # real pymtp raises for USB_LAYER/PTP_LAYER/GENERAL/AlreadyConnected;
            # the first poll after a flash routinely hits one
            raise RuntimeError('CommandFailed: LIBMTP_ERROR_USB_LAYER')
        if mode == 'error_then_ok':
            if not getattr(self, '_erred', False):
                self._erred = True
                raise RuntimeError('CommandFailed: LIBMTP_ERROR_USB_LAYER')
            return [ctypes.c_int(1)]
        if mode != 'ok':
            return []
        return [ctypes.c_int(1)]

    def get_serialnumber(self):
        return os.environ.get('FAKE_PYMTP_UID', '').encode()

    def get_manufacturer(self):
        return b'TinyUSB'

    def get_modelname(self):
        return b'MTP Example'

    def get_deviceversion(self):
        return b'1.0'

    def get_devicename(self):
        return b'TinyUSB MTP'

    def get_file_to_file(self, fid, path):
        if fid == 1:
            data = os.environ['FAKE_PYMTP_FILE1'].encode()
        elif fid == 2:
            with open(os.environ['FAKE_PYMTP_LOGO'], 'rb') as f:
                data = f.read()
        else:
            data = self._sent[fid]
        with open(path, 'wb') as f:
            f.write(data)

    def send_file_from_file(self, path, _name):
        with open(path, 'rb') as f:
            self._sent[self._next_id] = f.read()
        self._next_id += 1
        return self._next_id - 1

    def delete_object(self, fid):
        del self._sent[fid]

    def disconnect(self):
        # vendored pymtp raises when nothing is connected; a stub that silently accepts
        # it hides a LIBMTP_Release_Device(NULL) call on real hardware
        if self.device is None:
            raise NotConnected('no device connected')
        self.device = None
