#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Exercise the webserver and iperf server on one USB device, independent of host DHCP and routing.

Invoked by hil_test.py while it holds the board lock. Only the USB network interface
whose parent has the requested serial is moved. Each board gets its own namespace:
the example deliberately shares its MAC and 192.168.7.1 with every other board.
Requires iproute2, util-linux, iperf (2.x), udevadm wait, unprivileged user namespaces and sudo
on hil_root (agentrc), whose netns verb is the one root step; run it as a user other than root.
No persistent host configuration.
"""
import argparse
import hashlib
import http.client
import json
import os
from pathlib import Path
import signal
import socket
import sys
import time
import select
import subprocess

sys.path.append(os.path.dirname(os.path.abspath(__file__)))  # PYTHONSAFEPATH drops it
# hil_util is stdlib-only at import. usb_observe and hil_root load agentrc, which only the
# --uid parent uses: the --http client stays stdlib-only.
from helper import hil_util  # noqa: E402

DEVICE_IP = '192.168.7.1'
IPERF_PORT = 5001  # lwiperf's LWIPERF_TCP_PORT_DEFAULT
IPERF_SECONDS = 3
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
    from helper import usb_observe  # the --uid parent only; see the import note above
    matches = []
    for dev in usb_observe.usb_scan(vid='cafe', serial=uid):
        matches.extend(p.name for p in Path(dev['dir']).glob('*:*/net/*'))
    return sorted(set(matches))


def busport_for_uid(uid):
    from helper import usb_observe  # the --uid parent only; see the import note above
    devs = usb_observe.usb_scan(vid='cafe', serial=uid)
    if len(devs) != 1:
        raise RuntimeError(f'USB serial {uid}: expected one device, found {len(devs)}')
    return Path(devs[0]['dir']).name


def hil_root():
    from helper import agentrc  # the --uid parent only; see the import note above
    return agentrc.load_file('skills/hil/scripts/hil_root.py')


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


def interface_index(iface):
    """The kernel ifindex of `iface` in our namespace; it survives a move into a fresh one."""
    return int(Path('/sys/class/net', iface, 'ifindex').read_text())


def close_pipes(proc):
    """Close our ends of a child's pipes. Never raises."""
    for pipe in (proc.stdout, proc.stderr, proc.stdin):
        try:
            if pipe is not None:
                pipe.close()
        except OSError:
            pass


def stop_client(proc, grace=5):
    # unshare execs the client, so proc is the client itself. Never killpg: it shares
    # the HIL caller's group.
    for sig in (signal.SIGTERM, signal.SIGKILL):
        proc.send_signal(sig)
        try:
            proc.communicate(timeout=grace)
            return
        except subprocess.TimeoutExpired:
            pass
    close_pipes(proc)
    raise RuntimeError('network client did not exit after SIGKILL')


def check_device(uid):
    iface = wait_interface(uid)
    # Bind the move to this device: hil_root moves the one interface at its busport and
    # prints that interface's index, which must be the one read here.
    ifindex = interface_index(iface)
    busport = busport_for_uid(uid)
    print(f'USB serial {uid}: interface {iface} (ifindex {ifindex}) at {busport}', flush=True)
    # Anonymous user and network namespace, ours: its last process exiting releases the
    # USB interface, even on SIGKILL. hil_root moves the interface in only for a namespace
    # a user other than root (the sudo caller) created.
    argv = ['unshare', '--user', '--map-root-user', '--net', sys.executable,
            str(Path(__file__).resolve()), '--http']
    # Keep the child in our process group: hil_test's interruption sweep can kill
    # the whole group without stranding a detached namespace owner.
    proc = subprocess.Popen(argv, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE, text=True)
    try:
        if not select.select([proc.stdout], [], [], 5)[0]:
            raise RuntimeError('network namespace client did not become ready')
        ready = proc.stdout.readline(64).strip()
        if not ready.isdecimal():
            raise RuntimeError(f'network namespace client failed to start: {ready!r}')
        pid = int(ready)
        moved = command(hil_root().argv('netns', busport, str(pid)), timeout=30).split()
        if moved[:1] != [str(ifindex)]:
            raise RuntimeError(f'hil_root netns {busport}: moved {moved}, expected ifindex {ifindex}')
        ip = ['nsenter', '--preserve-credentials', f'--target={pid}', '--user', '--net', 'ip']
        # A fresh netns may also hold fallback tunnels (tunl0, sit0, ...) of loaded modules.
        links = json.loads(command(ip + ['-j', 'link', 'show']))
        moved_links = [link for link in links if link.get('ifindex') == ifindex]
        if len(moved_links) != 1:
            found = [(link['ifname'], link.get('ifindex')) for link in links]
            raise RuntimeError(f'pid {pid}: expected one network interface with ifindex {ifindex}, found {found}')
        iface = moved_links[0]['ifname']
        command(ip + ['addr', 'flush', 'dev', iface])
        command(ip + ['addr', 'add', '192.168.7.2/24', 'dev', iface])
        command(ip + ['link', 'set', 'dev', iface, 'up'])
        try:
            stdout, stderr = proc.communicate(input='go\n', timeout=45)
        except subprocess.TimeoutExpired as exc:
            raise RuntimeError('network client timed out after 45s') from exc
        if proc.returncode:
            raise RuntimeError(f'network client failed (rc={proc.returncode}): {stdout} {stderr}')
        print(stdout, end='', flush=True)
    finally:
        if proc.poll() is None:
            stop_client(proc)
        else:
            close_pipes(proc)


def check_http():
    # Direct connections also ignore the host's HTTP proxy settings.
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


def check_iperf():
    # lwiperf runs only on the high-throughput SRAM tier (INCLUDE_IPERF), so a refused port is
    # reported, not failed: it cannot tell a build without lwiperf from one whose server failed
    # to start. Its tradeoff (-r) and dual (-d) tests fail against iperf 2.2, so only host to
    # device is measured.
    try:
        socket.create_connection((DEVICE_IP, IPERF_PORT), timeout=3).close()
    except ConnectionRefusedError:
        print(f'iperf: port {IPERF_PORT} refused, no server', flush=True)
        return
    try:
        ret = subprocess.run(['iperf', '-c', DEVICE_IP, '-p', str(IPERF_PORT), '-t', str(IPERF_SECONDS), '-y', 'C'],
                             capture_output=True, text=True, timeout=IPERF_SECONDS + 15)
    except subprocess.TimeoutExpired as exc:
        raise RuntimeError(f'iperf timed out after {exc.timeout}s') from exc
    # iperf 2 exits 0 even on a refused or broken connection: only one whole CSV report counts.
    lines = ret.stdout.splitlines()
    fields = lines[0].split(',') if len(lines) == 1 else []
    if ret.returncode or ret.stderr.strip() or len(fields) != 9:
        raise RuntimeError(f'iperf failed (rc={ret.returncode}): {ret.stdout} {ret.stderr}')
    *_, interval, nbytes, bps = fields
    start, end = map(float, interval.split('-'))
    if end - start < IPERF_SECONDS - 0.5 or int(nbytes) <= 0:
        raise RuntimeError(f'iperf ended early: {lines[0]}')
    print(f'iperf: {int(bps) / 1e6:.1f} Mbit/s host to device', flush=True)


def interrupted(signum, _frame):
    raise TimeoutError(f'network test interrupted (signal {signum})')


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    mode = ap.add_mutually_exclusive_group(required=True)
    mode.add_argument('--uid')
    mode.add_argument('--http', action='store_true', help=argparse.SUPPRESS)
    args = ap.parse_args()
    # Each side bounds itself even if the other, or its caller, dies.
    signal.signal(signal.SIGALRM, interrupted)
    signal.signal(signal.SIGTERM, interrupted)
    signal.alarm(90)
    if args.http:
        print(os.getpid(), flush=True)
        if sys.stdin.readline().strip() != 'go':
            raise RuntimeError('network setup owner exited before starting HTTP')
        check_http()
        check_iperf()
    else:
        check_device(args.uid)
    signal.alarm(0)


if __name__ == '__main__':
    main()
