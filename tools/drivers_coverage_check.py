#!/usr/bin/env python3
"""Driver coverage for membrowse's CI boards and the HIL board pool.

Every dcd_*/hcd_* driver under src/portable (plus ehci/ohci, minus template/)
is checked against two rosters: `.github/ci-pinned-boards.json` and the HIL
rig rosters (test/hil/tinyusb.json, test/hil/hfp.json - which board family,
if any, on the physical rig builds it).

A pinned board covers the drivers its default configure compiles, per its
hw/bsp/family.json row: `cmake.portable` filtered by `cmake.roles` - dcd_*
counts with a device or dual role, hcd_*/ehci/ohci with host or dual. This
trusts family.json, so coverage is only as fresh as the catalog: a
skip.txt/only.txt/example CMakeLists.txt edit shows up at its next refresh.
`uncovered` waives drivers with a reason; a waiver may overlap a driver the
pinned boards compile (e.g. an empty build without its CFG_ option).

Fatal, one line per error to stderr, exit 1: malformed json, a non-object or
board-less or duplicate entry, an unknown board, a pinned board with no
family.json row or a null cmake row, a family no CI toolchain builds
(ci_set_matrix.family_list), a waiver naming no driver source file or with a
blank reason, and a driver neither covered nor waived. Exit 0 otherwise, with
INFO lines on stdout: each waiver with the pinned boards it suppresses, and
drivers with no rig board.

The HIL family mapping reuses tools/ci_select.py's rule-3/4 machinery
(port_families()/port_option_gates()/board_roles(), the same data and role
filter that pick which rig boards a src/portable/ diff selects) rather than
a second heuristic.
"""
import functools
import json
import os
import sys

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
FAMILY_JSON = os.path.join(REPO, 'hw', 'bsp', 'family.json')
PORTABLE = os.path.join(REPO, 'src', 'portable')
sys.path.insert(0, os.path.join(REPO, 'tools'))
import ci_select  # noqa: E402
sys.path.insert(0, os.path.join(REPO, '.github', 'scripts'))
import ci_set_matrix  # noqa: E402


def _is_host_driver(d):
    return d.startswith('hcd_') or d in ('ehci', 'ohci')


def row_drivers(row):
    """Driver stems a family.json cmake row compiles for a role that uses them."""
    roles = set(row['roles'])
    out = set()
    for src in row['portable']:
        stem = os.path.splitext(os.path.basename(src))[0]
        if _is_host_driver(stem):
            wanted = {'host', 'dual'}
        elif stem.startswith('dcd_'):
            wanted = {'device', 'dual'}
        else:
            continue
        if roles & wanted:
            out.add(stem)
    return out


@functools.lru_cache(maxsize=None)
def list_driver_paths(portable_dir):
    """{stem: source path} for every dcd_*/hcd_* driver plus ehci/ohci, template
    excluded. A stem collision (two files with the same driver name) silently
    keeps whichever os.walk() visits last - the real tree has none today."""
    paths = {}
    for root, _dirs, files in os.walk(portable_dir):
        if os.path.basename(root) == 'template':
            continue
        for f in files:
            stem, ext = os.path.splitext(f)
            if ext != '.c':
                continue
            if stem.startswith(('dcd_', 'hcd_')) or stem in ('ehci', 'ohci'):
                paths[stem] = os.path.join(root, f)
    return paths


def _port_scope(driver_path, gates_by_port, repo_root):
    """(port, families building it, build options gating it) for one driver source."""
    port = _driver_port(driver_path, repo_root)
    if not port:
        return None, set(), set()
    return port, ci_select.port_families(port, repo_root), gates_by_port.get(port, set())


def pinned_coverage(path, catalog_path=FAMILY_JSON):
    """(errors, {board: driver stems} before waivers, uncovered)."""
    with open(path) as f:
        data = json.load(f)
    boards = data.get('boards')
    uncovered = data.get('uncovered', {})
    if not isinstance(boards, list) or not boards:
        return [f'{path}: "boards" must be a non-empty list'], {}, {}
    if not isinstance(uncovered, dict):
        return [f'{path}: "uncovered" must be an object of driver: reason'], {}, {}
    with open(catalog_path) as f:
        catalog = json.load(f)

    errors = []
    drivers = set(list_driver_paths(PORTABLE))
    # ci_set_matrix.family_list is the ground truth for which families CI builds at
    # all - espressif included, with no toolchain, since hil-build-esp builds it by
    # board name rather than as a matrix leg.
    ci_families = set(ci_set_matrix.family_list)
    coverage = {}
    for i, t in enumerate(boards):
        where = f'boards[{i}]'
        if not isinstance(t, dict):
            errors.append(f'{where}: must be an object, not {type(t).__name__}')
            continue
        board = t.get('board')
        if not board:
            errors.append(f'{where}: missing "board"')
            continue
        where = f'{where} ({board})'
        if board in coverage:
            errors.append(f'{where}: duplicate entry')
            continue
        family = ci_select.board_family(board, REPO)
        if family is None:
            errors.append(f'{where}: unknown board (no hw/bsp/*/boards/{board})')
            continue
        if family not in ci_families:
            errors.append(
                f'{where}: family "{family}" is pinned but built by no CI toolchain '
                f'(not in ci_set_matrix.family_list), so it covers nothing')
        if board not in catalog.get(family, {}):
            errors.append(f'{where}: no row in hw/bsp/family.json')
            continue
        row = catalog[family][board]
        if not (row and row.get('cmake')):
            errors.append(f'{where}: hw/bsp/family.json row has no cmake configure')
            continue
        coverage[board] = row_drivers(row['cmake'])

    for d, reason in uncovered.items():
        if d not in drivers:
            errors.append(f'uncovered: "{d}" matches no driver source file')
        if not (isinstance(reason, str) and reason.strip()):
            errors.append(f'uncovered "{d}": reason must be a non-empty string')

    covered = set().union(*coverage.values())
    for d in sorted(drivers - covered - set(uncovered)):
        errors.append(f'membrowse: {d} has no CI board and no uncovered entry')
    return errors, coverage, uncovered


def check(path, catalog_path=FAMILY_JSON):
    """Fatal roster validity and undocumented coverage errors, one line each."""
    return pinned_coverage(path, catalog_path)[0]


def membrowse_gaps(coverage, uncovered):
    """('INFO', message) per waiver, naming the pinned boards whose compiled
    driver it suppresses. Only meaningful once check() passed."""
    out = []
    for d, reason in sorted(uncovered.items()):
        boards = sorted(b for b, drivers in coverage.items() if d in drivers)
        which = f'suppresses: {", ".join(boards)}' if boards else 'no candidates'
        out.append(('INFO', f'membrowse: {d} uncovered - {reason} ({which})'))
    return out


def _driver_port(driver_path, repo_root):
    """Port dir per ci_select's rule-3/4 extraction, e.g. 'synopsys/dwc2' from
    'src/portable/synopsys/dwc2/dcd_dwc2.c', or None if it doesn't match (it
    always should - DriverScan pins the tree shape ci_select._PORT_PATH_RE
    expects)."""
    rel = os.path.relpath(driver_path, repo_root).replace(os.sep, '/')
    m = ci_select._PORT_PATH_RE.match(rel)
    return m.group(1) if m else None


def _hil_roster_boards(repo_root):
    boards = {}
    for name in ('tinyusb.json', 'hfp.json'):
        with open(os.path.join(repo_root, 'test', 'hil', name)) as f:
            for board in json.load(f).get('boards', []):
                boards.setdefault(board['name'], board)
    return list(boards.values())


def hil_gaps(repo_root=REPO):
    """('INFO', message) for every driver with no board on the HIL rig.
    A driver is covered when a roster board (a) has the matching role for the
    driver - device for dcd_*, host for hcd_*/ehci/ohci, checked against
    ci_select.board_roles(), exactly the `board_roles(b) & roles` filter
    rule 3/4 applies - and (b) either its family is in the driver's port's
    family set (ci_select.port_families - the same family.cmake references
    rule 3/4 reads for a src/portable/ diff) or it turns on a build option
    that gates the port regardless of family (ci_select.port_option_gates/
    board_options - e.g. analog/max3421's MAX3421_HOST)."""
    roster_boards = _hil_roster_boards(repo_root)
    gates_by_port = ci_select.port_option_gates(repo_root)
    driver_paths = list_driver_paths(os.path.join(repo_root, 'src', 'portable'))
    out = []
    for d in sorted(driver_paths):
        _port, fams, gates = _port_scope(driver_paths[d], gates_by_port, repo_root)
        # not ci_select._port_roles: bare ehci.c/ohci.c would read as "both" roles there
        role = 'host' if _is_host_driver(d) else 'device'
        covered = any(
            (ci_select.board_family(b['name'], repo_root) in fams or
             (gates and ci_select.board_options(b, repo_root) & gates)) and
            role in ci_select.board_roles(b)
            for b in roster_boards)
        if not covered:
            out.append(('INFO', f'hil: {d} has no board on the rig'))
    return out


def main():
    path = sys.argv[1] if len(sys.argv) > 1 else os.path.join(
        REPO, '.github', 'ci-pinned-boards.json')
    catalog_path = sys.argv[2] if len(sys.argv) > 2 else FAMILY_JSON
    errors, coverage, uncovered = pinned_coverage(path, catalog_path)
    for e in errors:
        print(e, file=sys.stderr)
    if errors:
        return 1
    gaps = membrowse_gaps(coverage, uncovered) + hil_gaps()
    for level, msg in gaps:
        print(f'{level}: {msg}')
    return 0


if __name__ == '__main__':
    sys.exit(main())
