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

        if 'build' in board and 'flags_on' in board['build']:
            for f in board['build']['flags_on']:
                if f == '':
                    matrix[toolchain].append(f'-b {name}')
                else:
                    matrix[toolchain].append(f'-b {name} -f1 {f.replace(" ", " -f1 ")}')
        else:
            matrix[toolchain].append(f'-b {name}')

    print(json.dumps(matrix))


if __name__ == '__main__':
    main()
