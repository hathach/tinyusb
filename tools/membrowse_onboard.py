#!/usr/bin/env python3
"""Backfill membrowse history for one CI board/example.

Thin wrapper over `membrowse onboard` that derives every convention-bound
argument from the board and example name, so a backfill cannot land under a
target name that differs from what CI uploads (family_support.cmake uses
`<board>/<cmake-target>`, i.e. the example BASENAME - not the role/name path).
That includes the linker scripts and --defsym values: they're extracted from
the same build dir's ninja graph, using membrowse_report.py's own extraction
helpers, so a backfill is computed over the SAME regions CI's per-commit
upload used - not membrowse's DEFAULT Code/Data regions, which would land
under the same target name as CI but read as a step-discontinuity code-size
event in the dashboard (and be unfixable after the fact: history is keyed on
the target name).

Composes, from repo-root-relative conventions:
  build dir:    examples/cmake-build-<board>   (must be configured beforehand)
  build script: cmake --build <build_dir> --target <basename>
  elf path:     <build_dir>/<role>/<basename>/<basename>.elf
  target name:  <board>/<basename>
  ld scripts:   extracted from `ninja -t commands <basename>` in build dir
  --defsym      ditto
  change scope: --build-dirs src/ hw/          (skip rebuilds elsewhere)

Dry-run by default; pass --upload for the real run (requires MEMBROWSE_API_KEY
in the environment - read at run time, passed as argv, never printed).
"""
import argparse
import os
import shutil
import subprocess
import sys

from membrowse_report import ninja_commands, extract_ld_scripts, extract_defsyms


def compose(board, example, num_commits, upload, api_key, extra,
            ld_scripts=(), defsyms=()):
    """Return the membrowse onboard argv for one board/example backfill."""
    basename = os.path.basename(example.rstrip('/'))
    build_dir = f'examples/cmake-build-{board}'
    cmd = [
        'membrowse', 'onboard', str(num_commits),
        f'cmake --build {build_dir} --target {basename}',
        f'{build_dir}/{example}/{basename}.elf',
        f'{board}/{basename}',
        api_key,
        '--build-dirs', 'src/', 'hw/',
    ]
    if ld_scripts:
        cmd += ['--ld-scripts', ' '.join(ld_scripts)]
    for sym in defsyms:
        cmd += ['--def', sym]
    if not upload:
        cmd.append('--dry-run')
    cmd += extra
    return cmd


def main():
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

    build_dir = f'examples/cmake-build-{args.board}'
    if not os.path.isdir(build_dir):
        sys.exit(f'{build_dir} not configured - run from the repo root after e.g.\n'
                 f'  cmake -B {build_dir} -DBOARD={args.board} -G Ninja '
                 f'-DCMAKE_BUILD_TYPE=MinSizeRel examples')

    # Same ninja-graph extraction CI's family_add_membrowse()/membrowse_report.py
    # uses, so the backfill lands under the same regions as CI's own uploads for
    # this target name (see the module docstring). ninja_commands() itself exits
    # loudly on a failed query.
    basename = os.path.basename(args.example.rstrip('/'))
    ninja = shutil.which('ninja') or 'ninja'
    commands_text = ninja_commands(ninja, build_dir, basename)
    ld_scripts = extract_ld_scripts(commands_text)
    defsyms = extract_defsyms(commands_text)
    if not ld_scripts:
        sys.exit(f'no linker script found in the ninja build graph for target '
                 f'{basename!r} in {build_dir!r} - a backfill would compute over '
                 f'different regions than CI under the same target name; check '
                 f'the board/example, or that {build_dir} was built at least once')

    cmd = compose(args.board, args.example, args.num_commits, args.upload,
                  api_key or 'dry-run-placeholder', extra, ld_scripts, defsyms)

    shown = [('***' if c == api_key and api_key else c) for c in cmd]
    print('+ ' + ' '.join(shown), flush=True)
    return subprocess.run(cmd).returncode


if __name__ == '__main__':
    sys.exit(main())
