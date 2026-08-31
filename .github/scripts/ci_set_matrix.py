#!/usr/bin/env python3
import argparse
import json
import os
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

# family: [supported toolchain]
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
    "ch583": ["riscv-gcc"],
    "da1469x": ["arm-gcc"],
    "fomu": ["riscv-gcc"],
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
    # S3, P4 will be built by hil test
    # "-bespressif_s3_devkitm": ["esp-idf"],
    # "-bespressif_p4_function_ev": ["esp-idf"],
}


def set_matrix_json(select=None):
    sel_fams = None
    if select:
        # every shape check is explicit: this runs AFTER main()'s fail-open handler, so
        # an AttributeError on e.g. {"build": ["stm32f4"]} would red the step instead
        # of falling back to the full matrix - the outcome that handler exists to prevent
        b = select.get('build') if isinstance(select, dict) else None
        if not isinstance(b, dict):
            b = {}
        if b.get('full') is False:
            fams = b.get('families')
            if not (isinstance(fams, list) and all(isinstance(f, str) for f in fams)):
                # key ABSENT (or not a list of names) is an unusable selection, not
                # "nothing selected": scoping every toolchain to [] would build zero
                # families and report a vacuous green. An explicit families: [] stays a
                # legitimate nothing-selected.
                print('ci_set_matrix: UNSCOPED - build.full is false but the families '
                      'list is unusable, emitting the full matrix', file=sys.stderr)
            else:
                sel_fams = set(fams)
    matrix = {}
    for toolchain in toolchain_list:
        fams = [family for family, tc in family_list.items() if toolchain in tc]
        if sel_fams is not None:
            fams = [f for f in fams if f in sel_fams]
        matrix[toolchain] = fams
    if sel_fams:
        # a family this file does not list builds on no toolchain, so it contributes no
        # leg. hw/bsp holds several CI has never built (efm32, py32f0, same7x, ...) plus
        # espressif, whose boards hil-build-esp builds by name.
        # espressif is not a gap: its examples need the ESP-IDF environment
        # (CLAUDE.md: `. "$IDF_PATH/export.sh"` before any build), which the cmake legs
        # do not have - that is why it is commented out of family_list above. Its
        # coverage comes from hil-build-esp, which builds those boards BY NAME in an IDF
        # container, so an espressif-only PR is already validated and falling open to the
        # full matrix would add 74 legs, none of which can compile espressif.
        unbuilt = sorted(f for f in sel_fams if f not in family_list and f != 'espressif')
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
            return set_matrix_json(None)
        if unbuilt:
            print(f'ci_set_matrix: selected families built by no toolchain here: '
                  f'{", ".join(unbuilt)}', file=sys.stderr)
    print(json.dumps(matrix))


def main():
    parser = argparse.ArgumentParser()
    group = parser.add_mutually_exclusive_group()
    group.add_argument('--select', help='tools/ci_select.py JSON; scopes families when build.full is false')
    # a whole selection as one argv/env value can exceed the exec limits on a big
    # diff, which fails the calling step BEFORE it can fall open; callers that
    # already have the selection on disk pass the path instead
    group.add_argument('--select-file', help='file holding the same JSON as --select')
    group.add_argument('--base', help='git ref: run tools/ci_select.py --base REF and scope from it')
    args = parser.parse_args()

    select = None
    try:
        if args.select:
            select = json.loads(args.select)
        elif args.select_file:
            with open(args.select_file) as f:
                select = json.load(f)
        elif args.base:
            root = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
            r = subprocess.run([sys.executable, os.path.join(root, 'tools', 'ci_select.py'),
                                '--base', args.base],
                               capture_output=True, text=True, cwd=root, check=True)
            select = json.loads(r.stdout)
    except Exception as e:  # fail-open: an unusable selection must never turn into a red job
        # UNSCOPED is the marker build.yml greps for: it must then drop the build extras
        # (example map, family regex) too, or a full build gets labelled and filtered as
        # a scoped one. Keep the token on every fall-open path.
        print(f'ci_set_matrix: UNSCOPED - selection unusable ({e}), emitting the full '
              f'matrix', file=sys.stderr)
        select = None
    set_matrix_json(select)


if __name__ == '__main__':
    main()
