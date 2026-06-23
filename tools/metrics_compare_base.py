#!/usr/bin/env python3
"""Build base branch (master) and current tree, then compare code size metrics.

Creates cmake-metrics/<board>/{base,build} directories for each board.
With --combined, also writes cmake-metrics/_combined/metrics_compare.md aggregating
all boards into a single comparison.

Usage:
  python tools/metrics_compare_base.py -b raspberry_pi_pico
  python tools/metrics_compare_base.py -b raspberry_pi_pico -b raspberry_pi_pico2
  python tools/metrics_compare_base.py -b raspberry_pi_pico -f portable/raspberrypi
  python tools/metrics_compare_base.py -b raspberry_pi_pico -e device/cdc_msc
  python tools/metrics_compare_base.py -b raspberry_pi_pico -e device/cdc_msc --bloaty
  python tools/metrics_compare_base.py --ci                          # first board of each arm-gcc family, combined
  python tools/metrics_compare_base.py -b pico -b pico2 --combined   # aggregate listed boards
"""
import argparse
import glob
import json
import os
import re
import shlex
import subprocess
import sys

TINYUSB_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
METRICS_DIR = os.path.join(TINYUSB_ROOT, 'cmake-metrics')

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
    """Symlink dependency directories (fetched by tools/get_deps.py) from the main
    checkout into the temporary worktree. Without this, the base build fails because
    the worktree doesn't have the untracked deps."""
    def link_subdirs(rel_parent):
        src_parent = os.path.join(main_root, rel_parent)
        dst_parent = os.path.join(worktree_dir, rel_parent)
        if not os.path.isdir(src_parent):
            return
        os.makedirs(dst_parent, exist_ok=True)
        for entry in os.listdir(src_parent):
            src = os.path.join(src_parent, entry)
            dst = os.path.join(dst_parent, entry)
            if os.path.isdir(src) and not os.path.exists(dst):
                os.symlink(src, dst)

    # lib/* and tools/* deps (e.g. lib/lwip, tools/linkermap)
    link_subdirs('lib')
    link_subdirs('tools')
    # hw/mcu/<vendor>/<dep> (e.g. hw/mcu/raspberry_pi/Pico-PIO-USB)
    hw_mcu = os.path.join(main_root, 'hw', 'mcu')
    if os.path.isdir(hw_mcu):
        for vendor in os.listdir(hw_mcu):
            link_subdirs(os.path.join('hw', 'mcu', vendor))


def ci_first_boards():
    """Return the first board (alphabetical) of each arm-gcc CI family."""
    matrix_py = os.path.join(TINYUSB_ROOT, '.github', 'workflows', 'ci_set_matrix.py')
    if not os.path.isfile(matrix_py):
        return []
    ret = run([sys.executable, matrix_py])
    if ret.returncode != 0:
        return []
    try:
        data = json.loads(ret.stdout)
    except json.JSONDecodeError:
        return []
    families = data.get('arm-gcc', [])
    boards = []
    bsp_root = os.path.join(TINYUSB_ROOT, 'hw', 'bsp')
    for family in families:
        family_boards = sorted(
            d for d in os.listdir(os.path.join(bsp_root, family, 'boards'))
            if os.path.isdir(os.path.join(bsp_root, family, 'boards', d))
        ) if os.path.isdir(os.path.join(bsp_root, family, 'boards')) else []
        if family_boards:
            boards.append(family_boards[0])
    return boards


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


def generate_metrics(build_dir, out_basename, filters, example=None):
    """Run metrics.py combine on .map.json files. Returns metrics json path or None.

    `filters` is a list of substrings; metrics.py keeps a compile unit if its path
    contains any of them.
    """
    if example:
        patterns = glob.glob(f'{build_dir}/{example}/*.map.json')
    else:
        patterns = glob.glob(f'{build_dir}/**/*.map.json', recursive=True)
    if not patterns:
        print(f'  Error: no .map.json files in {build_dir}' + (f' for {example}' if example else ''))
        return None

    metrics_py = os.path.join(TINYUSB_ROOT, 'tools', 'metrics.py')
    cmd = [sys.executable, metrics_py, 'combine']
    for f in filters:
        cmd += ['-f', f]
    cmd += ['-j', '-q', '-o', out_basename, *patterns]
    ret = run(cmd)
    if ret.returncode != 0:
        print(f'  Error: {ret.stderr}')
        return None
    return f'{out_basename}.json'


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
                        help='Use bloaty for detailed section/symbol diff (requires -e)')
    parser.add_argument('--ci', action='store_true',
                        help='Add the first board of every arm-gcc CI family. Implies --combined.')
    parser.add_argument('--combined', action='store_true',
                        help='Aggregate map.json files across all boards into one comparison '
                             '(in cmake-metrics/_combined/), instead of (or in addition to) per-board.')
    parser.add_argument('-v', '--verbose', action='store_true',
                        help='Print build commands')
    args = parser.parse_args()
    verbose = args.verbose

    if args.bloaty and not args.example:
        parser.error('--bloaty requires -e/--example')

    if args.ci:
        args.combined = True
        ci_boards = ci_first_boards()
        if not ci_boards:
            parser.error('--ci: failed to derive boards from .github/workflows/ci_set_matrix.py')
        # Append, dedup, preserve order
        seen = set(args.board)
        for b in ci_boards:
            if b not in seen:
                args.board.append(b)
                seen.add(b)

    if not args.board:
        parser.error('at least one -b BOARD is required (or pass --ci)')

    metrics_py = os.path.join(TINYUSB_ROOT, 'tools', 'metrics.py')
    worktree_dir = os.path.join(METRICS_DIR, '_worktree')

    # Per-side filters: when no override is given, each build uses its own
    # absolute <checkout>/src/ path so we only match TinyUSB stack code from that
    # checkout (and never vendored-dep `src/` like pico-sdk/src/...).
    if args.filter:
        base_filters = cur_filters = list(args.filter)
    else:
        base_filters = [tinyusb_src_filter(worktree_dir)]
        cur_filters = [tinyusb_src_filter(TINYUSB_ROOT)]

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

    # Symlink dependency dirs (lib/*, hw/mcu/*/*, tools/*) so the worktree builds.
    symlink_deps(TINYUSB_ROOT, worktree_dir)

    try:
        examples = args.example or [None]
        # For --combined: track every (base_build, cur_build) pair so we can aggregate at the end.
        built_pairs = []

        for board in args.board:
            print(f'\n=== {board} ===')
            board_dir = os.path.join(METRICS_DIR, board)
            base_build = os.path.join(board_dir, 'base')
            cur_build = os.path.join(board_dir, 'build')

            # Build only the requested examples (or all if -e not given). Single-example
            # mode used to build everything and filter at metric time — that was wasted work.
            board_failed = False
            for example in examples:
                build_label = f' --target {os.path.basename(example)}' if example else ''
                print(f'[2/5] Building {args.base_branch} for {board}{build_label}...')
                if not build_board(worktree_dir, base_build, board, example):
                    board_failed = True
                    break
                print(f'[3/5] Building current for {board}{build_label}...')
                if not build_board(TINYUSB_ROOT, cur_build, board, example):
                    board_failed = True
                    break
            if board_failed:
                continue

            built_pairs.append((board, base_build, cur_build))

            for example in examples:
                suffix = f'_{example.replace("/", "_")}' if example else ''
                label = f' ({example})' if example else ''

                # Step 4: Generate metrics
                print(f'[4/5] Generating metrics for {board}{label}...')
                base_json = generate_metrics(base_build, os.path.join(board_dir, f'base_metrics{suffix}'),
                                             base_filters, example)
                cur_json = generate_metrics(cur_build, os.path.join(board_dir, f'build_metrics{suffix}'),
                                            cur_filters, example)
                if not base_json or not cur_json:
                    continue

                # Step 5: Compare
                out_base = os.path.join(board_dir, f'metrics_compare{suffix}')
                print(f'[5/5] Comparing {board}{label}...')
                ret = run([sys.executable, metrics_py, 'compare', '-m', '-o', out_base, base_json, cur_json])
                print(ret.stdout)

                # Optional: bloaty diff
                if args.bloaty and example:
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
        # Aggregates the per-board metrics JSONs (not raw map.json globs) so the argv
        # stays small even with --ci spanning many boards.
        if args.combined and built_pairs:
            combined_dir = os.path.join(METRICS_DIR, '_combined')
            os.makedirs(combined_dir, exist_ok=True)

            # Use the no-suffix per-board JSONs (whole-board metrics). Combined mode
            # is meant for board-level sweeps; -e/--example combinations skip combined.
            base_jsons, cur_jsons = [], []
            for board, _, _ in built_pairs:
                bj = os.path.join(METRICS_DIR, board, 'base_metrics.json')
                cj = os.path.join(METRICS_DIR, board, 'build_metrics.json')
                if os.path.isfile(bj) and os.path.isfile(cj):
                    base_jsons.append(bj)
                    cur_jsons.append(cj)

            if not base_jsons or not cur_jsons:
                print('  combined: no per-board metrics found (did you pass -e? skip --combined with -e)')
            else:
                print(f'\n=== combined ({len(base_jsons)} boards) ===')
                base_out = os.path.join(combined_dir, 'base_metrics')
                cur_out = os.path.join(combined_dir, 'build_metrics')

                # Per-board JSONs are already filtered to TinyUSB-only files; combine
                # without re-filtering so we don't accidentally drop entries.
                def _combine(out_basename, inputs):
                    cmd = [sys.executable, metrics_py, 'combine',
                           '-j', '-q', '-o', out_basename, *inputs]
                    return run(cmd)

                ret = _combine(base_out, base_jsons)
                if ret.returncode != 0:
                    print(f'  combined base error: {ret.stderr}')
                else:
                    ret = _combine(cur_out, cur_jsons)
                    if ret.returncode != 0:
                        print(f'  combined current error: {ret.stderr}')
                    else:
                        out_combined = os.path.join(combined_dir, 'metrics_compare')
                        ret = run([sys.executable, metrics_py, 'compare', '-m',
                                   '-o', out_combined, f'{base_out}.json', f'{cur_out}.json'])
                        print(ret.stdout)
                        print(f'  combined report: {out_combined}.md')
    finally:
        print(f'\nCleaning up worktree...')
        run(['git', '-C', TINYUSB_ROOT, 'worktree', 'remove', '--force', worktree_dir])


if __name__ == '__main__':
    main()
