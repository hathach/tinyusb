#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""PR-diff -> HIL selection: which rig boards and which tests a change can affect.

Stdlib-only (runs on bare CI runners; never imports hil_test/hil_flash/hil_lock).
Fail-open: any file no rule classifies forces the full matrix. See
docs/superpowers/specs/2026-07-29-hil-pr-scoped-selection-design.md.

JSON: full, boards (name -> 'all' | [tests]), families (bsp families the diff
touches, including ones with no rig board - build-only consumers such as /pre-pr
sample from these), args (hil_test.py args per config) and args_flasher (the same
args split by each board's flasher, for CI legs that split one rig by flasher).
"""
import argparse
import functools
import glob
import json
import os
import re
import subprocess
import sys

from hil_examples import device_tests, dual_tests, host_test

ALL_TESTS = {'device': device_tests, 'dual': dual_tests, 'host': host_test}

# class dir -> config macro suffix exceptions (rule 3); dfu is per-file, handled inline
NET_MACROS = ('ECM_RNDIS', 'NCM')

_NONCODE_RE = re.compile(
    r'^(docs/|\.claude/|.*\.(md|rst)$|LICENSE)')
_FULL_RE = re.compile(
    r'^(src/common/|src/osal/|src/tusb\.c$|src/tusb\.h$|src/tusb_option\.h$|'
    r'test/hil/|\.github/workflows/build.*\.yml$|\.github/actions/|'
    r'tools/build\.py$|tools/get_deps\.py$|tools/cmake/|hw/mcu/|lib/|'
    r'hw/bsp/(family_support\.cmake|board_api\.h|board\.c|ansi_escape\.h)$|'
    r'examples/build_system/|examples/CMakeLists\.txt$|'
    # board_test is HIL infrastructure, not a test: hil_test.py flashes it to park
    # every board (variant boundary + end-of-board teardown), so every board depends on it
    r'examples/device/board_test/)')

# --no-renames: with rename detection git reports only a rename's destination, so code
# moved out of an HIL-relevant path would be classified by its new path alone
GIT_DIFF_ARGV = ['git', 'diff', '--no-renames', '--name-only']


def test_role(test: str) -> str:
    return test.split('/', 1)[0]           # 'device' | 'dual' | 'host'


def board_roles(board: dict) -> set:
    t = board.get('tests', {})
    roles = set()
    if t.get('device'):
        roles.add('device')
    if t.get('host'):
        roles.add('host')
    if t.get('dual'):
        roles.update(('device', 'host'))
    for only in t.get('only', []):
        r = test_role(only)
        roles.update(('device', 'host') if r == 'dual' else (r,))
    return roles


def board_tests(board: dict) -> list:
    """Every test this board would run today (mirrors hil_test.test_board's default)."""
    t = board.get('tests', {})
    if 'only' in t:
        run = list(t['only'])
    else:
        run = []
        if t.get('device'):
            run += device_tests
        if t.get('dual'):
            run += dual_tests
        if t.get('host'):
            run += host_test
    return [x for x in run if x not in t.get('skip', [])]


# cached: called per changed file x roster board, and the tree doesn't change mid-run
@functools.lru_cache(maxsize=None)
def board_family(board_name: str, repo_root: str):
    hits = glob.glob(os.path.join(repo_root, 'hw/bsp/*/boards', board_name))
    return os.path.basename(os.path.dirname(os.path.dirname(hits[0]))) if hits else None


# `if (OPTION STREQUAL "1")` guards in family_support.cmake, and the option tokens
# a roster entry passes to the build (NAME=VALUE / -DNAME=VALUE)
_CM_IF_RE = re.compile(r'if\s*\(')
_CM_ELSE_RE = re.compile(r'else(if)?\s*\(')
_CM_ENDIF_RE = re.compile(r'endif\s*\(')
_CM_OPT_RE = re.compile(r'if\s*\(\s*\$?\{?([A-Za-z_]\w*)\}?\s+STREQUAL\s+"?1"?\s*\)')
_CM_PORT_RE = re.compile(r'src/portable/((?:[^/\s]+/)?[^/\s]+)/')
_FALSY = ('', '0', 'off', 'false', 'no')


@functools.lru_cache(maxsize=None)
def port_option_gates(repo_root: str) -> dict:
    """port dir -> build options that compile it regardless of the board's family
    file, e.g. {'analog/max3421': {'MAX3421_HOST'}} from family_support.cmake."""
    gates = {}
    try:
        text = open(os.path.join(repo_root, 'hw/bsp/family_support.cmake')).read()
    except OSError:
        return gates
    stack = []                                  # one entry per open if(): its option, or None
    for line in text.splitlines():
        line = line.strip()
        if _CM_IF_RE.match(line):
            m = _CM_OPT_RE.match(line)
            stack.append(m.group(1) if m else None)
        elif _CM_ELSE_RE.match(line):
            if stack:
                stack[-1] = None                # the guard doesn't hold in this branch
        elif _CM_ENDIF_RE.match(line):
            if stack:
                stack.pop()
        opts = {o for o in stack if o}
        m = _CM_PORT_RE.search(line)
        if opts and m:
            gates.setdefault(m.group(1), set()).update(opts)
    return gates


_CM_SET_RE = re.compile(r'set\s*\(\s*([A-Za-z_]\w*)\s+([^)\s]+)\s*\)')


# cached: called per changed portable file x roster board
@functools.lru_cache(maxsize=None)
def bsp_board_options(board_name: str, repo_root: str) -> frozenset:
    """Build options a board turns on in its own BSP: `set(<OPT> <value>)` in
    hw/bsp/<family>/boards/<board>/board.cmake, e.g. MAX3421_HOST on the espressif
    and rp2040 max3421 boards. CMake only - HIL CI builds nothing with Make, so a
    board.mk-only option (e.g. nrf5340dk's MAX3421_HOST) compiles no port here."""
    fam = board_family(board_name, repo_root)
    if not fam:
        return frozenset()
    path = os.path.join(repo_root, 'hw/bsp', fam, 'boards', board_name, 'board.cmake')
    try:
        text = open(path).read()
    except OSError:
        return frozenset()
    out = set()
    for line in text.splitlines():
        line = line.strip()
        if line.startswith('#'):
            continue
        m = _CM_SET_RE.match(line)
        if m and m.group(2).strip('"').lower() not in _FALSY:
            out.add(m.group(1))
    return frozenset(out)


def board_options(board: dict, repo_root: str) -> set:
    """Build options a board has truthy: the roster entry's build.args plus each
    variant's defines (NAME=VALUE) and raw CFLAGS (-DNAME=VALUE), plus whatever its
    own board.cmake sets (a board can enable a gated port without the roster saying so)."""
    toks = list(board.get('build', {}).get('args', []))
    for v in board.get('variant', []):
        toks += list(v.get('defines', []))
        toks += v.get('flags', '').split()
    out = set(bsp_board_options(board['name'], repo_root))
    for t in toks:
        name, _, val = (t[2:] if t.startswith('-D') else t).partition('=')
        if name and val.strip().strip('"').lower() not in _FALSY:
            out.add(name.strip())
    return out


@functools.lru_cache(maxsize=None)
def port_families(port_dir: str, repo_root: str) -> set:
    """Board families that compile this src/portable dir. CMake only: HIL CI builds
    every board with CMake, so a port wired up in family.mk alone is compiled for no
    HIL board and must not select one. family.cmake lists portable sources directly
    for most families; espressif instead references them from a nested component
    CMakeLists.txt (hw/bsp/espressif/components/tinyusb_src/CMakeLists.txt)."""
    fams = set()
    bsp_root = os.path.join(repo_root, 'hw/bsp')
    # trailing '/' so a port dir is not a prefix of a sibling: bare 'microchip/pic'
    # would otherwise match '.../microchip/pic32mz/...' and inherit its families
    needle = port_dir + '/'
    for f in glob.glob(os.path.join(bsp_root, '*/family.cmake')) + \
             glob.glob(os.path.join(bsp_root, '*/components/*/CMakeLists.txt')):
        try:
            if needle in open(f).read():
                fam = os.path.relpath(f, bsp_root).split(os.sep, 1)[0]
                fams.add(fam)
        except OSError:
            pass
    return fams


def _config_enables(cfg_path: str, macros) -> bool:
    try:
        text = open(cfg_path).read()
    except OSError:
        return False
    return any(re.search(rf'#define\s+{m}\s+\(?\s*0*[1-9]', text) for m in macros)


def roster_only_tests(all_boards) -> set:
    """Test paths that only appear in a roster board's tests.only list (e.g.
    espressif boards), not in the shared device/dual/host_test lists."""
    out = set()
    for b in all_boards:
        out.update(b.get('tests', {}).get('only', []))
    return out


def class_examples(macros, role: str, repo_root: str, extra_tests: set) -> set:
    """Tests (from role's + dual lists, plus roster-only-list tests of that role)
    whose example config enables any macro."""
    pool = role_tests({role}, extra_tests)
    out = set()
    for test in pool:
        cfg = os.path.join(repo_root, 'examples', test, 'src', 'tusb_config.h')
        if _config_enables(cfg, macros):
            out.add(test)
    return out


def role_tests(roles: set, extras: set) -> set:
    """Every test for the given role(s): each role's own list + dual tests,
    plus roster-only-list tests (extras) matching those roles or 'dual'."""
    pool = set(dual_tests)
    for r in roles:
        pool |= set(ALL_TESTS[r])
    pool |= {t for t in extras if test_role(t) in roles or test_role(t) == 'dual'}
    return pool


class _Sel:
    """Accumulates contributions. board->set(tests) plus 'all-board' markers."""
    def __init__(self):
        self.full = False
        self.by_board = {}      # name -> set of tests, or 'all'
        self.roles = set()      # roles touched by any contribution
        self.families = set()   # bsp families touched (incl. off-rig ones: build-only consumers)
        self.reasons = []

    def add(self, boards, tests, reason):
        """tests: 'all' or iterable of test paths."""
        self.reasons.append(reason)
        for b in boards:
            cur = self.by_board.get(b)
            if tests == 'all' or cur == 'all':
                self.by_board[b] = 'all'
            else:
                self.by_board[b] = (cur or set()) | set(tests)

    def force_full(self, reason):
        self.full = True
        self.reasons.append(reason)


def _classify_one(path, repo_root, roster_boards, extras: set, s: _Sel):
    base = os.path.basename(path)
    if _NONCODE_RE.match(path):
        s.reasons.append(f'{path}: non-code, no contribution')
        return
    if _FULL_RE.match(path):
        s.force_full(f'{path}: core/infra -> full matrix')
        return

    m = re.match(r'src/portable/((?:[^/]+/)?[^/]+)/', path)
    if m:
        port = m.group(1)
        if re.match(r'(dcd_|.*_device)', base):
            roles = {'device'}
        elif re.match(r'(hcd_|.*_host)', base):
            roles = {'host'}
        else:
            roles = {'device', 'host'}
        fams = port_families(port, repo_root)
        if not fams:
            # no family references this port: either a new/renamed port dir or a
            # family.cmake layout the scan misses - widen instead of contributing nothing
            s.force_full(f'{path}: port {port} maps to no board family -> full matrix')
            return
        s.families.update(fams)
        # a board can also pull the port in through a build option (e.g. MAX3421_HOST=1
        # from the roster on metro_m4_express, or from its own board.cmake), which its
        # family file never names
        gates = port_option_gates(repo_root).get(port, set())
        boards = [b['name'] for b in roster_boards
                  if (board_family(b['name'], repo_root) in fams or
                      (gates and board_options(b, repo_root) & gates)) and (board_roles(b) & roles)]
        tests = role_tests(roles, extras)
        s.roles.update(roles)
        why = f'{path}: port {port} -> families {sorted(fams)}'
        if gates:
            why += f' + option {sorted(gates)}'
        s.add(boards, tests, f'{why} -> boards {boards} ({"/".join(sorted(roles))})')
        return

    m = re.match(r'src/class/([^/]+)/', path)
    if m:
        cls = m.group(1)
        if re.search(r'_device\.[ch]$', base):
            roles = {'device'}
        elif re.search(r'_host\.[ch]$', base):
            roles = {'host'}
        else:
            roles = {'device', 'host'}
        # macro names per role
        def macros(prefix):
            if cls == 'net':
                return [f'CFG_{prefix}_{m2}' for m2 in NET_MACROS]
            if cls == 'dfu':
                if base.startswith('dfu_rt'):
                    return [f'CFG_{prefix}_DFU_RUNTIME']
                if base.startswith('dfu_device') or base.startswith('dfu_host'):
                    return [f'CFG_{prefix}_DFU']
                return [f'CFG_{prefix}_DFU', f'CFG_{prefix}_DFU_RUNTIME']
            return [f'CFG_{prefix}_{cls.upper()}']
        tests = set()
        if 'device' in roles:
            tests |= class_examples(macros('TUD'), 'device', repo_root, extras)
        if 'host' in roles:
            tests |= class_examples(macros('TUH'), 'host', repo_root, extras)
        boards = [b['name'] for b in roster_boards if board_roles(b) & roles]
        s.roles.update(roles)
        s.add(boards, tests, f'{path}: class {cls} -> {sorted(tests)} ({"/".join(sorted(roles))})')
        return

    m = re.match(r'src/(device|host)/', path)
    if m:
        role = m.group(1)
        boards = [b['name'] for b in roster_boards if role in board_roles(b)]
        s.roles.add(role)
        s.add(boards, role_tests({role}, extras), f'{path}: core {role} stack -> all {role} tests')
        return

    m = re.match(r'hw/bsp/([^/]+)/(?:boards/([^/]+)/)?', path)
    if m:
        fam, brd = m.group(1), m.group(2)
        s.families.add(fam)
        if brd:
            boards = [b['name'] for b in roster_boards if b['name'] == brd]
            why = f'{path}: bsp board {brd}'
        else:
            boards = [b['name'] for b in roster_boards
                      if board_family(b['name'], repo_root) == fam]
            why = f'{path}: bsp family {fam}'
        s.roles.update(('device', 'host'))
        s.add(boards, 'all', f'{why} -> boards {boards}')
        return

    m = re.match(r'examples/(device|host|dual)/([^/]+)/', path)
    if m:
        test = f'{m.group(1)}/{m.group(2)}'
        known = any(test in pool for pool in ALL_TESTS.values()) or test in extras
        if known:
            boards = [b['name'] for b in roster_boards]
            role = test_role(test)
            s.roles.update(('device', 'host') if role == 'dual' else (role,))
            s.add(boards, [test], f'{path}: example -> {test} on all boards')
        else:
            s.reasons.append(f'{path}: example not in HIL lists, no contribution')
        return

    s.force_full(f'{path}: unclassified -> full matrix')


def classify(changed_files, repo_root, rosters):
    all_boards = []
    seen = set()
    for _, boards in rosters:
        for b in boards:
            if b['name'] not in seen:
                seen.add(b['name'])
                all_boards.append(b)

    extras = roster_only_tests(all_boards)
    s = _Sel()
    # no early exit once full: keep classifying so `families` still reports every
    # family the diff touches (build-only consumers need it). Nothing after the first
    # force_full can change full/boards/args - the full branch below ignores by_board.
    for path in changed_files:
        _classify_one(path, repo_root, all_boards, extras, s)

    if s.full:
        return {'full': True, 'boards': {b['name']: 'all' for b in all_boards},
                'families': sorted(s.families), 'reasons': s.reasons}

    # role pruning: single-role selections drop the other role's tests and boards
    by_name = {b['name']: b for b in all_boards}
    out = {}
    for name, tests in s.by_board.items():
        allowed = board_tests(by_name[name])
        if tests == 'all':
            kept = list(allowed)
        else:
            kept = [t for t in allowed if t in tests]
        if s.roles and s.roles != {'device', 'host'}:
            role = next(iter(s.roles))
            kept = [t for t in kept if test_role(t) in (role, 'dual')]
        if kept:
            out[name] = 'all' if set(kept) == set(allowed) else sorted(kept)
    return {'full': False, 'boards': out, 'families': sorted(s.families),
            'reasons': s.reasons}


def _board_args(name, chosen) -> list:
    parts = [f'-b {name}']
    if chosen != 'all':
        parts.append(f'-bt {name}:{",".join(chosen)}')
    return parts


def selection_args(sel, rosters):
    """hil_test.py args per config. Empty means either 'full matrix' or 'nothing
    selected' - callers must read sel['full'] to tell them apart."""
    args = {}
    for cfg_path, boards in rosters:
        parts = []
        if not sel['full']:
            for b in boards:
                chosen = sel['boards'].get(b['name'])
                if chosen is not None:
                    parts += _board_args(b['name'], chosen)
        args[os.path.basename(cfg_path)] = ' '.join(parts)
    return args


def selection_args_by_flasher(sel, rosters):
    """{config: {flasher name: args}}. CI runs one rig as several jobs split by
    flasher (esptool vs the rest); each must gate on its own subset, otherwise the
    other leg runs a filter matching zero boards and reports a vacuous green."""
    out = {}
    for cfg_path, boards in rosters:
        per = {}
        if not sel['full']:
            for b in boards:
                chosen = sel['boards'].get(b['name'])
                if chosen is None:
                    continue
                per.setdefault(b.get('flasher', {}).get('name', ''), []).extend(
                    _board_args(b['name'], chosen))
        out[os.path.basename(cfg_path)] = {f: ' '.join(p) for f, p in per.items()}
    return out


def changed_files_from_git(base, repo_root):
    mb = subprocess.run(['git', 'merge-base', 'HEAD', base], cwd=repo_root,
                        capture_output=True, text=True, check=True).stdout.strip()
    diff = subprocess.run(GIT_DIFF_ARGV + [f'{mb}..HEAD'], cwd=repo_root,
                          capture_output=True, text=True, check=True).stdout
    return [l for l in diff.splitlines() if l.strip()]


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    g = ap.add_mutually_exclusive_group(required=True)
    g.add_argument('--base', help='git ref to diff against (merge-base..HEAD)')
    g.add_argument('--diff-file', help='newline-separated changed-file list')
    ap.add_argument('configs', nargs='+', help='rig roster JSON file(s)')
    a = ap.parse_args()

    repo_root = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    rosters = []
    for c in a.configs:
        with open(c) as f:
            rosters.append((c, json.load(f)['boards']))

    files = (open(a.diff_file).read().splitlines() if a.diff_file
             else changed_files_from_git(a.base, repo_root))
    files = [f for f in files if f.strip()]

    s = classify(files, repo_root, rosters)
    s['args'] = selection_args(s, rosters)
    s['args_flasher'] = selection_args_by_flasher(s, rosters)
    for r in s['reasons']:
        print(f'hil_select: {r}', file=sys.stderr)
    print(json.dumps(s))


if __name__ == '__main__':
    main()
