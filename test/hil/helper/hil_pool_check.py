#!/usr/bin/env python3
"""Quick HIL pool health check.

For every board in the rig's HIL config: is the flash probe on the USB bus, does a
light example flash, and does the board's USB device (uid) come back up? Missing
firmware is BUILT on the spot through the build contract (check_build.py) — never
skipped; --no-build opts out, and a named boards-skip board must be prebuilt. No recovery: a wedged probe or board is
reported, and recovering it is usb-kernel-recover's job. Prints a markdown summary
table. Row statuses: ok (flashed and verified; under --scan-only: probe present —
the scan checks presence only), flash-failed (firmware delivery failed: probe
missing, build failed, flasher error, silent flash no-op, park not verified),
failed (the check ran but did not verify: flashed with no enumeration/serial, or
the check itself errored), locked (board flock held by another process —
reported, never waited on or bypassed).

Config is picked by hostname unless given: ci -> tinyusb.json, tusb (hifiphile
rig) -> hfp.json, anything else is a dev PC -> local.json.

Lives in test/hil/helper/ beside hil_lock.py; imports it and hil_flash.
"""

import argparse
import io
import json
import os
import re
import shlex
import shutil
import socket
import sys
import threading
import time
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))  # hil_flash + the helper package
import hil_flash
from helper import hil_lock, hil_report, hil_util

REPO_ROOT = hil_util.TINYUSB_ROOT
CHECK_BUILD = REPO_ROOT / '.claude' / 'skills' / 'build' / 'scripts' / 'check_build.py'
CONFIG_BY_HOST = {'ci': 'tinyusb.json', 'tusb': 'hfp.json'}  # anything else: dev PC -> local.json

# light-example preference; first built wins
DEVICE_CANDIDATES = ['device/dfu_runtime', 'device/cdc_msc', 'device/cdc_msc_freertos',
                     'device/hid_composite_freertos', 'device/cdc_dual_ports']
HOST_CANDIDATES = ['host/device_info', 'host/cdc_msc_hid', 'host/msc_file_explorer_freertos']

ENUM_WAIT = 12       # s, uid wait after flash
SERIAL_WAIT = 6      # s, host-board serial-output wait
BUILD_TIMEOUT = 1800  # s, one board's light images for every roster variant, first configure included

print_mutex = threading.Lock()
t0 = time.monotonic()


def say(msg: str) -> None:
    with print_mutex:
        print(f'[{time.monotonic() - t0:6.1f}s] {msg}', file=sys.__stdout__, flush=True)


def scan_usb() -> dict:
    """busport -> {'serial', 'vidpid', 'ino'} for every enumerated USB device. Only
    <bus>-<port>[.<port>...] dirs match; root hubs ('usbN', no dash) are excluded because
    their 'serial' is a fabricated PCI address, and including them measured 6-7s/scan slower
    (an observation; NOT an autosuspend wake -- that read is cached and does no I/O).
    Keyed by busport, not serial: two devices can share a serial (an Espressif
    USB-Serial-JTAG bridge and the cafe device it flashes both derive it from the same
    MAC), and one dict slot would silently drop whichever lost the race."""
    found = {}
    # usb_scan's `serial` read is bounded by default (see hil_util.read_sysfs) -- this tool
    # has no pool guard behind it and is run exactly when a device is suspected wedged. A
    # device that will not answer is simply absent from the table.
    devs = hil_util.usb_scan()
    for dev in devs:
        try:
            found[dev['busport']] = {
                'serial': dev['serial'].lower(),
                'vidpid': f"{dev['vid']}:{dev['pid']}",
                'ino': os.stat(dev['dir'] + '/').st_ino}
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
    PID-pinned. VID cafe keeps an Espressif USB-Serial-JTAG (303a) that shares the MAC
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
    """Nonblocking flock per hil_lock.py protocol. Returns the handle, or a str with the
    holder's info when the board is locked elsewhere. Board locks are ALWAYS respected: a
    held board is reported and skipped, never waited on, and there is no bypass here."""
    os.makedirs(hil_lock.BOARD_LOCK_DIR, exist_ok=True)
    try:
        fh = hil_lock.flock_nb(name)
    except BlockingIOError:  # EWOULDBLOCK: the flock is held
        info = hil_lock.read_record(name)
        return json.dumps(info) if info else 'unknown holder'
    except OSError as e:  # the lock file itself (EACCES/EROFS/ENOSPC), not a holder
        return f'ERROR: lock file: {e.strerror}'
    if not hil_lock.write_record(fh, 'pool_check'):
        # an invisible lock (flock held, no record) is worse than no lock: status cannot
        # show us and release cannot recognize the protected holder
        hil_lock.clear_record(fh)
        fh.close()
        return 'ERROR: holder record write failed (lock dir unwritable?)'
    return fh


def unlock_board(fh) -> None:
    hil_lock.clear_record(fh)
    fh.close()


def resolve_variant(board: dict, example: str, note: list | None = None) -> str:
    """Build-dir variant name for `example`: the first of the board's variants with
    already-built firmware, falling back to the board name. Notes the pick when it
    differs from the board name (e.g. nanoch32v203's build dir is variant
    'nanoch32v203-fsdev', not the board name)."""
    name = board['name']
    for v in hil_report.board_variants(board):
        vn = v['name']
        if hil_flash.find_firmware(vn, example, flasher=board['flasher']['name']):
            if vn != name and note is not None and f'variant: {vn}' not in note:
                note.append(f'variant: {vn}')
            return vn
    return name


def light_candidates(board: dict):
    """(kind, candidates, buildable): kind is 'device' (uid check) or 'host' (serial-output
    check); candidates in preference order, the roster's skip list removed; buildable the
    ones worth building — an only-list board must get one of its own examples, since
    dfu_runtime etc. may not even configure for it."""
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
    cand = [c for c in dict.fromkeys(cand) if c not in skip]
    return kind, cand, [c for c in cand if not only or c in only]


def pick_example(board: dict, note: list):
    """(example, kind, variant, fw) of the first candidate already built for this board;
    variant is the build-dir variant that has it (see resolve_variant), fw the firmware
    path to flash, extension included. (None, kind, None, None) when none is built."""
    kind, cand, _ = light_candidates(board)
    for ex in cand:
        variant = resolve_variant(board, ex, note)
        fw = hil_flash.find_firmware(variant, ex, flasher=board['flasher']['name'])
        if fw:
            return ex, kind, variant, fw
    return None, kind, None, None


def fresh_firmware(board: dict, example: str):
    """(variant, fw) this run's build wrote. check_build writes cmake-build/, so it is
    looked there even when an explicit -B narrowed the search: this is OUR fresh build,
    not a stale fallback."""
    for v in hil_report.board_variants(board):
        fw = hil_flash.find_firmware(v['name'], example, roots=['cmake-build'],
                                     flasher=board['flasher']['name'])
        if fw:
            return v['name'], fw
    return None, None


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
    """Run a hil_flash flash_*/reset_* backend, normalizing raises to a failure: several
    backends raise instead of returning nonzero (get_serial_dev when a bridge's
    /dev/serial/by-id node vanishes, a missing config.env, a .jlink script OSError), and an
    exception must become a noted failure, not a crashed row. Returns (rc, error line)."""
    try:
        ret = fn(*fn_args)
        if ret.returncode == 0:
            return 0, ''
        err = flash_error_line(hil_util.cmd_stdout_text(ret.stdout))
        return ret.returncode, err or f'rc={ret.returncode}'
    except Exception as e:
        return -1, repr(e)[:90]


def flash(board: dict, fw, note: list) -> bool:
    """One flash attempt; a failure is noted, never retried or recovered here.

    `fw` comes from pick_example: a re-resolve here would use the global search policy and
    miss a firmware this run's build wrote into cmake-build/ under an exclusive -B."""
    rc, err = call_flasher(hil_flash.flash_primitive(board['flasher']['name']), board, str(fw))
    if rc == 0:
        return True
    if rc == 127 and board['flasher']['name'].lower() == 'esptool':
        note.append(f'flasher tool missing ({err}) — esptool needs the ESP-IDF env '
                    f'(. "$IDF_PATH/export.sh")')
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
    the same probe can fail transiently and leave the target halted.

    "logger": "rtt" boards have no VCOM: the same check runs over the probe's RTT
    console instead. The reset happens BEFORE the console opens (it owns the probe),
    which also zeroes the .bss ring — so pre-reset backlog cannot count as life, and
    without a reset Commander delivers the boot burst the preceding flash left."""
    reset_fn = hil_flash.reset_primitive(board['flasher']['name']) if do_reset else None
    if board.get('logger') == 'rtt':
        if reset_fn:
            # a failed reset leaves the previous run's ring intact: attaching anyway would
            # score stale output as life
            rc, err = call_flasher(reset_fn, board)
            if rc:
                say(f'{board["name"]:26} reset failed: {err}')
                return None
        try:
            ser = hil_util.JlinkRtt(board, timeout=0.3)
        except hil_util.RttError as e:
            say(f'{board["name"]:26} no RTT console: {e}')
            return None
        try:
            data = b''
            deadline = time.monotonic() + SERIAL_WAIT
            while time.monotonic() < deadline:
                ser.write(b'U')
                data += ser.read(256)
                # JLinkExe's banner arrives whether or not the target is alive --
                # judged unfiltered it scores a dead board 'alive'. Same shared filter
                # as test_host_device_info; complete_only drops a trailing partial
                # line, so a banner FRAGMENT split by this read boundary cannot count
                # as target output either.
                td = hil_util.strip_banner(data, complete_only=True)
                if want_hello:
                    if b'Hello from TinyUSB' in td:
                        return td
                elif td and not boardtest_output(td):
                    return td
            return hil_util.strip_banner(data)
        except hil_util.RttError:
            return None  # console died mid-poll (server exited, probe dropped)
        finally:
            ser.close()
    import serial
    try:
        port = hil_util.get_serial_dev(board['flasher']['uid'], None, None, 0)
        ser = serial.Serial(port, baudrate=115200, timeout=0.3, write_timeout=1)
    except Exception as e:
        say(f'{board["name"]:26} no flasher serial port: {e}')
        return None
    try:
        # flush BEFORE the reset: this drops the pre-reset CDC backlog (which must not
        # count as life) while keeping the post-reset boot banner, which prints while the
        # reset tool is still tearing down and a post-reset flush would eat
        ser.reset_input_buffer()
        if reset_fn:
            reset_fn(board)
        # judge the WHOLE window, not the first chunk: the probe's CDC bridge has its own
        # FIFO, so stale pre-flash output (e.g. board_test hellos) can arrive after our
        # host-side flush and must not decide the verdict alone.
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
            # early-exit on the caller's positive signal (board_test hello for park
            # verification, any non-board_test output for example liveness): stale
            # bridge-FIFO backlog of the OTHER kind must not end the window
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


_build_lock = threading.Lock()  # one check_build at a time: --fetch-deps and idf.py's
                                # source-tree dependencies.lock are shared by every board
_built: set = set()             # (variant, example) this run built


def build(board: dict, examples: list, config: Path, note: list) -> bool:
    """Build `examples` for every roster variant of this board through the build contract,
    into the cmake-build/cmake-build-<variant> dirs hil_test.py flashes too. Call BEFORE
    taking the board lock: builds are long. False with the failure noted."""
    cmd = [sys.executable, str(CHECK_BUILD), '--board', board['name'], '--shared',
           '--variants', str(config), '--fetch-deps']
    for e in examples:
        cmd += ['-e', e]
    if board['flasher']['name'].lower() == 'esptool' and not shutil.which('idf.py'):
        idf_path = os.environ.get('IDF_PATH')
        if not idf_path or not (Path(idf_path) / 'export.sh').is_file():
            note.append('cannot build: ESP-IDF env missing (. "$IDF_PATH/export.sh")')
            return False
        # sourced in this child only: export.sh rewrites PATH and the python venv
        cmd = ['bash', '-c', f'. "$IDF_PATH/export.sh" >/dev/null && {shlex.join(cmd)}']
    with _build_lock:
        r = hil_util.run_cmd(cmd, cwd=str(REPO_ROOT), timeout=BUILD_TIMEOUT,
                             split_stderr=True, quiet=True)
    if r.returncode == 124:
        note.append('build timeout')
        return False
    lines = hil_util.cmd_stdout_text(r.stdout).strip().splitlines()
    try:
        result = json.loads(lines[-1])
    except (IndexError, ValueError):
        note.append(f'build: check_build.py exited {r.returncode} without a verdict')
        return False
    if result.get('error'):
        note.append(f'build: {result["error"][:120]}')
        return False
    bad = [b for b in result.get('boards', []) if b.get('status') != 'ok']
    if bad or not result.get('pass'):
        b = bad[0] if bad else {}
        note.append(f'build failed: {b.get("buildDir", "?")}: {(b.get("firstError") or "")[:90]}')
        return False
    return True


def verdict(row: dict, ok: bool) -> str:
    """Row status for a verification result, preserving a 'flash-failed' a deeper
    layer already recorded (silent flash no-op, board_test delivery failure)."""
    return 'ok' if ok else ('flash-failed' if row['status'] == 'flash-failed' else 'failed')


def host_alive(board: dict, note: list, row: dict, flashed_example: bool = False) -> bool:
    """Serial aliveness. With flashed_example=True (a host example was just flashed),
    board_test-shaped output FAILS the check: the parked image still talking means the
    example flash silently didn't take — the host analog of the device path's PID check,
    and a delivery failure (row['status'] = 'flash-failed', which verdict() keeps)."""
    data = check_host_serial(board)
    if data and flashed_example and boardtest_output(data):
        note.append('board_test output after example flash: silent flash no-op')
        row['status'] = 'flash-failed'
        return False
    return bool(data)


def check_device(board: dict, example: str, variant: str, old_ino, note: list, row: dict) -> bool:
    """Wait for the flashed board's uid to re-enumerate. A PID mismatch means the build
    dir is stale (warn) — unless this run built the firmware, when stale is impossible
    and it can only be a silent flash no-op (fail). An unknown expected PID scores ok
    with a 'pid unverified' note."""
    expected_pid = get_expected_pid(example)
    hit = wait_device(board['uid'], None, old_ino, ENUM_WAIT)
    if not hit:
        row['device'] = '❌ not enumerated'
        return False
    if expected_pid is None:
        note.append('pid unverified')
    elif not hit[1].endswith(expected_pid):
        if (variant, example) in _built:
            row['device'] = f'❌ {hit[1]}'
            note.append(f'pid {hit[1]}, this run built {expected_pid}: silent flash no-op')
            row['status'] = 'flash-failed'
            return False
        note.append(f'⚠ pid {hit[1]}, source says {expected_pid}: stale build or silent flash no-op')
    row['device'] = f'✅ {hit[1]}'
    return True


def check_board(board: dict, args) -> dict:
    name = board['name']
    row = {'name': name, 'probe': '❌ missing', 'flash': '–', 'device': '–', 'note': [], 'status': 'failed'}
    note = row['note']

    probe = find_usb(board['flasher']['uid'])
    if probe:
        row['probe'] = f'✅ {probe[0]}'
    else:
        say(f'{name:26} probe MISSING ({board["flasher"]["name"]} {board["flasher"]["uid"]})')

    # existing firmware only; a missing build is built further down (after a lock peek),
    # except in scan/no-build modes and never for a missing probe
    example, kind, variant, fw = pick_example(board, note)
    if kind == 'host':
        note.append('host-only board')

    if args.scan_only:
        hit = find_device(board['uid'], None)
        # report the BOARD's usb state too: enumerated (with busport), off-bus (normal
        # when parked in board_test), or n/a for host-only boards
        if hit:
            row['device'] = f'✅ {hit[1]} @{hit[0]}'
        elif kind == 'host':
            row['device'] = '– n/a (host-only)'
        else:
            row['device'] = '⚫ off bus (parked?)'
        # scan verifies probe presence only, so probe present is ok; a missing probe means
        # no firmware could be delivered → flash-failed
        row['status'] = 'ok' if probe else 'flash-failed'
        if probe:
            say(f'{name:26} probe ✅ {probe[0]}' + (f'  device {hit[1]}' if hit else ''))
        return row
    if not probe:
        row['status'] = 'flash-failed'
        return row

    bt_variant = resolve_variant(board, 'device/board_test', note)
    _, _, buildable = light_candidates(board)
    # the second candidate covers a preferred example that fails to build
    wanted = buildable[:2] if example is None else []
    if not args.no_park and hil_flash.find_firmware(bt_variant, 'device/board_test',
                                                    flasher=board['flasher']['name']) is None:
        wanted.append('device/board_test')  # board_test is only the park image
    if wanted and args.no_build:
        note.append(f'build skipped (--no-build): {", ".join(Path(e).name for e in wanted)}')
    elif wanted and name in args.parked:
        # check_build.py --variants builds only the roster's active boards
        note.append('not built: a boards-skip board needs prebuilt firmware')
    elif wanted:
        # builds are long and run BEFORE locking (park must never hold the flock through
        # one); peek first so minutes of building are not wasted on — or a rebuilt tree
        # swapped under — a board CI holds right now
        peek = lock_board(name)
        if isinstance(peek, str):
            if peek.startswith('ERROR:'):  # environment failure, not a held lock
                row['flash'] = '❌ lock'
                row['status'] = 'failed'
            else:
                row['flash'] = '🔒 locked'
                row['status'] = 'locked'
            note.append(peek)
            say(f'{name:26} locked: {peek}')
            return row
        unlock_board(peek)
        if build(board, wanted, args.config_path, note) and example is None:
            for ex in buildable[:2]:
                variant, fw = fresh_firmware(board, ex)
                if fw:
                    example = ex
                    _built.add((variant, ex))
                    note.append(f'built {Path(ex).name}')
                    break
            else:
                note.append('build produced no firmware')

    if example is None:
        if not any(n.startswith(('build', 'not built', 'cannot build')) for n in note):
            note.append('no firmware built')
        if kind != 'host':
            row['status'] = 'flash-failed'
            say(f'{name:26} probe ✅ {probe[0]}  (no firmware to flash)')
            return row
        # host-only board: aliveness is still checkable without flashing — reset and listen
        # to whatever is on it (parked board_test echoes and hellos on the flasher UART)

    lk = lock_board(name)
    if isinstance(lk, str):
        if lk.startswith('ERROR:'):  # environment failure, not a held lock
            row['flash'] = '❌ lock'
            row['status'] = 'failed'
        else:
            row['flash'] = '🔒 locked'
            row['status'] = 'locked'
        note.append(lk)
        say(f'{name:26} locked: {lk}')
        return row
    try:
        if example is None:  # host-only without firmware: UART-only aliveness check
            ok = host_alive(board, note, row)
            row['device'] = '✅ serial out' if ok else '❌ no serial out'
            row['status'] = verdict(row, ok)
            say(f'{name:26} –  {row["device"]}  (existing firmware)')
            return row

        pre = find_device(board['uid'], None)
        old_ino = pre[2] if pre else None

        try:
            if not flash(board, fw, note):
                row['flash'] = f'❌ {Path(example).name}'
                row['status'] = 'flash-failed'
                say(f'{name:26} flash FAILED ({example})')
                return row
            row['flash'] = f'✅ {Path(example).name}'

            if kind == 'host':
                ok = host_alive(board, note, row, flashed_example=True)
                row['device'] = '✅ serial out' if ok else '❌ no serial out'
            else:
                ok = check_device(board, example, variant, old_ino, note, row)
            row['status'] = verdict(row, ok)
            say(f'{name:26} {row["flash"]}  {row["device"]}')
            return row
        finally:
            # teardown for EVERY path that attempted a flash (a failed programmer op can
            # still have erased/half-written the target), while the lock is still held
            if not args.no_park:
                park_board(board, kind, row, note)
    finally:
        unlock_board(lk)


def park_board(board: dict, kind: str, row: dict, note: list) -> None:
    """Re-park with board_test (built, if absent, before the lock) and VERIFY it took: board_test never enumerates USB, so a device board's cafe
    device must drop off the bus, and a host board must answer with board_test's
    own output — a rc=0 park that changed nothing (silent no-op) must not pass.
    A board left unparked marks an ok row flash-failed (never downgrading a
    'failed' verify verdict — that is the more diagnostic signal), with one
    exception: an espressif board without the ESP-IDF env cannot build
    board_test — noted, not a board fault."""
    # capture BEFORE the park flash: uid-disappearance only verifies the park if the
    # device was on the bus to begin with
    on_bus_before = kind != 'host' and find_device(board['uid'], None) is not None
    variant = resolve_variant(board, 'device/board_test', note)
    fw = (hil_flash.find_firmware(variant, 'device/board_test', flasher=board['flasher']['name'])
          or fresh_firmware(board, 'device/board_test')[1])
    if fw is None:
        if any(n.startswith('cannot build: ESP-IDF env missing') for n in note):
            note.append('park skipped (no ESP-IDF env)')
        else:
            # --no-build disables builds, not parking (--no-park is that opt-out): a board
            # left running a USB-active image is unparked either way; the note says why
            note.append('unparked: no board_test firmware')
            if row['status'] == 'ok':
                row['status'] = 'flash-failed'
        return
    rc, err = call_flasher(hil_flash.flash_primitive(board['flasher']['name']), board, str(fw))
    if rc != 0:
        note.append(f'park flash failed: {err}')
        if row['status'] == 'ok':
            row['status'] = 'flash-failed'
        return
    if kind == 'host':
        # no second reset (the park flash's own reset started board_test); POSITIVE
        # marker: its hello must appear, and stale bridge-FIFO output alongside it is not
        # disqualifying
        data = check_host_serial(board, do_reset=False, want_hello=True)
        if not (data and b'Hello from TinyUSB' in data):
            note.append('park unverified: no board_test output')
            if row['status'] == 'ok':
                row['status'] = 'flash-failed'
        return
    if not on_bus_before:
        # never enumerated this run: uid-disappearance cannot tell a verified park from a
        # silent no-op — say so instead of passing vacuously
        note.append('park unverified (device already off bus)')
        return
    deadline = time.monotonic() + 6
    while time.monotonic() < deadline:
        if find_device(board['uid'], None) is None:
            return
        time.sleep(0.5)
    note.append('park unverified: device still enumerated')
    if row['status'] == 'ok':
        row['status'] = 'flash-failed'


def check_board_safe(board: dict, args) -> dict:
    """Isolate one board's exceptions: a crashing worker must not discard every
    other board's row and the table."""
    try:
        return check_board(board, args)
    except Exception as e:
        name = board.get('name', '?')
        say(f'{name:26} INTERNAL ERROR: {e!r}')
        return {'name': name, 'probe': '–', 'flash': '–', 'device': '❌ error',
                'note': [repr(e)[:120]], 'status': 'failed'}


def main() -> None:
    # toolchain/flasher CLIs live in the user bin dirs, which non-login shells may lack --
    # the same PATH shim hil_remote.py applies on the remote side
    for d in (Path.home() / 'bin', Path.home() / '.local' / 'bin'):
        if d.is_dir() and str(d) not in os.environ.get('PATH', '').split(os.pathsep):
            os.environ['PATH'] = f'{d}{os.pathsep}{os.environ.get("PATH", "")}'

    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument('config', nargs='?', help='HIL config json (default: by hostname)')
    parser.add_argument('-b', '--board', action='append', default=[], help='only these boards')
    parser.add_argument('-B', '--build-dir', default=None,
                        help='firmware parent dir, searched EXCLUSIVELY when given '
                             '(default: examples, plus cmake-build as fallback)')
    parser.add_argument('--scan-only', action='store_true',
                        help='USB presence scan only: no locks, no flashing')
    parser.add_argument('--no-build', action='store_true',
                        help='do not build missing firmware (default: build the light example on the spot)')
    parser.add_argument('--no-park', action='store_true',
                        help='leave the light example running (default: park with board_test)')
    # no cross-process flash budget against a concurrent hil_test.py run (its semaphores
    # are in-process), so keep this modest
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
    args.config_path = cfg_path
    args.parked = {b['name'] for b in config.get('boards-skip', [])}
    if args.board:
        boards += config.get('boards-skip', [])  # explicitly named parked boards are fair game
        unknown = set(args.board) - {b['name'] for b in boards}
        if unknown:
            sys.exit(f'board(s) not in {cfg_path.name}: {", ".join(sorted(unknown))}')
        boards = [b for b in boards if b['name'] in args.board]

    hil_flash.build_dir = args.build_dir or 'examples'
    hil_util.verbose = args.verbose
    if args.build_dir is None:
        # default mode: search both standard layouts (cmake-build/ from tools/build.py and
        # ESP-IDF, examples/ from manual builds). An EXPLICIT -B stays exclusive: the caller
        # named an artifact tree, so a miss must report rather than flash an older build.
        hil_flash.EXTRA_BUILD_DIRS = ['cmake-build', 'examples']
    roots = ' + '.join(dict.fromkeys([hil_flash.build_dir, *hil_flash.EXTRA_BUILD_DIRS]))
    say(f'pool check: host {host}, config {cfg_path.name}, {len(boards)} boards, '
        f'{"scan-only" if args.scan_only else f"flash via {{{roots}}}/cmake-build-<board>"}')

    if args.verbose:
        rows = [check_board_safe(b, args) for b in boards]
    else:
        with io.StringIO() as spool, ThreadPoolExecutor(max_workers=args.jobs) as pool:
            sys.stdout = spool  # silence hil_util.run_cmd's COMMAND FAILED dumps; say() uses __stdout__
            try:
                rows = list(pool.map(lambda b: check_board_safe(b, args), boards))
            finally:
                sys.stdout = sys.__stdout__

    status_mark = {'ok': '✅ ok', 'flash-failed': '❌ flash-failed', 'failed': '❌ failed',
                   'locked': '🔒 locked'}
    headers = ['Board', 'Probe', 'Flash', 'Device', 'Status', 'Note']
    cells = [[r['name'], r['probe'], r['flash'], r['device'],
              status_mark.get(r['status'], r['status']), '; '.join(r['note'])] for r in rows]
    # display_width, not len(): ✅ / ❌ / 🔒 / ⚠ are one character and two columns, so
    # len() pads every row holding one a column short of the header rule
    _w = hil_util.display_width
    widths = [max(_w(h), *(_w(c[i]) for c in cells)) if cells else _w(h)
              for i, h in enumerate(headers)]
    line = lambda vals: ('| ' + ' | '.join(hil_util.pad(v, w)
                                           for v, w in zip(vals, widths)) + ' |')
    print()
    print(line(headers))
    print('|' + '|'.join('-' * (w + 2) for w in widths) + '|')
    for c in cells:
        print(line(c))

    counts = {'ok': 0, 'flash-failed': 0, 'failed': 0, 'locked': 0}
    for r in rows:
        counts[r.get('status', 'failed')] += 1
    print(f'\n{counts["ok"]} ok · {counts["flash-failed"]} flash-failed · {counts["failed"]} failed '
          f'· {counts["locked"]} locked · in {time.monotonic() - t0:.0f}s')
    sys.exit(min(counts['flash-failed'] + counts['failed'], 125))


if __name__ == '__main__':
    main()
