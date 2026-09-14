#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Exercise the webserver on one USB device, independent of host DHCP and routing.

Invoked by hil_test.py while it holds the board lock. Only the USB network interface
whose parent has the requested serial is moved. Each board gets its own namespace:
the example deliberately shares its MAC and 192.168.7.1 with every other board.
Requires iproute2, util-linux, udevadm wait, and root (or passwordless sudo).
No persistent host configuration.
"""
import argparse
import hashlib
import http.client
import json
import os
from pathlib import Path
import signal
import sys
import time
import select
import subprocess

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


def privileged(argv):
    return (['sudo', '-n'] if os.geteuid() != 0 else []) + argv


def stop_client(proc, pid):
    # The unshare child runs as us, even when its sudo wrapper belongs to root.
    # Terminate that child directly so sudo can reap it and return normally.
    for sig in (signal.SIGTERM, signal.SIGKILL):
        if pid is not None:
            try:
                os.kill(pid, sig)
            except ProcessLookupError:
                pass
        try:
            proc.communicate(timeout=5)
            return
        except subprocess.TimeoutExpired:
            pass
    hil_util._close_pipes(proc)
    raise RuntimeError('network client did not exit after SIGKILL')


def check_device(uid):
    iface = wait_interface(uid)
    print(f'USB serial {uid}: interface {iface}', flush=True)
    # Anonymous namespace: its last process exiting releases the USB interface,
    # even on SIGKILL. unshare drops to the caller's UID/GID BEFORE executing any
    # checkout-controlled Python. Only trusted system tools run with elevation.
    argv = privileged(['unshare', '--net', '--setgid', str(os.getgid()),
                       '--setuid', str(os.getuid()), sys.executable,
                       str(Path(__file__).resolve()), '--http'])
    # Keep the child in our process group: hil_test's interruption sweep can kill
    # the whole group without stranding a detached namespace owner.
    proc = subprocess.Popen(argv, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE, text=True)
    pid = None
    try:
        if not select.select([proc.stdout], [], [], 5)[0]:
            raise RuntimeError('network namespace client did not become ready')
        ready = proc.stdout.readline(64).strip()
        if not ready.isdecimal():
            raise RuntimeError(f'network namespace client failed to start: {ready!r}')
        pid = int(ready)
        ns = f'/proc/{pid}/ns/net'
        command(privileged(['ip', 'link', 'set', 'dev', iface, 'netns', str(pid)]))
        ip = privileged(['nsenter', '--net=' + ns, 'ip'])
        links = json.loads(command(ip + ['-j', 'link', 'show']))
        names = [link['ifname'] for link in links if link['ifname'] != 'lo']
        if len(names) != 1:
            raise RuntimeError(f'{ns}: expected one network interface, found {names}')
        iface = names[0]
        command(ip + ['addr', 'flush', 'dev', iface])
        command(ip + ['addr', 'add', '192.168.7.2/24', 'dev', iface])
        command(ip + ['link', 'set', 'dev', iface, 'up'])
        try:
            stdout, stderr = proc.communicate(input='go\n', timeout=30)
        except subprocess.TimeoutExpired as exc:
            raise RuntimeError('network HTTP client timed out after 30s') from exc
        if proc.returncode:
            raise RuntimeError(f'network HTTP client failed (rc={proc.returncode}): {stdout} {stderr}')
        print(stdout, end='', flush=True)
    finally:
        if proc.poll() is None:
            stop_client(proc, pid)
        else:
            hil_util._close_pipes(proc)


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
        print(os.getpid(), flush=True)
        if sys.stdin.readline().strip() != 'go':
            raise RuntimeError('network setup owner exited before starting HTTP')
        print(f'network client uid/gid: {os.getuid()}/{os.getgid()}', flush=True)
        check_http()
    else:
        check_device(args.uid)
    signal.alarm(0)


if __name__ == '__main__':
    main()
