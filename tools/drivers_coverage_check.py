#!/usr/bin/env python3
"""Driver coverage for membrowse's CI boards and the HIL board pool.

Every dcd_*/hcd_* driver under src/portable (plus ehci/ohci, minus template/)
is checked against two rosters: `.github/ci-pinned-boards.json` (which
board's `drivers` list, or `uncovered`, covers it) and the HIL rig rosters
(test/hil/tinyusb.json, test/hil/hfp.json - which board family, if any, on
the physical rig builds it).

Coverage GAPS are informational only and never fail the run: a membrowse gap
documented in `uncovered` prints INFO, an undocumented one prints WARNING,
and a driver with no rig board prints INFO. VALIDITY errors - malformed
json, an unknown driver/board name, a board/driver mismatch, a board whose
family no CI toolchain actually builds (ci_set_matrix.family_list), a driver claimed by both
`boards` and `uncovered`, or an hcd_*/ehci/ohci claim on a board that
builds no host/ or dual/ example (host examples are only.txt opt-in) - are
bugs in the file, not gaps, and stay fatal: one line per error to stderr,
exit 1. Exit 0 otherwise, with any INFO/WARNING lines on stdout.

The HIL family mapping reuses tools/ci_select.py's rule-3/4 machinery
(port_families()/port_option_gates()/board_roles(), the same data and role
filter that pick which rig boards a src/portable/ diff selects) rather than
a second heuristic.
"""
import json
import os
import sys

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
sys.path.insert(0, os.path.join(REPO, 'tools'))
import build  # noqa: E402
import build_utils  # noqa: E402
import ci_select  # noqa: E402
sys.path.insert(0, os.path.join(REPO, '.github', 'scripts'))
import ci_set_matrix  # noqa: E402


def _is_host_driver(d):
    return d.startswith('hcd_') or d in ('ehci', 'ohci')


def _builds_host_or_dual(board, family):
    """True if `board` (of `family`) builds at least one host/ or dual/ example -
    the ground truth build.py's cmake_board() gets from the skip.txt/only.txt
    opt-in for a real build.

    Both build.get_examples() and build_utils.skip_example() are cwd-relative, so
    the chdir must scope BOTH calls, not just the first: restoring cwd before
    skip_example() runs made it answer against the CALLER's cwd (e.g. running
    this checker from outside the repo produced bogus FATAL "builds no host/ or
    dual/ example" errors for every host/dual driver claim)."""
    with ci_select._in_repo(REPO):
        examples = build.get_examples(family)
        return any(e.startswith(('host/', 'dual/'))
                  and not build_utils.skip_example(e, board)
                  for e in examples)


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


def list_drivers(portable_dir):
    """Driver source basenames: dcd_*/hcd_* stems plus ehci/ohci, template excluded."""
    return set(list_driver_paths(portable_dir))


def load_boards(path):
    with open(path) as f:  # NOSONAR - trusted local developer CLI path
        return json.load(f)


def check(path):
    """Validity errors only - fatal, one line each. Coverage gaps (a driver
    with no CI board) are no longer errors; see membrowse_gaps()."""
    errors = []
    data = load_boards(path)
    boards = data.get('boards')
    uncovered = data.get('uncovered', {})
    if not isinstance(boards, list) or not boards:
        return [f'{path}: "boards" must be a non-empty list']
    if not isinstance(uncovered, dict):
        return [f'{path}: "uncovered" must be an object of driver: reason']

    driver_paths = list_driver_paths(os.path.join(REPO, 'src', 'portable'))
    drivers = set(driver_paths)
    # ci_set_matrix.family_list is the ground truth for which families any CI
    # toolchain actually builds. espressif is a deliberate exception there (see its
    # own comment): hil-build-esp builds it by board name, not through this file, so
    # its absence from family_list doesn't mean it's unbuilt.
    ci_families = set(ci_set_matrix.family_list) | {'espressif'}
    gates_by_port = ci_select.port_option_gates(REPO)
    covered = set()
    for i, t in enumerate(boards):
        where = f'boards[{i}]'
        if not isinstance(t, dict):
            # a bare string/number/list here would otherwise TypeError on t.get()
            # below - one clear error beats a traceback or bogus "missing" errors
            errors.append(f'{where}: must be an object, not {type(t).__name__}')
            continue
        for key in ('board', 'drivers'):
            if key not in t:
                errors.append(f'{where}: missing "{key}"')
        board = t.get('board', '')
        family = ci_select.board_family(board, REPO) if board else None
        board_ok = family is not None
        options = ci_select.board_options({'name': board}, REPO) if board_ok else set()
        if board and not board_ok:
            errors.append(f'{where}: unknown board "{board}" (no hw/bsp/*/boards/{board})')
        if board_ok and family not in ci_families:
            errors.append(
                f'{where} ({board}): family "{family}" is pinned but built by no CI '
                f'toolchain (not in ci_set_matrix.family_list) - no leg ever builds it, '
                f'so its "drivers" coverage claim is false')
        driver_list = t.get('drivers', [])
        if not isinstance(driver_list, list):
            # a string iterates character-by-character below (each "character
            # matches no driver source file"); null/a number/an object outright
            # TypeErrors on `for d in driver_list` - one clear error beats either
            errors.append(f'{where}: "drivers" must be a list of driver names')
            driver_list = []
        for d in driver_list:
            if d not in drivers:
                errors.append(f'{where} ({board}): "{d}" matches no driver source file')
            elif board_ok:
                port = _driver_port(driver_paths[d], REPO)
                families = ci_select.port_families(port, REPO) if port else set()
                gates = gates_by_port.get(port, set()) if port else set()
                if family not in families and not gates & options:
                    errors.append(
                        f'{where} ({board}): claims "{d}" but does not build its '
                        f'"{port}" port')
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
            errors.append(f'"{d}" is both covered and uncovered')
        if not (isinstance(reason, str) and reason.strip()):
            errors.append(f'uncovered "{d}": reason must be a non-empty string')

    return errors


def membrowse_gaps(path):
    """(level, message) for every driver `path` (ci-pinned-boards.json) does not
    cover: INFO when the gap is documented in "uncovered", WARNING when it is
    silently missing."""
    data = load_boards(path)
    drivers = list_drivers(os.path.join(REPO, 'src', 'portable'))
    covered = {d for t in data.get('boards', []) for d in t.get('drivers', [])}
    uncovered = data.get('uncovered', {})
    out = []
    for d in sorted(drivers - covered):
        if d in uncovered:
            out.append(('INFO', f'membrowse: {d} uncovered - {uncovered[d]}'))
        else:
            out.append(('WARNING',
                        f'membrowse: {d} has no CI board (and no uncovered entry)'))
    return out


def _driver_port(driver_path, repo_root):
    """Port dir per ci_select's rule-3/4 extraction, e.g. 'synopsys/dwc2' from
    'src/portable/synopsys/dwc2/dcd_dwc2.c', or None if it doesn't match (it
    always should - DriverScan pins the tree shape ci_select._PORT_PATH_RE
    expects)."""
    rel = os.path.relpath(driver_path, repo_root).replace(os.sep, '/')
    m = ci_select._PORT_PATH_RE.match(rel)
    return m.group(1) if m else None


def _driver_role(d):
    """'host' for hcd_*/ehci/ohci, else 'device' - _is_host_driver's naming
    convention (not ci_select._port_roles: a bare 'ehci.c'/'ohci.c' matches
    neither of _port_roles' dcd_/hcd_ patterns, so it falls through to "both"
    there, which would defeat the host-only filter these two need)."""
    return 'host' if _is_host_driver(d) else 'device'


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
    roster_boards = build_utils.hil_roster_boards(repo_root)
    gates_by_port = ci_select.port_option_gates(repo_root)
    driver_paths = list_driver_paths(os.path.join(repo_root, 'src', 'portable'))
    out = []
    for d in sorted(driver_paths):
        port = _driver_port(driver_paths[d], repo_root)
        fams = ci_select.port_families(port, repo_root) if port else set()
        gates = gates_by_port.get(port, set()) if port else set()
        role = _driver_role(d)
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
    errors = check(path)
    for e in errors:
        print(e, file=sys.stderr)
    if errors:
        return 1
    for level, msg in membrowse_gaps(path) + hil_gaps():
        print(f'{level}: {msg}')
    return 0


if __name__ == '__main__':
    sys.exit(main())
