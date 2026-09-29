#!/usr/bin/env python3
"""Quick HIL pool health check.

For every board in the rig's HIL config: is the flash probe on the USB bus, does a light
example flash, and does the board's USB device (uid) come back up? Missing firmware is
built through the build contract (check_build.py); --no-build opts out. No recovery: a
wedged probe or board is reported for usb-kernel-recover. Prints a markdown table, or
with --json the JSON document the table is rendered from. Row statuses: ok, flash-failed
(firmware not delivered or not verified as delivered), failed (the check ran but did not
verify), locked (held by another process; never waited on or bypassed).

Config is picked by hostname unless given: ci -> tinyusb.json, tusb (hifiphile rig) ->
hfp.json, anything else is a dev PC -> local.json.
"""

import argparse
import contextlib
import functools
import io
import json
import os
import re
import shlex
import shutil
import socket
import sys
import time
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

t0 = time.monotonic()


def say(msg: str) -> None:
    print(f'[{time.monotonic() - t0:6.1f}s] {msg}', file=sys.__stdout__, flush=True)


def scan_usb() -> dict:
    """busport -> {'serial', 'vidpid'} for every enumerated USB device, root hubs excluded
    (their 'serial' is a fabricated PCI address). Keyed by busport, not serial: an
    Espressif USB-Serial-JTAG bridge and the cafe device it flashes share one (the MAC)."""
    return {d['busport']: {'serial': d['serial'].lower(), 'vidpid': f"{d['vid']}:{d['pid']}"}
            for d in hil_util.usb_scan()}


def find_usb(uid: str) -> str | None:
    """Busport of the flasher probe with this uid, excluding VID cafe (TinyUSB DUT firmware):
    a probe's uid can coincidentally equal its DUT's (Espressif USB-Serial-JTAG bridges
    derive both from the same MAC), and the DUT is never the probe.

    J-Link zero-pads numeric serials (681295394 -> 000681295394): an all-digit uid
    matches an all-digit serial only when that serial equals the uid zero-padded to
    the serial's own length (leading zeros only) — never when the zero-stripped uid
    is empty, so a placeholder serial (metro_m4_express's probe legitimately reports
    '123456') can't be mistaken for an unrelated device."""
    u = uid.lower()
    candidates = [(bp, dev) for bp, dev in scan_usb().items() if not dev['vidpid'].startswith('cafe:')]
    for bp, dev in candidates:
        if dev['serial'] == u:
            return bp
    stripped = u.lstrip('0')
    if u.isdigit() and stripped:
        for bp, dev in candidates:
            s = dev['serial']
            if s.isdigit() and s == stripped.zfill(len(s)):
                return bp
    return None


def find_device(uid: str):
    """(busport, vidpid, inode) of the board's TinyUSB device (idVendor cafe, so an
    Espressif USB-Serial-JTAG sharing the MAC serial cannot false-pass). usb_scan reads the
    lock-guarded `serial` of cafe devices only, not of every probe and hub on the bus."""
    for d in hil_util.usb_scan(vid='cafe', serial=uid):
        try:
            return d['busport'], f"cafe:{d['pid']}", os.stat(d['dir'] + '/').st_ino
        except OSError:
            continue
    return None


def wait_device(uid: str, old_ino, budget: float):
    """Wait for the board's device with a NEW sysfs inode (flash resets the MCU, so a
    genuine flash must re-enumerate; the inode is the re-enumeration marker)."""
    deadline = time.monotonic() + budget
    while time.monotonic() < deadline:
        hit = find_device(uid)
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


def find_image(board: dict, example: str, roots: list | None = None, built: dict | None = None):
    """(variant, fw) of the first of the board's variants with `example` built, or
    (None, None). With `built` (build()'s result), only a variant whose build verified that
    image counts: a pass can still have skipped it, leaving an older file in the dir."""
    for v in hil_report.board_variants(board):
        if built is not None and Path(example).name not in built.get(v['name'], ()):
            continue
        fw = hil_flash.find_firmware(v['name'], example, roots=roots, flasher=board['flasher']['name'])
        if fw:
            return v['name'], fw
    return None, None


@functools.lru_cache(maxsize=None)
def get_expected_pid(example: str) -> str | None:
    """USB_PID for `example`'s device descriptor (examples/<example>/src/
    usb_descriptors.c, '#define USB_PID 0x....'), lowercased and without the 0x
    prefix to match sysfs idProduct. None when the file or define isn't there — host
    examples have no usb_descriptors.c, and the caller must stay quiet rather than
    false-warn."""
    try:
        text = (REPO_ROOT / 'examples' / example / 'src' / 'usb_descriptors.c').read_text()
    except OSError:
        return None
    # optional parens as in tools/check_example_pids.py's parser
    m = re.search(r'#define\s+USB_PID\s+\(?\s*(0x[0-9a-fA-F]+)', text)
    return m.group(1)[2:].lower() if m else None


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
    """One flash attempt; a failure is noted, never retried or recovered here."""
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


def idf_env_missing(board: dict) -> bool:
    """An ESP board whose build has neither idf.py on PATH nor IDF_PATH/export.sh."""
    if board['flasher']['name'].lower() != 'esptool' or shutil.which('idf.py'):
        return False
    idf_path = os.environ.get('IDF_PATH')
    return not idf_path or not (Path(idf_path) / 'export.sh').is_file()


def build(board: dict, examples: list, config: Path, note: list) -> dict | None:
    """Build `examples` for every roster variant of this board through the build contract,
    into the cmake-build/cmake-build-<variant> dirs hil_test.py flashes too. Call BEFORE
    taking the board lock: builds are long. Returns {variant: example names the build
    verified}, or None with the failure noted."""
    cmd = [sys.executable, str(CHECK_BUILD), '--board', board['name'], '--shared',
           '--variants', str(config), '--fetch-deps']
    for e in examples:
        cmd += ['-e', e]
    if idf_env_missing(board):
        note.append('cannot build: ESP-IDF env missing (. "$IDF_PATH/export.sh")')
        return None
    if board['flasher']['name'].lower() == 'esptool' and not shutil.which('idf.py'):
        # sourced in this child only: export.sh rewrites PATH and the python venv
        cmd = ['bash', '-c', f'. "$IDF_PATH/export.sh" >/dev/null && {shlex.join(cmd)}']
    r = hil_util.run_cmd(cmd, cwd=str(REPO_ROOT), timeout=BUILD_TIMEOUT,
                         split_stderr=True, quiet=True)
    if r.returncode == 124:
        note.append('build timeout')
        return None
    lines = hil_util.cmd_stdout_text(r.stdout).strip().splitlines()
    try:
        result = json.loads(lines[-1])
    except (IndexError, ValueError):
        note.append(f'build: check_build.py exited {r.returncode} without a verdict')
        return None
    if result.get('error'):
        note.append(f'build: {result["error"][:120]}')
        return None
    bad = [b for b in result.get('boards', []) if b.get('status') != 'ok']
    if bad or not result.get('pass'):
        b = bad[0] if bad else {}
        note.append(f'build failed: {b.get("buildDir", "?")}: {(b.get("firstError") or "")[:90]}')
        return None
    return {b['buildDir'].rsplit('cmake-build-', 1)[-1]: set(b.get('okExamples', ()))
            for b in result['boards']}


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


def check_device(board: dict, example: str, fresh: bool, old_ino, note: list, row: dict) -> bool:
    """Wait for the flashed board's uid to re-enumerate. A PID mismatch means the build
    dir is stale (warn) — unless the image is `fresh` (check_build just verified it), when
    stale is impossible and it can only be a silent flash no-op (fail). An unknown
    expected PID scores ok with a 'pid unverified' note."""
    expected_pid = get_expected_pid(example)
    hit = wait_device(board['uid'], old_ino, ENUM_WAIT)
    if not hit:
        row['device'] = '❌ not enumerated'
        return False
    if expected_pid is None:
        note.append('pid unverified')
    elif not hit[1].endswith(expected_pid):
        if fresh:
            row['device'] = f'❌ {hit[1]}'
            note.append(f'pid {hit[1]}, this run built {expected_pid}: silent flash no-op')
            row['status'] = 'flash-failed'
            return False
        note.append(f'⚠ pid {hit[1]}, source says {expected_pid}: stale build or silent flash no-op')
    row['device'] = f'✅ {hit[1]}'
    return True


def not_locked(row: dict, why: str) -> dict:
    """The row for a board lock_board could not take: held elsewhere, or an ERROR: from the
    lock file itself, which is an environment failure rather than contention."""
    held = not why.startswith('ERROR:')
    row['flash'], row['status'] = ('🔒 locked', 'locked') if held else ('❌ lock', 'failed')
    row['note'].append(why)
    say(f'{row["name"]:26} locked: {why}')
    return row


def new_row(name: str, **cells) -> dict:
    return {'name': name, 'probe': '❌ missing', 'probe_busport': None, 'flash': '–', 'device': '–',
            'note': [], 'status': 'failed', **cells}


def check_board(board: dict, args) -> dict:
    name = board['name']
    row = new_row(name)
    note = row['note']

    probe = find_usb(board['flasher']['uid'])
    if probe:
        row['probe'] = f'✅ {probe}'
        row['probe_busport'] = probe
    else:
        say(f'{name:26} probe MISSING ({board["flasher"]["name"]} {board["flasher"]["uid"]})')
    kind, candidates, buildable = light_candidates(board)
    if kind == 'host':
        note.append('host-only board')

    if args.scan_only:
        hit = find_device(board['uid'])
        # the BOARD's usb state too: enumerated, off-bus (normal when parked in board_test),
        # or n/a for host-only boards
        if hit:
            row['device'] = f'✅ {hit[1]} @{hit[0]}'
        elif kind == 'host':
            row['device'] = '– n/a (host-only)'
        else:
            row['device'] = '⚫ off bus (parked?)'
        row['status'] = 'ok' if probe else 'flash-failed'
        if probe:
            say(f'{name:26} probe ✅ {probe}' + (f'  device {hit[1]}' if hit else ''))
        return row
    if not probe:
        row['status'] = 'flash-failed'
        return row

    example = variant = fw = None
    for ex in candidates:
        variant, fw = find_image(board, ex)
        if fw:
            example = ex
            break
    bt_fw = None if args.no_park else find_image(board, 'device/board_test')[1]
    fresh = False
    # the second candidate covers a preferred example the build system skips for this board
    wanted = buildable[:2] if example is None else []
    if not args.no_park and bt_fw is None:
        wanted.append('device/board_test')  # board_test is only the park image
    if example is None and not buildable:
        note.append('no light example for this board')
    if wanted and args.no_build:
        note.append(f'build skipped (--no-build): {", ".join(Path(e).name for e in wanted)}')
    elif wanted and name in args.parked:
        # check_build.py --variants builds only the roster's active boards
        note.append('not built: a boards-skip board needs prebuilt firmware')
    elif wanted:
        # builds are long and run BEFORE locking (park must never hold the flock through
        # one); skip a board CI holds right now. is_locked reads the holder record only:
        # probing the flock itself would make a concurrent CI acquire fail spuriously
        if hil_lock.is_locked(name):
            row['flash'], row['status'] = '🔒 locked', 'locked'
            note.append(json.dumps(hil_lock.read_record(name)))
            say(f'{name:26} locked: {note[-1]}')
            return row
        built = build(board, wanted, args.config_path, note)
        if built is not None:
            if bt_fw is None and not args.no_park:
                bt_fw = find_image(board, 'device/board_test', ['cmake-build'], built)[1]
            # check_build writes cmake-build/, looked at even when an explicit -B narrowed
            # the search; a verified image is current with the tree
            for ex in buildable[:2] if example is None else []:
                variant, fw = find_image(board, ex, ['cmake-build'], built)
                if fw:
                    example, fresh = ex, True
                    note.append(f'built {Path(ex).name}')
                    break
            if example is None and buildable:
                note.append('build produced no firmware')
    if variant and variant != name:
        note.append(f'variant: {variant}')

    if example is None and kind != 'host':
        row['status'] = 'flash-failed'
        say(f'{name:26} probe ✅ {probe}  (no firmware to flash)')
        return row

    lk = lock_board(name)
    if isinstance(lk, str):
        return not_locked(row, lk)
    try:
        if example is None:
            # host-only board without light firmware: still checkable without a flash, by
            # the output of whatever runs (parked board_test echoes and hellos)
            ok = host_alive(board, note, row)
            row['device'] = '✅ serial out' if ok else '❌ no serial out'
            row['status'] = verdict(row, ok)
            say(f'{name:26} –  {row["device"]}  (existing firmware)')
            return row

        pre = find_device(board['uid'])
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
                ok = check_device(board, example, fresh, old_ino, note, row)
            row['status'] = verdict(row, ok)
            say(f'{name:26} {row["flash"]}  {row["device"]}')
            return row
        finally:
            # teardown for EVERY path that attempted a flash (a failed programmer op can
            # still have erased/half-written the target), while the lock is still held
            if not args.no_park:
                park_board(board, kind, bt_fw, row, note)
    finally:
        unlock_board(lk)


def park_board(board: dict, kind: str, fw, row: dict, note: list) -> None:
    """Flash board_test and VERIFY it took: board_test never enumerates USB, so a device
    board's cafe device must drop off the bus, and a host board must answer with
    board_test's own output. A board left unparked turns an ok row flash-failed (a
    'failed' verdict is kept: it is the more diagnostic one), except an ESP board that
    could not build board_test for want of the IDF env — noted, not a board fault."""
    # capture BEFORE the park flash: uid-disappearance only verifies the park if the
    # device was on the bus to begin with
    on_bus_before = kind != 'host' and find_device(board['uid']) is not None
    if fw is None:
        if idf_env_missing(board):
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
        if find_device(board['uid']) is None:
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
        return new_row(name, probe='–', device='❌ error', note=[repr(e)[:120]])


def check_pool(boards: list, args, header: str) -> list:
    say(header)
    # silence hil_util.run_cmd's COMMAND FAILED dumps; say() writes to sys.__stdout__
    with contextlib.nullcontext() if args.verbose else contextlib.redirect_stdout(io.StringIO()):
        return [check_board_safe(b, args) for b in boards]


@contextlib.contextmanager
def stdout_to_stderr():
    """fd 1 onto stderr, so no flasher or build child can write to stdout either."""
    sys.stdout.flush()
    saved = os.dup(1)
    try:
        os.dup2(2, 1)
        yield
    finally:
        sys.stdout.flush()
        os.dup2(saved, 1)
        os.close(saved)


def pool_document(host: str, config: str, scan_only: bool, rows: list) -> dict:
    """The one result both outputs come from. `coverage` says what a row's status covers:
    probe-only (--scan-only), skipped-locked (never touched) or full-attempted;
    `probe_busport` is the probe's bus position for callers that look a board up."""
    counts = {'ok': 0, 'flash-failed': 0, 'failed': 0, 'locked': 0}
    for r in rows:
        counts[r['status']] += 1
        r['coverage'] = ('probe-only' if scan_only else
                         'skipped-locked' if r['status'] == 'locked' else 'full-attempted')
    return {'host': host, 'config': config, 'mode': 'scan-only' if scan_only else 'full',
            'rows': rows, 'counts': counts, 'elapsed_s': round(time.monotonic() - t0, 1)}


def render_table(doc: dict) -> str:
    status_mark = {'ok': '✅ ok', 'flash-failed': '❌ flash-failed', 'failed': '❌ failed',
                   'locked': '🔒 locked'}
    headers = ['Board', 'Probe', 'Flash', 'Device', 'Status', 'Note']
    cells = [[r['name'], r['probe'], r['flash'], r['device'],
              status_mark.get(r['status'], r['status']), '; '.join(r['note'])] for r in doc['rows']]
    # display_width, not len(): ✅ / ❌ / 🔒 / ⚠ are one character and two columns, so
    # len() pads every row holding one a column short of the header rule
    _w = hil_util.display_width
    widths = [max(_w(h), *(_w(c[i]) for c in cells)) if cells else _w(h)
              for i, h in enumerate(headers)]
    line = lambda vals: ('| ' + ' | '.join(hil_util.pad(v, w)
                                           for v, w in zip(vals, widths)) + ' |')
    counts = doc['counts']
    return '\n'.join(['', line(headers), '|' + '|'.join('-' * (w + 2) for w in widths) + '|',
                      *(line(c) for c in cells), '',
                      f'{counts["ok"]} ok · {counts["flash-failed"]} flash-failed · {counts["failed"]} failed '
                      f'· {counts["locked"]} locked · in {doc["elapsed_s"]:.0f}s'])


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
    parser.add_argument('--json', action='store_true',
                        help='print only the result document on stdout; progress goes to stderr')
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
    args.config_path = cfg_path.resolve()  # check_build.py reads it from the repo root
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
    header = (f'pool check: host {host}, config {cfg_path.name}, {len(boards)} boards, '
              f'{"scan-only" if args.scan_only else f"flash via {{{roots}}}/cmake-build-<board>"}')

    with stdout_to_stderr() if args.json else contextlib.nullcontext():
        rows = check_pool(boards, args, header)
    doc = pool_document(host, str(args.config_path), args.scan_only, rows)
    print(json.dumps(doc, ensure_ascii=False) if args.json else render_table(doc))
    counts = doc['counts']
    sys.exit(min(counts['flash-failed'] + counts['failed'], 125))


if __name__ == '__main__':
    main()
