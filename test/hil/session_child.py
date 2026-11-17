#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Mechanics shared by the disposable session children hil_test runs under run_cmd (mtp_test.py,
# usbtmc_test.py). Stdlib only, like them: nothing here may pull in the harness, so this is a small
# local twin of agentrc's usb_observe read_sysfs/usb_scan.
import os
import threading

SERIAL_GRACE = 2.0  # s

# attribute path -> (inode, reader thread) of a read given up on. A child lives one bounded session,
# so the threads this keeps blocked are bounded by its lifetime.
_stranded = {}


def write_pidfile(path):
    """Record "<pid> <starttime>" for hil_test._session_alive; called before any device work."""
    # starttime is proc(5) stat field 22: a recycled pid has another
    with open('/proc/self/stat') as f:
        start = f.read().rpartition(')')[2].split()[19]
    with open(path, 'w') as f:
        f.write(f'{os.getpid()} {start}\n')


def enumeration_id(path):
    """The kernfs inode of `path`, None when it is gone: a re-enumeration makes a new node even at the
    same busport and devnum. os.stat never calls ->show(), so it cannot block on a wedged device."""
    try:
        return os.stat(path).st_ino
    except OSError:
        return None


def bounded_read(path, grace=SERIAL_GRACE):
    """A sysfs attribute's text, or None when it is unreadable or does not answer within `grace` s.

    `serial` is served under the device lock a wedged usbfs ioctl holds, and every DUT of one example
    shares its vid:pid, so a scan for one board reads every wedged neighbour's serial. A reader that
    timed out is not joined by a second one on the same node: the node is retried once that reader
    finished or the node re-enumerated (a new inode)."""
    key = str(path)
    ino = enumeration_id(path)  # before the read: a node replaced during the grace must not be the one memoised
    if ino is None:
        return None
    stuck = _stranded.get(key)
    if stuck is not None and stuck[0] == ino and stuck[1].is_alive():
        return None
    _stranded.pop(key, None)
    out = {}

    def _read():
        try:
            with open(path) as f:
                out['v'] = f.read().strip()
        except (OSError, ValueError):
            pass

    t = threading.Thread(target=_read, daemon=True)
    t.start()
    t.join(grace)
    if t.is_alive():
        _stranded[key] = (ino, t)
        return None
    return out.get('v')


def _attr(dev, name):
    with open(os.path.join(dev, name)) as f:
        return f.read().strip()


def device_identity(dev, vid, pid):
    """(serial, busnum, devnum) of sysfs device dir `dev` when it is vid:pid (lowercase hex), else None.

    idVendor/idProduct are lock-free and rule out other devices before the one locked read, `serial`.
    None too when the serial does not answer, or the device re-enumerated across the reads: the serial
    and the address would then belong to different enumerations."""
    ino = enumeration_id(dev)
    if ino is None:
        return None
    try:
        if _attr(dev, 'idVendor') != vid or _attr(dev, 'idProduct') != pid:
            return None
        serial = bounded_read(os.path.join(dev, 'serial'))
        if serial is None:
            return None
        busnum, devnum = int(_attr(dev, 'busnum')), int(_attr(dev, 'devnum'))
    except (OSError, ValueError):
        return None   # left mid-read
    return (serial, busnum, devnum) if enumeration_id(dev) == ino else None


def find_devices(sys_usb, vid, pid, serial):
    """(busnum, devnum) of every vid:pid device under `sys_usb` whose serial is exactly `serial`."""
    try:
        names = sorted(os.listdir(sys_usb))
    except OSError:
        return []
    found = []
    for name in names:
        # root hubs are usbN, not <bus>-<port>; interfaces are <busport>:<cfg>.<ifnum>
        if '-' not in name or ':' in name:
            continue
        ident = device_identity(os.path.join(sys_usb, name), vid, pid)
        if ident is not None and ident[0] == serial:
            found.append(ident[1:])
    return found
