#!/usr/bin/env python3
"""Capture one USB bus with usbmon into a Wireshark pcapng.

usage: usbcap.py <bus|VID:PID|VID:|auto> [seconds] [outfile] [--snaplen N]

  <bus>      numeric USB bus (lsusb "Bus 00N" -> N); 0 or "auto" = every bus
  <VID:PID>  the bus of a plugged-in device, e.g. cafe:4010 or 1a86:8010
  <VID:>     any device of that vendor, e.g. cafe:

A selector that matches devices on more than one bus is refused with the
matches listed; pass the bus instead. Assumes usbmon is loaded and
/dev/usbmon* is readable by the wireshark group (see SKILL.md), so tshark
captures without sudo.
"""
import argparse
import os
import re
import subprocess
import sys

LSUSB = re.compile(r'^Bus (\d+) Device (\d+): ID ([0-9a-f]{4}):([0-9a-f]{4})\s*(.*)$', re.I)


def devices(lsusb):
    """[(bus, device, vid, pid, name)] from lsusb output."""
    found = []
    for line in lsusb.splitlines():
        m = LSUSB.match(line.strip())
        if m:
            found.append((int(m[1]), int(m[2]), m[3].lower(), m[4].lower(), m[5].strip()))
    return found


def resolve(target, lsusb):
    """The usbmon bus number for a selector; 0 means every bus. `lsusb` is
    called for its output only when a device has to be looked up."""
    if target == 'auto':
        return 0
    if target.isdigit():
        return int(target)
    m = re.fullmatch(r'([0-9a-f]{4}):([0-9a-f]{4})?', target, re.I)
    if not m:
        raise ValueError(f"bad target '{target}': expected a bus number, VID:PID, VID: or auto")
    vid, pid = m[1].lower(), (m[2] or '').lower()
    matches = [d for d in devices(lsusb()) if d[2] == vid and (not pid or d[3] == pid)]
    if not matches:
        raise ValueError(f"no device matching '{target}' (plugged in?)")
    buses = sorted({d[0] for d in matches})
    if len(buses) > 1:
        raise ValueError(f"'{target}' matches devices on several buses; pass the bus number instead:\n" +
                         '\n'.join(f'  bus {b} device {d}: {v}:{p} {n}' for b, d, v, p, n in matches))
    return buses[0]


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('target')
    parser.add_argument('seconds', nargs='?', type=int, default=10)
    parser.add_argument('outfile', nargs='?', default=f'/tmp/usbcap-{os.getpid()}.pcapng')
    parser.add_argument('--snaplen', type=int, help='bytes per packet, e.g. 128 keeps URB headers only')
    args = parser.parse_args()
    if args.seconds <= 0:
        sys.exit('seconds must be positive')

    try:
        bus = resolve(args.target, lambda: subprocess.run(['lsusb'], capture_output=True, text=True, check=True).stdout)
    except ValueError as e:
        sys.exit(str(e))

    print(f'capturing usbmon{bus} for {args.seconds}s -> {args.outfile}')
    cmd = ['tshark', '-i', f'usbmon{bus}', '-a', f'duration:{args.seconds}', '-w', args.outfile]
    if args.snaplen:
        cmd += ['-s', str(args.snaplen)]
    capture = subprocess.run(cmd, capture_output=True, text=True)
    if capture.returncode != 0:
        sys.exit(f'tshark exited {capture.returncode}: {capture.stderr.strip()}\n'
                 f'(no /dev/usbmon{bus} access? see SKILL.md: wireshark group, or wrap in sg wireshark)')
    count = subprocess.run(['capinfos', '-c', '-M', args.outfile], capture_output=True, text=True)
    if count.returncode != 0:
        sys.exit(f'capture written but unreadable: {count.stderr.strip()}')
    packets = count.stdout.rsplit(':', 1)[-1].strip()  # "Number of packets:   6"
    print(f'saved {args.outfile}  ({packets} packets)')
    print(f'analyze: tshark -r {args.outfile}   |   tshark -r {args.outfile} -V')


if __name__ == '__main__':
    main()
