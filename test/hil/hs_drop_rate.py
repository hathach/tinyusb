#!/usr/bin/env python3
"""Pooled SETUP drop-rate measurement for a USB high-speed device on the sniffer tap.

A SETUP token that gets no handshake from the device is a "drop": a SETUP may not be NAKed
or STALLed, so the host retries ~124 us later and fails the transfer with -EPROTO after
three attempts. Drop rate, pooled over many SETUPs from a fresh flash, is the primary
metric for CH32H417 HS work -- the usbtest pass count is too noisy to judge changes by.

Every run starts from a fresh flash: the drop rate drifts upward with uptime (44% fresh to
71% after ~3 min of hammering), so a measurement taken on a warm board is not comparable to
one taken on a cold board. Results are pooled across runs because a single ~90-SETUP capture
spans 44-71% on one unchanged build -- wide enough to invent differences that are not there.
"""
import argparse
import subprocess
import sys
import time
from pathlib import Path

SETUP_PID = '0x2d'
ACK_PID = '0xd2'


def parse_setup_outcomes(rows):
    """Count (acked, dropped) SETUP transactions.

    rows: iterable of (time, pid, src, dst) as emitted by tshark -T fields.
    A SETUP is acked when an ACK sourced BY THE DEVICE (src "1.x") appears within the
    next two frames -- the DATA0 payload sits between the token and the handshake.
    """
    rows = list(rows)
    acked = dropped = 0
    for i, row in enumerate(rows):
        if row[1] != SETUP_PID:
            continue
        window = rows[i + 1:i + 3]
        if any(r[1] == ACK_PID and r[2].startswith('1.') for r in window):
            acked += 1
        else:
            dropped += 1
    return acked, dropped


def tshark_rows(pcap):
    out = subprocess.run(
        ['tshark', '-r', str(pcap), '-T', 'fields', '-e', 'frame.time_relative',
         '-e', 'usbll.pid', '-e', 'usbll.src', '-e', 'usbll.dst'],
        capture_output=True, text=True, check=True).stdout
    rows = []
    for line in out.splitlines():
        parts = line.split('\t')
        if len(parts) >= 4 and parts[1]:
            rows.append(tuple(parts[:4]))
    return rows


def find_device(serial):
    """Return 'BBB:DDD' for lsusb -s, located by USB serial string."""
    for dev in Path('/sys/bus/usb/devices').glob('*-*'):
        try:
            if (dev / 'serial').read_text().strip() != serial:
                continue
            return f"{(dev / 'busnum').read_text().strip().zfill(3)}:" \
                   f"{(dev / 'devnum').read_text().strip().zfill(3)}"
        except OSError:
            continue
    return None


def one_run(args, tmp):
    """Capture the wire across a fresh flash: enumeration, then a control hammer.

    The capture deliberately starts BEFORE the flash. Enumeration is the densest run of
    control transfers the device ever sees, and it is the one that happens on a genuinely
    cold controller -- exactly the population this metric is about. Capturing only the
    post-enumeration `lsusb -v` hammer yields ~10 SETUPs per run, because lsusb answers
    mostly from the kernel's cached descriptors and issues few live control transfers.
    """
    fw = Path(args.firmware)
    if not fw.is_file():
        sys.exit(f'firmware not found: {fw} (did the build succeed?)')
    pcap = tmp / 'cap.pcapng'
    pcap.unlink(missing_ok=True)
    sniffer = subprocess.Popen(
        ['usb_sniffer', '--capture', '--fifo', str(pcap), '--speed', 'hs',
         '--fold', '--limit', '4000000'],
        stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    time.sleep(1)
    flash = Path(__file__).resolve().parent / 'wch_uart_flash.py'
    subprocess.run(['python3', str(flash), '--uid', args.flasher_uid, str(fw)],
                   check=True, timeout=120)
    time.sleep(8)  # re-enumeration, captured
    dev = find_device(args.serial)
    if not dev:
        sniffer.terminate()
        sniffer.wait(timeout=15)
        sys.exit(f'device with serial {args.serial} did not re-enumerate after flashing')
    for _ in range(args.hammer):
        subprocess.run(['sudo', 'lsusb', '-v', '-s', dev],
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    sniffer.terminate()
    sniffer.wait(timeout=15)
    return parse_setup_outcomes(tshark_rows(pcap))


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--firmware', required=True, help='.bin to flash before each run')
    p.add_argument('--flasher-uid', required=True, help='WCH-LinkE serial, e.g. E8E68F066EEE')
    p.add_argument('--serial', required=True, help='device USB serial string')
    p.add_argument('--runs', type=int, default=4, help='fresh-flash runs to pool (default 4)')
    p.add_argument('--hammer', type=int, default=8, help='lsusb -v iterations per run')
    p.add_argument('--min-setups', type=int, default=300, help='pooled SETUPs required')
    p.add_argument('--label', default='', help='label printed with the result')
    args = p.parse_args()

    tmp = Path('/tmp/hs_drop_rate')
    tmp.mkdir(exist_ok=True)
    tot_a = tot_d = 0
    for n in range(1, args.runs + 1):
        a, d = one_run(args, tmp)
        tot_a += a
        tot_d += d
        total = a + d
        rate = 100 * d / total if total else 0
        print(f'  run {n}: SETUPs={total:4d} dropped={d:4d} rate={rate:5.1f}%', flush=True)
    pooled = tot_a + tot_d
    rate = 100 * tot_d / pooled if pooled else 0
    print(f'POOLED {args.label}: SETUPs={pooled} dropped={tot_d} rate={rate:.1f}%')
    if pooled < args.min_setups:
        print(f'WARNING: only {pooled} SETUPs pooled (<{args.min_setups}); '
              f'raise --runs or --hammer before trusting this number')
        return 2
    return 0


if __name__ == '__main__':
    sys.exit(main())
