#!/usr/bin/env python3
"""Quick HIL pool health check.

For every board in the rig's HIL config: is the flash probe on the USB bus, does a
light example flash, and does the board's USB device (uid) come back up? Applies
only per-device-safe recovery (probe authorized-toggle, board reset/re-flash) and
prints a markdown summary table. Boards are flock'd (hil_lock.py protocol) so a
concurrent CI hil_test run is never disturbed; locked boards are reported and
skipped.

Config is picked by hostname unless given: ci -> tinyusb.json, tusb (hifiphile
rig) -> hfp.json, anything else is a dev PC -> local.json.

Must live in <repo>/.claude/skills/hil/ (imports test/hil/hil_lock.py and
test/hil/hil_flash.py, uses the repo's usb_recover.sh).
"""

import argparse
import io
import json
import glob
import os
import re
import socket
import subprocess
import sys
import threading
import time
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(REPO_ROOT / 'test' / 'hil'))

import hil_lock
import hil_flash

USB_RECOVER = REPO_ROOT / '.claude' / 'skills' / 'usb-kernel-recover' / 'scripts' / 'usb_recover.sh'
SEEN_CACHE = Path.home() / '.cache' / 'tinyusb-hil' / 'pool_seen.json'
CONFIG_BY_HOST = {'ci': 'tinyusb.json', 'tusb': 'hfp.json'}  # anything else: dev PC -> local.json

# light-example preference; first built wins
DEVICE_CANDIDATES = ['device/dfu_runtime', 'device/cdc_msc', 'device/cdc_msc_freertos',
                     'device/hid_composite_freertos', 'device/cdc_dual_ports']
HOST_CANDIDATES = ['host/device_info', 'host/cdc_msc_hid', 'host/msc_file_explorer_freertos']
# expected PID (unique per example, see usb_descriptors.c). A mismatch is still "alive"
# but flags a stale build or a silent flash no-op (probe reset the MCU without writing).
EXAMPLE_PID = {'device/dfu_runtime': '400c'}

ENUM_WAIT = 12       # s, uid wait after flash
ENUM_WAIT_RETRY = 8  # s, uid wait after a recovery reset/re-flash
SERIAL_WAIT = 6      # s, host-board serial-output wait

print_mutex = threading.Lock()
t0 = time.monotonic()


def say(msg: str) -> None:
    with print_mutex:
        print(f'[{time.monotonic() - t0:6.1f}s] {msg}', file=sys.__stdout__, flush=True)


def scan_usb() -> dict:
    """serial(lower) -> (busport, 'vid:pid', inode) for every enumerated USB device."""
    found = {}
    for f in glob.glob('/sys/bus/usb/devices/*/serial'):
        d = os.path.dirname(f)
        try:
            sn = open(f).read().strip().lower()
            vidpid = f'{open(d + "/idVendor").read().strip()}:{open(d + "/idProduct").read().strip()}'
            found[sn] = (os.path.basename(d), vidpid, os.stat(d + '/').st_ino)
        except OSError:
            continue
    return found


def find_usb(uid: str, devs: dict | None = None):
    """Locate uid on the bus. J-Link zero-pads numeric serials (681295394 ->
    000681295394), so all-digit uids match with leading zeros stripped."""
    devs = devs if devs is not None else scan_usb()
    u = uid.lower()
    if u in devs:
        return devs[u]
    if u.isdigit():
        for s, v in devs.items():
            if s.isdigit() and s.lstrip('0') == u.lstrip('0'):
                return v
    return None


def find_device(uid: str, pid: str | None):
    """Board-online check: TinyUSB device (idVendor cafe) with this uid, optionally
    PID-pinned. VID cafe keeps an Espressif USB-Serial-JTAG (303a) sharing the MAC
    serial from false-passing."""
    for sn, (busport, vidpid, ino) in scan_usb().items():
        if sn == uid.lower() and vidpid.startswith('cafe:') and (pid is None or vidpid.endswith(pid)):
            return busport, vidpid, ino
    return None


def wait_device(uid: str, pid: str | None, old_ino, budget: float):
    """Wait for the board's device with a NEW sysfs inode (flash resets the MCU, so a
    genuine flash must re-enumerate; the inode is the re-enumeration marker)."""
    deadline = time.monotonic() + budget
    while time.monotonic() < deadline:
        hit = find_device(uid, pid)
        if hit and hit[2] != old_ino:
            return hit
        time.sleep(0.5)
    return None


def lock_board(name: str):
    """Nonblocking flock per hil_lock.py protocol. Returns handle, or a str with
    the holder's info when the board is locked elsewhere. Board locks are ALWAYS
    respected: a held board is reported as locked and skipped — never waited on,
    and there is deliberately no bypass here."""
    os.makedirs(hil_lock.BOARD_LOCK_DIR, exist_ok=True)
    try:
        fh = hil_lock.flock_nb(name)
    except OSError:
        info = hil_lock.read_record(name)
        return json.dumps(info) if info else 'unknown holder'
    hil_lock.write_record(fh, 'pool_check')
    return fh


def unlock_board(fh) -> None:
    hil_lock.clear_record(fh)
    fh.close()


def can_recover() -> bool:
    if not USB_RECOVER.is_file():
        return False
    r = subprocess.run(['sudo', '-n', 'true'], capture_output=True)
    return r.returncode == 0


def recover_probe(uid: str, busport: str) -> bool:
    """Soft-replug an enumerated-but-wedged probe: deauthorize+reauthorize (no VBUS
    cut, touches only this device). Success = the probe re-enumerated (new sysfs
    inode), not the helper's exit code (observed to flake while the toggle worked).
    J-Links respond with a full disconnect and can stay off the bus for >8 s."""
    pre = find_usb(uid)
    subprocess.run(['sudo', '-n', str(USB_RECOVER), 'authorized', busport],
                   capture_output=True, text=True)
    deadline = time.monotonic() + 20
    while time.monotonic() < deadline:
        post = find_usb(uid)
        if post and (pre is None or post[2] != pre[2]):
            return True
        time.sleep(0.5)
    return False


def pick_example(board: dict) -> tuple[str | None, str | None]:
    """(example, kind) with built firmware for this board's first variant; kind is
    'device' (uid check) or 'host' (serial-output check)."""
    tests = board.get('tests', {})
    only = tests.get('only', [])
    is_device = tests.get('device') or any(t.startswith('device/') for t in only)
    if is_device:
        cand = DEVICE_CANDIDATES + [t for t in only if t.startswith('device/') and t != 'device/usbtest']
        kind = 'device'
    else:
        cand = HOST_CANDIDATES + [t for t in only if t.startswith('host/')]
        kind = 'host'
    variant = (board.get('variant') or [{'name': board['name']}])[0]['name']
    for ex in dict.fromkeys(cand):
        if hil_flash.find_firmware(variant, ex):
            return ex, kind
    return None, kind


def flash(board: dict, example: str, allow_recovery: bool, probe_port: str, note: list) -> bool:
    """Flash with one retry; on repeated failure soft-replug the probe and try once
    more. Returns True on success."""
    variant = (board.get('variant') or [{'name': board['name']}])[0]['name']
    fw = hil_flash.find_firmware(variant, example)
    fn = getattr(hil_flash, f'flash_{board["flasher"]["name"].lower()}')
    for attempt in range(3):
        if attempt == 2:
            if not (allow_recovery and probe_port):
                return False
            say(f'{board["name"]:26} recovery: replugging probe {probe_port} (authorized toggle)')
            if not recover_probe(board['flasher']['uid'], probe_port):
                note.append('probe replug failed')
                return False
            note.append('probe replugged')
        ret = fn(board, str(fw))
        if ret.returncode == 0:
            return True
        err = flash_error_line(hil_flash.cmd_stdout_text(ret.stdout)) or f'rc={ret.returncode}'
        if attempt == 0:
            say(f'{board["name"]:26} flash retry: {err}')
        else:
            note.append(f'flash: {err}')
    return False


def flash_error_line(out: str) -> str:
    """Most informative line of a failed flash's output: last error-looking line,
    else the last non-empty one."""
    lines = [l.strip() for l in out.splitlines() if l.strip()]
    for l in reversed(lines):
        if any(k in l.lower() for k in ('error', 'fail', 'unknown', 'cannot', 'timeout',
                                        'no valid', 'not found', 'unable')):
            return l[:90]
    return lines[-1][:90] if lines else ''


def check_host_serial(board: dict) -> bool:
    """Host-only boards never enumerate their uid (their USB port is the host side);
    aliveness = any output on the flasher's UART bridge after a reset. A probe byte
    is written each poll so an echo-only firmware (board_test) also counts."""
    import serial
    try:
        port = hil_flash.get_serial_dev(board['flasher']['uid'], None, None, 0)
        ser = serial.Serial(port, baudrate=115200, timeout=0.3, write_timeout=1)
    except Exception as e:
        say(f'{board["name"]:26} no flasher serial port: {e}')
        return False
    try:
        getattr(hil_flash, f'reset_{board["flasher"]["name"].lower()}')(board)
        deadline = time.monotonic() + SERIAL_WAIT
        while time.monotonic() < deadline:
            try:
                ser.write(b'U')
            except serial.SerialTimeoutException:
                pass
            if ser.read(64):
                return True
        return False
    finally:
        ser.close()


def ensure_board_test(board: dict):
    """board_test firmware path for this board, building it if absent."""
    variant = (board.get('variant') or [{'name': board['name']}])[0]['name']
    fw = hil_flash.find_firmware(variant, 'device/board_test')
    if fw:
        return fw
    bdir = hil_flash.TINYUSB_ROOT / hil_flash.build_dir / f'cmake-build-{variant}'
    src = hil_flash.TINYUSB_ROOT / 'examples'
    if not (bdir / 'CMakeCache.txt').exists():
        r = subprocess.run(['cmake', '-B', str(bdir), f'-DBOARD={board["name"]}', '-G', 'Ninja',
                            '-DCMAKE_BUILD_TYPE=MinSizeRel', str(src)], capture_output=True, text=True)
        if r.returncode != 0:
            return None
    subprocess.run(['cmake', '--build', str(bdir), '--target', 'board_test'],
                   capture_output=True, text=True)
    return hil_flash.find_firmware(variant, 'device/board_test')


def host_alive(board: dict, note: list) -> bool:
    """Serial aliveness with recovery: silent -> (build and) flash board_test (it
    hellos every second and echoes) -> recheck. Also cures a silent flash no-op
    that left the board crashed."""
    if check_host_serial(board):
        return True
    fw = ensure_board_test(board)
    if fw is None:
        note.append('serial silent; board_test build failed')
        return False
    say(f'{board["name"]:26} recovery: serial silent, flashing board_test')
    ret = getattr(hil_flash, f'flash_{board["flasher"]["name"].lower()}')(board, str(fw))
    if ret.returncode != 0:
        note.append('serial silent; board_test flash failed')
        return False
    note.append('recovered via board_test reflash')
    return check_host_serial(board)


def check_board(board: dict, args, allow_recovery: bool, seen: dict) -> dict:
    name = board['name']
    row = {'name': name, 'probe': '❌ missing', 'flash': '–', 'device': '–', 'note': []}
    note = row['note']

    probe = find_usb(board['flasher']['uid'])
    if probe:
        row['probe'] = f'✅ {probe[0]}'
        seen[board['flasher']['uid']] = {'name': f'{name} probe', 'busport': probe[0],
                                         'when': time.strftime('%Y-%m-%d %H:%M')}
    else:
        last = seen.get(board['flasher']['uid'])
        note.append(f'probe last seen {last["busport"]} {last["when"]}' if last
                    else 'probe never seen by pool_check')
        say(f'{name:26} probe MISSING ({board["flasher"]["name"]} {board["flasher"]["uid"]})')

    example, kind = pick_example(board)
    if kind == 'host':
        note.append('host-only board')

    if args.scan_only:
        hit = find_device(board['uid'], None)
        row['device'] = f'✅ {hit[1]}' if hit else '– (scan)'
        if probe:
            say(f'{name:26} probe ✅ {probe[0]}' + (f'  device {hit[1]}' if hit else ''))
        return row
    if not probe:
        return row
    if example is None:
        note.append('no firmware built')
        if kind != 'host':
            say(f'{name:26} probe ✅ {probe[0]}  (no firmware built, flash skipped)')
            return row
        # host-only board: aliveness is still checkable without flashing — reset and
        # listen to whatever firmware is on it (the parked board_test echoes and
        # prints a periodic hello on the flasher UART)

    lk = lock_board(name)
    if isinstance(lk, str):
        row['flash'] = '🔒 locked'
        note.append(lk)
        say(f'{name:26} locked: {lk}')
        return row
    try:
        if example is None:  # host-only without firmware: UART-only aliveness check
            ok = host_alive(board, note)
            row['device'] = '✅ serial out' if ok else '❌ no serial out'
            say(f'{name:26} –  {row["device"]}  (existing firmware)')
            return row

        pre = find_device(board['uid'], None)
        old_ino = pre[2] if pre else None

        if not flash(board, example, allow_recovery, probe[0], note):
            row['flash'] = f'❌ {Path(example).name}'
            say(f'{name:26} flash FAILED ({example})')
            return row
        row['flash'] = f'✅ {Path(example).name}'

        if kind == 'host':
            ok = host_alive(board, note)
            row['device'] = '✅ serial out' if ok else '❌ no serial out'
        else:
            hit = wait_device(board['uid'], None, old_ino, ENUM_WAIT)
            if not hit:
                # board recovery: re-reset via the probe, then wait again
                say(f'{name:26} recovery: uid not up, resetting board')
                getattr(hil_flash, f'reset_{board["flasher"]["name"].lower()}')(board)
                hit = wait_device(board['uid'], None, old_ino, ENUM_WAIT_RETRY)
                note.append('reset recovered' if hit else 'reset did not help')
            if hit:
                row['device'] = f'✅ {hit[1]}'
                expected = EXAMPLE_PID.get(example)
                if expected and not hit[1].endswith(expected):
                    note.append(f'⚠ pid {hit[1]}, source says {expected}: stale build or silent flash no-op')
                seen[board['uid']] = {'name': name, 'busport': hit[0],
                                      'when': time.strftime('%Y-%m-%d %H:%M')}
            else:
                row['device'] = '❌ not enumerated'
        ok = row['device'].startswith('✅')
        say(f'{name:26} {row["flash"]}  {row["device"]}')

        if not args.no_park and ok:
            variant = (board.get('variant') or [{'name': name}])[0]['name']
            if hil_flash.find_firmware(variant, 'device/board_test'):
                ret = getattr(hil_flash, f'flash_{board["flasher"]["name"].lower()}')(
                    board, str(hil_flash.find_firmware(variant, 'device/board_test')))
                if ret.returncode != 0:
                    note.append('park flash failed')
        return row
    finally:
        unlock_board(lk)


def controller_summary() -> list[str]:
    """USB topology: controller (PCI addr, vendor) -> bus -> root-port subtree device
    counts (hubs included, interfaces/root hubs not). Bus numbers renumber every boot;
    PCI addresses and root-port numbers are stable."""
    vendor_names = {'0x1022': 'AMD', '0x1912': 'Renesas', '0x8086': 'Intel', '0x1b21': 'ASMedia'}
    ctrl = {}
    for root in glob.glob('/sys/bus/usb/devices/usb*'):
        bus = int(os.path.basename(root)[3:])
        m = re.findall(r'[0-9a-f]{4}:[0-9a-f]{2}:[0-9a-f]{2}\.[0-9a-f]', os.path.realpath(root))
        pci = m[-1] if m else '?'
        c = ctrl.setdefault(pci, {'vendor': '?', 'buses': {}})
        subtrees = {}
        for d in glob.glob(f'/sys/bus/usb/devices/{bus}-*'):
            b = os.path.basename(d)
            if ':' in b:
                continue
            subtrees[b.split('.')[0]] = subtrees.get(b.split('.')[0], 0) + 1
        c['buses'][bus] = subtrees
        try:
            vid = open(f'/sys/bus/pci/devices/{pci}/vendor').read().strip()
            c['vendor'] = vendor_names.get(vid, vid)
        except OSError:
            pass

    lines = []
    for pci, c in sorted(ctrl.items()):
        lines.append(f'{pci} ({c["vendor"]})')
        for bus, subtrees in sorted(c['buses'].items()):
            detail = '   '.join(f'{k}: {n} dev' for k, n in
                                sorted(subtrees.items(), key=lambda i: int(i[0].split('-')[1])))
            lines.append(f'  bus {bus}: {sum(subtrees.values())} devices'
                         + (f'   {detail}' if detail else ''))
    return lines


def healthy(row: dict) -> bool:
    return not (row['probe'].startswith('❌') or row['flash'].startswith('❌')
                or row['device'].startswith('❌'))


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument('config', nargs='?', help='HIL config json (default: by hostname)')
    parser.add_argument('-b', '--board', action='append', default=[], help='only these boards')
    parser.add_argument('-B', '--build-dir', default='examples',
                        help='firmware parent dir, as hil_test -B (default: examples)')
    parser.add_argument('--scan-only', action='store_true',
                        help='USB presence scan only: no locks, no flashing')
    parser.add_argument('--no-park', action='store_true',
                        help='leave the light example running (default: park with board_test)')
    parser.add_argument('-j', '--jobs', type=int, default=6)
    parser.add_argument('-v', '--verbose', action='store_true')
    args = parser.parse_args()

    host = socket.gethostname()
    cfg_name = args.config or CONFIG_BY_HOST.get(host, 'local.json')
    cfg_path = Path(cfg_name)
    if not cfg_path.exists():
        cfg_path = REPO_ROOT / 'test' / 'hil' / cfg_name
    if not cfg_path.exists():
        sys.exit(f'config not found: {cfg_name} (host {host}; dev PCs need test/hil/local.json)')
    with cfg_path.open() as f:
        config = json.load(f)

    boards = list(config['boards'])  # boards-skip (parked hardware) is not scanned by default
    if args.board:
        boards += config.get('boards-skip', [])  # explicitly named parked boards are fair game
        unknown = set(args.board) - {b['name'] for b in boards}
        if unknown:
            sys.exit(f'board(s) not in {cfg_path.name}: {", ".join(sorted(unknown))}')
        boards = [b for b in boards if b['name'] in args.board]

    hil_flash.build_dir = args.build_dir
    hil_flash.verbose = args.verbose
    allow_recovery = not args.scan_only and can_recover()
    seen = {}
    try:
        seen = json.loads(SEEN_CACHE.read_text())
    except (OSError, ValueError):
        pass

    say(f'pool check: host {host}, config {cfg_path.name}, {len(boards)} boards, '
        f'{"scan-only" if args.scan_only else f"flash via {args.build_dir}/cmake-build-<board>"}'
        f'{"" if allow_recovery or args.scan_only else ", recovery unavailable (no sudo -n / usb_recover.sh)"}')

    if args.verbose:
        rows = [check_board(b, args, allow_recovery, seen) for b in boards]
    else:
        with io.StringIO() as spool, ThreadPoolExecutor(max_workers=args.jobs) as pool:
            sys.stdout = spool  # silence hil_test's COMMAND FAILED dumps; say() uses __stdout__
            try:
                rows = list(pool.map(lambda b: check_board(b, args, allow_recovery, seen), boards))
            finally:
                sys.stdout = sys.__stdout__

    try:
        SEEN_CACHE.parent.mkdir(parents=True, exist_ok=True)
        SEEN_CACHE.write_text(json.dumps(seen, indent=1, sort_keys=True) + '\n')
    except OSError:
        pass

    headers = ['Board', 'Probe', 'Flash', 'Device', 'Note']
    cells = [[r['name'], r['probe'], r['flash'], r['device'], '; '.join(r['note'])] for r in rows]
    widths = [max(len(h), *(len(c[i]) for c in cells)) if cells else len(h)
              for i, h in enumerate(headers)]
    line = lambda vals: '| ' + ' | '.join(v.ljust(w) for v, w in zip(vals, widths)) + ' |'
    print()
    print(line(headers))
    print('|' + '|'.join('-' * (w + 2) for w in widths) + '|')
    for c in cells:
        print(line(c))

    print('\nUSB topology (controller → root-port subtree):')
    for line in controller_summary():
        print(f'  {line}')

    bad = [r for r in rows if not healthy(r)]
    locked = sum(1 for r in rows if r['flash'].startswith('🔒'))
    print(f'\n{len(rows) - len(bad) - locked} healthy · {len(bad)} unhealthy · {locked} locked '
          f'· in {time.monotonic() - t0:.0f}s')
    sys.exit(min(len(bad), 125))


if __name__ == '__main__':
    main()
