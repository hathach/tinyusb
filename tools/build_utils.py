#!/usr/bin/env python3
import argparse
import functools
import os
import shlex
import subprocess
import sys
import pathlib
import re

import get_deps

build_format = '| {:29} | {:30} | {:18} | {:7} | {:6} | {:6} |'

SUCCEEDED = "\033[32msucceeded\033[0m"
FAILED = "\033[31mfailed\033[0m"
SKIPPED = "\033[33mskipped\033[0m"


# Every read here is a source file, not user text: decode it the same way on every
# machine. Without this the reads take the locale's encoding, and one of the eight
# tracked non-ASCII files this now touches (hw/bsp/nrf/boards/nrf54lm20dk/board.cmake
# among them) raises UnicodeDecodeError under LC_ALL=C - a ValueError, which sails
# straight through the `except OSError` fail-opens.
_TEXT = {'encoding': 'utf-8', 'errors': 'replace'}

_FAMILY_MCUS_RE = re.compile(r'set\s*\(\s*FAMILY_MCUS\s+([^)]*)\)')
# cmake command names are case-insensitive, so SET(...) defines a variable as much
# as set(...) does; the name group keeps `SET(${OUTVAR} ...)` out either way.
_CMAKE_SET_RE = re.compile(r'set\s*\(\s*([A-Za-z_]\w*)\s+([^)\s]+)', re.IGNORECASE)
_CMAKE_VAR_RE = re.compile(r'\$\{([A-Za-z_]\w*)\}')
_CMAKE_CASE_RE = re.compile(r'string\s*\(\s*(TOUPPER|TOLOWER)\s+(\S+)\s+([A-Za-z_]\w*)\s*\)')



def _cwd_cache(fn):
    """lru_cache, keyed on the working directory as well as the arguments.

    Every cached helper below takes repo-RELATIVE paths ('hw/bsp/<fam>',
    'examples/<ex>/skip.txt', or the literal 'hw/bsp' glob), while ci_select._in_repo()
    chdirs around each call so one process can classify more than one tree - the
    code-size skill's base-vs-branch worktrees, /pre-pr, a test pointing at a fixture.
    Without the cwd in the key the second tree silently gets the first tree's
    skip.txt/only.txt and FAMILY_MCUS answers. Master had no caching here, so this
    hazard arrived with it."""
    cache = {}

    @functools.wraps(fn)
    def wrapper(*args):
        key = (os.getcwd(), args)
        if key not in cache:
            cache[key] = fn(*args)
        return cache[key]

    wrapper.cache_clear = cache.clear
    return wrapper

@_cwd_cache
def _cmake_sets(path):
    """One cmake file's variable assignments as NAME -> first definition seen, as
    either a literal value or an ('TOUPPER'|'TOLOWER', source) pair. Only used to
    expand ${...} tokens; never mutate the cached dict.

    string(TOUPPER ...) is not decoration: hw/bsp/maxim derives its ONLY FAMILY_MCUS
    entry that way (`string(TOUPPER ${MAX_DEVICE} MAX_DEVICE_UPPER)`), as do the eight
    at32 families, so dropping those lines left nine families with an empty MCU set."""
    try:
        text = pathlib.Path(path).read_text(**_TEXT)
    except OSError:
        return {}
    out = {}
    for line in text.splitlines():
        line = line.strip()
        if line.startswith('#'):
            continue
        m = _CMAKE_CASE_RE.match(line)
        if m:
            # strip quotes like the set() branch below: string(TOUPPER "${VAR}" DST) is
            # idiomatic cmake, and keeping them yields a '"NAME"' token that can never
            # equal a mcu: entry
            out.setdefault(m.group(3), (m.group(1), m.group(2).strip('"')))
            continue
        m = _CMAKE_SET_RE.match(line)
        if m:
            out.setdefault(m.group(1), m.group(2).strip('"'))
    return out


def _cmake_expand(value, files, depth=0):
    """`value` with every ${VAR} replaced, resolving each name against `files` in
    order, or None when any name resolves nowhere OR the result still carries a `${`.
    That last case is the one _CMAKE_VAR_RE cannot see - a hyphen in the name, a nested
    ${${X}}, an unterminated brace - where the loop below finds nothing to substitute
    and would otherwise hand the raw text back as if it were a resolved MCU name.
    Bounded depth: a cmake file may define a var in terms of another one, and a
    self-referential set() must not recurse forever."""
    if depth > 4:
        return None
    out = value
    for name in set(_CMAKE_VAR_RE.findall(value)):
        val = None
        for f in files:
            val = _cmake_sets(f).get(name)
            if val is not None:
                break
        if val is None:
            return None
        if isinstance(val, tuple):                     # string(TOUPPER src DST)
            src = _cmake_expand(val[1], files, depth + 1)
            if src is None:
                return None
            val = src.upper() if val[0] == 'TOUPPER' else src.lower()
        else:
            val = _cmake_expand(val, files, depth + 1)
            if val is None:
                return None
        out = out.replace('${' + name + '}', val)
    return None if '${' in out else out


@_cwd_cache
def _board_dirs(board):
    """(board_dir, family_dir) for a board name, or (None, None). Cached: skip_example
    is asked (board x example) times - 566k lstat calls per selector run without this,
    since the glob rescans every hw/bsp/*/boards for each example."""
    hits = list(pathlib.Path("hw/bsp").glob("*/boards/" + board))
    if not hits:
        return None, None
    return hits[0], hits[0].parent.parent


@_cwd_cache
def _family_mcus(family_dir, board_dir):
    """The MCU names CMake's family_filter iterates. family_support.cmake:176/190
    loop `foreach(MCU IN LISTS FAMILY_MCUS)`, so a family-wide list (broadcom_64bit
    sets "BCM2711 BCM2835") makes ANY of its entries decide skip.txt/only.txt -- not
    just the one CFG_TUSB_MCU the configured board names.

    ${...} tokens are expanded from `set(VAR value)` and `string(TOUPPER src VAR)` in
    the board's board.cmake first, then in family.cmake: hw/bsp/ra sets
    `FAMILY_MCUS RAXXX ${MCU_VARIANT}` and ra6m5_ek/board.cmake sets MCU_VARIANT ra6m5,
    which is the token dual/host_info_to_device_cdc/only.txt actually spells; hw/bsp/maxim
    sets `FAMILY_MCUS ${MAX_DEVICE_UPPER}`, upper-cased from the board's MAX_DEVICE. A
    token resolving nowhere is dropped (nothing can be said about it).

    A family that never spells `set(FAMILY_MCUS ...)` at all gets one more chance: the
    name is resolved as a variable, which covers the derived form hw/bsp/espressif uses
    (`string(TOUPPER ${IDF_TARGET} FAMILY_MCUS)`).

    Only unconditional set() calls count: nrf and mcx pick FAMILY_MCUS per board
    inside if() blocks this does not evaluate, so for those two families the whole
    cmake-side MCU set is whatever the CFG_TUSB_MCU scrape in _board_mcu finds.

    nrf: the scrape reads the FIRST CFG_TUSB_MCU token of hw/bsp/nrf/family.mk, so
    every nrf board answers NRF54, the NRF5X ones included. Harmless only because no
    skip.txt/only.txt names an nrf token today.

    mcx: load-bearing, not academic -- mcu:MCXA15 is live in six examples' skip.txt
    (device/{cdc_msc,audio_test,hid_composite,audio_4_channel_mic,midi_test}_freertos
    and device/net_lwip_webserver). Those answers come out right only because the
    scrape falls through to each board's make-only board.mk, which still spells the
    token; an mcx board carrying board.cmake alone (MCU_VARIANT and no CFG_TUSB_MCU)
    would scrape 'NONE' and skip EVERY example on it, silently. TestFamilyMcusFallback
    fails the day such a board lands. The fix then is to evaluate the
    if(MCU_VARIANT STREQUAL ...) branches, not to add another scrape.
    """
    fam_cmake = pathlib.Path(family_dir) / "family.cmake"
    try:
        text = fam_cmake.read_text(**_TEXT)
    except OSError:
        return frozenset()
    board_cmake = pathlib.Path(board_dir) / "board.cmake"
    out = set()
    depth = 0
    any_set = False
    for line in text.splitlines():
        line = line.strip()
        m = _FAMILY_MCUS_RE.match(line)
        if m:
            any_set = True
        if m and depth == 0:
            files = (str(board_cmake), str(fam_cmake))
            for tok in m.group(1).split():
                if tok in ("CACHE", "INTERNAL") or tok.startswith('"'):
                    continue
                val = _cmake_expand(tok, files)
                if val:
                    out.add(val)
        if re.match(r'if\s*\(', line):
            depth += 1
        elif re.match(r'endif\s*\(', line):
            depth = max(0, depth - 1)
    if not out and not any_set:
        # FAMILY_MCUS can also be produced rather than set: hw/bsp/espressif derives it
        # with `string(TOUPPER ${IDF_TARGET} FAMILY_MCUS)`, which _FAMILY_MCUS_RE cannot
        # see, leaving espressif's whole cmake answer resting on the IDF_TARGET scrape.
        #
        # `not any_set` is load-bearing: _cmake_sets is if()-blind and keeps the FIRST
        # definition, so on a family that sets FAMILY_MCUS only inside conditionals
        # (mcx, nrf) this would leak branch one's value onto every board - mcx/frdm_mcxn947
        # answered MCXA15, which six examples' skip.txt names, dropping 12 firmware
        # images CMake actually builds. Those families keep the CFG_TUSB_MCU scrape.
        val = _cmake_expand('${FAMILY_MCUS}', (str(board_cmake), str(fam_cmake)))
        if val:
            out.add(val)
    return frozenset(out)


@_cwd_cache
def _scrape_mcu(family_dir, board_dir, family):
    """(CFG_TUSB_MCU token of this board, the text it was read from), master's
    algorithm verbatim: family.mk (family.cmake when there is none) first, falling
    back to the board's board.mk (board.cmake when there is none) only when the
    family file names no token at all. espressif spells its MCU as
    `set(IDF_TARGET "...")` instead. The text comes back with it because the make
    path reads MAX3421_HOST out of that same single file - which file that is IS
    part of master's answer, so it cannot be re-derived by the caller."""
    family_mk = family_dir / "family.mk"
    if not family_mk.exists():
        family_mk = family_dir / "family.cmake"
    mk_contents = family_mk.read_text(**_TEXT)

    # Find the mcu, first in family mk then board mk
    if "CFG_TUSB_MCU=OPT_MCU_" not in mk_contents:
        board_mk = board_dir / "board.mk"
        if not board_mk.exists():
            board_mk = board_dir / "board.cmake"
        mk_contents = board_mk.read_text(**_TEXT)

    mcu = "NONE"
    if family == "espressif":
        for line in mk_contents.splitlines():
            match = re.search(r'set\(IDF_TARGET\s+"([^"]+)"\)', line)
            if match:
                mcu = match.group(1).upper()
                break
    else:
        for token in mk_contents.split():
            if "CFG_TUSB_MCU=OPT_MCU_" in token:
                # Strip " because cmake files has them.
                token = token.strip("\"")
                _, opt_mcu = token.split("=")
                mcu = opt_mcu[len("OPT_MCU_"):]
            if mcu != "NONE":
                break
    return mcu, mk_contents


@_cwd_cache
def _board_mcu(board_dir, family_dir, family):
    """(CFG_TUSB_MCU of this board, MAX3421_HOST enabled by its cmake BSP).

    MAX3421_HOST is read from family.cmake AND board.cmake rather than only the file
    the MCU token came from: feather_rp2040_max3421 sets it in its board.cmake while
    its MCU token comes from rp2040's family file, and family_support.cmake:940
    appends MAX3421 to FAMILY_MCUS for it. board.mk is deliberately not read - a
    make-only option compiles nothing in a cmake build (and the make path answers
    with master's own single-file scrape, see _skip_example_make)."""
    family_dir = pathlib.Path(family_dir)
    board_dir = pathlib.Path(board_dir)
    mcu, _ = _scrape_mcu(family_dir, board_dir, family)
    if "${" in mcu:
        # the scrape is textual, so a computed token comes back verbatim
        # (tm4c board.cmake spells OPT_MCU_TM4C${MCU_SUB_VARIANT}, maxim
        # OPT_MCU_${MAX_DEVICE_UPPER}). Expand it the same way FAMILY_MCUS tokens are;
        # what still will not resolve stays as-is and _skip_example treats it as
        # "MCU unknown" rather than silently matching no mcu: token at all.
        mcu = _cmake_expand(mcu, (str(board_dir / "board.cmake"),
                                  str(family_dir / "family.cmake"))) or mcu

    max3421_enabled = False
    for f in (family_dir / "family.cmake", board_dir / "board.cmake"):
        try:
            text = f.read_text(**_TEXT)
        except OSError:
            continue
        # a commented-out `# set(MAX3421_HOST 1)` (feather_nrf52840_express) enables
        # nothing; master never hit one because it only read the MCU token's file
        if any(not l.lstrip().startswith('#') and
               ("MAX3421_HOST=1" in l or 'MAX3421_HOST 1' in l)
               for l in text.splitlines()):
            max3421_enabled = True
            break

    return mcu, max3421_enabled


@_cwd_cache
def _filter_tokens(path):
    """skip.txt / only.txt as a token set, or None when the file does not exist."""
    f = pathlib.Path(path)
    return frozenset(f.read_text(**_TEXT).split()) if f.exists() else None


def skip_example(example, board, extra_defines=(), build_system='cmake'):
    """Is this example unbuildable on this board, for this build system?

    The two build systems ask DIFFERENT questions and must not share an answer:

    'cmake' mirrors CMake's family_filter (hw/bsp/family_support.cmake:171-207),
    including the whole FAMILY_MCUS list the family.cmake sets.

    'make' is master's original algorithm, unchanged. family.mk and family.cmake are
    not the same build: hw/bsp/lpc54/family.cmake sets FAMILY_MCUS LPC54 and wires the
    ohci host sources, while family.mk builds OPT_MCU_LPC54XXX and compiles no HCD
    source at all -- feeding the cmake MCU union to a make build un-skips the host
    examples only.txt gates on mcu:LPC54 and they fail to link (undefined hcd_init).

    extra_defines: NAME=VALUE tokens the build passes on the command line
    (build.py -D). MAX3421_HOST=1 there enables the max3421 host controller
    exactly like a BSP that sets it, and family_support.cmake:940 appends MAX3421
    to FAMILY_MCUS for it -- so a roster board whose MAX3421 comes from the build
    args (metro_m4_express) must resolve its only.txt the same way. cmake only:
    master's make algorithm never looked at them.
    """
    return _skip_example(example, board, tuple(extra_defines), build_system)


@_cwd_cache
def _skip_example_make(example, board):
    """master's skip_example, verbatim (tools/build_utils.py @ 9c202e8c6): the
    make build's own answer, derived from family.mk/board.mk with the single
    CFG_TUSB_MCU token that file names. Do not "improve" it -- it is the mirror of
    what `make BOARD=... all` actually compiles."""
    ex_dir = pathlib.Path('examples/') / example

    # board within family
    board_dir, family_dir = _board_dirs(board)
    if board_dir is None:
        # Skip unknown boards
        return True
    family = family_dir.name

    mcu, mk_contents = _scrape_mcu(family_dir, board_dir, family)

    # Skip all OPT_MCU_NONE these are WIP port
    if mcu == "NONE":
        return True

    max3421_enabled = False
    for line in mk_contents.splitlines():
        if "MAX3421_HOST=1" in line or 'MAX3421_HOST 1' in line:
            max3421_enabled = True
            break

    skip_file = ex_dir / "skip.txt"
    only_file = ex_dir / "only.txt"

    if skip_file.exists():
        skips = skip_file.read_text(**_TEXT).split()
        if ("mcu:" + mcu in skips or
            "board:" + board in skips or
            "family:" + family in skips):
            return True

    if only_file.exists():
        onlys = only_file.read_text(**_TEXT).split()
        if not ("mcu:" + mcu in onlys or
                ("mcu:MAX3421" in onlys and max3421_enabled) or
                "board:" + board in onlys or
                "family:" + family in onlys):
            return True

    return False


@_cwd_cache
def _skip_example(example, board, extra_defines, build_system):
    if build_system == 'make':
        return _skip_example_make(example, board)

    ex_dir = pathlib.Path('examples/') / example

    # board within family
    board_dir, family_dir = _board_dirs(board)
    if board_dir is None:
        # Skip unknown boards
        return True
    family = family_dir.name

    mcu, max3421_enabled = _board_mcu(str(board_dir), str(family_dir), family)

    # Skip all OPT_MCU_NONE these are WIP port
    if mcu == "NONE":
        return True

    if any(t.strip().strip('"') == "MAX3421_HOST=1" for t in extra_defines):
        max3421_enabled = True

    mcus = set(_family_mcus(str(family_dir), str(board_dir)))
    if "${" not in mcu:
        mcus.add(mcu)
    if not mcus:
        # nothing resolved: neither FAMILY_MCUS nor the scraped CFG_TUSB_MCU token
        # yielded a name. Answering "skip" here would silently drop EVERY example on
        # the board (an only.txt can then never match), so say "buildable" and let
        # the real filter decide - build.py checks the targets CMake actually
        # registered, and CMake itself is the authority on the make/cmake legs.
        return False
    if max3421_enabled:
        mcus.add("MAX3421")                      # family_support.cmake:940

    keys = {"board:" + board, "family:" + family} | {"mcu:" + m for m in mcus}

    skips = _filter_tokens(str(ex_dir / "skip.txt"))
    if skips is not None and (skips & keys):
        return True

    onlys = _filter_tokens(str(ex_dir / "only.txt"))
    if onlys is not None and not (onlys & keys):
        return True

    return False


HEAD_UNKNOWN = object()


def dep_head(path):
    """The dep checkout's commit; None when git would answer for the enclosing tinyusb
    repo (no .git of its own, a vendored copy), so the pin cannot apply; HEAD_UNKNOWN
    when the dir has its own .git that names no commit - a git-init without a fetch, or
    a checkout broken partway - where the revision built is anybody's guess."""
    if not (path / '.git').exists():
        return None
    r = subprocess.run(['git', '-C', str(path), 'rev-parse', 'HEAD'], capture_output=True, text=True)
    head = r.stdout.strip()
    return head if r.returncode == 0 and head else HEAD_UNKNOWN


def missing_deps(family, root=None):
    """The family's dependencies that are not what get_deps.py's table asks for, each
    named with why. Present means content, not a directory: get_deps.py git-inits the dep
    dir before fetching and exits 0 whatever the fetch did (its run_cmd's status is
    ignored), so a fetch that failed leaves a dir holding nothing but .git. A checkout at
    another commit is the same kind of miss: when the change under test bumps a pin, a
    stale checkout builds the revision the change is replacing and verifies nothing."""
    root = pathlib.Path(root) if root else pathlib.Path(__file__).resolve().parents[1]
    needed = list(get_deps.deps_mandatory) + \
        [d for d, entry in get_deps.deps_optional.items() if family in entry[2].split()]
    out = []
    for d in needed:
        p = root / d
        if not p.is_dir() or not any(f.name != '.git' for f in p.iterdir()):
            out.append(d)
            continue
        pin, head = get_deps.deps_all[d][1], dep_head(p)
        if head is HEAD_UNKNOWN:
            out.append(f'{d} (revision unknown, pinned {pin[:10]})')
        elif head is not None and head != pin:
            out.append(f'{d} (at {head[:10]}, pinned {pin[:10]})')
    return out


def build_size(make_cmd):
    size_output = subprocess.run(make_cmd + ' size', shell=True, stdout=subprocess.PIPE).stdout.decode("utf-8").splitlines()
    for i, l in enumerate(size_output):
        text_title = 'text	   data	    bss	    dec'
        if text_title in l:
            size_list = size_output[i+1].split('\t')
            flash_size = int(size_list[0])
            sram_size = int(size_list[1]) + int(size_list[2])
            return (flash_size, sram_size)

    return (0, 0)


class BoardInfoError(ValueError):
    pass


def _cmake_set_values(path, name):
    """Every uncommented set(NAME value) of one variable in a cmake file, in order."""
    try:
        text = pathlib.Path(path).read_text(**_TEXT)
    except OSError:
        return []
    out = []
    for line in text.splitlines():
        line = line.strip()
        if line.startswith('#'):
            continue
        m = _CMAKE_SET_RE.match(line)
        if m and m.group(1) == name:
            out.append(m.group(2).strip('"'))
    return out


def _expand_or_refuse(value, files, depth=0):
    """`value` with every ${VAR} replaced from the first of `files` that sets it.
    Unlike _cmake_expand this refuses a variable set more than once in that file:
    those are conditional branches (`if (M4 STREQUAL "1") set(JLINK_CORE _M4)`)
    the textual parser cannot pick between, and a debug probe pointed at the
    wrong core is worse than no answer."""
    if depth > 4:
        raise BoardInfoError(f'{value}: expansion too deep')
    out = value
    for name in set(_CMAKE_VAR_RE.findall(value)):
        for f in files:
            vals = _cmake_set_values(f, name)
            if vals:
                break
        else:
            raise BoardInfoError(f'${{{name}}} is set in none of {", ".join(files)}')
        distinct = list(dict.fromkeys(vals))
        if len(distinct) > 1:
            raise BoardInfoError(f'${{{name}}} is set conditionally in {f}: '
                                 f'{", ".join(distinct)} - pass the value by hand')
        out = out.replace('${' + name + '}', _expand_or_refuse(distinct[0], files, depth + 1))
    if '${' in out:
        raise BoardInfoError(f'{value}: cannot expand')
    return out


def _board_files(board):
    if not pathlib.Path('hw/bsp').is_dir():
        raise BoardInfoError('no hw/bsp here - run from the tinyusb root')
    if not re.fullmatch(r'[A-Za-z0-9_.-]+', board):
        raise BoardInfoError(f'{board!r} is not a board name')   # _board_dirs would glob it
    hits = sorted(pathlib.Path('hw/bsp').glob('*/boards/' + board))
    if not hits:
        raise BoardInfoError(f'unknown board {board}')
    if len(hits) > 1:
        raise BoardInfoError(f'{board} exists in several families: ' + ', '.join(map(str, hits)))
    board_dir = hits[0]
    return board_dir, (str(board_dir / 'board.cmake'), str(board_dir.parent.parent / 'family.cmake'))


def _checked_device(value, source):
    """A J-Link device name as the BSPs and Ozone projects spell it (LPC11U37/401,
    XMC4500-1024). A scrape that yields anything else reaches a probe's command line
    as an option rather than a device, so refuse it like _board_files refuses a board."""
    if not re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9_./+-]*', value):
        raise BoardInfoError(f'{value!r} in {source} is not a J-Link device name')
    return value


def board_jlink(board):
    """The board's J-Link device name from its board.cmake, else its family.cmake,
    ${...} expanded. A conditional or unresolvable definition is refused unless
    the board's reference Ozone project names the device (it is what a trace
    capture drives anyway)."""
    _, files = _board_files(board)
    for f in files:
        vals = _cmake_set_values(f, 'JLINK_DEVICE')
        if vals:
            break
    else:
        raise BoardInfoError(f'{board}: no JLINK_DEVICE in board.cmake or family.cmake')
    distinct = list(dict.fromkeys(vals))
    try:
        if len(distinct) > 1:
            raise BoardInfoError(f'{board}: JLINK_DEVICE is set conditionally in {f}: '
                                 f'{", ".join(distinct)} - pass the device by hand')
        return _checked_device(_expand_or_refuse(distinct[0], files), f)
    except BoardInfoError as e:
        jdebug = board_jdebug(board)
        if not jdebug:
            raise
        m = re.search(r'^\s*Project\.SetDevice\s*\(\s*"([^"]+)"', pathlib.Path(jdebug).read_text(**_TEXT), re.M)
        if not m:
            raise BoardInfoError(f'{e}; {jdebug} names no device either')
        return _checked_device(m.group(1), jdebug)


def board_jdebug(board):
    """The board's reference Ozone project (hw/bsp/<family>/boards/<board>/ozone/*.jdebug),
    or None; refuses when there are several."""
    board_dir, _ = _board_files(board)
    hits = sorted((board_dir / 'ozone').glob('*.jdebug'))
    if len(hits) > 1:
        raise BoardInfoError(f'{board}: several reference projects: '
                             + ', '.join(str(h) for h in hits))
    return str(hits[0]) if hits else None


def main(argv=None):
    p = argparse.ArgumentParser(description='Board facts for the debug skills; run from the tinyusb root.')
    sub = p.add_subparsers(dest='cmd', required=True)
    bi = sub.add_parser('board-info', help='J-Link device and reference Ozone project of a '
                                           'board, as shell assignments')
    bi.add_argument('board')
    args = p.parse_args(argv)
    try:
        device = board_jlink(args.board)
        jdebug = board_jdebug(args.board)
    except BoardInfoError as e:
        print(f'error: {e}', file=sys.stderr)
        return 1
    print(f'JLINK_DEVICE={shlex.quote(device)}')
    print(f'JDEBUG={shlex.quote(jdebug) if jdebug else ""}')
    return 0


if __name__ == '__main__':
    sys.exit(main())
