# SPDX-License-Identifier: MIT
"""What a test needs to drive usbtest.main() in-process: the serial stub hil_test's import
needs, a stdout that survives main's reconfigure, and the collaborators that would touch
sysfs, dmesg or a device, stubbed. Import before hil_test."""
import io
import sys
import types

# hil_test imports pyserial, which the bare pre-commit runner lacks; no test opens a port
_serial = types.ModuleType('serial')
_serial.Serial = type('Serial', (), {})
_serial.SerialException = type('SerialException', (Exception,), {})
_serial.SerialTimeoutException = type('SerialTimeoutException', (Exception,), {})
sys.modules.setdefault('serial', _serial)

DEV = {'serial': 'U', 'node': '/dev/bus/usb/999/999', 'speed': '480', 'tier': 1, 'sysname': '1-1'}


class Out(io.StringIO):
    def reconfigure(self, **kw):   # main() line-buffers stdout
        pass


def patch(test, obj, name, value):
    test.addCleanup(setattr, obj, name, getattr(obj, name))
    setattr(obj, name, value)


def stub_device(test, usbtest, run_case):
    """main() finds DEV, passes the host check, needs no pattern, dmesg or stranded sysfs,
    and runs every case through `run_case(num, dev, testusb, quick, timeout)`."""
    patch(test, usbtest, 'find_device', lambda serial, first=False: dict(DEV))
    patch(test, usbtest, 'check_host_compat', lambda d: None)
    patch(test, usbtest, 'set_pattern', lambda v: None)
    patch(test, usbtest, 'dmesg_tail', lambda: '')
    patch(test, usbtest, '_hu', lambda: types.SimpleNamespace(path_stranded=lambda p: False,
                                                              strand_note=lambda: ''))
    patch(test, usbtest, 'run_case', run_case)


def argv(test, *extra):
    """sys.argv for a one-case --json run of DEV, restored after the test."""
    test.addCleanup(setattr, sys, 'argv', sys.argv)
    sys.argv = ['usbtest.py', '--serial', DEV['serial'], '--json', '--tests', '1',
                '--testusb', sys.executable, *extra]
