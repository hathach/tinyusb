#!/usr/bin/env python3
import os
import json


def main():
    board_list = []

    # Find all board.cmake files
    for root, dirs, files in os.walk("hw/bsp"):
        for file in files:
            if file == "board.cmake":
                board_list.append(os.path.basename(root))

    print('Generating presets for the following boards:')
    print(board_list)

    # Generate the presets
    presets = {}
    presets['version'] = 6

    # Configure presets
    presets['configurePresets'] = [
        {"name": "default",
         "hidden": True,
         "description": r"Configure preset for the ${presetName} board",
         "generator": "Ninja Multi-Config",
         "binaryDir": r"${sourceDir}/build/${presetName}",
         "cacheVariables": {
             "CMAKE_DEFAULT_BUILD_TYPE": "RelWithDebInfo",
             "BOARD": r"${presetName}"
         }}]

    presets['configurePresets'].extend(
        sorted(
            [
                {
                    'name': board,
                    'inherits': 'default'
                }
                for board in board_list
            ], key=lambda x: x['name']
        )
    )

    # Build presets
    # no inheritance since 'name' doesn't support macro expansion
    presets['buildPresets'] = sorted(
        [
            {
                'name': board,
                'description': "Build preset for the " + board + " board",
                'configurePreset': board
            }
            for board in board_list
        ], key=lambda x: x['name']
    )

    # Workflow presets
    presets['workflowPresets'] = sorted(
        [
            {
                "name": board,
                "steps": [
                    {
                        "type": "configure",
                        "name": board
                    },
                    {
                        "type": "build",
                        "name": board
                    }
                ]
            }
            for board in board_list
        ], key=lambda x: x['name']
    )

    with open("hw/bsp/BoardPresets.json", "w") as f:
        f.write('{}\n'.format(json.dumps(presets, indent=2)))

    # Generate presets for examples
    presets = {
        "version": 6,
        "include": [
            "../../../hw/bsp/BoardPresets.json"
        ]
    }

    example_list = []
    for root, dirs, files in os.walk("examples"):
        for file in files:
            if file == "CMakeLists.txt":
                with open(os.path.join(root, 'CMakePresets.json'), 'w') as f:
                    f.write('{}\n'.format(json.dumps(presets, indent=2)))
                example_list.append(os.path.basename(root))

    print('Generating presets for the following examples:')
    print(example_list)


if __name__ == "__main__":
    main()
