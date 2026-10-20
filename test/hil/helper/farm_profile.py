#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""tinyusb's profile for agentrc's farm check engine (the `farm-check` skill; the engine's
module docstring is the contract), run by helper/hil_farm_check.py.

Per board: the flash probe matched by its uid (never a cafe device), a light example
(`dfu_runtime` for a device board, judged by its cafe device re-enumerating; a host example
for a host-only board, judged by its UART output), then a park with `board_test` from the same
variant, verified. Firmware is never built here: by default it comes from a per-host cache
(CACHE_DIR) of CI artifacts, and a board the cache has no variant of gets one downloaded from
master push runs and kept; -B names another firmware root, never fetched into. Every flash and
reset goes through tinyusb's hil_flash with the engine's tracked runner.

Rosters by hostname: ci -> tinyusb.json, tusb (hifiphile rig) -> hfp.json; any other host
names its roster (a dev PC: test/hil/local.json) or is refused.
"""

import errno
import functools
import itertools
import json
import os
import re
import subprocess
import sys
import tempfile
import time
from datetime import datetime, timedelta, timezone
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))  # hil_flash + the helper package
import hil_flash
from helper import hil_report, hil_util, usb_observe

REPO_ROOT = hil_util.TINYUSB_ROOT
CONFIG_BY_HOST = {'ci': 'tinyusb.json', 'tusb': 'hfp.json'}
# persistent and shared by the host's worktrees; not /tmp, which is RAM on ci.lan
CACHE_DIR = Path.home() / '.cache' / 'tinyusb-hil' / 'firmware'
REPO = 'hathach/tinyusb'
RETENTION_DAYS = 90  # GitHub artifact retention: older runs have nothing left to download
# bounds the search every run repeats for unavailable firmware, counting only runs with
# firmware artifacts: a master push builds the whole HIL matrix (PR runs are change-selected)
# unless it changed no code, when build.yml skips hil-build
MASTER_RUNS = 10

# light-example preference; first one found wins
DEVICE_CANDIDATES = ['device/dfu_runtime']
HOST_CANDIDATES = ['host/device_info', 'host/cdc_msc_hid', 'host/msc_file_explorer_freertos']

ENUM_WAIT = 12       # s, uid wait after flash
SERIAL_WAIT = 6      # s, host-board serial-output wait
PARK_WAIT = 6        # s, for a parked device to leave the bus
HELLO = b'Hello from TinyUSB'
POSTCONDITION = {'device': 'board_test running: the cafe device off the bus',
                 'host': 'board_test running: its hello on the flasher UART'}


def say(msg: str) -> None:
    print(msg, file=sys.__stdout__, flush=True)


# --- roster -----------------------------------------------------------------------------------
def roster(host: str, config, args) -> dict:
    name = config or CONFIG_BY_HOST.get(host)
    if name is None:
        raise LookupError(f'host {host} has no roster here: name one (a dev PC: test/hil/local.json)')
    path = Path(name)
    if not path.exists():
        path = REPO_ROOT / 'test' / 'hil' / name
    with path.open() as f:
        cfg = json.load(f)
    # boards-skip is parked hardware: checked only when -b names it
    return {'path': str(path), 'boards': list(cfg['boards']), 'named_only': list(cfg.get('boards-skip', []))}


def lock_name(board: dict) -> str:
    return board['name']


def add_arguments(parser) -> None:
    parser.add_argument('-B', '--build-dir', default=None,
                        help='firmware parent dir holding cmake-build-<variant>, e.g. cmake-build '
                             f'for a local build (default: the CI artifact cache {CACHE_DIR})')


# --- the bus ----------------------------------------------------------------------------------
def scan_usb() -> dict:
    """busport -> {'serial', 'vidpid'} for every enumerated USB device, root hubs excluded
    (their 'serial' is a fabricated PCI address). Keyed by busport, not serial: an
    Espressif USB-Serial-JTAG bridge and the cafe device it flashes share one (the MAC)."""
    return {d['busport']: {'serial': d['serial'].lower(), 'vidpid': f"{d['vid']}:{d['pid']}"}
            for d in usb_observe.usb_scan()}


def probe_candidates(board: dict) -> list:
    """Busports of every flasher probe matching the board's flasher uid, best tier only,
    excluding VID cafe (TinyUSB DUT firmware): a probe's uid can coincidentally equal its
    DUT's (Espressif USB-Serial-JTAG bridges derive both from the same MAC), and the DUT is
    never the probe. Used by --scan-only too.

    J-Link zero-pads numeric serials (681295394 -> 000681295394): an all-digit uid with no
    exact match matches an all-digit serial only when that serial equals the uid zero-padded
    to the serial's own length (leading zeros only) — never when the zero-stripped uid is
    empty, so a placeholder serial (metro_m4_express's probe legitimately reports '123456')
    can't be mistaken for an unrelated device."""
    u = board['flasher']['uid'].lower()
    probes = sorted((bp, dev['serial']) for bp, dev in scan_usb().items() if not dev['vidpid'].startswith('cafe:'))
    exact = [bp for bp, s in probes if s == u]
    stripped = u.lstrip('0')
    if exact or not (u.isdigit() and stripped):
        return exact
    return [bp for bp, s in probes if s.isdigit() and s == stripped.zfill(len(s))]


def find_device(uid: str):
    """(busport, vidpid, inode) of the board's TinyUSB device (idVendor cafe, so an
    Espressif USB-Serial-JTAG sharing the MAC serial cannot false-pass). usb_scan reads the
    lock-guarded `serial` of cafe devices only, not of every probe and hub on the bus."""
    for d in usb_observe.usb_scan(vid='cafe', serial=uid):
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


def scan_device(board: dict) -> dict:
    """The board's own USB state: enumerated, off-bus (normal when parked in board_test), or
    n/a for a host-only board."""
    host = light_candidates(board)[0] == 'host'
    hit = find_device(board['uid'])
    if hit:
        device = f'✅ {hit[1]} @{hit[0]}'
    else:
        device = '– n/a (host-only)' if host else '⚫ off bus (parked?)'
    return {'device': device, 'notes': ['host-only board'] if host else []}


# --- images -----------------------------------------------------------------------------------
def light_candidates(board: dict):
    """(kind, candidates): kind is 'device' (uid check) or 'host' (serial-output check);
    candidates in preference order, the roster's skip list removed, and an only-list host
    board limited to its own examples (`only` picks tests; a device board is checked with
    dfu_runtime wherever it is built)."""
    tests = board.get('tests', {})
    only = tests.get('only', [])
    skip = set(tests.get('skip', []))  # config's known-broken examples: never pick one
    if tests.get('device') or any(t.startswith('device/') for t in only):
        return 'device', [c for c in DEVICE_CANDIDATES if c not in skip]
    cand = HOST_CANDIDATES + [t for t in only if t.startswith('host/')]
    return 'host', [c for c in dict.fromkeys(cand) if c not in skip and (not only or c in only)]


def find_image(board: dict, example: str):
    """(variant, fw) of the first of the board's variants with `example` built, or
    (None, None)."""
    for v in hil_report.board_variants(board):
        fw = hil_flash.find_firmware(v['name'], example, flasher=board['flasher']['name'])
        if fw:
            return v['name'], fw
    return None, None


def provenance(variant: str, args) -> dict:
    """The engine's provenance of a variant tree: the CI run its cache `.source` names, or
    missing (a -B root has no build receipt to name, a cache tree without `.source` no run)."""
    if args.build_dir is not None:
        return {'kind': 'missing', 'reason': f'firmware root {args.build_dir}: no build receipt'}
    src = CACHE_DIR / f'cmake-build-{variant}' / '.source'
    try:
        s = json.loads(src.read_text())
        # trees cached before the conclusion was recorded have none
        return {'kind': 'ci', 'repo': REPO, 'commit': s['sha'], 'run': s['run'], 'artifact': s['artifact'],
                'conclusion': s.get('conclusion')}
    except (OSError, ValueError, KeyError, TypeError, AttributeError) as e:
        return {'kind': 'missing', 'reason': f'{src}: {e!r}'[:200]}


def plan(board: dict, args, prepared, leave_running) -> dict:
    kind, candidates = light_candidates(board)
    notes = ['host-only board'] if kind == 'host' else []

    def refused(status, why):
        return {'refused': {'status': status, 'notes': notes + why}}
    if why := hil_flash.retained_lock_only(board):
        return refused('flash-failed', [why])
    if kind == 'host' and board.get('logger') == 'rtt':
        return refused('failed', ['RTT host boards unsupported'])
    example = variant = fw = None
    for ex in candidates:
        variant, fw = find_image(board, ex)
        if fw:
            example = ex
            break
    if example is None:
        uncached = (prepared or {}).get('uncached', {})
        why = [f'{v["name"]}: {uncached[v["name"]]}' for v in hil_report.board_variants(board) if v['name'] in uncached]
        return refused('flash-failed', why or [f'no light example firmware under {hil_flash.build_dir}'])
    if variant != board['name']:
        notes.append(f'variant: {variant}')
    park = hil_flash.find_firmware(variant, 'device/board_test', flasher=board['flasher']['name'])
    if park is None:
        return refused('flash-failed', [f'no board_test firmware for {variant} to park with'])
    # every input of both images, before the lock: a light flash must never leave a board its park cannot follow
    for what, image in (('light', fw), ('park', park)):
        if why := input_problem(board, image):
            return refused('flash-failed', [f'{what} image: {why}'])
    prov = provenance(variant, args)
    return {'image': {'path': str(fw), 'variant': variant, 'label': Path(example).name, 'provenance': prov},
            'final': {'postcondition': POSTCONDITION[kind],
                      'image': {'path': str(park), 'variant': variant, 'label': 'board_test', 'provenance': prov}},
            'notes': notes, 'kind': kind}


# --- fetch ------------------------------------------------------------------------------------
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


def input_problem(board: dict, fw) -> str:
    """Why the board's flasher could not flash `fw` from its files ('' when it could)."""
    if not Path(fw).is_file():
        return f'{fw} is missing'
    return hil_flash.esptool_input_problem(fw) if board['flasher']['name'].lower() == 'esptool' else ''


def usable(board: dict, variant: str, root: Path) -> bool:
    """The variant tree under `root` holds a light image and board_test, each with every
    input its flasher reads."""
    flasher = board['flasher']['name']
    find = lambda ex: hil_flash.find_firmware(variant, ex, roots=[str(root)], flasher=flasher)
    light = next(filter(None, map(find, light_candidates(board)[1])), None)
    fws = [light, find('device/board_test')]
    return all(fws) and not any(input_problem(board, fw) for fw in fws)


def fetch_variant(board: dict, variant: str, run: dict, artifact: str, log) -> bool:
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
            log(f'{variant:26} run {run["id"]}: no light image or board_test, trying older')
            return False
        (tree / '.source').write_text(json.dumps(
            {'run': run['id'], 'sha': run['head_sha'], 'artifact': artifact,
             'conclusion': run.get('conclusion')}) + '\n')
        try:
            tree.rename(CACHE_DIR / tree.name)
            log(f'{variant:26} cached from run {run["id"]} ({run["head_sha"][:9]})')
        except OSError as e:
            if e.errno not in (errno.EEXIST, errno.ENOTEMPTY):
                raise
            log(f'{variant:26} cached meanwhile by another fetch')
        return True


def gh_error(e: Exception) -> str:
    lines = (getattr(e, 'stderr', None) or str(e)).strip().splitlines()
    return lines[-1][:120] if lines else repr(e)[:120]


def fetch_missing(boards: list, log=say) -> dict:
    """Cache one variant of each board that has none cached: the first, in roster order,
    usable in the newest of the last MASTER_RUNS runs with firmware artifacts that carries it.
    Returns {variant: reason} for the variants of boards left without one. A cached tree is
    never revalidated or replaced: delete it to refetch."""
    want = {}
    for b in boards:
        try:
            vs = [v['name'] for v in hil_report.board_variants(b)]
        except (ValueError, KeyError):
            continue  # the board's own row reports it: no name at once, a bad variant list in plan
        if not any((CACHE_DIR / f'cmake-build-{v}').is_dir() for v in vs):
            want[b['name']] = vs
    if not want:
        return {}
    log(f'fetching firmware for {len(want)} board(s) from CI master runs into {CACHE_DIR}')
    try:
        CACHE_DIR.mkdir(parents=True, exist_ok=True)
        runs = ((run, arts) for run in master_runs() if (arts := run_artifacts(run['id'])))
        for run, artifacts in itertools.islice(runs, MASTER_RUNS):
            for board in [b for b in boards if b.get('name') in want]:
                for variant in want[board['name']]:
                    if variant in artifacts and fetch_variant(board, variant, run, artifacts[variant], log):
                        del want[board['name']]
                        break
            if not want:
                return {}
        reason = f'not cached: no usable artifact in the latest {MASTER_RUNS} completed master push runs with firmware'
    except (OSError, subprocess.SubprocessError, ValueError, KeyError) as e:
        reason = f'not cached, fetch failed: {gh_error(e)}'
    return {v: reason for vs in want.values() for v in vs}


def prepare(boards: list, args, log) -> dict:
    """Full mode, before any board is locked: the firmware root, the flash provider (an
    incomplete one refuses here) and the cache fetch."""
    hil_flash.build_dir = str(args.build_dir or CACHE_DIR)
    hil_util.verbose = args.verbose
    hil_util.provider()
    return {'uncached': fetch_missing(boards, log) if args.build_dir is None else {}}


# --- inside the engine's reservation ----------------------------------------------------------
def _runner(ctx):
    """ctx.run as hil_flash's `run=`, with this harness's verbosity."""
    return functools.partial(ctx.run, verbose=hil_util.verbose)


def _flash(ctx, board: dict, image: dict):
    op = hil_flash.flash_primitive(board['flasher']['name'])(board, image['path'], run=_runner(ctx))
    ctx.record(op)
    return op


def flash(ctx, board: dict, plan: dict):
    return _flash(ctx, board, plan['image'])


def observe_before(ctx, board: dict, plan: dict):
    return find_device(board['uid'])


def check_host_serial(ctx, board: dict, do_reset: bool = True, want_hello: bool = False) -> bytes | None:
    """Host-only boards never enumerate their uid (their USB port is the host side);
    aliveness = output on the flasher's UART bridge after a reset. A probe byte is
    written each poll so an echo-only firmware (board_test) also answers. Returns
    the first output chunk (b'' when silent, None when the port is absent/drops) so
    the caller can also judge WHAT answered — see boardtest_output().

    The reset runs through the engine's ctx.run and is recorded as it completes, and no UART
    access starts until every run is proven ended (ctx.settle raises Outstanding otherwise).

    do_reset=False listens to the firmware as-is: used right after a flash whose
    own reset already started it — a second openocd/JLink session back-to-back on
    the same probe can fail transiently and leave the target halted."""
    import serial
    ctx.settle()
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
                op = reset_fn(board, run=_runner(ctx))
                ctx.record(op)
                ctx.settle()
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
                if HELLO in data:
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
    residue = data.replace(HELLO, b'')
    for junk in (b'U', b'\r', b'\n'):
        residue = residue.replace(junk, b'')
    return len(residue) == 0


def host_status(data: bytes | None, note: list) -> str:
    """Row status from a host example's serial output after its flash. board_test-shaped
    output means the parked image is still talking: the example flash silently didn't take,
    a delivery failure."""
    if data and boardtest_output(data):
        note.append('board_test output after example flash: silent flash no-op')
        return 'flash-failed'
    return 'ok' if data else 'failed'


def observe(ctx, board: dict, plan: dict, before) -> dict:
    """The light image's return: a host board's UART output after a reset, a device board's
    cafe device re-enumerated (a new sysfs inode)."""
    notes, unverified = [], []
    if plan['kind'] == 'host':
        data = check_host_serial(ctx, board)
        status = host_status(data, notes)
        device = '✅ serial out' if status == 'ok' else '❌ no serial out'
        evidence = ['no flasher serial port' if data is None else
                    f'flasher UART after a reset: {len(data)} bytes, {data[:48]!r}']
    else:
        hit = wait_device(board['uid'], before[2] if before else None, ENUM_WAIT)
        status = 'ok' if hit else 'failed'
        device = f'✅ {hit[1]}' if hit else '❌ not enumerated'
        was = f'{before[1]} @{before[0]} inode {before[2]}' if before else 'off bus'
        evidence = [f'{hit[1]} @{hit[0]} inode {hit[2]} (before: {was})' if hit else
                    f'no new cafe device within {ENUM_WAIT}s (before: {was})']
        if hit and before and hit[1] == before[1]:
            # a silent flash no-op re-enumerates the previous image on the flash's reset
            unverified.append(f'{hit[1]} before the flash too: new image unverified')
            notes.extend(unverified)
    return {'status': status, 'device': device, 'notes': notes, 'unverified': unverified, 'evidence': evidence}


def final_state(ctx, board: dict, plan: dict, observation) -> dict:
    """Flash board_test and verify it took: board_test never enumerates USB, so a device
    board's cafe device must drop off the bus, and a host board must answer with board_test's
    own output. Verified only where that is seen, once the park flash is proven ended."""
    # captured BEFORE the park flash: the device's disappearance only verifies the park if
    # it was on the bus to begin with
    on_bus_before = plan['kind'] != 'host' and find_device(board['uid']) is not None
    op = _flash(ctx, board, plan['final']['image'])
    out = {'verified': False, 'evidence': [], 'notes': []}

    def unverified(why):
        out['evidence'].append(why)
        out['notes'].append(why)
        return out
    if op.returncode != 0:
        text = hil_util.cmd_stdout_text(op.stdout) + '\n' + hil_util.cmd_stdout_text(op.stderr)
        return unverified(f'park flash failed: {ctx.flash_error_line(text) or f"rc={op.returncode}"}')
    ctx.settle()
    if plan['kind'] == 'host':
        # no second reset (the park flash's own reset started board_test); POSITIVE
        # marker: its hello must appear, and stale bridge-FIFO output alongside it is not
        # disqualifying
        data = check_host_serial(ctx, board, do_reset=False, want_hello=True)
        if not (data and HELLO in data):
            return unverified('park unverified: no board_test output')
        out['evidence'].append('board_test hello on the flasher UART after the park flash')
    elif not on_bus_before:
        # never enumerated this run: its absence cannot tell a park from a silent no-op
        return unverified('park unverified (device already off bus)')
    else:
        deadline = time.monotonic() + PARK_WAIT
        while find_device(board['uid']) is not None:
            if time.monotonic() >= deadline:
                return unverified('park unverified: device still enumerated')
            time.sleep(0.5)
        out['evidence'].append('cafe device left the bus after the park flash')
    out['verified'] = True
    return out
