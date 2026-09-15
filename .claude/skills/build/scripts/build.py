#!/usr/bin/env python3
"""Build TinyUSB examples for the boards a change affects, or for named boards.

  build.py (--scope PATH... | --base REF | --board B...) [-e role/name]... [-T target]...
           [-D SYMBOL]... [--cflag FLAG]... [--shared]

Scope resolution goes through tools/ci_select.py: one board per affected family
(a rig-roster board of that family first, else the first in hw/bsp/<family>/boards,
preferring one that builds an example the change affects), or the representative pair
when the selection is the full matrix. Each board builds through tools/build.py in a
private cmake-build-agent-<pid> dir; --shared uses the canonical cmake-build-<board>
that HIL flashes from and must not be shared with a parallel agent. Dependencies the family needs (get_deps.py's table) are checked first:
a missing one is an error naming the remedy, or fetched when --fetch-deps is given.

stdout ends with one JSON line: {"pass", "boards": [{"board", "family", "buildDir",
"status", "built" and "okExamples" (elfs this run wrote), "firstError"}], "resolution", "nothingToBuild",
"uncovered"}. A scope path nothing builds is one of two kinds, in ci_select's words:
"nothingToBuild" is nothing to verify (docs, .claude/, unit tests, HIL harness);
"uncovered" is firmware no board's build compiled (a class no example enables, a lib
nothing builds, a port whose family no built board has, an example no built board
wrote an elf for), a coverage gap whatever else built.
Exit 0 pass, 1 a board failed, 2 usage or resolution error ("error" carries the
message, a missing dependency's remedy included), 3 uncovered paths.
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
import importlib.util  # noqa: E402
_spec = importlib.util.spec_from_file_location('tools_build', ROOT / 'tools' / 'build.py')
tools_build = importlib.util.module_from_spec(_spec)  # `import build` here is this file
_spec.loader.exec_module(tools_build)


def family_of(board):
    hits = sorted(p.parent.parent.name for p in ROOT.glob(f'hw/bsp/*/boards/{board}'))
    if len(hits) != 1:
        fail(f'board {board!r} matches {hits or "no"} hw/bsp family')
    return hits[0]


def family_boards(family):
    d = ROOT / 'hw' / 'bsp' / family / 'boards'
    return sorted(p.name for p in d.iterdir() if p.is_dir()) if d.is_dir() else []


def expand_scope(scope):
    """The files of every directory in the scope, new ones included. ci_select and the
    changed-board rule both classify file paths: a bare directory matches neither, so a
    board directory would resolve to its family's sample instead of the board itself.
    Untracked-but-not-ignored files count, because the work being verified is usually
    uncommitted and a new board or driver file would otherwise be classified away."""
    out = []
    for p in scope:
        if (ROOT / p).is_dir():
            r = subprocess.run(['git', 'ls-files', '-z', '--cached', '--others', '--exclude-standard', '--', p],
                               capture_output=True, text=True, cwd=ROOT)
            if r.returncode != 0:
                fail(f'git ls-files failed for {p}:\n{r.stderr.strip()}')
            files = [f for f in r.stdout.split('\0') if f]
            if not files:
                fail(f'{p} is a directory with no files git reports; name the files to build for')
            out.extend(files)
        else:
            out.append(p)
    return out


def changed_paths(base):
    """The branch's changed paths against base, the same set ci_select --base classifies."""
    r = subprocess.run(['git', 'diff', '--name-only', f'{base}...HEAD'], capture_output=True, text=True, cwd=ROOT)
    if r.returncode != 0:
        fail(f'git diff against {base} failed:\n{r.stderr.strip()}')
    return r.stdout.split()


def select(scope=None, base=None, config=HIL_CONFIG):
    """(ci_select's JSON, its per-path build-axis reasons) for a path list or, with
    --base, for the branch diff: only the base form sees dependency revision changes
    (get_deps.py table edits)."""
    if base:
        cmd = [sys.executable, str(ROOT / 'tools' / 'ci_select.py'), '--base', base, str(config)]
    else:
        with tempfile.NamedTemporaryFile('w', suffix='.txt', delete=False) as f:
            f.write('\n'.join(scope) + '\n')
        cmd = [sys.executable, str(ROOT / 'tools' / 'ci_select.py'), '--diff-file', f.name, str(config)]
    r = subprocess.run(cmd, capture_output=True, text=True, cwd=ROOT)
    if r.returncode != 0:
        fail(f'ci_select failed:\n{r.stderr.strip()}')
    reasons = [l.split('ci_select[build]: ', 1)[1] for l in r.stderr.splitlines()
               if l.startswith('ci_select[build]: ')]
    return json.loads(r.stdout.splitlines()[-1]), reasons


def representative(candidates, examples):
    """The family's board to build: the first candidate this build can compile one of
    the affected examples on, else the first. ci_select keeps a family when ANY of its
    boards builds the selection under EITHER build system, so the first candidate can be
    one skipped for every affected example (samd11's cynthion_d11 is skip.txt'd out of
    device/mtp) and verify none of the change. No example list means the family's whole
    set, where any board compiles some of it."""
    def builds_any(board):
        try:
            return any(not tools_build.build_utils.skip_example(e, board) for e in examples)
        except OSError:              # family mid-bring-up, unreadable to the mcu scrape
            return True
    if not examples:
        return candidates[0]
    return next((b for b in candidates if builds_any(b)), candidates[0])


def boards_for(selection, scope=(), reasons=()):
    """One board per affected family: a board whose own hw/bsp dir is in the scope,
    else a rig-roster board of the family, else one under hw/bsp/<family>/boards,
    preferring in either case one that builds an affected example (representative).
    The representative pair for the full matrix, plus one board per family a scope
    path names (a port, bsp or mcu path): the pair stands in for the matrix on core
    code, not on a port it does not contain. No family is not a verdict on its own:
    see coverage() for what the scope's unbuilt paths mean."""
    build = selection['build']
    changed = {m.group(2): m.group(1) for m in map(BOARD_PATH.match, scope) if m}
    # a board the change deletes is still in the diff: drop it, so its family falls
    # through to the rig-roster/first-board pick instead of failing family_of()
    changed = {b: f for b, f in changed.items() if (ROOT / 'hw' / 'bsp' / f / 'boards' / b).is_dir()}
    rig = {b: family_of(b) for b in selection.get('boards', {})}
    changed_note = f', changed boards {sorted(changed)}' if changed else ''
    if build['full']:
        boards = list(dict.fromkeys(FULL_MATRIX_BOARDS + sorted(changed)))
        have = {family_of(b) for b in boards}
        named = sorted(set().union(*(named_families(r) or set() for r in reasons)) - have)
        for fam in named:
            candidates = sorted(b for b, f in rig.items() if f == fam) or family_boards(fam)
            boards.extend(candidates[:1])          # none: a family with no boards, coverage() reports it
        return boards, 'full matrix' + changed_note + (f', named families {named}' if named else '')
    if not build['families']:
        return [], 'no build family'
    boards = []
    fam_ex = build.get('family_examples', {})
    for fam in build['families']:
        own = sorted(b for b, f in changed.items() if f == fam)
        on_rig = sorted(b for b, f in rig.items() if f == fam)
        candidates = own or list(dict.fromkeys(on_rig + family_boards(fam)))
        if not candidates:
            fail(f'family {fam} has no boards under hw/bsp/{fam}/boards')
        boards.extend(own or [representative(candidates, fam_ex.get(fam))])
    return boards, f'one board per family {build["families"]}' + changed_note


FAMILIES_IN = re.compile(r"-> families \[(.*?)\]|: bsp family (\S+)$")
EXAMPLES_IN = re.compile(r"-> \[(.*?)\]$|: example (\S+)$")
ROLE_IN = re.compile(r": core (device|host) stack$")


def _named(pattern, reason):
    m = pattern.search(reason)
    if not m:
        return None
    return set(re.findall(r"'([^']+)'", m.group(1))) if m.group(1) is not None else {m.group(2)}


def named_families(reason):
    return _named(FAMILIES_IN, reason)


def named_examples(reason):
    """Examples the reason names; for a core stack path, every example of that role
    (dual examples run both), since any one of them compiles the stack."""
    m = ROLE_IN.search(reason)
    if m:
        return {f'{role}/{p.name}' for role in (m.group(1), 'dual')
                for p in (ROOT / 'examples' / role).iterdir() if p.is_dir()}
    return _named(EXAMPLES_IN, reason)


def coverage(reasons, scope, results, chosen=False):
    """ci_select's per-path build reasons as (nothing to verify, uncovered firmware),
    judged against what was actually built. 'no build contribution' is its wording
    for non-code paths, and a get_deps.py edit that changes no entry is the same;
    every other 'no contribution' is a class, typec or lib no example enables. A path
    that named families is a gap when none was built: a port mapping to no family, a
    family _prune_buildable dropped (dir gone, boards unreadable, or every example
    filtered, which it drops without a reason line), or a full-matrix run whose pair
    does not contain the port. A path that named examples is a gap when no board wrote
    an elf for any of them; a core stack path names every example of its role; any
    other contributing path is a gap when the run produced no elf at all (-T help is
    a green build of nothing). `chosen` (-e or -T given) hands all that to the caller.
    With no board at all, every path not explained as nothing-to-verify is a gap."""
    built_fams = {r['family'] for r in results}
    ok_ex = set().union(*(set(r.get('okExamples', ())) for r in results)) if results else set()
    benign, gaps = [], []
    for r in reasons:
        fams, exs = named_families(r), named_examples(r)
        if r.endswith('no build contribution') or r.endswith('no dep entry changed, no contribution'):
            benign.append(r)
        elif r.endswith('no contribution') or r.endswith('dropped'):
            gaps.append(r)
        elif fams is not None and not fams & built_fams:
            gaps.append(r)
        elif chosen:
            continue
        elif exs is not None and not {e.split('/', 1)[1] for e in exs} & ok_ex:
            gaps.append(r)
        elif results and not ok_ex:
            gaps.append(r)
    if not results:
        explained = {r.split(': ', 1)[0] for r in benign + gaps}
        for path in scope:
            if path not in explained:
                gaps.append(next((r for r in reasons if r.startswith(path + ': ')),
                                 f'{path}: no build reason from ci_select'))
    return benign, gaps


def missing_deps(family):
    """Present means content, not a directory: get_deps.py git-inits the dep dir before
    fetching and exits 0 whatever the fetch did (its run_cmd's status is ignored), so a
    fetch that failed leaves a dir holding nothing but .git."""
    needed = list(get_deps.deps_mandatory) + \
        [d for d, entry in get_deps.deps_optional.items() if family in entry[2].split()]
    return [d for d in needed if not (ROOT / d).is_dir()
            or not any(p.name != '.git' for p in (ROOT / d).iterdir())]


def ensure_deps(family, fetch, verbose):
    missing = missing_deps(family)
    if missing and fetch:
        run([sys.executable, str(ROOT / 'tools' / 'get_deps.py'), family], verbose)
        missing = missing_deps(family)
    if missing:
        fail(f'family {family} is missing dependencies: {", ".join(missing)}. In a worktree symlink them '
             f'from the primary checkout; otherwise rerun with --fetch-deps (python3 tools/get_deps.py {family})')


def configured(board, family, examples, defines, build_dir, elfs, fresh):
    """After a green build, the elfs it verified. Espressif builds one idf tree per
    example (<dir>/<role>/<example>): the examples this run attempted are decided by
    the same functions tools/build.py uses, and a shared dir's tree for one it skipped
    proves nothing. Every other family is one CMake tree whose registered targets say
    which elfs the configuration still builds; unreadable, only elfs written now count.
    -e narrows that further to the examples asked for: a shared dir also keeps the elf
    of one this run never built."""
    if family == 'espressif':
        attempted = {e.split('/', 1)[1] for e in tools_build.get_examples(family)
                     if (not examples or e in examples)
                     and not tools_build.build_utils.skip_example(e, board, defines)}
        return [e for e in elfs if e.parent.name in attempted]
    reg = tools_build.cmake_registered_targets(str(ROOT / build_dir))
    if not reg:
        return fresh
    asked = {e.split('/', 1)[1] for e in examples}
    return [e for e in elfs if e.stem in reg and (not asked or e.stem in asked)]


def build_one(board, examples, targets, defines, cflags, shared, fetch, verbose):
    family = family_of(board)
    # tools/build.py hands -D to cmake but not to idf.py, so a define would be
    # dropped and the build would pass without the configuration it was asked for
    if defines and family == 'espressif':
        fail(f'-D is not forwarded to idf.py, so it cannot configure {board} (family espressif). '
             'A preprocessor macro can go through --cflag; a build-system setting (LOG, LOGGER) has no '
             'path here, since the BSP translates those into other defines')
    ensure_deps(family, fetch, verbose)
    name = board if shared else f'agent-{os.getpid()}-{board}'
    cmd = [sys.executable, str(ROOT / 'tools' / 'build.py'), '-b', board]
    if not shared:
        cmd += ['--build-name', name]
    for e in examples:
        cmd += ['-e', e]
    for t in targets:
        cmd += ['-T', t]
    for d in defines:
        cmd += ['-D', d]
    for f in cflags:
        cmd += [f'--cflag={f}']
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
    # built counts only what this invocation wrote: a shared dir keeps older elfs. A
    # green build leaves every configured target up to date, so an elf it did not
    # relink is verified too, but only if its target is still configured: a shared dir
    # also keeps the elf of an example this configuration no longer builds. -T builds
    # the named targets alone, so then only a fresh elf is evidence of anything.
    elfs = list((ROOT / build_dir).rglob('*.elf')) if (ROOT / build_dir).is_dir() else []
    fresh = [e for e in elfs if e.stat().st_mtime >= started]
    verified = configured(board, family, examples, defines, build_dir, elfs, fresh) \
        if status == 'ok' and not targets else fresh
    return {'board': board, 'family': family, 'buildDir': build_dir, 'status': status,
            'built': len(fresh), 'okExamples': sorted({e.stem for e in verified}), 'firstError': first}


def first_error(out):
    lines = [l.strip() for l in out.splitlines()]
    return next((l for l in lines if DIAGNOSTIC.search(l)), None) or \
        next((l for l in lines if CMAKE_ERROR.match(l)), None)


def run(cmd, verbose):
    r = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, cwd=ROOT)
    if verbose:
        sys.stderr.write(r.stdout)
    return r.returncode, r.stdout


class Parser(argparse.ArgumentParser):
    def error(self, message):  # usage errors reach the caller the same way as resolution errors
        self.print_usage(sys.stderr)
        fail(message)


def fail(message):
    """Usage or resolution error, exit 2. The JSON line carries it too, so a caller
    that reads stdout only (a workflow summary) can quote a missing-dependency remedy."""
    sys.stderr.write(f'error: {message}\n')
    print(json.dumps({'pass': False, 'boards': [], 'error': message}))
    sys.exit(2)


def main(argv=None):
    p = Parser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    how = p.add_mutually_exclusive_group(required=True)
    how.add_argument('--scope', nargs='+', metavar='PATH',
                     help='changed paths or directories, repo-relative')
    how.add_argument('--base', metavar='REF', help='resolve the scope from git diff against REF')
    how.add_argument('--board', action='append', default=None, help='build this board (repeatable)')
    p.add_argument('-e', '--example', action='append', default=[], help='only these examples (role/name)')
    p.add_argument('-T', '--target', action='append', default=[], help='build target (default all)')
    p.add_argument('-D', '--define', action='append', default=[],
                   help='build-system define, e.g. -D LOG=2 (repeatable)')
    p.add_argument('--cflag', action='append', default=[],
                   help='raw compiler flag, e.g. --cflag=-DCFG_TUH_CDC_FTDI_LATENCY=16, to compile a '
                        'config-guarded branch no example enables (repeatable)')
    p.add_argument('--fetch-deps', action='store_true', help='run tools/get_deps.py for a family whose deps are missing')
    p.add_argument('--shared', action='store_true',
                   help='build in the canonical cmake-build-<board> HIL dir instead of a private one')
    p.add_argument('--config', default=str(HIL_CONFIG), help='rig roster for ci_select (default: tinyusb.json)')
    p.add_argument('-v', '--verbose', action='store_true', help='stream build output to stderr')
    a = p.parse_args(argv)
    os.chdir(ROOT)  # tools/build.py's example listing reads examples/ relative to the root

    extra = {}
    if a.board:
        boards, how_resolved = a.board, 'named boards'
    else:
        paths = expand_scope(a.scope) if a.scope is not None else changed_paths(a.base)
        sel, reasons = select(scope=paths if a.scope is not None else None, base=a.base, config=Path(a.config))
        boards, how_resolved = boards_for(sel, paths, reasons)
    results = [build_one(b, a.example, a.target, a.define, a.cflag, a.shared, a.fetch_deps, a.verbose)
               for b in boards]
    built_ok = all(r['status'] == 'ok' for r in results)
    if not a.board:
        # an uncovered path fails the scope even when every board built green: a class
        # driver plus the core file that registers it is the common shape
        extra['nothingToBuild'], extra['uncovered'] = coverage(reasons, paths, results, bool(a.example or a.target))
    ok = built_ok and not extra.get('uncovered')
    print(json.dumps({'pass': ok, 'boards': results, 'resolution': how_resolved, **extra}))
    return 0 if ok else (1 if not built_ok else 3)


if __name__ == '__main__':
    sys.exit(main())
