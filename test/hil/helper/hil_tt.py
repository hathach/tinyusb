# SPDX-License-Identifier: MIT
"""Reset the hub transaction translator (TT) a full-/low-speed DUT sits behind.

A URB cancelled between a bulk/control start-split and its complete-split can leave that TT
buffer busy until Clear_TT_Buffer (USB 2.0 11.17.5), and Linux xHCI sends one only on an
endpoint halt (v6.12 xhci-ring.c xhci_clear_hub_tt_buffer). Once no buffer is free the TT
NAKs every start-split, so every later transfer to the port times out, re-enumeration
included. Reset_TT (11.24.2.9) frees them without resetting the port or the device.
"""
import fcntl
import os
import struct
import sys
from contextlib import redirect_stdout
from pathlib import Path

SYS_USB = Path('/sys/bus/usb/devices')
_CTRLTRANSFER = struct.Struct('@BBHHHIP')                   # struct usbdevfs_ctrltransfer
USBDEVFS_CONTROL = 0xC0005500 | _CTRLTRANSFER.size << 16   # _IOWR('U', 0, ...)
RESET_TT = 9
HELPER_TIMEOUT = 10

# Hubs whose helper timed out in this process. The usbfs ioctl waits for the HUB's device
# lock, which the hub driver holds while it enumerates any of its ports: the helper may
# still be waiting there, so another would only queue behind it.
_unconfirmed: set = set()


def tt_port(busport, speed):
    """(hub usbfs node, TT port) behind which a full-/low-speed device sits, or None: a
    high-speed device, a root port, or a hub that is not running one TT per port -- a
    single-TT hub's reset would disturb every full-/low-speed device on it, and a hub
    running at full speed has no TT."""
    if speed not in ('12', '1.5'):
        return None
    parent, _, port = busport.rpartition('.')
    if not parent:
        return None
    hub = SYS_USB / parent
    try:
        # sysfs lists only the active configuration's interfaces; a hub has one, and it runs
        # one TT per port only while that interface is the multi-TT one (11.23.1)
        intf = list(hub.glob(f'{parent}:*.0'))
        if len(intf) != 1 or (intf[0] / 'bInterfaceProtocol').read_text().strip() != '02':
            return None
        node = '/dev/bus/usb/%03d/%03d' % (int((hub / 'busnum').read_text()),
                                           int((hub / 'devnum').read_text()))
    except (OSError, ValueError):
        return None
    return node, int(port)


def reset_tt_ioctl(node, port):
    # bmRequestType 0x23: class request to a hub port
    arg = _CTRLTRANSFER.pack(0x23, RESET_TT, 0, port, 0, 1000, 0)
    with open(node, 'wb', buffering=0) as f:
        fcntl.ioctl(f, USBDEVFS_CONTROL, arg)


def reset_tt(busport, speed) -> bool:
    """Reset the device's TT if it has one of its own. False when that could not be
    confirmed: this hub's helper timed out, now or earlier in this process."""
    tt = tt_port(busport, speed)
    if not tt:
        return True
    node, port = tt
    if node in _unconfirmed:
        return False
    from helper import hil_util
    here = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    code = (f'import sys; sys.path.insert(0, {here!r}); from helper import hil_tt; '
            'hil_tt.reset_tt_ioctl(sys.argv[1], int(sys.argv[2]))')
    cmd = [sys.executable, '-c', code, node, str(port)]
    if os.geteuid() != 0:
        cmd = ['sudo', '-n'] + cmd
    try:
        # stderr: run_cmd prints a timeout banner, and usbtest's stdout is its JSON report
        with redirect_stdout(sys.stderr):
            r = hil_util.run_cmd(cmd, timeout=HELPER_TIMEOUT, quiet=True)
    except OSError as e:          # no sudo or python to spawn: an ordinary failure
        print(f'Reset_TT {node} port {port} failed: {e}', file=sys.stderr)
        return True
    if r.returncode == 124:
        _unconfirmed.add(node)
        print(f'Reset_TT {node} port {port}: helper timed out, possible hub-lock contention',
              file=sys.stderr)
        return False
    if r.returncode != 0:
        print(f'Reset_TT {node} port {port} failed: {hil_util.cmd_stdout_text(r.stdout).strip()}',
              file=sys.stderr)
    return True
