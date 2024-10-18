#!/usr/bin/env python
#
# SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0

import sys
import argparse
from pathlib import Path
from glob import glob
from idf_component_tools.manifest import ManifestManager


def override_with_local_component(component, local_path, app):
    app_path = Path(app)

    absolute_local_path = Path(local_path).absolute()
    if not absolute_local_path.exists():
        print('[Error] {} path does not exist'.format(local_path))
        raise Exception
    if not app_path.exists():
        print('[Error] {} path does not exist'.format(app_path))
        raise Exception

    print('[Info] Processing app {}'.format(app))
    manager = ManifestManager(app_path / 'main', 'app')
    if '/' not in component:
        # Prepend with default namespace
        component_with_namespace = 'espressif/' + component

    try:
        manager.manifest_tree['dependencies'][component_with_namespace] = {
            'version': '*',
            'override_path': str(absolute_local_path)
            }
    except KeyError:
        print('[Error] {} app does not depend on {}'.format(app, component_with_namespace))
        raise KeyError

    manager.dump()


def override_with_local_component_all(component, local_path, apps):
    # Process wildcard, e.g. "app_prefix_*"
    apps_with_glob = list()
    for app in apps:
        apps_with_glob += glob(app)

    # Go through all collected apps
    for app in apps_with_glob:
        try:
            override_with_local_component(component, local_path, app)
        except:
            print("[Error] Could not process app {}".format(app))
            return -1
    return 0


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('component', help='Existing component that the app depends on')
    parser.add_argument('local_path', help='Path to component that will be used instead of the managed version')
    parser.add_argument('apps', nargs='*', help='List of apps to process')
    args = parser.parse_args()
    sys.exit(override_with_local_component_all(args.component, args.local_path, args.apps))
