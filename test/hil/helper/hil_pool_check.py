#!/usr/bin/env python3
"""Quick HIL pool health check.

For every board in the rig's HIL config: is the flash probe on the USB bus, does a
light example flash, and does the board's USB device (uid) come back up? Missing
firmware is BUILT on the spot (tools/build.py, idf.py for espressif; one get_deps
retry) — never skipped; --no-build opts out. Applies only per-device-safe recovery
(probe authorized-toggle, board reset/re-flash) and prints a markdown summary
table. Row statuses: ok (flashed and verified; under --scan-only: probe present —
the scan checks presence only), flash-failed (firmware delivery failed: probe
missing, build failed, flasher error, silent flash no-op, park not verified),
failed (the check ran but did not verify: flashed with no enumeration/serial, or
the check itself errored), locked (board flock held by another process —
reported, never waited on or bypassed).

Config is picked by hostname unless given: ci -> tinyusb.json, tusb (hifiphile
rig) -> hfp.json, anything else is a dev PC -> local.json.

Lives in test/hil/helper/ beside hil_lock.py; imports it and hil_flash; board
recovery uses the repo's .claude/skills/usb-kernel-recover/scripts/usb_recover.sh.
"""

import argparse
import io
import json
import glob
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
from helper import hil_lock, hil_util

REPO_ROOT = hil_util.TINYUSB_ROOT
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
_STRANDED_WARNED = False   # scan_usb's caveat: once per process, not once per poll
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
    # device that will not answer is simply absent from the table; the footer says so.
    devs = hil_util.usb_scan()
    # ONCE per process, at SCAN time, not only in the footer: this tool prints rows as it
    # goes over minutes, so a board dropped from the scan says "probe MISSING" within
    # seconds while the only qualification would arrive after the final counts -- and an
    # operator acting on the streaming output, or a run cut short by ^C, never sees it.
    global _STRANDED_WARNED
    if hil_util.sysfs_stranded() and not _STRANDED_WARNED:
        _STRANDED_WARNED = True
        say('WARNING: a bounded sysfs read gave up; rows below that say a probe or board '
            'is missing may be this scan losing sight of healthy hardware. Find the '
            'wedged device (usb-kernel-recover) and re-run.')
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
    except OSError:
        # NB: conflates a held flock with open() failures (EACCES/EROFS/ENOSPC) — benign
        # while everything on the rig runs as one uid
        info = hil_lock.read_record(name)
        return json.dumps(info) if info else 'unknown holder'
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


def can_recover() -> bool:
    if not USB_RECOVER.is_file():
        return False
    try:
        # run_cmd, not subprocess.run: run's post-timeout reap is an UNBOUNDED wait(), and
        # our kill bounces off a setuid-root sudo with EPERM, leaving communicate() on a
        # pipe that never closes. run_cmd killpgs, escalates through sudo, reaps bounded.
        r = hil_util.run_cmd('sudo -n true', timeout=10, quiet=True)
    except OSError:  # sudo not installed
        return False
    return r.returncode == 0


def recover_probe(uid: str, busport: str) -> bool:
    """Soft-replug an enumerated-but-wedged probe: deauthorize+reauthorize (no VBUS cut,
    touches only this device). Success = the probe re-enumerated (new sysfs inode), not the
    helper's exit code, which flakes while the toggle works. J-Links respond with a full
    disconnect and can stay off the bus for >8 s."""
    pre = find_usb(uid)
    # Bounded through run_cmd (same reason as can_recover): the sysfs authorized store can
    # block in D state on a wedged device, and this runs while the board's release-
    # PROTECTED flock is held -- a hang here would lock the board until the host reboots.
    cmd = ' '.join(shlex.quote(a) for a in
                   ['sudo', '-n', str(USB_RECOVER), 'authorized', busport])
    if hil_util.run_cmd(cmd, timeout=30, quiet=True).returncode == 124:
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
        if hil_flash.find_firmware(vn, example, flasher=board['flasher']['name']):
            if vn != name and note is not None and f'variant: {vn}' not in note:
                note.append(f'variant: {vn}')
            return vn
    return name


def pick_example(board: dict, note: list, build_missing: bool = True):
    """(example, kind, variant, fw) with built firmware for this board; kind is
    'device' (uid check) or 'host' (serial-output check); variant is the resolved
    build-dir variant that has it (see resolve_variant); fw is the firmware path to
    flash, extension included. When nothing is built and build_missing is set (the default —
    never skip a board for lack of a build), the preferred candidate is built on
    the spot via ensure_fw."""
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
        fw = hil_flash.find_firmware(variant, ex, flasher=board['flasher']['name'])
        if fw:
            return ex, kind, variant, fw
    if not build_missing:
        return None, kind, None, None
    # nothing built anywhere: build the preferred candidate (an only-list board
    # must get one of its own examples — dfu_runtime etc. may not even configure)
    pref = [c for c in dict.fromkeys(cand) if c not in skip and (not only or c in only)]
    if not pref:
        return None, kind, None, None
    variant = (board.get('variant') or [{'name': board['name']}])[0]['name']
    for ex in pref[:2]:  # the second candidate covers a preferred example that fails to build
        fw = ensure_fw(board, variant, ex, note)
        if fw:
            return ex, kind, variant, fw
    return None, kind, None, None


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
    exception must not skip the caller's retry/recovery ladder. Returns (rc, error line)."""
    try:
        ret = fn(*fn_args)
        if ret.returncode == 0:
            return 0, ''
        err = flash_error_line(hil_util.cmd_stdout_text(ret.stdout))
        return ret.returncode, err or f'rc={ret.returncode}'
    except Exception as e:
        return -1, repr(e)[:90]


def flash(board: dict, fw, allow_recovery: bool, probe_port: str, note: list) -> bool:
    """Flash the resolved firmware with one retry; on repeated failure soft-replug the
    probe and always make one final attempt afterward, confirmed replug or not — some
    probes (WCH-Link, ST-Link, CP210x, picoprobe) keep their sysfs kobject across an
    authorized toggle instead of dropping off the bus. Returns True on success.

    `fw` comes from pick_example: a re-resolve here would use the global search policy and
    miss a firmware ensure_fw just built into cmake-build/ under an exclusive -B."""
    fn = getattr(hil_flash, f'flash_{board["flasher"]["name"].lower()}')
    for attempt in range(3):
        if attempt == 2:
            if not (allow_recovery and probe_port):
                return False
            cur = find_usb(board['flasher']['uid'])
            if cur is None:
                # probe gone from the bus: its old busport may now hold an UNRELATED device
                # (bus renumbering) and the helper only checks occupancy, so toggling would
                # deauthorize an innocent fixture
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
        if rc == 127:  # flasher binary missing: retries/probe recovery can't fix env
            note.append(f'flasher tool missing ({err}) — esptool needs the ESP-IDF env '
                        f'(. "$IDF_PATH/export.sh")'
                        if board['flasher']['name'].lower() == 'esptool' else
                        f'flasher tool missing: {err}')
            return False
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
    the same probe can fail transiently and leave the target halted.

    "logger": "rtt" boards have no VCOM: the same check runs over the probe's RTT
    console instead. The reset happens BEFORE the console opens (it owns the probe),
    which also zeroes the .bss ring — so pre-reset backlog cannot count as life, and
    without a reset Commander delivers the boot burst the preceding flash left."""
    if board.get('logger') == 'rtt':
        if do_reset:
            # a failed reset leaves the previous run's ring intact: attaching anyway would
            # score stale output as life, so bail to host_alive's board_test reflash ladder
            rc, err = call_flasher(getattr(hil_flash, f'reset_{board["flasher"]["name"].lower()}'), board)
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
        if do_reset:
            getattr(hil_flash, f'reset_{board["flasher"]["name"].lower()}')(board)
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


def build_example(board: dict, variant: str, example: str) -> int:
    """Build one example for this board: tools/build.py (same invocation shape as
    hil_test.build_board), or idf.py directly for espressif (tools/build.py's esp branch
    ignores -T and builds everything; variant flags travel as -DCFLAGS_CLI, the channel
    tools/build.py uses). Bounded and process-group-killed via run_cmd; 600 s covers a
    first configure+build of an SDK-heavy family (pico, nrf, esp). Builds normally run
    pre-lock, so a board flock is not held here except on rare recovery paths. Per-build
    compile parallelism is capped at cpu/-j so -j concurrent builds cannot swamp sibling
    workers' verification windows. Returns the returncode (127 = ESP-IDF env missing)."""
    name = board['name']
    variants = board.get('variant') or [{'name': name}]
    vcfg = next((v for v in variants if v['name'] == variant), variants[0])
    if board['flasher']['name'].lower() == 'esptool':
        if not shutil.which('idf.py'):
            return 127  # ESP-IDF env not sourced in this shell
        # -B keyed off the VARIANT so ensure_fw's post-build lookup finds it
        cmd = ['idf.py', '-C', f'examples/{example}',
               '-B', f'cmake-build/cmake-build-{vcfg["name"]}/{example}',
               '-G', 'Ninja', f'-DBOARD={name}', 'build']
        for d in vcfg.get('defines', []):
            cmd.insert(-1, f'-D{d}')
        if vcfg.get('flags'):
            cmd.insert(-1, f'-DCFLAGS_CLI={vcfg["flags"]}')
        # the IDF component manager writes examples/<ex>/dependencies.lock in the
        # SOURCE tree (idf.py -B relocates only the build dir), so concurrent esp
        # builds of one example for different targets corrupt each other's solve
        with _esp_lock, _build_sem:
            return hil_util.run_cmd(shlex.join(cmd), cwd=str(hil_util.TINYUSB_ROOT),
                                    timeout=600).returncode
    cmd = [sys.executable, str(hil_util.TINYUSB_ROOT / 'tools' / 'build.py'),
           '-b', name, '-T', Path(example).name,
           '-j', str(max(1, (os.cpu_count() or _jobs) // _jobs))]
    if vcfg['name'] != name:
        cmd += ['--build-name', vcfg['name']]
    for d in vcfg.get('defines', []):
        cmd += ['-D', d]
    for tok in vcfg.get('flags', '').split():
        cmd += [f'--cflag={tok}']
    with _build_sem:
        return hil_util.run_cmd(shlex.join(cmd), cwd=str(hil_util.TINYUSB_ROOT),
                                timeout=600).returncode


_deps_lock = threading.Lock()  # one get_deps at a time (it also drains _build_sem)
_esp_lock = threading.Lock()   # idf.py mutates source-tree dependencies.lock per example
_no_build = False              # --no-build: ensure_fw never invokes a build
_jobs = 4                      # mirrors -j; set in main before the pool starts
_build_sem = threading.BoundedSemaphore(4)  # build slots; get_deps drains ALL (exclusive)
_builds: dict = {}             # (variant, example) -> (fw|None, reason): one attempt per run


def ensure_fw(board: dict, variant: str, example: str, note: list):
    """Firmware for `example`, building it when absent — never skip a board for lack of a
    build (--no-build opts out). One retry with deps fetched and the CMake caches dropped
    when the first build fails (fresh checkouts lack the family deps; a cache configured
    in a broken env poisons every later attempt). Returns the firmware path, or None with
    the failure noted. Call BEFORE taking the board lock: builds are long. One attempt per
    (variant, example) per run, memoized in _builds, so a repeat call (park, under the
    held flock) resolves instantly even when an exclusive -B hides the fresh artifact."""
    fw = hil_flash.find_firmware(variant, example, flasher=board['flasher']['name'])
    if fw:
        return fw
    key, base = (variant, example), Path(example).name
    if key in _builds:
        return _builds[key][0]
    if _no_build:
        _builds[key] = (None, 'disabled')
        note.append(f'build skipped (--no-build): {base}')
        return None
    rc = build_example(board, variant, example)
    if rc == 127 and board['flasher']['name'].lower() == 'esptool':
        _builds[key] = (None, 'no-env')
        note.append(f'cannot build {base}: ESP-IDF env missing '
                    f'(. "$IDF_PATH/export.sh")')
        return None
    if rc == 124:  # hung build: a deps/cache retry cannot cure it, don't double the stall
        _builds[key] = (None, 'timeout')
        note.append(f'build timeout: {base}')
        return None
    if rc != 0:
        # retry once with deps fetched and the CMake caches dropped (cache only — a tree
        # wipe would destroy every other example's firmware). get_deps git-resets shared
        # deps that are already present, so it drains ALL build slots first.
        with _deps_lock:
            for _ in range(_jobs):
                _build_sem.acquire()
            try:
                r = hil_util.run_cmd(shlex.join([sys.executable, str(hil_util.TINYUSB_ROOT / 'tools' / 'get_deps.py'),
                                                 '-b', board['name']]),
                                     cwd=str(hil_util.TINYUSB_ROOT), timeout=600)
            finally:
                for _ in range(_jobs):
                    _build_sem.release()
        if r.returncode != 0:
            note.append('get_deps failed')
        bd = hil_util.TINYUSB_ROOT / 'cmake-build' / f'cmake-build-{variant}'
        # esp configures one level deeper (<variant>/<example>/): wipe both layouts
        for d in (bd, bd / example):
            shutil.rmtree(d / 'CMakeFiles', ignore_errors=True)
            (d / 'CMakeCache.txt').unlink(missing_ok=True)
        rc = build_example(board, variant, example)
    if rc != 0:
        _builds[key] = (None, 'fail')
        note.append(f'build failed: {base}')
        return None
    # both build paths write to cmake-build/, so look there even when an explicit -B
    # narrowed the global search — this is OUR fresh build, not a stale fallback
    fw = hil_flash.find_firmware(variant, example,
                                 roots=[hil_flash.build_dir, 'cmake-build'],
                                 flasher=board['flasher']['name'])
    _builds[key] = (fw, 'ok' if fw else 'no-fw')
    note.append(f'built {base}' if fw else f'build produced no firmware: {base}')
    return fw


def ensure_board_test(board: dict, variant: str, note: list):
    """board_test firmware for parking, building it if absent (via ensure_fw).
    Espressif included — tools/build.py builds board_test for that family too;
    the build just needs the ESP-IDF env (127 → noted, park is then skipped)."""
    fw = hil_flash.find_firmware(variant, 'device/board_test', flasher=board['flasher']['name'])
    if fw:
        return fw
    variants = board.get('variant') or [{'name': board['name']}]
    if not any(v['name'] == variant for v in variants):
        variant = variants[0]['name']
    return ensure_fw(board, variant, 'device/board_test', note)


def verdict(row: dict, ok: bool) -> str:
    """Row status for a verification result, preserving a 'flash-failed' a deeper
    layer already recorded (silent flash no-op, board_test delivery failure)."""
    return 'ok' if ok else ('flash-failed' if row['status'] == 'flash-failed' else 'failed')


def host_alive(board: dict, note: list, row: dict, flashed_example: bool = False) -> bool:
    """Serial aliveness with recovery: silent -> (build and) flash board_test (it
    hellos every second and echoes) -> recheck. Also cures a silent flash no-op
    that left the board crashed.

    With flashed_example=True (a host example was just flashed), board_test-shaped
    output FAILS the check: the parked image still talking means the example flash
    silently didn't take — the host analog of the device path's PID check.

    Side effect: delivery-class failures (silent no-op, board_test build/flash
    failure) set row['status'] = 'flash-failed' so verdict() preserves the cause;
    the caller derives the final status from the return value via verdict()."""
    data = check_host_serial(board)
    if data:
        if flashed_example and boardtest_output(data):
            note.append('board_test output after example flash: silent flash no-op')
            row['status'] = 'flash-failed'
            return False
        return True
    variant = resolve_variant(board, 'device/board_test', note)
    fw = ensure_board_test(board, variant, note)
    if fw is None:
        note.append('serial silent; board_test unavailable')
        row['status'] = 'flash-failed'
        return False
    say(f'{board["name"]:26} recovery: serial silent, flashing board_test')
    rc, err = call_flasher(getattr(hil_flash, f'flash_{board["flasher"]["name"].lower()}'), board, str(fw))
    if rc != 0:
        note.append(f'serial silent; board_test flash failed: {err}')
        row['status'] = 'flash-failed'
        return False
    if not check_host_serial(board):
        return False
    if flashed_example:
        # board_test talking proves the BOARD is alive, but the just-flashed
        # example never produced serial — that verification still fails
        note.append('example silent; board alive via board_test reflash')
        return False
    note.append('recovered via board_test reflash')
    return True


def device_recover_and_check(board: dict, example: str, variant: str, old_ino, note: list, row: dict, seen: dict) -> bool:
    """Wait for the flashed board's uid to re-enumerate; on timeout, try one board
    reset (skipped for flashers with no hardware reset — see hil_flash.RESET_NOOP,
    it would just burn the wait) and wait again.

    The PID policy is deliberately asymmetric. Pre-reset, the re-enumeration was
    caused by the flash itself, so a PID mismatch most likely means the build dir
    is stale (the flash DID write what find_firmware found) — warn, don't fail —
    UNLESS the firmware was built this very run: then 'stale build' is impossible
    and the mismatch can only be a silent flash no-op, which fails. Post-reset,
    the re-enumeration proves nothing about the flash (the reset alone explains
    it), so a mismatch is treated as a silent flash no-op and fails; an unknown
    expected PID scores ok with a 'pid unverified' note in both paths."""
    name = board['name']
    expected_pid = get_expected_pid(example)
    built_this_run = _builds.get((variant, example), (None, ''))[1] == 'ok'

    def seen_hit(hit):
        seen[board['uid']] = {'name': name, 'busport': hit[0], 'when': time.strftime('%Y-%m-%d %H:%M')}

    hit = wait_device(board['uid'], None, old_ino, ENUM_WAIT)
    if hit:
        if expected_pid is not None and not hit[1].endswith(expected_pid):
            if built_this_run:
                row['device'] = f'❌ {hit[1]}'
                note.append(f'pid {hit[1]}, this run built {expected_pid}: silent flash no-op')
                row['status'] = 'flash-failed'
                return False
            note.append(f'⚠ pid {hit[1]}, source says {expected_pid}: stale build or silent flash no-op')
        elif expected_pid is None:
            note.append('pid unverified')
        row['device'] = f'✅ {hit[1]}'
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
    row['status'] = 'flash-failed'
    return False


def check_board(board: dict, args, allow_recovery: bool, seen: dict) -> dict:
    name = board['name']
    row = {'name': name, 'probe': '❌ missing', 'flash': '–', 'device': '–', 'note': [], 'status': 'failed'}
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

    # existing firmware only; a missing build is built further down (after a lock peek),
    # except in scan/no-build modes and never for a missing probe
    example, kind, variant, fw = pick_example(board, note, build_missing=False)
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
    need_example = example is None and not args.no_build
    # board_test is also host_alive's recovery image, so host boards pre-build it
    # even under --no-park; --no-build gates EVERY build, board_test included
    need_bt = (not args.no_build
               and (not args.no_park or kind == 'host')
               and hil_flash.find_firmware(bt_variant, 'device/board_test',
                                           flasher=board['flasher']['name']) is None)
    if need_example or need_bt:
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
        if need_example:
            example, kind, variant, fw = pick_example(board, note, build_missing=True)
        if need_bt and (example is not None or kind == 'host'):
            # skip the park build when the example build already failed on a device board:
            # the row returns before any flash/park could use it
            ensure_board_test(board, bt_variant, note)

    if example is None:
        if not any(n.startswith(('build failed', 'build timeout', 'build produced',
                                 'build skipped', 'cannot build')) for n in note):
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
            if not flash(board, fw, allow_recovery, probe[0], note):
                row['flash'] = f'❌ {Path(example).name}'
                row['status'] = 'flash-failed'
                say(f'{name:26} flash FAILED ({example})')
                return row
            row['flash'] = f'✅ {Path(example).name}'

            if kind == 'host':
                ok = host_alive(board, note, row, flashed_example=True)
                row['device'] = '✅ serial out' if ok else '❌ no serial out'
            else:
                ok = device_recover_and_check(board, example, variant, old_ino, note, row, seen)
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
    """Re-park with board_test, building it if absent (ensure_board_test), and
    VERIFY it took: board_test never enumerates USB, so a device board's cafe
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
    fw = ensure_board_test(board, variant, note)
    if fw is None:
        if any(n.startswith('cannot build board_test') for n in note):
            note.append('park skipped (no ESP-IDF env)')
        else:
            # --no-build disables builds, not parking (--no-park is that opt-out):
            # a board left running a USB-active image is unparked either way
            note.append('unparked: board_test not built (--no-build)'
                        if any(n.startswith('build skipped (--no-build): board_test') for n in note)
                        else 'unparked: board_test unavailable (build failed/timed out)')
            if row['status'] == 'ok':
                row['status'] = 'flash-failed'
        return
    rc, err = call_flasher(getattr(hil_flash, f'flash_{board["flasher"]["name"].lower()}'),
                           board, str(fw))
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


def check_board_safe(board: dict, args, allow_recovery: bool, seen: dict) -> dict:
    """Isolate one board's exceptions: a crashing worker must not discard every
    other board's row, the table, the topology, and the seen-cache write."""
    try:
        return check_board(board, args, allow_recovery, seen)
    except Exception as e:
        name = board.get('name', '?')
        say(f'{name:26} INTERNAL ERROR: {e!r}')
        return {'name': name, 'probe': '–', 'flash': '–', 'device': '❌ error',
                'note': [repr(e)[:120]], 'status': 'failed'}


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
    # toolchain/flasher CLIs live in the user bin dirs, which non-login shells may lack --
    # the same PATH shim hil_ci.sh applies on the remote side
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
    global _no_build, _jobs, _build_sem
    _no_build = args.no_build
    _jobs = max(1, args.jobs)
    _build_sem = threading.BoundedSemaphore(_jobs)

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
    hil_util.verbose = args.verbose
    if args.build_dir is None:
        # default mode: search both standard layouts (cmake-build/ from tools/build.py and
        # ESP-IDF, examples/ from manual builds). An EXPLICIT -B stays exclusive: the caller
        # named an artifact tree, so a miss must report rather than flash an older build.
        hil_flash.EXTRA_BUILD_DIRS = ['cmake-build', 'examples']
    allow_recovery = not args.scan_only and can_recover()
    seen = {}
    try:
        loaded = json.loads(SEEN_CACHE.read_text())
        if isinstance(loaded, dict):  # tolerate a torn/hand-edited cache
            seen = {k: v for k, v in loaded.items() if isinstance(v, dict)}
    except (OSError, ValueError):
        pass

    roots = ' + '.join(dict.fromkeys([hil_flash.build_dir, *hil_flash.EXTRA_BUILD_DIRS]))
    say(f'pool check: host {host}, config {cfg_path.name}, {len(boards)} boards, '
        f'{"scan-only" if args.scan_only else f"flash via {{{roots}}}/cmake-build-<board>"}'
        f'{"" if allow_recovery or args.scan_only else ", recovery unavailable (no sudo -n / usb_recover.sh)"}')

    if args.verbose:
        rows = [check_board_safe(b, args, allow_recovery, seen) for b in boards]
    else:
        with io.StringIO() as spool, ThreadPoolExecutor(max_workers=args.jobs) as pool:
            sys.stdout = spool  # silence hil_util.run_cmd's COMMAND FAILED dumps; say() uses __stdout__
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

    print('\nUSB topology (controller → root-port subtree):')
    for line in controller_summary():
        print(f'  {line}')

    counts = {'ok': 0, 'flash-failed': 0, 'failed': 0, 'locked': 0}
    for r in rows:
        counts[r.get('status', 'failed')] += 1
    print(f'\n{counts["ok"]} ok · {counts["flash-failed"]} flash-failed · {counts["failed"]} failed '
          f'· {counts["locked"]} locked · in {time.monotonic() - t0:.0f}s')
    if hil_util.sysfs_stranded():
        # Without this the table is the worst kind of wrong: a device whose `serial` never
        # answered is absent from the scan, which prints as "probe MISSING"/"off bus" for
        # hardware that is physically present -- during exactly the incident this tool is
        # run to diagnose, and it sends the operator to power-cycle a healthy rig.
        print('WARNING: at least one sysfs read did not answer within '
              f'{hil_util.SYSFS_READ_GRACE:.0f}s, so rows above that say a probe or board '
              f'is missing may be this tool losing sight of healthy hardware rather than '
              f'absent hardware. Find the wedged device (see the usb-kernel-recover '
              f'skill) and re-run before acting on the table.')
    sys.exit(min(counts['flash-failed'] + counts['failed'], 125))


if __name__ == '__main__':
    main()
