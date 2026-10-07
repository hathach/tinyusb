#!/usr/bin/env python3
import argparse
import json
import os
import shlex
import subprocess
import sys

# toolchain, url
toolchain_list = [
    "aarch64-gcc",
    "arm-clang",
    "arm-iar",
    "arm-gcc",
    "esp-idf",
    "ft9xx-gcc",
    "msp430-gcc",
    "riscv-gcc",
    "rx-gcc"
]

# family: [cmake toolchains that build it]. An empty list means CI builds the family
# by board name instead of as a matrix leg (espressif: hil-build-esp, in an IDF
# container), so it belongs here - "not in family_list" means CI never builds it.
family_list = {
    "apm32f0xx": ["arm-gcc"],
    "at32f402_405": ["arm-gcc"],
    "at32f403a_407": ["arm-gcc"],
    "at32f413": ["arm-gcc"],
    "at32f415": ["arm-gcc"],
    "at32f423": ["arm-gcc"],
    "at32f425": ["arm-gcc"],
    "at32f435_437": ["arm-gcc"],
    "at32f45x": ["arm-gcc"],
    "broadcom_32bit": ["arm-gcc"],
    "broadcom_64bit": ["aarch64-gcc"],
    "ch32f20x": ["arm-gcc"],
    "ch32v10x": ["riscv-gcc"],
    "ch32v20x": ["riscv-gcc"],
    "ch32v30x": ["riscv-gcc"],
    "ch32x035": ["riscv-gcc"],
    "ch583": ["riscv-gcc"],
    "da1469x": ["arm-gcc"],
    "efm32": ["arm-gcc"],
    "f1c100s": ["arm-gcc"],
    "fomu": ["riscv-gcc"],
    "espressif": [],
    "ft9xx": ["ft9xx-gcc"],
    "gd32vf103": ["riscv-gcc"],
    "hpmicro": ["riscv-gcc"],
    "imxrt": ["arm-gcc", "arm-clang"],
    "kinetis_k": ["arm-gcc"],
    "kinetis_k32l": ["arm-gcc"],
    "kinetis_kl": ["arm-gcc"],
    "lpc11": ["arm-gcc", "arm-clang"],
    "lpc13": ["arm-gcc", "arm-clang"],
    "lpc15": ["arm-gcc", "arm-clang"],
    "lpc17": ["arm-gcc", "arm-clang"],
    "lpc18": ["arm-gcc", "arm-clang"],
    "lpc40": ["arm-gcc", "arm-clang"],
    "lpc43": ["arm-gcc", "arm-clang"],
    "lpc51": ["arm-gcc", "arm-clang"],
    "lpc54": ["arm-gcc", "arm-clang"],
    "lpc55": ["arm-gcc", "arm-clang"],
    "maxim": ["arm-gcc"],
    "mcx": ["arm-gcc"],
    "mm32": ["arm-gcc"],
    "msp430": ["msp430-gcc"],
    "msp432e4": ["arm-gcc"],
    "nrf": ["arm-gcc", "arm-clang"],
    "nuc100_120": ["arm-gcc"],
    "nuc121_125": ["arm-gcc"],
    "nuc126": ["arm-gcc"],
    "nuc505": ["arm-gcc"],
    "ra": ["arm-gcc"],
    "rp2040": ["arm-gcc"],
    "rw61x": ["arm-gcc"],
    "rx": ["rx-gcc"],
    "samd11": ["arm-gcc", "arm-clang"],
    "samd2x_l2x": ["arm-gcc", "arm-clang"],
    "samd5x_e5x": ["arm-gcc", "arm-clang"],
    "same7x": ["arm-gcc"],
    "samg": ["arm-gcc", "arm-clang"],
    "stm32c0": ["arm-gcc", "arm-clang", "arm-iar"],
    "stm32c5": ["arm-gcc", "arm-clang", "arm-iar"],
    "stm32f0": ["arm-gcc", "arm-clang", "arm-iar"],
    "stm32f1": ["arm-gcc", "arm-clang", "arm-iar"],
    "stm32f2": ["arm-gcc", "arm-clang", "arm-iar"],
    "stm32f3": ["arm-gcc", "arm-clang", "arm-iar"],
    "stm32f4": ["arm-gcc", "arm-clang", "arm-iar"],
    "stm32f7": ["arm-gcc", "arm-clang", "arm-iar"],
    "stm32g0": ["arm-gcc", "arm-clang", "arm-iar"],
    "stm32g4": ["arm-gcc", "arm-clang", "arm-iar"],
    "stm32h5": ["arm-gcc", "arm-clang", "arm-iar"],
    "stm32h7": ["arm-gcc", "arm-clang", "arm-iar"],
    "stm32h7rs": ["arm-gcc", "arm-clang", "arm-iar"],
    "stm32l0": ["arm-gcc", "arm-clang", "arm-iar"],
    "stm32l4": ["arm-gcc", "arm-clang", "arm-iar"],
    "stm32n6": ["arm-gcc"],
    "stm32u0": ["arm-gcc", "arm-clang", "arm-iar"],
    "stm32u5": ["arm-gcc", "arm-clang", "arm-iar"],
    "stm32wb": ["arm-gcc", "arm-clang", "arm-iar"],
    "stm32wba": ["arm-gcc", "arm-clang", "arm-iar"],
    "tm4c": ["arm-gcc"],
    "xmc4000": ["arm-gcc"],
}


REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
# the toolchains cmake-required builds a changed board with: every one a GitHub runner
# can set up (.github/actions/setup_toolchain), espressif's through its IDF container
REQUIRED_TOOLCHAINS = ['aarch64-gcc', 'arm-gcc', 'esp-idf', 'ft9xx-gcc', 'msp430-gcc', 'riscv-gcc', 'rx-gcc']
# build.yml's `cmake` job matrix (its cmake_toolchains output): the pinned boards built there
CMAKE_JOB_TOOLCHAINS = ['aarch64-gcc', 'arm-gcc', 'ft9xx-gcc', 'msp430-gcc', 'riscv-gcc']


def pinned_boards(repo_root=REPO):
    with open(os.path.join(repo_root, '.github', 'ci-pinned-boards.json')) as f:
        return {e['board'] for e in json.load(f)['boards']}


def board_family(board, repo_root=REPO):
    bsp = os.path.join(repo_root, 'hw', 'bsp')
    return next((fam for fam in sorted(os.listdir(bsp))
                 if os.path.isdir(os.path.join(bsp, fam, 'boards', board))), None)


def pinned_families(repo_root=REPO):
    """Families with a board in .github/ci-pinned-boards.json. Board names are
    unique across hw/bsp/*/boards, so the board alone gives the family."""
    return {f for f in map(lambda b: board_family(b, repo_root), pinned_boards(repo_root)) if f}


def usable(select):
    """The manifest, or None after saying UNSCOPED: the marker build.yml and
    .circleci/config.yml grep for to drop the build extras along with the scoping."""
    if select is None:
        return None
    try:
        # imported here, under the fall-open: a broken selector must still leave the
        # unscoped matrix
        sys.path.insert(0, os.path.join(REPO, 'tools'))
        import change_impact
        return change_impact.check_manifest(select)
    except Exception as e:
        print(f'ci_set_matrix: UNSCOPED - selection unusable ({e}), emitting the full matrix', file=sys.stderr)
        return None


def required_json(select):
    """{toolchain: ['-b <board>', ...]} for the boards the change edits that the pinned
    matrix does not build (D3), from any usable manifest, full or scoped. A board of a
    family no CI toolchain builds is listed under 'unbuildable'."""
    select = usable(select)
    out = {tc: [] for tc in REQUIRED_TOOLCHAINS}
    out['unbuildable'] = []
    if select is None:
        print(json.dumps(out))
        return
    b = select['build']
    built = {f for f, tcs in family_list.items() if set(tcs) & set(CMAKE_JOB_TOOLCHAINS)}
    covered_fams = pinned_families() & built & (built if b['full'] else set(b['families']))
    pinned = pinned_boards()
    for board in b['required_boards']:
        fam = board_family(board)
        if board in pinned and fam in covered_fams:
            continue
        tcs = ['esp-idf'] if fam == 'espressif' else family_list.get(fam, [])
        tc = next((t for t in tcs if t in REQUIRED_TOOLCHAINS), None)
        if tc is None:
            out['unbuildable'].append(board)
            print(f'ci_set_matrix: required board {board} ({fam}) is built by no CI toolchain', file=sys.stderr)
        else:
            out[tc].append(f'-b {board}')
    print(json.dumps(out))


def example_map_json(select):
    """{family: [examples]} for build_util.yml/config2.yml's -e filter: a family that
    builds every example has no entry, and an unusable selection filters nothing."""
    select = usable(select)
    fams = select['build']['families'] if select else {}
    print(json.dumps({f: v['examples'] for f, v in fams.items() if v['examples'] != 'all'}))


def leg_key(arg):
    """(board, upload name) of a `-b <board> [--build-name <name>] ...` leg."""
    words = shlex.split(arg)
    board = words[words.index('-b') + 1]
    return board, words[words.index('--build-name') + 1] if '--build-name' in words else board


def membrowse_json(pinned, hil_esp, hil_esp_all, with_esp):
    """{'all': legs, 'identical': legs} for the membrowse-identical job. Every leg of the
    universe - the pinned boards, plus the tinyusb roster's espressif legs where
    hil-build-esp runs (with_esp) - is uploaded once per commit: by the leg that owns it,
    else --identical. A cmake leg uploads every pinned board of its family (build.py
    --ci-pinned-boards-only), hil-build-esp its scoped espressif legs. 'all' is for a run
    whose gate builds nothing."""
    universe = {(b, b): f'-b {b}' for b in sorted(pinned_boards()) if board_family(b) != 'espressif'}
    if with_esp:
        universe.update((leg_key(a), a) for a in hil_esp_all)
    measured = {leg_key(a) for a in hil_esp} if with_esp else set()
    fams = {f for tc in CMAKE_JOB_TOOLCHAINS for f in pinned.get(tc, [])}
    measured.update((b, b) for b in pinned_boards() if board_family(b) in fams)
    print(json.dumps({'all': list(universe.values()),
                      'identical': [a for k, a in universe.items() if k not in measured]}))


def set_matrix_json(select=None, pinned=False):
    sel_fams = None
    select = usable(select)
    if select is not None and not select['build']['full']:
        # an explicit empty families map is a legitimate nothing-selected
        sel_fams = set(select['build']['families'])
    matrix = {}
    for toolchain in toolchain_list:
        fams = [family for family, tc in family_list.items() if toolchain in tc]
        if sel_fams is not None:
            fams = [f for f in fams if f in sel_fams]
        matrix[toolchain] = fams
    if sel_fams:
        # a family this file does not list builds on no toolchain, so it contributes no
        # leg. hw/bsp holds a few CI has never built (pic32mz, py32f0, ...) - a
        # missing family.cmake or no CI toolchain support, not a gap in this file.
        # espressif is in family_list with no toolchain: hil-build-esp builds its boards
        # BY NAME in an IDF container (an esp-idf leg here would double-build them, and
        # CircleCI would build every espressif board), so falling open to the full matrix
        # for an espressif-only selection would add 74 legs, none of which can compile it.
        unbuilt = sorted(f for f in sel_fams if f not in family_list)
        if unbuilt and not any(matrix.values()):
            # NONE of the selected families is buildable here, so every leg would skip
            # and the PR would go green from a build job that ran no compiler. That is
            # an unusable selection, not "nothing selected": say UNSCOPED - which
            # build.yml and .circleci/config.yml both grep for - and emit the full
            # matrix. An explicit families: [] is still a legitimate nothing-selected,
            # and a PARTIAL miss still scopes to the families that do build.
            print(f'ci_set_matrix: UNSCOPED - no selected family is built by any '
                  f'toolchain here ({", ".join(unbuilt)}), emitting the full matrix',
                  file=sys.stderr)
            return set_matrix_json(None, pinned)
        if unbuilt:
            print(f'ci_set_matrix: selected families built by no toolchain here: '
                  f'{", ".join(unbuilt)}', file=sys.stderr)
    if pinned:
        # last, after every UNSCOPED decision above: those rules ask whether the
        # SELECTION is usable, and an empty pinned matrix is a legitimate "no pinned
        # family selected", not a selection to widen back to the full matrix.
        keep = pinned_families()
        matrix = {tc: [f for f in fams if f in keep] for tc, fams in matrix.items()}
    print(json.dumps(matrix))


def main():
    parser = argparse.ArgumentParser()
    group = parser.add_mutually_exclusive_group()
    group.add_argument('--select', help='selection manifest v1 (check_build.py --select-only); '
                                        'scopes families when build.full is false')
    # a whole selection as one argv/env value can exceed the exec limits on a big
    # diff, which fails the calling step BEFORE it can fall open; callers that
    # already have the selection on disk pass the path instead
    group.add_argument('--select-file', help='file holding the same JSON as --select')
    group.add_argument('--base', help='git ref: select with check_build.py --select-only --base REF')
    parser.add_argument('--pinned', action='store_true',
                        help='keep only families with a board in .github/ci-pinned-boards.json '
                             '(the GHA cmake leg; CircleCI builds every board unfiltered)')
    parser.add_argument('--required', action='store_true',
                        help='print the cmake-required legs instead: changed boards the pinned matrix does not build')
    parser.add_argument('--example-map', action='store_true',
                        help="print the selection's per-family example filter instead")
    mb = parser.add_argument_group('membrowse', 'print the membrowse-identical legs instead (JSON values)')
    mb.add_argument('--membrowse', action='store_true')
    mb.add_argument('--pinned-json', default='{}', help="the cmake job's matrix")
    mb.add_argument('--hil-json', default='{}', help="hil-build's matrix (its esp-idf legs)")
    mb.add_argument('--hil-full-json', default='{}', help='the unscoped tinyusb.json matrix')
    mb.add_argument('--with-esp', action='store_true', help='hil-build-esp runs here (repository owner)')
    args = parser.parse_args()
    if args.membrowse:
        membrowse_json(json.loads(args.pinned_json), json.loads(args.hil_json).get('esp-idf', []),
                       json.loads(args.hil_full_json).get('esp-idf', []), args.with_esp)
        return

    select = None
    try:
        if args.select:
            select = json.loads(args.select)
        elif args.select_file:
            with open(args.select_file) as f:
                select = json.load(f)
        elif args.base:
            r = subprocess.run([sys.executable, os.path.join(REPO, '.claude', 'skills', 'build', 'scripts',
                                                             'check_build.py'), '--select-only', '--base', args.base],
                               capture_output=True, text=True, cwd=REPO, check=True)
            select = json.loads(r.stdout.splitlines()[-1])
    except Exception as e:  # fail-open: an unusable selection must never turn into a red job
        # UNSCOPED is the marker build.yml greps for: it must then drop the build extras
        # (example map, family regex) too, or a full build gets labelled and filtered as
        # a scoped one. Keep the token on every fall-open path.
        print(f'ci_set_matrix: UNSCOPED - selection unusable ({e}), emitting the full '
              f'matrix', file=sys.stderr)
        select = None
    if args.example_map:
        example_map_json(select)
    elif args.required:
        required_json(select)
    else:
        set_matrix_json(select, args.pinned)


if __name__ == '__main__':
    main()
