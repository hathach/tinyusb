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
import json
from contextlib import redirect_stdout
import os
import pathlib
import re
import shutil
import subprocess
import sys
import time
from pathlib import Path

sys.path.append(os.path.dirname(os.path.abspath(__file__)))  # PYTHONSAFEPATH drops it

VID = 'cafe'
PID = '4010'
GZ_REF = '0525 a4a0'  # copy Gadget Zero's capability profile (ctrl_out+iso+intr)
SYS_USB = Path('/sys/bus/usb/devices')
DRIVER = Path('/sys/bus/usb/drivers/usbtest')
PATTERN_PARAM = Path('/sys/module/usbtest/parameters/pattern')
RECOVER_FLASH_TIMEOUT = 90  # bound on the post-hang reflash; typical flash is 10-20s
RECOVER_RESET_TIMEOUT = 30  # bound on the post-hang probe reset; ResetTarget measures ~130ms


def recovery_steps(flasher_name: str, time_left: float) -> list:
    """Ordered (kind, bound) recovery attempts that fit in `time_left`.

    RESET FIRST, reflash second. A probe reset fails the in-flight URB at the source just
    as a park-flash does, but it is non-destructive -- the firmware under test survives, so
    the wedge can still be autopsied -- writes no flash, and cannot brick SWD the way a bad
    park image has on mimxrt1064_evk and max32666fthr (survived a power cycle). Measured
    128-129 ms against a full erase+program, and it works on i.MX RT and on DWC2 alike
    (stm32f407disco, 2026-08-16: `r; g` -> USB disconnect, re-enumerated 325 ms later).

    The reset also fits budgets a reflash does not: the old gate skipped recovery entirely
    when RECOVER_FLASH_TIMEOUT did not fit, which left the holder in place for the next
    job. Whether either worked is decided by wedged_pids(), never by the exit code -- a
    clean flash only proves the probe wrote the MCU.
    """
    import hil_flash
    steps = []
    reset_fn = getattr(hil_flash, f'reset_{flasher_name.lower()}', None)
    if getattr(reset_fn, 'no_op', False):
        reset_fn = None        # a stub that returns rc 0 without resetting: do not claim it
    if reset_fn and time_left >= RECOVER_RESET_TIMEOUT:
        steps.append(('reset', RECOVER_RESET_TIMEOUT))
    if time_left >= RECOVER_FLASH_TIMEOUT:
        steps.append(('flash', RECOVER_FLASH_TIMEOUT))
    return steps
HELPER_TIMEOUT = 30         # default bound for sudo helpers (dmesg/modprobe/setpci/tee)

# Battery per tier, in run order: control sanity, simple bulk, queued, unaligned, unlink,
# halt/toggle, throughput last.
TIER_CASES = {
    1: [0, 9, 10, 1, 2, 3, 4, 5, 6, 7, 8, 17, 18, 19, 20, 11, 12, 24, 13, 29, 27, 28],
    2: [14, 21],
    3: [25, 26],
    4: [15, 16, 22, 23],
}

# Per-case testusb parameters (full speed / high speed). All -s/-v values are multiples
# of 512 so transfers stay packet-aligned at both speeds: the device streams whole max-size
# packets and a non-aligned IN length would babble. 14/21 must never run with defaults
# (vary >= length is -EINVAL in the kernel).
PARAMS = {
    0: ('-c 1', '-c 1'),
    9: ('-c 256', '-c 1000'),
    10: ('-c 64 -g 16', '-c 256 -g 16'),
    **{n: ('-c 128 -s 1024 -v 512', '-c 512 -s 1024 -v 512') for n in (1, 2, 3, 4, 17, 18, 19, 20)},
    **{n: ('-c 8 -s 1024 -g 8', '-c 32 -s 1024 -g 16') for n in (5, 6, 7, 8)},
    **{n: ('-c 64 -s 1024 -g 8', '-c 256 -s 1024 -g 8') for n in (11, 12, 24)},
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
    # A driver-registry write (new_id/remove_id/bind) blocks in D state when a wedged
    # device holds its lock: fail fast instead of piling up unkillable writers -- the rig
    # needs USB recovery first.
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
        sys.exit(f'write "{data}" > {path} blocked >15s: USB subsystem is wedged '
                 '(a D-state device lock exists). Recover the rig (usb-kernel-recover skill) '
                 'before running batteries.')
    if check and r.returncode != 0:
        sys.exit(f'write "{data}" > {path} failed: {r.stderr.strip()}')
    return r.returncode == 0


def _read_sysfs_bounded(path, grace=1.0):
    """Bounded sysfs attribute read. The value, or None, or hil_util.SYSFS_UNKNOWN.

    Delegates to hil_util.read_sysfs (imported here, like every helper import in this
    file) so both properties hold: the strand cap -- find_device re-scans after EVERY
    case, so a 30-case battery against a wedged peer would otherwise strand dozens of
    threads and fds -- and UNKNOWN kept distinct from None. Folding UNKNOWN into None made
    a blinded scan read as "device dropped off the bus", which aborts down a path that
    skips the HUNG recovery entirely.
    """
    from helper import hil_util
    return hil_util.read_sysfs(str(path), grace)


_DEV_CACHE: dict = {}   # serial -> sysname, see find_device


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
        dev_serial = _read_sysfs_bounded(d / 'serial')
        if not isinstance(dev_serial, str) or dev_serial.lower() != serial.lower():
            return None      # gone, mismatched, or unconfirmable -> full scan decides
        return {
            'sysname': sysname,
            'serial': dev_serial,
            'node': '/dev/bus/usb/%03d/%03d' % (int((d / 'busnum').read_text()),
                                                int((d / 'devnum').read_text())),
            'speed': (d / 'speed').read_text().strip(),
            'tier': int((d / 'bcdDevice').read_text().strip()[-2:], 16),
        }
    except (OSError, ValueError):
        return None


def find_device(serial, first=False):
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
    matches, inconclusive = [], []
    for dev in SYS_USB.iterdir():
        try:
            if (dev / 'idVendor').read_text().strip() != VID or \
               (dev / 'idProduct').read_text().strip() != PID:
                continue
            # BOUNDED: idVendor/idProduct are cached descriptors, but `serial` is served
            # under device_lock(), so an unbounded read blocks us in D state on exactly the
            # DUT whose hang we are here to report, losing every verdict collected so far.
            dev_serial = _read_sysfs_bounded(dev / 'serial')
            if dev_serial is not None and not isinstance(dev_serial, str):
                inconclusive.append(dev.name)   # unknown: NOT proof it is not ours
                continue
            if dev_serial is None:
                continue
            if serial and dev_serial.lower() != serial.lower():
                continue
            matches.append({
                'sysname': dev.name,
                'serial': dev_serial,
                'node': '/dev/bus/usb/%03d/%03d' % (int((dev / 'busnum').read_text()),
                                                    int((dev / 'devnum').read_text())),
                'speed': (dev / 'speed').read_text().strip(),
                'tier': int((dev / 'bcdDevice').read_text().strip()[-2:], 16),
            })
        except (OSError, ValueError):
            continue
    if not matches:
        # "could not tell" is not "gone". The caller aborts the battery on a falsy return
        # and that path skips the HUNG reflash, so a blinded scan would report the wedge
        # we exist to recover from as a physical disconnect.
        return {'inconclusive': inconclusive} if inconclusive else None
    if serial and len(matches) == 1:
        _DEV_CACHE[serial.lower()] = matches[0]['sysname']
    if len(matches) > 1 and not first:
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
            r = _sudo_soft(['setpci', '-s', pci.name, '0x6c.l'], capture_output=True, text=True)
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


def bind_usbtest(dev):
    """Bind the device's interface 0 to the usbtest driver."""
    if not DRIVER.exists():
        r = _sudo_soft(['modprobe', 'usbtest'])
        if r.returncode != 0 or not DRIVER.exists():
            sys.exit(f'cannot load usbtest module: {r.stderr.strip()}')

    # always re-register in case a stale dynamic id carries a different profile
    intf = f'{dev["sysname"]}:1.0'
    drv = SYS_USB / intf / 'driver'
    stale_binding = drv.is_symlink() and drv.resolve().name == 'usbtest'
    sysfs_write(DRIVER / 'remove_id', f'{VID} {PID}', check=False)
    sysfs_write(DRIVER / 'new_id', f'{VID} {PID} 0 {GZ_REF}')
    if stale_binding:
        # it probed against the OLD dynamic id's capability profile; unbind once (the
        # device is idle here) so the loop below reprobes the fresh one
        sysfs_write(drv / 'unbind', intf, check=False)

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
        # unrecovered_hang False and lets the finally run the remove_id/unbind that must
        # never happen while a D-state device lock is held
        print(f'{cmd[0]}: {type(e).__name__}: {e}', file=sys.stderr)
        return subprocess.CompletedProcess(cmd, 1, '', '')


def dmesg_tail():
    r = _sudo_soft(['dmesg'])
    lines = [l for l in r.stdout.splitlines() if 'usbtest' in l]
    return '\n'.join(lines[-8:])




def wedged_pids(devnode):
    """(pids, complete): pids still in D state on `devnode` after a recovery reflash.

    Matched by device node rather than by our child's pid because run_case() may wrap
    testusb in sudo: the Popen pid is then the wrapper and the blocked process is its
    child. A clean flash only proves the probe wrote the MCU, not that the D-state holder
    let go -- this is what tells the two apart.

    FAIL CLOSED. `complete` is False when an entry could be HIDDEN from us, and the caller
    must then keep treating the hang as unrecovered: the holder is root-owned (run_case
    uses `sudo -n` whenever the node is not writable) and a hidepid/ProtectProc mount
    hides exactly that entry. Reporting "no holder" from a scan that could not see it
    clears unrecovered_hang and lets cleanup run remove_id/unbind against a device whose
    usbfs lock is still held -- which deadlocks the bus, not just this board.

    Self-contained: /proc is plain text and this is one pass over it, so importing a
    helper to do it would only add a failure mode on the recovery path.
    """
    stuck, complete = [], True
    # A restricted /proc hides other users' entries ENTIRELY -- no entry, so no
    # PermissionError to catch -- and testusb runs under sudo, so the holder is exactly
    # what is hidden. Detect the restriction itself rather than its symptom.
    if os.geteuid() != 0 and not os.access('/proc/1/cmdline', os.R_OK):
        complete = False
    for d in pathlib.Path('/proc').glob('[0-9]*'):
        try:
            st = (d / 'stat').read_bytes()
            if st[st.rindex(b')') + 2:st.rindex(b')') + 3] != b'D':
                continue
            if devnode.encode() in (d / 'cmdline').read_bytes():
                stuck.append(int(d.name))
        except PermissionError:
            complete = False      # cannot rule this pid out
        except (OSError, ValueError):
            continue              # raced with exit
    return stuck, complete


def run_case(num, dev, testusb, quick, timeout):
    fs_hs = PARAMS[num][0 if dev['speed'] == '12' else 1]
    if quick:
        fs_hs = re.sub(r'-c (\d+)', lambda m: f'-c {max(1, int(m.group(1)) // 8)}', fs_hs)
    cmd = [testusb, '-D', dev['node'], '-t', str(num)] + fs_hs.split()
    # device nodes are usually opened directly (udev rule); sudo only if not
    if not os.access(dev['node'], os.W_OK) and os.geteuid() != 0:
        cmd = ['sudo', '-n'] + cmd
    result = {'num': num, 'name': CASE_NAMES[num], 'params': fs_hs}

    # NO start_new_session: testusb must stay in OUR process group so the caller's outer
    # killpg still reaps it; a sudo-wrapped child is escalated through sudo below instead.
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
            # SIGKILL had no effect: the child is in uninterruptible sleep on an in-kernel
            # usbfs ioctl. Abandon it — waiting or re-signalling can never succeed.
            result.update(status='HUNG', detail=f'testusb stuck in D state after {timeout}s',
                          dmesg=dmesg_tail())
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

    # no result line: the kernel returned -EOPNOTSUPP (capability profile or
    # in-kernel parameter gate) and testusb skipped silently
    result.update(status='NOTRUN', detail='case gated off: check binding profile/pattern',
                  stderr=out.strip())
    return result


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('--serial', help='board uid (USB serial string) to select the device')
    p.add_argument('--tier', type=int, choices=sorted(TIER_CASES),
                   help='override tier (default: from device bcdDevice)')
    p.add_argument('--tests', help='comma-separated case numbers, overrides tier battery')
    p.add_argument('--quick', action='store_true', help='divide iteration counts by 8')
    p.add_argument('--json', action='store_true', help='machine-readable output on stdout')
    p.add_argument('--keep-binding', action='store_true', help='leave usbtest dynamic id registered')
    p.add_argument('--testusb', default=None, help='path to testusb binary')
    p.add_argument('--timeout', type=int, default=120, help='per-case timeout in seconds')
    p.add_argument('--recover-board', help='board JSON (name + flasher) for the post-hang '
                   'reflash recovery; without it a HUNG case leaves the device wedged')
    p.add_argument('--recover-fw', help='firmware path reflashed by the post-hang recovery')
    p.add_argument('--outer-timeout', type=int, default=0,
                   help='the caller\'s total bound on this process; a reflash that cannot '
                        'finish before it is skipped rather than orphaned mid-flash')
    p.add_argument('--budget', type=int, default=0,
                   help='stop starting new cases after this many seconds (0 = no limit). '
                        'Callers that impose their own outer timeout set this to reserve '
                        'the remainder for the post-hang recovery path')
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
        # find_device is THREE-valued: a device, {'ambiguous': [...]}, or
        # {'inconclusive': [...]} when bounded reads could not rule a device out. Screening
        # only for 'ambiguous' let the inconclusive marker through as if it were a device,
        # and the next statement subscripts dev['tier'] -> KeyError, no JSON on stdout, and
        # hil_test reports "usbtest did not run / 0-30" for a merely-unreadable bus.
        if dev and not ({'ambiguous', 'inconclusive'} & dev.keys()):
            break
        if time.monotonic() > deadline:
            if dev and 'ambiguous' in dev:
                sys.exit(f"multiple devices with serial {args.serial}: {', '.join(dev['ambiguous'])} "
                         '— stale enumeration from another port? replug or retry')
            if dev and 'inconclusive' in dev:
                from helper import hil_util as _hu
                sys.exit(f"cannot tell whether {VID}:{PID} is present: bounded sysfs reads "
                         f"did not answer for {', '.join(dev['inconclusive'])}"
                         f"{_hu.sysfs_blind_note()}")
            sys.exit(f'no {VID}:{PID} device' + (f' with serial {args.serial}' if args.serial else ''))
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
            cases.append(int(tok))
    else:
        cases = [n for t in range(1, tier + 1) for n in TIER_CASES[t]]

    info = f"device {dev['serial']} {dev['node']} speed={dev['speed']} tier={tier}"
    if not args.json:
        print(info)

    # before touching the device: an incompatible host exits here, before any bind
    check_host_compat(dev)

    results = []
    unrecovered_hang = False
    try:
        bind_usbtest(dev)
        set_pattern(0)  # tier 1 firmware sources zeros; also required by perf cases 27/28

        abort_reason = None   # set on any early exit; drives the BUDGET back-fill below
        for idx, num in enumerate(cases):
            # Only a HUNG case aborts the battery; an ordinary case timeout is a FAIL and
            # the loop continues, each burning --timeout+5s, so without this the run can
            # still be in the case loop when the outer timeout SIGKILLs it before it emits
            # JSON. Checked before dispatch: worst overshoot is one case.
            if args.budget and time.monotonic() - t_start > args.budget:
                abort_reason = f'battery budget {args.budget}s exhausted'
                break
            results.append(run_case(num, dev, testusb, args.quick, args.timeout))
            r = results[-1]
            if not args.json:
                extra = f" {r.get('secs', '')}s" if r['status'] == 'PASS' else f" {r.get('detail', '')}"
                extra += f" {r['mbps']} MB/s" if 'mbps' in r else ''
                print(f"test {num:2d} {r['name']:22s} {r['status']:6s}{extra}")
            if r['status'] == 'HUNG':
                abort_reason = 'battery aborted on a kernel-side hang'
                # Reflash, NEVER a root-port cycle: resetting the MCU through the DUT's own
                # debug probe fails the in-flight URB at the source, so the ioctl returns,
                # the queued kill lands and the cleanup below is lock-safe -- and it reaches
                # exactly one board, where a root-port cycle bounces every fixture under the
                # port (and could never remove power anyway; see usb-kernel-recover).
                # Deliberately not gated on a hub-worker check: our own stuck testusb is
                # what drives a hub worker into usb_lock_device(), so a pre-check reads
                # wedged by construction.
                #
                # Assume unrecovered until proven otherwise, so any early exit from this
                # block reaches the finally with the flag set instead of running the
                # remove_id/unbind that must not happen while a device lock is held.
                unrecovered_hang = True
                print('aborting battery: kernel-side hang, device wedged mid-transfer',
                      file=sys.stderr)
                if not (args.recover_board and args.recover_fw):
                    print('no --recover-board/--recover-fw: the device stays wedged and '
                          'cleanup is skipped', file=sys.stderr)
                    break
                # The reflash is bounded to RECOVER_FLASH_TIMEOUT and skipped when the
                # caller's outer bound cannot contain it: the flasher runs in its own
                # session, so an outer killpg mid-flash would ORPHAN it on the probe. Gate
                # each step on the time actually LEFT -- reserving for the worst case up
                # front skipped recovery for nearly every real hang, since the hang-prone
                # cases run late in the tier order.
                def _time_left():
                    if not args.outer_timeout:
                        return float('inf')
                    # what still runs after a step: run_cmd's post-kill reap (10s),
                    # the settle (5s), the sudo-escalated descendant reap run_case may
                    # have just paid (up to 7s) and the JSON write
                    return args.outer_timeout - (time.monotonic() - t_start) - 35

                if _time_left() < RECOVER_RESET_TIMEOUT:
                    print('insufficient time before the outer bound for even a bounded '
                          'reset; the device stays wedged and cleanup is skipped',
                          file=sys.stderr)
                    break
                try:
                    board = json.loads(args.recover_board)
                    bname, fname = board['name'], board['flasher']['name']
                    import hil_flash   # deferred: stdlib-only unless recovery actually runs
                    flash_fn = getattr(hil_flash, f'flash_{fname.lower()}')
                    reset_fn = getattr(hil_flash, f'reset_{fname.lower()}', None)
                except Exception as e:   # malformed/short json, import failure, unknown flasher
                    print(f'reflash recovery unavailable ({e})', file=sys.stderr)
                    break
                # DELIVERY must be convoy-safe or the recovery makes things worse: our own
                # testusb is D-state on this DUT's node, so a flasher that enumerates by
                # OPENING usbfs nodes blocks on it, survives SIGKILL and is abandoned --
                # a SECOND stray, the budget spent, the device still wedged. On 2026-08-12
                # a vid_pid-pinned openocd was the only flasher that still reached its
                # probe; JLinkExe's ShowEmuList returned zero. See hil_flash.convoy_safe.
                if not hil_flash.convoy_safe(board['flasher']):
                    print(f'{fname} is not convoy-safe for delivery (it enumerates by '
                          f'opening usbfs nodes, and this DUT has a D-state holder on '
                          f'its own node): skipping the reflash rather than adding a '
                          f'second stray. Pin the roster entry with vid_pid on an '
                          f'openocd flasher to enable recovery for this board.',
                          file=sys.stderr)
                    break
                # RESET FIRST (see recovery_steps). Non-destructive, ~130 ms, and it
                # clears the wedge by the same mechanism as the reflash. wedged_pids is the
                # arbiter: reset_esptool is a stub that returns rc 0 without resetting
                # anything, so an exit code here proves nothing.
                steps = recovery_steps(fname, _time_left())
                if reset_fn and any(k == 'reset' for k, _ in steps):
                    print(f'auto-recovering: resetting {bname} via {fname} probe '
                          f'(non-destructive; reflash only if this does not clear it)',
                          file=sys.stderr)
                    try:
                        with redirect_stdout(sys.stderr):
                            reset_fn(board, timeout=RECOVER_RESET_TIMEOUT)
                    except TypeError:
                        with redirect_stdout(sys.stderr):
                            reset_fn(board)        # older primitives take no bound
                    except Exception as e:
                        print(f'probe reset raised: {e}; falling through to the reflash',
                              file=sys.stderr)
                    time.sleep(5)                  # let the freed ioctl unwind
                    stuck, complete = wedged_pids(dev['node'])
                    if complete and not stuck:
                        print('probe reset cleared the wedge; skipping the reflash '
                              '(firmware under test left intact for autopsy)',
                              file=sys.stderr)
                        unrecovered_hang = False
                        break
                if _time_left() < RECOVER_FLASH_TIMEOUT:
                    print('reset did not clear it and no budget left for a reflash; '
                          'the device stays wedged', file=sys.stderr)
                    break
                print(f'auto-recovering: reflashing {bname} via '
                      f'{fname} (see .claude/skills/usb-kernel-recover). '
                      f'Unbudgeted by flash_permit, like the root-cycle it replaced: the '
                      f'per-controller semaphores live in hil_test\'s process.',
                      file=sys.stderr)
                # run_cmd bounds the flash; its banners go to stdout, which in --json mode
                # carries the result object -- keep them off it. A raising flasher (missing
                # serial node, unwritable CWD) must not cost the battery its JSON report.
                try:
                    with redirect_stdout(sys.stderr):
                        ret = flash_fn(board, args.recover_fw, timeout=RECOVER_FLASH_TIMEOUT)
                except Exception as e:
                    print(f'reflash raised: {e}; the device may still be wedged', file=sys.stderr)
                    break
                if ret.returncode != 0:
                    # a wedged RP DAP answers nothing and the probe has no reset line;
                    # POR it via the Rescue DP and retry once, exactly as the normal
                    # flash path does (no-op for every other board/failure)
                    out_txt = ret.stdout if isinstance(ret.stdout, str) else ''
                    # inside the redirect like its siblings (hil_test slices the result
                    # object from the first '{' on stdout), and only if a POR + retry
                    # still fits before the outer kill
                    rescued = False
                    try:
                        if _time_left() >= 2 * RECOVER_FLASH_TIMEOUT:
                            with redirect_stdout(sys.stderr):
                                rescued = hil_flash.rescue_openocd(
                                    board, out_txt, timeout=RECOVER_FLASH_TIMEOUT)
                        if rescued:
                            print('DAP wedged; rescued via Rescue DP, retrying reflash',
                                  file=sys.stderr)
                            with redirect_stdout(sys.stderr):
                                ret = flash_fn(board, args.recover_fw,
                                               timeout=RECOVER_FLASH_TIMEOUT)
                    except Exception as e:
                        # guarded like the first flash: a raise here would unwind past the
                        # BUDGET back-fill and the JSON print
                        print(f'rescue/retry raised: {e}', file=sys.stderr)
                    if ret.returncode != 0:
                        print(f'reflash failed (rc {ret.returncode}); the device may still '
                              f'be wedged', file=sys.stderr)
                # settle even on a non-zero exit: the reset may have landed before the
                # flasher failed, and the freed ioctl needs a moment to unwind before
                # wedged_pids samples
                time.sleep(5)
                # Authoritative either way: a clean flash only proves the probe wrote the
                # MCU, not that the D-state holder let go.
                stuck, complete = wedged_pids(dev['node'])
                if stuck:
                    print(f'{dev["sysname"]}: pid(s) {stuck} still in D state on '
                          f'{dev["node"]} — the device lock was never released', file=sys.stderr)
                    # No hub-worker verdict here: our own testusb still holds the DUT's
                    # device lock, which is what drives a hub worker into usb_lock_device()
                    # -- any verdict from here is confounded by construction.
                elif not complete:
                    print('cannot confirm recovery: /proc is only partly readable, so a '
                          'hidden D-state holder cannot be ruled out', file=sys.stderr)
                else:
                    unrecovered_hang = False
                break
            # re-resolve: a mid-battery re-enumeration changes the devnum and so the node
            # path. Match on the concrete serial (not args.serial, which may be None) so
            # this can never retarget to another device sharing the VID:PID.
            # first=False: the ambiguity guard exists because ONE serial can match two
            # sysfs nodes on the dual-port WCH parts, and `dev = live` below makes any
            # mistake stick for the rest of the battery -- including wedged_pids() then
            # scanning the wrong node and clearing unrecovered_hang on a device it never
            # checked. Ambiguous comes back as {'ambiguous': [...]}, handled below.
            live = find_device(dev['serial'])
            if live and live.get('ambiguous'):
                # two nodes now answer to one serial (the dual-port WCH parts do this
                # around a re-enumeration). Picking either would file the rest of the
                # battery's verdicts under a device we cannot identify, so stop here and
                # keep the recovery in play rather than guess.
                abort_reason = (f'serial {dev["serial"]} matches more than one device '
                                f'({", ".join(live["ambiguous"])}) after case {num}')
                unrecovered_hang = True
                break
            if live and live.get('inconclusive'):
                # bounded reads stopped answering, so we cannot say the device left --
                # treat it as the wedge it probably is, which keeps the HUNG reflash and
                # the lock-safe cleanup in play
                from helper import hil_util as _hu
                abort_reason = ('cannot tell whether the device is still present: bounded '
                                'sysfs reads stopped answering' + _hu.sysfs_blind_note())
                unrecovered_hang = True
                break
            if not live:
                # no second entry for `num`: run_case already recorded it, and a duplicate
                # inflates the denominator (31/30) and reports a PASSing case as failed
                abort_reason = f'device dropped off the bus after case {num}'
                break
            dev = live
        if abort_reason and all(c in {r['num'] for r in results} for c in cases) \
                and 'dropped off the bus' in abort_reason and results:
            # nothing left to back-fill (the drop happened during/after the LAST case),
            # so the run would report a clean pass; the case it died on is not a pass
            if results[-1].get('status') == 'PASS':
                # only a PASS: a real FAIL/NOTRUN verdict names the actual regression
                # (errno, dmesg) and must not be overwritten by the drop message
                results[-1] = dict(results[-1], status='FAIL', detail=abort_reason)
        if abort_reason:
            # One BUDGET entry per case never dispatched, on EVERY abort path: a shrunken
            # denominator (4/5 instead of 4/30) hides that most of the battery never
            # executed and makes a regression in the skipped range read as "not the
            # problem".
            ran = {r['num'] for r in results}
            results += [{'num': n, 'status': 'BUDGET', 'detail': f'not run: {abort_reason}'}
                        for n in cases if n not in ran]
    finally:
        # best-effort cleanup: a sudo/sysfs failure here (sudo() may sys.exit) must not replace
        # an exception propagating out of the try body with a less useful one
        try:
            if unrecovered_hang:
                # testusb still holds the device lock in a usbfs ioctl: remove_id/unbind
                # would join the convoy and deadlock the bus (see usb-kernel-recover)
                print('skipping cleanup after unrecovered hang: ask the operator for a full PVE host '
                      'power cycle (a VM reboot is not reliable — hubs latch up across the PCIe reset)',
                      file=sys.stderr)
            elif not args.keep_binding:
                sysfs_write(DRIVER / 'remove_id', f'{VID} {PID}', check=False)
                # release every claimed interface: another device sharing the VID:PID
                # (stale example firmware) may have been grabbed on probe and would stay
                # bound to usbtest until re-plugged, hijacking the next test's device
                for intf in DRIVER.glob('*:*'):
                    sysfs_write(DRIVER / 'unbind', intf.name, check=False)
        except SystemExit:
            pass

    # BUDGET, not NOTRUN: NOTRUN is taken, for a case the KERNEL gated off (-EOPNOTSUPP,
    # see run_case) -- a real result that must stay in `failed` and keep its case number.
    # BUDGET keeps the denominator honest without lying about the numerator: naming cases
    # that never executed as failures sends a maintainer bisecting one of them.
    notrun = [r for r in results if r['status'] == 'BUDGET']
    failed = [r for r in results if r['status'] not in ('PASS', 'BUDGET')]
    ran = len(results)
    if args.json:
        # `wedged` is the verdict this process ALREADY computed; without it the caller had
        # to infer one from 'HUNG' in our stdout, which misses a recovery that ran and
        # failed, the inconclusive abort (no case reaches status HUNG), and any battery
        # killed before it printed.
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
    # NOTRUN counts toward the exit status even though it is reported separately: a
    # standalone run whose cases were all skipped has NOT passed, and returning 0 hands a
    # false success to any script driving this directly.
    return len(failed) + len(notrun)


if __name__ == '__main__':
    sys.exit(main())
