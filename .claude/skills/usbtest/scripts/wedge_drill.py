#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Try to wedge one roster board on purpose and check that usbtest.py's in-run recovery clears it.

  wedge_drill.py --config CONFIG --board BOARD [--variant NAME] [--delay S] [--timeout S]
                 [--allow-concurrent]

Lock, flash device/usbtest, start case 27 (bulk write perf, whose kernel wait in test_queue is
unbounded) with post-hang recovery, and --delay seconds after its testusb process appears halt
the DUT core through the board's recovery flasher. The intent: a halted core arms no new buffers,
the transfer in flight never completes, testusb sits in D state on the device lock, and usbtest.py
must call it HUNG, reset the DUT through the probe and reap the original testusb. Whether a halt
does that is variant-dependent (USB engine, DMA, debug freeze): read the variant's reference
manual (read-doc) before drilling it. With no flash between, case 1 must then pass on the same
serial. The board is parked on device/board_test only when it is known to be usable.

Refuses before touching hardware (exit 2) as run_case.py does, and when the board's recovery
flasher is not openocd (the halt runs through it) or is not convoy-safe, or --delay plus the halt
bound leaves the halt no room to finish before the case times out.

stdout ends with one JSON line: {"drill", "board", "variant", "reason", "halt", "cases", "wedged",
"recovery", "device", "smoke", "cleanup", "boardState", "error"}. drill is "pass" (HUNG, reset
rc 0, testusb reaped, the serial back as one device on a new devnum, smoke case passed), "fail"
(the harness did not recover a wedge it was given), or "inconclusive" (no wedge was produced:
the case ended before the halt landed, the halt failed, or no case hung). `cleanup` is the drill's
own reset after a non-pass and never counts as harness recovery. Exit 0 only for a pass with the
board parked; 1 otherwise; 2 refused.
"""
import argparse
import json
import os
import re
import shlex
import signal
import subprocess
import sys
import tempfile
import threading
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import run_case  # noqa: E402
from run_case import PROC, Refused, Terminated, hil_flash, hil_lock, hil_util, usbtest  # noqa: E402

CASE = 27
SMOKE = 1
HALT_TIMEOUT = 20       # openocd init + halt
APPEAR_TIMEOUT = run_case.SETUP_S   # usbtest.py's start before its first case


def halt_bound():
    """The halt's worst case: run_cmd's bound plus its post-kill reap."""
    return HALT_TIMEOUT + hil_util.REAP_GRACE


HALTED_MARK = 'WEDGE_DRILL_HALTED'


class ProbeHeld(Exception):
    """A halt's openocd survived SIGKILL: likely in D state on the probe's device lock, which
    any further open of the probe would also block on."""

    def __init__(self, pid):
        super().__init__(f'openocd {pid} survived SIGKILL and may still hold the probe')
        self.pid = pid


def wch(rec_board):
    return 'target/wch-riscv.cfg' in rec_board['flasher'].get('args', '')


def kill_and_reap(p):
    """SIGKILL `p`'s session and wait REAP_GRACE for it, through signals: an interruption is
    re-raised only once its death is confirmed, and an unconfirmed death is ProbeHeld whatever
    interrupted."""
    deadline = time.monotonic() + hil_util.REAP_GRACE
    interrupted = None
    while True:
        try:
            try:
                os.killpg(p.pid, signal.SIGKILL)
            except ProcessLookupError:
                pass
            p.wait(timeout=max(0, deadline - time.monotonic()))
            break
        except (Terminated, KeyboardInterrupt) as e:
            interrupted = interrupted or e
        except subprocess.TimeoutExpired:
            raise ProbeHeld(p.pid) from None
    if interrupted:
        raise interrupted


def halt_held(rec_board, timeout):
    """halt_openocd for a WCH-Link: its openocd shutdown sends the probe's detach (wlinke.c
    wlink_quit), which resumes the core, so the halt holds only if openocd is SIGKILLed once
    halted. The probe opens once, so openocd is dead when this returns; ProbeHeld otherwise."""
    cmd = (hil_flash._openocd_cmd_base(rec_board['flasher'])
           + f' -c "init; halt; echo {HALTED_MARK}; sleep {int(timeout * 1000)}; shutdown"')
    # no shell: the process reaped below must be openocd itself
    p = subprocess.Popen(shlex.split(cmd), stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                         text=True, errors='replace', start_new_session=True)
    out, halted = [], threading.Event()

    def read():
        for line in p.stdout:
            out.append(line)
            if line.strip() == HALTED_MARK:
                halted.set()
                return

    try:
        reader = threading.Thread(target=read, daemon=True)
        reader.start()
        reader.join(timeout)
    finally:
        kill_and_reap(p)
    return subprocess.CompletedProcess(cmd, 0 if halted.is_set() else 1, ''.join(out), '')


def on_signal(signum, frame):
    """SIGTERM and SIGINT alike: kill the battery and unwind to the cleanup, once."""
    signal.signal(signal.SIGTERM, signal.SIG_IGN)
    signal.signal(signal.SIGINT, signal.SIG_IGN)
    run_case.kill_children(INHERITED)
    raise Terminated()


def recovery_board(board):
    """The board with its recovery flasher as `flasher`, or Refused when the drill cannot use it."""
    rec = hil_flash.recover_flasher(board)
    if rec.get('name') != 'openocd':
        raise Refused(f'{board["name"]}: the drill halts through an openocd recovery flasher; '
                      f'this board recovers through {rec.get("name")}')
    if not hil_flash.convoy_safe(rec):
        raise Refused(f'{board["name"]}: its recovery flasher is not convoy-safe, so usbtest.py '
                      f'would not attempt a recovery')
    return {'name': board['name'], 'flasher': rec}


def identity(uid):
    """(node, devnum) of the one enumerated device with this serial, else None."""
    found = hil_util.usb_scan(vid_pid=(usbtest.VID, usbtest.PID), serial=uid)
    if len(found) != 1:
        return None
    d = Path(found[0]['dir'])
    try:
        bus, dev = int((d / 'busnum').read_text()), int((d / 'devnum').read_text())
    except (OSError, ValueError):
        return None
    return '/dev/bus/usb/%03d/%03d' % (bus, dev), dev


def testusb_pid(node, case=None):
    """The pid of a testusb on `node` (running `case`, when given), or None."""
    for d in PROC.glob('[0-9]*'):
        try:
            argv = (d / 'cmdline').read_bytes().decode(errors='replace').split('\0')
        except OSError:
            continue
        if os.path.basename(argv[0]) == 'testusb' and node in argv and (
                case is None or any(a == '-t' and b == str(case) for a, b in zip(argv, argv[1:]))):
            return int(d.name)
    return None


def age(pid):
    """Seconds since `pid` started, or None."""
    try:
        start = int((PROC / str(pid) / 'stat').read_text().rsplit(')', 1)[1].split()[19])
        uptime = float((PROC / 'uptime').read_text().split()[0])
    except (OSError, ValueError, IndexError):
        return None
    return uptime - start / os.sysconf('SC_CLK_TCK')


def alive(pid):
    try:
        return (PROC / str(pid) / 'stat').read_text().rsplit(')', 1)[1].split()[0] != 'Z'
    except (OSError, IndexError):
        return False


def recovery_from(stderr):
    """What usbtest.py's recover_hang reported: {attempted, reset_rc, reaped}."""
    m = re.search(r'^reset rc (-?\d+)$', stderr, re.M)
    rec = {'attempted': 'auto-recovering: resetting' in stderr,
           'reset_rc': int(m.group(1)) if m else None,
           'reaped': 'recovery freed the device: testusb reaped' in stderr}
    if rec['attempted'] and rec['reset_rc'] != 0:
        rec['log'] = stderr[-800:]   # the failed reset's own output
    return rec


def inject(rec_board, node, delay, timeout, battery_done, report):
    """Halt the core while case 27's testusb is in flight, finishing before the case can time
    out. True only when the halt succeeded and the case was still running after it."""
    deadline = time.monotonic() + APPEAR_TIMEOUT
    pid = None
    while pid is None:
        if battery_done.is_set() or time.monotonic() > deadline:
            report['halt']['note'] = f'case {CASE} testusb process never seen'
            return False
        pid = testusb_pid(node, CASE)
        time.sleep(0.05)
    started = age(pid)
    report['halt']['processAge'] = started and round(started, 2)
    # from the process's own start, not from when we saw it: the case's timeout runs from there
    if started is None or started + delay + halt_bound() >= timeout:
        report['halt']['note'] = 'no room left to halt before the case times out'
        return False
    if battery_done.wait(delay) or not alive(pid):
        report['halt']['note'] = f'case {CASE} ended before the halt'
        return False
    report['halt']['issued'] = True   # before the call: a halt that times out may still land
    t = time.monotonic()
    halt = halt_held if wch(rec_board) else hil_flash.halt_openocd
    ret = halt(rec_board, timeout=HALT_TIMEOUT)
    report['halt'].update(rc=ret.returncode, took=round(time.monotonic() - t, 2),
                          caseRunningAfter=alive(pid))
    if ret.returncode != 0:
        report['halt']['note'] = 'halt failed: ' + hil_util.cmd_stdout_text(ret.stdout)[-200:]
        return False
    if not report['halt']['caseRunningAfter']:
        report['halt']['note'] = f'case {CASE} ended while the halt ran'
        return False
    return True


def join(worker):
    """Join the battery thread even through a signal: the probe is not touched again while
    the battery may still be using it. Re-raises the first interruption afterwards."""
    interrupted = None
    while worker.is_alive():
        try:
            worker.join()
        except (Terminated, KeyboardInterrupt) as e:
            interrupted = interrupted or e
    if interrupted:
        raise interrupted


def cleanup(rec_board, report):
    """The drill's own reset after a non-pass: never counted as the harness's recovery. True
    when the reset returned 0."""
    reset = hil_flash.reset_primitive(rec_board['flasher']['name'])
    try:
        ret = reset(rec_board, timeout=usbtest.RECOVER_RESET_TIMEOUT)
    except Exception as e:
        report['cleanup'] = f'drill reset raised {type(e).__name__}: {e}'
        return False
    report['cleanup'] = f'drill reset rc {ret.returncode}'
    return ret.returncode == 0


def usable(uid):
    """One device answers to the serial with no testusb on its node."""
    if not run_case.enumerated(uid):
        return False
    now = identity(uid)
    return now is not None and testusb_pid(now[0]) is None


def drill(board, rec_board, fw, delay, timeout, report):
    err = run_case.flash(board, fw)
    report['boardState'] = 'flash failed' if err else 'usbtest firmware'
    if err:
        report['error'] = err
        return 'inconclusive'
    if not run_case.enumerated(board['uid']):
        report['error'] = f'no {usbtest.VID}:{usbtest.PID} device with serial {board["uid"]} after flashing'
        return 'inconclusive'
    before = identity(board['uid'])
    if before is None:
        report['error'] = f'serial {board["uid"]} is not exactly one enumerated device'
        return 'inconclusive'
    report['device']['before'] = before[1]

    result, done = {}, threading.Event()

    def run_battery():
        try:
            result['v'] = run_case.battery(board, fw, [CASE], timeout)
        except BaseException as e:   # surfaced on the main thread after join
            result['e'] = e
        finally:
            done.set()

    report['wedged'] = None   # unknown until a verdict says otherwise
    worker = threading.Thread(target=run_battery)
    worker.start()
    try:
        halted = inject(rec_board, before[0], delay, timeout, done, report)
    except ProbeHeld as e:
        report['halt']['probeHeld'] = e.pid
        run_case.kill_children(INHERITED)   # before the battery's own recovery opens the probe
        raise
    finally:
        join(worker)
    if 'e' in result:
        raise result['e']
    data, stderr, killed = result['v']
    report['recovery'] = recovery_from(stderr)
    if data is None:
        report['error'] = killed or 'usbtest.py printed no verdict'
        return 'fail' if halted else 'inconclusive'
    report['cases'] = [{k: c[k] for k in ('num', 'status', 'detail') if k in c}
                       for c in data.get('cases', [])]
    report['wedged'] = bool(data.get('wedged'))
    if not halted:
        return 'inconclusive'
    if not any(c['status'] == 'HUNG' for c in report['cases']):
        report['reason'] = 'the halt produced no HUNG case: no wedge to recover'
        return 'inconclusive'
    rec = report['recovery']
    if report['wedged'] or not (rec['attempted'] and rec['reset_rc'] == 0 and rec['reaped']):
        report['reason'] = 'usbtest.py did not recover the wedge'
        return 'fail'
    if not run_case.enumerated(board['uid']):
        report['reason'] = 'the device did not come back after the recovery'
        return 'fail'
    after = identity(board['uid'])
    report['device']['after'] = after and after[1]
    if after is None or after[1] == before[1]:
        report['reason'] = 'no fresh enumeration of exactly one device after the recovery'
        return 'fail'
    report['wedged'] = None
    data, stderr, killed = run_case.battery(board, fw, [SMOKE], timeout)
    if data is not None:
        report['wedged'] = bool(data.get('wedged'))
    smoke = data and next((c for c in data.get('cases', []) if c.get('num') == SMOKE), None)
    report['smoke'] = smoke['status'] if smoke else (killed or 'no verdict')
    if not smoke or smoke['status'] != 'PASS' or report['wedged'] is not False:
        report['reason'] = f'case {SMOKE} after the recovery: {report["smoke"]}'
        return 'fail'
    report['reason'] = 'HUNG, reset, testusb reaped, re-enumerated, smoke passed'
    return 'pass'


INHERITED = set()   # children we did not spawn, e.g. a launching shell's `2> >(...)`


def live_children(grace):
    """Pids of this process's children still not exited after up to `grace` s, INHERITED
    aside. Every child is reaped or SIGKILLed by now, so one still alive is stuck in D state,
    e.g. a halt's openocd on the probe's device lock."""
    me, deadline = str(os.getpid()), time.monotonic() + grace
    while True:
        pids = []
        for d in PROC.glob('[0-9]*'):
            try:
                fields = (d / 'stat').read_text().rsplit(')', 1)[1].split()
            except (OSError, IndexError):
                continue
            if fields[1] == me and fields[0] != 'Z' and int(d.name) not in INHERITED:
                pids.append(int(d.name))
        if not pids or time.monotonic() > deadline:
            return pids
        time.sleep(0.1)


def finish_locked(board, rec_board, park_fw, report):
    """Cleanup and parking under the lock, on every path out of the drill."""
    if report['boardState'] == 'flash failed':
        return
    if not report['halt'].get('probeHeld'):
        # whatever path got here, signals included: no probe access past a live child
        stuck = live_children(hil_util.REAP_GRACE)
        if stuck:
            report['halt']['probeHeld'] = stuck[0]
    if report['halt'].get('probeHeld'):
        report['boardState'] = (f'unknown, not parked: openocd {report["halt"]["probeHeld"]} may '
                                f'still hold the probe (usb-kernel-recover)')
        return
    if report['drill'] == 'pass':
        safe = True
    elif report['halt'].get('issued'):
        # a wedged or unknown verdict leaves a testusb we cannot place (an old node), so no
        # reset return code makes that board safe to flash through
        safe = (cleanup(rec_board, report) and report['wedged'] is False
                and usable(board['uid']))
    else:
        # no halt: only a known, unwedged verdict says the drill left the device as it was
        safe = (report['boardState'] == 'usbtest firmware' and report['wedged'] is False
                and not report['error'])
    if not safe:
        report['boardState'] = 'unknown, not parked: check it (usb-kernel-recover)'
        return
    if report['error'].startswith('terminated'):
        report['boardState'] = 'not parked: terminated'   # whoever signalled wants the board
        return
    err = run_case.flash(board, park_fw)
    report['boardState'] = f'park failed: {err}' if err else 'parked on board_test'
    if err and not report['error']:
        report['error'] = f'park failed: {err}'


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('--config', required=True, help="this host's HIL config json (hil skill)")
    p.add_argument('--board', required=True)
    p.add_argument('--variant', help='roster variant, required when the board has several')
    p.add_argument('--delay', type=float, default=0.3, help='seconds into case 27 before the halt (default 0.3)')
    p.add_argument('--timeout', type=int, default=60, help='per-case timeout in seconds (default 60, as HIL)')
    p.add_argument('--allow-concurrent', action='store_true',
                   help="skip the live-battery check; see run_case.py's description first")
    args = p.parse_args()

    report = {'drill': 'inconclusive', 'board': args.board, 'variant': args.variant, 'reason': '',
              'halt': {}, 'cases': [], 'wedged': False, 'recovery': {}, 'device': {},
              'smoke': None, 'cleanup': None, 'boardState': 'untouched', 'error': ''}

    def finish(code):
        signal.signal(signal.SIGTERM, signal.SIG_IGN)
        signal.signal(signal.SIGINT, signal.SIG_IGN)
        if report['error']:
            print(f'error: {report["error"]}', file=sys.stderr)
        print(json.dumps(report))
        return code

    if not (0 <= args.delay and args.delay + halt_bound() < args.timeout):
        report['error'] = (f'--delay {args.delay} plus the {halt_bound()}s halt bound must end '
                           f'before the {args.timeout}s case timeout')
        return finish(2)

    INHERITED.update(live_children(0))
    cwd = os.getcwd()
    previous = {s: signal.signal(s, on_signal) for s in (signal.SIGTERM, signal.SIGINT)}
    try:
        try:
            board, variant = run_case.resolve(args.config, args.board, args.variant)
            report['variant'] = variant
            rec_board = recovery_board(board)
            flasher = board['flasher']['name']
            fw = run_case.firmware(variant, 'device/usbtest', flasher)
            park_fw = run_case.firmware(variant, 'device/board_test', flasher)
            if not args.allow_concurrent:
                run_case.check_peers()
            try:
                lock = hil_lock.acquire_board_lock(board['name'], reason='wedge_drill.py')
            except RuntimeError as e:
                raise Refused(f'{board["name"]}: {e}')
            if lock is None:
                raise Refused(f'{board["name"]}: board lock unavailable or bypassed (HIL_NO_BOARD_LOCK)')
        except Refused as e:
            report['error'] = str(e)
            return finish(2)
        except Terminated:
            report['error'] = 'terminated before any hardware action'
            return finish(2)
        try:
            with tempfile.TemporaryDirectory(prefix='wedge_drill-', ignore_cleanup_errors=True) as workdir:
                os.chdir(workdir)
                try:
                    if not args.allow_concurrent:
                        run_case.check_peers()   # again: a peer may have started before the lock
                    report['drill'] = drill(board, rec_board, fw, args.delay, args.timeout, report)
                except Refused as e:
                    report['error'] = str(e)
                    return finish(2)
                except (Terminated, KeyboardInterrupt):
                    report['error'] = 'terminated (signal), e.g. by hil_lock.py release'
                except Exception as e:
                    report['error'] = f'{type(e).__name__}: {e}'
                try:
                    finish_locked(board, rec_board, park_fw, report)
                except (Terminated, KeyboardInterrupt):
                    report['error'] = report['error'] or 'terminated (signal) during cleanup'
                    report['boardState'] = 'unknown, not parked: cleanup interrupted'
        finally:
            os.chdir(cwd)
            run_case.release(lock)
        parked = report['boardState'] == 'parked on board_test'
        return finish(0 if report['drill'] == 'pass' and parked else 1)
    finally:
        for s, h in previous.items():
            signal.signal(s, h)


if __name__ == '__main__':
    sys.exit(main())
