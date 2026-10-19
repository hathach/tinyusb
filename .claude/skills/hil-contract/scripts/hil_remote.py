#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Run hil_test.py on a remote rig (default ci.lan) from a dev PC.

Takes hil_test.py's own arguments, minus the config: stages the harness, the config and the
firmware the run will look for under -B (default cmake-build, the build skill's --shared
layout), runs it there, and copies the report pair and the <config>.failed re-run spec back
to the checkout root.

`--run-id ID` records the run under <checkout>/.hil-remote/: ID.started.json, locked for as
long as the run lives, then the done record ID.done.json (exit status, the report files it
copied back). `wait ID` blocks until then, at most --timeout seconds, and prints one JSON
status line.

`receipt --out FILE --head SHA [-b BOARD]...` writes a build receipt: HEAD, the roster's digest
and the digest of every file a run of those boards would stage, refusing an unbuilt variant, a
stale example's firmware left in a variant dir, a tree that is not clean or a HEAD other than
SHA, the one the build began at; the build skill's check_build.py --receipt runs it right after
its build.
`--receipt FILE` on a run refuses to stage unless HEAD, the roster and every staged file still
match it, stages the snapshot of the firmware it checked, and runs nothing when HEAD, the roster
or a tracked file moved while it staged.

Env overrides: REMOTE (ssh host), REMOTE_DIR (rm -rf'd and recreated each run, under a lock
on <REMOTE_DIR>.lock that refuses a second run sharing it), CONFIG (HIL config json), ROOT_DIR
(checkout to test; defaults to this script's own).
"""
import argparse
import contextlib
import errno
import hashlib
import importlib
import json
import os
import re
import shlex
import shutil
import subprocess
import sys
import tempfile
import time
from pathlib import Path

try:
    import fcntl
except ImportError:  # Windows
    fcntl = None
    import msvcrt

ROOT = Path(os.environ.get('ROOT_DIR') or Path(__file__).resolve().parents[4]).resolve()

# Everything the rig executes, by repo-relative path. test_hil_bounded's RemoteStaging walks
# the harness's import closure and requires each file here, so keep this a plain literal.
# test/hil/pyocd/ runs as pyocd's launcher and its user script: no import walk can see it.
# helper/agentrc.py exec_module's agentrc's shared scripts (the board-lock primitive, the USB
# observation, the RTT console) after checking lib/agentrc/.git/HEAD against the commit
# tools/get_deps.py pins: the lib/agentrc and tools/ entries let the rig's tree pass the same
# check, and RemoteStaging requires one per agentrc.load target.
HARNESS_FILES = (
    'test/hil/hil_test.py',
    'test/hil/hil_flash.py',
    'test/hil/usbtest.py',
    'test/hil/pymtp.py',
    'test/hil/mtp_test.py',
    'test/hil/mtp_raw.py',
    'test/hil/helper/__init__.py',
    'test/hil/helper/agentrc.py',
    'test/hil/helper/hil_args.py',
    'test/hil/helper/hil_lock.py',
    'test/hil/helper/hil_report.py',
    'test/hil/helper/hil_tt.py',
    'test/hil/helper/hil_util.py',
    'test/hil/helper/usb_observe.py',
    'test/hil/pyocd/run_pyocd.py',
    'test/hil/pyocd/user_script.py',
    'tools/get_deps.py',
    'lib/agentrc/.git/HEAD',
    'lib/agentrc/skills/hil/scripts/hil_lock.py',
    'lib/agentrc/skills/hil/scripts/usb_observe.py',
    'lib/agentrc/skills/rtt/scripts/rtt.py',
)

# A stalled link must end the run, not hang it: no subprocess timeout fits a HIL run of
# up to HIL_POOL_TIMEOUT, so ssh itself drops a peer silent for 30 s x 4.
SSH_OPTS = ('-o', 'ServerAliveInterval=30', '-o', 'ServerAliveCountMax=4', '-o', 'ConnectTimeout=20')

STAGED_SUFFIXES = ('.elf', '.bin', '.hex')
STAGED_NAMES = ('config.env', 'flash_args')
FIRMWARE_FILTER = ['--prune-empty-dirs', '--include=*/', *(f'--include=*{x}' for x in STAGED_SUFFIXES),
                   *(f'--include={n}' for n in STAGED_NAMES), '--exclude=*']
# a variant dir's <role>/<name> example dirs, examples/'s layout, which hil_test.py flashes from
EXAMPLE_ROLES = ('device', 'dual', 'host', 'typec')

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


RUNS_DIR = '.hil-remote'
# under the Bash tool's 10-min foreground cap, so one wait is one tool call
WAIT_SECS = 570
POLL_SECS = 5
# a background launch writes its started record within this, or it never started
START_SECS = 30
# wait's exit statuses; errors exit 1
RUNNING, DEAD = 3, 4


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


def build_command(config_path, boards, build_dir):
    """The build contract's line for the dirs hil_test.py flashes, each roster variant included."""
    if config_path.is_relative_to(ROOT):
        config_path = config_path.relative_to(ROOT)
    board_args = ' '.join(f'--board {shlex.quote(b)}' for b in boards) or '--board <board>'
    line = (f'python3 .claude/skills/build/scripts/check_build.py {board_args} --shared '
            f'--variants {shlex.quote(config_path.as_posix())}')
    if build_dir != 'cmake-build':
        line += f'\n  (it writes under cmake-build/, not {build_dir}/)'
    return line


def select_boards(config, args):
    """The boards hil_test.py will run, refusing an unknown one: it fails the whole remote run
    after staging. A build of a board this drops satisfies nothing."""
    boards = args.board
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
    return selected


def variant_dirs(config, build_dir, board):
    """The <build_dir>/cmake-build-<variant> dirs hil_test.py flashes a board's variants from."""
    return [ROOT / build_dir / f'cmake-build-{v}' for v in helper('hil_report').variants_of(config, board)]


def resolve_firmware(config, config_path, args):
    """Return the existing <build_dir>/cmake-build-<variant> dirs to stage, refusing before
    anything remote is touched: a board with no build at all would only show up as
    `Skip (no binary)` rows."""
    boards, build_dir = args.board, args.build_dir
    selected = select_boards(config, args)
    dirs, missing, unbuilt = [], [], []
    for name in selected:
        found = variant_dirs(config, build_dir, name)
        present = [d for d in found if d.is_dir()]
        if boards and not present:
            missing.append(f'  {name}: none of {", ".join(d.relative_to(ROOT).as_posix() for d in found)}')
            unbuilt.append(name)
        for i, d in enumerate(found):
            if d not in present and boards:
                # hil_test.py logs `Skip (no binary)` and exits 0 for these cells, except that a
                # one-test run that flashes fails a later variant's same-PID boundary (HIL contract, Build)
                boundary = '' if args.skip_flash or i == 0 else ' or fail the same-PID boundary on a one-test run'
                print(f'warning: {name}: no {d.relative_to(ROOT).as_posix()} -- its cells will be skipped{boundary}',
                      file=sys.stderr)
        dirs += present
    if missing:
        fail(f'no build under {build_dir}/ for:\n' + '\n'.join(missing) +
             f'\nbuild with\n  {build_command(config_path, unbuilt, build_dir)}')
    if not dirs:
        fail(f'no {build_dir}/cmake-build-* build for any selected board in the config -- nothing to test; '
             f'build with\n  {build_command(config_path, selected, build_dir)}')
    return dirs


def sidecar_matches(sidecar, config):
    """hil_report.json is not namespaced by rig, so a sidecar from another rig or config
    would merge boards that never ran here. Require one row from this roster."""
    try:
        rows = json.loads(sidecar.read_text(encoding='utf-8')).get('rows') or []
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


def local_operand(path):
    """A local file as rsync takes it. Windows rsync reads a drive letter as a remote host, so
    there the file is named relative to the checkout, where rsync runs."""
    if os.name != 'nt':
        return str(path)
    try:
        return Path(os.path.relpath(path, ROOT)).as_posix()
    except ValueError:
        fail(f'{path} is not on the drive of the checkout {ROOT}, so rsync cannot name it')


def rsync_program():
    """The rsync to run, None when there is none. Windows: an rsync.exe in an absolute PATH dir,
    which is all subprocess can start by name, and not one in the cwd, which CreateProcess
    searches first; a relative dir would name it from the cwd, not from ROOT where it runs."""
    if os.name != 'nt':
        return 'rsync' if shutil.which('rsync') else None
    for d in os.environ.get('PATH', '').split(os.pathsep):
        if os.path.isabs(d) and os.path.isfile(os.path.join(d, 'rsync.exe')):
            return os.path.join(d, 'rsync.exe')
    return None


def rsync(guard, *args, optional=False):
    """rsync with a remote endpoint under `guard` = (remote_dir, token): the remote rsync
    server starts only under a shared hold on the tree's lock with this run's token still
    in place, so a transfer never lands in a tree another run rebuilt after our lease died.
    Local files are Paths; options and host:path specs are str, passed as they are."""
    remote_dir, token = guard
    args = [local_operand(a) if isinstance(a, Path) else a for a in args]
    program = rsync_program() if os.name == 'nt' else 'rsync'
    if program is None:
        fail('rsync.exe is no longer on PATH')
    # rsync appends its --server arguments after --rsync-path, so `sh -c '...' rsync` makes
    # them "$@" of the guard shell, which execs the real rsync on them
    server = f'sh -c {shlex.quote(tree_guard(remote_dir, token) + " && exec rsync \"$@\"")} rsync'
    # an optional fetch (a green run writes no .failed) must not print rsync's code-23 error
    return subprocess.run([program, '-e', shlex.join(['ssh', *SSH_OPTS]), f'--rsync-path={server}', *args],
                          cwd=ROOT, stderr=subprocess.DEVNULL if optional else None).returncode


def copy_back(remote, remote_dir, token, config_name):
    """Fetch the report pair all-or-nothing: the markdown is a rendering of the sidecar, and
    a half-copied pair publishes last run's table beside this run's data. Returns the
    checkout-relative paths it wrote."""
    copied = []
    report = helper('hil_report')
    md, js = ROOT / report.REPORT_MD, ROOT / report.REPORT_JSON
    tmp = [p.with_name(p.name + '.tmp') for p in (md, js)]
    guard = (remote_dir, token)
    ok = all(rsync(guard, '-q', f'{remote}:{remote_dir}/{p.name}', t, optional=True) == 0 and t.is_file()
             for p, t in zip((md, js), tmp))
    if ok:
        for p, t in zip((md, js), tmp):
            t.replace(p)
        copied += [report.REPORT_MD, report.REPORT_JSON]
        print(f'==> Report copied to {md} (+ sidecar)')
    else:
        for p in (*tmp, md, js):
            p.unlink(missing_ok=True)
        print('==> warning: report copy-back incomplete; removed the stale local pair -- an '
              '--accumulate retry has no merge base until a run succeeds', file=sys.stderr)
    # A green run writes no .failed, so a stale local spec would re-flash boards that passed.
    spec = ROOT / f'{config_name}.failed'
    spec.unlink(missing_ok=True)
    if rsync(guard, '-q', f'{remote}:{remote_dir}/{spec.name}', spec, optional=True) == 0 and spec.is_file():
        copied.append(spec.name)
        print(f'==> {spec.name} copied to {spec}')
    return copied


@contextlib.contextmanager
def remote_lease(remote, remote_dir, build_dir):
    """Wipe and recreate remote_dir, yielding (its absolute path, this run's token) while this
    ssh session holds its lock: another runner sharing REMOTE_DIR would otherwise rm -rf this
    run's tree."""
    if not rsync_program():   # before the wipe: every transfer of the run needs it
        fail('rsync is not on PATH; the staging and the copy-back need it (on Windows, MSYS2\'s or Cygwin\'s rsync.exe)')
    token = f'{os.getpid()}-{time.time_ns()}'
    # stderr to a file, not a pipe: nothing drains a pipe while the lease is held, and a
    # remote that writes more than the pipe buffer (rm -rf reporting hundreds of entries)
    # would block with the lock taken
    err_file = tempfile.TemporaryFile('w+', encoding='utf-8', errors='replace')
    lease = subprocess.Popen(
        ['ssh', *SSH_OPTS, remote, shlex.join(['bash', '-c', SETUP_SCRIPT, 'hil-setup', remote_dir, build_dir, token])],
        stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=err_file, encoding='utf-8', errors='replace')
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


def git(*argv):
    out = subprocess.run(['git', '-C', str(ROOT), *argv], capture_output=True, text=True)
    if out.returncode:
        fail(f'git {" ".join(argv)}: {out.stderr.strip()}')
    return out.stdout.strip()


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def staged_paths(dirs):
    """The files FIRMWARE_FILTER stages from dirs."""
    return [f for d in dirs for f in sorted(d.rglob('*'))
            if f.is_file() and (f.suffix in STAGED_SUFFIXES or f.name in STAGED_NAMES)]


def staged_files(dirs, base=None):
    """{path relative to base (default the checkout): sha256} of what FIRMWARE_FILTER stages from dirs."""
    return {f.relative_to(base or ROOT).as_posix(): digest(f) for f in staged_paths(dirs)}


def snapshot(dirs, into):
    """Copy what FIRMWARE_FILTER stages from dirs to the same checkout-relative layout under
    into, returning the copied dirs: a build rewriting cmake-build/ after the receipt check
    would otherwise send the rig bytes the receipt does not pin."""
    for f in staged_paths(dirs):
        dest = into / f.relative_to(ROOT)
        dest.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(f, dest)
    copies = [into / d.relative_to(ROOT) for d in dirs]
    for c in copies:
        c.mkdir(parents=True, exist_ok=True)   # rsync needs every source, staged files or not
    return copies


def example_firmware(d, files):
    """The <role>/<name> example dirs of variant dir d holding staged <name>.<suffix> firmware,
    directly or in one config subdir, where hil_flash.find_firmware() looks."""
    prefix = f'{d.relative_to(ROOT).as_posix()}/'
    return {'/'.join(parts[:2]) for parts in (p[len(prefix):].split('/') for p in files if p.startswith(prefix))
            if len(parts) in (3, 4) and parts[0] in EXAMPLE_ROLES
            and Path(parts[-1]).stem == parts[1] and Path(parts[-1]).suffix in STAGED_SUFFIXES}


def unverified_examples(board, d, examples):
    """The examples of variant dir d the board's build does not build: a shared dir keeps a
    skipped or dropped example's old firmware. The same test as check_build's configured()."""
    if str(ROOT / 'tools') not in sys.path:
        sys.path.insert(0, str(ROOT / 'tools'))
    tools_build = importlib.import_module('build')
    if [p.parent.parent.name for p in ROOT.glob(f'hw/bsp/*/boards/{board}')] == ['espressif']:
        # one idf tree per example: the build attempted the ones tools/build.py does not skip
        cwd = os.getcwd()
        os.chdir(ROOT)  # tools/build.py reads examples/ and hw/bsp/ relative to the working directory
        try:
            built = {e for e in tools_build.get_examples('espressif') if not tools_build.build_utils.skip_example(e, board)}
        finally:
            os.chdir(cwd)
    else:
        registered = tools_build.cmake_registered_targets(str(d))
        if registered is None:
            fail(f'cmake cannot list the targets of {d.relative_to(ROOT).as_posix()}, so the firmware its build made cannot be told '
                 f'from a stale one')
        built = {e for e in examples if e.split('/')[1] in registered}
    return sorted(examples - built)


def parse_run(argv):
    """(args, config, config path) of hil_test.py's arguments."""
    config_path = Path(os.environ.get('CONFIG') or ROOT / 'test/hil/tinyusb.json').resolve()
    if not (ROOT / 'test/hil/hil_test.py').is_file():
        fail(f'{ROOT} does not look like a tinyusb checkout')
    args = helper('hil_args').build_parser().parse_args([*argv, str(config_path)])
    check_build_dir(args.build_dir)
    try:
        config = json.loads(config_path.read_text(encoding='utf-8'))
    except (OSError, ValueError) as e:
        fail(f'could not read the config {config_path}: {e}')
    return args, config, config_path


def write_receipt(argv):
    out, argv = pop_option(argv, '--out')
    if not out:
        fail('receipt needs --out FILE')
    built_head, argv = pop_option(argv, '--head')
    if not built_head:
        fail('receipt needs --head SHA, the HEAD the build began at')
    args, config, config_path = parse_run(argv)
    boards = select_boards(config, args)
    dirs = [(b, d) for b in boards for d in variant_dirs(config, args.build_dir, b)]
    files = staged_files([d for _, d in dirs])
    # only example firmware: CMake's compiler-ABI probes leave a .bin in a dir never built
    examples = {d: example_firmware(d, files) for _, d in dirs}
    unbuilt = [d.relative_to(ROOT).as_posix() for _, d in dirs if not examples[d]]
    if unbuilt:
        fail(f'not built: {", ".join(unbuilt)}; a receipt covers every variant the run selects, build with\n'
             f'  {build_command(config_path, boards, args.build_dir)}')
    stale = [f'{d.relative_to(ROOT).as_posix()}/{e}' for b, d in dirs for e in unverified_examples(b, d, examples[d])]
    if stale:
        fail(f'firmware of an example the build does not build: {", ".join(stale)}; a receipt pins only firmware '
             f'the build verified and a run stages every file there, so remove those dirs and write the receipt again')
    dirty = git('status', '--porcelain', '--untracked-files=normal')
    if dirty:
        fail(f'the tree is not clean:\n{dirty}\na receipt pins the firmware to a commit: commit these (a hw/bsp/family.json '
             f'a build rewrote included) or remove them, then rebuild on that commit')
    head = git('rev-parse', 'HEAD')
    if head != built_head:
        fail(f'HEAD is {head[:12]}, not {built_head[:12]} the build began at: a commit landed during the build, '
             f'so its firmware is not of HEAD; build again')
    receipt = {'head': head, 'configDigest': digest(config_path), 'boards': boards, 'files': files}
    Path(out).parent.mkdir(parents=True, exist_ok=True)
    write_json(Path(out), receipt)
    print(json.dumps({**receipt, 'files': len(receipt['files'])}))
    return 0


def check_receipt(path, config_path, boards, dirs, staged):
    """Refuse to stage firmware the build receipt at path does not pin to this HEAD and roster:
    dirs are every variant dir of the run's boards, staged the checkout-relative digests of
    the firmware to stage."""
    try:
        receipt = json.loads(Path(path).read_text(encoding='utf-8'))
        head, pinned = receipt['head'], receipt['files']
        config_digest, built = receipt['configDigest'], receipt['boards']
    except (OSError, ValueError, KeyError, TypeError) as e:
        fail(f'unusable build receipt {path}: {e!r}')
    unbuilt = [b for b in boards if b not in built]
    prefixes = tuple(f'{d.relative_to(ROOT).as_posix()}/' for d in dirs)
    differ = sorted({*(p for p in staged if pinned.get(p) != staged[p]),
                     *(p for p in pinned if p.startswith(prefixes) and p not in staged)})
    why = [*checkout_drift(head, config_digest, config_path),
           unbuilt and f'it does not cover {", ".join(unbuilt)}',
           differ and f'{len(differ)} staged file(s) differ from it, first {", ".join(differ[:5])}']
    if any(why):
        fail(f'the build receipt {path} does not match: {"; ".join(filter(None, why))}; rebuild and write a new receipt')
    return head, config_digest


def checkout_drift(head, config_digest, config_path):
    """Why the harness and roster the checkout stages are not those of head and config_digest."""
    now = git('rev-parse', 'HEAD')
    # the harness and roster staged beside the firmware are HEAD's only on a clean tree
    dirty = git('status', '--porcelain', '--untracked-files=no')
    return [head != now and f'it is for {head[:12]} but the checkout is at {now[:12]}',
            dirty and f'tracked files changed since HEAD: {", ".join(line.split(None, 1)[1] for line in dirty.splitlines()[:5])}',
            config_digest != digest(config_path) and f'the roster {config_path} is not the one it was built from']


def run(argv, receipt=None):
    """One remote run: (its exit status, the report files copied back)."""
    remote = os.environ.get('REMOTE', 'ci.lan')
    remote_dir = os.environ.get('REMOTE_DIR', '/tmp/tinyusb-hil')
    check_remote_dir(remote_dir)
    args, config, config_path = parse_run(argv)
    try:
        helper('hil_lock')   # the rig refuses an off-pin lib/agentrc too, but only after the wipe
    except ImportError as e:
        if e.name != 'fcntl':   # Windows: agentrc.load() passed the pin check before the POSIX-only lock ran
            fail(e)
    if args.build:
        fail(f'--build would build on the rig, which gets binaries only; build locally with\n'
             f'  {build_command(config_path, select_boards(config, args), args.build_dir)}')
    firmware = resolve_firmware(config, config_path, args)
    snap_parent = None
    if os.name == 'nt':   # in the checkout, where local_operand() can name it to rsync
        snap_parent = ROOT / RUNS_DIR
        snap_parent.mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='hil-remote-', dir=snap_parent) as snap:
        pinned = None
        if receipt is not None:
            firmware = snapshot(firmware, Path(snap))
            boards = select_boards(config, args)
            pinned = check_receipt(receipt, config_path, boards,
                                   [d for b in boards for d in variant_dirs(config, args.build_dir, b)],
                                   staged_files(firmware, Path(snap)))
        for p in (config_path, *firmware):
            local_operand(p)   # Windows: refuse one rsync cannot name now, not after the lease wiped the rig's tree

        print(f'==> Setting up remote {remote}:{remote_dir}')
        with remote_lease(remote, remote_dir, args.build_dir) as (remote_dir, token):
            return stage_and_run(remote, remote_dir, token, argv, args, config, config_path, firmware, receipt, pinned)


def stage_and_run(remote, remote_dir, token, argv, args, config, config_path, firmware, receipt=None, pinned=None):
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
            if rsync(guard, '-q', sidecar, f'{remote}:{remote_dir}/') != 0:
                fail('could not upload hil_report.json')

    print('==> Copying the harness and config')
    if rsync(guard, '-aqR', *HARNESS_FILES, f'{remote}:{remote_dir}/') != 0 or \
            rsync(guard, '-q', config_path, f'{remote}:{remote_dir}/test/hil/') != 0:
        fail('could not stage the harness')
    print(f'==> Copying firmware from {len(firmware)} build dir(s) under {args.build_dir}/')
    # one transfer: every dir lands under the same <build_dir>/, by its own name
    if rsync(guard, '-a', *FIRMWARE_FILTER, *firmware, f'{remote}:{remote_dir}/{args.build_dir}/') != 0:
        fail('could not stage the firmware')
    # the harness and roster went from the live checkout, which may have moved since the receipt check
    drift = pinned and list(filter(None, checkout_drift(*pinned, config_path)))
    if drift:
        fail(f'the checkout changed during the rig set-up, so what was staged is not what it checked against the build '
             f'receipt {receipt}: {"; ".join(drift)}; not running it')

    print(f'==> Running HIL test on {remote}')
    rc = subprocess.run(['ssh', *SSH_OPTS, remote, run_command(remote_dir, argv, config_path.name, token)],
                        stdin=subprocess.DEVNULL).returncode
    if rc == TREE_REPLACED:
        return rc, []     # nothing of ours ran there: no report to fetch, and another run's must not land here
    return rc, copy_back(remote, remote_dir, token, config_path.name)


def run_paths(run_id):
    if not re.fullmatch(r'[A-Za-z0-9_-][A-Za-z0-9_.-]{0,63}', run_id):
        fail(f'a run id is 1-64 of [A-Za-z0-9_.-], not starting with a dot: {run_id!r}')
    runs = ROOT / RUNS_DIR
    return runs / f'{run_id}.started.json', runs / f'{run_id}.done.json'


def write_json(path, data):
    """Atomic: a reader sees the whole record or none."""
    tmp = path.with_name(f'.{path.name}.{os.getpid()}.tmp')
    tmp.write_text(json.dumps(data) + '\n')
    tmp.replace(path)


def publish_locked(path, data):
    """Publish a record under an exclusive flock taken before it appears, refusing an existing
    one; returns the fd that holds the lock, which the kernel drops when this process dies."""
    tmp = path.with_name(f'.{path.name}.{os.getpid()}.tmp')
    try:
        f = open(tmp, 'x')
    except FileExistsError:
        if tmp.stat().st_nlink > 1:   # a killed run's hidden name under a reused pid: writing it rewrites that record
            raise
        tmp.unlink()
        f = open(tmp, 'x')
    with f:
        f.write(json.dumps(data) + '\n')
    fd = os.open(tmp, os.O_RDONLY)
    try:
        if fcntl:
            fcntl.flock(fd, fcntl.LOCK_EX)
        else:
            win_lock(fd)
        os.link(tmp, path)
    except BaseException:
        os.close(fd)
        tmp.unlink(missing_ok=True)
        raise
    if fcntl:   # Windows cannot remove a name of a file still open: run_recorded does, after the close
        tmp.unlink(missing_ok=True)
    return fd


# msvcrt locks are mandatory and per byte: lock one far past the record, so readers still read it
WIN_LOCK_OFFSET = 0x7FFFFFFE


def win_lock(fd, block=True):
    """The Windows stand-in for flock(LOCK_EX), held until win_unlock() or the handle closes;
    BlockingIOError when not `block` and another handle holds it."""
    pos = os.lseek(fd, 0, os.SEEK_CUR)
    os.lseek(fd, WIN_LOCK_OFFSET, os.SEEK_SET)
    try:
        while True:
            try:
                msvcrt.locking(fd, msvcrt.LK_NBLCK, 1)
                return
            except OSError as e:
                if e.errno not in (errno.EACCES, errno.EDEADLOCK):
                    raise
                if not block:
                    raise BlockingIOError
                time.sleep(0.05)   # LK_LOCK gives up after about 10 s
    finally:
        os.lseek(fd, pos, os.SEEK_SET)


def win_unlock(fd):
    """Release win_lock() now: Windows drops a closed handle's lock only "after a delay"."""
    pos = os.lseek(fd, 0, os.SEEK_CUR)
    os.lseek(fd, WIN_LOCK_OFFSET, os.SEEK_SET)
    try:
        msvcrt.locking(fd, msvcrt.LK_UNLCK, 1)
    finally:
        os.lseek(fd, pos, os.SEEK_SET)


# a waiter's probe holds the lock for a moment; a run holds it for its whole life
PROBE_TRIES, PROBE_SECS = 10, 0.02


def win_probe(fd):
    """flock(LOCK_SH | LOCK_NB) for run_alive(): msvcrt has no shared lock, so two waiters' probes
    contend; BlockingIOError only when the lock stays held across the retries."""
    for _ in range(PROBE_TRIES):
        try:
            win_lock(fd, block=False)
        except BlockingIOError:
            time.sleep(PROBE_SECS)
            continue
        win_unlock(fd)
        return
    raise BlockingIOError


def pop_option(argv, name):
    """(the option's value or None, argv without it): the rest goes to hil_test.py."""
    for i, a in enumerate(argv):
        if a == name:
            value, rest = (argv[i + 1] if i + 1 < len(argv) else ''), argv[:i] + argv[i + 2:]
        elif a.startswith(f'{name}='):
            value, rest = a.partition('=')[2], argv[:i] + argv[i + 1:]
        else:
            continue
        if not value:
            fail(f'{name} needs a value')
        return value, rest
    return None, argv


def run_recorded(run_id, argv, receipt):
    """run() between a started record, locked while it lives, and a done record written
    whatever ends it short of a kill: what `wait` reads, since nothing else outlives a
    background run."""
    started, done = run_paths(run_id)
    started.parent.mkdir(exist_ok=True)
    if not fcntl:
        sweep_stale_tmps(started.parent)
    try:
        lock = publish_locked(started, {'runId': run_id, 'pid': os.getpid(), 'startedAt': time.time()})
    except FileExistsError:
        fail(f'run id {run_id} was used before ({started.relative_to(ROOT).as_posix()}); pick a new one')
    rc, reports = 1, []
    try:
        rc, reports = run(argv, receipt)
        return rc
    except SystemExit as e:
        rc = e.code if isinstance(e.code, int) else int(e.code is not None)
        raise
    finally:
        write_json(done, {'runId': run_id, 'exit': rc, 'reports': reports})
        try:
            if not fcntl:
                win_unlock(lock)
        finally:
            os.close(lock)
        if not fcntl:
            with contextlib.suppress(OSError):   # a waiter's probe holding the file open makes Windows refuse; the next sweep takes it
                started.with_name(f'.{started.name}.{os.getpid()}.tmp').unlink(missing_ok=True)


def sweep_stale_tmps(runs):
    """Windows: remove the hidden second name of each started record a killed run left behind
    (run_recorded removes its own after the close, which a kill skips). A live run holds its
    file open, which Windows refuses to unlink, and a launch not yet linked has one name."""
    for tmp in runs.glob('.*.started.json.*.tmp'):
        try:
            if tmp.stat().st_nlink > 1:
                tmp.unlink()
        except OSError:
            pass


def run_alive(started):
    """Its run still holds the started record's lock."""
    fd = os.open(started, os.O_RDONLY)
    try:
        if fcntl:
            fcntl.flock(fd, fcntl.LOCK_SH | fcntl.LOCK_NB)
        else:
            win_probe(fd)
        return False
    except BlockingIOError:
        return True
    finally:
        os.close(fd)


def wait_secs(text):
    secs = float(text)
    if not 0 <= secs <= WAIT_SECS:
        raise argparse.ArgumentTypeError(f'0 to {WAIT_SECS} s: one wait must end inside the Bash tool\'s 10-min cap')
    return secs


def wait(argv):
    p = argparse.ArgumentParser(prog='hil_remote.py wait',
                                description=f'exit 0 done, {RUNNING} still running at --timeout, '
                                            f'{DEAD} ended without a done record')
    p.add_argument('run_id')
    p.add_argument('--timeout', type=wait_secs, default=WAIT_SECS, help=f'seconds, at most {WAIT_SECS} (the default)')
    a = p.parse_args(argv)
    started, done = run_paths(a.run_id)
    begun = time.monotonic()
    deadline = begun + a.timeout
    while True:
        if done.is_file():
            print(json.dumps({'state': 'done', **json.loads(done.read_text())}))
            return 0
        try:
            rec = json.loads(started.read_text())
        except FileNotFoundError:
            grace = min(begun + START_SECS, deadline) - time.monotonic()
            if grace > 0:
                time.sleep(min(POLL_SECS, grace))
                continue
            fail(f'no run {a.run_id} was started from {ROOT}')
        if not run_alive(started):
            if done.is_file():
                continue    # it wrote the done record between the two checks
            print(json.dumps({'state': 'dead', 'runId': a.run_id, 'pid': rec['pid'],
                              'detail': 'the run ended without a done record (killed, or its session died): '
                                        'no local report is from it'}))
            return DEAD
        left = deadline - time.monotonic()
        if left <= 0:
            print(json.dumps({'state': 'running', 'runId': a.run_id, 'pid': rec['pid'],
                              'elapsedSecs': round(time.time() - rec['startedAt'])}))
            return RUNNING
        time.sleep(min(POLL_SECS, left))


def main(argv):
    if argv[:1] == ['wait']:
        return wait(argv[1:])
    if argv[:1] == ['receipt']:
        return write_receipt(argv[1:])
    run_id, argv = pop_option(argv, '--run-id')
    receipt, argv = pop_option(argv, '--receipt')
    return run(argv, receipt)[0] if run_id is None else run_recorded(run_id, argv, receipt)


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
