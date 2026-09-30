#!/usr/bin/env python3
"""Quick HIL pool health check.

For every board in the rig's HIL config: is the flash probe on the USB bus, does a light
example flash, and does the board's USB device (uid) come back up? Firmware is never
built here: by default it comes from a per-host cache (CACHE_DIR) of CI artifacts, and
a board the cache has no variant of gets one downloaded once from master push runs; -B names another
firmware root, never fetched into. No recovery: a wedged probe or board is reported for
usb-kernel-recover. Prints a markdown table, or with --json the JSON document the table is
rendered from. Row statuses: ok, flash-failed (firmware not delivered or not verified as
delivered), failed (the check ran but did not verify), locked (held by another process;
never waited on or bypassed).

Config is picked by hostname unless given: ci -> tinyusb.json, tusb (hifiphile rig) ->
hfp.json, anything else is a dev PC -> local.json.
"""

import argparse
import contextlib
import errno
import io
import itertools
import json
import os
import re
import socket
import subprocess
import sys
import tempfile
import time
from datetime import datetime, timedelta, timezone
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))  # hil_flash + the helper package
import hil_flash
from helper import hil_lock, hil_report, hil_util

REPO_ROOT = hil_util.TINYUSB_ROOT
CONFIG_BY_HOST = {'ci': 'tinyusb.json', 'tusb': 'hfp.json'}  # anything else: dev PC -> local.json
# persistent and shared by the host's worktrees; not /tmp, which is RAM on ci.lan
CACHE_DIR = Path.home() / '.cache' / 'tinyusb-hil' / 'firmware'
REPO = 'hathach/tinyusb'
RETENTION_DAYS = 90  # GitHub artifact retention: older runs have nothing left to download
# bounds the search every run repeats for unavailable firmware, counting only runs with
# firmware artifacts: a master push builds the whole HIL matrix (PR runs are change-selected)
# unless it changed no code, when build.yml skips hil-build
MASTER_RUNS = 10

# light-example preference; first one found wins
DEVICE_CANDIDATES = ['device/dfu_runtime', 'device/cdc_msc', 'device/cdc_msc_freertos',
                     'device/hid_composite_freertos', 'device/cdc_dual_ports']
HOST_CANDIDATES = ['host/device_info', 'host/cdc_msc_hid', 'host/msc_file_explorer_freertos']

ENUM_WAIT = 12       # s, uid wait after flash
SERIAL_WAIT = 6      # s, host-board serial-output wait

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
    """(kind, candidates): kind is 'device' (uid check) or 'host' (serial-output check);
    candidates in preference order, the roster's skip list removed, and an only-list board
    limited to its own examples."""
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
    return kind, [c for c in dict.fromkeys(cand) if c not in skip and (not only or c in only)]


def find_image(board: dict, example: str):
    """(variant, fw) of the first of the board's variants with `example` built, or
    (None, None)."""
    for v in hil_report.board_variants(board):
        fw = hil_flash.find_firmware(v['name'], example, flasher=board['flasher']['name'])
        if fw:
            return v['name'], fw
    return None, None


def gh(*args: str, env: dict | None = None) -> str:
    return subprocess.run(['gh', *args], capture_output=True, text=True, timeout=600,
                          check=True, env=env).stdout


def master_runs():
    """Completed master push runs of build.yml, newest first, within artifact retention.
    Only master pushes: a fork PR's artifact is built by untrusted code, and esptool's
    flash_args reaches a command line. Filtered here, not by the API: its branch/event/
    status filters have returned lists missing the newest runs."""
    horizon = datetime.now(timezone.utc) - timedelta(days=RETENTION_DAYS)
    seen = set()  # a run created between page fetches shifts an older one onto the next page
    page = 1
    while True:
        runs = json.loads(gh('api', f'repos/{REPO}/actions/workflows/build.yml/runs'
                                    f'?per_page=100&page={page}'))['workflow_runs']
        if not runs:
            return
        for r in runs:
            if datetime.fromisoformat(r['created_at']) < horizon:
                return
            if r['id'] in seen:
                continue
            seen.add(r['id'])
            if (r['head_branch'], r['event'], r['status']) == ('master', 'push', 'completed'):
                yield r
        page += 1


def artifact_variant(name: str) -> str | None:
    """Variant of a 'binaries-<toolchain>-<matrix args>' artifact (build_util.yml)."""
    m = re.search(r'--build-name[ =](\S+)', name) or re.search(r'-b (\S+)', name)
    return m.group(1) if m else None


def run_artifacts(run_id: int) -> dict:
    """variant -> artifact name, for a run's unexpired firmware artifacts."""
    out = gh('api', '--paginate', f'repos/{REPO}/actions/runs/{run_id}/artifacts?per_page=100',
             '-q', '.artifacts[] | select(.expired | not) | .name')
    return {v: n for n in out.splitlines() if n.startswith('binaries-') and (v := artifact_variant(n))}


def usable(board: dict, variant: str, root: Path) -> bool:
    """The variant tree under `root` holds a light image and board_test, plus the files
    flash_esptool opens next to an esptool image."""
    flasher = board['flasher']['name']
    find = lambda ex: hil_flash.find_firmware(variant, ex, roots=[str(root)], flasher=flasher)
    light = next(filter(None, map(find, light_candidates(board)[1])), None)
    fws = [light, find('device/board_test')]
    return all(fws) and (flasher.lower() != 'esptool' or all(
        (fw.parent / f).is_file() for fw in fws for f in ('config.env', 'flash_args')))


def fetch_variant(board: dict, variant: str, run: dict, artifact: str) -> bool:
    """Download one artifact and publish its variant tree into CACHE_DIR; False when it
    lacks a usable image. One rename publishes it, so a reader never sees a partial tree;
    losing that rename to a concurrent fetch keeps the winner's copy."""
    with tempfile.TemporaryDirectory(dir=CACHE_DIR, prefix='.staging-', ignore_cleanup_errors=True) as td:
        staging = Path(td)
        tree = staging / f'cmake-build-{variant}'
        # gh stages the zip in TMPDIR whatever -D says
        gh('run', 'download', str(run['id']), '-R', REPO, '-n', artifact, '-D', td,
           env=dict(os.environ, TMPDIR=str(CACHE_DIR)))
        if not usable(board, variant, staging):
            say(f'{variant:26} run {run["id"]}: no light image or board_test, trying older')
            return False
        (tree / '.source').write_text(json.dumps(
            {'run': run['id'], 'sha': run['head_sha'], 'artifact': artifact}) + '\n')
        try:
            tree.rename(CACHE_DIR / tree.name)
            say(f'{variant:26} cached from run {run["id"]} ({run["head_sha"][:9]})')
        except OSError as e:
            if e.errno not in (errno.EEXIST, errno.ENOTEMPTY):
                raise
            say(f'{variant:26} cached meanwhile by another fetch')
        return True


def gh_error(e: Exception) -> str:
    lines = (getattr(e, 'stderr', None) or str(e)).strip().splitlines()
    return lines[-1][:120] if lines else repr(e)[:120]


def fetch_missing(boards: list) -> dict:
    """Cache one variant of each board that has none cached: the first, in roster order,
    usable in the newest of the last MASTER_RUNS runs with firmware artifacts that carries it.
    Returns {variant: reason} for the variants of boards left without one. A cached tree is
    never revalidated or replaced: delete it to refetch."""
    want = {b['name']: [v['name'] for v in hil_report.board_variants(b)] for b in boards}
    want = {name: vs for name, vs in want.items()
            if not any((CACHE_DIR / f'cmake-build-{v}').is_dir() for v in vs)}
    if not want:
        return {}
    say(f'fetching firmware for {len(want)} board(s) from CI master runs into {CACHE_DIR}')
    try:
        CACHE_DIR.mkdir(parents=True, exist_ok=True)
        runs = ((run, arts) for run in master_runs() if (arts := run_artifacts(run['id'])))
        for run, artifacts in itertools.islice(runs, MASTER_RUNS):
            for board in [b for b in boards if b['name'] in want]:
                for variant in want[board['name']]:
                    if variant in artifacts and fetch_variant(board, variant, run, artifacts[variant]):
                        del want[board['name']]
                        break
            if not want:
                return {}
        reason = f'not cached: no usable artifact in the latest {MASTER_RUNS} completed master push runs with firmware'
    except (OSError, subprocess.SubprocessError, ValueError, KeyError) as e:
        reason = f'not cached, fetch failed: {gh_error(e)}'
    return {v: reason for vs in want.values() for v in vs}


def flash(board: dict, fw) -> str:
    """One flash attempt, never retried or recovered here: '' on success, else its most
    informative error line. Several backends raise instead of returning nonzero
    (get_serial_dev when a bridge's /dev/serial/by-id node vanishes, a missing config.env, a
    .jlink script OSError); that must become a noted failure, not a crashed row."""
    try:
        ret = hil_flash.flash_primitive(board['flasher']['name'])(board, str(fw))
    except Exception as e:
        return repr(e)[:90]
    if ret.returncode == 0:
        return ''
    return flash_error_line(hil_util.cmd_stdout_text(ret.stdout)) or f'rc={ret.returncode}'


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
        if do_reset:
            reset_fn = hil_flash.reset_primitive(board['flasher']['name'])
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
    means the flash silently didn't take."""
    residue = data.replace(b'Hello from TinyUSB', b'')
    for junk in (b'U', b'\r', b'\n'):
        residue = residue.replace(junk, b'')
    return len(residue) == 0


def host_status(board: dict, note: list) -> str:
    """Row status from a host example's serial output after its flash. board_test-shaped
    output means the parked image is still talking: the example flash silently didn't take,
    a delivery failure."""
    data = check_host_serial(board)
    if data and boardtest_output(data):
        note.append('board_test output after example flash: silent flash no-op')
        return 'flash-failed'
    return 'ok' if data else 'failed'


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
    kind, candidates = light_candidates(board)
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
    if kind == 'host' and board.get('logger') == 'rtt':
        note.append('RTT host boards unsupported')
        return row

    example = variant = fw = None
    for ex in candidates:
        variant, fw = find_image(board, ex)
        if fw:
            example = ex
            break
    if example is None:
        row['status'] = 'flash-failed'
        why = [f'{v["name"]}: {args.uncached[v["name"]]}' for v in hil_report.board_variants(board)
               if v['name'] in args.uncached]
        note.extend(why or [f'no light example firmware under {hil_flash.build_dir}'])
        return row
    if variant != name:
        note.append(f'variant: {variant}')
    bt_fw = None
    if not args.no_park:
        # checked before flashing: a board flashed without a park image is left USB-active
        bt_fw = hil_flash.find_firmware(variant, 'device/board_test', flasher=board['flasher']['name'])
        if bt_fw is None:
            row['status'] = 'flash-failed'
            note.append(f'no board_test firmware for {variant} to park with')
            return row

    lk = lock_board(name)
    if isinstance(lk, str):
        return not_locked(row, lk)
    try:
        pre = find_device(board['uid'])
        old_ino = pre[2] if pre else None

        try:
            err = flash(board, fw)
            if err:
                note.append(f'flash: {err}')
                row['flash'] = f'❌ {Path(example).name}'
                row['status'] = 'flash-failed'
                say(f'{name:26} flash FAILED ({example})')
                return row
            row['flash'] = f'✅ {Path(example).name}'

            if kind == 'host':
                row['status'] = host_status(board, note)
                row['device'] = '✅ serial out' if row['status'] == 'ok' else '❌ no serial out'
            else:
                hit = wait_device(board['uid'], old_ino, ENUM_WAIT)
                row['device'] = f'✅ {hit[1]}' if hit else '❌ not enumerated'
                row['status'] = 'ok' if hit else 'failed'
                if hit and pre and hit[1] == pre[1]:
                    # a silent flash no-op re-enumerates the previous image on the flash's reset
                    note.append(f'{hit[1]} before the flash too: new image unverified')
            say(f'{name:26} {row["flash"]}  {row["device"]}')
            return row
        finally:
            # teardown for EVERY path that attempted a flash (a failed programmer op can
            # still have erased/half-written the target), while the lock is still held
            if bt_fw:
                park_board(board, kind, bt_fw, row, note)
    finally:
        unlock_board(lk)


def park_board(board: dict, kind: str, fw, row: dict, note: list) -> None:
    """Flash board_test and VERIFY it took: board_test never enumerates USB, so a device
    board's cafe device must drop off the bus, and a host board must answer with
    board_test's own output. A board left unparked turns an ok row flash-failed (a
    'failed' verdict is kept: it is the more diagnostic one)."""
    # capture BEFORE the park flash: uid-disappearance only verifies the park if the
    # device was on the bus to begin with
    on_bus_before = kind != 'host' and find_device(board['uid']) is not None
    err = flash(board, fw)
    if err:
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
                        help='firmware parent dir holding cmake-build-<variant>, e.g. cmake-build '
                             f'for a local build (default: the CI artifact cache {CACHE_DIR})')
    parser.add_argument('--scan-only', action='store_true',
                        help='USB presence scan only: no locks, no flashing')
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
    if args.board:
        boards += config.get('boards-skip', [])  # explicitly named parked boards are fair game
        unknown = set(args.board) - {b['name'] for b in boards}
        if unknown:
            sys.exit(f'board(s) not in {cfg_path.name}: {", ".join(sorted(unknown))}')
        boards = [b for b in boards if b['name'] in args.board]

    hil_flash.build_dir = str(args.build_dir or CACHE_DIR)
    hil_util.verbose = args.verbose
    header = (f'pool check: host {host}, config {cfg_path.name}, {len(boards)} boards, '
              f'{"scan-only" if args.scan_only else f"flash via {hil_flash.build_dir}/cmake-build-<variant>"}')

    with stdout_to_stderr() if args.json else contextlib.nullcontext():
        fetch = not args.scan_only and args.build_dir is None
        args.uncached = fetch_missing(boards) if fetch else {}
        rows = check_pool(boards, args, header)
    doc = pool_document(host, str(cfg_path.resolve()), args.scan_only, rows)
    print(json.dumps(doc, ensure_ascii=False) if args.json else render_table(doc))
    counts = doc['counts']
    sys.exit(min(counts['flash-failed'] + counts['failed'], 125))


if __name__ == '__main__':
    main()
