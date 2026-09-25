#!/usr/bin/env python3
"""Build base branch (master) and current tree, then compare code size metrics.

Creates cmake-metrics/<board>/{base,build} directories for each board.
With --combined, also writes cmake-metrics/_combined/metrics_compare.md over every
board's elf pairs. Base and current elfs are paired by (board, elf path) and each
pair's per-file deltas are reported; the sizes come from --engine (membrowse,
linkermap or bloaty, see tools/metrics_compare.py).

Usage:
  python tools/metrics_compare_base.py -b raspberry_pi_pico
  python tools/metrics_compare_base.py -b raspberry_pi_pico -b raspberry_pi_pico2
  python tools/metrics_compare_base.py -b raspberry_pi_pico -f portable/raspberrypi
  python tools/metrics_compare_base.py -b raspberry_pi_pico -e device/cdc_msc
  python tools/metrics_compare_base.py -b raspberry_pi_pico -e device/cdc_msc --bloaty
  python tools/metrics_compare_base.py -b raspberry_pi_pico --engine linkermap --json
  python tools/metrics_compare_base.py --ci                                  # CI-pinned boards, combined
  python tools/metrics_compare_base.py -b raspberry_pi_pico -b raspberry_pi_pico2 --combined  # combine listed boards
"""
import argparse
import concurrent.futures
import glob
import json
import os
import re
import runpy
import shlex
import shutil
import subprocess
import sys

import metrics_compare

TINYUSB_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
METRICS_DIR = os.path.join(TINYUSB_ROOT, 'cmake-metrics')
CI_PINNED_BOARDS = os.path.join(TINYUSB_ROOT, '.github', 'ci-pinned-boards.json')

def tinyusb_src_filter(checkout_dir):
    """Return a path-substring filter that uniquely matches TinyUSB stack source files
    in `checkout_dir`. The substring is the absolute path to the checkout's `src/`
    dir — collision-free with vendored deps (pico-sdk, lwip, FreeRTOS, etc.) which
    live at unrelated paths."""
    return os.path.realpath(os.path.join(checkout_dir, 'src')) + os.sep

verbose = False


def run(cmd, **kwargs):
    """Run a command. cmd must be a list (no shell=True). On `timeout=`-induced
    TimeoutExpired, return a CompletedProcess with rc=124 instead of letting the
    exception propagate, so the caller can fall through to error reporting and
    worktree cleanup rather than crashing with a traceback."""
    if not isinstance(cmd, list):
        raise TypeError('run() requires a list, got str — fix the caller')
    if verbose:
        print(f'  $ {" ".join(shlex.quote(str(c)) for c in cmd)}')
    try:
        return subprocess.run(cmd, capture_output=True, text=True, **kwargs)
    except subprocess.TimeoutExpired as e:
        msg = f'Command timed out after {e.timeout}s: {" ".join(shlex.quote(str(c)) for c in cmd)}'
        stderr = (e.stderr or '') + ('\n' if e.stderr else '') + msg
        return subprocess.CompletedProcess(cmd, 124, stdout=(e.stdout or ''), stderr=stderr)


def symlink_deps(main_root, worktree_dir):
    """Symlink each dependency the worktree's own tools/get_deps.py lists, when fetched in
    the main checkout: the worktree lacks these untracked dirs, and a base revision can
    name paths the current manifest has renamed or dropped."""
    manifest = runpy.run_path(os.path.join(worktree_dir, 'tools', 'get_deps.py'))
    for rel in manifest['deps_all']:
        src = os.path.join(main_root, rel)
        dst = os.path.join(worktree_dir, rel)
        if os.path.isdir(src) and not os.path.lexists(dst):
            os.makedirs(os.path.dirname(dst), exist_ok=True)
            os.symlink(src, dst)


def ci_pinned_boards():
    """Boards of .github/ci-pinned-boards.json: CI's membrowse set, which covers every
    dcd/hcd driver not waived in its `uncovered` list (drivers-coverage hook)."""
    with open(CI_PINNED_BOARDS) as f:
        return [entry['board'] for entry in json.load(f)['boards']]


def build_board(src_dir, build_dir, board, example=None):
    """Configure and build examples for a board. Returns True on success.

    When `example` is given, only that target is built (`cmake --build --target NAME`),
    keeping single-example workflows fast.
    """
    os.makedirs(build_dir, exist_ok=True)
    ret = run(['cmake', '-B', build_dir, '-G', 'Ninja',
               f'-DBOARD={board}', '-DCMAKE_BUILD_TYPE=MinSizeRel',
               os.path.join(src_dir, 'examples')])
    if ret.returncode != 0:
        print(f'  Error configuring {board}: {ret.stderr}')
        return False
    cmd = ['cmake', '--build', build_dir]
    if example:
        cmd += ['--target', os.path.basename(example)]
    ret = run(cmd, timeout=600)
    if ret.returncode != 0:
        print(f'  Error building {board}: {ret.stderr}')
        return False
    return True


def report_path(board, example):
    """A scope's report path without extension: cmake-metrics/<board>/metrics_compare[_<ex>]."""
    suffix = f'_{example.replace("/", "_")}' if example else ''
    return os.path.join(METRICS_DIR, board, f'metrics_compare{suffix}')


def generate_sizes(build_dir, filters, example=None, engine='membrowse'):
    """Return (sizes, errors) for the scope's elfs.

    `sizes` maps each elf path relative to `build_dir` to its
    metrics_compare.ENGINES[engine] sizes, or to None when that failed; `errors`
    lists (relative elf path, message), the path None when no elf can be
    sized at all.
    """
    # escape the dir, not the wildcards: a checkout path is a path, not a pattern
    root = glob.escape(build_dir)
    pattern = f'{root}/{example}/*.elf' if example \
        else f'{root}/**/*.elf'
    elfs = sorted(glob.glob(pattern, recursive=True))
    if not elfs:
        print(f'  Error: no .elf files in {build_dir}')
        return {}, [(None, f'no .elf files in {build_dir}')]

    sizer = metrics_compare.ENGINES[engine].sizes

    def report(elf):
        try:
            return sizer(elf, filters), None
        except RuntimeError as e:
            return None, str(e)

    # report errors are caught here, not as tracebacks after both builds already ran
    try:
        with concurrent.futures.ThreadPoolExecutor() as pool:
            results = list(pool.map(report, elfs))
    except FileNotFoundError as e:
        print(f'  Error: {engine} not found ({e}) - install {metrics_compare.ENGINES[engine].install}, '
              f'or pick another --engine')
        return {}, [(None, f'{engine} not found')]
    sizes, errors = {}, []
    for elf, (elf_sizes, error) in zip(elfs, results):
        rel = os.path.relpath(elf, build_dir)
        sizes[rel] = elf_sizes
        if error:
            print(f'  Error: {error}')
            errors.append((rel, error))
    return sizes, errors


def write_report(path, md, data=None):
    """Write a report to `path`.md, and `data` to `path`.json when given; stdout
    gets the Markdown without the per-pair details."""
    with open(f'{path}.md', 'w') as f:
        f.write(md)
    print(md.split('\n<details>')[0].rstrip())
    print(f'  report: {path}.md')
    if data is not None:
        with open(f'{path}.json', 'w') as f:
            json.dump(data, f, indent=1, sort_keys=True)
            f.write('\n')
        print(f'  json: {path}.json')


def main():
    global verbose

    parser = argparse.ArgumentParser(description='Compare code size metrics with base branch')
    parser.add_argument('-b', '--board', action='append', default=[],
                        help='Board name (repeatable). Required unless --ci is given.')
    parser.add_argument('-f', '--filter', action='append', default=None,
                        help='Path-substring filter (repeatable). When given, '
                             'overrides the default and is applied to BOTH base and '
                             'current builds. Default: each side\'s own absolute '
                             '<checkout>/src/ path, which uniquely matches TinyUSB '
                             'stack code without colliding with vendored deps.')
    parser.add_argument('--base-branch', default='master',
                        help='Base branch to compare against (default: master)')
    parser.add_argument('-e', '--example', action='append', default=None,
                        help='Compare specific example (repeatable, e.g. -e device/cdc_msc -e host/cdc_msc_hid)')
    parser.add_argument('--bloaty', action='store_true',
                        help='Also print bloaty\'s section and symbol diff of each -e example '
                             '(console only, whatever --engine)')
    parser.add_argument('--engine', choices=sorted(metrics_compare.ENGINES),
                        default='membrowse',
                        help='Where per-file sizes come from: membrowse (default, symbols with '
                             'linker-script regions), linkermap (the GNU ld map\'s input '
                             'sections) or bloaty (DWARF compile units)')
    parser.add_argument('--json', action='store_true',
                        help='Also write each report\'s paired sizes as metrics_compare*.json '
                             'next to its .md')
    parser.add_argument('--ci', action='store_true',
                        help='Add the CI-pinned boards (.github/ci-pinned-boards.json, covering '
                             'every dcd/hcd driver not waived there). Implies --combined.')
    parser.add_argument('--combined', action='store_true',
                        help='Also write one comparison over every board '
                             '(cmake-metrics/_combined/metrics_compare.md), in addition to per-board.')
    parser.add_argument('-v', '--verbose', action='store_true',
                        help='Print build commands')
    args = parser.parse_args()
    verbose = args.verbose

    if args.bloaty and not args.example:
        parser.error('--bloaty requires -e/--example')

    if args.ci:
        args.combined = True
        ci_boards = ci_pinned_boards()
        # Append, dedup, preserve order
        seen = set(args.board)
        for b in ci_boards:
            if b not in seen:
                args.board.append(b)
                seen.add(b)

    if not args.board:
        parser.error('at least one -b BOARD is required (or pass --ci)')

    worktree_dir = os.path.join(METRICS_DIR, '_worktree')

    # Per-side filters: when no override is given, each build uses its own
    # absolute <checkout>/src/ path so we only match TinyUSB stack code from that
    # checkout (and never vendored-dep `src/` like pico-sdk/src/...).
    if args.filter:
        base_filters = cur_filters = list(args.filter)
    else:
        base_filters = [tinyusb_src_filter(worktree_dir)]
        cur_filters = [tinyusb_src_filter(TINYUSB_ROOT)]

    # Drop every report this run will write before anything can fail - a run that
    # stops early (worktree setup, a build) must not leave a previous run's report
    # for a reader to take for this one's: cmake-metrics/ is gitignored and persists.
    # .json too: a run without --json must not leave an older one beside a newer .md.
    examples = args.example or [None]
    combined_dir = os.path.join(METRICS_DIR, '_combined')
    if args.combined:
        shutil.rmtree(combined_dir, ignore_errors=True)
    for board in args.board:
        for example in examples:
            for ext in ('md', 'json'):
                stale_path = f'{report_path(board, example)}.{ext}'
                if os.path.isfile(stale_path):
                    os.remove(stale_path)

    # Step 1: Create worktree for base branch
    print(f'[1/5] Setting up {args.base_branch} worktree...')
    if os.path.isdir(worktree_dir):
        run(['git', '-C', TINYUSB_ROOT, 'worktree', 'remove', '--force', worktree_dir])
    # --detach: check out the ref at a detached HEAD instead of trying to claim the
    # branch. Lets us add a worktree of `master` even if master is already checked
    # out elsewhere (main repo, another worktree).
    ret = run(['git', '-C', TINYUSB_ROOT, 'worktree', 'add', '--detach',
               worktree_dir, args.base_branch])
    if ret.returncode != 0:
        print(f'Error creating worktree: {ret.stderr}')
        sys.exit(1)

    symlink_deps(TINYUSB_ROOT, worktree_dir)

    # the commit actually built, which the ref may no longer name later
    base_sha = run(['git', '-C', worktree_dir, 'rev-parse', 'HEAD']).stdout.strip()

    def report_data(data):
        """The JSON report for --json, None without it."""
        if not args.json:
            return None
        return {**data, 'base_ref': args.base_branch, 'base_sha': base_sha,
                'filters': {'base': base_filters, 'current': cur_filters}}

    failed = False
    try:
        # --combined: every board's elf sizes and failures, paired at the end
        combined_sides = {'base': {}, 'current': {}}
        combined_failures = []

        for board in args.board:
            print(f'\n=== {board} ===')
            board_dir = os.path.join(METRICS_DIR, board)
            base_build = os.path.join(board_dir, 'base')
            cur_build = os.path.join(board_dir, 'build')
            shutil.rmtree(base_build, ignore_errors=True)
            shutil.rmtree(cur_build, ignore_errors=True)

            # Build only the requested examples (or all if -e not given). Single-example
            # mode used to build everything and filter at metric time — that was wasted work.
            board_failed = None  # the side whose build failed
            for example in examples:
                build_label = f' --target {os.path.basename(example)}' if example else ''
                print(f'[2/5] Building {args.base_branch} for {board}{build_label}...')
                if not build_board(worktree_dir, base_build, board, example):
                    board_failed = 'base'
                    break
                print(f'[3/5] Building current for {board}{build_label}...')
                if not build_board(TINYUSB_ROOT, cur_build, board, example):
                    board_failed = 'current'
                    break
            if board_failed:
                failed = True
                # still write each scope's report, with the build failure in it
                build_failure = ((board, None), board_failed, 'build', f'build failed{build_label}, see log')
                combined_failures.append(build_failure)

            for example in examples:
                label = f' ({example})' if example else ''

                # Step 4/5: Generate sizes and compare
                sides = {'base': {}, 'current': {}}
                failures = [build_failure] if board_failed else []
                if not board_failed:
                    print(f'[4/5] Sizing {board}{label} with {args.engine}...')
                    for side, build, filters in (('base', base_build, base_filters),
                                                 ('current', cur_build, cur_filters)):
                        sizes, errors = generate_sizes(build, filters, example, args.engine)
                        sides[side] = {(board, rel): v for rel, v in sizes.items()}
                        failures += [((board, rel), side, 'report', msg) for rel, msg in errors]

                print(f'[5/5] Comparing {board}{label}...')
                md, failures, ok, data = metrics_compare.compare_sides(
                    sides['base'], sides['current'], args.engine, failures, [board], scope=(board, None))
                failed |= not ok
                if not board_failed:  # a build failure is recorded once, above
                    for side, sizes in sides.items():
                        combined_sides[side].update(sizes)
                    combined_failures += failures
                write_report(report_path(board, example), md, report_data(data))

                # Optional: bloaty diff
                if args.bloaty and example and not board_failed:
                    elf_name = os.path.basename(example)
                    base_elf = os.path.join(base_build, example, f'{elf_name}.elf')
                    cur_elf = os.path.join(cur_build, example, f'{elf_name}.elf')
                    if os.path.exists(base_elf) and os.path.exists(cur_elf):
                        # Bloaty expects one regex; OR-join all filters (current side
                        # for the new ELF, base side for the base ELF).
                        bloaty_regex = '(' + '|'.join(
                            re.escape(f) for f in (cur_filters + base_filters)
                        ) + ')'
                        bloaty_common = ['bloaty', '--domain=vm', f'--source-filter={bloaty_regex}']
                        print(f'--- bloaty sections ---')
                        ret = run(bloaty_common + ['-d', 'compileunits,sections', cur_elf, '--', base_elf])
                        print(ret.stdout)
                        print(f'--- bloaty symbols ---')
                        ret = run(bloaty_common + ['-d', 'compileunits,symbols', '-s', 'vm',
                                                    cur_elf, '--', base_elf])
                        print(ret.stdout)
                    else:
                        print(f'  bloaty: ELF not found')

        # Optional combined comparison across all boards.
        if args.combined:
            os.makedirs(combined_dir, exist_ok=True)
            print(f'\n=== combined ({len(args.board)} boards) ===')
            # every scope was filter-checked above, and its failures carried over
            md, _failures, ok, data = metrics_compare.compare_sides(
                combined_sides['base'], combined_sides['current'], args.engine,
                combined_failures, args.board)
            failed |= not ok
            write_report(os.path.join(combined_dir, 'metrics_compare'), md, report_data(data))
    finally:
        print(f'\nCleaning up worktree...')
        run(['git', '-C', TINYUSB_ROOT, 'worktree', 'remove', '--force', worktree_dir])
    return 1 if failed else 0


if __name__ == '__main__':
    sys.exit(main())
