# SPDX-License-Identifier: MIT
"""What a test needs to drive usbtest.main() in-process: the serial stub hil_test's import
needs, a stdout that survives main's reconfigure, and the collaborators that would touch
sysfs, dmesg or a device, stubbed. Import before hil_test."""
import io
import os
import signal
import sys
import time
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


def end_cleanup_holder(hil_lock, board, finish, wait=1.0):
    """Finish a retained session and leave no cleanup-holder process behind.

    A loaded host can make release_token() time out after it has signalled the
    holder.  Test teardown must still wait for that exact process to exit, or
    kill it before TemporaryDirectory removes its lock record and receipt.
    """
    rec = hil_lock.read_record(board)
    if not hil_lock.is_cleanup_holder(rec):
        return

    finish()
    pid, start = rec['pid'], rec['start']
    try:
        hil_lock.release_token(rec['token'])
    except hil_lock.ReleaseRefused:
        pass

    deadline = time.monotonic() + wait
    while hil_lock.proc_start(pid) == start and time.monotonic() < deadline:
        try:
            reaped, _ = os.waitpid(pid, os.WNOHANG)
        except ChildProcessError:
            reaped = 0
        if reaped == pid:
            return
        time.sleep(0.01)

    if hil_lock.proc_start(pid) == start:
        try:
            hil_lock._pidfd_signal(  # Test teardown needs the provider's PID-reuse guard.
                pid, start, signal.SIGKILL, lambda: hil_lock.proc_start(pid) == start)
        except hil_lock.ReleaseRefused:
            if hil_lock.proc_start(pid) != start:
                return
            raise

        deadline = time.monotonic() + wait
        while hil_lock.proc_start(pid) == start and time.monotonic() < deadline:
            try:
                reaped, _ = os.waitpid(pid, os.WNOHANG)
            except ChildProcessError:
                reaped = 0
            if reaped == pid:
                return
            time.sleep(0.01)
        if hil_lock.proc_start(pid) == start:
            raise RuntimeError(f'cleanup holder pid {pid} survived SIGKILL')


def stub_device(test, usbtest, run_case):
    """main() finds DEV, passes the host check, needs no pattern or stranded sysfs,
    and runs every case through `run_case(num, dev, testusb, quick, timeout)`."""
    patch(test, usbtest, 'find_device', lambda serial: dict(DEV))
    patch(test, usbtest, 'check_host_compat', lambda d: None)
    patch(test, usbtest, 'set_pattern', lambda v: None)
    patch(test, usbtest, '_uo', lambda: types.SimpleNamespace(path_stranded=lambda p: False,
                                                              SYS_USB_DEVICES='/sys/bus/usb/devices'))
    patch(test, usbtest, 'run_case', run_case)


def argv(test, *extra):
    """sys.argv for a one-case --json run of DEV, restored after the test."""
    test.addCleanup(setattr, sys, 'argv', sys.argv)
    sys.argv = ['usbtest.py', '--serial', DEV['serial'], '--json', '--tests', '1',
                '--testusb', sys.executable, *extra]
