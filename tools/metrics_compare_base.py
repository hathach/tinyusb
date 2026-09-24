#!/usr/bin/env python3
"""Build base branch (master) and current tree, then compare code size metrics.

Creates cmake-metrics/<board>/{base,build} directories for each board.
With --combined, also writes cmake-metrics/_combined/metrics_compare.md over every
board's elf pairs. The membrowse engine pairs base and current elfs by (board, elf
path) and reports per-pair deltas.

Usage:
  python tools/metrics_compare_base.py -b raspberry_pi_pico
  python tools/metrics_compare_base.py -b raspberry_pi_pico -b raspberry_pi_pico2
  python tools/metrics_compare_base.py -b raspberry_pi_pico -f portable/raspberrypi
  python tools/metrics_compare_base.py -b raspberry_pi_pico -e device/cdc_msc
  python tools/metrics_compare_base.py -b raspberry_pi_pico -e device/cdc_msc --bloaty
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

import membrowse_compare

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


def build_board(src_dir, build_dir, board, example=None, linkermap=False):
    """Configure and build examples for a board. Returns True on success.

    When `example` is given, only that target is built (`cmake --build --target NAME`),
    keeping single-example workflows fast.

    When `linkermap` is set, also build the linkermap target (`<ex>-linkermap`, or
    the `examples-linkermap` aggregate when no example is given) so map.json files
    exist for the linkermap engine. A base tree older than that target still writes
    map.json from a POST_BUILD hook, so a failed target is fatal only without map.json
    (main() empties build_dir first, so any map.json found is this build's).
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

    if linkermap:
        target = f'{os.path.basename(example)}-linkermap' if example else 'examples-linkermap'
        ret = run(['cmake', '--build', build_dir, '--target', target], timeout=600)
        # escape the dir, not the wildcards: a checkout path is a path, not a pattern
        root = glob.escape(build_dir)
        map_pattern = f'{root}/{example}/*.map.json' if example \
            else f'{root}/**/*.map.json'
        if ret.returncode != 0 and not glob.glob(map_pattern, recursive=True):
            print(f'  Error: linkermap target failed for {board} - '
                  f'run `python3 tools/get_deps.py` to fetch tools/linkermap')
            return False
    return True


def generate_metrics(build_dir, out_basename, filters, example=None):
    """Run metrics.py combine on .map.json files. Returns metrics json path or None.

    `filters` is a list of substrings; metrics.py keeps a compile unit if its path
    contains any of them.
    """
    # escape the dir, not the wildcards: a checkout path is a path, not a pattern
    root = glob.escape(build_dir)
    if example:
        patterns = glob.glob(f'{root}/{example}/*.map.json')
    else:
        patterns = glob.glob(f'{root}/**/*.map.json', recursive=True)
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


def generate_membrowse_sizes(build_dir, filters, example=None):
    """Return (sizes, errors) for the scope's elfs.

    `sizes` maps each elf path relative to `build_dir` to its
    membrowse_compare.elf_sizes(), or to None when its report failed; `errors`
    lists (relative elf path, message), the path None when no elf can be
    reported at all.
    """
    # escape the dir, not the wildcards: a checkout path is a path, not a pattern
    root = glob.escape(build_dir)
    pattern = f'{root}/{example}/*.elf' if example \
        else f'{root}/**/*.elf'
    elfs = sorted(glob.glob(pattern, recursive=True))
    if not elfs:
        print(f'  Error: no .elf files in {build_dir}')
        return {}, [(None, f'no .elf files in {build_dir}')]

    def report(elf):
        try:
            return membrowse_compare.elf_sizes(
                membrowse_compare.report_for_elf(elf, elf + '.map'), filters), None
        except RuntimeError as e:
            return None, str(e)
        except json.JSONDecodeError as e:
            return None, f'malformed membrowse report for {elf}: {e}'

    # report errors are caught here, not as tracebacks after both builds already ran
    try:
        with concurrent.futures.ThreadPoolExecutor() as pool:
            results = list(pool.map(report, elfs))
    except FileNotFoundError:
        print('  Error: `membrowse` CLI not found - install it with '
              '`pip install membrowse`, or pass --engine linkermap to use '
              'the legacy map.json path instead')
        return {}, [(None, '`membrowse` CLI not found')]
    sizes, errors = {}, []
    for elf, (elf_sizes, error) in zip(elfs, results):
        rel = os.path.relpath(elf, build_dir)
        sizes[rel] = elf_sizes
        if error:
            print(f'  Error: {error}')
            errors.append((rel, error))
    return sizes, errors


def write_report(path, md):
    """Write a membrowse report; stdout gets it without the per-pair details."""
    with open(path, 'w') as f:
        f.write(md)
    print(md.split('\n<details>')[0].rstrip())
    print(f'  report: {path}')


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
    parser.add_argument('--engine', choices=['membrowse', 'linkermap'],
                        default='membrowse',
                        help='Size-diff engine (default: membrowse local reports; '
                             'linkermap is the legacy map.json path)')
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

    symlink_deps(TINYUSB_ROOT, worktree_dir)

    failed = False
    try:
        examples = args.example or [None]
        # linkermap --combined: boards whose both sides built
        built_pairs = []
        # membrowse --combined: every board's elf sizes and failures, paired at the end
        combined_sides = {'base': {}, 'current': {}}
        combined_failures = []

        combined_dir = os.path.join(METRICS_DIR, '_combined')
        # unconditional, like the per-board cleanup below: a combined step that
        # fails, or never runs (no board built, or -e leaving no whole-board
        # JSONs), must not leave a previous run's report for a reader to take
        # for this one's — cmake-metrics/ is gitignored and persists.
        if args.combined:
            shutil.rmtree(combined_dir, ignore_errors=True)

        for board in args.board:
            print(f'\n=== {board} ===')
            board_dir = os.path.join(METRICS_DIR, board)
            for example in examples:
                suffix = f'_{example.replace("/", "_")}' if example else ''
                # drop the metrics JSONs too: the combined step reads them back by
                # name, so a previous run's file would stand in for one this run
                # failed to generate (worst case: fresh base vs stale current).
                for stale in (f'metrics_compare{suffix}.md',
                              f'base_metrics{suffix}.json',
                              f'build_metrics{suffix}.json'):
                    stale_path = os.path.join(board_dir, stale)
                    if os.path.isfile(stale_path):
                        os.remove(stale_path)
            base_build = os.path.join(board_dir, 'base')
            cur_build = os.path.join(board_dir, 'build')
            shutil.rmtree(base_build, ignore_errors=True)
            shutil.rmtree(cur_build, ignore_errors=True)

            # Build only the requested examples (or all if -e not given). Single-example
            # mode used to build everything and filter at metric time — that was wasted work.
            board_failed = None  # the side whose build failed
            want_linkermap = args.engine == 'linkermap'
            for example in examples:
                build_label = f' --target {os.path.basename(example)}' if example else ''
                print(f'[2/5] Building {args.base_branch} for {board}{build_label}...')
                if not build_board(worktree_dir, base_build, board, example, linkermap=want_linkermap):
                    board_failed = 'base'
                    break
                print(f'[3/5] Building current for {board}{build_label}...')
                if not build_board(TINYUSB_ROOT, cur_build, board, example, linkermap=want_linkermap):
                    board_failed = 'current'
                    break
            if board_failed:
                failed = True
                if args.engine != 'membrowse':
                    continue
                # still write each scope's report, with the build failure in it
                build_failure = ((board, None), board_failed, 'build', f'build failed{build_label}, see log')
                combined_failures.append(build_failure)
            else:
                built_pairs.append((board, base_build, cur_build))

            for example in examples:
                suffix = f'_{example.replace("/", "_")}' if example else ''
                label = f' ({example})' if example else ''

                # Step 4/5: Generate metrics and compare
                out_base = os.path.join(board_dir, f'metrics_compare{suffix}')
                if args.engine == 'membrowse':
                    sides = {'base': {}, 'current': {}}
                    failures = [build_failure] if board_failed else []
                    if not board_failed:
                        print(f'[4/5] Generating membrowse reports for {board}{label}...')
                        for side, build, filters in (('base', base_build, base_filters),
                                                     ('current', cur_build, cur_filters)):
                            sizes, errors = generate_membrowse_sizes(build, filters, example)
                            sides[side] = {(board, rel): v for rel, v in sizes.items()}
                            failures += [((board, rel), side, 'report', msg) for rel, msg in errors]

                    print(f'[5/5] Comparing {board}{label}...')
                    md, failures, ok = membrowse_compare.compare_sides(
                        sides['base'], sides['current'], failures, [board], scope=(board, None))
                    failed |= not ok
                    if not board_failed:  # a build failure is recorded once, above
                        for side, sizes in sides.items():
                            combined_sides[side].update(sizes)
                        combined_failures += failures
                    write_report(f'{out_base}.md', md)
                else:
                    print(f'[4/5] Generating metrics for {board}{label}...')
                    base_json = generate_metrics(base_build, os.path.join(board_dir, f'base_metrics{suffix}'),
                                                 base_filters, example)
                    cur_json = generate_metrics(cur_build, os.path.join(board_dir, f'build_metrics{suffix}'),
                                                cur_filters, example)
                    if not base_json or not cur_json:
                        failed = True
                        continue

                    print(f'[5/5] Comparing {board}{label}...')
                    ret = run([sys.executable, metrics_py, 'compare', '-m', '-o', out_base, base_json, cur_json])
                    print(ret.stdout)

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
        if args.combined and args.engine == 'membrowse':
            os.makedirs(combined_dir, exist_ok=True)
            print(f'\n=== combined ({len(args.board)} boards) ===')
            # every scope was filter-checked above, and its failures carried over
            md, _failures, ok = membrowse_compare.compare_sides(
                combined_sides['base'], combined_sides['current'], combined_failures, args.board)
            failed |= not ok
            write_report(os.path.join(combined_dir, 'metrics_compare.md'), md)
        elif args.combined and built_pairs:
            # Aggregates the per-board metrics JSONs (not raw map.json globs) so the argv
            # stays small even with --ci spanning many boards.
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
                    failed = True
                else:
                    ret = _combine(cur_out, cur_jsons)
                    if ret.returncode != 0:
                        print(f'  combined current error: {ret.stderr}')
                        failed = True
                    else:
                        out_combined = os.path.join(combined_dir, 'metrics_compare')
                        ret = run([sys.executable, metrics_py, 'compare', '-m',
                                   '-o', out_combined, f'{base_out}.json', f'{cur_out}.json'])
                        print(ret.stdout)
                        if ret.returncode != 0:
                            print(f'  combined compare error: {ret.stderr}')
                            failed = True
                        else:
                            print(f'  combined report: {out_combined}.md')
    finally:
        print(f'\nCleaning up worktree...')
        run(['git', '-C', TINYUSB_ROOT, 'worktree', 'remove', '--force', worktree_dir])
    return 1 if failed else 0


if __name__ == '__main__':
    sys.exit(main())
