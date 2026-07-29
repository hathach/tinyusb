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
import shlex
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

ENUM_WAIT = 12       # s, uid wait after flash
ENUM_WAIT_RETRY = 8  # s, uid wait after a recovery reset/re-flash
SERIAL_WAIT = 6      # s, host-board serial-output wait

print_mutex = threading.Lock()
t0 = time.monotonic()


def say(msg: str) -> None:
    with print_mutex:
        print(f'[{time.monotonic() - t0:6.1f}s] {msg}', file=sys.__stdout__, flush=True)


def scan_usb() -> dict:
    """busport -> {'serial', 'vidpid', 'ino'} for every enumerated USB device. Only
    <bus>-<port>[.<port>...] dirs match (root hubs, named 'usbN' with no dash, are
    excluded: their fabricated PCI-address 'serial' and slow autosuspend-wake read
    cost 6-7s/scan on this rig). Keyed by busport, not serial: a serial can be
    shared by two different devices (e.g. an Espressif USB-Serial-JTAG bridge and
    the cafe TinyUSB device it flashes derive both from the same MAC) — collapsing
    them into one dict slot would silently drop whichever lost the race."""
    found = {}
    for f in glob.glob('/sys/bus/usb/devices/*-*/serial'):
        d = os.path.dirname(f)
        busport = os.path.basename(d)
        try:
            sn = open(f).read().strip().lower()
            vidpid = f'{open(d + "/idVendor").read().strip()}:{open(d + "/idProduct").read().strip()}'
            found[busport] = {'serial': sn, 'vidpid': vidpid, 'ino': os.stat(d + '/').st_ino}
        except OSError:
            continue
    return found


def find_usb(uid: str, devs: dict | None = None):
    """Locate a flasher probe by uid, excluding VID cafe (TinyUSB DUT firmware): a
    probe's uid can coincidentally equal its DUT's (Espressif USB-Serial-JTAG
    bridges derive both from the same MAC), and the DUT is never the probe.

    J-Link zero-pads numeric serials (681295394 -> 000681295394): an all-digit uid
    matches an all-digit serial only when that serial equals the uid zero-padded to
    the serial's own length (leading zeros only) — never when the zero-stripped uid
    is empty, so a placeholder serial (metro_m4_express's probe legitimately reports
    '123456') can't be mistaken for an unrelated device."""
    devs = devs if devs is not None else scan_usb()
    u = uid.lower()
    candidates = [(bp, dev) for bp, dev in devs.items() if not dev['vidpid'].startswith('cafe:')]
    for bp, dev in candidates:
        if dev['serial'] == u:
            return bp, dev['vidpid'], dev['ino']
    stripped = u.lstrip('0')
    if u.isdigit() and stripped:
        for bp, dev in candidates:
            s = dev['serial']
            if s.isdigit() and s == stripped.zfill(len(s)):
                return bp, dev['vidpid'], dev['ino']
    return None


def find_device(uid: str, pid: str | None):
    """Board-online check: TinyUSB device (idVendor cafe) with this uid, optionally
    PID-pinned. VID cafe keeps an Espressif USB-Serial-JTAG (303a) sharing the MAC
    serial from false-passing."""
    for busport, dev in scan_usb().items():
        if (dev['serial'] == uid.lower() and dev['vidpid'].startswith('cafe:')
                and (pid is None or dev['vidpid'].endswith(pid))):
            return busport, dev['vidpid'], dev['ino']
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
        # NB: conflates a held flock with open() failures (EACCES/EROFS/ENOSPC) —
        # benign while everything on the rig runs as one uid; a cross-uid setup
        # would need flock_nb to distinguish the two
        info = hil_lock.read_record(name)
        return json.dumps(info) if info else 'unknown holder'
    if not hil_lock.write_record(fh, 'pool_check'):
        # an invisible lock (flock held, no record) is worse than no lock: status
        # can't show us and release can't recognize the protected holder — bail out
        hil_lock.clear_record(fh)
        fh.close()
        return 'ERROR: holder record write failed (lock dir unwritable?)'
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
    try:
        # bounded: the sysfs authorized store can block in D state on a wedged
        # device, and this runs while the board's (release-protected) flock is held
        subprocess.run(['sudo', '-n', str(USB_RECOVER), 'authorized', busport],
                       capture_output=True, text=True, timeout=30)
    except subprocess.TimeoutExpired:
        return False
    deadline = time.monotonic() + 20
    while time.monotonic() < deadline:
        post = find_usb(uid)
        if post and (pre is None or post[2] != pre[2]):
            return True
        time.sleep(0.5)
    return False


def resolve_variant(board: dict, example: str, note: list | None = None) -> str:
    """Build-dir variant name for `example`: the first of the board's variants with
    already-built firmware, falling back to the board name. Notes the pick when it
    differs from the board name (e.g. nanoch32v203's build dir is variant
    'nanoch32v203-fsdev', not the board name)."""
    name = board['name']
    for v in board.get('variant') or [{'name': name}]:
        vn = v['name']
        if hil_flash.find_firmware(vn, example):
            if vn != name and note is not None:
                note.append(f'variant: {vn}')
            return vn
    return name


def pick_example(board: dict, note: list) -> tuple[str | None, str | None, str | None]:
    """(example, kind, variant) with built firmware for this board; kind is
    'device' (uid check) or 'host' (serial-output check); variant is the resolved
    build-dir variant that has it (see resolve_variant)."""
    tests = board.get('tests', {})
    only = tests.get('only', [])
    skip = set(tests.get('skip', []))  # config's known-broken examples: never pick one
    is_device = tests.get('device') or any(t.startswith('device/') for t in only)
    if is_device:
        cand = DEVICE_CANDIDATES + [t for t in only if t.startswith('device/') and t != 'device/usbtest']
        kind = 'device'
    else:
        cand = HOST_CANDIDATES + [t for t in only if t.startswith('host/')]
        kind = 'host'
    for ex in dict.fromkeys(cand):
        if ex in skip:
            continue
        variant = resolve_variant(board, ex, note)
        if hil_flash.find_firmware(variant, ex):
            return ex, kind, variant
    return None, kind, None


_pid_cache: dict[str, str | None] = {}


def get_expected_pid(example: str) -> str | None:
    """USB_PID for `example`'s device descriptor (examples/<example>/src/
    usb_descriptors.c, '#define USB_PID 0x....'), lowercased and without the 0x
    prefix to match sysfs idProduct. Cached per example; None (also cached) when
    the file or define isn't there — host examples have no usb_descriptors.c, and
    the caller must stay quiet rather than false-warn."""
    if example not in _pid_cache:
        pid = None
        try:
            text = (REPO_ROOT / 'examples' / example / 'src' / 'usb_descriptors.c').read_text()
            # optional parens as in tools/check_example_pids.py's parser
            m = re.search(r'#define\s+USB_PID\s+\(?\s*(0x[0-9a-fA-F]+)', text)
            if m:
                pid = m.group(1)[2:].lower()
        except OSError:
            pass
        _pid_cache[example] = pid
    return _pid_cache[example]


def call_flasher(fn, *fn_args) -> tuple[int, str]:
    """Run a hil_flash flash_*/reset_* backend, normalizing raises to a failure:
    several backends raise instead of returning nonzero (get_serial_dev
    RuntimeError when a bridge's /dev/serial/by-id node vanishes, config.env
    FileNotFoundError, .jlink script OSError) and an exception must not skip the
    caller's retry/recovery ladder. Returns (returncode, error line)."""
    try:
        ret = fn(*fn_args)
        if ret.returncode == 0:
            return 0, ''
        err = flash_error_line(hil_flash.cmd_stdout_text(ret.stdout))
        return ret.returncode, err or f'rc={ret.returncode}'
    except Exception as e:
        return -1, repr(e)[:90]


def flash(board: dict, example: str, variant: str, allow_recovery: bool, probe_port: str, note: list) -> bool:
    """Flash with one retry; on repeated failure soft-replug the probe and always
    make one final flash attempt afterward, regardless of whether the replug is
    confirmed — some probes (WCH-Link, ST-Link, CP210x, picoprobe) leave their
    sysfs kobject intact across an authorized toggle instead of dropping off the
    bus. Returns True on success."""
    fw = hil_flash.find_firmware(variant, example)
    fn = getattr(hil_flash, f'flash_{board["flasher"]["name"].lower()}')
    for attempt in range(3):
        if attempt == 2:
            if not (allow_recovery and probe_port):
                return False
            cur = find_usb(board['flasher']['uid'])
            if cur is None:
                # probe gone from the bus: its old busport may now hold an UNRELATED
                # device (bus renumbering) and the helper only checks occupancy, so
                # toggling would deauthorize an innocent fixture — skip the toggle
                note.append('probe vanished before toggle')
            else:
                say(f'{board["name"]:26} recovery: replugging probe {cur[0]} (authorized toggle)')
                if recover_probe(board['flasher']['uid'], cur[0]):
                    note.append('probe replugged')
                    time.sleep(2)  # udev recreates /dev/serial/by-id symlinks after re-enumeration
                else:
                    note.append('probe toggle unconfirmed')
        rc, err = call_flasher(fn, board, str(fw))
        if rc == 0:
            return True
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


def check_host_serial(board: dict, do_reset: bool = True, want_hello: bool = False) -> bytes | None:
    """Host-only boards never enumerate their uid (their USB port is the host side);
    aliveness = output on the flasher's UART bridge after a reset. A probe byte is
    written each poll so an echo-only firmware (board_test) also answers. Returns
    the first output chunk (b'' when silent, None when the port is absent/drops) so
    the caller can also judge WHAT answered — see boardtest_output().

    do_reset=False listens to the firmware as-is: used right after a flash whose
    own reset already started it — a second openocd/JLink session back-to-back on
    the same probe can fail transiently and leave the target halted."""
    import serial
    try:
        port = hil_flash.get_serial_dev(board['flasher']['uid'], None, None, 0)
        ser = serial.Serial(port, baudrate=115200, timeout=0.3, write_timeout=1)
    except Exception as e:
        say(f'{board["name"]:26} no flasher serial port: {e}')
        return None
    try:
        # flush BEFORE issuing the reset: pyserial's open-time flush is long past,
        # so this drops the pre-reset CDC backlog (which must not count as life)
        # while keeping the board's post-reset boot banner, which prints while the
        # reset tool is still tearing down and would be eaten by a post-reset flush
        ser.reset_input_buffer()
        if do_reset:
            getattr(hil_flash, f'reset_{board["flasher"]["name"].lower()}')(board)
        # collect the WHOLE window and judge content, not the first chunk: the
        # probe's CDC bridge has its own FIFO, so stale pre-flash output (e.g.
        # board_test hellos) can arrive after our host-side flush and must not
        # decide the verdict alone. Early-exit once non-board_test output proves
        # a real example is talking.
        data = b''
        deadline = time.monotonic() + SERIAL_WAIT
        while time.monotonic() < deadline:
            try:
                ser.write(b'U')
                data += ser.read(256)
            except serial.SerialTimeoutException:
                pass
            except serial.SerialException:
                return None  # port dropped mid-poll (bridge re-enumerating)
            # early-exit on the caller's positive signal: fresh board_test hello
            # (park verification) vs any non-board_test output (example liveness);
            # stale bridge-FIFO backlog of the OTHER kind must not end the window
            if want_hello:
                if b'Hello from TinyUSB' in data:
                    return data
            elif data and not boardtest_output(data):
                return data
        return data
    finally:
        ser.close()


def boardtest_output(data: bytes) -> bool:
    """True when (non-empty) serial output is recognizably ONLY board_test's: its
    periodic HELLO_STR and echoes of our b'U' pokes, nothing else. Any residue
    beyond that (an example banner, log lines) proves other firmware is talking,
    however much stale board_test backlog surrounds it. Used as a negative
    identity marker — after flashing a host example, board_test-only chatter
    means the flash silently didn't take (the host analog of the PID check)."""
    residue = data.replace(b'Hello from TinyUSB', b'')
    for junk in (b'U', b'\r', b'\n'):
        residue = residue.replace(junk, b'')
    return len(residue) == 0


def ensure_board_test(board: dict, variant: str):
    """board_test firmware path for this board's variant, building it via
    tools/build.py — the same invocation shape hil_test.build_board uses (board
    name, -D per build.args, variant defines/--build-name) — if absent."""
    fw = hil_flash.find_firmware(variant, 'device/board_test')
    if fw:
        return fw
    if board['flasher']['name'].lower() == 'esptool':
        # tools/build.py's espressif branch ignores -T and idf-builds EVERY example
        # (needing the ESP-IDF env we don't have) — never attempt it here
        return None
    name = board['name']
    variants = board.get('variant') or [{'name': name}]
    vcfg = next((v for v in variants if v['name'] == variant), None)
    if vcfg is None:
        # resolve_variant fell back to the bare board name; for a variants-only
        # board that build config doesn't exist — build the first real variant
        # (with its defines) rather than an rhport/speed setup the rig never uses
        vcfg = variants[0]
        variant = vcfg['name']
    cmd = [sys.executable, str(hil_flash.TINYUSB_ROOT / 'tools' / 'build.py'),
           '-b', name, '-T', 'board_test']
    for d in board.get('build', {}).get('args', []):
        cmd += ['-D', d]
    if variant != name:
        cmd += ['--build-name', variant]
    for d in vcfg.get('defines', []):
        cmd += ['-D', d]
    for tok in vcfg.get('flags', '').split():
        cmd += [f'--cflag={tok}']
    # via run_cmd: bounded (runs while the board's flock is held) AND process-group
    # killed on timeout — a bare subprocess.run timeout reaps only the build.py
    # wrapper, leaving cmake/ninja orphans mutating the build tree after we return
    r = hil_flash.run_cmd(shlex.join(cmd), cwd=str(hil_flash.TINYUSB_ROOT), timeout=300)
    if r.returncode != 0:
        return None
    # tools/build.py always writes to cmake-build/: look there too even when an
    # explicit -B narrowed the global search — this is OUR fresh build, not a
    # stale-candidate fallback
    return hil_flash.find_firmware(variant, 'device/board_test',
                                   roots=[hil_flash.build_dir, 'cmake-build'])


def host_alive(board: dict, note: list, flashed_example: bool = False) -> bool:
    """Serial aliveness with recovery: silent -> (build and) flash board_test (it
    hellos every second and echoes) -> recheck. Also cures a silent flash no-op
    that left the board crashed.

    With flashed_example=True (a host example was just flashed), board_test-shaped
    output FAILS the check: the parked image still talking means the example flash
    silently didn't take — the host analog of the device path's PID check."""
    data = check_host_serial(board)
    if data:
        if flashed_example and boardtest_output(data):
            note.append('board_test output after example flash: silent flash no-op')
            return False
        return True
    variant = resolve_variant(board, 'device/board_test', note)
    fw = ensure_board_test(board, variant)
    if fw is None:
        note.append('serial silent; board_test build failed')
        return False
    say(f'{board["name"]:26} recovery: serial silent, flashing board_test')
    rc, err = call_flasher(getattr(hil_flash, f'flash_{board["flasher"]["name"].lower()}'), board, str(fw))
    if rc != 0:
        note.append(f'serial silent; board_test flash failed: {err}')
        return False
    ok = bool(check_host_serial(board))
    if ok:
        note.append('recovered via board_test reflash')
    return ok


def device_recover_and_check(board: dict, example: str, old_ino, note: list, row: dict, seen: dict) -> bool:
    """Wait for the flashed board's uid to re-enumerate; on timeout, try one board
    reset (skipped for flashers with no hardware reset — see hil_flash.RESET_NOOP,
    it would just burn the wait) and wait again.

    The PID policy is deliberately asymmetric. Pre-reset, the re-enumeration was
    caused by the flash itself, so a PID mismatch most likely means the build dir
    is stale (the flash DID write what find_firmware found) — warn, don't fail.
    Post-reset, the re-enumeration proves nothing about the flash (the reset alone
    explains it), so a mismatch is treated as a silent flash no-op and fails; an
    unknown expected PID scores ok with a 'pid unverified' note in both paths."""
    name = board['name']
    expected_pid = get_expected_pid(example)

    def seen_hit(hit):
        seen[board['uid']] = {'name': name, 'busport': hit[0], 'when': time.strftime('%Y-%m-%d %H:%M')}

    hit = wait_device(board['uid'], None, old_ino, ENUM_WAIT)
    if hit:
        row['device'] = f'✅ {hit[1]}'
        if expected_pid is None:
            note.append('pid unverified')
        elif not hit[1].endswith(expected_pid):
            note.append(f'⚠ pid {hit[1]}, source says {expected_pid}: stale build or silent flash no-op')
        seen_hit(hit)
        return True

    flasher_name = board['flasher']['name'].lower()
    if flasher_name in hil_flash.RESET_NOOP:
        note.append(f'no hardware reset available for {flasher_name}')
        row['device'] = '❌ not enumerated'
        return False

    say(f'{name:26} recovery: uid not up, resetting board')
    rc, err = call_flasher(getattr(hil_flash, f'reset_{flasher_name}'), board)
    if rc != 0:
        note.append(f'reset failed: {err}')
    hit = wait_device(board['uid'], None, old_ino, ENUM_WAIT_RETRY)
    if not hit:
        row['device'] = '❌ not enumerated'
        note.append('reset did not help')
        return False
    if expected_pid is None:
        row['device'] = f'✅ {hit[1]}'
        note.append('reset recovered (pid unverified)')
        seen_hit(hit)
        return True
    if hit[1].endswith(expected_pid):
        row['device'] = f'✅ {hit[1]}'
        note.append('reset recovered')
        seen_hit(hit)
        return True
    row['device'] = f'❌ {hit[1]}'
    note.append(f'reset recovered wrong pid, expected {expected_pid}: silent flash no-op')
    return False


def check_board(board: dict, args, allow_recovery: bool, seen: dict) -> dict:
    name = board['name']
    row = {'name': name, 'probe': '❌ missing', 'flash': '–', 'device': '–', 'note': [], 'status': 'bad'}
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

    example, kind, variant = pick_example(board, note)
    if kind == 'host':
        note.append('host-only board')

    if args.scan_only:
        hit = find_device(board['uid'], None)
        row['device'] = f'✅ {hit[1]}' if hit else '– (scan)'
        # the probe check DID run: a missing probe is a real failure even in scan mode
        row['status'] = 'skipped' if probe else 'bad'
        if probe:
            say(f'{name:26} probe ✅ {probe[0]}' + (f'  device {hit[1]}' if hit else ''))
        return row
    if not probe:
        return row  # status stays 'bad'
    if example is None:
        note.append('no firmware built')
        if kind != 'host':
            row['status'] = 'skipped'
            say(f'{name:26} probe ✅ {probe[0]}  (no firmware built, flash skipped)')
            return row
        # host-only board: aliveness is still checkable without flashing — reset and
        # listen to whatever firmware is on it (the parked board_test echoes and
        # prints a periodic hello on the flasher UART)

    lk = lock_board(name)
    if isinstance(lk, str):
        if lk.startswith('ERROR:'):  # environment failure, not a held lock
            row['flash'] = '❌ lock'
            row['status'] = 'bad'
        else:
            row['flash'] = '🔒 locked'
            row['status'] = 'locked'
        note.append(lk)
        say(f'{name:26} locked: {lk}')
        return row
    try:
        if example is None:  # host-only without firmware: UART-only aliveness check
            ok = host_alive(board, note)
            row['device'] = '✅ serial out' if ok else '❌ no serial out'
            row['status'] = 'ok' if ok else 'bad'
            say(f'{name:26} –  {row["device"]}  (existing firmware)')
            return row

        pre = find_device(board['uid'], None)
        old_ino = pre[2] if pre else None

        try:
            if not flash(board, example, variant, allow_recovery, probe[0], note):
                row['flash'] = f'❌ {Path(example).name}'
                row['status'] = 'bad'
                say(f'{name:26} flash FAILED ({example})')
                return row
            row['flash'] = f'✅ {Path(example).name}'

            if kind == 'host':
                ok = host_alive(board, note, flashed_example=True)
                row['device'] = '✅ serial out' if ok else '❌ no serial out'
            else:
                ok = device_recover_and_check(board, example, old_ino, note, row, seen)
            row['status'] = 'ok' if ok else 'bad'
            say(f'{name:26} {row["flash"]}  {row["device"]}')
            return row
        finally:
            # teardown for EVERY path that attempted a flash (a failed programmer op
            # can still have erased/half-written the target): re-park while the
            # board lock is still held
            if not args.no_park:
                park_board(board, kind, row, note)
    finally:
        unlock_board(lk)


def park_board(board: dict, kind: str, row: dict, note: list) -> None:
    """Re-park with board_test, building it if absent (ensure_board_test), and
    VERIFY it took: board_test never enumerates USB, so a device board's cafe
    device must drop off the bus, and a host board must answer with board_test's
    own output — a rc=0 park that changed nothing (silent no-op) must not pass.
    A board left unparked makes the row bad, with one documented exception:
    esptool boards whose ESP-IDF builds have never included board_test (absence
    is the family norm, noted but not a board fault)."""
    variant = resolve_variant(board, 'device/board_test', note)
    fw = ensure_board_test(board, variant)
    if fw is None:
        if board['flasher']['name'].lower() == 'esptool':
            note.append('park unavailable (ESP-IDF builds carry no board_test)')
        else:
            note.append('unparked: board_test unavailable (build failed/timed out)')
            row['status'] = 'bad'
        return
    rc, err = call_flasher(getattr(hil_flash, f'flash_{board["flasher"]["name"].lower()}'),
                           board, str(fw))
    if rc != 0:
        note.append(f'park flash failed: {err}')
        row['status'] = 'bad'
        return
    if kind == 'host':
        # no second reset (the park flash's own reset already started board_test);
        # POSITIVE marker: its hello must appear — stale example output may still
        # drain from the probe bridge's FIFO alongside it and is not disqualifying
        data = check_host_serial(board, do_reset=False, want_hello=True)
        if not (data and b'Hello from TinyUSB' in data):
            note.append('park unverified: no board_test output')
            row['status'] = 'bad'
        return
    deadline = time.monotonic() + 6
    while time.monotonic() < deadline:
        if find_device(board['uid'], None) is None:
            return
        time.sleep(0.5)
    note.append('park unverified: device still enumerated')
    row['status'] = 'bad'


def check_board_safe(board: dict, args, allow_recovery: bool, seen: dict) -> dict:
    """Isolate one board's exceptions: a crashing worker must not discard every
    other board's row, the table, the topology, and the seen-cache write."""
    try:
        return check_board(board, args, allow_recovery, seen)
    except Exception as e:
        name = board.get('name', '?')
        say(f'{name:26} INTERNAL ERROR: {e!r}')
        return {'name': name, 'probe': '–', 'flash': '–', 'device': '❌ error',
                'note': [repr(e)[:120]], 'status': 'bad'}


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


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument('config', nargs='?', help='HIL config json (default: by hostname)')
    parser.add_argument('-b', '--board', action='append', default=[], help='only these boards')
    parser.add_argument('-B', '--build-dir', default=None,
                        help='firmware parent dir, searched EXCLUSIVELY when given '
                             '(default: examples, plus cmake-build as fallback)')
    parser.add_argument('--scan-only', action='store_true',
                        help='USB presence scan only: no locks, no flashing')
    parser.add_argument('--no-park', action='store_true',
                        help='leave the light example running (default: park with board_test)')
    # no cross-process flash budget with a concurrent hil_test.py run yet (would need
    # a file-lock budget in hil_lock; hil_test uses in-process semaphores) — keep modest
    parser.add_argument('-j', '--jobs', type=int, default=4)
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

    hil_flash.build_dir = args.build_dir or 'examples'
    hil_flash.verbose = args.verbose
    if args.build_dir is None:
        # default mode: search both standard layouts (cmake-build/ from tools/build.py
        # + ESP-IDF, examples/ from manual builds). An EXPLICIT -B is exclusive — the
        # caller named an artifact tree, so a miss must report, not silently flash an
        # older build from elsewhere. hil_test's -B is likewise untouched by this.
        hil_flash.EXTRA_BUILD_DIRS = ['cmake-build', 'examples']
    allow_recovery = not args.scan_only and can_recover()
    seen = {}
    try:
        loaded = json.loads(SEEN_CACHE.read_text())
        if isinstance(loaded, dict):  # tolerate a torn/hand-edited cache
            seen = {k: v for k, v in loaded.items() if isinstance(v, dict)}
    except (OSError, ValueError):
        pass

    say(f'pool check: host {host}, config {cfg_path.name}, {len(boards)} boards, '
        f'{"scan-only" if args.scan_only else f"flash via {args.build_dir}/cmake-build-<board>"}'
        f'{"" if allow_recovery or args.scan_only else ", recovery unavailable (no sudo -n / usb_recover.sh)"}')

    if args.verbose:
        rows = [check_board_safe(b, args, allow_recovery, seen) for b in boards]
    else:
        with io.StringIO() as spool, ThreadPoolExecutor(max_workers=args.jobs) as pool:
            sys.stdout = spool  # silence hil_flash's COMMAND FAILED dumps; say() uses __stdout__
            try:
                rows = list(pool.map(lambda b: check_board_safe(b, args, allow_recovery, seen), boards))
            finally:
                sys.stdout = sys.__stdout__

    try:
        SEEN_CACHE.parent.mkdir(parents=True, exist_ok=True)
        tmp = SEEN_CACHE.with_suffix('.json.tmp')
        tmp.write_text(json.dumps(seen, indent=1, sort_keys=True) + '\n')
        tmp.replace(SEEN_CACHE)  # atomic: a killed run can't tear the cache
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

    counts = {'ok': 0, 'bad': 0, 'skipped': 0, 'locked': 0}
    for r in rows:
        counts[r.get('status', 'bad')] += 1
    print(f'\n{counts["ok"]} ok · {counts["bad"]} bad · {counts["skipped"]} skipped · {counts["locked"]} locked '
          f'· in {time.monotonic() - t0:.0f}s')
    sys.exit(min(counts['bad'], 125))


if __name__ == '__main__':
    main()
