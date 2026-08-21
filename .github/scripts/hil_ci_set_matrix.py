import argparse
import json
import os
import sys


def _resolve_config_path(config_file):
    if os.path.exists(config_file):
        return config_file

    # bare roster names resolve against the repo's test/hil (this script lives in
    # .github/scripts); build.yml passes explicit paths, this is for hand-runs
    repo_relative = os.path.join(os.path.dirname(__file__), '..', '..', 'test', 'hil', config_file)
    if os.path.exists(repo_relative):
        return repo_relative

    raise FileNotFoundError(f'Config file not found: {config_file}')


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('config_files', nargs='+', help='Configuration JSON file(s)')
    g = parser.add_mutually_exclusive_group()
    g.add_argument('--select', help='ci_select.py JSON; scopes boards when full=false')
    # a whole selection as one argv can exceed MAX_ARG_STRLEN on a big diff, which
    # would fail the step instead of falling open; callers that already have the
    # selection on disk pass the path instead
    g.add_argument('--select-file', help='file holding the same JSON as --select')
    args = parser.parse_args()

    raw = args.select
    sel = None
    try:
        if args.select_file:
            with open(args.select_file) as f:
                raw = f.read()
        if raw:
            sel = json.loads(raw)
        if sel is not None and not isinstance(sel, dict):
            raise ValueError(f'selection is {type(sel).__name__}, not an object')
    except Exception as e:  # fail-open: an unusable selection must never red the job
        print(f'hil_ci_set_matrix: selection unusable ({e}) - full roster',
              file=sys.stderr)
        sel = None

    selected = None
    if sel and not sel.get('full'):
        # key ABSENT is an unusable selection, not "nothing selected" - same reading as
        # ci_set_matrix.py. Filtering every board out would skip every hil-build leg and,
        # through needs:, both rig jobs: an all-green PR with zero hardware coverage.
        # An explicit boards: {} stays a legitimate nothing-selected.
        if not isinstance(sel.get('boards'), dict):
            print('hil_ci_set_matrix: selection has full false but no usable boards '
                  'map - full roster', file=sys.stderr)
            sel = None          # ALL of it is unusable, hil_examples included: keeping
                                # the -e lists would build a few examples per board
                                # while the rig, unfiltered, runs that board's whole
                                # test list - flash failures on the fail-open path
        else:
            selected = set(sel['boards'])
    ex_map = (sel or {}).get('hil_examples') or {}
    if not isinstance(ex_map, dict):
        ex_map = {}

    # Toolchain buckets must match the toolchains instantiated by the hil-build
    # job in .github/workflows/build.yml. Keep all keys present (even if empty)
    # so `fromJSON(hil_json)[toolchain]` always resolves to a list.
    matrix = {
        'arm-gcc': [],
        'riscv-gcc': [],
        'esp-idf': []
    }

    seen = {toolchain: set() for toolchain in matrix}

    def append_build_arg(toolchain, build_arg):
        if build_arg not in seen[toolchain]:
            seen[toolchain].add(build_arg)
            matrix[toolchain].append(build_arg)

    for config_file in args.config_files:
        with open(_resolve_config_path(config_file)) as f:
            config = json.load(f)

        for board in config['boards']:
            if selected is not None and board['name'] not in selected:
                continue
            name = board['name']
            flasher = board['flasher']
            # esptool boards must build under esp-idf; others default to arm-gcc
            # but may opt into another bucket via an explicit "toolchain" field
            # (e.g. RISC-V boards like ch32v20x need "riscv-gcc").
            if flasher['name'] == 'esptool':
                toolchain = 'esp-idf'
            else:
                toolchain = board.get('toolchain', 'arm-gcc')
            if toolchain not in matrix:
                # a board in no bucket would never be built, and the bare KeyError
                # below would only say so as a traceback from the set-matrix job
                raise SystemExit(
                    f'{name}: toolchain {toolchain!r} is not a build bucket '
                    f'({", ".join(matrix)}); add it here and to the hil-build / '
                    f'hil-build-esp jobs in .github/workflows/build.yml')

            build_board = f'-b {name}'

            # PR selection: build only the examples this board will run (its test
            # list plus device/board_test, the parking firmware) - tools/build.py -e.
            # Absent key (hand runs, full non-PR builds) keeps --target all.
            for ex in ex_map.get(name, []):
                build_board += f' -e {ex}'

            # Each variant builds into cmake-build-<variant.name> with its own cmake
            # -D defines and raw CFLAGS. No 'variant' -> a single build named after
            # the board.
            variants = board.get('variant') or [{'name': name, 'flags': ''}]
            for v in variants:
                arg = build_board
                if v['name'] != name:
                    arg += f' --build-name {v["name"]}'
                for d in v.get('defines', []):
                    arg += f' -D{d}'
                for tok in v.get('flags', '').split():
                    arg += f' --cflag={tok}'
                append_build_arg(toolchain, arg)

    print(json.dumps(matrix))


if __name__ == '__main__':
    main()
