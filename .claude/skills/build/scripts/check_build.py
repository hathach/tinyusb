#!/usr/bin/env python3
"""Build TinyUSB examples for the boards a change affects, or for named boards.

  check_build.py (--scope PATH... | --base REF | --board B...) [-e role/name]... [-T target]...
           [-D SYMBOL]... [--cflag FLAG]... [--shared]

Scope resolution goes through tools/ci_select.py: one board per affected family
(a rig-roster board of that family first, else the first in hw/bsp/<family>/boards,
preferring one that builds an example the change affects), plus one more board per
USB-IP requirement set of the changed drivers that none of them selects, or the
representative pair when the selection is the full matrix. Each board builds through
tools/build.py in a private cmake-build-agent-<pid> dir; --shared uses the canonical
cmake-build-<board> that HIL flashes from, must not be shared with a parallel agent,
and is refused when its CMake cache still carries an option this run does not set.
Dependencies the family needs (get_deps.py's table) are checked first: one missing,
empty or not at the pinned commit is an error naming the remedy, or fetched when
--fetch-deps is given.

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
import get_deps  # noqa: E402  the dependency table, one source with the fetcher
import build as tools_build  # noqa: E402  tools/build.py, first on the path above


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
    """The branch's changed paths against base, the same set ci_select --base classifies.
    --no-renames as ci_select does: rename detection reports only a rename's destination,
    so the board or port a file moved out of would never be built."""
    r = subprocess.run(['git', 'diff', '--no-renames', '--name-only', f'{base}...HEAD'],
                       capture_output=True, text=True, cwd=ROOT)
    if r.returncode != 0:
        fail(f'git diff against {base} failed:\n{r.stderr.strip()}')
    return r.stdout.split()


def select(scope=None, base=None, config=HIL_CONFIG):
    """(ci_select's JSON, its per-path build-axis reasons) for a path list or, with
    --base, for the branch diff: only the base form sees dependency revision changes
    (get_deps.py table edits)."""
    ci_select = [sys.executable, str(ROOT / 'tools' / 'ci_select.py')]
    with tempfile.TemporaryDirectory() as tmp:   # the path list goes away with it, fail() included
        if base:
            cmd = ci_select + ['--base', base, str(config)]
        else:
            listing = Path(tmp) / 'scope.txt'
            listing.write_text('\n'.join(scope) + '\n')
            cmd = ci_select + ['--diff-file', str(listing), str(config)]
        r = subprocess.run(cmd, capture_output=True, text=True, cwd=ROOT)
    if r.returncode != 0:
        fail(f'ci_select failed:\n{r.stderr.strip()}')
    reasons = [l.split('ci_select[build]: ', 1)[1] for l in r.stderr.splitlines()
               if l.startswith('ci_select[build]: ')]
    return json.loads(r.stdout.splitlines()[-1]), reasons


def representatives(candidates, examples, usbips=(), keep=()):
    """The family's boards to build: the boards in `keep` (the ones the change edits),
    plus one candidate per USB-IP requirement set none of them selects, plus a plain first
    pick when that leaves nothing. A candidate that compiles one of the affected examples
    is preferred, the criterion dropped rather than returning nothing when it leaves no
    candidate: ci_select keeps a family when ANY of its boards builds the selection under
    EITHER build system, so the first candidate can be one skipped for every affected
    example (samd11's cynthion_d11 is skip.txt'd out of device/mtp) and verify none of the
    change.
    One board is not enough for a family carrying two IPs: every stm32l4 board but
    stm32l412nucleo is DWC2 while the family's family.cmake also compiles fsdev, so a
    scope changing both drivers needs a board apiece or one of them preprocesses away to
    nothing. A requirement set no candidate satisfies adds no board - then no board of the
    family compiles that driver at all, and coverage() reports the path as uncovered
    instead of the run going green on a body it never saw.
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
    # widest set first: a board selecting FSDEV and FSDEV_DRD answers the hcd's
    # requirement and the dcd's, where the other order would pick two boards
    for req in sorted(usbips, key=lambda s: (-len(s), sorted(s))):
        if any(req <= board_usbips(b) for b in picked):
            continue
        hit = next((b for b in pool if req <= board_usbips(b)), None)
        if hit:
            picked.append(hit)
    return picked or [pool[0]]


USBIP_TERM = re.compile(r'^\s*defined\s*\(\s*(TUP_USBIP_\w+)\s*\)\s*$')
USBIP_DEFINE = re.compile(r'^#define (TUP_USBIP_\w+)', re.M)
MCU_DEFINE = re.compile(r'defined\s*\(\s*(\w+)\s*\)')
WORD = re.compile(r'\w+')


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
def path_usbips(path):
    """One TUP_USBIP_* set per driver a changed src/portable path stands for: its own when
    the path is a driver, else every driver's in its dir (a header they all include). The
    port dir's union is too weak for a driver: hcd_stm32_fsdev.c is guarded by
    TUP_USBIP_FSDEV && TUP_USBIP_FSDEV_DRD, and a board selecting only its dcd sibling's
    TUP_USBIP_FSDEV (ch32v203c_r0_1v0) preprocesses the changed body away."""
    p = ROOT / path
    own = source_usbips(str(p)) if p.suffix == '.c' else frozenset()
    if own:
        return frozenset({own})
    return frozenset(s for s in map(source_usbips, sorted(str(c) for c in p.parent.glob('*.c'))) if s)


@functools.lru_cache(maxsize=None)
def mcu_defines():
    """The device-model macros tusb_mcu.h branches on (STM32L476xx, LPC54114_cm4_SERIES),
    so the one a board spells can be picked out of its cmake wherever it spells it."""
    text = (ROOT / 'src' / 'common' / 'tusb_mcu.h').read_text(encoding='utf-8', errors='replace')
    return frozenset(n for n in MCU_DEFINE.findall(text) if not n.startswith(('TUP_', 'CFG_', '__')))


FAMILY_MCUS_SET = re.compile(r'set\s*\(\s*FAMILY_MCUS\s+([^)\s]+)')
CMAKE_COND = re.compile(r'^(if|elseif|else|endif)\s*\((.*)\)\s*$')
VARIANT_EQ = re.compile(r'MCU_VARIANT\s+STREQUAL\s+"?([\w.]+)"?')


def logical_lines(text):
    """One cmake command per item, a call whose arguments span lines joined (maxim's
    family.cmake wraps an if() over two), comment lines dropped: a condition read half
    is a condition misread."""
    pending = ''
    for line in text.splitlines():
        line = line.strip()
        if line.startswith('#'):
            continue
        pending = f'{pending} {line}' if pending else line
        if pending.count('(') > pending.count(')'):
            continue
        yield pending
        pending = ''


def variant_test(cond, variant):
    """Whether an if() condition holds for this MCU_VARIANT, None when nothing can say:
    a board whose cmake spells no variant, or a condition that is not an MCU_VARIANT
    equality or an OR-chain of them (nrf ORs two)."""
    terms = VARIANT_EQ.findall(cond)
    if not variant or not terms or VARIANT_EQ.sub('', cond).replace('OR', ' ').strip(' \t()'):
        return None
    return variant in terms


@functools.lru_cache(maxsize=None)
def variant_mcu(family, variant):
    """(family.cmake picks FAMILY_MCUS inside an if(), the token it picks for this
    MCU_VARIANT). A cmake build compiles with CFG_TUSB_MCU=OPT_MCU_${FAMILY_MCUS}
    (family_add_tinyusb), and nrf and mcx choose FAMILY_MCUS per variant, which
    build_utils' scrape does not evaluate - it takes the family file's FIRST
    CFG_TUSB_MCU token, so every nrf board answers NRF54, the nrf52840 ones included,
    and those select no TUP_USBIP at all while NRF54 selects DWC2.
    Only MCU_VARIANT equality decides a branch; a pick under any other condition, or one
    whose token is not a plain name, leaves the token None for the caller to read as
    'cannot say'."""
    try:
        text = (ROOT / 'hw' / 'bsp' / family / 'family.cmake').read_text(encoding='utf-8', errors='replace')
    except OSError:
        return False, None
    keyed, token = False, None
    stack = []                   # per open if(): [this branch active, what the chain did]
    for line in logical_lines(text):
        m = CMAKE_COND.match(line)
        if m:
            kw, cond = m.groups()
            if kw == 'endif':
                if stack:
                    stack.pop()
            elif kw == 'if':
                stack.append([variant_test(cond, variant), ''])
            elif stack:
                active, chain = stack[-1]
                chain = chain or ('taken' if active is True else 'unknown' if active is None else '')
                if chain:        # an earlier branch decided the chain, or nothing can say it did
                    active = False if chain == 'taken' else None
                else:
                    active = True if kw == 'else' else variant_test(cond, variant)
                stack[-1] = [active, chain]
            continue
        m = FAMILY_MCUS_SET.search(line)
        if not m:
            continue
        keyed = keyed or bool(stack)
        if all(a is True for a, _ in stack):
            tok = m.group(1).strip('"')
            token = tok if tok.isidentifier() else None
    return keyed, token


@functools.lru_cache(maxsize=None)
def board_usbips(board):
    """The TUP_USBIP_* a board's MCU selects, asked of the preprocessor instead of
    scraped: tusb_mcu.h keys them off CFG_TUSB_MCU and then, for a family carrying two
    IPs, off the device-model define, a chain no regex reproduces. Empty is 'cannot
    say' - no host cc, an MCU token that resolves to no OPT_MCU_*, a branch including an
    SDK header (imxrt, lpc54), a device define the board's cmake never spells, a family
    whose FAMILY_MCUS this board's MCU_VARIANT does not resolve (variant_mcu) - and the
    caller falls back to its other criteria."""
    family = family_of(board)
    board_dir = ROOT / 'hw' / 'bsp' / family / 'boards' / board
    try:
        mcu, _ = tools_build.build_utils._board_mcu(str(board_dir), str(ROOT / 'hw' / 'bsp' / family), family)
    except OSError:                  # family mid-bring-up: no family.cmake to scrape
        return frozenset()
    variant = tools_build.build_utils._cmake_sets(str(board_dir / 'board.cmake')).get('MCU_VARIANT')
    keyed, picked = variant_mcu(family, variant if isinstance(variant, str) else None)
    if keyed:                        # the scrape cannot answer a per-variant family
        if not picked:
            return frozenset()
        mcu = picked
    # the board's own cmake first: family.cmake names a device define only for the
    # families that pick one there, and then the same one for every board
    names = set()
    for f in (board_dir / 'board.cmake', ROOT / 'hw' / 'bsp' / family / 'family.cmake'):
        try:
            names = mcu_defines() & set(WORD.findall(f.read_text(encoding='utf-8', errors='replace')))
        except OSError:
            continue
        if names:
            break
    cmd = ['cc', '-E', '-dM', f'-DCFG_TUSB_MCU=OPT_MCU_{mcu}',
           # tusb_option.h reaches for the firmware's tusb_config.h, which no board has
           # outside an example's build tree; nothing before tusb_mcu.h reads it
           '-DCFG_TUSB_CONFIG_FILE=<stdint.h>',
           '-I', str(ROOT / 'src'), '-x', 'c', str(ROOT / 'src' / 'tusb_option.h')] + \
        [f'-D{n}' for n in sorted(names)]
    try:
        r = subprocess.run(cmd, capture_output=True, text=True, cwd=ROOT)
    except OSError:                  # no host compiler: every board answers 'cannot say'
        return frozenset()
    return frozenset(USBIP_DEFINE.findall(r.stdout))


def boards_for(selection, scope=(), reasons=()):
    """One board per affected family, plus one more per changed driver the first does not
    compile: a board whose own hw/bsp dir is in the scope, else a rig-roster board of the
    family, else one under hw/bsp/<family>/boards, and then a board per USB-IP requirement
    set of the changed ports that none of those selects (representatives).
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
    ips = family_usbips(reasons)
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
            picked = [b for b in representatives(candidates, None, ips.get(fam, ()), have.get(fam, []))
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
        boards.extend(representatives(candidates, fam_ex.get(fam), ips.get(fam, ()), own))
    return boards, f'one board per family {build["families"]}' + changed_note


FAMILIES_IN = re.compile(r"-> families \[(.*?)\]|: bsp family (\S+)$")
EXAMPLES_IN = re.compile(r"-> \[(.*?)\]$|: example (\S+)$")
ROLE_IN = re.compile(r": core (device|host) stack$")
PORT_IN = re.compile(r": port (\S+) -> families \[")


def _named(pattern, reason):
    m = pattern.search(reason)
    if not m:
        return None
    return set(re.findall(r"'([^']+)'", m.group(1))) if m.group(1) is not None else {m.group(2)}


def named_families(reason):
    return _named(FAMILIES_IN, reason)


def family_usbips(reasons):
    """family -> the TUP_USBIP_* sets its picks must select between them, one per driver
    the scope changed. A family carrying two IPs (stm32l4 is fsdev and dwc2, ch32v20x
    fsdev and wch usbhs) is named by a port change whatever its boards are, so without
    this a pick can be a board the changed driver preprocesses away to nothing."""
    out = {}
    for r in reasons:
        if PORT_IN.search(r):
            for fam in named_families(r) or ():
                out.setdefault(fam, set()).update(path_usbips(r.split(': ', 1)[0]))
    return out


def usbip_covered(reason, results):
    """Whether the boards built compile the changed driver, not merely its family: the
    USB-IP sets the path needs against the ones a built board's MCU selects. A board that
    cannot say (board_usbips empty - no host cc, an MCU branch behind an SDK header)
    counts as satisfying, so a family the preprocessor probe cannot answer for is never
    reported as a gap. A reason naming no port is nothing to judge this way."""
    if not PORT_IN.search(reason):
        return True
    fams = named_families(reason) or set()
    have = [board_usbips(r['board']) for r in results if r.get('board') and r['family'] in fams]
    return all(any(not ips or req <= ips for ips in have)
               for req in path_usbips(reason.split(': ', 1)[0]))


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
    families but none selects the USB IP its driver is guarded by (usbip_covered): no
    board of those families compiles the changed body. A path that named examples is a
    gap when no board wrote an elf for any of them; a core stack path names every example
    of its role; any other contributing path is a gap when the run produced no elf at all
    (-T help is a green build of nothing). `chosen` (-e or -T given) hands all that to
    the caller. With no board at all, every path not explained as nothing-to-verify is a gap."""
    built_fams = {r['family'] for r in results}
    ok_ex = set().union(*(set(r.get('okExamples', ())) for r in results)) if results else set()
    benign, gaps = [], []
    for r in reasons:
        fams, exs = named_families(r), named_examples(r)
        if r.endswith('no build contribution') or r.endswith('no dep entry changed, no contribution'):
            benign.append(r)
        elif r.endswith('no contribution') or r.endswith('dropped'):
            gaps.append(r)
        elif fams is not None and not (fams & built_fams and usbip_covered(r, results)):
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


def dep_head(path):
    """The dep checkout's commit, or None when nothing can say: a dir git would answer
    for the enclosing tinyusb repo (no .git of its own, a vendored copy) or a checkout
    with no HEAD yet."""
    if not (path / '.git').exists():
        return None
    r = subprocess.run(['git', '-C', str(path), 'rev-parse', 'HEAD'], capture_output=True, text=True)
    return r.stdout.strip() if r.returncode == 0 else None


def missing_deps(family):
    """The family's dependencies that are not what get_deps.py's table asks for, each
    named with why. Present means content, not a directory: get_deps.py git-inits the dep
    dir before fetching and exits 0 whatever the fetch did (its run_cmd's status is
    ignored), so a fetch that failed leaves a dir holding nothing but .git. A checkout at
    another commit is the same kind of miss: when the change under test bumps a pin, a
    stale checkout builds the revision the change is replacing and verifies nothing."""
    needed = list(get_deps.deps_mandatory) + \
        [d for d, entry in get_deps.deps_optional.items() if family in entry[2].split()]
    out = []
    for d in needed:
        p = ROOT / d
        if not p.is_dir() or not any(f.name != '.git' for f in p.iterdir()):
            out.append(d)
            continue
        pin, head = get_deps.deps_all[d][1], dep_head(p)
        if head is not None and head != pin:
            out.append(f'{d} (at {head[:10]}, pinned {pin[:10]})')
    return out


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


def stale_options(build_dir, supplied):
    """{option: value} a build dir's CMake cache still carries from an earlier configure
    and this invocation does not set. cmake keeps a -D for the life of the dir and the
    build files act on it - family_support.cmake reads LOG, LOGGER and CFLAGS_CLI with
    if(DEFINED) and MAX3421_HOST with STREQUAL, a family.cmake reads what it likes
    (RHPORT_DEVICE) - so a shared dir silently builds, and the HIL flashes out of it, a
    configuration nobody asked for. Which names those are is no fixed list, so the entry's
    type answers instead of a whitelist: a -D no cmake code declares keeps UNINITIALIZED,
    the type only a command line gives, and the four tools/build.py passes on every
    configure are this run's own. An empty value is an option too: -DLOG= leaves a cache
    entry, if(DEFINED LOG) is true for it, and the build compiles with CFG_TUSB_DEBUG=.
    An option this run does set is no risk: its -D overwrites the cached value.
    Espressif builds one idf tree per example under the dir, each with a cache full of
    idf.py's own untyped defines; -D is refused for that family (build_one), so there only
    the sticky trio can have come from a command line."""
    out = {}
    root = ROOT / build_dir
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
            if name in STICKY_OPTIONS or (from_build_py and kind == 'UNINITIALIZED'):
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
    ensure_deps(family, fetch, verbose)
    name = board if shared else f'agent-{os.getpid()}-{board}'
    build_dir = f'cmake-build/cmake-build-{name}'
    if shared:
        stale = stale_options(build_dir, {d.partition('=')[0] for d in defines} |
                              ({'CFLAGS_CLI'} if cflags else set()))
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
