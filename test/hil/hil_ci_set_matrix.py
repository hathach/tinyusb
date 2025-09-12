import argparse
import json
import os


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('config_file', help='Configuration JSON file')
    args = parser.parse_args()

    config_file = args.config_file

    # if config file is not found, try to find it in the same directory as this script
    if not os.path.exists(config_file):
        config_file = os.path.join(os.path.dirname(__file__), config_file)
    with open(config_file) as f:
        config = json.load(f)

    matrix = {
        'arm-gcc': [],
        'esp-idf': []
    }
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
                        matrix[toolchain].append(build_board)
                    else:
                        matrix[toolchain].append(f'{build_board} -f1 {f.replace(" ", " -f1 ")}')
            else:
                matrix[toolchain].append(build_board)
        else:
            matrix[toolchain].append(build_board)

    print(json.dumps(matrix))


if __name__ == '__main__':
    main()
