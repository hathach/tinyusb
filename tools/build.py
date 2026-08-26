#!/usr/bin/env python3
import argparse
import json
import random
import os
import re
import sys
import time
import subprocess
import shlex
from pathlib import Path
from multiprocessing import Pool

import build_utils

STATUS_OK = "\033[32mOK\033[0m"
STATUS_FAILED = "\033[31mFailed\033[0m"
STATUS_SKIPPED = "\033[33mSkipped\033[0m"

RET_OK = 0
RET_FAILED = 1
RET_SKIPPED = 2

build_format = '| {:30} | {:40} | {:16} | {:5} |'
build_separator = '-' * 95
build_status = [STATUS_OK, STATUS_FAILED, STATUS_SKIPPED]

verbose = False
parallel_jobs = os.cpu_count()


# -----------------------------
# Helper
# -----------------------------
def run_cmd(cmd):
    if isinstance(cmd, str):
        raise TypeError("run_cmd expects a list/tuple of args, not a string")
    args = cmd
    cmd_display = " ".join(args)
    r = subprocess.run(args, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    title = f'Command Error: {cmd_display}'
    if r.returncode != 0:
        # print build output if failed
        if os.getenv('GITHUB_ACTIONS'):
            print(f"::group::{title}")
            print(r.stdout.decode("utf-8"))
            print(f"::endgroup::")
        else:
            print(title)
            print(r.stdout.decode("utf-8"))
    elif verbose:
        print(cmd_display)
        print(r.stdout.decode("utf-8"))
    return r


def find_family(board):
    bsp_dir = Path("hw/bsp")
    for family_dir in bsp_dir.iterdir():
        if family_dir.is_dir():
            board_dir = family_dir / 'boards' / board
            if board_dir.exists():
                return family_dir.name
    return None


def get_examples(family):
    all_examples = []
    for d in os.scandir("examples"):
        if d.is_dir() and 'cmake' not in d.name and 'build_system' not in d.name:
            for entry in os.scandir(d.path):
                if entry.is_dir() and 'cmake' not in entry.name:
                    if family != 'espressif' or 'freertos' in entry.name:
                        all_examples.append(d.name + '/' + entry.name)

    if family == 'espressif':
        all_examples.append('device/board_test')
        all_examples.append('device/usbtest')
        all_examples.append('device/video_capture')
        all_examples.append('host/device_info')
    all_examples.sort()
    return all_examples


def resolve_example_target_groups(build_targets, examples, board, extra_defines=()):
    """Map generic targets onto per-example targets for a filtered build (-e), as ONE
    GROUP PER REQUESTED TARGET: 'all' -> the example executables, anything else (e.g.
    tinyusb_metrics) passes through as its own single-entry group.

    Grouped rather than flattened because each group becomes one `cmake --build
    --target a b c` invocation: the examples of a group build in parallel (flattening
    them into one target per invocation serialises the whole leg - measured +39% at
    -j4 and +220% at -j32 on stm32f407disco), while separate groups stay ordered, so a
    target that must run after the examples still does.

    extra_defines are this build's -D tokens: MAX3421_HOST=1 there decides
    only.txt for the max3421 examples (see build_utils.skip_example).
    Returns None when no requested example is buildable on this board."""
    buildable = [e for e in examples
                 if not build_utils.skip_example(e, board, extra_defines)]
    if not buildable:
        return None
    names = list(dict.fromkeys(e.split('/', 1)[1] for e in buildable))
    return [list(names) if t == 'all' else [t] for t in build_targets]


_TARGET_HELP_RE = re.compile(r'^([A-Za-z0-9_.+-]+):')
# role/name, the only shape resolve_example_target_groups and the CMake target names accept
EXAMPLE_RE = re.compile(r'[A-Za-z0-9_]+/[A-Za-z0-9_]+')


def parse_target_help(text):
    """Bare target names out of `cmake --build <dir> --target help`; the Ninja
    generator prints one '<name>: phony' line per target. Names containing '/' are
    per-directory utility targets (device/edit_cache) or absolute CMakeFiles paths,
    never an example target."""
    return {m.group(1) for m in map(_TARGET_HELP_RE.match, text.splitlines()) if m}


def cmake_registered_targets(build_dir):
    """The targets CMake actually created in build_dir, or None when that cannot be
    read. Ground truth: skip.txt/only.txt only mirrors family_filter, so an example
    the role CMakeLists never lists (or a stale -e name) still looks buildable to it
    and `cmake --build --target <it>` hard-fails. None keeps the mirror's answer."""
    r = subprocess.run(['cmake', '--build', build_dir, '--target', 'help'],
                       stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    if r.returncode != 0:
        return None
    return parse_target_help(r.stdout.decode('utf-8', 'replace')) or None


def print_build_result(board, build_target, status, duration):
    if isinstance(duration, (int, float)):
        duration = "{:.2f}s".format(duration)
    print(build_format.format(board, build_target, build_status[status], duration))

# -----------------------------
# CMake
# -----------------------------
def cmake_board(board, build_args, build_name, build_cflags, build_targets, examples=None, defines=()):
    ret = [0, 0, 0]
    start_time = time.monotonic()

    build_dir = f'cmake-build/cmake-build-{build_name or board}'
    build_flags = []
    if build_cflags:
        build_flags.append('-DCFLAGS_CLI=' + ' '.join(build_cflags))

    family = find_family(board)
    if family == 'espressif':
        # for espressif, we have to build example individually
        all_examples = get_examples(family)
        if examples is not None:
            all_examples = [e for e in all_examples if e in examples]
            if not all_examples:
                print_build_result(board, 'examples (PR filter)', 2, '-')
                return [0, 0, 1]
        for example in all_examples:
            example_build_dir = f'{build_dir}/{example}'
            if build_utils.skip_example(example, board, defines):
                ret[2] += 1
            elif build_targets == ['all']:
                rcmd = run_cmd([
                    'idf.py', '-C', f'examples/{example}', '-B', example_build_dir, '-GNinja',
                    f'-DBOARD={board}', *build_flags, 'build'
                ])
                ret[0 if rcmd.returncode == 0 else 1] += 1
            elif not os.path.isdir(example_build_dir) and build_targets == ['examples-membrowse-upload']:
                # No build dir means 'all' never ran here (no-code-change CI run, PR
                # filter, or family skip). Every other family still gets an --identical
                # upload in this situation because its 'cmake' configure step (cheap,
                # no compiler invoked) runs regardless - espressif has no such cheap
                # step, only a real `idf.py build`, which this must NOT trigger. Route
                # straight through membrowse_report.py's --identical path instead: it
                # needs neither the ELF nor a configured ninja build dir once the ELF
                # is missing (see its own fix), so pass one that does not exist.
                name = example.split('/', 1)[1]
                rcmd = run_cmd([
                    sys.executable, os.path.join(os.path.dirname(__file__), 'membrowse_report.py'),
                    '--build-dir', example_build_dir, '--ninja', 'ninja', '--target', name,
                    '--elf', f'{example_build_dir}/{name}.elf',
                    '--target-name', f'{board}/{name}', '--upload',
                ])
                ret[0 if rcmd.returncode == 0 else 1] += 1
            elif not os.path.isdir(example_build_dir):
                # a non-'all' target (e.g. examples-membrowse-upload) runs against an
                # already-configured IDF build dir; without one - PR filter, family
                # skip, or a standalone --target invocation never ran 'all' here - there
                # is nothing to run the target against, so skip rather than fail
                print_build_result(board, f'{example} ({build_targets[0]}, no build dir)', 2, '-')
                ret[2] += 1
            else:
                rcmd = None
                for target in build_targets:
                    rcmd = run_cmd(['cmake', '--build', example_build_dir, '--target', target])
                    if rcmd.returncode != 0:
                        break
                ret[0 if rcmd.returncode == 0 else 1] += 1
    else:
        # the skip.txt/only.txt prefilter reads no configure output: answer it first,
        # so a selection this board builds nothing of costs no cmake run at all
        if examples is not None:
            examples = [e for e in examples
                        if not build_utils.skip_example(e, board, defines)]
            if not examples:
                print_build_result(board, 'examples (PR filter)', 2, '-')
                return [0, 0, 1]
        rcmd = run_cmd(['cmake', 'examples', '-B', build_dir, '-GNinja',
                        f'-DBOARD={board}', '-DCMAKE_BUILD_TYPE=MinSizeRel',
                        *build_args, *build_flags])
        if rcmd.returncode == 0:
            target_groups = [[t] for t in build_targets]
            if examples is not None:
                registered = cmake_registered_targets(build_dir)
                if registered is not None:
                    kept = [e for e in examples if e.split('/', 1)[1] in registered]
                    for e in examples:
                        if e not in kept:
                            print_build_result(board, f'{e} (no such target)', 2, '-')
                    examples = kept
                if not examples:
                    print_build_result(board, 'examples (no such target)', 2, '-')
                    return [0, 0, 1]
                target_groups = resolve_example_target_groups(build_targets, examples, board, defines)
                if registered is None:
                    # ground truth unavailable, so nothing checked these names against
                    # what CMake created. ninja validates a whole invocation up front:
                    # one unknown name in the batch builds NOTHING, where a target each
                    # builds everything up to it. Give up the parallelism, not the work.
                    target_groups = [[t] for g in target_groups for t in g]
            cmd = ["cmake", "--build", build_dir, '--parallel', str(parallel_jobs)]
            for group in target_groups:
                rcmd = run_cmd(cmd + ['--target'] + group)
                if rcmd.returncode != 0:
                    break
        ret[0 if rcmd.returncode == 0 else 1] += 1

    print_build_result(board, ','.join(build_targets), 0 if ret[1] == 0 else 1, time.monotonic() - start_time)
    return ret


# -----------------------------
# Make
# -----------------------------
def make_one_example(example, board, make_option, build_targets, defines=()):
    # Check if board is skipped. Make semantics: family.mk decides, not the
    # family.cmake MCU list (see build_utils.skip_example).
    if build_utils.skip_example(example, board, defines, build_system='make'):
        print_build_result(board, example, 2, '-')
        r = 2
    else:
        start_time = time.monotonic()
        make_cmd = ["make", "-C", f"examples/{example}", f"BOARD={board}", '-j', str(parallel_jobs)]
        if make_option:
            make_cmd += shlex.split(make_option)
        r = 0
        for target in build_targets:
            build_result = run_cmd(make_cmd + [target])
            if build_result.returncode != 0:
                r = 1
                break
        print_build_result(board, example, r, time.monotonic() - start_time)

    ret = [0, 0, 0]
    ret[r] = 1
    return ret


def make_board(board, build_args, build_targets, examples=None, defines=()):
    print(build_separator)
    family = find_family(board);
    all_examples = get_examples(family)
    if examples is not None:
        all_examples = [e for e in all_examples if e in examples]
        if not all_examples:
            print_build_result(board, 'examples (PR filter)', 2, '-')
            return [0, 0, 1]
    start_time = time.monotonic()
    ret = [0, 0, 0]
    if family == 'espressif' or family == 'rp2040':
        # espressif and rp2040 do not support make, use cmake instead
        final_status = 2
    else:
        with Pool(processes=os.cpu_count()) as pool:
            pool_args = list((map(lambda e, b=board, o=f"{build_args}", t=build_targets, d=defines: [e, b, o, t, d], all_examples)))
            r = pool.starmap(make_one_example, pool_args)
            # sum all element of same index (column sum)
            ret = list(map(sum, list(zip(*r))))
        final_status = 0 if ret[1] == 0 else 1
    print_build_result(board, 'all', final_status, time.monotonic() - start_time)
    return ret


# -----------------------------
# Build Family
# -----------------------------
def build_boards_list(boards, build_defines, build_system, build_name, build_cflags, build_targets, examples=None):
    ret = [0, 0, 0]
    # the -D tokens are part of the skip.txt/only.txt answer (MAX3421_HOST=1), so
    # the -e filter has to see them too; sorted+tuple keeps skip_example cacheable
    defines = tuple(sorted(build_defines))
    for b in boards:
        r = [0, 0, 0]
        if build_system == 'cmake':
            build_args = [f'-D{d}' for d in build_defines]
            r = cmake_board(b, build_args, build_name, build_cflags, build_targets, examples, defines)
        elif build_system == 'make':
            build_args = ' '.join(f'{d}' for d in build_defines)
            r = make_board(b, build_args, build_targets, examples, defines)
        ret[0] += r[0]
        ret[1] += r[1]
        ret[2] += r[2]
    return ret


def get_family_boards(family, one_random, one_first, examples=None, build_system='cmake',
                      extra_defines=()):
    """Get list of boards for a family.

    Args:
        family: Family name
        one_random: If True, return only one random board
        one_first: If True, return only the first board (alphabetical)
        examples: PR example filter (-e). The one-board pick then prefers a board that
            can build at least one of them: the family is in the matrix BECAUSE some
            board of it builds these examples (ci_select._prune_buildable asks about
            every board, since CircleCI builds every board), but GHA builds one. Without
            this, lpc54 selected for host/msc_file_explorer picks lpcxpresso54114 -
            which every one of those examples skips - and the leg runs to green having
            compiled nothing and uploaded no metrics.
        build_system: which skip answer to ask for; the two differ (build_utils)
        extra_defines: this build's -D tokens, so a board whose only.txt match comes
            from -DMAX3421_HOST=1 is not judged unbuildable here and buildable in
            cmake_board

    Returns:
        List of board names
    """
    all_boards = []
    for entry in os.scandir(f"hw/bsp/{family}/boards"):
        if entry.is_dir():
            all_boards.append(entry.name)
    if not all_boards:
        print(f"No boards found for family '{family}'")
        return []
    all_boards.sort()

    # If only-one flags are set, pick a board buildable under the filter (else first/random)
    if one_first or one_random:
        def buildable(board):
            # no filter, or nothing in the filter is buildable anywhere: keep today's
            # answer rather than inventing a different board
            return examples is None or any(
                not build_utils.skip_example(e, board, extra_defines, build_system)
                for e in examples)

        candidates = [b for b in all_boards if buildable(b)] or all_boards
        if one_first:
            return [candidates[0]]
        if one_random:
            return [random.choice(candidates)]

    return all_boards


def resolve_ci_boards(boards_path, family, boards_only, examples=None,
                      build_system='cmake', extra_defines=()):
    """Resolve a family to its CI boards (.github/ci-boards.json) - the only
    board-curation mechanism now that ci_skip_boards/ci_preferred_boards are gone.

    An explicit CI board is not automatically buildable: when `examples` narrows the
    request (-e), a CI board is kept only if it can build at least one of them
    (build_utils.skip_example - the same skip.txt/only.txt answer
    get_family_boards()'s one-first pick filters by). Otherwise a scoped PR could
    pick a CI board that builds NONE of the selected examples, so the leg
    compiles nothing, uploads nothing, and reports green (concrete case: stm32f4's
    CI board is stm32f407disco, and examples/device/cdc_dual_ports/skip.txt skips
    that board, so a PR touching only that example needs a different stm32f4 board).

    A family with no CI boards - or whose CI boards all fail the filter, which is
    the same situation as none for the purpose at hand - falls back to the
    one-first pick, or to nothing under boards_only (the upload step must not touch
    build dirs of boards that were built only as compile smoke-checks; a CI board
    that cannot build the filter is exactly that: a smoke-check substitute, not the
    tracked board)."""
    with open(boards_path) as f:
        data = json.load(f)
    ci_boards = [t['board'] for t in data['boards'] if t['family'] == family]
    if examples is not None:
        ci_boards = [b for b in ci_boards if any(
            not build_utils.skip_example(e, b, extra_defines, build_system)
            for e in examples)]
    if ci_boards:
        return ci_boards
    if boards_only:
        return []
    return get_family_boards(family, one_random=False, one_first=True,
                             examples=examples, build_system=build_system,
                             extra_defines=extra_defines)


# -----------------------------
# Main
# -----------------------------
def main():
    global verbose
    global parallel_jobs

    parser = argparse.ArgumentParser()
    parser.add_argument('families', nargs='*', default=[], help='Families to build')
    parser.add_argument('-b', '--board', action='append', default=[], help='Boards to build')
    parser.add_argument('-t', '--toolchain', default='gcc', help='Toolchain to use, default is gcc')
    parser.add_argument('-s', '--build-system', default='cmake', help='Build system to use, default is cmake')
    parser.add_argument('-D', '--define-symbol', action='append', default=[], help='Define to pass to build system')
    parser.add_argument('--build-name', default=None,
                        help='Override build dir name (cmake-build-<name>); default is the board name. Used for HIL variants.')
    parser.add_argument('--cflag', action='append', default=[],
                        help='Raw compiler flag appended to CFLAGS_CLI, e.g. --cflag=-DCFG_TUD_DWC2_DMA_ENABLE=1 (repeatable)')
    parser.add_argument('--one-random', action='store_true', default=False,
                        help='Build only one random board of each specified family')
    parser.add_argument('--one-first', action='store_true', default=False,
                        help='Build only the first board (alphabetical) of each specified family')
    parser.add_argument('--ci-boards', default=None, metavar='JSON',
                        help='Path to ci-boards.json: build the CI boards '
                             'of each family (fallback: first board alphabetically)')
    parser.add_argument('--ci-boards-only', action='store_true', default=False,
                        help='With --ci-boards: skip families that have no CI board')
    parser.add_argument('-j', '--jobs', type=int, default=os.cpu_count(), help='Number of jobs to run in parallel')
    parser.add_argument('-T', '--target', action='append', default=[],
                        help='Build target to use, may be specified multiple times (default: all)')
    parser.add_argument('-e', '--example', action='append', default=[],
                        help='Only build these examples (role/name, repeatable). Default: all examples')
    parser.add_argument('-v', '--verbose', action='store_true', help='Verbose output')
    args = parser.parse_args()

    families = args.families
    boards = args.board
    toolchain = args.toolchain
    build_system = args.build_system
    build_defines = args.define_symbol
    build_name = args.build_name
    build_cflags = args.cflag
    one_random = args.one_random
    one_first = args.one_first
    ci_boards_path = args.ci_boards
    ci_boards_only = args.ci_boards_only
    if ci_boards_only and not ci_boards_path:
        parser.error('--ci-boards-only requires --ci-boards')
    if ci_boards_path and (one_first or one_random):
        parser.error('--ci-boards replaces --one-first/--one-random')
    build_targets = args.target if args.target else ['all']
    examples = args.example or None
    verbose = args.verbose
    parallel_jobs = args.jobs

    for e in args.example:
        if not EXAMPLE_RE.fullmatch(e):
            parser.error(f"-e/--example takes 'role/name' (e.g. device/cdc_msc), got '{e}'")
        # a name no example dir answers to would silently build nothing on every board
        # and still exit 0 (every row is a Skipped, and main() returns the FAILED count).
        # The -e lists are generated - from ci_select's example map and from HIL roster
        # test names - so a stale one must be loud, not green
        if not os.path.isdir(os.path.join('examples', e)):
            parser.error(f"-e/--example '{e}': no such example directory examples/{e}")

    build_defines.append(f'TOOLCHAIN={toolchain}')

    if len(families) == 0 and len(boards) == 0:
        print("Please specify families or board to build")
        return 1

    # --build-name renames the single shared build dir, so building more than one
    # board with it would clobber/mix artifacts
    if build_name and (len(families) > 0 or len(boards) != 1):
        print("--build-name requires exactly one board (-b) and no families")
        return 1

    print(build_separator)
    print(build_format.format('Board', 'Target', '\033[39mResult\033[0m', 'Time'))
    total_time = time.monotonic()

    # get all families
    all_families = []
    if 'all' in families:
        for entry in os.scandir("hw/bsp"):
            if entry.is_dir() and entry.name != 'espressif' and os.path.isfile(entry.path + "/family.cmake"):
                all_families.append(entry.name)
    else:
        all_families = list(families)
    all_families.sort()

    # get boards from families and append to boards list
    all_boards = list(boards)
    for f in all_families:
        if ci_boards_path:
            all_boards.extend(resolve_ci_boards(ci_boards_path, f, ci_boards_only, examples,
                                                build_system, tuple(build_defines)))
        else:
            all_boards.extend(get_family_boards(f, one_random, one_first, examples,
                                                build_system, tuple(build_defines)))

    # build all boards
    result = build_boards_list(all_boards, build_defines, build_system, build_name, build_cflags, build_targets,
                               examples)

    total_time = time.monotonic() - total_time
    print(build_separator)
    print(f"Build Summary: {result[0]} {STATUS_OK}, {result[1]} {STATUS_FAILED} and took {total_time:.2f}s")
    print(build_separator)

    return result[1]


if __name__ == '__main__':
    sys.exit(main())
