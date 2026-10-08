#!/usr/bin/env python3
"""Build TinyUSB examples for the boards a change affects, or for named boards.

  check_build.py (--scope PATH... | --base REF [--worktree] | --endpoints A..B | --board B...)
           [-e role/name]... [-T target]... [-D SYMBOL]... [--cflag FLAG]... [--shared [--variants CONFIG [--receipt FILE]]]
  check_build.py --select-only (--scope PATH... | --base REF [--worktree] | --endpoints A..B) [--config ROSTER]...

--select-only prints change_impact's selection manifest v1 (SKILL.md) and stops: the selection
contract CI and agents read.

Scope resolution goes through tools/change_impact.py: one board per affected family
(a rig-roster board of that family first, else the first in hw/bsp/<family>/boards,
preferring one that builds an example the change affects), plus one more board per
changed driver none of them compiles by its hw/bsp/family.json row, or the
representative pair when the selection is the full matrix. Drivers without USB-IP
guards add a compiling board per MCU_VARIANT of the family. Each board builds through
tools/build.py in a private cmake-build-agent-<pid> dir; --shared uses the canonical
cmake-build-<board> that HIL flashes from, must not be shared with a parallel agent,
and is refused when it still carries an option from an earlier configure this run does not set.
--variants builds each of a named board's HIL variants in the roster CONFIG instead, into the
cmake-build-<variant> dir hil_test.py flashes it from, with the variant's defines and flags.
--receipt then writes the HIL build receipt (hil_remote.py receipt: HEAD, the roster, every
staged file's digest) after a passing build of every example on a tree clean before and after
and a HEAD unmoved since the build began, so a receipt always comes from a build of its HEAD;
its line is the JSON's "receipt".
Dependencies the family needs (get_deps.py's table) are checked first: one missing,
empty or not at the pinned commit is an error naming the remedy, or fetched when
--fetch-deps is given.

stdout ends with one JSON line: {"pass", "boards": [{"board", "family", "buildDir",
"status", "built" and "okExamples" (elfs this run wrote), "firstError", "familyJson"
(tools/build.py's line on the board's catalog row, when the configure was the default
one)}], "resolution", "nothingToBuild", "uncovered", "familyJsonChanged" (a build
rewrote hw/bsp/family.json; commit it with the change)}. A scope path nothing builds is one of two kinds, in change_impact's words:
"nothingToBuild" is nothing to verify (docs, .claude/, unit tests, HIL harness);
"uncovered" is firmware no board's build compiled (a class no example enables, a lib
nothing builds, a port no built board kept a line of after preprocessing - wrong
family, or a configuration whose guard empties the driver - an example no built board wrote an elf for, or a
build target the default sweep never runs), a coverage gap whatever else built.
Exit 0 pass, 1 a board failed, 2 usage or resolution error ("error" carries the
message, a missing dependency's remedy included) or a refused receipt ("receipt" carries
its "error"), 3 uncovered paths.
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
ROW = re.compile(r'^\|\s*(\S+)\s*\|\s*(.+?)\s*\|\s*\x1b\[\d+m(OK|Failed|Skipped)\x1b\[0m', re.M)
sys.path.insert(0, str(ROOT / 'tools'))
import build as tools_build  # noqa: E402  tools/build.py, first on the path above
import change_impact  # noqa: E402  the same classifier run below, here for its option knowledge
import family_json  # noqa: E402  hw/bsp/family.json: what each board's default configure compiles
sys.path.insert(0, str(ROOT / 'test' / 'hil' / 'helper'))
import hil_report  # noqa: E402  stdlib-only; board_variants() reads a roster board's builds


def family_of(board):
    hits = sorted(p.parent.parent.name for p in ROOT.glob(f'hw/bsp/*/boards/{board}'))
    if len(hits) != 1:
        fail(f'board {board!r} matches {hits or "no"} hw/bsp family')
    return hits[0]


def family_boards(family):
    d = ROOT / 'hw' / 'bsp' / family / 'boards'
    return sorted(p.name for p in d.iterdir() if p.is_dir()) if d.is_dir() else []


def expand_scope(scope):
    """The files of every directory in the scope, new ones included. change_impact and the
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
            # change_impact classifies an unknown path as the full matrix, whose pair then
            # covers it: a typo would come back green for a path no change touched
            fail(f'{p} does not exist and is not a tracked file; only a path present in the '
                 f'tree or deleted from it can be in the scope')
    return out


def tracked(path):
    """Whether HEAD holds exactly this path: a deletion, staged or not, is a change to
    build for. Not a pathspec lookup, which would let `src/*.c` through as a match."""
    r = subprocess.run(['git', 'cat-file', '-e', f'HEAD:{path}'], capture_output=True, text=True, cwd=ROOT)
    return r.returncode == 0


def select(scope=None, base=None, configs=(HIL_CONFIG,), endpoints=None, worktree=False):
    """change_impact's selection manifest v1 for a path list, the branch diff against base
    (with worktree, the uncommitted tree too) or a push's endpoints A..B. A scope that
    carries a get_deps.py edit resolves its dep entries against HEAD (--deps-base).
    Isolated (-I -S) as on a bare CI runner: the selector is stdlib-only, so an import
    that needs site-packages fails here first."""
    selector = [sys.executable, '-I', '-S', str(ROOT / 'tools' / 'change_impact.py'), '--manifest']
    with tempfile.TemporaryDirectory() as tmp:   # the path list goes away with it, fail() included
        if base:
            cmd = selector + ['--base', base] + (['--worktree'] if worktree else [])
        elif endpoints:
            cmd = selector + ['--endpoints', endpoints]
        else:
            listing = Path(tmp) / 'scope.txt'
            listing.write_text('\n'.join(scope) + '\n')
            cmd = selector + ['--diff-file', str(listing), '--deps-base', 'HEAD']
        r = subprocess.run(cmd + [str(c) for c in configs], capture_output=True, text=True, cwd=ROOT)
    if r.returncode != 0:
        fail(f'change_impact failed:\n{r.stderr.strip()}')
    return json.loads(r.stdout.splitlines()[-1])


def representatives(candidates, examples, drivers=(), keep=()):
    """The family's boards to build: the boards in `keep` (the ones the change edits),
    plus one candidate per changed driver none of them compiles (its catalog row lists
    the driver, selects the USB IPs its guard names where the probe can say, and its row
    and own cmake turn on no option the guard negates and leave off none of the per-board
    ones it requires: hcd_dwc2.c is empty on a MAX3421 board, hcd_rp2040.c on a PIO-USB
    one, hcd_pio_usb.c on a board that is neither). Drivers without USB-IP guards need
    one compiling board per MCU_VARIANT in board.cmake; boards without it share one
    group. The missing guard is a deliberate selection proxy for variant-dependent code,
    not a claim that guarded drivers have no variant-specific behaviour. Add a plain
    first pick when that leaves nothing. A candidate that compiles
    one of the affected examples is preferred, the criterion dropped when it leaves no
    candidate: change_impact keeps a family when ANY of its boards builds the selection under
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
        src = str(ROOT / 'src' / 'portable' / driver)
        group = (lambda b: None) if source_usbips(src) else board_mcu_variant
        covered = {group(b) for b in picked if compiles(b, driver)}
        for board in pool:
            variant = group(board)
            if variant not in covered and compiles(board, driver):
                picked.append(board)
                covered.add(variant)
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
    is left out). Empty for a driver no TUP_USBIP gates (rp2040, nrf5x)."""
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


def board_cmake_path(board):
    return ROOT / 'hw' / 'bsp' / family_of(board) / 'boards' / board / 'board.cmake'


@functools.lru_cache(maxsize=None)
def board_mcu_variant(board):
    """The board.cmake MCU_VARIANT, or None for the shared unset group."""
    values = tools_build.build_utils._cmake_set_values(board_cmake_path(board), 'MCU_VARIANT')
    return values[0] if values else None


@functools.lru_cache(maxsize=None)
def board_cmake_options(board):
    """The CFG_ options a board's own board.cmake turns on. A row's `defines` hold only
    what tusb_mcu.h keys on, so an option that gates a driver's guard without reaching
    tusb_mcu.h is absent from it: adafruit_fruit_jam sets CFG_TUH_RPI_PIO_USB, which
    hcd_rp2040.c's guard negates, and its row's `defines` are empty. Scraped rather than
    observed, which only a selection may do - a candidate is rejected, never accepted, on
    this, and the body test after the build is still the verdict."""
    try:
        text = board_cmake_path(board).read_text(encoding='utf-8', errors='replace')
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


def boards_for(manifest):
    """One board per affected family, plus one more per changed driver the first does not
    compile: a board whose own hw/bsp dir is in the scope, else a rig-roster board of the
    family, else one under hw/bsp/<family>/boards, and then a board per USB-IP requirement
    set of the changed ports, and one turning on the build option an option-gated port
    needs, that none of those selects (representatives).
    The representative pair for the full matrix, plus boards for every family a scope
    path names (a port, bsp or mcu path): the pair stands in for the matrix on core
    code, not on a port it does not contain. No family is not a verdict on its own:
    see coverage() for what the scope's unbuilt paths mean."""
    build = manifest['build']
    records = build.get('paths', [])
    # required_boards holds only boards still in the tree: one the change deletes falls
    # through to the rig-roster/first-board pick instead of failing family_of()
    changed = {b: family_of(b) for b in build['required_boards']}
    rig = {b: family_of(b) for b in manifest.get('hil', {}).get('boards', {})}
    changed_note = f', changed boards {sorted(changed)}' if changed else ''
    drivers = family_drivers(records)
    if build['full']:
        boards = list(dict.fromkeys(FULL_MATRIX_BOARDS + sorted(changed)))
        have = {}
        for b in boards:
            have.setdefault(family_of(b), []).append(b)
        added = []
        for fam in sorted(set().union(*(set(r['families'] or ()) for r in records))):
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
    for fam, sel in build['families'].items():
        own = sorted(b for b, f in changed.items() if f == fam)
        on_rig = sorted(b for b, f in rig.items() if f == fam)
        candidates = list(dict.fromkeys(own + on_rig + family_boards(fam)))
        if not candidates:
            fail(f'family {fam} has no boards under hw/bsp/{fam}/boards')
        exs = None if sel['examples'] == 'all' else sel['examples']
        boards.extend(representatives(candidates, exs, drivers.get(fam, ()), own))
    return boards, f'one board per family {list(build["families"])}' + changed_note


def family_drivers(records):
    """family -> the src/portable-relative drivers the scope changed that its picks must compile
    between them. A family carrying two IPs (stm32l4 is fsdev and dwc2, ch32v20x fsdev
    and wch usbhs) is named by a port change whatever its boards are, so without this a
    pick can be a board the changed driver preprocesses away to nothing."""
    out = {}
    for r in records:
        if r['port'] is not None:
            for fam in r['families'] or ():
                out.setdefault(fam, set()).update(drivers_of(r['path']))
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


def port_gap(record, results):
    """Why the boards built do not cover a changed port path, None when they do: for
    each driver the path stands for, some built board of its families must have compiled
    a translation unit of it, in an example it built, that kept lines of the driver
    after preprocessing. That is the final word, whatever the USB-IP probe said at
    selection: dcd_nrf5x.c is in every nrf build and empty on NRF54, hcd_rp2040.c empty
    under MAX3421. A driver the change deleted has no body to compile."""
    path = record['path']
    if record['port'] is None or not (ROOT / path).exists():
        return None
    fams = set(record['families'] or ())
    built = [r for r in results if r.get('board') and r['family'] in fams and r.get('buildDir')]
    why = []
    for d in drivers_of(path):
        verdicts = [(r['board'], body_verdict(r, d)) for r in built]
        if any(v == 'compiled' for _, v in verdicts):
            continue
        why.append(f'{d}: ' + ('; '.join(f'{b} {v}' for b, v in verdicts) if verdicts else 'no board of its family built'))
    return '; '.join(why) or None


def coverage(records, results, chosen=False, targets=(), dropped=()):
    """change_impact's per-path build records as (nothing to verify, uncovered firmware),
    judged against what was actually built, each entry the record's reason. Effect
    'none' is nothing to verify (non-code, a get_deps.py edit that changes no entry);
    'gap' is firmware no build compiles (a class, typec or lib no example enables, a
    port or hw/mcu path mapping to no family), as is every family _prune_buildable
    dropped (`dropped`, its own line). A path that named families is a gap when none was
    built: a family pruned away (dir gone, boards unreadable, or every example filtered),
    or a full-matrix run whose pair does not contain the port. A port path is a gap too
    when the boards built are of its families but none compiled the changed body
    (port_gap): the driver is in no built board's default configuration, or its guard
    preprocessed it away there. A path that named examples is a gap when no board wrote
    an elf for any of them (a core stack path names every example of its role); any
    other contributing path is a gap when the run produced no elf at all (-T help is a
    green build of nothing). A path whose record names build targets (membrowse_cli.py:
    examples-membrowse-upload) is a gap unless the run selected each: the default sweep
    builds `all`, which never runs them, and another -e or -T does not stand in for one.
    code_size.py is validated by CI's code-size step, which no local build runs.
    `chosen` (-e or -T given) hands the rest to the caller, bar a family no board of which
    was built, bar those targets and bar code_size.py: narrowing the examples does not
    change which families the scope resolves to. With no board at all, every path not
    explained as nothing-to-verify is a gap."""
    built_fams = {r['family'] for r in results}
    ok_ex = set().union(*(set(r.get('okExamples', ())) for r in results)) if results else set()
    benign, gaps, explained = [], list(dropped), set()
    for rec in records:
        r, fams, exs = rec['reason'], rec['families'], rec['examples']
        missing = [t for t in rec['targets'] if t not in targets]
        n = len(benign) + len(gaps)
        if rec['effect'] == 'none':
            benign.append(r)
        elif rec['effect'] == 'gap':
            gaps.append(r)
        elif fams is not None and not set(fams) & built_fams:
            gaps.append(r)
        elif missing:
            gaps.append(f'{r} (the default sweep builds `all`, which does not run '
                        f'{", ".join(missing)}: rerun with -T all' + ''.join(f' -T {t}' for t in missing) + ')')
        elif rec['rule'] == 'size-script':
            gaps.append(f"{r} (its snapshot is validated by CI's code-size step; not verified by this run)")
        elif chosen:
            continue
        elif (gap := port_gap(rec, results)):
            gaps.append(f'{r} ({gap})')
        elif exs is not None and not {e.split('/', 1)[1] for e in exs} & ok_ex:
            gaps.append(r)
        elif results and not ok_ex:
            gaps.append(r)
        if len(benign) + len(gaps) > n:
            explained.add(rec['path'])
    if not results:
        gaps += [rec['reason'] for rec in records if rec['path'] not in explained]
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
        attempted = {e.split('/', 1)[1] for e in
                     tools_build.select_examples(board, examples or None, defines, root=str(ROOT))[0]}
        return [e for e in elfs if e.parent.name in attempted]
    reg = tools_build.cmake_registered_targets(str(ROOT / build_dir))
    if not reg:
        return fresh
    asked = {e.split('/', 1)[1] for e in examples}
    return [e for e in elfs if e.stem in reg and (not asked or e.stem in asked)]


STICKY_OPTIONS = ('LOG', 'LOGGER', 'CFLAGS_CLI')   # family_support.cmake reads these with if(DEFINED)
BUILD_PY_OPTIONS = ('BOARD', 'CMAKE_BUILD_TYPE', 'TOOLCHAIN')
# what idf.py's ensure_build_directory() passes cmake on its own (ESP-IDF v5.5 tools/idf_py_actions/tools.py)
IDF_PY_OPTIONS = ('CCACHE_ENABLE', 'ESP_PLATFORM', 'PYTHON', 'PYTHON_DEPS_CHECKED')
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
    only a command line gives, and BUILD_PY_OPTIONS, which tools/build.py passes on
    every configure, are this run's own, as is the MEMBROWSE_BOARD it passes a named
    build: the dir's own name. A recorded name the cache no longer carries is
    no risk either - nothing holds its value. An empty value is an option too: -DLOG=
    leaves a cache entry, if(DEFINED LOG) is true for it, and the build compiles with
    CFG_TUSB_DEBUG=.
    An option this run does set is no risk: its -D overwrites the cached value.
    Espressif builds one idf tree per example under the dir, whose cache also holds
    IDF_PY_OPTIONS, idf.py's own untyped defines; past those, the fallback reads each
    tree too, since a direct tools/build.py -D run leaves a tree no record speaks for."""
    out = {}
    root = ROOT / build_dir
    recorded = recorded_options(build_dir)
    caches = [(root / 'CMakeCache.txt', ())] + \
        [(c, IDF_PY_OPTIONS) for c in sorted(root.glob('*/*/CMakeCache.txt'))]
    own_name = Path(build_dir).name.removeprefix('cmake-build-')
    for cache, tool_options in caches:
        try:
            text = cache.read_text(encoding='utf-8', errors='replace')
        except OSError:
            continue
        for line in text.splitlines():
            m = CACHE_ENTRY.match(line)
            if not m:
                continue
            name, kind, value = m.groups()
            if name in supplied or name in BUILD_PY_OPTIONS or name in tool_options or \
                    (name == 'MEMBROWSE_BOARD' and value == own_name):
                continue
            if name in recorded or name in STICKY_OPTIONS or kind == 'UNINITIALIZED':
                out[name] = value
    return out


def roster_variants(boards, config):
    """(board, build name, defines, cflags) for each variant hil_test.py runs of each board."""
    try:
        roster = {b['name']: b for b in json.loads(Path(config).read_text())['boards']}
    except (OSError, ValueError, KeyError, TypeError) as e:
        fail(f'could not read the HIL roster {config}: {e}')
    unknown = [b for b in boards if b not in roster]
    if unknown:
        fail(f'not in {config}: {" ".join(unknown)}; --variants takes rig board names')
    out = []
    for b in boards:
        try:
            variants = hil_report.board_variants(roster[b])
        except ValueError as e:
            fail(f'{config}: {e}')
        out += [(b, v['name'], v['defines'], v['flags']) for v in variants]
    return out


def build_one(board, examples, targets, defines, cflags, shared, fetch, verbose, name=None):
    family = family_of(board)
    # tools/build.py puts its own -DBOARD and friends before the caller's, so a -D on
    # one of those keys would win and the artifacts would carry another board's name
    owned = sorted({d.partition('=')[0].partition(':')[0] for d in defines} & set(BUILD_PY_OPTIONS))
    if owned:
        fail(f'-D {", ".join(owned)}: tools/build.py owns {"/".join(BUILD_PY_OPTIONS)}; name the board '
             f'with --board and leave the build type and toolchain to it')
    ensure_deps(family, fetch, verbose)
    name = name or board
    if not shared:
        name = f'agent-{os.getpid()}-{name}'
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
    if name != board:
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
        status, first = 'failed', tools_build.build_utils.first_error(out) or rows[-1][1]
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


def run(cmd, verbose):
    # streamed as it arrives: a variant build runs for minutes, and a silent buffer reads as a
    # stall. PYTHONUNBUFFERED, or tools/build.py block-buffers its piped stdout until it exits
    out = []
    with subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, cwd=ROOT,
                          env={**os.environ, 'PYTHONUNBUFFERED': '1'}) as p:
        for line in p.stdout:
            out.append(line)
            if verbose:
                sys.stderr.write(line)
                sys.stderr.flush()
    return p.returncode, ''.join(out)


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


def git_status():
    return subprocess.run(['git', '-C', str(ROOT), 'status', '--porcelain', '--untracked-files=normal'], capture_output=True, text=True, check=True).stdout.strip()


def git_head():
    return subprocess.run(['git', '-C', str(ROOT), 'rev-parse', 'HEAD'], capture_output=True, text=True, check=True).stdout.strip()


def write_receipt(out, head, boards, config):
    """hil_remote.py's receipt of what this build of head left for a HIL run: its JSON line, or {"error"}
    (a build that rewrote a tracked file, hw/bsp/family.json included, leaves the tree unclean)."""
    run = subprocess.run([sys.executable, str(ROOT / '.claude/skills/hil/scripts/hil_remote.py'), 'receipt', '--out', out,
                          '--head', head,
                          *(x for b in boards for x in ('-b', b))],
                         cwd=ROOT, env={**os.environ, 'CONFIG': str(Path(config).resolve())}, capture_output=True, text=True)
    if run.returncode:
        return {'error': run.stderr.strip()}
    return json.loads(run.stdout.strip().splitlines()[-1])


def main(argv=None):
    p = Parser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    how = p.add_mutually_exclusive_group(required=True)
    how.add_argument('--scope', nargs='+', metavar='PATH',
                     help='changed paths or directories, repo-relative')
    how.add_argument('--base', metavar='REF', help='resolve the scope from git diff against REF')
    how.add_argument('--endpoints', metavar='A..B',
                     help="resolve the scope from a push's A..B diff; B must be the checked-out HEAD")
    how.add_argument('--board', action='append', default=None, help='build this board (repeatable)')
    p.add_argument('--worktree', action='store_true',
                   help='with --base: include uncommitted and untracked changes in the scope')
    p.add_argument('--select-only', action='store_true',
                   help="print change_impact's selection manifest for the scope and stop: no dependency check, no build")
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
    p.add_argument('--variants', metavar='CONFIG',
                   help='with --board and --shared: build each HIL variant the roster CONFIG gives the board, '
                        'in its cmake-build-<variant> with its defines and flags')
    p.add_argument('--receipt', metavar='FILE',
                   help='with --variants and every example: write the HIL build receipt after a passing build')
    p.add_argument('--config', action='append', default=None,
                   help='rig roster for change_impact (repeatable; default: tinyusb.json)')
    p.add_argument('-v', '--verbose', action='store_true', help='stream build output to stderr')
    a = p.parse_args(argv)
    os.chdir(ROOT)  # tools/build.py's example listing reads examples/ relative to the root

    extra = {}
    configs = a.config or [str(HIL_CONFIG)]
    if a.worktree and not a.base:
        fail('--worktree needs --base')
    if a.select_only:
        if a.board:
            fail('--select-only selects from a scope, --base or --endpoints, not named boards')
        paths = expand_scope(a.scope) if a.scope is not None else None
        print(json.dumps(select(paths, a.base, configs, a.endpoints, a.worktree)))
        return 0
    if a.variants is not None and not (a.board and a.shared):
        fail('--variants needs --board and --shared: it builds the dirs hil_test.py flashes')
    if a.receipt is not None and (a.variants is None or a.example or a.target):
        fail('--receipt needs --variants and every example (no -e/-T): it pins everything a HIL run stages')
    if a.receipt is not None and git_status():
        fail(f'--receipt needs a clean tree before the build:\n{git_status()}\ncommit or remove these first')
    head = git_head() if a.receipt is not None else None
    if a.board:
        boards, how_resolved = a.board, 'named boards'
    else:
        manifest = select(expand_scope(a.scope) if a.scope is not None else None, a.base, configs,
                          a.endpoints, a.worktree)
        records = manifest['build']['paths']
        boards, how_resolved = boards_for(manifest)
    before = catalog_text()
    builds = roster_variants(boards, a.variants) if a.variants is not None else [(b, None, [], []) for b in boards]
    results = [build_one(b, a.example, a.target, a.define + d, a.cflag + f, a.shared, a.fetch_deps, a.verbose, n)
               for b, n, d, f in builds]
    built_ok = all(r['status'] == 'ok' for r in results)
    # a build that rewrote a board's row leaves a tracked file modified: the caller
    # commits it with the change that moved it
    extra['familyJsonChanged'] = catalog_text() != before
    if not a.board:
        # an uncovered path fails the scope even when every board built green: a class
        # driver plus the core file that registers it is the common shape
        extra['nothingToBuild'], extra['uncovered'] = coverage(
            records, results, bool(a.example or a.target), a.target,
            manifest['build']['dropped'].values())
    ok = built_ok and not extra.get('uncovered')
    if ok and a.receipt is not None:
        extra['receipt'] = write_receipt(a.receipt, head, boards, a.variants)
        if 'error' in extra['receipt']:
            print(json.dumps({'pass': False, 'boards': results, 'resolution': how_resolved, **extra}))
            return 2
    print(json.dumps({'pass': ok, 'boards': results, 'resolution': how_resolved, **extra}))
    return 0 if ok else (1 if not built_ok else 3)


if __name__ == '__main__':
    sys.exit(main())
