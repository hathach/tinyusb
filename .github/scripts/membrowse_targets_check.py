#!/usr/bin/env python3
"""Validate .github/membrowse-targets.json against the driver and board tree.

Every dcd_*/hcd_* driver under src/portable (plus ehci/ohci, minus template/)
must be covered by a pinned board's `drivers` list or listed in `uncovered`
with a non-empty reason. Boards/families must exist under hw/bsp. A pinned
entry claiming an hcd_*/ehci/ohci driver must also build at least one host/
or dual/ example - host examples are only.txt opt-in, so a board can list a
host driver it never actually compiles. Exit 0 on success; print one line
per error to stderr and exit 1 otherwise.
"""
import json
import os
import sys

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
sys.path.insert(0, os.path.join(REPO, 'tools'))
import build  # noqa: E402
import build_utils  # noqa: E402


def _is_host_driver(d):
    return d.startswith('hcd_') or d in ('ehci', 'ohci')


def _builds_host_or_dual(board, family):
    """True if `board` (of `family`) builds at least one host/ or dual/ example -
    the ground truth build.py's cmake_board() gets from the skip.txt/only.txt
    opt-in for a real build."""
    cwd = os.getcwd()
    try:
        os.chdir(REPO)
        examples = build.get_examples(family)
    finally:
        os.chdir(cwd)
    return any((e.startswith('host/') or e.startswith('dual/'))
              and not build_utils.skip_example(e, board)
              for e in examples)


def list_drivers(portable_dir):
    """Driver source basenames: dcd_*/hcd_* stems plus ehci/ohci, template excluded."""
    drivers = set()
    for root, _dirs, files in os.walk(portable_dir):
        if os.path.basename(root) == 'template':
            continue
        for f in files:
            stem, ext = os.path.splitext(f)
            if ext != '.c':
                continue
            if stem.startswith(('dcd_', 'hcd_')) or stem in ('ehci', 'ohci'):
                drivers.add(stem)
    return drivers


def load_targets(path):
    with open(path) as f:
        return json.load(f)


def check(path):
    errors = []
    data = load_targets(path)
    targets = data.get('targets')
    uncovered = data.get('uncovered', {})
    if not isinstance(targets, list) or not targets:
        return [f'{path}: "targets" must be a non-empty list']
    if not isinstance(uncovered, dict):
        return [f'{path}: "uncovered" must be an object of driver: reason']

    drivers = list_drivers(os.path.join(REPO, 'src', 'portable'))
    covered = set()
    for i, t in enumerate(targets):
        where = f'targets[{i}]'
        for key in ('board', 'family', 'drivers'):
            if key not in t:
                errors.append(f'{where}: missing "{key}"')
        board, family = t.get('board', ''), t.get('family', '')
        board_ok = False
        if family and not os.path.isdir(os.path.join(REPO, 'hw', 'bsp', family)):
            errors.append(f'{where}: unknown family "{family}"')
        elif board and not os.path.isdir(
                os.path.join(REPO, 'hw', 'bsp', family, 'boards', board)):
            errors.append(f'{where}: unknown board "{board}" in family "{family}"')
        else:
            board_ok = bool(board and family)
        driver_list = t.get('drivers', [])
        for d in driver_list:
            if d not in drivers:
                errors.append(f'{where} ({board}): "{d}" matches no driver source file')
            covered.add(d)

        host_drivers = [d for d in driver_list if _is_host_driver(d)]
        if host_drivers and board_ok and not _builds_host_or_dual(board, family):
            errors.append(
                f'{where} ({board}): claims {host_drivers} but builds no host/ or '
                f'dual/ example (host examples are only.txt opt-in)')

    for d, reason in uncovered.items():
        if d not in drivers:
            errors.append(f'uncovered: "{d}" matches no driver source file')
        if d in covered:
            errors.append(f'"{d}" is both pinned and uncovered')
        if not (isinstance(reason, str) and reason.strip()):
            errors.append(f'uncovered "{d}": reason must be a non-empty string')

    for d in sorted(drivers - covered - set(uncovered)):
        errors.append(f'driver "{d}" has no pinned board and is not in "uncovered"')
    return errors


def main():
    path = sys.argv[1] if len(sys.argv) > 1 else os.path.join(
        REPO, '.github', 'membrowse-targets.json')
    errors = check(path)
    for e in errors:
        print(e, file=sys.stderr)
    return 1 if errors else 0


if __name__ == '__main__':
    sys.exit(main())
