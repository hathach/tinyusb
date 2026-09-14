#!/usr/bin/env python3
"""Build TinyUSB examples for the boards a change affects, or for named boards.

  build.py (--scope PATH... | --base REF | --board B...) [-e role/name]... [-T target]... [--shared]

Scope resolution goes through tools/ci_select.py: one board per affected family
(a rig-roster board of that family first, else the first in hw/bsp/<family>/boards),
or the representative pair when the selection is the full matrix. Each board builds
through tools/build.py in a private cmake-build-agent-<pid> dir; --shared uses the
canonical cmake-build-<board> that HIL flashes from and must not be shared with a
parallel agent. Dependencies the family needs (get_deps.py's table) are checked first:
a missing one is an error naming the remedy, or fetched when --fetch-deps is given.

stdout ends with one JSON line: {"pass", "boards": [{"board", "family", "buildDir",
"status", "built" (elfs this run wrote), "firstError"}], "resolution"}. Exit 0 pass, 1 a board failed, 2 usage or
resolution error (nothing to build is an error, never a pass).
"""

import argparse
import json
import os
import re
import subprocess
import sys
import tempfile
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[4]
HIL_CONFIG = ROOT / 'test' / 'hil' / 'tinyusb.json'
FULL_MATRIX_BOARDS = ['stm32f407disco', 'raspberry_pi_pico']
# a compiler/linker diagnostic first; CMake's own error next; never ninja's FAILED: wrapper
DIAGNOSTIC = re.compile(r'^\S+:\d+(?::\d+)?: (?:fatal )?error:|undefined reference to|multiple definition of')
CMAKE_ERROR = re.compile(r'^CMake Error')
BOARD_PATH = re.compile(r'^hw/bsp/([^/]+)/boards/([^/]+)/')
ROW = re.compile(r'^\|\s*(\S+)\s*\|\s*(.+?)\s*\|\s*\x1b\[\d+m(OK|Failed|Skipped)\x1b\[0m', re.M)
sys.path.insert(0, str(ROOT / 'tools'))
import get_deps  # noqa: E402  the dependency table, one source with the fetcher


def family_of(board):
    hits = sorted(p.parent.parent.name for p in ROOT.glob(f'hw/bsp/*/boards/{board}'))
    if len(hits) != 1:
        fail(f'board {board!r} matches {hits or "no"} hw/bsp family')
    return hits[0]


def family_boards(family):
    return sorted(p.name for p in (ROOT / 'hw' / 'bsp' / family / 'boards').iterdir() if p.is_dir())


def changed_paths(base):
    """The branch's changed paths against base, the same set ci_select --base classifies."""
    r = subprocess.run(['git', 'diff', '--name-only', f'{base}...HEAD'], capture_output=True, text=True, cwd=ROOT)
    if r.returncode != 0:
        fail(f'git diff against {base} failed:\n{r.stderr.strip()}')
    return r.stdout.split()


def select(scope=None, base=None, config=HIL_CONFIG):
    """ci_select's JSON for a path list or, with --base, for the branch diff: only the
    base form sees dependency revision changes (get_deps.py table edits)."""
    if base:
        cmd = [sys.executable, str(ROOT / 'tools' / 'ci_select.py'), '--base', base, str(config)]
    else:
        with tempfile.NamedTemporaryFile('w', suffix='.txt', delete=False) as f:
            f.write('\n'.join(scope) + '\n')
        cmd = [sys.executable, str(ROOT / 'tools' / 'ci_select.py'), '--diff-file', f.name, str(config)]
    r = subprocess.run(cmd, capture_output=True, text=True, cwd=ROOT)
    if r.returncode != 0:
        fail(f'ci_select failed:\n{r.stderr.strip()}')
    return json.loads(r.stdout.splitlines()[-1])


def boards_for(selection, scope=()):
    """One board per affected family: a board whose own hw/bsp dir is in the scope,
    else a rig-roster board of the family, else the first under hw/bsp/<family>/boards.
    The representative pair for the full matrix. Zero families is an error: a scope
    that builds nothing must never pass."""
    build = selection['build']
    changed = {m.group(2): m.group(1) for m in map(BOARD_PATH.match, scope) if m}
    if build['full']:
        return list(dict.fromkeys(FULL_MATRIX_BOARDS + sorted(changed))), \
            'full matrix' + (f', changed boards {sorted(changed)}' if changed else '')
    if not build['families']:
        fail('the scope selects no build family; name a board with --board if it must build anyway')
    rig = {b: family_of(b) for b in selection.get('boards', {})}
    boards = []
    for fam in build['families']:
        own = sorted(b for b, f in changed.items() if f == fam)
        on_rig = sorted(b for b, f in rig.items() if f == fam)
        candidates = own or on_rig or family_boards(fam)
        if not candidates:
            fail(f'family {fam} has no boards under hw/bsp/{fam}/boards')
        boards.extend(own or candidates[:1])
    return boards, f'one board per family {build["families"]}' + (f', changed boards {sorted(changed)}' if changed else '')


def missing_deps(family):
    needed = list(get_deps.deps_mandatory) + \
        [d for d, entry in get_deps.deps_optional.items() if family in entry[2].split()]
    return [d for d in needed if not (ROOT / d).is_dir() or not any((ROOT / d).iterdir())]


def ensure_deps(family, fetch, verbose):
    missing = missing_deps(family)
    if missing and fetch:
        run([sys.executable, str(ROOT / 'tools' / 'get_deps.py'), family], verbose)
        missing = missing_deps(family)
    if missing:
        fail(f'family {family} is missing dependencies: {", ".join(missing)}. In a worktree symlink them '
             f'from the primary checkout; otherwise rerun with --fetch-deps (python3 tools/get_deps.py {family})')


def build_one(board, examples, targets, shared, fetch, verbose):
    family = family_of(board)
    ensure_deps(family, fetch, verbose)
    name = board if shared else f'agent-{os.getpid()}-{board}'
    cmd = [sys.executable, str(ROOT / 'tools' / 'build.py'), '-b', board]
    if not shared:
        cmd += ['--build-name', name]
    for e in examples:
        cmd += ['-e', e]
    for t in targets:
        cmd += ['-T', t]
    started = time.time()
    rc, out = run(cmd, verbose)
    rows = ROW.findall(out)
    statuses = [s for _, _, s in rows]
    build_dir = f'cmake-build/cmake-build-{name}'
    if not rows:
        status, first = 'error', (out.strip().splitlines() or ['tools/build.py produced no result rows'])[-1]
    elif all(s == 'Skipped' for s in statuses):
        status, first = 'skipped', 'no example was built for this board (all rows Skipped)'
    elif 'Failed' in statuses or rc != 0:
        status, first = 'failed', first_error(out) or rows[-1][1]
    else:
        status, first = 'ok', ''
    # only artifacts this invocation wrote: a shared dir keeps older examples' elfs
    built = sum(1 for e in (ROOT / build_dir).rglob('*.elf') if e.stat().st_mtime >= started) \
        if (ROOT / build_dir).is_dir() else 0
    return {'board': board, 'family': family, 'buildDir': build_dir, 'status': status,
            'built': built, 'firstError': first}


def first_error(out):
    lines = [l.strip() for l in out.splitlines()]
    return next((l for l in lines if DIAGNOSTIC.search(l)), None) or \
        next((l for l in lines if CMAKE_ERROR.match(l)), None)


def run(cmd, verbose):
    r = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, cwd=ROOT)
    if verbose:
        sys.stderr.write(r.stdout)
    return r.returncode, r.stdout


def fail(message):
    sys.stderr.write(f'error: {message}\n')
    sys.exit(2)


def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    how = p.add_mutually_exclusive_group(required=True)
    how.add_argument('--scope', nargs='+', metavar='PATH', help='changed paths, repo-relative')
    how.add_argument('--base', metavar='REF', help='resolve the scope from git diff against REF')
    how.add_argument('--board', action='append', default=None, help='build this board (repeatable)')
    p.add_argument('-e', '--example', action='append', default=[], help='only these examples (role/name)')
    p.add_argument('-T', '--target', action='append', default=[], help='build target (default all)')
    p.add_argument('--fetch-deps', action='store_true', help='run tools/get_deps.py for a family whose deps are missing')
    p.add_argument('--shared', action='store_true',
                   help='build in the canonical cmake-build-<board> HIL dir instead of a private one')
    p.add_argument('--config', default=str(HIL_CONFIG), help='rig roster for ci_select (default: tinyusb.json)')
    p.add_argument('-v', '--verbose', action='store_true', help='stream build output to stderr')
    a = p.parse_args(argv)

    if a.board:
        boards, how_resolved = a.board, 'named boards'
    else:
        sel = select(scope=a.scope, base=a.base, config=Path(a.config))
        boards, how_resolved = boards_for(sel, a.scope if a.scope is not None else changed_paths(a.base))
    results = [build_one(b, a.example, a.target, a.shared, a.fetch_deps, a.verbose) for b in boards]
    ok = all(r['status'] == 'ok' for r in results)
    print(json.dumps({'pass': ok, 'boards': results, 'resolution': how_resolved}))
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
