#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""PR-diff -> CI selection: which rig boards and which tests a change can affect.

Lives in tools/ so it can serve both HIL selection and, from Task 3, build-family
selection. Stdlib-only (runs on bare CI runners; imports hil_util for the example
rosters, never hil_test/pyserial — test_hil_util.BottomLayer enforces the stdlib
closure). Fail-open: any file no rule classifies forces the full matrix. See
docs/superpowers/specs/2026-07-29-hil-pr-scoped-selection-design.md and
docs/superpowers/specs/2026-08-19-ci-build-family-filter-design.md.

JSON: full, boards (name -> 'all' | [tests]), families (bsp families the diff
touches, including ones with no rig board - build-only consumers such as /pre-pr
sample from these), args (hil_test.py args per config) and args_flasher (the same
args split by each board's flasher, for CI legs that split one rig by flasher).
"""
import argparse
import ast
import contextlib
import functools
import glob
import io
import json
import os
import re
import subprocess
import sys

# tools/ -> repo root is ONE level up. Guarded by TestModuleMove.test_repo_root_guard:
# a wrong parent count here silently re-points every repo-relative glob (it happened
# at the helper/ move).
_REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(_REPO_ROOT, 'test', 'hil'))  # for `from helper...`
from helper.hil_util import device_tests, dual_tests, host_test

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))   # tools/, for build helpers
import build_utils
import build as build_py

ALL_TESTS = {'device': device_tests, 'dual': dual_tests, 'host': host_test}

# class dir -> config macro suffix exceptions (rule 3); dfu is per-file, handled inline
NET_MACROS = ('ECM_RNDIS', 'NCM')


def _read(path: str) -> str:
    """Read a source file with a fixed encoding. The locale's is not it: several tracked
    sources carry non-ASCII bytes, and under LC_ALL=C the decode raises UnicodeDecodeError
    - a ValueError, which every `except OSError` fail-open below would let through as a
    traceback instead of a full matrix."""
    with open(path, encoding='utf-8', errors='replace') as f:
        return f.read()


_NONCODE_RE = re.compile(
    r'^(docs/|\.claude/|.*\.(md|rst)$|LICENSE)')
# Build-size metrics tooling. HIL axis ONLY: nothing on the rig runs any of it, and
# without this rule these paths are unclassified, so a metrics-only PR booked an
# exclusive full 30-board sweep to validate a script no board executes.
# The BUILD axis deliberately keeps its full-matrix answer: `tinyusb_metrics` runs
# tools/metrics.py as a build target (examples/CMakeLists.txt), and build_util.yml adds
# `--target tinyusb_metrics` to every metrics leg - a break in it fails the build, so a
# build has to exercise it.
_METRICS_RE = re.compile(
    r'^(tools/metrics[^/]*\.py$|\.github/scripts/metrics_[^/]*\.py$)')
_FULL_RE = re.compile(
    r'^(src/common/|src/osal/|src/tusb\.c$|src/tusb\.h$|src/tusb_option\.h$|'
    r'test/hil/|\.github/workflows/build.*\.yml$|\.github/actions/|\.github/scripts/|'
    r'tools/build\.py$|tools/cmake/|'
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
        text = _read(os.path.join(repo_root, 'hw/bsp/family_support.cmake'))
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
        text = _read(path)
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
    """Build options a board has truthy: each variant's defines (NAME=VALUE) and raw
    CFLAGS (-DNAME=VALUE), plus whatever its own board.cmake sets (a board can enable a
    gated port without the roster saying so). A board whose option is always on carries
    a single variant named after itself - metro_m4_express and MAX3421_HOST=1, which is
    what makes it the one rig board that compiles hcd_max3421.c."""
    toks = []
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
def path_families(rel_dir: str, repo_root: str) -> set:
    """Board families whose family.cmake (or espressif component CMakeLists)
    references rel_dir at a directory boundary. CMake only, on every axis: CMake
    is the first-class build system and Make follows it, so family.mk is never
    read - a port wired up in family.mk alone (microchip/pic32mz) is built by no
    CI job and resolves to nothing. Boundary = '/', whitespace, quote, paren,
    brace or end: `${TOP}/hw/mcu/nordic/nrfx` has no trailing slash, while bare
    'microchip/pic' must not match '.../microchip/pic32mz/...'."""
    pat = re.compile(re.escape(rel_dir) + r'(?=[/\s"\')}]|$)', re.M)
    return {fam for fam, text in _family_file_texts(repo_root) if pat.search(text)}


@functools.lru_cache(maxsize=None)
def _family_file_texts(repo_root: str) -> tuple:
    """((family, text), ...) for every family.cmake and espressif component
    CMakeLists.txt, read once. path_families is called per distinct directory in the
    diff and its own cache only helps repeats: a 6,000-file hw/mcu dep bump re-read
    these 84 files 99,892 times (2.2 s) before this."""
    bsp_root = os.path.join(repo_root, 'hw/bsp')
    out = []
    for f in sorted(glob.glob(os.path.join(bsp_root, '*/family.cmake')) +
                    glob.glob(os.path.join(bsp_root, '*/components/*/CMakeLists.txt'))):
        try:
            out.append((os.path.relpath(f, bsp_root).split(os.sep, 1)[0], _read(f)))
        except OSError:
            pass
    return tuple(out)


def port_families(port_dir: str, repo_root: str) -> set:
    # 'portable/', not 'src/portable/': family.cmake always spells the full literal
    # path ('${TOP}/src/portable/...'), but espressif's component CMakeLists.txt
    # assigns 'src' into a ${tusb_src} variable first (`${tusb_src}/portable/...`),
    # so a leading 'src/' in the needle would never match there and silently drop
    # espressif boards (see TestRealRosterPortFamilies).
    return path_families('portable/' + port_dir, repo_root)


def mcu_families(path: str, repo_root: str) -> set:
    """Families referencing a changed hw/mcu path: longest resolving dir prefix,
    hw/mcu/<vendor>/<sub>/... down to hw/mcu/<vendor>."""
    parts = path.split('/')
    for n in range(len(parts) - 1, 2, -1):
        fams = path_families('/'.join(parts[:n]), repo_root)
        if fams:
            return fams
    return set()


GET_DEPS_PATH = 'tools/get_deps.py'
_DEPS_DICTS = ('deps_mandatory', 'deps_optional')


def _deps_split(text: str):
    """(module dump with the two dep-dict assigns removed, {dict name: entries}).
    Parsed with ast, never exec'd: this runs on PR content."""
    mod = ast.parse(text)
    dicts, rest = {}, []
    for node in mod.body:
        if (isinstance(node, ast.Assign) and len(node.targets) == 1 and
                isinstance(node.targets[0], ast.Name) and
                node.targets[0].id in _DEPS_DICTS and isinstance(node.value, ast.Dict)):
            dicts[node.targets[0].id] = ast.literal_eval(node.value)
        else:
            rest.append(node)
    mod.body = rest
    # annotate_fields=False keeps the dump readable-length; line numbers are not
    # included unless asked for, so reformatting alone never reads as a logic change
    return ast.dump(mod, annotate_fields=False), dicts


# Family tokens in tools/get_deps.py that name no hw/bsp directory. get_deps matches a
# token against a requested family name verbatim (`f in deps_optional[d][2].split()`),
# so a token like these matches nothing - a stale spelling in get_deps.py, not a
# selector bug, and out of scope to change here. Pinned so that any OTHER unresolvable
# token (real drift) falls open to the full matrix instead of silently selecting
# nothing, and so TestOrphanInvariant fails the day one is fixed or a new one appears.
#   sam3x, samd21, samd51, same5x -> pre-rename spellings, listed alongside the current
#                                    samd2x_l2x / samd5x_e5x / same7x in the same entry
#   stm32l1, stm32l5 -> no hw/bsp family in the tree at all
_DEPS_ALIAS_TOKENS = frozenset({'sam3x', 'samd21', 'samd51', 'same5x',
                                'stm32l1', 'stm32l5'})


def get_deps_changed_families(base_text: str, head_text: str, repo_root: str):
    """Families whose tools/get_deps.py dep entries changed between two versions of
    the file, or None meaning 'cannot tell - use the full matrix'.

    None on: anything outside deps_mandatory/deps_optional differing (a logic change
    to get_deps affects every family), a mandatory `'all'` entry changing, a token
    that resolves to no family and is not a known alias, or text that will not parse.
    Callers with no base content at all - `--diff-file` mode has no git and therefore
    no merge-base blob - pass None themselves.

    An entry that is added, removed or edited contributes the family tokens of BOTH
    sides (a removed entry has only a base side). The two dicts are diffed SEPARATELY:
    merging them first would hide a move between deps_mandatory and deps_optional,
    which changes which families fetch the dep even though the value is untouched."""
    try:
        base_rest, base_d = _deps_split(base_text)
        head_rest, head_d = _deps_split(head_text)
    except (SyntaxError, ValueError, TypeError):
        return None
    if base_rest != head_rest:
        return None
    toks = set()
    for name in _DEPS_DICTS:
        base_x, head_x = base_d.get(name, {}), head_d.get(name, {})
        for key in set(base_x) | set(head_x):
            if base_x.get(key) == head_x.get(key):
                continue
            for entry in (base_x.get(key), head_x.get(key)):
                if entry and len(entry) > 2:
                    toks.update(str(entry[2]).split())
    if 'all' in toks:
        return None
    fams = set(all_bsp_families(repo_root))
    if toks - fams - _DEPS_ALIAS_TOKENS:
        # a changed entry we cannot map to a family. "changed but unmappable" is NOT
        # "nothing changed": reading it as the latter empties the entire build matrix
        # for a dep bump, so fall open instead
        return None
    return toks & fams


_CLS_INC_RE = re.compile(r'#\s*include\s*[<"]class/([^/"<>]+)/([^"<>]+)[">]')


@functools.lru_cache(maxsize=None)
def class_include_edges(repo_root: str) -> dict:
    """'<class>/<header>' -> the other class dirs that include it. A class header
    pulled in by a second class ships in every firmware enabling that second class:
    src/class/midi/midi{,2}_{device,host}.h include class/audio/audio.h, and
    net_device.h includes class/cdc/cdc.h. The class rule derives macros from the
    directory name alone, so without this edge a change to the included header
    selects only its own class's examples - and on a board that skips those (e.g.
    metro_m4_express skips audio_test_freertos), nothing at all.

    Derived from the actual #include lines rather than a hand-written table so it
    cannot rot when a class picks up or drops a cross-class include."""
    edges = {}
    for f in sorted(glob.glob(os.path.join(repo_root, 'src/class/*/*.[ch]'))):
        cls = os.path.basename(os.path.dirname(f))
        try:
            text = _read(f)
        except OSError:
            continue
        for inc_cls, inc_hdr in _CLS_INC_RE.findall(text):
            if inc_cls != cls:
                edges.setdefault(f'{inc_cls}/{inc_hdr}', set()).add(cls)
    return edges


_CLS_STEM_RE = re.compile(r'(.*?)(?:_(?:device|host))?\.[ch]$')


def class_macros(cls: str, base: str, prefix: str) -> list:
    """Config macros that compile a class dir's code, for role prefix TUD/TUH.
    `base` refines dfu (it splits DFU from DFU_RUNTIME per file) and adds the file's
    own macro where that differs from the directory's; pass '' for a class reached
    through an include edge, where the widest set is correct."""
    if cls == 'net':
        return [f'CFG_{prefix}_{m}' for m in NET_MACROS]
    if cls == 'dfu':
        if base.startswith('dfu_rt'):
            return [f'CFG_{prefix}_DFU_RUNTIME']
        if base.startswith('dfu_device') or base.startswith('dfu_host'):
            return [f'CFG_{prefix}_DFU']
        return [f'CFG_{prefix}_DFU', f'CFG_{prefix}_DFU_RUNTIME']
    out = [f'CFG_{prefix}_{cls.upper()}']
    # A class directory can hold more than one class. src/class/midi ships MIDI 1.0
    # AND MIDI 2.0: midi2_device.c is `#if CFG_TUD_ENABLED && CFG_TUD_MIDI2`, and
    # examples/device/midi2_device is the only example that enables it - so the
    # directory macro alone selected the midi_test examples, which do not compile the
    # changed file, and none of the ones that do. Union, never replace: the file may
    # still be pulled in by the directory's own macro, and over-selecting costs a build
    # while under-selecting merges a break.
    m = _CLS_STEM_RE.match(base)
    if m and m.group(1) and m.group(1) != cls:
        out.append(f'CFG_{prefix}_{m.group(1).upper()}')
    return out


# A define is OFF only when its value is a literal zero (0, 00, (0)), optionally
# followed by a comment. Anything else counts as ON - including a value this cannot
# evaluate, e.g. `#define CFG_TUH_MIDI CFG_TUH_DEVICE_MAX` (examples/host/midi_rx).
# Fail-open: reading such a define as OFF made midi_host.c select zero families and
# let a compile break merge green.
#
# A macro defined more than once is ON if ANY of its defines is non-zero, because
# the preprocessor branches are not evaluated here: uac2_speaker_fb defines
# CFG_TUD_HID 1 under `#if CFG_AUDIO_DEBUG` and 0 in the #else, and the default
# build (CFG_AUDIO_DEBUG defaults to 1) compiles the HID class in. Deciding on the
# LAST/only match found made that example invisible to CFG_TUD_HID changes.
_DEF_VALUE = r'^[ \t]*#[ \t]*define[ \t]+{}[ \t]+(\S[^\n]*?)[ \t]*$'
_DEF_ZERO_VALUE = re.compile(r'\(?\s*0+\s*\)?\s*(?://.*|/\*.*)?')


# Shared rule-recognition primitives. The two classifiers walk the same diff with
# different answers, but they must RECOGNISE the same things: one copy each, so a
# new naming convention cannot land in one walk and be missed by the other.
_PORT_PATH_RE = re.compile(r'src/portable/((?:[^/]+/)?[^/]+)/')


def _port_roles(base: str) -> set:
    """Which USB role a src/portable file serves, from its name: dcd_*/ *_device is
    the device-controller side, hcd_*/ *_host the host side, anything else (shared
    headers, glue) both."""
    if re.match(r'(dcd_|.*_device)', base):
        return {'device'}
    if re.match(r'(hcd_|.*_host)', base):
        return {'host'}
    return {'device', 'host'}


def _class_roles(base: str) -> set:
    """Same question for a src/class file: <cls>_device.[ch] / <cls>_host.[ch],
    else both - the class's shared header ships in either role."""
    if re.search(r'_device\.[ch]$', base):
        return {'device'}
    if re.search(r'_host\.[ch]$', base):
        return {'host'}
    return {'device', 'host'}


def _config_enables(cfg_path: str, macros) -> bool:
    try:
        with open(cfg_path, encoding='utf-8', errors='replace') as f:
            text = f.read()
    except OSError:
        return False
    for m in macros:
        for value in re.findall(_DEF_VALUE.format(m), text, re.M):
            if not _DEF_ZERO_VALUE.fullmatch(value):
                return True
    return False


def examples_enabling(pool, macros, repo_root: str) -> set:
    """The 'role/name' entries of `pool` whose src/tusb_config.h turns any of
    `macros` on. The pool differs per classifier (HIL test lists vs every example),
    the question does not."""
    return {ex for ex in pool
            if _config_enables(os.path.join(repo_root, 'examples', ex, 'src',
                                            'tusb_config.h'), macros)}


# cached: called per changed lib file, and the tree doesn't change mid-run
@functools.lru_cache(maxsize=None)
def lib_examples(lib_name: str, repo_root: str) -> set:
    """Examples whose OWN examples/<role>/<name>/{CMakeLists.txt,Makefile} references
    lib/<lib_name> at a directory boundary (same boundary rule as path_families, so
    'lib/net' cannot inherit lib/networking's example).

    Per-example on purpose: lib/SEGGER_RTT is named by family_support.cmake's
    LOGGER=rtt plumbing, which no CI example build turns on (all three references -
    family_support.cmake, family_support.mk, rp2040/family.cmake - sit inside a
    LOGGER=rtt guard), so a family-file scan would wrongly narrow it to three families
    instead of answering 'nobody'.

    The whole example TREE is scanned, not just its top-level files: examples/host/
    msc_file_explorer_freertos/src/CMakeLists.txt names lib/embedded-cli, and that
    example survived only because its top-level file happens to name it too."""
    pat = re.compile(re.escape('lib/' + lib_name) + r'(?=[/\s"\')}]|$)', re.M)
    out = set()
    for ex in all_examples(repo_root):
        for f in sorted(glob.glob(os.path.join(repo_root, 'examples', ex, '**', '*'),
                                  recursive=True)):
            if os.path.basename(f) not in ('CMakeLists.txt', 'Makefile'):
                continue
            try:
                text = _read(f)
            except OSError:
                continue
            if pat.search(text):
                out.add(ex)
                break
    return out


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
    return examples_enabling(role_tests({role}, extra_tests), macros, repo_root)


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


def _classify_one(path, repo_root, roster_boards, extras: set, s: _Sel,
                  get_deps_families=None):
    base = os.path.basename(path)
    if _NONCODE_RE.match(path):
        s.reasons.append(f'{path}: non-code, no contribution')
        return
    if _METRICS_RE.match(path):
        s.reasons.append(f'{path}: build-size metrics tooling, no HIL contribution')
        return
    if _FULL_RE.match(path):
        s.force_full(f'{path}: core/infra -> full matrix')
        return

    if path == GET_DEPS_PATH:
        if get_deps_families is None:
            s.force_full(f'{path}: dep changes not resolvable -> full matrix')
            return
        if not get_deps_families:
            s.reasons.append(f'{path}: no dep entry changed, no contribution')
            return
        fams = sorted(get_deps_families)
        s.families.update(fams)
        boards = [b['name'] for b in roster_boards
                  if board_family(b['name'], repo_root) in get_deps_families]
        s.roles.update(('device', 'host'))
        s.add(boards, 'all', f'{path}: dep entries changed -> families {fams} -> '
                             f'boards {boards}')
        return

    m = _PORT_PATH_RE.match(path)
    if m:
        port = m.group(1)
        roles = _port_roles(base)
        fams = port_families(port, repo_root)
        if not fams:
            # empty means empty (maintainer ruling), same reading as hw/mcu and as the
            # build walk: no family's build references this port, so nothing compiles it
            # and there is nothing to run. Forcing the full 30-board rig here bought no
            # coverage at all - the build side selected zero families for the same path.
            # Live for src/portable/template and the two microchip pic ports;
            # TestPortFamiliesCoverage is the drift guard for a port that stops resolving.
            s.reasons.append(f'{path}: port {port} maps to no board family, no contribution')
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
        roles = _class_roles(base)
        # this file's own class, plus any class whose headers include it
        via = sorted(class_include_edges(repo_root).get(f'{cls}/{base}', ()))

        def macros(prefix):
            return (class_macros(cls, base, prefix) +
                    [m2 for c in via for m2 in class_macros(c, '', prefix)])
        tests = set()
        if 'device' in roles:
            tests |= class_examples(macros('TUD'), 'device', repo_root, extras)
        if 'host' in roles:
            tests |= class_examples(macros('TUH'), 'host', repo_root, extras)
        boards = [b['name'] for b in roster_boards if board_roles(b) & roles]
        s.roles.update(roles)
        why = f'{path}: class {cls}' + (f' (+ included by {via})' if via else '')
        s.add(boards, tests, f'{why} -> {sorted(tests)} ({"/".join(sorted(roles))})')
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

    if re.match(r'hw/mcu/', path):
        fams = mcu_families(path, repo_root)
        if not fams:
            # empty means empty (maintainer ruling): if no family's build references
            # the path, no build consumes the change - there is nothing to compile,
            # so there is nothing to run either. TestOrphanInvariant's
            # test_tracked_mcu_vendors_resolve is the drift guard: a real vendor dir
            # that stops resolving fails pre-commit instead of silently vanishing
            s.reasons.append(f'{path}: hw/mcu path resolves to no family, no contribution')
            return
        s.families.update(fams)
        boards = [b['name'] for b in roster_boards
                  if board_family(b['name'], repo_root) in fams]
        s.roles.update(('device', 'host'))
        s.add(boards, 'all', f'{path}: mcu dir -> families {sorted(fams)} -> boards {boards}')
        return

    m = re.match(r'lib/([^/]+)/', path)
    if m:
        lib = m.group(1)
        # only the tests whose example builds the lib, and only those the rig runs
        tests = {e for e in lib_examples(lib, repo_root)
                 if any(e in pool for pool in ALL_TESTS.values()) or e in extras}
        if not tests:
            s.reasons.append(f'{path}: lib {lib} used by no HIL test, no contribution')
            return
        roles = set()
        for test in tests:
            r = test_role(test)
            roles.update(('device', 'host') if r == 'dual' else (r,))
        boards = [b['name'] for b in roster_boards]
        s.roles.update(roles)
        s.add(boards, sorted(tests), f'{path}: lib {lib} -> {sorted(tests)} on all boards')
        return

    m = _BUILD_EX_RE.match(path)
    if m:
        if m.group(1) not in _HIL_EX_ROLES:
            # examples/typec: the build matrix compiles it, nothing on the rig runs it
            s.reasons.append(f'{path}: {m.group(1)} example, no HIL contribution')
            return
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


def classify(changed_files, repo_root, rosters, get_deps_families=None):
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
        _classify_one(path, repo_root, all_boards, extras, s, get_deps_families)

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


def hil_examples(sel, rosters):
    """{board: examples hil-build must produce}: the board's selected tests plus
    device/board_test, which hil_test.py flashes to park at every variant
    boundary and at end-of-board teardown. Emitted for full selections too - the
    HIL example universe is a fraction of the tree regardless of the diff."""
    by_name = {}
    for _, boards in rosters:
        for b in boards:
            by_name.setdefault(b['name'], []).append(b)
    if sel['full']:
        chosen = {n: 'all' for n in by_name}
    else:
        chosen = sel['boards']
    out = {}
    for name, tests in chosen.items():
        if tests == 'all':
            # a board named by two rosters (rig migration, or shared between rigs)
            # may run different tests on each: union them. Superset firmware costs a
            # build; a missing image fails the run on whichever rig lost the toss.
            run = set().union(*(board_tests(b) for b in by_name[name]))
        else:
            run = set(tests)
        out[name] = sorted(run | {'device/board_test'})
    return out


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


def merge_base(base, repo_root):
    return subprocess.run(['git', 'merge-base', 'HEAD', base], cwd=repo_root,
                          capture_output=True, text=True, check=True).stdout.strip()


def git_show(spec, repo_root):
    return subprocess.run(['git', 'show', spec], cwd=repo_root,
                          capture_output=True, text=True, check=True).stdout


def changed_files_from_git(base, repo_root):
    diff = subprocess.run(GIT_DIFF_ARGV + [f'{merge_base(base, repo_root)}..HEAD'],
                          cwd=repo_root, capture_output=True, text=True, check=True).stdout
    return [l for l in diff.splitlines() if l.strip()]


def get_deps_families_from_git(base, repo_root):
    """The changed dep entries' families for a --base run, or None (-> full matrix)
    if git cannot produce both sides of tools/get_deps.py."""
    try:
        mb = merge_base(base, repo_root)
        return get_deps_changed_families(git_show(f'{mb}:{GET_DEPS_PATH}', repo_root),
                                         git_show(f'HEAD:{GET_DEPS_PATH}', repo_root),
                                         repo_root)
    except (subprocess.CalledProcessError, OSError) as e:
        print(f'ci_select: {GET_DEPS_PATH}: base content unreadable ({e})', file=sys.stderr)
        return None


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    g = ap.add_mutually_exclusive_group(required=True)
    g.add_argument('--base', help='git ref to diff against (merge-base..HEAD)')
    g.add_argument('--diff-file', help='newline-separated changed-file list')
    ap.add_argument('configs', nargs='*', help='rig roster JSON file(s); omit for the build view alone')
    a = ap.parse_args()

    repo_root = _REPO_ROOT
    rosters = []
    for c in a.configs:
        with open(c, encoding='utf-8', errors='replace') as f:
            rosters.append((c, json.load(f)['boards']))

    files = (_read(a.diff_file).splitlines() if a.diff_file
             else changed_files_from_git(a.base, repo_root))
    files = [f for f in files if f.strip()]

    # --diff-file has no git and so no base content: the rule falls open to full
    gd = (get_deps_families_from_git(a.base, repo_root)
          if a.base and GET_DEPS_PATH in files else None)

    s = classify(files, repo_root, rosters, gd)
    s['args'] = selection_args(s, rosters)
    s['args_flasher'] = selection_args_by_flasher(s, rosters)
    if rosters:
        s['hil_examples'] = hil_examples(s, rosters)
    s['build'] = classify_build(files, repo_root, gd)
    for r in s['build']['reasons']:
        print(f'ci_select[build]: {r}', file=sys.stderr)
    for r in s['reasons']:
        print(f'ci_select: {r}', file=sys.stderr)
    print(json.dumps(s))


# -------------------------------------------------------------
# Build-axis classifier (spec rule table, docs/superpowers/specs/
# 2026-08-19-ci-build-family-filter-design.md). Independent of the HIL
# classifier: same diff, second walk, its own fail-open.
# -------------------------------------------------------------
# Both walks recognise an example path with the SAME regex, so a role can never be
# known to one walk and unclassified (-> full matrix) to the other. What differs is the
# answer: the rig runs device/host/dual tests, while the build matrix also compiles
# examples/typec, which nothing on the rig runs.
_EX_ROLES = ('device', 'dual', 'host', 'typec')
_HIL_EX_ROLES = ('device', 'host', 'dual')
_BUILD_EX_RE = re.compile(r'examples/(%s)/([^/]+)/' % '|'.join(_EX_ROLES))


@functools.lru_cache(maxsize=None)
def all_examples(repo_root: str) -> tuple:
    """Every examples/<role>/<name> with a CMakeLists.txt, as 'role/name'."""
    out = []
    for role in _EX_ROLES:
        for d in sorted(glob.glob(os.path.join(repo_root, 'examples', role, '*/'))):
            if os.path.isfile(os.path.join(d, 'CMakeLists.txt')):
                out.append(f'{role}/{os.path.basename(d.rstrip(os.sep))}')
    return tuple(out)


def role_examples(repo_root: str, roles) -> set:
    want = set(roles)
    return {e for e in all_examples(repo_root) if e.split('/', 1)[0] in want}


@functools.lru_cache(maxsize=None)
def all_bsp_families(repo_root: str) -> tuple:
    return tuple(sorted(d for d in os.listdir(os.path.join(repo_root, 'hw/bsp'))
                        if os.path.isdir(os.path.join(repo_root, 'hw/bsp', d))))


def _build_class_examples(cls: str, base: str, roles: set, repo_root: str) -> set:
    """Examples (all 46, not the HIL lists) whose tusb_config.h enables the class's
    macros for the given roles, plus classes that #include the changed header."""
    via = sorted(class_include_edges(repo_root).get(f'{cls}/{base}', ()))
    out = set()
    for prefix, role in (('TUD', 'device'), ('TUH', 'host')):
        if role not in roles:
            continue
        macros = class_macros(cls, base, prefix) + \
                 [m for c in via for m in class_macros(c, '', prefix)]
        out |= examples_enabling(all_examples(repo_root), macros, repo_root)
    return out


class _BSel:
    """family -> set(examples) | 'all', unioned per family."""
    def __init__(self):
        self.full = False
        self.fam_ex = {}
        self.reasons = []

    def add(self, fams, examples, reason):
        self.reasons.append(reason)
        for f in fams:
            cur = self.fam_ex.get(f)
            if examples == 'all' or cur == 'all':
                self.fam_ex[f] = 'all'
            else:
                self.fam_ex[f] = (cur or set()) | set(examples)

    def force_full(self, reason):
        self.full = True
        self.reasons.append(reason)


def _classify_build_one(path, repo_root, s: _BSel, get_deps_families=None):
    base = os.path.basename(path)
    if _NONCODE_RE.match(path):                                   # rule 1
        return
    if re.match(r'test/hil/', path):                              # rule 2
        s.reasons.append(f'{path}: HIL harness, no build contribution')
        return
    if path == GET_DEPS_PATH:                                     # get_deps rule
        if get_deps_families is None:
            s.force_full(f'{path}: dep changes not resolvable -> full build matrix')
            return
        if not get_deps_families:
            s.reasons.append(f'{path}: no dep entry changed, no contribution')
            return
        fams = sorted(get_deps_families)
        s.add(fams, 'all', f'{path}: dep entries changed -> families {fams}')
        return
    m = _PORT_PATH_RE.match(path)
    if m:                                                         # rules 3-5
        port = m.group(1)
        fams = port_families(port, repo_root)
        roles = _port_roles(base)
        exs = 'all' if roles == {'device', 'host'} else \
            role_examples(repo_root, tuple(roles) + ('dual',))
        s.add(fams, exs, f'{path}: port {port} -> families {sorted(fams)}')
        return
    if re.match(r'hw/bsp/[^/]+/', path):                          # rule 6
        fam = path.split('/')[2]
        s.add({fam}, 'all', f'{path}: bsp family {fam}')
        return
    if re.match(r'hw/mcu/', path):                                # rule 7
        fams = mcu_families(path, repo_root)
        if not fams:
            # empty means empty, same reading as the HIL walk: no family's build
            # references the path, so no build compiles it
            s.reasons.append(f'{path}: hw/mcu path resolves to no family, no contribution')
            return
        s.add(fams, 'all', f'{path}: mcu -> families {sorted(fams)}')
        return
    m = re.match(r'src/class/([^/]+)/', path)
    if m:                                                         # rules 8-10
        cls = m.group(1)
        roles = _class_roles(base)
        exs = _build_class_examples(cls, base, roles, repo_root)
        if not exs:
            # Empty means empty - maintainer decision. No example config enables this
            # class, so no build exercises it and
            # nothing is selected. The file IS still parsed by every full build
            # (src/CMakeLists.txt, src/tinyusb.mk list class sources unconditionally,
            # the CFG_ guard sits inside), so a break outside the guard surfaces on the
            # next master push - the accepted safety net.
            s.reasons.append(f'{path}: class {cls} enabled by no example config, '
                             f'no contribution')
            return
        s.add(all_bsp_families(repo_root), exs,
              f'{path}: class {cls} -> {sorted(exs)}')
        return
    m = re.match(r'src/(device|host)/', path)
    if m:                                                         # rules 11-12
        role = m.group(1)
        s.add(all_bsp_families(repo_root), role_examples(repo_root, (role, 'dual')),
              f'{path}: core {role} stack')
        return
    m = _BUILD_EX_RE.match(path)
    if m:                                                         # rules 13-14
        ex = f'{m.group(1)}/{m.group(2)}'
        if ex in all_examples(repo_root):
            s.add(all_bsp_families(repo_root), {ex}, f'{path}: example {ex}')
        else:
            # a deleted example builds nothing; removing it from the role
            # CMakeLists (rule 15) is what forces the full matrix
            s.reasons.append(f'{path}: not an example dir, no build contribution')
        return
    m = re.match(r'lib/([^/]+)/', path)
    if m:                                                         # lib rule
        lib = m.group(1)
        exs = lib_examples(lib, repo_root)
        if not exs:
            # empty means empty: no example's build pulls this lib in, so no build
            # compiles it (lib/SEGGER_RTT is only reached through LOGGER=rtt, which
            # no CI build sets)
            s.reasons.append(f'{path}: lib {lib} built by no example, no contribution')
            return
        s.add(all_bsp_families(repo_root), exs, f'{path}: lib {lib} -> {sorted(exs)}')
        return
    s.force_full(f'{path}: unclassified -> full build matrix')    # rules 15-17


@contextlib.contextmanager
def _in_repo(repo_root):
    """build_utils/build.py use repo-relative paths; scope a chdir around them.
    get_family_boards also prints on an empty family - swallow stdout so the
    selector's machine-read JSON stays clean (diagnostics belong on stderr)."""
    old = os.getcwd()
    os.chdir(repo_root)
    try:
        with contextlib.redirect_stdout(io.StringIO()):
            yield
    finally:
        os.chdir(old)


def _prune_buildable(fams, fam_ex, repo_root):
    """Intersect each family's selection with what the family can build at all
    (build_utils.skip_example - the same skip.txt/only.txt data CMake's
    family_filter reads).

    ANY board of the family counts, not just the one GHA's --one-first picks:
    CircleCI's cmake legs build every board of a family, so an example gated to a
    single board (only.txt board:mimxrt1060_evk) would otherwise lose ALL compile
    coverage exactly when a PR touches it. get_family_boards(.., False, False) is
    that full list, with the same CI skip lists the build jobs apply.

    EITHER build system counts too. This one list gates CircleCI's make legs as well
    as its cmake ones, and the two answer different questions (build_utils.skip_example):
    examples/device/dfu carries `mcu:BCM2835` in skip.txt, which the cmake FAMILY_MCUS
    union applies to every broadcom_64bit board while the make scrape applies it to
    none - asking cmake alone drops the only aarch64-gcc family in the matrix and
    `build-make-aarch64-gcc` stops compiling dfu at all."""
    out_fams, out_ex, reasons = [], {}, []
    allex = list(all_examples(repo_root))
    with _in_repo(repo_root):
        for fam in fams:
            if not os.path.isdir(os.path.join(repo_root, 'hw/bsp', fam, 'boards')):
                # a PR that deletes or renames hw/bsp/<fam> still names it in the
                # diff (rule 6); the family builds nothing now, and get_family_boards
                # would raise FileNotFoundError out of the whole selector
                reasons.append(f'{fam}: family dir gone from tree, dropped')
                continue
            try:
                # ci=True unconditionally: this answers "what will CI build", so it must
                # not change with GITHUB_ACTIONS/CIRCLECI being set. Locally the lists
                # are off by default, and rp2040 would keep feather_rp2040_max3421 -
                # the only board satisfying the max3421 only.txt files - giving a
                # developer a family list the runner will not reproduce.
                boards = build_py.get_family_boards(fam, False, False, ci=True)
            except OSError as e:                 # belt and braces: never traceback here
                reasons.append(f'{fam}: boards unreadable ({e}), dropped')
                continue
            if not boards:
                out_fams.append(fam)         # unknown layout: keep unfiltered
                continue
            # what this family's build path can even see, asked the same way for
            # every family. build.py's espressif branch builds get_examples('espressif')
            # only (the *_freertos examples plus a short extra list); keeping the family
            # for anything else spins up CI's most expensive leg to skip every example
            # it was given. Identical to the unfiltered list on all 81 other families.
            pool = set(build_py.get_examples(fam))
            try:
                buildable = [e for e in allex if e in pool and
                             any(not build_utils.skip_example(e, b) or
                                 not build_utils.skip_example(e, b, (), 'make')
                                 for b in boards)]
            except OSError as e:
                # a family mid-bring-up (boards/ but no family.cmake/family.mk yet)
                # reads as unbuildable to the scrape; keep it rather than tracebacking
                # out of the selector and losing the scoping for the whole PR
                reasons.append(f'{fam}: mcu scrape unreadable ({e}), kept unfiltered')
                out_fams.append(fam)
                continue
            want = fam_ex.get(fam)
            have = set(buildable)
            kept = buildable if want is None else [e for e in want if e in have]
            if not kept:
                continue                     # this diff builds nothing for this family
            out_fams.append(fam)
            if set(kept) != set(buildable):
                out_ex[fam] = kept
    return out_fams, out_ex, reasons


def classify_build(changed_files, repo_root, get_deps_families=None):
    s = _BSel()
    for p in changed_files:
        _classify_build_one(p, repo_root, s, get_deps_families)
    if s.full:
        return {'full': True, 'families': list(all_bsp_families(repo_root)),
                'family_examples': {}, 'reasons': s.reasons}
    fams = sorted(s.fam_ex)
    fam_ex = {f: sorted(e) for f, e in s.fam_ex.items() if e != 'all'}
    fams, fam_ex, pruned = _prune_buildable(fams, fam_ex, repo_root)
    s.reasons += pruned
    return {'full': False, 'families': fams, 'family_examples': fam_ex,
            'reasons': s.reasons}


if __name__ == '__main__':
    main()
