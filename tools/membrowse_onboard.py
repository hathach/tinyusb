#!/usr/bin/env python3
"""Backfill membrowse history for one CI board/example.

Thin wrapper over `membrowse onboard` that derives every convention-bound
argument from the board and example name, so a backfill cannot land under a
target name that differs from what CI uploads (family_support.cmake uses
`<board>/<cmake-target>`, i.e. the example BASENAME - not the role/name path).
Each historical build writes a small linker shim from that commit's ninja graph,
so linker scripts and --defsym values match the ELF being reported rather than
the caller's current checkout.

`membrowse onboard` (membrowse/commands/onboard.py) checks out and `git clean
-fdx`s every historical commit unconditionally, in whatever directory it runs.
Run in place, that would detach HEAD and wipe every ignored file in this repo
root - deps symlinks included - regardless of how clean `git status` looked
first. This wrapper instead runs it in a disposable `git worktree`
(cmake-metrics/_onboard_worktree, removed when done), symlinking deps into it
first the same way metrics_compare_base.py's base-branch build does - and
since that same clean also strips the symlinks it just made, the build
script below re-links them before every historical build too, not just
the first.

Composes, from repo-root-relative conventions:
  build dir:    examples/cmake-build-<board> (reconfigured for every commit)
  build script: <relink deps> && cmake -S examples -B <build_dir> ... &&
                cmake --build <build_dir> --target <basename>
  elf path:     <build_dir>/<role>/<basename>/<basename>.elf
  target name:  <board>/<basename>
  ld scripts:   regenerated after every build from its ninja graph
  change scope: --build-dirs src/ hw/ examples/<role>/<name>/  (skip rebuilds
                elsewhere - the example's own dir is in scope too, since its
                sources link into the same elf as src/ and hw/)

Dry-run by default; pass --upload for the real run (requires MEMBROWSE_API_KEY
in the environment - read at run time, passed as argv, never printed).
"""
import argparse
import os
import shlex
import shutil
import subprocess
import sys

from membrowse_report import LD_SCRIPT_RE, ninja_commands, extract_defsyms
from metrics_compare_base import symlink_deps


def compose(board, example, num_commits, upload, api_key, extra,
            repo_root=None, worktree_dir=None):
    """Return the membrowse onboard argv for one board/example backfill."""
    basename = os.path.basename(example.rstrip('/'))
    build_dir = f'examples/cmake-build-{board}'
    # `membrowse onboard` runs `git clean -fdx` before every historical build,
    # which deletes this ignored build_dir - reconfigure it fresh each time
    # instead of relying on the one checked below to survive.
    configure = (f'cmake -S examples -B {build_dir} -DBOARD={board} '
                 f'-G Ninja -DCMAKE_BUILD_TYPE=MinSizeRel')
    build_script = f'{configure} && cmake --build {build_dir} --target {basename}'
    shim_path = f'{build_dir}/.membrowse-onboard.ld'
    if repo_root and worktree_dir:
        # The same `git clean -fdx` also wipes the dep symlinks main() set up
        # before the run - before EVERY historical build, not just the first.
        # Re-run this script (by absolute path, so it's the current checkout's
        # copy, not whatever tools/membrowse_onboard.py looked like at the
        # historical commit) to recreate them each time, right after the clean.
        relink = (f'{shlex.quote(sys.executable)} {shlex.quote(os.path.abspath(__file__))} '
                 f'--relink-deps {shlex.quote(repo_root)} {shlex.quote(worktree_dir)}')
        build_script = f'{relink} && {build_script}'
        write_shim = (f'{shlex.quote(sys.executable)} {shlex.quote(os.path.abspath(__file__))} '
                      f'--write-linker-shim {shlex.quote(shutil.which("ninja") or "ninja")} '
                      f'{shlex.quote(build_dir)} {shlex.quote(basename)} {shlex.quote(shim_path)}')
        build_script = f'{build_script} && {write_shim}'
    cmd = [
        'membrowse', 'onboard', str(num_commits),
        build_script,
        f'{build_dir}/{example}/{basename}.elf',
        f'{board}/{basename}',
        api_key,
        '--build-dirs', 'src/', 'hw/', f'examples/{example.rstrip("/")}/',
    ]
    if repo_root and worktree_dir:
        cmd += ['--ld-scripts', shim_path]
    if not upload:
        cmd.append('--dry-run')
    cmd += extra
    return cmd


def write_linker_shim(ninja, build_dir, target, output):
    """Write current build's linker scripts and defsyms to a stable path."""
    commands = ninja_commands(ninja, build_dir, target)
    scripts = list(dict.fromkeys(LD_SCRIPT_RE.findall(commands)))
    if not scripts:
        sys.exit(f'no linker script found in the ninja build graph for target {target!r}')

    lines = []
    for sym in extract_defsyms(commands):
        name, separator, value = sym.partition('=')
        if not separator:
            sys.exit(f'invalid --defsym value in ninja build graph: {sym!r}')
        lines.append(f'{name} = {value};')
    for script in scripts:
        path = script if os.path.isabs(script) else os.path.abspath(os.path.join(build_dir, script))
        if '"' in path or not os.path.isfile(path):
            sys.exit(f'linker script not found or unsupported: {path!r}')
        lines.append(f'INCLUDE "{path}"')
    with open(output, 'w') as f:  # NOSONAR - trusted internal CLI path
        f.write('\n'.join(lines) + '\n')
    return 0


def main():
    # Internal re-entry point: the composed build script calls this script
    # back (see compose()) to relink deps after each historical `git clean
    # -fdx`. Handled before the normal argparse below since it's a distinct
    # invocation shape, not a board/example backfill.
    if len(sys.argv) == 4 and sys.argv[1] == '--relink-deps':
        symlink_deps(sys.argv[2], sys.argv[3])
        return 0
    if len(sys.argv) == 6 and sys.argv[1] == '--write-linker-shim':
        return write_linker_shim(*sys.argv[2:])

    parser = argparse.ArgumentParser(
        description='Backfill membrowse history for one CI board/example '
                    '(dry-run unless --upload). Extra args after -- go to '
                    '`membrowse onboard` verbatim (see its --help).')
    parser.add_argument('board', help='Board name, e.g. stm32f407disco')
    parser.add_argument('example', help='Example as role/name, e.g. device/cdc_msc')
    parser.add_argument('-n', '--num-commits', type=int, default=30,
                        help='Most recent N commits to backfill (default: 30)')
    parser.add_argument('--upload', action='store_true', default=False,
                        help='Really upload (default is --dry-run)')
    args, extra = parser.parse_known_args()

    api_key = os.environ.get('MEMBROWSE_API_KEY')
    if args.upload and not api_key:
        sys.exit('MEMBROWSE_API_KEY must be set in the environment for --upload')

    # membrowse onboard checks out each past commit in THIS worktree - refuse
    # to start with uncommitted changes it could clobber or carry along. A
    # failed `git status` (not a repo, git missing) leaves stdout empty, which
    # would read as "clean" and let exactly that clobbering through: treat it
    # as fatal rather than as an answer.
    status = subprocess.run(['git', 'status', '--porcelain'],
                            capture_output=True, text=True)
    if status.returncode != 0:
        sys.exit(f'`git status --porcelain` failed (exit {status.returncode}) - '
                 f'cannot tell whether the worktree is clean:\n{status.stderr.strip()}')
    dirty = status.stdout.strip()
    if dirty:
        sys.exit('working tree is not clean - commit or stash before onboarding:\n'
                 + dirty)

    # Isolate the actual onboard run (checks out + `git clean -fdx`s every
    # historical commit in place - see the module docstring) in a disposable
    # worktree, never repo_root itself.
    repo_root = os.getcwd()
    worktree_dir = os.path.join(repo_root, 'cmake-metrics', '_onboard_worktree')

    cmd = compose(args.board, args.example, args.num_commits, args.upload,
                  api_key or 'dry-run-placeholder', extra, repo_root, worktree_dir)

    shown = [('***' if c == api_key and api_key else c) for c in cmd]
    print('+ ' + ' '.join(shown), flush=True)

    if os.path.isdir(worktree_dir):
        subprocess.run(['git', 'worktree', 'remove', '--force', worktree_dir],
                       capture_output=True)
    os.makedirs(os.path.dirname(worktree_dir), exist_ok=True)
    ret = subprocess.run(['git', 'worktree', 'add', '--detach', worktree_dir, 'HEAD'],
                         capture_output=True, text=True)
    if ret.returncode != 0:
        sys.exit(f'failed to create disposable worktree at {worktree_dir}:\n{ret.stderr}')
    symlink_deps(repo_root, worktree_dir)

    try:
        return subprocess.run(cmd, cwd=worktree_dir).returncode  # NOSONAR - trusted local developer CLI argv
    finally:
        subprocess.run(['git', 'worktree', 'remove', '--force', worktree_dir],
                       capture_output=True)


if __name__ == '__main__':
    sys.exit(main())
