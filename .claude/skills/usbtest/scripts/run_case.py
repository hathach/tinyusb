#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Run chosen usbtest cases on one roster board: lock, flash, enumerate, run, park or leave, release.

  run_case.py --config CONFIG --board BOARD --tests N[,N...] --after park|leave
              [--variant NAME] [--timeout S] [--allow-concurrent]

For a session that has NOT taken the board. Inside a lock you already hold, with the firmware
already flashed, run `test/hil/usbtest.py --serial <uid> --tests N --json` directly instead.

Refuses before touching hardware (exit 2) when: the board or variant is unknown or ambiguous,
the board sits in boards-skip, does not run device/usbtest or lacks a uid or flasher name in the
roster, the usbtest (or, for --after park, the board_test) firmware is not built, the board lock
is held or unusable, or a testusb, usbtest.py, hil_test.py or other run_case.py process is alive
on this host, checked before and again after taking the lock. That check is host-wide and does
not see a hil_test.py started after it, nor join hil_test.py's per-controller battery permits:
--allow-concurrent skips it only once you have established that no battery shares this board's
host controller.

Flashes with the roster's flasher from cmake-build/cmake-build-<variant> (the build skill's
--shared --variants layout), waits for cafe:4010 with the board's serial, and runs usbtest.py
with post-hang recovery when the board's recovery flasher can deliver it. --after park flashes
device/board_test afterwards unless the device is wedged or the verdict is incomplete.

stdout ends with one JSON line: {"pass", "board", "variant", "tests", "cases", "wedged",
"boardState", "error"}, each case with usbtest.py's num, name, status, detail and, when it captured
them, stderr and dmesg. Exit 0 every case passed, 1 a case failed or the run failed after the
lock was taken, 2 refused before any hardware action. A SIGTERM (`hil_lock.py release` sends one
to the lock holder) kills the running flasher or usbtest.py, reports boardState unknown with a
verdict line, then releases the lock.
"""
import argparse
import json
import os
import signal
import sys
import tempfile
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[4]
HIL_DIR = ROOT / 'test' / 'hil'
sys.path.insert(0, str(HIL_DIR))
sys.path.insert(0, str(ROOT / 'tools'))
import ci_select  # noqa: E402
import hil_flash  # noqa: E402
import usbtest  # noqa: E402
from helper import hil_lock, hil_report, hil_util  # noqa: E402

ENUM_TIMEOUT = 8        # hil_test.py ENUM_TIMEOUT
SETTLE = 3              # hil_test.py USBTEST_SETTLE: enumeration can bounce once after a flash
PEERS = ('testusb', 'usbtest.py', 'hil_test.py', 'run_case.py')
HELPER_S = usbtest.HELPER_TIMEOUT + 5   # a sudo helper at its bound plus usbtest.run()'s reap
# usbtest.py's start before case 1, its steps at their bounds: the 8 s device wait, 3 s of
# host-compat retries, setpci, modprobe, the id-registration lock, the new_id and pattern writes
# (sysfs_write: 15 s tee + 5 s reap) and the 3 s bind. An estimate, not a bound: the battery
# budget starts before it, so a slower start costs cases (reported BUDGET), not a kill.
SETUP_S = 8 + 3 + 2 * HELPER_S + usbtest.REGISTER_LOCK_TIMEOUT + 2 * (15 + 5) + 3
# each case: its timeout, the 5 s post-kill reap, the dmesg read, and the find_device between cases
CASE_EXTRA_S = 5 + HELPER_S + 5
PROC = Path('/proc')


class Refused(Exception):
    """Stop before any hardware action."""


class Terminated(BaseException):
    """SIGTERM: a BaseException, like KeyboardInterrupt, so run_cmd kills its child on the way out
    and no `except Exception` on the path (flash()'s) mistakes it for a failed step."""


def on_sigterm(signum, frame):
    signal.signal(signal.SIGTERM, signal.SIG_IGN)   # a second one must not cut the cleanup short
    raise Terminated()


def resolve(config, board_name, variant):
    """(board, variant name) from the roster, or Refused naming the alternatives."""
    try:
        cfg = json.loads(Path(config).read_text())
    except (OSError, ValueError) as e:
        raise Refused(f'cannot read {config}: {e}')
    listed = [b['name'] for b in cfg.get('boards', [])]
    if listed.count(board_name) > 1:
        raise Refused(f'{board_name} appears {listed.count(board_name)} times in {config}')
    boards = {b['name']: b for b in cfg.get('boards', [])}
    if board_name not in boards:
        if any(b['name'] == board_name for b in cfg.get('boards-skip', [])):
            raise Refused(f'{board_name} is in boards-skip of {config}')
        raise Refused(f'{board_name} is not a board in {config}')
    board = boards[board_name]
    if 'device/usbtest' not in ci_select.board_tests(board):
        raise Refused(f'{board_name} does not run device/usbtest in {config} (its "tests" entry); '
                      f'its device port may not reach this host')
    flasher = board.get('flasher')
    if not (isinstance(board.get('uid'), str) and board['uid'] and isinstance(flasher, dict)
            and isinstance(flasher.get('name'), str) and flasher['name']):
        raise Refused(f'{board_name} in {config} needs a "uid" and a "flasher" with a "name"')
    try:
        names = [v['name'] for v in hil_report.board_variants(board)]
    except ValueError as e:
        raise Refused(f'{board_name}: {e}')
    if len(set(names)) != len(names):
        raise Refused(f'{board_name} lists a variant name twice: {", ".join(names)}')
    if variant is None:
        if len(names) > 1:
            raise Refused(f'{board_name} has variants {", ".join(names)}: pass --variant')
        variant = names[0]
    elif variant not in names:
        raise Refused(f'{board_name} has no variant {variant}; it has {", ".join(names)}')
    return board, variant


def lineage():
    """This process and its ancestors: a wrapper such as sudo or timeout names run_case.py too."""
    pids, pid = set(), os.getpid()
    while pid > 1 and pid not in pids:
        pids.add(pid)
        try:
            pid = int((PROC / str(pid) / 'stat').read_text().rsplit(')', 1)[1].split()[1])
        except (OSError, ValueError, IndexError):
            break
    return pids


def live_peers():
    """([(pid, name)], complete) for processes that run or drive a battery on this host."""
    found, complete = [], True
    own = lineage()
    # hidepid hides other users' processes without an error, and testusb may run under sudo:
    # detect the restriction itself
    if os.geteuid() != 0 and not os.access(PROC / '1' / 'cmdline', os.R_OK):
        complete = False
    for d in PROC.glob('[0-9]*'):
        if int(d.name) in own:
            continue
        try:
            argv = (d / 'cmdline').read_bytes().split(b'\0')
        except (FileNotFoundError, ProcessLookupError):
            continue            # exited while we looked
        except OSError:
            complete = False    # could be a battery we cannot see
            continue
        # argv[0] for testusb, argv[1] for a script run by python3, later under sudo -n
        names = [os.path.basename(a.decode(errors='replace')) for a in argv[:4]]
        hit = next((n for n in names if n in PEERS), None)
        if hit:
            found.append((int(d.name), hit))
    return found, complete


def check_peers():
    peers, complete = live_peers()
    if peers:
        raise Refused('a battery may be running on this host: '
                      + ', '.join(f'{name} (pid {pid})' for pid, name in peers))
    if not complete:
        raise Refused('cannot read every process on this host, so a running battery '
                      'cannot be ruled out')


def firmware(variant, example, flasher):
    fw = hil_flash.find_firmware(variant, example, flasher=flasher)
    if fw is None:
        raise Refused(f'no {example} build for {variant} (flasher {flasher}): build it with the '
                      f'build skill\'s --shared --variants <config>')
    return str(fw)


def flash(board, fw):
    try:
        ret = hil_flash.flash_primitive(board['flasher']['name'])(board, fw)
    except Exception as e:   # a raising flasher is a failed flash, not a crash holding the lock
        return f'flash raised {type(e).__name__}: {e}'
    if ret.returncode != 0:
        return f'flash failed (rc {ret.returncode}): {hil_util.cmd_stdout_text(ret.stdout)[-300:]}'
    return None


def enumerated(uid):
    deadline = time.monotonic() + ENUM_TIMEOUT
    while time.monotonic() < deadline:
        if hil_util.usb_scan(vid_pid=(usbtest.VID, usbtest.PID), serial=uid):
            time.sleep(SETTLE)
            return True
        time.sleep(0.2)
    return False


def battery(board, fw, tests, timeout):
    """(usbtest.py's JSON verdict or None when it printed none, its stderr, a note when the
    bound killed it)."""
    cmd = [sys.executable, str(HIL_DIR / 'usbtest.py'), '--serial', board['uid'], '--json',
           '--tests', ','.join(map(str, tests)), '--timeout', str(timeout)]
    rec = hil_flash.recover_flasher(board)
    recovery = hil_flash.convoy_safe(rec)
    if recovery:
        cmd += ['--recover-board', json.dumps({'name': board['name'], 'flasher': rec}),
                '--recover-fw', fw]
    # usbtest.py stops dispatching cases at its budget, as under hil_test.py, so the kill needs
    # room past it only for the one case already started, then usbtest.recovery_reserve() when
    # a recovery can run, else the WEDGE_CONFIRM_S watch
    budget = SETUP_S + len(tests) * (timeout + CASE_EXTRA_S)
    cmd += ['--budget', str(budget)]
    bound = budget + timeout + CASE_EXTRA_S + (
        usbtest.recovery_reserve(rec) if recovery else usbtest.WEDGE_CONFIRM_S)
    r = hil_util.run_cmd(cmd, timeout=bound, split_stderr=True, quiet=True)
    out, err = hil_util.cmd_stdout_text(r.stdout), hil_util.cmd_stdout_text(r.stderr)
    if r.returncode == 124:     # whatever it printed, a killed battery has no verdict
        return None, err, f'usbtest.py killed at its {bound}s bound (rc 124)'
    brace = out.find('{')
    try:
        return (json.loads(out[brace:]) if brace >= 0 else None), err, ''
    except ValueError:
        return None, err, ''


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('--config', required=True, help="this host's HIL config json (hil skill)")
    p.add_argument('--board', required=True)
    p.add_argument('--tests', required=True, help='comma-separated usbtest case numbers')
    p.add_argument('--after', required=True, choices=('park', 'leave'),
                   help='park: flash device/board_test afterwards; leave: keep usbtest running')
    p.add_argument('--variant', help='roster variant, required when the board has several')
    p.add_argument('--timeout', type=int, default=60, help='per-case timeout in seconds (default 60, as HIL)')
    p.add_argument('--allow-concurrent', action='store_true',
                   help='skip the live-battery check; see the description first')
    args = p.parse_args()

    report = {'pass': False, 'board': args.board, 'variant': args.variant, 'tests': args.tests,
              'cases': [], 'wedged': False, 'boardState': 'untouched', 'error': ''}

    def finish(code, error=''):
        signal.signal(signal.SIGTERM, signal.SIG_IGN)   # one verdict line; the release still follows
        report['error'] = error
        if error:
            print(f'error: {error}', file=sys.stderr)
        print(json.dumps(report))
        return code

    try:
        tests = [int(t) for t in args.tests.split(',')]
    except ValueError:
        return finish(2, f'--tests {args.tests!r} is not a list of case numbers')
    if len(set(tests)) != len(tests) or not all(t in usbtest.PARAMS for t in tests):
        return finish(2, f'--tests {args.tests!r}: every case once, each one of {sorted(usbtest.PARAMS)}')
    if args.timeout <= 0:
        return finish(2, '--timeout must be positive')

    previous = signal.signal(signal.SIGTERM, on_sigterm)
    try:
        return lock_and_run(args, tests, report, finish)
    finally:
        signal.signal(signal.SIGTERM, previous)


def lock_and_run(args, tests, report, finish):
    cwd = os.getcwd()   # before the lock: nothing after it may fail outside the cleanup
    try:
        board, variant = resolve(args.config, args.board, args.variant)
        report['variant'] = variant
        flasher = board['flasher']['name']
        fw = firmware(variant, 'device/usbtest', flasher)
        park_fw = firmware(variant, 'device/board_test', flasher) if args.after == 'park' else None
        if not args.allow_concurrent:
            check_peers()
        try:
            lock = hil_lock.acquire_board_lock(board['name'], reason=f'run_case.py usbtest {args.tests}')
        except RuntimeError as e:
            raise Refused(f'{board["name"]}: {e}')
        if lock is None:
            raise Refused(f'{board["name"]}: board lock unavailable or bypassed (HIL_NO_BOARD_LOCK)')
    except Refused as e:
        return finish(2, str(e))
    except Terminated:
        return finish(2, 'terminated (SIGTERM) before any hardware action')

    try:
        if not args.allow_concurrent:
            try:
                check_peers()   # again: a peer may have started before the lock was taken
            except Refused as e:
                return finish(2, str(e))
        # flash_jlink writes its command file into the cwd: keep it out of the checkout. Cleanup
        # runs after run_locked printed the verdict, so its errors must not add a second one.
        with tempfile.TemporaryDirectory(prefix='run_case-', ignore_cleanup_errors=True) as workdir:
            os.chdir(workdir)
            return run_locked(board, fw, park_fw, tests, args.timeout, report, finish)
    except Terminated:
        report['pass'] = False
        report['boardState'] = 'unknown: terminated (SIGTERM) mid-run'
        return finish(1, 'terminated (SIGTERM), e.g. by hil_lock.py release')
    except Exception as e:   # any failure after the lock still ends with a verdict line
        return finish(1, f'{type(e).__name__}: {e}')
    finally:
        try:
            os.chdir(cwd)
        finally:
            release(lock)


def release(lock):
    hil_lock.clear_record(lock)
    lock.close()


def run_locked(board, fw, park_fw, tests, timeout, report, finish):
    err = flash(board, fw)
    report['boardState'] = 'flash failed' if err else 'usbtest firmware'
    if err:
        return finish(1, err)
    if not enumerated(board['uid']):
        return finish(1, f'no {usbtest.VID}:{usbtest.PID} device with serial {board["uid"]} '
                         f'{ENUM_TIMEOUT}s after flashing')
    data, stderr, killed = battery(board, fw, tests, timeout)
    if stderr.strip():
        print(stderr.rstrip(), file=sys.stderr)
    if data is None:
        report['boardState'] = 'usbtest firmware, verdict incomplete'
        return finish(1, killed or 'usbtest.py printed no verdict')
    report['cases'] = [{k: c[k] for k in ('num', 'name', 'status', 'detail', 'stderr', 'dmesg') if k in c}
                       for c in data.get('cases', [])]
    report['wedged'] = bool(data.get('wedged'))
    report['pass'] = (not report['wedged'] and data.get('failed') == 0
                      and data.get('notrun') == 0 and len(report['cases']) == len(tests))
    for c in report['cases']:
        print(f"case {c['num']:2d} {c.get('name', ''):22s} {c['status']:6s} {c.get('detail', '')}")
        for k in ('stderr', 'dmesg'):
            if c.get(k):
                print('    ' + c[k].replace('\n', '\n    '))
    if report['wedged']:
        report['boardState'] = 'wedged: recover it (usb-kernel-recover)'
        return finish(1)
    if park_fw:
        err = flash(board, park_fw)
        report['boardState'] = f'park failed: {err}' if err else 'parked on board_test'
        if err:
            report['pass'] = False
            return finish(1, err)
    return finish(0 if report['pass'] else 1)


if __name__ == '__main__':
    sys.exit(main())
