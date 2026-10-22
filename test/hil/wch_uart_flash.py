#!/usr/bin/env python3
"""Flash a CH32H417 board through the BSP UART loader over the WCH-LinkE VCP.

The nanoCH32H417's only SWD/SDI pins are the USB2 D+/D- pair: with an untaped USB3
cable plugged into a host, the host PHY's line termination corrupts SDI and wlink
cannot flash. The BSP's UART loader (hw/bsp/ch32h417/family.c) reflashes over
USART1 instead, which has dedicated pins. Protocol documented there.

Usage: wch_uart_flash.py (--uid LINKE_SERIAL | --port /dev/ttyACMx) firmware.bin
Stdlib only (termios); no pyserial dependency.
"""

import argparse
import binascii
import glob
import os
import struct
import sys
import termios
import time

MAGIC = b"\x55\xaaFL"
CHUNK = 1024


def open_tty(path: str) -> int:
    fd = os.open(path, os.O_RDWR | os.O_NOCTTY)
    a = termios.tcgetattr(fd)
    a[0] = 0  # iflag: raw
    a[1] = 0  # oflag: raw
    a[2] = termios.CS8 | termios.CREAD | termios.CLOCAL
    a[3] = 0  # lflag: raw
    a[4] = termios.B115200
    a[5] = termios.B115200
    a[6][termios.VMIN] = 0
    a[6][termios.VTIME] = 1  # 100 ms read granularity
    termios.tcsetattr(fd, termios.TCSANOW, a)
    termios.tcflush(fd, termios.TCIOFLUSH)
    return fd


def write_all(fd: int, data: bytes) -> None:
    """Write every byte: os.write() may accept fewer than requested, and a short write
    swallowed silently would desync the loader protocol mid-flash."""
    mv = memoryview(data)
    while mv:
        n = os.write(fd, mv)
        if n == 0:  # blocking tty should never return 0; bail rather than spin forever
            raise OSError("tty write returned 0")
        mv = mv[n:]


def wait_for(fd: int, want: bytes, timeout: float, ok_noise: bytes = b"L") -> bool:
    """Read until a byte in `want` arrives; bytes in ok_noise are ignored quietly."""
    want_set = set(want)      # sets of ints, built once (not per received byte)
    noise_set = set(ok_noise)
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        b = os.read(fd, 1)
        if not b:
            continue
        if b[0] in want_set:
            return True
        if b[0] not in noise_set:
            # unexpected byte (e.g. 'E'/'X'): fail fast
            return False
    return False


def flash(fd: int, image: bytes) -> bool:
    padded = image + b"\xff" * (-len(image) % CHUNK)
    crc = binascii.crc32(padded) & 0xFFFFFFFF

    # Ask running firmware to reboot into the loader (harmless if already there)
    write_all(fd, MAGIC)
    time.sleep(0.2)

    # Sync on the loader banner (device resets + boots: allow a few seconds). The dying
    # firmware may emit arbitrary debug-UART bytes during the transition: ignore everything
    # that is not the banner rather than failing fast.
    deadline = time.monotonic() + 8.0
    seen = False
    while time.monotonic() < deadline:
        b = os.read(fd, 1)
        if b == b"L":
            seen = True
            break
    if not seen:
        print("no loader banner ('L') - firmware without the hook, or dead board", file=sys.stderr)
        return False

    termios.tcflush(fd, termios.TCIFLUSH)  # drop queued banners
    write_all(fd, b"H" + struct.pack("<II", len(padded), crc))
    if not wait_for(fd, b"A", timeout=25.0):  # erase can take a while
        print("erase not acknowledged", file=sys.stderr)
        return False

    for off in range(0, len(padded), CHUNK):
        write_all(fd, b"C" + struct.pack("<H", CHUNK) + padded[off:off + CHUNK])
        if not wait_for(fd, b"K", timeout=5.0, ok_noise=b""):
            print(f"chunk at {off:#x} not acknowledged", file=sys.stderr)
            return False

    if not wait_for(fd, b"D", timeout=10.0, ok_noise=b""):
        print("final CRC check failed on device", file=sys.stderr)
        return False
    return True


def main() -> int:
    p = argparse.ArgumentParser()
    g = p.add_mutually_exclusive_group(required=True)
    g.add_argument("--uid", help="WCH-LinkE serial; VCP resolved via /dev/serial/by-id")
    g.add_argument("--port", help="tty device path")
    p.add_argument("firmware", help=".bin image (flash layout, offset 0)")
    args = p.parse_args()

    port = args.port
    if args.uid:
        hits = [t for t in glob.glob("/dev/serial/by-id/*") if args.uid in t and t.endswith("if01")]
        if not hits:
            print(f"no VCP found for uid {args.uid}", file=sys.stderr)
            return 1
        port = hits[0]

    with open(args.firmware, "rb") as f:
        image = f.read()
    if not image:
        print("empty image", file=sys.stderr)
        return 1

    fd = open_tty(port)
    try:
        t0 = time.monotonic()
        ok = flash(fd, image)
        if ok:
            print(f"flashed {len(image)} bytes in {time.monotonic() - t0:.1f}s via {port}")
            return 0
        return 1
    finally:
        os.close(fd)


if __name__ == "__main__":
    sys.exit(main())
