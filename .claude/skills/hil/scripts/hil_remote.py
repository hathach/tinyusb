#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Run hil_test.py on a remote rig (default ci.lan) from a dev PC.

Takes hil_test.py's own arguments, minus the config: stages the harness, the config and the
firmware the run will look for under -B (default cmake-build, the build skill's --shared
layout), runs it there, and copies the report pair and the <config>.failed re-run spec back
to the checkout root.

Env overrides: REMOTE (ssh host), REMOTE_DIR (rm -rf'd and recreated each run), CONFIG
(HIL config json), ROOT_DIR (checkout to test; defaults to this script's own).
"""
import json
import os
import re
import shlex
import subprocess
import sys
from pathlib import Path

ROOT = Path(os.environ.get('ROOT_DIR') or Path(__file__).resolve().parents[4]).resolve()

# Everything the rig executes, by repo-relative path. test_hil_bounded's RemoteStaging walks
# the harness's import closure and requires each file here, so keep this a plain literal.
# tools/rtt.py is loaded by hil_util through exec_module, which no import walk can see.
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
)

FIRMWARE_FILTER = ['--prune-empty-dirs', '--include=*/', '--include=*.elf', '--include=*.bin',
                   '--include=*.hex', '--include=config.env', '--include=flash_args', '--exclude=*']

# The screen is for the tilde: the remote shell would expand `~/` alone to HOME, which this
# run rm -rf's. `/` or `~/` plus at least one named component, no `..`, no `//`.
REMOTE_DIR_RE = re.compile(r'^(/|~/)[A-Za-z0-9_.~/-]*[A-Za-z0-9_-]$')

# Runs on the remote. $1 arrives shlex-quoted, so a leading ~/ is expanded here, where
# $HOME is a value; the second gate then sees the real rm -rf target.
SETUP_SCRIPT = r'''
set -e
case "$1" in
  "~/"*) d="$HOME/${1#"~/"}" ;;
  *) d="$1" ;;
esac
case "$d" in
  ''|/|"$HOME"|"$HOME"/) echo "refusing to rm -rf '$d'" >&2; exit 1 ;;
esac
rm -rf -- "$d"
mkdir -p -- "$d/test/hil/helper" "$d/tools" "$d/$2"
echo "HIL_REMOTE_DIR=$d"
'''


def fail(msg):
    sys.exit(f'error: {msg}')


def load_parser():
    sys.path.insert(0, str(ROOT / 'test' / 'hil'))
    from helper import hil_args
    return hil_args.build_parser()


def check_remote_dir(remote_dir):
    if not REMOTE_DIR_RE.match(remote_dir) or '..' in remote_dir or '//' in remote_dir:
        fail(f'REMOTE_DIR must be /path or ~/path of [A-Za-z0-9_.~/-], no "..", no trailing '
             f'slash -- it is an rm -rf target: {remote_dir}')


def check_build_dir(build_dir):
    parts = Path(build_dir).parts
    if Path(build_dir).is_absolute() or not parts or any(p in ('.', '..') for p in parts) \
            or not all(re.fullmatch(r'[A-Za-z0-9_.-]+', p) for p in parts):
        fail(f'-B must be a relative path inside the checkout: {build_dir}')


def variant_names(board):
    """The build dirs hil_test.py's find_firmware reads: one per variant, else the board's."""
    return [v['name'] for v in board.get('variant') or [{'name': board['name']}]]


def resolve_firmware(config, boards, build_dir):
    """Return the existing <build_dir>/cmake-build-<variant> dirs to stage, refusing before
    anything remote is touched: an unknown board fails the whole remote run after staging,
    and a board with no build at all would only show up as `Skip (no binary)` rows."""
    roster = {b['name']: b for b in config.get('boards', [])}
    unknown = [b for b in boards if b not in roster]
    if unknown:
        fail(f'not in the config: {" ".join(unknown)} (-b takes board names, not variant names)')
    root = ROOT / build_dir
    dirs, missing = [], []
    for name in boards or roster:
        found = [root / f'cmake-build-{v}' for v in variant_names(roster[name])]
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
        fail(f'no {build_dir}/cmake-build-* build for any board in the config -- nothing to test')
    return dirs


def sidecar_matches(sidecar, config):
    """hil_report.json is not namespaced by rig, so a sidecar from another rig or config
    would merge boards that never ran here. Require one row from this roster."""
    try:
        rows = json.loads(sidecar.read_text()).get('rows') or []
    except (OSError, ValueError, AttributeError):
        return False
    known = set()
    for b in config.get('boards', []):
        known.add(b.get('name'))
        known.update(v.get('name') for v in b.get('variant') or [])
    return not rows or any(isinstance(r, dict) and r.get('board') in known for r in rows)


def forwarded_env():
    """HIL_* knobs for the run. HIL_REPORT_DIR stays local: the copy-back reads the report
    from the remote dir, and relocating it would bring nothing home."""
    return [f'{k}={v}' for k, v in sorted(os.environ.items())
            if re.fullmatch(r'HIL_[A-Z0-9_]*', k) and k != 'HIL_REPORT_DIR']


def run_command(remote_dir, argv, config_name):
    # --retry 1 first so a caller's -r wins by argparse's last-wins; the pool guard does not
    # scale with retries, so a higher default would starve a shared rig.
    # The PATH prefix: flasher CLIs live in ~/.local/bin and ~/bin, and a non-interactive
    # ssh shell sources no profile.
    run = ['python3', '-u', 'test/hil/hil_test.py', '--retry', '1', *argv, f'test/hil/{config_name}']
    return (f'cd {shlex.quote(remote_dir)} && export PATH="$HOME/.local/bin:$HOME/bin:$PATH" && '
            f'exec env {shlex.join([*forwarded_env(), *run])}')


def rsync(*args, optional=False):
    # an optional fetch (a green run writes no .failed) must not print rsync's code-23 error
    return subprocess.run(['rsync', *args], cwd=ROOT,
                          stderr=subprocess.DEVNULL if optional else None).returncode


def copy_back(remote, remote_dir, config_name):
    """Fetch the report pair all-or-nothing: the markdown is a rendering of the sidecar, and
    a half-copied pair publishes last run's table beside this run's data."""
    md, js = ROOT / 'hil_report.md', ROOT / 'hil_report.json'
    tmp = [p.with_name(p.name + '.tmp') for p in (md, js)]
    ok = all(rsync('-q', f'{remote}:{remote_dir}/{p.name}', str(t), optional=True) == 0 and t.is_file()
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
    if rsync('-q', f'{remote}:{remote_dir}/{spec.name}', str(spec), optional=True) == 0 and spec.is_file():
        print(f'==> {spec.name} copied to {spec}')


def main(argv):
    remote = os.environ.get('REMOTE', 'ci.lan')
    remote_dir = os.environ.get('REMOTE_DIR', '/tmp/tinyusb-hil')
    config_path = Path(os.environ.get('CONFIG') or ROOT / 'test/hil/tinyusb.json').resolve()

    if not (ROOT / 'test/hil/hil_test.py').is_file():
        fail(f'{ROOT} does not look like a tinyusb checkout')
    check_remote_dir(remote_dir)
    args = load_parser().parse_args([*argv, str(config_path)])
    if args.build:
        fail('--build would build on the rig, which gets binaries only; build locally with\n'
             '  python3 .claude/skills/build/scripts/check_build.py --board <board> --shared')
    check_build_dir(args.build_dir)
    try:
        config = json.loads(config_path.read_text())
    except (OSError, ValueError) as e:
        fail(f'could not read the config {config_path}: {e}')
    firmware = resolve_firmware(config, args.board, args.build_dir)

    print(f'==> Setting up remote {remote}:{remote_dir}')
    setup = subprocess.run(['ssh', remote, shlex.join(['bash', '-s', '--', remote_dir, args.build_dir])],
                           input=SETUP_SCRIPT, capture_output=True, text=True)
    marked = [ln.partition('=')[2] for ln in setup.stdout.splitlines() if ln.startswith('HIL_REMOTE_DIR=')]
    if setup.returncode != 0 or len(marked) != 1 or not marked[0].startswith('/'):
        fail(f'remote setup failed (exit {setup.returncode}): {setup.stderr.strip()}')
    remote_dir = marked[0]

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
            if rsync('-q', str(sidecar), f'{remote}:{remote_dir}/') != 0:
                fail('could not upload hil_report.json')

    print('==> Copying the harness and config')
    if rsync('-aqR', *HARNESS_FILES, f'{remote}:{remote_dir}/') != 0 or \
            rsync('-q', str(config_path), f'{remote}:{remote_dir}/test/hil/') != 0:
        fail('could not stage the harness')
    print(f'==> Copying firmware from {len(firmware)} build dir(s) under {args.build_dir}/')
    for d in firmware:
        if rsync('-a', *FIRMWARE_FILTER, str(d), f'{remote}:{remote_dir}/{args.build_dir}/') != 0:
            fail(f'could not stage {d}')

    print(f'==> Running HIL test on {remote}')
    rc = subprocess.run(['ssh', remote, run_command(remote_dir, argv, config_path.name)],
                        stdin=subprocess.DEVNULL).returncode
    copy_back(remote, remote_dir, config_path.name)
    return rc


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
