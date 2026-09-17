#!/usr/bin/env python3
"""Build TinyUSB examples for the boards a change affects, or for named boards.

  check_build.py (--scope PATH... | --base REF | --board B...) [-e role/name]... [-T target]...
           [-D SYMBOL]... [--cflag FLAG]... [--shared]

Scope resolution goes through tools/ci_select.py: one board per affected family
(a rig-roster board of that family first, else the first in hw/bsp/<family>/boards,
preferring one that builds an example the change affects), plus one more board per
changed driver none of them compiles by its hw/bsp/family.json row, or the
representative pair when the selection is the full matrix. Each board builds through
tools/build.py in a private cmake-build-agent-<pid> dir; --shared uses the canonical
cmake-build-<board> that HIL flashes from, must not be shared with a parallel agent,
and is refused when it still carries an option from an earlier configure this run does not set.
Dependencies the family needs (get_deps.py's table) are checked first: one missing,
empty or not at the pinned commit is an error naming the remedy, or fetched when
--fetch-deps is given.

stdout ends with one JSON line: {"pass", "boards": [{"board", "family", "buildDir",
"status", "built" and "okExamples" (elfs this run wrote), "firstError", "familyJson"
(tools/build.py's line on the board's catalog row, when the configure was the default
one)}], "resolution", "nothingToBuild", "uncovered", "familyJsonChanged" (a build
rewrote hw/bsp/family.json; commit it with the change)}. A scope path nothing builds is one of two kinds, in ci_select's words:
"nothingToBuild" is nothing to verify (docs, .claude/, unit tests, HIL harness);
"uncovered" is firmware no board's build compiled (a class no example enables, a lib
nothing builds, a port no built board kept a line of after preprocessing - wrong
family, or a configuration whose guard empties the driver - an example no built board wrote an elf for, or a
build target the default sweep never runs), a coverage gap whatever else built.
Exit 0 pass, 1 a board failed, 2 usage or resolution error ("error" carries the
message, a missing dependency's remedy included), 3 uncovered paths.
"""

import argparse
import functools
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
import build as tools_build  # noqa: E402  tools/build.py, first on the path above
import ci_select  # noqa: E402  the same classifier run below, here for its option knowledge
import family_json  # noqa: E402  hw/bsp/family.json: what each board's default configure compiles


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
        elif (ROOT / p).exists() or tracked(p):
            out.append(p)
        else:
            # ci_select classifies an unknown path as the full matrix, whose pair then
            # covers it: a typo would come back green for a path no change touched
            fail(f'{p} does not exist and is not a tracked file; only a path present in the '
                 f'tree or deleted from it can be in the scope')
    return out


def tracked(path):
    """Whether HEAD holds exactly this path: a deletion, staged or not, is a change to
    build for. Not a pathspec lookup, which would let `src/*.c` through as a match."""
    r = subprocess.run(['git', 'cat-file', '-e', f'HEAD:{path}'], capture_output=True, text=True, cwd=ROOT)
    return r.returncode == 0


def changed_paths(base):
    """The branch's changed paths against base, the same set ci_select --base classifies.
    --no-renames as ci_select does: rename detection reports only a rename's destination,
    so the board or port a file moved out of would never be built."""
    r = subprocess.run(['git', 'diff', '--no-renames', '--name-only', f'{base}...HEAD'],
                       capture_output=True, text=True, cwd=ROOT)
    if r.returncode != 0:
        fail(f'git diff against {base} failed:\n{r.stderr.strip()}')
    return r.stdout.split()


GET_DEPS = 'tools/get_deps.py'
DEPS_UNRESOLVED = f'{GET_DEPS}: dep changes not resolvable -> full build matrix'


def worktree_dep_reasons():
    """ci_select's rule 16b answer for a get_deps.py edit the working tree carries, the one
    a path list cannot reach: --diff-file mode has no base blob of the file, so the rule
    falls open to the full matrix and boards_for stands the representative pair in for it -
    an nRF-only pin bump would report green with no nrf board built. HEAD is the base the
    scope form has, and ci_select classifies the entries changed against it. A file that
    matches HEAD carries its change in a commit instead, where only --base can say which
    entries it touched, so the run is refused rather than answered from the pair. A change
    the entry diff cannot resolve (a logic change to get_deps.py itself) keeps ci_select's
    full-matrix reason, the fail-open the pair does stand in for."""
    r = subprocess.run(['git', 'show', f'HEAD:{GET_DEPS}'], capture_output=True, text=True, cwd=ROOT)
    try:
        work = (ROOT / GET_DEPS).read_text(encoding='utf-8', errors='replace')
    except OSError:
        work = None
    if r.returncode != 0 or work is None or work == r.stdout:
        fail(f'{GET_DEPS} is in the scope with no working-tree change to read its dep entries off, so '
             f'the families the edit affects cannot be resolved here and the scope would build the '
             f'representative pair alone, verifying no dependency: rerun with --base <ref> for a dep '
             f'bump that is already committed')
    fams = ci_select.get_deps_changed_families(r.stdout, work, str(ROOT))
    if fams is None:                 # a logic change to get_deps itself: every family, ci_select's fail-open
        return [DEPS_UNRESOLVED]
    return ci_select.classify_build([GET_DEPS], str(ROOT), fams)['reasons']


def select(scope=None, base=None, config=HIL_CONFIG):
    """(ci_select's JSON, its per-path build-axis reasons) for a path list or, with
    --base, for the branch diff. Only the base form sees a dependency revision change
    (a get_deps.py table edit) through ci_select; for the scope form the uncommitted one
    is resolved here (worktree_dep_reasons)."""
    selector = [sys.executable, str(ROOT / 'tools' / 'ci_select.py')]
    with tempfile.TemporaryDirectory() as tmp:   # the path list goes away with it, fail() included
        if base:
            cmd = selector + ['--base', base, str(config)]
        else:
            listing = Path(tmp) / 'scope.txt'
            listing.write_text('\n'.join(scope) + '\n')
            cmd = selector + ['--diff-file', str(listing), str(config)]
        r = subprocess.run(cmd, capture_output=True, text=True, cwd=ROOT)
    if r.returncode != 0:
        fail(f'ci_select failed:\n{r.stderr.strip()}')
    reasons = [l.split('ci_select[build]: ', 1)[1] for l in r.stderr.splitlines()
               if l.startswith('ci_select[build]: ')]
    if not base and DEPS_UNRESOLVED in reasons:
        resolved = worktree_dep_reasons()
        reasons = [x for r in reasons for x in (resolved if r == DEPS_UNRESOLVED else [r])]
    return json.loads(r.stdout.splitlines()[-1]), reasons


def representatives(candidates, examples, drivers=(), keep=()):
    """The family's boards to build: the boards in `keep` (the ones the change edits),
    plus one candidate per changed driver none of them compiles (its catalog row lists
    the driver, selects the USB IPs its guard names where the probe can say, and its row
    and own cmake turn on no option the guard negates and leave off none of the per-board
    ones it requires: hcd_dwc2.c is empty on a MAX3421 board, hcd_rp2040.c on a PIO-USB
    one, hcd_pio_usb.c on a board that is neither), plus a
    plain first pick when that leaves nothing. A candidate that compiles one of the affected examples
    is preferred, the criterion dropped rather than returning nothing when it leaves no
    candidate: ci_select keeps a family when ANY of its boards builds the selection under
    EITHER build system, so the first candidate can be one skipped for every affected
    example (samd11's cynthion_d11 is skip.txt'd out of device/mtp) and verify none of the
    change.
    One board is not enough for a family carrying two IPs: every stm32l4 board but
    stm32l412nucleo is DWC2 while the family's family.cmake also compiles fsdev, so a
    scope changing both drivers needs a board apiece or one of them preprocesses away to
    nothing. A driver no candidate compiles adds no board - then no board of the family
    compiles it in its default configuration (analog/max3421 is in a row's portable list
    only for a board whose own board.cmake turns MAX3421_HOST on), and coverage() reports
    the path as uncovered instead of the run going green on a body it never saw.
    No example list means the family's whole set, where any board compiles some of it."""
    def builds_any(board):
        try:
            return any(not tools_build.build_utils.skip_example(e, board) for e in examples)
        except OSError:              # family mid-bring-up, unreadable to the mcu scrape
            return True
    pool = list(candidates)
    if examples:
        pool = [b for b in pool if builds_any(b)] or pool
    picked = list(keep)
    for driver in sorted(drivers):
        if any(compiles(b, driver) for b in picked):
            continue
        hit = next((b for b in pool if compiles(b, driver)), None)
        if hit:
            picked.append(hit)
    return picked or [pool[0]]


USBIP_TERM = re.compile(r'^\s*defined\s*\(\s*(TUP_USBIP_\w+)\s*\)\s*$')
# a CFG_ option a guard negates, either way the drivers write it: !CFG_TUH_MAX3421,
# or hcd_samd.c's !(defined(CFG_TUH_MAX3421) && CFG_TUH_MAX3421)
OFF_TERM = re.compile(r'!\s*(?:CFG_(\w+)|\(\s*defined\s*\(\s*CFG_(\w+)\s*\)\s*&&\s*CFG_\2\s*\))')
# a CFG_ option a guard requires on: a plain conjunct, as hcd_pio_usb.c names
# CFG_TUH_RPI_PIO_USB. A defined() test, a comparison and a negated term are not that
ON_TERM = re.compile(r'^\s*(CFG_\w+)\s*$')
LINE_MARK = re.compile(r'^# \d+ "([^"]*)"')
BOARD_CMAKE_SET = re.compile(r'^\s*set\s*\(\s*(CFG_\w+)\s+([^\s)]*)', re.M | re.I)
CMAKE_FALSE = {'', '0', 'OFF', 'NO', 'FALSE', 'N', 'IGNORE', 'NOTFOUND'}


@functools.lru_cache(maxsize=None)
def source_usbips(src):
    """What one driver's body needs defined, read off the `defined(TUP_USBIP_*)` conjuncts
    of the first #if guard naming one (a negated or bracketed term is not a conjunct and
    is left out). Empty for a driver no TUP_USBIP gates (rp2040, nrf5x), which its family
    compiles outright."""
    try:
        text = Path(src).read_text(encoding='utf-8', errors='replace')
    except OSError:                  # a file the change deletes is still in the diff
        return frozenset()
    for line in text.splitlines():
        if not line.startswith('#if'):
            continue
        req = frozenset(m.group(1) for m in map(USBIP_TERM.match, line[3:].split('&&')) if m)
        if req:
            return req
    return frozenset()


@functools.lru_cache(maxsize=None)
def guard_of(src):
    """One driver's first #if line, which is the guard in every portable source."""
    try:
        text = Path(src).read_text(encoding='utf-8', errors='replace')
    except OSError:                  # a file the change deletes is still in the diff
        return ''
    return next((l for l in text.splitlines() if l.startswith('#if')), '')


@functools.lru_cache(maxsize=None)
def source_off_options(src):
    """The CFG_ options one driver's guard requires off, read off its negated conjuncts.
    A board whose row turns one on compiles the file to an empty translation unit whatever
    its portable list says: hcd_dwc2.c and hcd_rp2040.c are excluded by CFG_TUH_MAX3421,
    so a MAX3421 board must not be the family's representative for them."""
    return frozenset('CFG_' + (m.group(1) or m.group(2)) for m in OFF_TERM.finditer(guard_of(src)))


@functools.lru_cache(maxsize=None)
def source_on_options(src):
    """The CFG_ options one driver's guard requires on, read off its plain conjuncts: the
    mirror of source_off_options, since a board that leaves CFG_TUH_RPI_PIO_USB off
    compiles hcd_pio_usb.c to an empty translation unit just as a MAX3421 one does
    hcd_rp2040.c. Only a conjunct some board.cmake of the family sets can reject a
    candidate (family_cmake_options)."""
    return frozenset(m.group(1) for m in map(ON_TERM.match, guard_of(src)[3:].split('&&')) if m)


@functools.lru_cache(maxsize=None)
def drivers_of(path):
    """The drivers a changed src/portable path stands for, relative to src/portable as a
    catalog row lists them: itself when it is a driver, else every driver in its dir (a
    header they all include)."""
    p, base = ROOT / path, ROOT / 'src' / 'portable'
    if p.suffix == '.c':
        return (p.relative_to(base).as_posix(),)
    return tuple(sorted(c.relative_to(base).as_posix() for c in p.parent.glob('*.c')))


@functools.lru_cache(maxsize=None)
def catalog():
    try:
        return family_json.load()
    except family_json.Failure as e:
        fail(str(e))


def row_of(board):
    """The board's default-configure row, None for a Make-only board or one the catalog
    lacks (pre-commit refuses such a tree; here it reads as nothing known)."""
    return (catalog().get(family_of(board), {}).get(board) or {}).get('cmake')


@functools.lru_cache(maxsize=None)
def board_usbips(board):
    """The TUP_USBIP_* a board's MCU selects, its row put to the host preprocessor:
    tusb_mcu.h keys them off CFG_TUSB_MCU and then, for a family carrying two IPs, off
    the device-model define the row's `defines` carry. None is 'cannot say': no row, no
    host cc, an MCU branch that includes an SDK header (imxrt, lpc54)."""
    row = row_of(board)
    if not row:
        return None
    ips, _ = family_json.usbips(family_json.host_probe_argv(row))
    return ips


@functools.lru_cache(maxsize=None)
def board_cmake_options(board):
    """The CFG_ options a board's own board.cmake turns on. A row's `defines` hold only
    what tusb_mcu.h keys on, so an option that gates a driver's guard without reaching
    tusb_mcu.h is absent from it: adafruit_fruit_jam sets CFG_TUH_RPI_PIO_USB, which
    hcd_rp2040.c's guard negates, and its row's `defines` are empty. Scraped rather than
    observed, which only a selection may do - a candidate is rejected, never accepted, on
    this, and the body test after the build is still the verdict."""
    path = ROOT / 'hw' / 'bsp' / family_of(board) / 'boards' / board / 'board.cmake'
    try:
        text = path.read_text(encoding='utf-8', errors='replace')
    except OSError:                  # Make-only board, or a family whose cmake is elsewhere
        return frozenset()
    return frozenset(m.group(1) for m in BOARD_CMAKE_SET.finditer(text)
                     if m.group(2).strip('"').upper() not in CMAKE_FALSE)


@functools.lru_cache(maxsize=None)
def family_cmake_options(family):
    """The CFG_ options some board.cmake of the family turns on: the per-board knobs a
    guard's positive conjunct can be held against. A conjunct outside that set says
    nothing about one board of the family - CFG_TUH_ENABLED comes from the example's
    tusb_config.h, CFG_TUH_MAX3421 from family.cmake's own MAX3421_HOST translation - and
    requiring it would reject every candidate."""
    return frozenset().union(*(board_cmake_options(b) for b in family_boards(family)))


def compiles(board, driver):
    """Whether the board's default configuration compiles src/portable/<driver> with every USB-IP
    conjunct its guard names selected, every per-board option it requires on, and none of
    the options it negates defined, as far as the row, the board's own cmake and the probe
    can say: an unknown probe forbids nothing here, the body test after the build decides."""
    row = row_of(board)
    if not row or driver not in row['portable']:
        return False
    src = str(ROOT / 'src' / 'portable' / driver)
    on = {d for d, v in (row.get('defines') or {}).items() if str(v) != '0'} | board_cmake_options(board)
    if on & source_off_options(src):
        return False
    if (source_on_options(src) & family_cmake_options(family_of(board))) - on:
        return False
    ips = board_usbips(board)
    return ips is None or source_usbips(src) <= ips


def boards_for(selection, scope=(), reasons=()):
    """One board per affected family, plus one more per changed driver the first does not
    compile: a board whose own hw/bsp dir is in the scope, else a rig-roster board of the
    family, else one under hw/bsp/<family>/boards, and then a board per USB-IP requirement
    set of the changed ports, and one turning on the build option an option-gated port
    needs, that none of those selects (representatives).
    The representative pair for the full matrix, plus boards for every family a scope
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
    drivers = family_drivers(reasons)
    if build['full']:
        boards = list(dict.fromkeys(FULL_MATRIX_BOARDS + sorted(changed)))
        have = {}
        for b in boards:
            have.setdefault(family_of(b), []).append(b)
        added = []
        for fam in sorted(set().union(*(named_families(r) or set() for r in reasons))):
            # the whole family, not the rig roster alone: the roster hides the one board
            # that selects the changed IP (stm32l4 is stm32l476disco on the rig, DWC2,
            # while only stm32l412nucleo is fsdev), and a family already in the pair still
            # needs a second board when the pair's does not compile the changed driver
            candidates = list(dict.fromkeys(have.get(fam, []) +
                                            sorted(b for b, f in rig.items() if f == fam) +
                                            family_boards(fam)))
            if not candidates:                     # a family with no boards, coverage() reports it
                continue
            picked = [b for b in representatives(candidates, None, drivers.get(fam, ()), have.get(fam, []))
                      if b not in boards]
            boards += picked
            if picked:
                added.append(fam)
        return boards, 'full matrix' + changed_note + (f', named families {added}' if added else '')
    if not build['families']:
        return [], 'no build family'
    boards = []
    fam_ex = build.get('family_examples', {})
    for fam in build['families']:
        own = sorted(b for b, f in changed.items() if f == fam)
        on_rig = sorted(b for b, f in rig.items() if f == fam)
        candidates = list(dict.fromkeys(own + on_rig + family_boards(fam)))
        if not candidates:
            fail(f'family {fam} has no boards under hw/bsp/{fam}/boards')
        boards.extend(representatives(candidates, fam_ex.get(fam), drivers.get(fam, ()), own))
    return boards, f'one board per family {build["families"]}' + changed_note


FAMILIES_IN = re.compile(r"-> families \[(.*?)\]|: bsp family (\S+)$")
EXAMPLES_IN = re.compile(r"-> \[(.*?)\]$|: example (\S+)$")
ROLE_IN = re.compile(r": core (device|host) stack$")
PORT_IN = re.compile(r": port (\S+) -> families \[")
# ci_select's wording for tools/metrics.py and .github/scripts/metrics_*.py, the one
# build reason that names a cmake target rather than families or examples
METRICS_IN = re.compile(r": metrics tooling runs in the build\b")


def _named(pattern, reason):
    m = pattern.search(reason)
    if not m:
        return None
    return set(re.findall(r"'([^']+)'", m.group(1))) if m.group(1) is not None else {m.group(2)}


def named_families(reason):
    return _named(FAMILIES_IN, reason)


def family_drivers(reasons):
    """family -> the src/portable-relative drivers the scope changed that its picks must compile
    between them. A family carrying two IPs (stm32l4 is fsdev and dwc2, ch32v20x fsdev
    and wch usbhs) is named by a port change whatever its boards are, so without this a
    pick can be a board the changed driver preprocesses away to nothing."""
    out = {}
    for r in reasons:
        if PORT_IN.search(r):
            for fam in named_families(r) or ():
                out.setdefault(fam, set()).update(drivers_of(r.split(': ', 1)[0]))
    return out


def instances(build_dir, driver):
    """(example, compile command) for every translation unit of src/portable/<driver> a build dir
    configured, or None when it has no compile database. One database for a cmake
    tree, whose objects live under <role>/<example>/; one per example tree for
    Espressif."""
    root = (ROOT / build_dir).resolve()
    dbs = [d for d in [root / 'compile_commands.json'] + sorted(root.glob('*/*/compile_commands.json')) if d.is_file()]
    if not dbs:
        return None
    target = (ROOT / 'src' / 'portable' / driver).resolve()
    out = []
    for db in dbs:
        try:
            entries = json.loads(db.read_text(encoding='utf-8'))
        except (OSError, ValueError):
            continue
        for e in entries:
            if Path(e['directory'], e['file']).resolve() != target:
                continue
            if db.parent == root:
                obj = Path(e['directory'], e.get('output', '')).resolve()
                parts = obj.relative_to(root).parts if root in obj.parents else ()
                example = parts[1] if len(parts) > 1 else ''
            else:
                example = db.parent.name
            out.append((example, e))
    return out


def source_lines(entry):
    """How many non-blank, non-directive lines of the entry's own file survive its
    preprocessing, None when the preprocessor run fails: a driver whose guard the
    configuration does not satisfy is an empty translation unit, whatever built."""
    argv = family_json.preprocess_argv(entry) + ['-E', entry['file']]
    try:
        r = subprocess.run(argv, capture_output=True, text=True, cwd=entry['directory'])
    except OSError:
        return None
    if r.returncode != 0:
        return None
    target = Path(entry['directory'], entry['file']).resolve()
    current, n = None, 0
    for line in r.stdout.splitlines():
        m = LINE_MARK.match(line)
        if m:
            current = Path(entry['directory'], m.group(1)).resolve()
        elif current == target and line.strip() and not line.lstrip().startswith('#'):
            n += 1
    return n


def body_verdict(result, driver):
    """'compiled' when some translation unit of the driver, in an example this run
    built, keeps lines of its own after preprocessing; else why not."""
    found = instances(result['buildDir'], driver)
    if found is None:
        return 'unverified: no compile database'
    if not found:
        return 'not compiled in its default configuration'
    ok = set(result.get('okExamples', ()))
    built = [e for ex, e in found if ex in ok]
    if not built:
        return 'compiled only in examples this run did not build'
    failure = None
    for e in built:
        n = source_lines(e)
        if n is None:
            failure = failure or 'unverified: preprocessing failed'
        elif n > 0:
            return 'compiled'
    return failure or 'body preprocessed away'


def port_gap(reason, results):
    """Why the boards built do not cover a changed port path, None when they do: for
    each driver the path stands for, some built board of its families must have compiled
    a translation unit of it, in an example it built, that kept lines of the driver
    after preprocessing. That is the final word, whatever the USB-IP probe said at
    selection: dcd_nrf5x.c is in every nrf build and empty on NRF54, hcd_rp2040.c empty
    under MAX3421. A driver the change deleted has no body to compile."""
    path = reason.split(': ', 1)[0]
    if not PORT_IN.search(reason) or not (ROOT / path).exists():
        return None
    fams = named_families(reason) or set()
    built = [r for r in results if r.get('board') and r['family'] in fams and r.get('buildDir')]
    why = []
    for d in drivers_of(path):
        verdicts = [(r['board'], body_verdict(r, d)) for r in built]
        if any(v == 'compiled' for _, v in verdicts):
            continue
        why.append(f'{d}: ' + ('; '.join(f'{b} {v}' for b, v in verdicts) if verdicts else 'no board of its family built'))
    return '; '.join(why) or None


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
    does not contain the port. A port path is a gap too when the boards built are of its
    families but none compiled the changed body (port_gap): the driver is in no built
    board's default configuration, or its guard preprocessed it away there. A path whose reason names a build target rather than families or examples
    (tools/metrics.py runs as tinyusb_metrics) is a gap whatever built: the default sweep
    builds `all`, which never runs that target. A path that named examples is a
    gap when no board wrote an elf for any of them; a core stack path names every example
    of its role; any other contributing path is a gap when the run produced no elf at all
    (-T help is a green build of nothing). `chosen` (-e or -T given) hands all that to
    the caller, bar a family no board of which was built: narrowing the examples does not
    change which families the scope resolves to. With no board at all, every path not
    explained as nothing-to-verify is a gap."""
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
        elif fams is not None and (gap := port_gap(r, results)):
            gaps.append(f'{r} ({gap})')
        elif METRICS_IN.search(r):
            gaps.append(f'{r} (the default sweep builds `all`, which does not run '
                        f'tinyusb_metrics: rerun with -T all -T tinyusb_metrics)')
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
    """build_utils' pin check, by name so a test can stand in for it."""
    return tools_build.build_utils.missing_deps(family, ROOT)


def ensure_deps(family, fetch, verbose):
    missing = missing_deps(family)
    if missing and fetch:
        run([sys.executable, str(ROOT / 'tools' / 'get_deps.py'), family], verbose)
        missing = missing_deps(family)
    if missing:
        fail(f'family {family} dependencies are not the pinned ones: {", ".join(missing)}. In a worktree '
             f'symlink them from the primary checkout and fetch there, since get_deps.py here checks out a '
             f'revision every other worktree shares; otherwise rerun with --fetch-deps '
             f'(python3 tools/get_deps.py {family})')


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


STICKY_OPTIONS = ('LOG', 'LOGGER', 'CFLAGS_CLI')   # family_support.cmake reads these with if(DEFINED)
BUILD_PY_OPTIONS = ('BOARD', 'CMAKE_BUILD_TYPE', 'LINKERMAP_OPTION', 'TOOLCHAIN')
CACHE_ENTRY = re.compile(r'^([A-Za-z_]\w*):([A-Z]+)=(.*)$')
AGENT_DEFINES = '.agent-defines'


def record_options(build_dir, supplied):
    """Record the option names this run configured the dir with, for stale_options() to read
    back. Written after the build, so it names a dir some configure reached; a run refused
    over a stale option exits before here, leaving the record that refused it in place."""
    d = ROOT / build_dir
    if d.is_dir():
        (d / AGENT_DEFINES).write_text(json.dumps(sorted(supplied)) + '\n')


def recorded_options(build_dir):
    """The option names an earlier run of this dir recorded, empty for a dir configured
    before the sidecar existed or for one whose sidecar no longer parses."""
    try:
        names = json.loads((ROOT / build_dir / AGENT_DEFINES).read_text(encoding='utf-8'))
        # a record from before build_one normalised its names can hold NAME:TYPE=value
        return {str(n).partition('=')[0].partition(':')[0] for n in names}
    except (OSError, ValueError, TypeError):
        return set()


def stale_options(build_dir, supplied):
    """{option: value} a build dir's CMake cache still carries from an earlier configure
    and this invocation does not set. cmake keeps a -D for the life of the dir and the
    build files act on it - family_support.cmake reads LOG, LOGGER and CFLAGS_CLI with
    if(DEFINED) and MAX3421_HOST with STREQUAL, a family.cmake reads what it likes
    (RHPORT_DEVICE) - so a shared dir silently builds, and the HIL flashes out of it, a
    configuration nobody asked for. Which names those are is no fixed list, so what an
    earlier run recorded answers first (record_options), whatever type the cache now shows:
    cmake code may declare a -D as a typed cache variable - hw/bsp/rp2040/pico_sdk_import.cmake
    retypes PICO_SDK_PATH to PATH - and the value then survives with nothing in the cache
    left to say a command line gave it. The entry type is the fallback for a dir configured
    before the sidecar existed: a -D no cmake code declares keeps UNINITIALIZED, the type
    only a command line gives, and the four tools/build.py passes on every configure are
    this run's own. A recorded name the cache no longer carries is no risk either - nothing
    holds its value. An empty value is an option too: -DLOG= leaves a cache
    entry, if(DEFINED LOG) is true for it, and the build compiles with CFG_TUSB_DEBUG=.
    An option this run does set is no risk: its -D overwrites the cached value.
    Espressif builds one idf tree per example under the dir, each with a cache full of
    idf.py's own untyped defines; -D is refused for that family (build_one), so there only
    the sticky trio can have come from a command line."""
    out = {}
    root = ROOT / build_dir
    recorded = recorded_options(build_dir)
    caches = [(root / 'CMakeCache.txt', True)] + \
        [(c, False) for c in sorted(root.glob('*/*/CMakeCache.txt'))]
    for cache, from_build_py in caches:
        try:
            text = cache.read_text(encoding='utf-8', errors='replace')
        except OSError:
            continue
        for line in text.splitlines():
            m = CACHE_ENTRY.match(line)
            if not m:
                continue
            name, kind, value = m.groups()
            if name in supplied or name in BUILD_PY_OPTIONS:
                continue
            if name in recorded or name in STICKY_OPTIONS or (from_build_py and kind == 'UNINITIALIZED'):
                out[name] = value
    return out


def build_one(board, examples, targets, defines, cflags, shared, fetch, verbose):
    family = family_of(board)
    # tools/build.py hands -D to cmake but not to idf.py, so a define would be
    # dropped and the build would pass without the configuration it was asked for
    if defines and family == 'espressif':
        fail(f'-D is not forwarded to idf.py, so it cannot configure {board} (family espressif). '
             'A preprocessor macro can go through --cflag; a build-system setting (LOG, LOGGER) has no '
             'path here, since the BSP translates those into other defines')
    # tools/build.py puts its own -DBOARD and friends before the caller's, so a -D on
    # one of those keys would win and the artifacts would carry another board's name
    owned = sorted({d.partition('=')[0].partition(':')[0] for d in defines} & set(BUILD_PY_OPTIONS))
    if owned:
        fail(f'-D {", ".join(owned)}: tools/build.py owns {"/".join(BUILD_PY_OPTIONS)}; name the board '
             f'with --board and leave the build type, linker map and toolchain to it')
    ensure_deps(family, fetch, verbose)
    name = board if shared else f'agent-{os.getpid()}-{board}'
    build_dir = f'cmake-build/cmake-build-{name}'
    # -D takes cmake's NAME:TYPE=value form too, whose cache entry is still keyed by NAME
    # alone: unnormalised, a typed define matches neither the cache nor its own sidecar record
    supplied = {d.partition('=')[0].partition(':')[0] for d in defines} | ({'CFLAGS_CLI'} if cflags else set())
    if shared:
        stale = stale_options(build_dir, supplied)
        if stale:
            fail(f'{build_dir} was configured with {", ".join(f"{k}={v}" for k, v in sorted(stale.items()))} '
                 f'and this run does not set {"/".join(sorted(stale))}: cmake keeps a -D for the life of the '
                 f'dir, so the firmware - and the HIL run that flashes it - would carry a configuration '
                 f'nobody asked for. Pass the same option(s), or remove the dir to build it clean')
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
    record_options(build_dir, supplied)
    # tools/build.py's one line on the board's hw/bsp/family.json row, when the configure
    # was the board's default one: updated, unchanged, or why it could not be observed
    catalog_line = next((l for l in reversed(out.splitlines()) if l.startswith('family.json: ')), None)
    rows = ROW.findall(out)
    statuses = [s for _, _, s in rows]
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
            'built': len(fresh), 'okExamples': sorted({e.stem for e in verified}), 'firstError': first,
            'familyJson': catalog_line}


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


def catalog_text():
    try:
        return family_json.CATALOG.read_text(encoding='utf-8')
    except FileNotFoundError:
        return None


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
    p.add_argument('--fetch-deps', action='store_true',
                   help='run tools/get_deps.py for a family whose deps are missing or off the pinned commit')
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
    before = catalog_text()
    results = [build_one(b, a.example, a.target, a.define, a.cflag, a.shared, a.fetch_deps, a.verbose)
               for b in boards]
    built_ok = all(r['status'] == 'ok' for r in results)
    # a build that rewrote a board's row leaves a tracked file modified: the caller
    # commits it with the change that moved it
    extra['familyJsonChanged'] = catalog_text() != before
    if not a.board:
        # an uncovered path fails the scope even when every board built green: a class
        # driver plus the core file that registers it is the common shape
        extra['nothingToBuild'], extra['uncovered'] = coverage(reasons, paths, results, bool(a.example or a.target))
    ok = built_ok and not extra.get('uncovered')
    print(json.dumps({'pass': ok, 'boards': results, 'resolution': how_resolved, **extra}))
    return 0 if ok else (1 if not built_ok else 3)


if __name__ == '__main__':
    sys.exit(main())
