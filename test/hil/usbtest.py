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
                 'Run as root or add a sudoers entry for testusb/modprobe/tee.')
    return r


def sysfs_write(path, data, check=True):
    r = sudo(['tee', str(path)], input=data)
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
    if len(matches) > 1 and not serial and not first:
        sys.exit(f'multiple {VID}:{PID} devices found, use --serial: '
                 + ', '.join(m["serial"] for m in matches))
    return matches[0]


def bind_usbtest(dev):
    """Bind the device's interface 0 to the usbtest driver."""
    if not DRIVER.exists():
        r = sudo(['modprobe', 'usbtest'])
        if r.returncode != 0 or not DRIVER.exists():
            sys.exit(f'cannot load usbtest module: {r.stderr.strip()}')

    # always re-register in case a stale dynamic id carries a different profile
    sysfs_write(DRIVER / 'remove_id', f'{VID} {PID}', check=False)
    sysfs_write(DRIVER / 'new_id', f'{VID} {PID} 0 {GZ_REF}')

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


def dmesg_tail():
    r = sudo(['dmesg'])
    lines = [l for l in r.stdout.splitlines() if 'usbtest' in l]
    return '\n'.join(lines[-8:])


def pci_addr_of_bus(busnum):
    """Return the PCI B:D.F backing a USB bus, or None for a non-PCI (SoC/platform) controller."""
    m = re.search(r'([0-9a-f]{4}:[0-9a-f]{2}:[0-9a-f]{2}\.[0-9])/usb\d+$',
                  os.path.realpath(f'/sys/bus/usb/devices/usb{int(busnum)}'))
    return m.group(1) if m else None


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
                      'is not writable and testusb is not in the sudoers allowlist')
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

    dev = find_device(args.serial)
    if not dev:
        sys.exit(f'no {VID}:{PID} device' + (f' with serial {args.serial}' if args.serial else ''))

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

    results = []
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
                pci = pci_addr_of_bus(dev['node'].split('/')[-2])
                if pci:
                    print(f'aborting battery: kernel-side hang, device wedged mid-transfer.\n'
                          f'auto-recovering: sudo usb_recover.sh pci-reset {pci} '
                          f'(see .claude/skills/usb-recover)', file=sys.stderr)
                    # FLR frees the D-state ioctl without the device lock; must run BEFORE
                    # any unbind/remove_id, which would deadlock the bus otherwise
                    sudo(['/usr/local/sbin/usb_recover.sh', 'pci-reset', pci])
                    time.sleep(5)  # let the bus re-enumerate before cleanup touches sysfs
                else:
                    print('aborting battery: kernel-side hang, and the controller has no PCI address '
                          'for FLR recovery — manual intervention (reboot) required', file=sys.stderr)
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
            if not args.keep_binding:
                sysfs_write(DRIVER / 'remove_id', f'{VID} {PID}', check=False)
                # release every claimed interface: other devices sharing the VID:PID (stale example
                # firmware on a test rig) may have been grabbed on probe and would otherwise stay
                # bound to usbtest until re-plugged, hijacking the next test's device
                for intf in DRIVER.glob('*:*'):
                    sysfs_write(DRIVER / 'unbind', intf.name, check=False)
        except SystemExit:
            pass

    failed = [r for r in results if r['status'] != 'PASS']
    if args.json:
        print(json.dumps({'serial': dev['serial'], 'speed': dev['speed'], 'tier': tier,
                          'passed': len(results) - len(failed), 'failed': len(failed),
                          'cases': results}, indent=2))
    else:
        print(f"{len(results) - len(failed)}/{len(results)} passed")
        for r in failed:
            print(f"  FAILED test {r['num']}: {r.get('detail', '')}")
            if r.get('dmesg'):
                print('    ' + r['dmesg'].replace('\n', '\n    '))
    return len(failed)


if __name__ == '__main__':
    sys.exit(main())
