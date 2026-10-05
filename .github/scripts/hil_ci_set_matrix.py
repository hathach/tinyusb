import argparse
import json
import shlex
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..', 'test', 'hil', 'helper'))
import hil_report  # noqa: E402  stdlib-only; board_variants() reads a roster board's builds


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
    g.add_argument('--select', help='selection manifest v1 (check_build.py --select-only); '
                                    'scopes boards when hil.full is false')
    # a whole selection as one argv can exceed MAX_ARG_STRLEN on a big diff, which
    # would fail the step instead of falling open; callers that already have the
    # selection on disk pass the path instead
    g.add_argument('--select-file', help='file holding the same JSON as --select')
    args = parser.parse_args()

    raw = args.select
    hil = None
    try:
        if args.select_file:
            with open(args.select_file) as f:
                raw = f.read()
        if raw:
            # imported here, under the fall-open: a broken selector must still leave the
            # full roster
            sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..', 'tools'))
            import ci_select
            hil = ci_select.check_manifest(json.loads(raw))['hil']
    except Exception as e:  # fail-open: an unusable selection must never red the job
        # ALL of it is unusable, hil_examples included: keeping the -e lists would build
        # a few examples per board while the rig, unfiltered, runs its whole test list
        print(f'hil_ci_set_matrix: selection unusable ({e}) - full roster',
              file=sys.stderr)
        hil = None

    # an explicit empty boards map stays a legitimate nothing-selected
    selected = None if hil is None or hil['full'] else set(hil['boards'])
    ex_map = hil['hil_examples'] if hil else {}

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

            # an always-on define (MAX3421_HOST=1, LOGGER=rtt) is a single self-named variant
            try:
                variants = hil_report.board_variants(board)
            except ValueError as e:
                raise SystemExit(f'{config_file}: {e}')
            for v in variants:
                if toolchain == 'esp-idf' and v['name'] == name and (v['defines'] or v['flags']):
                    # hil-build-esp sizes and uploads every leg as the board its --build-name names
                    raise SystemExit(f'{config_file}: {name}: an esp-idf variant needs a name of its own')
                arg = build_board
                if v['name'] != name:
                    arg += f' --build-name {v["name"]}'
                # build_util.yml's Build step splices this string into bash source,
                # so the quoting round-trips a spaced value into one argv item like
                # build_board's argv path. The SAME string also reaches the get_deps
                # env expansion and the artifact-name charset, where spaced/quoted
                # values still fail (loudly) -- keep defines space-free
                for d in v['defines']:
                    arg += f' -D{shlex.quote(d)}'
                for tok in v['flags']:
                    arg += f' --cflag={tok}'
                append_build_arg(toolchain, arg)

    print(json.dumps(matrix))


if __name__ == '__main__':
    main()
