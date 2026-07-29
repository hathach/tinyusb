#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""PR-diff -> HIL selection: which rig boards and which tests a change can affect.

Stdlib-only (runs on bare CI runners; never imports hil_test/hil_flash/hil_lock).
Fail-open: any file no rule classifies forces the full matrix. See
docs/superpowers/specs/2026-07-29-hil-pr-scoped-selection-design.md.
"""
import argparse
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
    r'examples/build_system/|examples/CMakeLists\.txt$)')


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


def board_family(board_name: str, repo_root: str):
    hits = glob.glob(os.path.join(repo_root, 'hw/bsp/*/boards', board_name))
    return os.path.basename(os.path.dirname(os.path.dirname(hits[0]))) if hits else None


def port_families(port_dir: str, repo_root: str) -> set:
    fams = set()
    bsp_root = os.path.join(repo_root, 'hw/bsp')
    # family.cmake/family.mk list portable sources directly for most families;
    # espressif instead references them from a nested component CMakeLists.txt
    # (hw/bsp/espressif/components/tinyusb_src/CMakeLists.txt) - scan both.
    for f in glob.glob(os.path.join(bsp_root, '*/family.cmake')) + \
             glob.glob(os.path.join(bsp_root, '*/family.mk')) + \
             glob.glob(os.path.join(bsp_root, '*/components/*/CMakeLists.txt')):
        try:
            if port_dir in open(f).read():
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
        boards = [b['name'] for b in roster_boards
                  if board_family(b['name'], repo_root) in fams and (board_roles(b) & roles)]
        tests = role_tests(roles, extras)
        s.roles.update(roles)
        s.add(boards, tests, f'{path}: port {port} -> families {sorted(fams)} -> boards {boards} ({"/".join(sorted(roles))})')
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
    for path in changed_files:
        _classify_one(path, repo_root, all_boards, extras, s)
        if s.full:
            break

    if s.full:
        return {'full': True, 'boards': {b['name']: 'all' for b in all_boards},
                'reasons': s.reasons}

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
    return {'full': False, 'boards': out, 'reasons': s.reasons}


def selection_args(sel, rosters):
    args = {}
    for cfg_path, boards in rosters:
        key = os.path.basename(cfg_path)
        if sel['full']:
            args[key] = ''
            continue
        parts = []
        for b in boards:
            chosen = sel['boards'].get(b['name'])
            if chosen is None:
                continue
            parts.append(f'-b {b["name"]}')
            if chosen != 'all':
                parts.append(f'-bt {b["name"]}:{",".join(chosen)}')
        args[key] = ' '.join(parts)
    return args


def changed_files_from_git(base, repo_root):
    mb = subprocess.run(['git', 'merge-base', 'HEAD', base], cwd=repo_root,
                        capture_output=True, text=True, check=True).stdout.strip()
    diff = subprocess.run(['git', 'diff', '--name-only', f'{mb}..HEAD'], cwd=repo_root,
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
    for r in s['reasons']:
        print(f'hil_select: {r}', file=sys.stderr)
    print(json.dumps(s))


if __name__ == '__main__':
    main()
