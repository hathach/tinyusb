#!/usr/bin/env python3
"""Run the Linux kernel usbtest/testusb battery against a TinyUSB usbtest device.

Device firmware: examples/device/usbtest (VID:PID cafe:4010). The firmware
advertises its capability tier in bcdDevice low byte; the battery is selected
accordingly (see examples/device/usbtest/README.md).

Requires: usbtest kernel module (CONFIG_USB_TEST), testusb binary (built from
kernel tools/usb/testusb.c), sudo for driver binding + usbfs ioctls.

testusb reporting quirks this script works around:
- its exit code is always 0 when the device exists: results are parsed from stdout
- a case gated off by the driver's capability profile (or an in-kernel parameter
  check) returns -EOPNOTSUPP, which testusb silently skips: a missing result line
  means NOT RUN, and is reported as a failure since every case in the selected
  battery is expected to run.

Binding uses the 5-field new_id form referencing Gadget Zero (0525:a4a0) so the
dynamic id inherits its capability profile (autoconf + ctrl_out + iso + intr).
Never register a plain "vid pid" dynamic id with usbtest: the dynid then has
driver_info == 0 and usbtest_probe() dereferences it without a NULL check
(kernel oops). autoconf is also what enables bulk endpoint discovery; the
capability flags only unlock cases, they don't require the endpoints to exist.
"""

import argparse
import fcntl
import json
import os
import re
import shutil
import subprocess
import sys
import time
from contextlib import redirect_stdout
from pathlib import Path

sys.path.append(os.path.dirname(os.path.abspath(__file__)))  # PYTHONSAFEPATH drops it

VID = 'cafe'
PID = '4010'
GZ_REF = '0525 a4a0'  # copy Gadget Zero's capability profile (ctrl_out+iso+intr)
SYS_USB = Path('/sys/bus/usb/devices')
DRIVER = Path('/sys/bus/usb/drivers/usbtest')
PATTERN_PARAM = Path('/sys/module/usbtest/parameters/pattern')
# In hil_lock's dir, shared by every run on the rig, but not a *.lock: hil_lock treats those
# as boards (status, release --all)
REGISTER_LOCK_NAME = 'usbtest-new_id.registry'
REGISTER_LOCK_TIMEOUT = 30  # a holder only checks and writes new_id, itself bounded at 15 s
RECOVER_FLASH_TIMEOUT = 90  # bound on the post-hang reflash; typical flash is 10-20s
RECOVER_RESET_TIMEOUT = 30  # bound on the post-hang probe reset; jlink ResetTarget ~130ms, stlink --rst --go ~100ms
RECOVER_SETTLE = 5          # after the recovery step, to let a freed ioctl unwind
# How long a HUNG case is watched before it counts as a wedge. HUNG only says the kill was
# not reaped within 5 s; a finite hold on the DUT's device lock looks the same and is reaped
# once it ends. Reserved on every HUNG path, recovery or not.
WEDGE_CONFIRM_S = 30
RECOVER_REAP = 5            # after the settle: a freed testusb reaps within this
# The unbounded work around the recovery step: json.loads of the roster entry, the child's
# first `import hil_flash`, convoy_safe, the BUDGET back-fill and the JSON print. Without
# it the reserve equals its own worst case exactly, and HIL_CMD_TIMEOUT and
# HIL_USBTEST_BATTERY_BUDGET are both env-overridable -- any of them moving up puts
# run_cmd's killpg back inside the reflash, orphaning the flasher on the probe.
RECOVER_OVERHEAD = 40


def recovery_reserve(flasher: dict) -> int:
    """Seconds this flasher's post-hang recovery can spend: the confirmation window, ONE
    bounded step (a probe reset where the flasher has one, else a reflash) plus run_cmd's
    post-SIGKILL reap, the settle and the reap check after it, and the overhead above.
    The caller's outer kill must sit past this or it lands mid-step and orphans the flasher
    on the probe."""
    import hil_flash
    name = (flasher.get('name') or '').lower()
    step = RECOVER_RESET_TIMEOUT if hil_flash.reset_primitive(name) else RECOVER_FLASH_TIMEOUT
    return (WEDGE_CONFIRM_S + step + _hu().REAP_GRACE + RECOVER_SETTLE + RECOVER_REAP
            + RECOVER_OVERHEAD)


HELPER_TIMEOUT = 30         # default bound for sudo helpers (dmesg/modprobe/setpci/tee)

# Battery per tier, in run order: control sanity, simple bulk, queued, unaligned, unlink,
# halt/toggle, throughput last.
TIER_CASES = {
    1: [0, 9, 10, 1, 2, 3, 4, 5, 6, 7, 8, 17, 18, 19, 20, 11, 12, 24, 13, 29, 27, 28],
    2: [14, 21],
    3: [25, 26],
    4: [15, 16, 22, 23],
}

UNLINK_CASES = (11, 12, 24)   # URB unlink mid-transfer: can strand a hub TT buffer (hil_tt)

# Per-case testusb parameters (full speed / high speed). All -s/-v values are multiples
# of 512 so transfers stay packet-aligned at both speeds: the device streams whole max-size
# packets and a non-aligned IN length would babble. 14/21 must never run with defaults
# (vary >= length is -EINVAL in the kernel).
PARAMS = {
    0: ('-c 1', '-c 1'),
    9: ('-c 256', '-c 500'),  # HS 1000 took 56 of 60 s with 5 batteries on a hub; 500 leaves ~2x
    10: ('-c 64 -g 16', '-c 256 -g 16'),
    **{n: ('-c 128 -s 1024 -v 512', '-c 512 -s 1024 -v 512') for n in (1, 2, 3, 4, 17, 18, 19, 20)},
    **{n: ('-c 8 -s 1024 -g 8', '-c 32 -s 1024 -g 16') for n in (5, 6, 7, 8)},
    **{n: ('-c 64 -s 1024 -g 8', '-c 256 -s 1024 -g 8') for n in UNLINK_CASES},
    13: ('-c 16 -s 512', '-c 64 -s 512'),
    29: ('-c 16 -s 512', '-c 64 -s 512'),
    27: ('-c 16 -s 1024 -g 32', '-c 128 -s 1024 -g 32'),
    28: ('-c 16 -s 1024 -g 32', '-c 128 -s 1024 -g 32'),
    14: ('-c 64 -s 512 -v 61', '-c 256 -s 512 -v 61'),
    21: ('-c 64 -s 512 -v 61', '-c 256 -s 512 -v 61'),
    25: ('-c 32 -s 512', '-c 256 -s 1024'),
    26: ('-c 32 -s 512', '-c 256 -s 1024'),
    **{n: ('-c 16 -s 512 -g 8', '-c 64 -s 1024 -g 8') for n in (15, 16, 22, 23)},
}

CASE_NAMES = {
    0: 'NOP', 1: 'bulk write', 2: 'bulk read', 3: 'bulk write vary', 4: 'bulk read vary',
    5: 'bulk sg write', 6: 'bulk sg read', 7: 'bulk sg write vary', 8: 'bulk sg read vary',
    9: 'ch9 subset', 10: 'queued control', 11: 'unlink reads', 12: 'unlink writes',
    13: 'ep halt set/clear', 14: 'ctrl_out write/read', 15: 'iso write', 16: 'iso read',
    17: 'bulk write unaligned', 18: 'bulk read unaligned', 19: 'bulk write premapped',
    20: 'bulk read premapped', 21: 'ctrl_out unaligned', 22: 'iso write unaligned',
    23: 'iso read unaligned', 24: 'unlink queued writes', 25: 'int write', 26: 'int read',
    27: 'bulk write perf', 28: 'bulk read perf', 29: 'toggle clear',
}

RE_PASS = re.compile(r'test (\d+),\s*(\d+)\.(\d+) secs')
RE_FAIL = re.compile(r'test (\d+) --> (\d+) \((.*)\)')


def run(cmd, **kw):
    # NOT subprocess.run(timeout=): CPython's post-timeout path is an UNBOUNDED wait() that
    # never returns on a D-state child -- the hang sysfs_write's timeout exists to catch.
    timeout = kw.pop('timeout', None)
    data = kw.pop('input', None)          # subprocess.run-only kwarg; Popen takes stdin
    kw.pop('capture_output', None)        # ditto: expressed by the PIPEs below
    kw.setdefault('text', True)
    kw.setdefault('encoding', 'utf-8')
    kw.setdefault('errors', 'replace')   # strict decode would raise out of _sudo_soft
    # NO start_new_session: these helpers (dmesg, modprobe, setpci, tee) must stay in our
    # process group so hil_test's outer killpg reaps them with us.
    timeout = timeout if timeout is not None else HELPER_TIMEOUT
    proc = subprocess.Popen(cmd, stdin=subprocess.PIPE if data is not None else None,
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE, **kw)
    try:
        out, err = proc.communicate(input=data, timeout=timeout)
        return subprocess.CompletedProcess(cmd, proc.returncode, out, err)
    except subprocess.TimeoutExpired:
        # Under sudo our child is only the wrapper; the root grandchild survives this and
        # is left for the report and hil_pool_check to name. Close our pipe ends so an
        # abandoned child costs no fds.
        try:
            proc.kill()          # same group as us: never killpg, that would kill us too
        except OSError:
            pass
        try:
            proc.communicate(timeout=5)
        except subprocess.TimeoutExpired:
            for pipe in (proc.stdout, proc.stderr, proc.stdin):
                try:
                    if pipe is not None:
                        pipe.close()
                except OSError:
                    pass          # unkillable: abandon it, the caller reports the timeout
        raise


def sudo(cmd, **kw):
    if os.geteuid() != 0:
        cmd = ['sudo', '-n'] + cmd
    r = run(cmd, **kw)
    if r.returncode != 0 and 'password is required' in (r.stderr or ''):
        sys.exit(f'sudo needs a password for: {" ".join(cmd)}\n'
                 'Run as root, or grant this user NOPASSWD sudo.')
    return r


def sysfs_write(path, data, check=True):
    # A driver-registry write (new_id/remove_id/bind) blocks in D state while a device lock
    # is held: fail fast instead of piling up unkillable writers. A timeout alone does not
    # say which holder: a peer's in-flight testusb case holds its lock for the whole case
    # (usbdev_do_ioctl), a wedged device holds it for good.
    #
    # Verified in v6.12.96: unbind_store -> device_driver_detach ->
    # device_release_driver_internal -> __device_driver_lock (drivers/base/dd.c), which
    # takes device_lock() -- the UNINTERRUPTIBLE variant, unlike the sysfs read path -- and
    # ALSO device_lock(parent), because usb_bus_type sets need_parent_lock = true
    # (drivers/usb/core/driver.c:2048). So one such write against a wedged device blocks
    # unkillably while holding the HUB's lock: that is the mechanism by which a single
    # wedged port takes its whole bus down, and why this fails fast instead.
    try:
        r = sudo(['tee', str(path)], input=data, timeout=15)
    except subprocess.TimeoutExpired:
        sys.exit(f'write "{data}" > {path} blocked >15s: a device lock is held -- possible '
                 'device-lock contention or a wedged device. Check dmesg before any recovery '
                 '(usb-kernel-recover skill).')
    if check and r.returncode != 0:
        sys.exit(f'write "{data}" > {path} failed: {r.stderr.strip()}')
    return r.returncode == 0


def _hu():
    """The helper module, imported lazily like every other helper use in this file."""
    from helper import hil_util
    return hil_util


def _tt():
    from helper import hil_tt
    return hil_tt


SERIAL_GRACE = 1.0   # tighter than hil_util's shared default on purpose: find_device
                     # re-scans every cafe:4010 peer after each of ~30 cases and inside
                     # the 8s startup poll, so N unreadable peers cost N x this per scan


def _read_sysfs(path):
    """The attribute's value, or None. See hil_util.read_sysfs for why `serial` can block."""
    return _hu().read_sysfs(str(path), SERIAL_GRACE)


_DEV_CACHE: dict = {}   # serial -> sysname, see find_device


def _describe(d, serial):
    """The device at sysfs dir `d`, whose `serial` the caller has read and matched."""
    return {
        'sysname': d.name,
        'serial': serial,
        'node': '/dev/bus/usb/%03d/%03d' % (int((d / 'busnum').read_text()),
                                            int((d / 'devnum').read_text())),
        'speed': (d / 'speed').read_text().strip(),
        'tier': int((d / 'bcdDevice').read_text().strip()[-2:], 16),
    }


def _reread(sysname, serial):
    """Re-describe an already-resolved device, CONFIRMING its serial.

    idVendor/idProduct/busnum/devnum/speed are lock-free (sysfs.c:688-705), so they cannot
    block on a wedged peer -- but every identical board answers them the same, so they
    prove nothing about identity. `serial` does, at one bounded read: a sysname is a
    topology path, and after a renumber (controller reset, reboot) it can name a DIFFERENT
    cafe:4010 board whose verdicts would be filed under this one. Returns None when the
    serial is gone, mismatched or unconfirmed -- caller falls back to a full scan.
    """
    d = SYS_USB / sysname
    try:
        if ((d / 'idVendor').read_text().strip() != VID
                or (d / 'idProduct').read_text().strip() != PID):
            return None
        dev_serial = _read_sysfs(d / 'serial')
        if not isinstance(dev_serial, str) or dev_serial.lower() != serial.lower():
            return None      # gone, mismatched, or unconfirmable -> full scan decides
        return _describe(d, dev_serial)
    except (OSError, ValueError):
        return None


def find_device(serial):
    """Locate the usbtest device in sysfs, return info dict or None.

    Cached by serial: this is called after EVERY case, and a full scan pays a bounded
    but real `serial` read for every cafe:4010 peer on the rig. With another board
    wedged that cost lands on a HEALTHY battery ~30 times over, truncating it into
    BUDGET entries. The fast path pays ONE bounded read -- our own device's serial, the
    only attribute that tells identical boards apart (see _reread).
    """
    if serial:
        sysname = _DEV_CACHE.get(serial.lower())
        if sysname:
            hit = _reread(sysname, serial)
            if hit:
                return hit
            _DEV_CACHE.pop(serial.lower(), None)
    matches = []
    for dev in SYS_USB.iterdir():
        try:
            if (dev / 'idVendor').read_text().strip() != VID or \
               (dev / 'idProduct').read_text().strip() != PID:
                continue
            # idVendor/idProduct are cached descriptors; `serial` is served under
            # device_lock(), so on a wedged DUT this read blocks until the wedge clears.
            # Contained by the caller's bound, not prevented here -- see hil_util.read_sysfs.
            dev_serial = _read_sysfs(dev / 'serial')
            if dev_serial is None:
                continue
            if serial and dev_serial.lower() != serial.lower():
                continue
            matches.append(_describe(dev, dev_serial))
        except (OSError, ValueError):
            continue
    if not matches:
        return None
    if serial and len(matches) == 1:
        _DEV_CACHE[serial.lower()] = matches[0]['sysname']
    if len(matches) > 1:
        if serial:
            # Dual-port parts (nanoch32v203, ch32v307) briefly enumerate BOTH ports with
            # one serial around a variant reflash, and picking one could bind the stale
            # port -- report ambiguity so the caller retries until it drops.
            return {'ambiguous': sorted(m['sysname'] for m in matches)}
        sys.exit(f'multiple {VID}:{PID} devices found, use --serial: '
                 + ', '.join(m["serial"] for m in matches))
    return matches[0]


def check_host_compat(dev):
    """Refuse to run when the DUT's upstream host controller is known-incompatible:
    the MosChip MCS9990 (9710:9990) outright (buggy FRINDEX silicon: EHCI never
    schedules int-OUT URBs and mangles unlinked reads - verified A/B 2026-07-09),
    and the Renesas uPD720201/02 unless it runs firmware >= 2.0.2.6 (see below)."""
    for attempt in range(3):
        try:
            root = Path(f"/sys/bus/usb/devices/usb{int(dev['node'].split('/')[-2])}")
            drv = (root / '../driver').resolve().name
            pci = (root / '..').resolve()
            vid_did = ((pci / 'vendor').read_text().strip(), (pci / 'device').read_text().strip())
            break
        except (OSError, ValueError):
            # transient sysfs error (racing a re-enumeration): retry so a blip does not
            # silently pass an incompatible host, then fail open but say so
            if attempt == 2:
                print('warning: cannot probe the upstream host controller; '
                      'skipping the host compatibility check', file=sys.stderr)
                return
            time.sleep(1)
    if vid_did == ('0x9710', '0x9990'):
        sys.exit(f'REFUSING to run: DUT is behind a MosChip MCS9990 ({pci.name}), which is '
                 'incompatible with usbtest: broken FRINDEX silicon - int-OUT URBs are never '
                 'placed in the EHCI periodic schedule and unlinked reads complete as short '
                 'transfers (EREMOTEIO). Move the DUT to an xHCI port.')
    if drv.startswith('xhci') and vid_did in (('0x1912', '0x0014'), ('0x1912', '0x0015')):
        # The Renesas uPD720201/uPD720202 must run firmware >= 2.0.2.6 (K2026090.mem;
        # RAM-uploaded, so it reverts to ROM on every power cycle): on ROM firmware its
        # command ring dies under unlink stress and the hub worker deadlocks holding the
        # device lock, needing a host power cycle (ch32v307 2026-07-10; ra6m5 test 24,
        # mimxrt1015 2026-07-11). Both parts expose the FW version at PCI config 0x6c.
        # Necessary, not sufficient -- batteries have killed the controller on current
        # firmware too, which per-board skips in the rig config handle.
        fw = None
        try:
            r = _sudo_soft(['setpci', '-s', pci.name, '0x6c.l'])
            if r.returncode == 0:
                fw = int(r.stdout.strip(), 16)
        except (OSError, ValueError):
            pass
        if fw is None:
            sys.exit(f'REFUSING to run: cannot read host xHCI Renesas ({pci.name}) firmware '
                     'version (setpci missing or not permitted) - usbtest requires verified '
                     'firmware >= 0x00202609 (2.0.2.6); on older firmware the command ring '
                     'dies under unlink stress. Install pciutils / fix sudo, or load the '
                     'firmware and re-check.')
        if fw < 0x00202609:
            sys.exit(f'REFUSING to run: host xHCI Renesas ({pci.name}) firmware 0x{fw:08x} '
                     '< 0x00202609 (2.0.2.6) - its command ring dies under usbtest unlink '
                     'stress. Load the latest firmware (K2026090.mem; it is RAM-uploaded and '
                     'reverts to ROM on every power cycle).')


def register_usbtest_id():
    """Register cafe:4010 with usbtest: one new_id write per module load, before any battery.

    Every new_id write runs driver_attach, which takes the device lock of every cafe:4010
    interface on the rig; a peer's testusb case holds its lock for the whole case, so a write
    beside running batteries waits for the slowest case and trips sysfs_write's bound. The
    kernel keeps no duplicate check and lists the id before driver_attach runs, so the check
    and the write share one flock: two initializers make one write, and neither returns
    while the other's attach is in flight. A wrong entry is repaired by a module reload
    (usbtest skill, "Repair a wrong profile"), never by remove_id + this."""
    if not DRIVER.exists():
        r = _sudo_soft(['modprobe', 'usbtest'])
        if r.returncode != 0 or not DRIVER.exists():
            sys.exit(f'cannot load usbtest module: {r.stderr.strip()}')
    from helper import hil_lock
    path = os.path.join(hil_lock.BOARD_LOCK_DIR, REGISTER_LOCK_NAME)
    try:
        os.makedirs(hil_lock.BOARD_LOCK_DIR, exist_ok=True)
        fh = open(path, 'a')
    except OSError as e:
        sys.exit(f'usbtest id registration lock {path}: {e.strerror}')
    with fh:
        deadline = time.monotonic() + REGISTER_LOCK_TIMEOUT
        while True:
            try:
                fcntl.flock(fh, fcntl.LOCK_EX | fcntl.LOCK_NB)
                break
            except BlockingIOError:
                if time.monotonic() > deadline:
                    sys.exit(f'usbtest id registration lock {path} held >{REGISTER_LOCK_TIMEOUT}s')
                time.sleep(0.1)
            except OSError as e:
                sys.exit(f'usbtest id registration lock {path}: {e.strerror}')
        try:
            listed = (DRIVER / 'new_id').read_text().splitlines()
        except OSError as e:
            sys.exit(f'cannot read {DRIVER / "new_id"}: {e.strerror}')
        if f'{VID} {PID}' not in listed:
            sysfs_write(DRIVER / 'new_id', f'{VID} {PID} 0 {GZ_REF}')


def bind_usbtest(dev):
    """Bind the device's interface 0 to usbtest. The id is registered already
    (register_usbtest_id), so this writes only this device's own bind/unbind."""
    intf = f'{dev["sysname"]}:1.0'
    deadline = time.monotonic() + 3
    while time.monotonic() < deadline:
        drv = SYS_USB / intf / 'driver'
        if drv.is_symlink():
            if drv.resolve().name == 'usbtest':
                return
            # claimed by a foreign driver: steal the interface
            sysfs_write(drv / 'unbind', intf)
        sysfs_write(DRIVER / 'bind', intf, check=False)
        time.sleep(0.2)
    sys.exit(f'interface {intf} did not bind to usbtest')


def set_pattern(value):
    try:
        if PATTERN_PARAM.read_text().strip() != str(value):
            sysfs_write(PATTERN_PARAM, str(value))
    except OSError as e:  # FileNotFoundError (no param), PermissionError (root-only), ...
        sys.exit(f'{PATTERN_PARAM} not usable ({e.strerror}): this usbtest module build may lack '
                 'the "pattern" param, or it is not readable')


def _sudo_soft(cmd, **kw):
    """sudo() for calls whose failure must never abort the battery: run() re-raises
    TimeoutExpired, and two of these are evaluated inside run_case's own timeout handler
    -- a raise there loses the HUNG verdict, the recovery and the JSON report."""
    try:
        return sudo(cmd, **kw)
    except (OSError, ValueError, subprocess.SubprocessError, SystemExit) as e:
        # SystemExit too: sudo() sys.exit()s on 'a password is required', unwinding out of
        # run_case's timeout handler before the HUNG verdict is recorded -- which leaves
        # unrecovered_hang False and reports a board still holding a D-state device lock as
        # not wedged
        why = f'{cmd[0]}: {type(e).__name__}: {e}'
        print(why, file=sys.stderr)
        return subprocess.CompletedProcess(cmd, 1, '', why)


def dmesg_tail():
    r = _sudo_soft(['dmesg'])
    lines = [l for l in r.stdout.splitlines() if 'usbtest' in l]
    return '\n'.join(lines[-8:])


sudo_forbidden = False   # set by main when a post-hang recovery was requested


def needs_sudo(node):
    """Device nodes are usually opened directly (udev rule); sudo only if not."""
    return not os.access(node, os.W_OK) and os.geteuid() != 0


def run_case(num, dev, testusb, quick, timeout):
    fs_hs = PARAMS[num][0 if dev['speed'] == '12' else 1]
    if quick:
        fs_hs = re.sub(r'-c (\d+)', lambda m: f'-c {max(1, int(m.group(1)) // 8)}', fs_hs)
    # -A <node> confines testusb's ftw() device scan to the DUT: with -D alone it opens every
    # usbfs node, blocking on any peer's held device lock (#4047). -A must precede -D, it clears it.
    cmd = [testusb, '-A', dev['node'], '-D', dev['node'], '-t', str(num)] + fs_hs.split()
    result = {'num': num, 'name': CASE_NAMES[num], 'params': fs_hs}
    if needs_sudo(dev['node']):
        if sudo_forbidden:
            # a re-enumeration after main's check can hand back a node only root may open
            result.update(status='FAIL', detail=f"{dev['node']} is not writable: refusing a "
                          'sudo-wrapped testusb under a recovery request')
            return result
        cmd = ['sudo', '-n'] + cmd

    # NO start_new_session: testusb must stay in OUR process group so the caller's outer
    # killpg still reaps it.
    # errors='replace': testusb output is not guaranteed UTF-8, and a strict decode would
    # raise out of here and out of main(), printing no JSON at all (battery '0/30').
    p = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                         text=True, encoding='utf-8', errors='replace')
    try:
        out, _ = p.communicate(timeout=timeout)
    except subprocess.TimeoutExpired:
        # Under sudo we only kill the wrapper; its root-owned testusb keeps the inherited
        # stdout pipe, so the reap below times out and the overrun is reported as HUNG.
        # Accepted rather than escalated: the rig's udev rules make the device node
        # writable, so sudo is the exception, and the harness must never sudo-kill a pid
        # it cannot prove is its own.
        try:
            p.kill()
        except OSError:
            pass
        try:
            out, _ = p.communicate(timeout=5)
        except subprocess.TimeoutExpired:
            # SIGKILL had no effect: the child is in uninterruptible sleep on the DUT's
            # device lock or an in-kernel usbfs ioctl. A finite hold is reaped once it ends,
            # so watch the child for WEDGE_CONFIRM_S before calling it a wedge (#3944). Under
            # sudo the child is the wrapper and reaps at once, which proves nothing.
            if cmd[0] != 'sudo':
                t0 = time.monotonic()
                try:
                    p.wait(timeout=WEDGE_CONFIRM_S)
                    waited = time.monotonic() - t0
                    result.update(status='FAIL', dmesg=dmesg_tail(), late_cleared=True,
                                  detail=f'timeout after {timeout}s (the kill landed '
                                         f'{waited:.0f}s late; testusb reaped during the '
                                         f'confirmation watch)')
                    return result
                except subprocess.TimeoutExpired:
                    pass
            result.update(status='HUNG', detail=f'testusb stuck in D state after {timeout}s',
                          dmesg=dmesg_tail())
            # not JSON: main() watches the child after the recovery step (sudo: cannot)
            result['_proc'] = None if cmd[0] == 'sudo' else p
            return result
        result.update(status='FAIL', detail=f'timeout after {timeout}s', dmesg=dmesg_tail())
        return result

    m = RE_PASS.search(out)
    if m and int(m.group(1)) == num:
        secs = float(f'{m.group(2)}.{m.group(3)}')
        result.update(status='PASS', secs=secs)
        if num in (27, 28) and secs > 0:
            opts = dict(zip(fs_hs.split()[::2], fs_hs.split()[1::2]))
            total = int(opts['-c']) * int(opts['-s']) * int(opts['-g'])
            result['mbps'] = round(total / secs / 1e6, 2)
        return result

    m = RE_FAIL.search(out)
    if m and int(m.group(1)) == num:
        result.update(status='FAIL', detail=f'errno {m.group(2)} ({m.group(3)})',
                      dmesg=dmesg_tail())
        return result

    if cmd[0] == 'sudo' and ('password is required' in out or 'a terminal is required' in out):
        result.update(status='FAIL', detail='sudo needs a password to run testusb: the device node '
                      'is not writable')
        return result

    # testusb prints '<speed> speed\t<node>\t<ifnum>' once it has opened the node
    # (tools/usb/testusb.c handle_testdev). Without it testusb never reached the ioctl: an
    # open failure, a usage error. Not NOTRUN, which sends the reader to the binding profile.
    if not re.search(rf'^\S+ speed\t{re.escape(dev["node"])}\t\d+$', out, re.M):
        result.update(status='FAIL', detail=f'testusb did not run the case (rc {p.returncode})',
                      stderr=out.strip())
        return result

    # no result line: the kernel returned -EOPNOTSUPP (capability profile or
    # in-kernel parameter gate) and testusb skipped silently
    result.update(status='NOTRUN', detail='case gated off: check binding profile/pattern',
                  stderr=out.strip())
    return result


def recover_hang(board_json, fw, proc, dev):
    """The ONE bounded post-hang recovery step, which recovery_reserve budgets: a probe reset
    where the flasher has one -- it fails the in-flight URB at the source so the ioctl returns
    and the queued kill lands, is non-destructive and ~130 ms -- else a reflash of the firmware
    under test (esptool). Never a root-port cycle: that bounces every fixture under the port
    and cannot remove power anyway (usb-kernel-recover). True only when the killed testusb
    reaped: a clean reset or flash proves the probe reached the MCU, not that the D-state
    holder let go."""
    if not (board_json and fw):
        print('no --recover-board/--recover-fw: the device stays wedged', file=sys.stderr)
        return False
    try:
        board = json.loads(board_json)
        bname, fname = board['name'], board['flasher']['name']
        import hil_flash   # deferred: stdlib-only unless recovery actually runs
        flash_fn = hil_flash.flash_primitive(fname)
    except Exception as e:   # malformed/short json, import failure, unknown flasher
        print(f'recovery unavailable ({e})', file=sys.stderr)
        return False
    # our own testusb is D-state on this DUT's node, so a flasher that enumerates by OPENING
    # usbfs nodes blocks on it, survives SIGKILL and becomes a second stray
    if not hil_flash.convoy_safe(board['flasher']):
        print(f'{fname} is not convoy-safe for delivery (it enumerates by '
              f'opening usbfs nodes, and this DUT has a D-state holder on '
              f'its own node): skipping the recovery rather than adding a '
              f'second stray. Pin the roster entry with vid_pid on an '
              f'openocd flasher to enable recovery for this board.',
              file=sys.stderr)
        return False
    reset_fn = hil_flash.reset_primitive(fname)
    # flasher banners must stay off stdout, which carries the --json result; a raising
    # flasher must not cost the battery its JSON report
    try:
        with redirect_stdout(sys.stderr):
            if reset_fn:
                print(f'auto-recovering: resetting {bname} via {fname} probe', file=sys.stderr)
                ret = reset_fn(board, timeout=RECOVER_RESET_TIMEOUT)
                print(f'reset rc {ret.returncode}', file=sys.stderr)
            else:
                print(f'auto-recovering: reflashing {bname} via {fname}', file=sys.stderr)
                ret = flash_fn(board, fw, timeout=RECOVER_FLASH_TIMEOUT)
                if ret.returncode != 0:
                    print(f'reflash failed (rc {ret.returncode})', file=sys.stderr)
    except Exception as e:
        print(f'recovery step raised: {e}', file=sys.stderr)
    time.sleep(RECOVER_SETTLE)     # let the freed ioctl unwind
    if proc is None:
        print('cannot confirm the recovery: testusb ran under sudo, so the '
              'killed child is the wrapper', file=sys.stderr)
        return False
    try:
        proc.wait(timeout=RECOVER_REAP)
    except subprocess.TimeoutExpired:
        print(f'{dev["sysname"]}: testusb still in D state on {dev["node"]} '
              f'-- the device lock was never released', file=sys.stderr)
        return False
    print('recovery freed the device: testusb reaped', file=sys.stderr)
    return True


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('--serial', help='board uid (USB serial string) to select the device')
    p.add_argument('--tier', type=int, choices=sorted(TIER_CASES),
                   help='override tier (default: from device bcdDevice)')
    p.add_argument('--tests', help='comma-separated case numbers, overrides tier battery')
    p.add_argument('--quick', action='store_true', help='divide iteration counts by 8')
    p.add_argument('--json', action='store_true', help='machine-readable output on stdout')
    p.add_argument('--testusb', default=None, help='path to testusb binary')
    p.add_argument('--timeout', type=int, default=120, help='per-case timeout in seconds')
    p.add_argument('--recover-board', help='board JSON (name + flasher) for the post-hang '
                   'recovery (a probe reset, else a reflash); without it a HUNG case leaves '
                   'the device wedged')
    p.add_argument('--recover-fw', help='firmware path the post-hang recovery reflashes when '
                   'the flasher has no reset primitive')
    p.add_argument('--budget', type=int, default=0,
                   help='stop starting new cases after this many seconds (0 = no limit). '
                        'Callers that impose their own outer bound set this BELOW it, '
                        'reserving the remainder for the post-hang recovery -- see '
                        'recovery_reserve() for what that step costs')
    args = p.parse_args()
    t_start = time.monotonic()
    sys.stdout.reconfigure(line_buffering=True)  # per-case results visible when piped/logged

    testusb = args.testusb or shutil.which('testusb') or os.path.expanduser('~/testusb')
    if not os.access(testusb, os.X_OK):
        sys.exit('testusb binary not found: build kernel tools/usb/testusb.c '
                 'and install it, or pass --testusb')

    # retry briefly: after a flash the enumeration may still be settling, and a dual-port
    # part's stale same-serial node takes a moment to drop off (see find_device)
    deadline = time.monotonic() + 8
    while True:
        dev = find_device(args.serial)
        if dev and 'ambiguous' not in dev:
            break
        if time.monotonic() > deadline:
            if dev and 'ambiguous' in dev:
                sys.exit(f"multiple devices with serial {args.serial}: {', '.join(dev['ambiguous'])} "
                         '— stale enumeration from another port? replug or retry')
            sys.exit(f'no {VID}:{PID} device'
                     + (f' with serial {args.serial}' if args.serial else ''))
        time.sleep(0.5)

    # a stale/foreign device advertising an out-of-range tier must not silently run an
    # empty battery ('0/0 passed' would read as green in CI)
    tier = args.tier or dev['tier']
    if not 1 <= tier <= max(TIER_CASES):
        sys.exit(f"device advertises tier {tier} (bcdDevice ...{tier:02x}); reflash a usbtest build "
                 f"or pass --tier 1..{max(TIER_CASES)} — refusing to run an unknown/empty battery")
    if args.tests:
        cases = []
        for tok in args.tests.split(','):
            tok = tok.strip()
            if not tok.isdecimal() or int(tok) not in PARAMS:  # isdecimal rejects unicode digits
                sys.exit(f'--tests: {tok!r} is not a known case number (valid 0..{max(PARAMS)})')
            # results are keyed by case number: an unrun duplicate would vanish from the report
            if int(tok) in cases:
                sys.exit(f'--tests: case {int(tok)} is listed twice')
            cases.append(int(tok))
    else:
        cases = [n for t in range(1, tier + 1) for n in TIER_CASES[t]]

    info = f"device {dev['serial']} {dev['node']} speed={dev['speed']} tier={tier}"
    if not args.json:
        print(info)

    # before touching the device: an incompatible host exits here, before any bind
    check_host_compat(dev)
    # run_case would wrap testusb in sudo, where a HUNG case's reap only proves the wrapper
    # exited, so the recovery the caller asked for could never be confirmed
    global sudo_forbidden
    sudo_forbidden = bool(args.recover_board)
    if sudo_forbidden and needs_sudo(dev['node']):
        sys.exit(f"{dev['node']} is not writable: testusb would run under sudo, where a "
                 'post-hang recovery cannot be confirmed; install tools/88-tinyusb.rules')

    register_usbtest_id()
    results = []
    unrecovered_hang = False
    try:
        bind_usbtest(dev)
        set_pattern(0)  # tier 1 firmware sources zeros; also required by perf cases 27/28

        abort_reason = None   # set on any early exit; drives the BUDGET back-fill below
        for num in cases:
            # An ordinary case timeout is a FAIL and the loop continues, each burning
            # --timeout+5s, so without this the run can still be in the case loop when the
            # outer timeout SIGKILLs it before it emits JSON. Checked before dispatch: worst
            # overshoot is one case.
            if args.budget and time.monotonic() - t_start > args.budget:
                abort_reason = f'battery budget {args.budget}s exhausted'
                break
            results.append(run_case(num, dev, testusb, args.quick, args.timeout))
            r = results[-1]
            if not args.json:
                extra = f" {r.get('secs', '')}s" if r['status'] == 'PASS' else f" {r.get('detail', '')}"
                extra += f" {r['mbps']} MB/s" if 'mbps' in r else ''
                print(f"test {num:2d} {r['name']:22s} {r['status']:6s}{extra}")
            if r.get('late_cleared'):
                # something held the DUT's device lock past the kill: the rest of this
                # battery could stall the same way, and a re-run costs less than the guesswork
                abort_reason = f'battery ended on a late-cleared timeout in case {num}'
                print(f'case {num} timed out; the kill landed late, not a wedge', file=sys.stderr)
                break
            if r['status'] == 'HUNG':
                # Set before the recovery: an exception there reaches the finally with it set.
                unrecovered_hang = True
                abort_reason = 'battery aborted on a kernel-side hang'
                print('aborting battery: kernel-side hang, device wedged mid-transfer',
                      file=sys.stderr)
                unrecovered_hang = not recover_hang(args.recover_board, args.recover_fw,
                                                    r.pop('_proc'), dev)
                break
            if (num in UNLINK_CASES or r['status'] != 'PASS') and \
                    not _tt().reset_tt(dev['sysname'], dev['speed']):
                # unconfirmed (hil_tt.reset_tt): the rest may stall on the same port. Not a
                # wedge of this device -- a sibling port's enumeration can hold the hub.
                abort_reason = f'Reset_TT after case {num} unconfirmed'
                if r['status'] == 'PASS':   # the battery must not read as a clean pass
                    r.update(status='FAIL', detail=abort_reason)
                break
            # re-resolve: a re-enumeration changes the node path. The concrete serial, not
            # args.serial (may be None), so this never retargets another cafe:4010 device.
            live = find_device(dev['serial'])
            if live and live.get('ambiguous'):
                # the dual-port WCH parts answer one serial on two nodes around a
                # re-enumeration: stop and latch board_wedged rather than file the rest
                # under a device we cannot identify
                abort_reason = (f'serial {dev["serial"]} matches more than one device '
                                f'({", ".join(live["ambiguous"])}) after case {num}')
                unrecovered_hang = True
                break
            if not live:
                # a serial read that gave up looks like a disconnect but is probably a
                # wedge: fail closed and latch board_wedged
                if _hu().path_stranded(str(SYS_USB / dev['sysname'] / 'serial')):
                    abort_reason = (f'cannot tell whether the device is still present '
                                    f'after case {num}: its serial read gave up')
                    unrecovered_hang = True
                    break
                abort_reason = f'device dropped off the bus after case {num}'
                # after the LAST case nothing is back-filled, so the run would read as a clean
                # pass; a FAIL/NOTRUN keeps its own errno and dmesg
                if num == cases[-1] and r['status'] == 'PASS':
                    r.update(status='FAIL', detail=abort_reason)
                break
            dev = live
        if abort_reason:
            # one BUDGET entry per case never dispatched: a shrunken denominator (4/5, not
            # 4/30) hides that most of the battery never ran
            ran = {r['num'] for r in results}
            results += [{'num': n, 'status': 'BUDGET', 'detail': f'not run: {abort_reason}'}
                        for n in cases if n not in ran]
    finally:
        # Nothing is written: remove_id/unbind take the uninterruptible device_lock (see
        # register_usbtest_id; the unbind path has wedged a host xHCI), and cafe:4010 is
        # usbtest's own PID, so a binding left behind claims nothing else.
        if unrecovered_hang and any(r['status'] == 'HUNG' for r in results):
            # testusb still holds the device lock in a usbfs ioctl (see usb-kernel-recover)
            print('unrecovered hang: the device keeps its usbfs lock until the DUT is reset '
                  'or the host is power-cycled (usb-kernel-recover skill)', file=sys.stderr)
        elif unrecovered_hang:
            # the ambiguous/unreadable-serial aborts: no case hung, so no held lock is known
            print(f'reported wedged: {abort_reason}', file=sys.stderr)

    # BUDGET, not NOTRUN: NOTRUN is a case the kernel gated off, a real result that stays in
    # `failed`; counting never-run cases as failures sends a maintainer bisecting them
    notrun = [r for r in results if r['status'] == 'BUDGET']
    failed = [r for r in results if r['status'] not in ('PASS', 'BUDGET')]
    ran = len(results)
    if args.json:
        # `wedged` also covers the aborts with no HUNG case, which the caller cannot infer
        print(json.dumps({'serial': dev['serial'], 'speed': dev['speed'], 'tier': tier,
                          'passed': ran - len(failed) - len(notrun),
                          'failed': len(failed), 'notrun': len(notrun),
                          'wedged': bool(unrecovered_hang),
                          'cases': results}, indent=2))
    else:
        print(f"{ran - len(failed) - len(notrun)}/{ran} passed"
              + (f", {len(notrun)} not run" if notrun else ""))
        for r in failed:
            print(f"  FAILED test {r['num']}: {r.get('detail', '')}")
            if r.get('dmesg'):
                print('    ' + r['dmesg'].replace('\n', '\n    '))
    if failed:
        print('diagnose failed cases with the usbtest skill, "Failed case" '
              '(.claude/skills/usbtest/SKILL.md)', file=sys.stderr)
    # BUDGET entries and a wedged device after a passing last case are not a success either
    return len(failed) + len(notrun) or int(unrecovered_hang)


if __name__ == '__main__':
    sys.exit(main())
