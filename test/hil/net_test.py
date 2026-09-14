#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Exercise the webserver on one USB device, independent of host DHCP and routing.

Invoked by hil_test.py while it holds the board lock. Only the USB network interface
whose parent has the requested serial is moved. Each board gets its own namespace:
the example deliberately shares its MAC and 192.168.7.1 with every other board.
Requires iproute2, udevadm wait, and root (or passwordless sudo).
No persistent host configuration.
"""
import argparse
from contextlib import contextmanager
import hashlib
import http.client
import json
import os
from pathlib import Path
import signal
import sys
import time
import uuid

from helper import hil_util

DEVICE_IP = '192.168.7.1'
# Default assets in the pinned lwIP dependency's src/apps/http/fsdata.c. Verify
# complete responses, including the binary image, rather than just TCP connect/200.
ASSETS = (
    ('/index.html', 1751, '1da17ad4638e314329a47a9176973ea59ff0b19fc498f55f01c19053c7816fcc'),
    ('/img/sics.gif', 724, '3b4eb378fdefac1e7dd6dbb6c84db20e1512463457626f5d991bf7c1ccb7cf63'),
)


def command(argv, timeout=5):
    ret = hil_util.run_cmd(argv, timeout=timeout, split_stderr=True, quiet=True)
    if ret.returncode:
        raise RuntimeError(f'{" ".join(argv)}: rc={ret.returncode}: {ret.stderr} {ret.stdout}')
    return ret.stdout


def interfaces_for_uid(uid):
    matches = []
    for dev in hil_util.usb_scan(vid='cafe', serial=uid):
        matches.extend(p.name for p in Path(dev['dir']).glob('*:*/net/*'))
    return sorted(set(matches))


def wait_interface(uid, timeout=30):
    deadline = time.monotonic() + timeout
    matches = []
    detail = ''
    while time.monotonic() < deadline:
        matches = interfaces_for_uid(uid)
        if len(matches) == 1:
            # A sysfs node exists BEFORE udev finishes initializing/renaming it.
            # Wait for this device only: a global settle would also wait for peers'
            # mtp-probe jobs. Rediscover by serial if the name changed during wait.
            ret = hil_util.run_cmd(['udevadm', 'wait', '--timeout=1',
                                    '/sys/class/net/' + matches[0]],
                                   timeout=2, split_stderr=True, quiet=True)
            detail = ret.stderr or ''
            if ret.returncode == 0 and interfaces_for_uid(uid) == matches:
                return matches[0]
        time.sleep(0.2)
    raise RuntimeError(f'USB serial {uid}: expected one initialized network interface within {timeout}s; '
                       f'found {matches}; {detail}')


@contextmanager
def network_namespace(iface):
    ns = 'tusb-net-' + uuid.uuid4().hex[:12]
    command(['ip', 'netns', 'add', ns])
    try:
        command(['ip', 'link', 'set', 'dev', iface, 'netns', ns])
        # udev may have renamed it while it was still on the host. The new
        # namespace contains only loopback and the interface we just moved.
        links = json.loads(command(['ip', '-n', ns, '-j', 'link', 'show']))
        names = [link['ifname'] for link in links if link['ifname'] != 'lo']
        if len(names) != 1:
            raise RuntimeError(f'{ns}: expected one network interface, found {names}')
        iface = names[0]
        # Remove any address a host network manager assigned before the move.
        command(['ip', '-n', ns, 'addr', 'flush', 'dev', iface])
        command(['ip', '-n', ns, 'addr', 'add', '192.168.7.2/24', 'dev', iface])
        command(['ip', '-n', ns, 'link', 'set', 'dev', iface, 'up'])
        yield ns
    finally:
        # Delete on success and failure. Once no process holds the namespace, its
        # physical USB interface returns to the host (ip-netns(8)).
        command(['ip', 'netns', 'delete', ns])


def check_http():
    # HTTP is present on every SRAM tier; INCLUDE_IPERF is deliberately absent on
    # smaller parts. Direct connections also ignore the host's HTTP proxy settings.
    total = 0
    for _ in range(3):
        for path, size, digest in ASSETS:
            conn = http.client.HTTPConnection(DEVICE_IP, timeout=3)
            try:
                # Bringing the interface up triggers the class packet filter. Only
                # the first connection may wait for that; data errors are failures.
                deadline = time.monotonic() + (10 if total == 0 else 0)
                while True:
                    try:
                        conn.connect()
                        break
                    except OSError:
                        conn.close()
                        if time.monotonic() >= deadline:
                            raise
                        time.sleep(0.5)
                conn.request('GET', path, headers={'Connection': 'close'})
                response = conn.getresponse()
                data = response.read(size + 1)
                if response.status != 200 or len(data) != size or hashlib.sha256(data).hexdigest() != digest:
                    raise AssertionError(f'{path}: HTTP {response.status}, {len(data)} bytes; expected {size} bytes with SHA256 {digest}')
                total += len(data)
            finally:
                conn.close()
    print(f'HTTP: {3 * len(ASSETS)} responses verified, {total} bytes', flush=True)


def interrupted(signum, _frame):
    raise TimeoutError(f'network test interrupted (signal {signum})')


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    mode = ap.add_mutually_exclusive_group(required=True)
    mode.add_argument('--uid')
    mode.add_argument('--http', action='store_true', help=argparse.SUPPRESS)
    args = ap.parse_args()
    # A root-owned helper must bound itself even if its unprivileged caller dies.
    signal.signal(signal.SIGALRM, interrupted)
    signal.signal(signal.SIGTERM, interrupted)
    signal.alarm(75)
    if args.http:
        check_http()
    else:
        if os.geteuid() != 0:
            raise RuntimeError('USB network setup requires root or passwordless sudo')
        iface = wait_interface(args.uid)
        print(f'USB serial {args.uid}: interface {iface}', flush=True)
        with network_namespace(iface) as ns:
            try:
                output = command(['ip', 'netns', 'exec', ns, sys.executable,
                                  str(Path(__file__).resolve()), '--http'], timeout=30)
            except RuntimeError:
                for view in (['-s', 'link'], ['addr'], ['neigh']):
                    try:
                        print(command(['ip', '-n', ns, *view, 'show']), file=sys.stderr)
                    except RuntimeError as diagnostic:
                        print(diagnostic, file=sys.stderr)
                raise
            print(output, end='', flush=True)
    signal.alarm(0)


if __name__ == '__main__':
    main()
