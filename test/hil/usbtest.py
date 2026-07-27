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
import os
import re
import shutil
import subprocess
import sys
import time
from pathlib import Path

VID = 'cafe'
PID = '4010'
GZ_REF = '0525 a4a0'  # copy Gadget Zero's capability profile (ctrl_out+iso+intr)
SYS_USB = Path('/sys/bus/usb/devices')
DRIVER = Path('/sys/bus/usb/drivers/usbtest')
USB_RECOVER = Path(__file__).resolve().parents[2] / '.claude/skills/usb-kernel-recover/scripts/usb_recover.sh'
PATTERN_PARAM = Path('/sys/module/usbtest/parameters/pattern')

# Battery per tier, in run order: control sanity first, then simple bulk,
# queued, unaligned, unlink, halt/toggle, throughput last.
TIER_CASES = {
    1: [0, 9, 10, 1, 2, 3, 4, 5, 6, 7, 8, 17, 18, 19, 20, 11, 12, 24, 13, 29, 27, 28],
    2: [14, 21],
    3: [25, 26],
    4: [15, 16, 22, 23],
}

# Per-case testusb parameters (full speed / high speed). All -s/-v values are
# multiples of 512 so transfers stay packet-aligned at both speeds: the device
# streams whole max-size packets and a non-aligned IN length would babble.
# 14/21 must never run with defaults (vary >= length is -EINVAL in the kernel).
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
    kw.setdefault('capture_output', True)
    kw.setdefault('text', True)
    return subprocess.run(cmd, **kw)


def sudo(cmd, **kw):
    if os.geteuid() != 0:
        cmd = ['sudo', '-n'] + cmd
    r = run(cmd, **kw)
    if r.returncode != 0 and 'password is required' in (r.stderr or ''):
        sys.exit(f'sudo needs a password for: {" ".join(cmd)}\n'
                 'Run as root, or grant this user NOPASSWD sudo.')
    return r


def sysfs_write(path, data, check=True):
    # A driver-registry write (new_id/remove_id/bind) blocks in D state when a wedged device
    # holds its lock (driver_attach walks the bus): fail fast and loud instead of piling up
    # unkillable writers and hanging the whole run -- the rig needs USB recovery first.
    try:
        r = sudo(['tee', str(path)], input=data, timeout=15)
    except subprocess.TimeoutExpired:
        sys.exit(f'write "{data}" > {path} blocked >15s: USB subsystem is wedged '
                 '(a D-state device lock exists). Recover the rig (usb_recover.sh) '
                 'before running batteries.')
    if check and r.returncode != 0:
        sys.exit(f'write "{data}" > {path} failed: {r.stderr.strip()}')
    return r.returncode == 0


def find_device(serial, first=False):
    """Locate the usbtest device in sysfs, return info dict or None."""
    matches = []
    for dev in SYS_USB.iterdir():
        try:
            if (dev / 'idVendor').read_text().strip() != VID or \
               (dev / 'idProduct').read_text().strip() != PID:
                continue
            dev_serial = (dev / 'serial').read_text().strip()
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
        return None
    if len(matches) > 1 and not first:
        if serial:
            # Dual-port parts (nanoch32v203 fsdev/usbfs, ch32v307 usbhs/usbfs) briefly enumerate
            # BOTH ports with the same serial around a variant reflash; picking one arbitrarily
            # could bind the stale port. Report ambiguity so the caller retries until it drops.
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
            # transient sysfs error (e.g. racing a re-enumeration): retry so a blip doesn't
            # silently pass an incompatible host; if the probe truly fails, fail open but say so
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
        # The Renesas uPD720201/uPD720202 must run its latest firmware (>= 2.0.2.6,
        # K2026090.mem; RAM-uploaded, so it reverts to ROM on every power cycle unless
        # re-loaded). On the ROM firmware its command ring intermittently dies under unlink
        # stress: a Configure Endpoint command stops completing, the hub worker deadlocks
        # holding the device lock (needs a host power cycle). Three separate boards killed
        # it this way (ch32v307 2026-07-10; ra6m5 test 24, mimxrt1015 2026-07-11). Both
        # parts expose the FW version register at PCI config offset 0x6c. NOTE this check
        # is necessary, not sufficient: board-specific batteries have killed the controller
        # on current firmware too (mimxrt1015, stop-endpoint timeout) - those are handled
        # by per-board skips in the rig config.
        fw = None
        try:
            r = sudo(['setpci', '-s', pci.name, '0x6c.l'], capture_output=True, text=True)
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
        r = sudo(['modprobe', 'usbtest'])
        if r.returncode != 0 or not DRIVER.exists():
            sys.exit(f'cannot load usbtest module: {r.stderr.strip()}')

    # always re-register in case a stale dynamic id carries a different profile
    intf = f'{dev["sysname"]}:1.0'
    drv = SYS_USB / intf / 'driver'
    stale_binding = drv.is_symlink() and drv.resolve().name == 'usbtest'
    sysfs_write(DRIVER / 'remove_id', f'{VID} {PID}', check=False)
    sysfs_write(DRIVER / 'new_id', f'{VID} {PID} 0 {GZ_REF}')
    if stale_binding:
        # bound before the re-registration: that probe captured the OLD dynamic id's capability
        # profile; unbind once (device is idle here) so the loop below reprobes the fresh one
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


def dmesg_tail():
    r = sudo(['dmesg'])
    lines = [l for l in r.stdout.splitlines() if 'usbtest' in l]
    return '\n'.join(lines[-8:])


def wedged_pids(devnode):
    """Return (pids, complete): PIDs in uninterruptible sleep whose cmdline names devnode, i.e.
    still holding its usbfs device lock, and whether every /proc entry could actually be read.

    Matched by device node rather than by our child's pid because run_case() may wrap testusb in
    sudo, in which case the Popen pid is the wrapper and the blocked process is its child --
    killing the wrapper would make a pid-based check look clean while the real holder is stuck.

    complete is False when a PermissionError hid an entry (a hidepid/ProtectProc mount, or the
    root-owned child of that same sudo). An entry we could not read might be the holder, so the
    caller must treat that as unrecovered rather than as an all-clear."""
    stuck, complete = [], True
    for entry in Path('/proc').iterdir():
        if not entry.name.isdigit():
            continue
        try:
            cmdline = (entry / 'cmdline').read_bytes()
        except PermissionError:
            complete = False        # cannot rule this pid out
            continue
        except OSError:
            continue                # raced with process exit: genuinely gone, not hidden
        if devnode.encode() not in cmdline:
            continue
        try:
            stat = (entry / 'stat').read_text()
            if stat[stat.rindex(')') + 2] == 'D':   # comm may contain ')', so scan from the right
                stuck.append(int(entry.name))
        except PermissionError:
            complete = False
        except (OSError, ValueError, IndexError):
            continue
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

    p = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    try:
        out, _ = p.communicate(timeout=timeout)
    except subprocess.TimeoutExpired:
        p.kill()
        try:
            out, _ = p.communicate(timeout=5)
        except subprocess.TimeoutExpired:
            # SIGKILL had no effect: the child is in uninterruptible sleep on an
            # in-kernel usbfs ioctl (device stopped responding mid-transfer).
            # Abandon it — waiting or re-signalling can never succeed.
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
    args = p.parse_args()
    sys.stdout.reconfigure(line_buffering=True)  # per-case results visible when piped/logged

    testusb = args.testusb or shutil.which('testusb') or os.path.expanduser('~/testusb')
    if not os.access(testusb, os.X_OK):
        sys.exit('testusb binary not found: build kernel tools/usb/testusb.c '
                 'and install it, or pass --testusb')

    # retry briefly: right after a flash the enumeration may still be settling, and on dual-port
    # parts the other port's stale same-serial node takes a moment to drop off (see find_device)
    deadline = time.monotonic() + 8
    while True:
        dev = find_device(args.serial)
        if dev and 'ambiguous' not in dev:
            break
        if time.monotonic() > deadline:
            if dev:
                sys.exit(f"multiple devices with serial {args.serial}: {', '.join(dev['ambiguous'])} "
                         '— stale enumeration from another port? replug or retry')
            sys.exit(f'no {VID}:{PID} device' + (f' with serial {args.serial}' if args.serial else ''))
        time.sleep(0.5)

    # tier drives which cases run; a stale/foreign device advertising an out-of-range tier
    # must not silently run an empty battery ('0/0 passed' would read as green in CI)
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

    # probe the upstream controller before touching the device: an incompatible host
    # (MosChip MCS9990, or uPD720201 on pre-2.0.2.6 firmware) exits here, before any bind
    check_host_compat(dev)

    results = []
    unrecovered_hang = False
    try:
        bind_usbtest(dev)
        set_pattern(0)  # tier 1 firmware sources zeros; also required by perf cases 27/28

        for num in cases:
            results.append(run_case(num, dev, testusb, args.quick, args.timeout))
            r = results[-1]
            if not args.json:
                extra = f" {r.get('secs', '')}s" if r['status'] == 'PASS' else f" {r.get('detail', '')}"
                extra += f" {r['mbps']} MB/s" if 'mbps' in r else ''
                print(f"test {num:2d} {r['name']:22s} {r['status']:6s}{extra}")
            if r['status'] == 'HUNG':
                print(f'aborting battery: kernel-side hang, device wedged mid-transfer.\n'
                      f'auto-recovering: {USB_RECOVER.name} root-cycle {dev["sysname"]} '
                      f'(see .claude/skills/usb-kernel-recover)', file=sys.stderr)
                # Cutting VBUS at the root port fails the in-flight URB so the usbfs ioctl returns.
                # Must run BEFORE any unbind/remove_id, which would take the device lock the stuck
                # ioctl holds and deadlock the bus.
                #
                # Assume unrecovered until proven otherwise: sudo() calls sys.exit() when `sudo -n`
                # needs a password, and that SystemExit would otherwise unwind straight past this
                # block into the finally cleanup with the flag still False -- running the exact
                # remove_id/unbind the comments there forbid while a device lock is held.
                unrecovered_hang = True
                # Pass the serial so the helper refuses a stale busport rather than cutting power
                # to whatever else now occupies that path. Popen rather than sudo()/subprocess.run:
                # run() would kill() then wait() unbounded on timeout, which never returns if
                # uhubctl is itself in D state -- the case the timeout exists for. Merge stderr
                # into stdout so the helper's target-identity and action lines are not lost.
                cmd = [str(USB_RECOVER), 'root-cycle', dev['sysname'], dev['serial']]
                if os.geteuid() != 0:
                    cmd = ['sudo', '-n'] + cmd
                p = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
                rc = None
                try:
                    out, _ = p.communicate(timeout=60)   # normal run is ~8s
                    rc = p.returncode
                except subprocess.TimeoutExpired:
                    p.kill()
                    try:
                        out, _ = p.communicate(timeout=5)
                        rc = p.returncode
                    except subprocess.TimeoutExpired:
                        out = ('root-cycle abandoned after 60s: uhubctl did not die to SIGKILL, so '
                               'it is wedged too and the convoy has spread beyond this device')
                if out:
                    print(out.strip(), file=sys.stderr)
                if rc is not None:
                    time.sleep(5)  # let the bus settle and the freed ioctl unwind
                    # Authoritative either way. A non-zero exit only means the device did not come
                    # back within the poll (a slow bootloader will do that) -- if nothing still
                    # holds the lock, the bus is usable and cleanup is safe. Conversely a zero exit
                    # only proves re-enumeration, not that the D-state holder let go.
                    stuck, complete = wedged_pids(dev['node'])
                    if stuck:
                        print(f'{dev["sysname"]}: pid(s) {stuck} still in D state on '
                              f'{dev["node"]} — the device lock was never released', file=sys.stderr)
                    elif not complete:
                        print('cannot confirm recovery: /proc is only partly readable, so a '
                              'hidden D-state holder cannot be ruled out', file=sys.stderr)
                    else:
                        unrecovered_hang = False
                break
            # re-resolve: after a mid-battery re-enumeration the devnum (and thus the node
            # path) changes; keep testing the live node instead of the stale one. Match on the
            # concrete serial (not args.serial, which may be None) so this can never retarget to
            # a different device that happens to share the VID:PID.
            live = find_device(dev['serial'], first=True)
            if not live:
                results.append({'num': num, 'status': 'FAIL',
                                'detail': f'device dropped off the bus after case {num}'})
                break
            dev = live
    finally:
        # best-effort cleanup: a sudo/sysfs failure here (sudo() may sys.exit) must not replace
        # an exception propagating out of the try body with a less useful one
        try:
            if unrecovered_hang:
                # testusb is still stuck in a usbfs ioctl holding the device lock; remove_id/unbind
                # would join the convoy and deadlock the bus (see usb-kernel-recover skill) — leave it be
                print('skipping cleanup after unrecovered hang: ask the operator for a full PVE host '
                      'power cycle (a VM reboot is not reliable — hubs latch up across the PCIe reset)',
                      file=sys.stderr)
            elif not args.keep_binding:
                sysfs_write(DRIVER / 'remove_id', f'{VID} {PID}', check=False)
                # release every claimed interface: other devices sharing the VID:PID (stale example
                # firmware on a test rig) may have been grabbed on probe and would otherwise stay
                # bound to usbtest until re-plugged, hijacking the next test's device
                for intf in DRIVER.glob('*:*'):
                    sysfs_write(DRIVER / 'unbind', intf.name, check=False)
        except SystemExit:
            pass

    failed = [r for r in results if r['status'] != 'PASS']
    ran = len(results)
    if args.json:
        print(json.dumps({'serial': dev['serial'], 'speed': dev['speed'], 'tier': tier,
                          'passed': ran - len(failed), 'failed': len(failed),
                          'cases': results}, indent=2))
    else:
        print(f"{ran - len(failed)}/{ran} passed")
        for r in failed:
            print(f"  FAILED test {r['num']}: {r.get('detail', '')}")
            if r.get('dmesg'):
                print('    ' + r['dmesg'].replace('\n', '\n    '))
    return len(failed)


if __name__ == '__main__':
    sys.exit(main())
