#!/usr/bin/env python3
"""Backfill membrowse history for one CI board/example.

Thin wrapper over `membrowse onboard` that derives every convention-bound
argument from the board and example name, so a backfill cannot land under a
target name that differs from what CI uploads (family_support.cmake uses
`<board>/<cmake-target>`, i.e. the example BASENAME - not the role/name path).

Composes, from repo-root-relative conventions:
  build dir:    examples/cmake-build-<board>   (must be configured beforehand)
  build script: cmake --build <build_dir> --target <basename>
  elf path:     <build_dir>/<role>/<basename>/<basename>.elf
  target name:  <board>/<basename>
  change scope: --build-dirs src/ hw/          (skip rebuilds elsewhere)

Dry-run by default; pass --upload for the real run (requires MEMBROWSE_API_KEY
in the environment - read at run time, passed as argv, never printed).
"""
import argparse
import os
import subprocess
import sys


def compose(board, example, num_commits, upload, api_key, extra):
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
    # to start with uncommitted changes it could clobber or carry along
    dirty = subprocess.run(['git', 'status', '--porcelain'],
                           capture_output=True, text=True).stdout.strip()
    if dirty:
        sys.exit('working tree is not clean - commit or stash before onboarding:\n'
                 + dirty)

    cmd = compose(args.board, args.example, args.num_commits, args.upload,
                  api_key or 'dry-run-placeholder', extra)
    build_dir = f'examples/cmake-build-{args.board}'
    if not os.path.isdir(build_dir):
        sys.exit(f'{build_dir} not configured - run from the repo root after e.g.\n'
                 f'  cmake -B {build_dir} -DBOARD={args.board} -G Ninja '
                 f'-DCMAKE_BUILD_TYPE=MinSizeRel examples')

    shown = [('***' if c == api_key and api_key else c) for c in cmd]
    print('+ ' + ' '.join(shown), flush=True)
    return subprocess.run(cmd).returncode


if __name__ == '__main__':
    sys.exit(main())
