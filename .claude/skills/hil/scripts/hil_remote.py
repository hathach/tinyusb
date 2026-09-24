#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Run hil_test.py on a remote rig (default ci.lan) from a dev PC.

Takes hil_test.py's own arguments, minus the config: stages the harness, the config and the
firmware the run will look for under -B (default cmake-build, the build skill's --shared
layout), runs it there, and copies the report pair and the <config>.failed re-run spec back
to the checkout root.

Env overrides: REMOTE (ssh host), REMOTE_DIR (rm -rf'd and recreated each run, under a lock
on <REMOTE_DIR>.lock that refuses a second run sharing it), CONFIG (HIL config json), ROOT_DIR
(checkout to test; defaults to this script's own).
"""
import contextlib
import importlib
import json
import os
import re
import shlex
import subprocess
import sys
import tempfile
import time
from pathlib import Path

ROOT = Path(os.environ.get('ROOT_DIR') or Path(__file__).resolve().parents[4]).resolve()

# Everything the rig executes, by repo-relative path. test_hil_bounded's RemoteStaging walks
# the harness's import closure and requires each file here, so keep this a plain literal.
# tools/rtt.py is loaded by hil_util through exec_module, which no import walk can see, and
# usb_recover.sh is run by hil_recover through sudo for the wedge-recovery shield.
HARNESS_FILES = (
    'test/hil/hil_test.py',
    'test/hil/hil_flash.py',
    'test/hil/usbtest.py',
    'test/hil/pymtp.py',
    'test/hil/mtp_test.py',
    'test/hil/helper/__init__.py',
    'test/hil/helper/hil_args.py',
    'test/hil/helper/hil_health.py',
    'test/hil/helper/hil_lock.py',
    'test/hil/helper/hil_recover.py',
    'test/hil/helper/hil_report.py',
    'test/hil/helper/hil_util.py',
    'tools/rtt.py',
    '.claude/skills/usb-kernel-recover/scripts/usb_recover.sh',
)

# A stalled link must end the run, not hang it: no subprocess timeout fits a HIL run of
# up to HIL_POOL_TIMEOUT, so ssh itself drops a peer silent for 30 s x 4.
SSH_OPTS = ('-o', 'ServerAliveInterval=30', '-o', 'ServerAliveCountMax=4', '-o', 'ConnectTimeout=20')

FIRMWARE_FILTER = ['--prune-empty-dirs', '--include=*/', '--include=*.elf', '--include=*.bin',
                   '--include=*.hex', '--include=config.env', '--include=flash_args', '--exclude=*']

# The screen is for the tilde: the remote shell would expand `~/` alone to HOME, which this
# run rm -rf's. `/` or `~/` plus at least one named component, no `..`, no `//`.
REMOTE_DIR_RE = re.compile(r'^(/|~/)[A-Za-z0-9_.~/-]*[A-Za-z0-9_-]$')

# Runs on the remote. $1 arrives shlex-quoted, so a leading ~/ is expanded here, where
# $HOME is a value; the second gate then sees the real rm -rf target. The lock sits beside
# the tree, which the wipe would take with it: exclusive for the wipe, then shared for the
# staging while the trailing cat keeps this session open, and the run session takes its own
# shared hold (run_command) so a wrapper that dies mid-run leaves hil_test.py's tree covered.
# A wiper needs the exclusive lock, which any shared holder refuses. The tree also carries
# this run's token: a run session that lost its lease (wrapper died between staging and the
# run, a wiper got in) finds another run's token, or none, and refuses the replacement tree.
SETUP_SCRIPT = r'''
set -e
case "$1" in
  "~/"*) d="$HOME/${1#"~/"}" ;;
  *) d="$1" ;;
esac
case "$d" in
  ''|/|"$HOME"|"$HOME"/) echo "refusing to rm -rf '$d'" >&2; exit 1 ;;
esac
command -v flock >/dev/null || { echo "flock not found on the rig" >&2; exit 1; }
mkdir -p -- "$(dirname -- "$d")"
exec 9>>"$d.lock"
flock -n -E 75 9 || {
  rc=$?
  case $rc in
    75) echo "another hil_remote run holds $d.lock -- not wiping its tree" >&2 ;;
    *) echo "flock on $d.lock failed (exit $rc) -- not wiping its tree" >&2 ;;
  esac
  exit 1
}
rm -rf -- "$d"
mkdir -p -- "$d/test/hil/helper" "$d/tools" "$d/$2"
printf '%s\n' "$3" >"$d/.hil-remote-run"
flock -s 9
# on its own line: a rig shell that greets on stdout without a newline would glue the marker
# to it, and the wrapper would wait for a line that never comes
printf '\nHIL_REMOTE_DIR=%s\n' "$d"
cat >/dev/null
'''


def fail(msg):
    sys.exit(f'error: {msg}')


def helper(name):
    """A test/hil/helper module, imported from this checkout's harness."""
    if str(ROOT / 'test' / 'hil') not in sys.path:
        sys.path.insert(0, str(ROOT / 'test' / 'hil'))
    return importlib.import_module(f'helper.{name}')


def check_remote_dir(remote_dir):
    if not REMOTE_DIR_RE.match(remote_dir) or '..' in remote_dir or '//' in remote_dir:
        fail(f'REMOTE_DIR must be /path or ~/path of [A-Za-z0-9_.~/-], no "..", no trailing '
             f'slash -- it is an rm -rf target: {remote_dir}')


def check_build_dir(build_dir):
    parts = Path(build_dir).parts
    if Path(build_dir).is_absolute() or not parts or any(p in ('.', '..') for p in parts) \
            or not all(re.fullmatch(r'[A-Za-z0-9_.-]+', p) for p in parts):
        fail(f'-B must be a relative path inside the checkout: {build_dir}')


def resolve_firmware(config, args):
    """Return the existing <build_dir>/cmake-build-<variant> dirs to stage, refusing before
    anything remote is touched: an unknown board fails the whole remote run after staging,
    and a board with no build at all would only show up as `Skip (no binary)` rows.
    Boards are selected as hil_test.py does, so a build of a board it drops satisfies nothing."""
    boards, build_dir = args.board, args.build_dir
    roster = {b['name']: b for b in config.get('boards', [])}
    unknown = [b for b in boards if b not in roster]
    if unknown:
        fail(f'not in the config: {" ".join(unknown)} (-b takes board names, not variant names)')
    selected = boards or list(roster)
    if args.flasher or args.exclude_flasher:
        selected = [n for n in selected if roster[n]['flasher']['name'] not in args.exclude_flasher
                    and (not args.flasher or roster[n]['flasher']['name'] in args.flasher)]
        if not selected:
            fail(f'no board left after the flasher filter (--flasher {args.flasher or "-"}, '
                 f'--exclude-flasher {args.exclude_flasher or "-"})')
    root = ROOT / build_dir
    dirs, missing = [], []
    for name in selected:
        found = [root / f'cmake-build-{v}' for v in helper('hil_report').variants_of(config, name)]
        present = [d for d in found if d.is_dir()]
        if boards and not present:
            missing.append(f'  {name}: none of {", ".join(str(d.relative_to(ROOT)) for d in found)}')
        for d in found:
            if d not in present and boards:
                # hil_test.py logs `Skip (no binary)` and exits 0 for these cells
                print(f'warning: {name}: no {d.relative_to(ROOT)} -- its cells will be skipped, not tested',
                      file=sys.stderr)
        dirs += present
    if missing:
        # the dirs, not a build command: a variant's dir name and flags come from the roster
        fail(f'no build under {build_dir}/ for:\n' + '\n'.join(missing))
    if not dirs:
        fail(f'no {build_dir}/cmake-build-* build for any selected board in the config -- nothing to test')
    return dirs


def sidecar_matches(sidecar, config):
    """hil_report.json is not namespaced by rig, so a sidecar from another rig or config
    would merge boards that never ran here. Require one row from this roster."""
    try:
        rows = json.loads(sidecar.read_text()).get('rows') or []
    except (OSError, ValueError, AttributeError):
        return False
    variants_of = helper('hil_report').variants_of
    known = {n for b in config.get('boards', []) for n in (b['name'], *variants_of(config, b['name']))}
    return not rows or any(isinstance(r, dict) and r.get('board') in known for r in rows)


def forwarded_env():
    """HIL_* knobs for the run. HIL_REPORT_DIR stays local: the copy-back reads the report
    from the remote dir, and relocating it would bring nothing home."""
    return [f'{k}={v}' for k, v in sorted(os.environ.items())
            if re.fullmatch(r'HIL_[A-Z0-9_]*', k) and k != 'HIL_REPORT_DIR']


def tree_guard(remote_dir, token):
    """Shell that takes a shared hold on the tree's lock (kept by the exec that follows it)
    and checks the tree is still this run's; false once a wiper replaced it."""
    return (f'exec 9>>{shlex.quote(remote_dir + ".lock")} && flock -s -w 60 9 && '
            f'[ "$(cat {shlex.quote(remote_dir + "/.hil-remote-run")} 2>/dev/null)" = {shlex.quote(token)} ]')


# The run session's exit when the guard fails: nothing of ours ran there. Above hil_test.py's
# 0-125 (its failure count), the shell's 126-127 and 128+signal, and below ssh's own 255.
TREE_REPLACED = 200


def run_command(remote_dir, argv, config_name, token=''):
    # --retry 1 first so a caller's -r wins by argparse's last-wins; the pool guard does not
    # scale with retries, so a higher default would starve a shared rig.
    # The PATH prefix: flasher CLIs live in ~/.local/bin and ~/bin, and a non-interactive
    # ssh shell sources no profile.
    run = ['python3', '-u', 'test/hil/hil_test.py', '--retry', '1', *argv, f'test/hil/{config_name}']
    # fd 9 stays open across the exec, so the shared lock lives as long as hil_test.py does
    return (f'{{ {tree_guard(remote_dir, token)}; }} || '
            f'{{ echo "{remote_dir} is not this run\'s tree any more (wiped by another run?)" >&2; exit {TREE_REPLACED}; }}; '
            f'cd {shlex.quote(remote_dir)} && export PATH="$HOME/.local/bin:$HOME/bin:$PATH" && '
            f'exec env {shlex.join([*forwarded_env(), *run])}')


def rsync(guard, *args, optional=False):
    """rsync with a remote endpoint under `guard` = (remote_dir, token): the remote rsync
    server starts only under a shared hold on the tree's lock with this run's token still
    in place, so a transfer never lands in a tree another run rebuilt after our lease died."""
    remote_dir, token = guard
    # rsync appends its --server arguments after --rsync-path, so `sh -c '...' rsync` makes
    # them "$@" of the guard shell, which execs the real rsync on them
    server = f'sh -c {shlex.quote(tree_guard(remote_dir, token) + " && exec rsync \"$@\"")} rsync'
    # an optional fetch (a green run writes no .failed) must not print rsync's code-23 error
    return subprocess.run(['rsync', '-e', shlex.join(['ssh', *SSH_OPTS]), f'--rsync-path={server}', *args],
                          cwd=ROOT, stderr=subprocess.DEVNULL if optional else None).returncode


def copy_back(remote, remote_dir, token, config_name):
    """Fetch the report pair all-or-nothing: the markdown is a rendering of the sidecar, and
    a half-copied pair publishes last run's table beside this run's data."""
    report = helper('hil_report')
    md, js = ROOT / report.REPORT_MD, ROOT / report.REPORT_JSON
    tmp = [p.with_name(p.name + '.tmp') for p in (md, js)]
    guard = (remote_dir, token)
    ok = all(rsync(guard, '-q', f'{remote}:{remote_dir}/{p.name}', str(t), optional=True) == 0 and t.is_file()
             for p, t in zip((md, js), tmp))
    if ok:
        for p, t in zip((md, js), tmp):
            t.replace(p)
        print(f'==> Report copied to {md} (+ sidecar)')
    else:
        for p in (*tmp, md, js):
            p.unlink(missing_ok=True)
        print('==> warning: report copy-back incomplete; removed the stale local pair -- an '
              '--accumulate retry has no merge base until a run succeeds', file=sys.stderr)
    # A green run writes no .failed, so a stale local spec would re-flash boards that passed.
    spec = ROOT / f'{config_name}.failed'
    spec.unlink(missing_ok=True)
    if rsync(guard, '-q', f'{remote}:{remote_dir}/{spec.name}', str(spec), optional=True) == 0 and spec.is_file():
        print(f'==> {spec.name} copied to {spec}')


@contextlib.contextmanager
def remote_lease(remote, remote_dir, build_dir):
    """Wipe and recreate remote_dir, yielding (its absolute path, this run's token) while this
    ssh session holds its lock: another runner sharing REMOTE_DIR would otherwise rm -rf this
    run's tree."""
    token = f'{os.getpid()}-{time.time_ns()}'
    # stderr to a file, not a pipe: nothing drains a pipe while the lease is held, and a
    # remote that writes more than the pipe buffer (rm -rf reporting hundreds of entries)
    # would block with the lock taken
    err_file = tempfile.TemporaryFile('w+')
    lease = subprocess.Popen(
        ['ssh', *SSH_OPTS, remote, shlex.join(['bash', '-c', SETUP_SCRIPT, 'hil-setup', remote_dir, build_dir, token])],
        stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=err_file, text=True)
    marked = next((ln.rstrip('\n').partition('=')[2] for ln in lease.stdout
                   if ln.startswith('HIL_REMOTE_DIR=')), '')
    if not marked.startswith('/'):
        lease.communicate()
        err_file.seek(0)
        fail(f'remote setup failed (exit {lease.returncode}): {err_file.read().strip()}')
    try:
        yield marked, token
    finally:
        try:
            lease.communicate(timeout=30)
        except subprocess.TimeoutExpired:
            lease.kill()
            lease.wait()


def main(argv):
    remote = os.environ.get('REMOTE', 'ci.lan')
    remote_dir = os.environ.get('REMOTE_DIR', '/tmp/tinyusb-hil')
    config_path = Path(os.environ.get('CONFIG') or ROOT / 'test/hil/tinyusb.json').resolve()

    if not (ROOT / 'test/hil/hil_test.py').is_file():
        fail(f'{ROOT} does not look like a tinyusb checkout')
    check_remote_dir(remote_dir)
    args = helper('hil_args').build_parser().parse_args([*argv, str(config_path)])
    if args.build:
        fail('--build would build on the rig, which gets binaries only; build locally with\n'
             '  python3 .claude/skills/build/scripts/check_build.py --board <board> --shared\n'
             'or, for a board with a "variant" list, as the hil skill\'s Prerequisites says')
    check_build_dir(args.build_dir)
    try:
        config = json.loads(config_path.read_text())
    except (OSError, ValueError) as e:
        fail(f'could not read the config {config_path}: {e}')
    firmware = resolve_firmware(config, args)

    print(f'==> Setting up remote {remote}:{remote_dir}')
    with remote_lease(remote, remote_dir, args.build_dir) as (remote_dir, token):
        return stage_and_run(remote, remote_dir, token, argv, args, config, config_path, firmware)


def stage_and_run(remote, remote_dir, token, argv, args, config, config_path, firmware):
    guard = (remote_dir, token)
    if args.accumulate:
        # the wipe cleared the rig's copy; without the local sidecar as merge base the retry's
        # small table replaces the full one
        sidecar = ROOT / 'hil_report.json'
        if not sidecar.is_file():
            print('==> warning: --accumulate but no local hil_report.json to merge onto -- this '
                  "run's table will REPLACE the previous one", file=sys.stderr)
        elif not sidecar_matches(sidecar, config):
            print(f'==> warning: hil_report.json holds no board from {config_path.name} -- not '
                  "uploading it; this run's table will REPLACE the previous one", file=sys.stderr)
        else:
            print('==> Uploading hil_report.json as the --accumulate merge base')
            if rsync(guard, '-q', str(sidecar), f'{remote}:{remote_dir}/') != 0:
                fail('could not upload hil_report.json')

    print('==> Copying the harness and config')
    if rsync(guard, '-aqR', *HARNESS_FILES, f'{remote}:{remote_dir}/') != 0 or \
            rsync(guard, '-q', str(config_path), f'{remote}:{remote_dir}/test/hil/') != 0:
        fail('could not stage the harness')
    print(f'==> Copying firmware from {len(firmware)} build dir(s) under {args.build_dir}/')
    # one transfer: every dir lands under the same <build_dir>/, by its own name
    if rsync(guard, '-a', *FIRMWARE_FILTER, *map(str, firmware), f'{remote}:{remote_dir}/{args.build_dir}/') != 0:
        fail('could not stage the firmware')

    print(f'==> Running HIL test on {remote}')
    rc = subprocess.run(['ssh', *SSH_OPTS, remote, run_command(remote_dir, argv, config_path.name, token)],
                        stdin=subprocess.DEVNULL).returncode
    if rc == TREE_REPLACED:
        return rc     # nothing of ours ran there: no report to fetch, and another run's must not land here
    copy_back(remote, remote_dir, token, config_path.name)
    return rc


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
