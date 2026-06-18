import argparse
import json
import os


def _resolve_config_path(config_file):
    if os.path.exists(config_file):
        return config_file

    script_relative = os.path.join(os.path.dirname(__file__), config_file)
    if os.path.exists(script_relative):
        return script_relative

    raise FileNotFoundError(f'Config file not found: {config_file}')


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('config_files', nargs='+', help='Configuration JSON file(s)')
    args = parser.parse_args()

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
            name = board['name']
            flasher = board['flasher']
            # esptool boards must build under esp-idf; others default to arm-gcc
            # but may opt into another bucket via an explicit "toolchain" field
            # (e.g. RISC-V boards like ch32v20x need "riscv-gcc").
            if flasher['name'] == 'esptool':
                toolchain = 'esp-idf'
            else:
                toolchain = board.get('toolchain', 'arm-gcc')

            build_board = f'-b {name}'
            if 'build' in board and 'args' in board['build']:
                build_board += ' ' + ' '.join(f'-D{a}' for a in board['build']['args'])

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
