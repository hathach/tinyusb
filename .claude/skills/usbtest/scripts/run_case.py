#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Run chosen usbtest cases on one roster board: lock, flash, enumerate, run, park or leave, release.

  run_case.py --config CONFIG --board BOARD --tests N[,N...] --after park|leave
              [--variant NAME] [--timeout S] [--allow-concurrent]

For a session that has NOT taken the board. Inside a lock you already hold, with the firmware
already flashed, run `test/hil/usbtest.py --serial <uid> --tests N --json` directly instead.

Refuses before touching hardware (exit 2) when: the board or variant is unknown or ambiguous,
the board sits in boards-skip, does not run device/usbtest or lacks a uid or flasher name in the
roster, it flashes through openocd_esp (its verdict would not log a session the provider left running; use
hil_test.py), the usbtest (or, for --after park, the board_test) firmware is not built, the board lock
is held or unusable, the busy check under it (hil_flash.probe_busy) names a live user of the board's probe or cannot
rule one out, or a testusb, usbtest.py, hil_test.py, wedge_drill.py or other run_case.py
process is alive on this host, checked before and again after taking the lock. That peer check is
host-wide and does not see a hil_test.py started after it: --allow-concurrent skips it, never the busy check, only
when another battery's load on this board's host controller does not matter for what you are measuring.
A host that is not Linux is refused first, once argparse has accepted the command line.

The lock is agentrc's hil_lock.board_task (protected; HIL_NO_BOARD_LOCK is ignored), ended on the run's cleanup
verdict: restored once parked on board_test, clean when --after leave keeps usbtest running with a complete,
unwedged verdict or a peer refused the run under the lock, failed otherwise (flash, enumeration, an incomplete
verdict, a wedge, a failed park, an exception or SIGTERM), which logs it in the board's cleanup log for the next
take to report until a later restore resolves it.

Flashes with the roster's flasher from cmake-build/cmake-build-<variant> (the build skill's
--shared --variants layout), waits for 0525:a4a0 with the board's serial, and runs usbtest.py
with post-hang recovery when the board's recovery flasher can deliver it. --after park flashes
device/board_test afterwards unless the device is wedged or the verdict is incomplete.

stdout ends with one JSON line: {"pass", "board", "variant", "tests", "cases", "wedged",
"boardState", "error"}, each case with usbtest.py's num, name, status, detail and, when it captured
them, stderr and dmesg; with "previousCleanupFailure" and "openCleanupFailures" (hil_lock.failure_report) when
the take found open failed cleanups, which go to stderr before any hardware action too, and "restoreNotes" for
those a park left open. Exit 0 every case passed, 1 a case failed or the run failed after the
lock was taken or the task's lock did not end as its verdict asked (its errors in "error"), a refusal's too, 2 refused
before any hardware action; a command line argparse rejects exits 2 with its usage on stderr and no JSON line. The
JSON line follows the lock's release. A SIGTERM kills the running flasher or usbtest.py, ends the task failed and reports
boardState unknown. A signal that lands as the task's __exit__ is entered leaves the lock held until this process
exits (hil_lock.BoardTask's limit), which follows at once.
"""
import argparse
import json
import os
import signal
import sys
import tempfile
import time
from pathlib import Path


def build_parser():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('--config', required=True, help="this host's HIL config json (the HIL contract)")
    p.add_argument('--board', required=True)
    p.add_argument('--tests', required=True, help='comma-separated usbtest case numbers')
    p.add_argument('--after', required=True, choices=('park', 'leave'),
                   help='park: flash device/board_test afterwards; leave: keep usbtest running')
    p.add_argument('--variant', help='roster variant, required when the board has several')
    p.add_argument('--timeout', type=int, default=60, help='per-case timeout in seconds (default 60, as HIL)')
    p.add_argument('--allow-concurrent', action='store_true',
                   help='skip the live-battery check; see the description first')
    return p


def new_report(args):
    return {'pass': False, 'board': args.board, 'variant': args.variant, 'tests': args.tests,
            'cases': [], 'wedged': False, 'boardState': 'untouched', 'error': ''}


if __name__ == '__main__' and sys.platform != 'linux':   # after argparse, so --help works; before the harness imports
    _report = new_report(build_parser().parse_args())
    _report['error'] = 'run_case.py needs a Linux host (usbfs, /proc, the usbtest kernel module)'
    print(f'error: {_report["error"]}', file=sys.stderr)
    print(json.dumps(_report))
    sys.exit(2)

ROOT = Path(__file__).resolve().parents[4]
HIL_DIR = ROOT / 'test' / 'hil'
sys.path.insert(0, str(HIL_DIR))
sys.path.insert(0, str(ROOT / 'tools'))
import change_impact  # noqa: E402
import hil_flash  # noqa: E402
import usbtest  # noqa: E402
from helper import hil_lock, hil_report, hil_util, usb_observe  # noqa: E402
hil_util.provider()   # an incomplete flash provider is refused here, before any board is locked

ENUM_TIMEOUT = 8        # hil_test.py ENUM_TIMEOUT
SETTLE = 3              # hil_test.py USBTEST_SETTLE: enumeration can bounce once after a flash
PEERS = ('testusb', 'usbtest.py', 'hil_test.py', 'run_case.py', 'wedge_drill.py')
HELPER_S = usbtest.HELPER_TIMEOUT + 5   # a root helper at its bound plus hil_usbtest.run()'s reap
# usbtest.py's start before case 1, its steps at their bounds: the 8 s device wait, 3 s of
# host-compat retries, modprobe, the pattern write (15 s hil_root + 5 s reap) and
# the usbfs CONNECT child at its bound plus the reap. An estimate, not a bound: the battery
# budget starts before it, so a slower start costs cases (reported BUDGET), not a kill.
SETUP_S = 8 + 3 + HELPER_S + (15 + 5) + (usbtest.CONNECT_TIMEOUT + 5)
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
    kill_children()
    raise Terminated()


def stat_fields(proc_dir):
    """<proc_dir>/stat's fields after the command name (state first), or None."""
    try:
        return (proc_dir / 'stat').read_text().rsplit(')', 1)[1].split()
    except (OSError, IndexError):
        return None


def children(spare=()):
    """[(pid, state)] of this process's children, zombies included, `spare` aside."""
    me, found = str(os.getpid()), []
    for d in PROC.glob('[0-9]*'):
        fields = stat_fields(d)
        if fields and len(fields) > 1 and fields[1] == me and int(d.name) not in spare:
            found.append((int(d.name), fields[0]))
    return found


def kill_children(spare=()):
    """SIGKILL every child and its session, `spare` aside: hil_usbtest.run's helpers run in this
    process group with no kill on interrupt, so they outlive a cancelled case."""
    for pid, _ in children(spare):   # unreaped, so neither this pid nor its group can be reused
        for kill in (os.killpg, os.kill):   # kill: a child not yet in its own session
            try:
                kill(pid, signal.SIGKILL)
            except OSError:
                pass


def resolve(config, board_name, variant):
    """(board, variant name) from the roster, its flashers from agentrc's board registry, or Refused
    naming the alternatives."""
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
    if 'device/usbtest' not in change_impact.board_tests(board):
        raise Refused(f'{board_name} does not run device/usbtest in {config} (its "tests" entry); '
                      f'its device port may not reach this host')
    try:   # its flasher and flasher_recover are agentrc's, by its probe (`hil` skill, Board registry)
        board = hil_util.hil_boards().resolve(board)
    except ValueError as e:
        raise Refused(f'{config}: {e}')
    if why := hil_flash.openocd_esp_refused(board, retains_lock=False):
        raise Refused(f'{board_name}: {why}')
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
        fields = stat_fields(PROC / str(pid))
        try:
            pid = int(fields[1])
        except (TypeError, ValueError, IndexError):
            break
    return pids


def live_peers():
    """([(pid, name)], complete) for processes that run or drive a battery on this host."""
    found, complete = [], True
    own = lineage()
    # hidepid hides other users' processes without an error: detect the restriction itself
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
        hit = argv_peer([a.decode(errors='replace') for a in argv])
        if hit:
            found.append((int(d.name), hit))
    return found, complete


def argv_peer(argv):
    """The PEERS name argv runs: its program past wrappers (sudo, timeout, env, nice) and their
    options, numbers and VAR=value, or for python the script past its options; -c/-m run none."""
    rest = iter(argv)
    prog = next((a for a in rest if not (os.path.basename(a) in ('sudo', 'timeout', 'env', 'nice')
                                         or a.startswith('-') or '=' in a
                                         or a.rstrip('smhd').replace('.', '', 1).isdigit())), '')
    if os.path.basename(prog).rstrip('0123456789.') == 'python':
        for a in rest:
            if a in ('-c', '-m'):
                return None
            if a in ('-W', '-X'):   # the python options that take a separate value
                next(rest, None)
            elif not a.startswith('-'):
                prog = a
                break
        else:
            return None
    name = os.path.basename(prog)
    return name if name in PEERS else None


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
        if usb_observe.usb_scan(vid_pid=(usbtest.VID, usbtest.PID), serial=uid):
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
    args = build_parser().parse_args()
    report = new_report(args)

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


def refusal(e) -> str:
    """Why a board task refused the board before any hardware action: the lock's refusal, or the
    busy check under it naming a probe user or unable to rule one out."""
    return hil_flash.busy_refusal(e) or (str(e) if isinstance(e, RuntimeError) else f'{type(e).__name__}: {e}')


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
    except Refused as e:
        return finish(2, str(e))
    except Terminated:
        return finish(2, 'terminated (SIGTERM) before any hardware action')

    task = hil_lock.board_task(hil_util.lock_name(board), f'run_case.py usbtest {args.tests}',
                               busy=lambda: hil_flash.probe_busy(board))
    entered = False
    try:
        with task:
            entered = True
            found(task)
            code, error = run_task(task, board, fw, park_fw, tests, args, report)
    except Terminated:
        if entered:
            report['pass'] = False
            report['boardState'] = 'unknown: terminated (SIGTERM) mid-run'
            code, error = 1, 'terminated (SIGTERM)'
        elif task.taken or task.ended:  # inside the admission: its take's failures and end still count
            code, error = 2, 'terminated (SIGTERM) before any hardware action'
        else:
            return finish(2, 'terminated (SIGTERM) before any hardware action')
    except Exception as e:
        if entered:   # any failure after the lock still ends with a verdict line
            code, error = 1, f'{type(e).__name__}: {e}'
        else:   # admission: the lock, or the busy check under it, refused the board
            code, error = 2, f'{board["name"]}: {refusal(e)}'
    finally:
        os.chdir(cwd)
    # published once the task ended, so its finalization counts
    code, error = ended(task, code, error, report)
    report['pass'] = report['pass'] and code == 0
    return finish(code, error)


def found(task):
    """The open failed cleanups the take found, reported before any hardware access."""
    if task.cleanup_failures:
        print(f'{len(task.cleanup_failures)} open cleanup failure(s): '
              + '; '.join(map(hil_lock.failure_text, task.cleanup_failures)), file=sys.stderr)


def ended(task, code, error, report):
    """(code, error) once the task ended, the open failed cleanups its take found and why a restore left
    any open in `report`: a lock that did not end as its verdict asked fails the run (1), its state and
    errors in `error`; advisory errors of a clean end only go to stderr."""
    report.update(hil_lock.failure_report(task.cleanup_failures))
    if task.restore_notes:
        report['restoreNotes'] = list(task.restore_notes)
        print(f'board task notes: {"; ".join(task.restore_notes)}', file=sys.stderr)
    if task.ended_cleanly:
        if task.errors:
            print(f'board task notes: {task.describe()}', file=sys.stderr)
        return code, error
    problem = f'board task did not end as its verdict asked: {task.describe()}'
    return 1, f'{error}; {problem}' if error else problem


def run_task(task, board, fw, park_fw, tests, args, report):
    """The run under the board's task: (exit code, error), the task's cleanup verdict given."""
    if not args.allow_concurrent:
        try:
            check_peers()   # again: a peer may have started before the lock was taken
        except Refused as e:
            task.clean()
            return 2, str(e)
    # flash_jlink writes its command file into the cwd: keep it out of the checkout. Cleanup errors
    # of the dir must not replace the verdict.
    with tempfile.TemporaryDirectory(prefix='run_case-', ignore_cleanup_errors=True) as workdir:
        os.chdir(workdir)
        return run_locked(task, board, fw, park_fw, tests, args.timeout, report)


def run_locked(task, board, fw, park_fw, tests, timeout, report):
    """Flash, enumerate, run, then park or leave: (exit code, error). Its last hardware action gives
    the task's verdict: restored once parked, clean when usbtest is left running with a complete
    unwedged verdict as --after leave asks, failed whenever the board's state is not known good."""
    err = flash(board, fw)
    report['boardState'] = 'flash failed' if err else 'usbtest firmware'
    if err:
        task.failed(detail=f'usbtest flash failed: {err}')
        return 1, err
    if not enumerated(board['uid']):
        err = f'no {usbtest.VID}:{usbtest.PID} device with serial {board["uid"]} {ENUM_TIMEOUT}s after flashing'
        task.failed(detail=err)
        return 1, err
    data, stderr, killed = battery(board, fw, tests, timeout)
    if stderr.strip():
        print(stderr.rstrip(), file=sys.stderr)
    if data is None:
        report['boardState'] = 'usbtest firmware, verdict incomplete'
        err = killed or 'usbtest.py printed no verdict'
        task.failed(detail=f'{report["boardState"]}: {err}')
        return 1, err
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
        task.failed(detail=f'usbtest reported the device wedged: {report["boardState"]}')
        return 1, ''
    if park_fw:
        err = flash(board, park_fw)
        report['boardState'] = f'park failed: {err}' if err else 'parked on board_test'
        if err:
            report['pass'] = False
            task.failed(detail=report['boardState'])
            return 1, err
        task.restored(evidence='run_case.py: parked on board_test')
    else:
        task.clean()
    return 0 if report['pass'] else 1, ''

if __name__ == '__main__':
    sys.exit(main())
