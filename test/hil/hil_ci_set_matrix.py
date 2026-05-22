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

    matrix = {
        'arm-gcc': [],
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
            if flasher['name'] == 'esptool':
                toolchain = 'esp-idf'
            else:
                toolchain = 'arm-gcc'

            build_board = f'-b {name}'
            if 'build' in board:
                if 'args' in board['build']:
                    build_board += ' ' + ' '.join(f'-D{a}' for a in board['build']['args'])
                if 'flags_on' in board['build']:
                    for f in board['build']['flags_on']:
                        if f == '':
                            append_build_arg(toolchain, build_board)
                        else:
                            append_build_arg(toolchain, f'{build_board} -f1 {f.replace(" ", " -f1 ")}')
                else:
                    append_build_arg(toolchain, build_board)
            else:
                append_build_arg(toolchain, build_board)

    print(json.dumps(matrix))


if __name__ == '__main__':
    main()
