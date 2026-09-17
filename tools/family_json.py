#!/usr/bin/env python3
"""hw/bsp/family.json: what each board's default cmake configure selects, observed from
the configure's own outputs, never scraped from cmake text.

    {"<family>": {"<board>": {"cmake": {"defines": {...}, "family_mcus": [...],
                                        "mcu": "OPT_MCU_*", "options": {...},
                                        "portable": [...]}}}}

A row is written by tools/build.py after a default configure of the board (no -D or
--cflag, the gcc toolchain, the family's deps at their pins, into a dir that did not
exist before): family_support.cmake leaves family.json.part in the build dir and the
configure exports compile_commands.json; `observe` joins them and `merge` writes the row.
`cmake: null` marks a board of a Make-only family, and only the boards NULL_BOARDS lists.
`check` needs no toolchain: inventory both ways against hw/bsp/*/boards/*, field shapes,
and the canonical serialization byte for byte. `fix` is what pre-commit runs: it drops
rows of boards that are gone, configures once into a fresh private dir every board that
has no row and every board whose cmake the commit changes, rewrites the file canonical,
and exits 1 when it changed anything, so the commit is retried with the file staged, the
way codespell's fixes are. `refresh` is the release sweep: every cmake board re-observed the
same way, exit 1 when any board could not be. Between releases an edit to what every row is
observed through (src/common/tusb_mcu.h, hw/bsp/family_support.cmake) is not detected.
"""
import argparse
import json
import os
import re
import shlex
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CATALOG = ROOT / 'hw' / 'bsp' / 'family.json'
PART = 'family.json.part'
FIELDS = ('defines', 'family_mcus', 'mcu', 'options', 'portable')
# Make-only boards, the one hand-typed fact in the file
NULL_BOARDS = {'pic32mz': ('olimex_emz64', 'olimex_hmz144')}
DIRECTIVE = re.compile(r'^\s*#\s*(if|elif|ifdef|ifndef)\b(.*)$')
IDENT = re.compile(r'\b([A-Za-z_]\w*)\b')
USBIP = re.compile(r'^#define (TUP_USBIP_\w+)', re.M)
DEP_FLAGS = {'-MD', '-MMD', '-MP', '-c'}
DEP_FLAGS_ARG = {'-MF', '-MT', '-MQ', '-o'}


class Failure(Exception):
    """One line naming why no row can be written; the build's own result is unaffected."""


def canonical_text(data):
    return json.dumps(data, sort_keys=True, indent=2, ensure_ascii=False) + '\n'


def load(path=CATALOG):
    """The catalog, {} when the file does not exist; malformed JSON is a Failure, never
    something to replace, and so is a family that is not an object of board rows: past
    this point every caller may take both levels for mappings."""
    try:
        text = path.read_text(encoding='utf-8')
    except FileNotFoundError:
        return {}
    try:
        data = json.loads(text)
    except ValueError as e:
        raise Failure(f'{path.relative_to(ROOT)} is not valid JSON ({e}); fix it by hand')
    if not isinstance(data, dict):
        raise Failure(f'{path.relative_to(ROOT)} is not a JSON object')
    for family, rows in data.items():
        if not isinstance(rows, dict):
            kind = 'null' if rows is None else f'a {type(rows).__name__}'
            raise Failure(f'{family} is {kind}, not an object of board rows; fix it by hand')
    return data


def tested_identifiers(text):
    """The macro names src/common/tusb_mcu.h branches on: identifiers in its #if, #elif,
    #ifdef and #ifndef lines, continuation lines joined. CFG_TUSB_MCU is the row's own
    `mcu`, the OPT_MCU_* and TUP_* names are its outputs, `defined` is syntax."""
    names = set()
    joined = re.sub(r'\\\n', ' ', text)
    for line in joined.splitlines():
        m = DIRECTIVE.match(line)
        if not m:
            continue
        for n in IDENT.findall(m.group(2)):
            if n != 'defined' and n != 'CFG_TUSB_MCU' and not n.startswith(('OPT_MCU_', 'TUP_')) \
                    and not n[0].isdigit():
                names.add(n)
    return names


def compile_tokens(entry):
    """One compile command's tokens, response files expanded."""
    tokens = list(entry['arguments']) if 'arguments' in entry else shlex.split(entry['command'])
    out = []
    for t in tokens:
        if t.startswith('@') and len(t) > 1:
            try:
                out.extend(shlex.split(Path(entry['directory'], t[1:]).read_text(encoding='utf-8')))
            except OSError:
                out.append(t)
        else:
            out.append(t)
    return out


def preprocess_argv(entry):
    """The entry's command with everything that names its source, object or dependency
    output removed, so the same compiler and context can preprocess another file."""
    tokens = compile_tokens(entry)
    src = str(Path(entry['directory'], entry['file']).resolve())
    out, skip = [], False
    for t in tokens:
        if skip:
            skip = False
            continue
        if t in DEP_FLAGS_ARG:
            skip = True
            continue
        if t in DEP_FLAGS or any(t.startswith(f) and len(t) > len(f) for f in ('-MF', '-MT', '-MQ', '-o')):
            continue
        if t == entry['file'] or str(Path(entry['directory'], t).resolve()) == src:
            continue
        out.append(t)
    return out


def defines_of(tokens):
    """{name: value} for every -D on a command, '1' for a bare -DNAME."""
    out = {}
    for i, t in enumerate(tokens):
        if t == '-D' and i + 1 < len(tokens):
            d = tokens[i + 1]
        elif t.startswith('-D') and len(t) > 2:
            d = t[2:]
        else:
            continue
        name, eq, value = d.partition('=')
        out[name] = value if eq else '1'
    return out


def observe(build_dir, root=None):
    """(family, board, row, entries) from one configured build dir, or a Failure. Entries
    are the compile commands of TinyUSB translation units under src/; the row's `mcu`
    must agree across them and `defines` is their union, one value per key."""
    build_dir, root = Path(build_dir), Path(root or ROOT)
    try:
        part = json.loads((build_dir / PART).read_text(encoding='utf-8'))
    except OSError:
        raise Failure(f'no {PART} in {build_dir}: the configure did not reach the end of family_support.cmake')
    except ValueError as e:
        raise Failure(f'{PART} in {build_dir} is not valid JSON ({e})')
    try:
        db = json.loads((build_dir / 'compile_commands.json').read_text(encoding='utf-8'))
    except OSError:
        raise Failure(f'no compile_commands.json in {build_dir}')
    except ValueError as e:
        raise Failure(f'compile_commands.json in {build_dir} is not valid JSON ({e})')
    src = (root / 'src').resolve()
    entries = []
    for e in db:
        f = Path(e['directory'], e['file']).resolve()
        if src in f.parents:
            entries.append((f.relative_to(src).as_posix(), e))
    if not entries:
        raise Failure(f'no TinyUSB translation unit in {build_dir}/compile_commands.json')
    tested = tested_identifiers((root / 'src' / 'common' / 'tusb_mcu.h').read_text(encoding='utf-8'))
    mcu, defines, portable = None, {}, set()
    for rel, e in entries:
        d = defines_of(compile_tokens(e))
        m = d.get('CFG_TUSB_MCU')
        if m is None:
            raise Failure(f'{rel} compiles without CFG_TUSB_MCU')
        if mcu is None:
            mcu = m
        elif m != mcu:
            raise Failure(f'CFG_TUSB_MCU differs between translation units: {mcu} and {m} ({rel})')
        # the union: rp2040 adds CFG_TUH_MAX3421 to its host targets only, where
        # family_support.cmake puts it on every target; one key with two values is
        # a variant, not a configuration
        for k, v in d.items():
            if k in tested:
                if defines.get(k, v) != v:
                    raise Failure(f'{k} differs between translation units: {defines[k]} and {v} ({rel})')
                defines[k] = v
        if rel.startswith('portable/'):
            portable.add(rel[len('portable/'):])
    row = {
        'defines': defines, 'family_mcus': sorted(set(part['family_mcus'])), 'mcu': mcu,
        'options': dict(part.get('options', {})), 'portable': sorted(portable),
    }
    return part['family'], part['board'], row, [e for _, e in entries]


def usbips(argv, run=subprocess.run):
    """The TUP_USBIP_* a preprocessor run over src/tusb_option.h defines, None when the
    compiler is absent or the run fails, with the reason."""
    try:
        r = run(argv, capture_output=True, text=True, cwd=ROOT)
    except OSError as e:
        return None, str(e)
    if r.returncode != 0:
        first = next((l for l in r.stderr.splitlines() if l.strip()), f'exit {r.returncode}')
        return None, first
    return frozenset(USBIP.findall(r.stdout)), ''


SYNTHETIC_CONFIG = ['-UCFG_TUSB_CONFIG_FILE', '-DCFG_TUSB_CONFIG_FILE=<stdint.h>']
OPTION_H = str(ROOT / 'src' / 'tusb_option.h')


def real_probe_argv(entry):
    """The board's own compiler, with its context, over tusb_option.h and a synthetic
    config, so both probes read the same configuration."""
    return preprocess_argv(entry) + ['-E', '-dM', *SYNTHETIC_CONFIG, '-x', 'c', OPTION_H]


def host_probe_argv(row, cc='cc'):
    """What a consumer without the toolchain can reproduce from the row alone."""
    return [cc, '-E', '-dM', f'-DCFG_TUSB_MCU={row["mcu"]}',
            *[f'-D{k}={v}' for k, v in sorted(row['defines'].items())],
            *SYNTHETIC_CONFIG, '-I', str(ROOT / 'src'), '-x', 'c', OPTION_H]


def validate(row, entries, run=subprocess.run):
    """The row is valid when the board's real compiler probe succeeds; when the host probe
    succeeds too, both must select the same USB IPs, or the row's defines are not what
    tusb_mcu.h keys on (an SDK header decides, as for LPC54). A note when the host probe
    is unavailable: consumers then know the IPs only through the real compiler."""
    real, why = usbips(real_probe_argv(entries[0]), run)
    if real is None:
        raise Failure(f'real compiler probe failed: {why}')
    host, why = usbips(host_probe_argv(row), run)
    if host is None:
        return f'host probe unavailable ({why})'
    if host != real:
        raise Failure(f'host probe selects {sorted(host ^ real)} differently from the real compiler: '
                      f'the defines do not carry what tusb_mcu.h keys on')
    return ''


def merge(family, board, row, path=CATALOG):
    """Write the row into the catalog if it differs. Returns 'updated' or 'unchanged'."""
    def put(data):
        data.setdefault(family, {})[board] = {'cmake': row}
    return rewrite(put, path)


def rewrite(edit, path=CATALOG):
    """Read, apply `edit` to the data in place, compare, replace atomically, under a lock
    so two writers (boards configuring at once, a repair beside a build) cannot lose
    each other's change. Returns 'updated' or 'unchanged'."""
    lock = ROOT / 'cmake-build' / '.family.json.lock'
    lock.parent.mkdir(exist_ok=True)
    with open(lock, 'w') as lf:
        _lock(lf)
        data = load(path)
        edit(data)
        text = canonical_text(data)
        try:
            before = path.read_text(encoding='utf-8')
        except OSError:
            before = None
        if text == before:
            return 'unchanged'
        tmp = path.with_suffix('.json.tmp')
        tmp.write_text(text, encoding='utf-8')
        os.replace(tmp, path)
        return 'updated'


def _lock(f):
    try:
        import fcntl
        fcntl.flock(f, fcntl.LOCK_EX)
    except ImportError:
        import msvcrt
        msvcrt.locking(f.fileno(), msvcrt.LK_LOCK, 1)


def join(build_dirs):
    """(family, board, row, entries) over the dirs of one configure (Espressif has one
    per example): the same board everywhere, the row fields agreeing, `portable` the
    union."""
    family = board = row = None
    portable, entries = set(), []
    for d in build_dirs:
        f, b, r, es = observe(d)
        if row is None:
            family, board, row = f, b, r
        elif (f, b) != (family, board):
            raise Failure(f'{d} configured {b}, not {board}')
        else:
            for k in ('mcu', 'defines', 'family_mcus', 'options'):
                if r[k] != row[k]:
                    raise Failure(f'{k} differs between example trees: {row[k]} and {r[k]} ({d})')
        portable.update(r['portable'])
        entries.extend(es)
    if row is None:
        raise Failure('no configured build dir')
    row['portable'] = sorted(portable)
    return family, board, row, entries


def update(build_dirs, run=subprocess.run, path=CATALOG):
    """The on-the-fly write after a canonical configure: join, validate, merge. Returns
    the stderr line."""
    board = None
    try:
        family, board, row, entries = join(build_dirs)
        note = validate(row, entries, run)
        what = merge(family, board, row, path)
    except Failure as e:
        return f'family.json: {board or "?"} not updated: {e}'
    return f'family.json: {what} {board}' + (f' ({note})' if note else '')


FIX = 'python3 tools/family_json.py fix'


def inventory(root=ROOT):
    """family -> its board dirs, the boards the catalog must have rows for."""
    bsp = root / 'hw' / 'bsp'
    return {f.name: sorted(b.name for b in (f / 'boards').iterdir() if b.is_dir())
            for f in sorted(bsp.iterdir()) if (f / 'boards').is_dir()}


def check(path=CATALOG, root=ROOT):
    """Every violation, each one line; a family with no row at all is one line."""
    out = []
    try:
        data = load(path)
        text = path.read_text(encoding='utf-8')
    except (Failure, OSError) as e:
        return [f'{path.relative_to(root)}: {e}']
    if text != canonical_text(data):
        out.append(f'{path.relative_to(root)} is not in canonical form')
    tree = inventory(root)
    option_h = (root / 'src' / 'tusb_option.h').read_text(encoding='utf-8')
    mcus = set(re.findall(r'^#define (OPT_MCU_\w+)', option_h, re.M))
    for family, boards in tree.items():
        rows = data.get(family, {})
        missing = [b for b in boards if b not in rows]
        if missing == boards:
            out.append(f'{family}: no row for any of its {len(boards)} boards')
        else:
            out.extend(f'{family}/{b}: no row' for b in missing)
        for board, entry in rows.items():
            if board not in boards:
                out.append(f'{family}/{board}: row for a board dir that does not exist; remove the row')
                continue
            null = board in NULL_BOARDS.get(family, ())
            row = entry.get('cmake') if isinstance(entry, dict) else None
            if set(entry) != {'cmake'} if isinstance(entry, dict) else True:
                out.append(f'{family}/{board}: an entry is {{"cmake": ...}} and nothing else')
                continue
            if row is None:
                if not null:
                    out.append(f'{family}/{board}: cmake is null but the board is not Make-only')
                continue
            if null:
                out.append(f'{family}/{board}: is Make-only (NULL_BOARDS) but carries a cmake row')
                continue
            out.extend(f'{family}/{board}: {v}' for v in row_violations(row, mcus, root))
    for family in data:
        if family not in tree:
            out.append(f'{family}: rows for a family dir that does not exist; remove them')
    return out


def changed_cmake(root=ROOT, run=subprocess.run):
    """The hw/bsp cmake files the tree changes against HEAD, which is what a commit is about
    to carry; nothing when git cannot say (no git, no repository, no HEAD). --no-renames as
    ci_select does: rename detection reports only a rename's destination, so the board or
    family a file moved out of would keep its row from before the move."""
    try:
        r = run(['git', '-C', str(root), 'diff', '--no-renames', '--name-only', '-z', 'HEAD', '--', 'hw/bsp'],
                capture_output=True, text=True)
    except OSError:
        return []
    return [p for p in r.stdout.split('\0') if p] if r.returncode == 0 else []


def stale_rows(changed, tree):
    """(family, board) whose row was observed before one of the `changed` paths edited the
    cmake it was configured from: a file under the board's own dir is that board, one in
    the family dir is every board of the family. A family's cmake is its .cmake files and
    its CMakeLists.txt: hw/bsp/espressif/components/tinyusb_src/CMakeLists.txt is where
    every Espressif row's portable sources and defines come from. hw/bsp/family_support.cmake and
    src/common/tusb_mcu.h are left out on purpose: each feeds every row, and re-observing all
    of them is a sweep, not a hook; that sweep is `refresh`, run when a release is cut."""
    out = set()
    for p in changed:
        parts = p.split('/')
        if not (p.endswith('.cmake') or p.endswith('/CMakeLists.txt')) or len(parts) < 4 \
                or parts[:2] != ['hw', 'bsp'] or parts[2] not in tree:
            continue
        family = parts[2]
        if parts[3] == 'boards':
            if len(parts) > 4 and parts[4] in tree[family]:
                out.add((family, parts[4]))
        else:
            out.update((family, b) for b in tree[family])
    return out


OBSERVED = re.compile(r'^family\.json: (updated|unchanged) (\S+)')


def reobserve(boards, root=ROOT, run=subprocess.run):
    """Configure each (family, board) once with tools/build.py into a private dir of this
    process, removed after, so the row is observed fresh. Prints build.py's line per board;
    returns the boards it did not observe, with why: a nonzero exit, a "not updated"
    reason, or no line at all (an Espressif example that failed to configure)."""
    failed = []
    for family, board in boards:
        name = f'fj-{board}-{os.getpid()}'
        build_dir = root / 'cmake-build' / f'cmake-build-{name}'
        r = run([sys.executable, 'tools/build.py', '-b', board, '--configure-only', '--build-name', name],
                cwd=root, capture_output=True, text=True)
        shutil.rmtree(build_dir, ignore_errors=True)
        line = next((l for l in r.stderr.splitlines() if l.startswith('family.json: ')), None)
        print(line or f'family.json: {board} not updated: tools/build.py exit {r.returncode}')
        m = OBSERVED.match(line or '')
        if r.returncode != 0 or not m or m.group(2) != board:
            failed.append((family, board, line or f'tools/build.py exit {r.returncode}'))
    return failed


def prune(path, root):
    """Drop rows of boards or families that are gone, set NULL_BOARDS' rows to null,
    rewrite the file canonical (one locked edit); the inventory and the data after."""
    tree = inventory(root)

    def edit(data):
        for family in list(data):
            if family not in tree:
                del data[family]
                continue
            for board in list(data[family]):
                if board not in tree[family]:
                    del data[family][board]
        for family, boards in NULL_BOARDS.items():
            for board in boards:
                if board in tree.get(family, ()):
                    data.setdefault(family, {})[board] = {'cmake': None}
    rewrite(edit, path)
    return tree, load(path)


def fix(path=CATALOG, root=ROOT, run=subprocess.run, changed_paths=None):
    """Repair what observation can: prune, then re-observe each board without a row and
    each board whose cmake `changed_paths` names - the row it carries was observed from
    the configuration before that edit, defaulting to what git reports uncommitted.
    Returns (whether the file changed, what check() still reports). A board this machine
    cannot configure keeps the row it has, with tools/build.py's reason printed, and a
    board that has none stays reported."""
    before = path.read_text(encoding='utf-8') if path.is_file() else None
    try:
        tree, data = prune(path, root)
    except Failure as e:
        return False, [str(e)]
    stale = stale_rows(changed_cmake(root) if changed_paths is None else changed_paths, tree)
    todo = [(family, board) for family, boards in tree.items() for board in boards
            if board not in NULL_BOARDS.get(family, ())
            and (not isinstance(data.get(family, {}).get(board), dict)
                 or data[family][board].get('cmake') is None or (family, board) in stale)]
    reobserve(todo, root, run)
    changed = (path.read_text(encoding='utf-8') if path.is_file() else None) != before
    return changed, check(path, root)


def refresh(path=CATALOG, root=ROOT, run=subprocess.run):
    """The release sweep: prune, then re-observe every board that is not Make-only, rows
    or no rows. Returns (boards asked, boards not observed with why, what check() still
    reports); a board not observed keeps the row it had."""
    try:
        tree, _ = prune(path, root)
    except Failure as e:
        return [], [], [str(e)]
    todo = [(family, board) for family, boards in tree.items() for board in boards
            if board not in NULL_BOARDS.get(family, ())]
    return todo, reobserve(todo, root, run), check(path, root)


def row_violations(row, mcus, root):
    out = []
    if not isinstance(row, dict) or tuple(sorted(row)) != FIELDS:
        return [f'fields must be exactly {", ".join(FIELDS)}']
    if not isinstance(row['mcu'], str) or row['mcu'] not in mcus:
        out.append(f'mcu {row["mcu"]!r} is not an OPT_MCU_* defined in src/tusb_option.h')
    for name in ('family_mcus', 'portable'):
        v = row[name]
        if not isinstance(v, list) or not all(isinstance(x, str) for x in v) or v != sorted(set(v)):
            out.append(f'{name} must be a sorted list of unique strings')
    for name in ('defines', 'options'):
        v = row[name]
        if not isinstance(v, dict) or not all(isinstance(k, str) and isinstance(x, str) for k, x in v.items()):
            out.append(f'{name} must map names to string values')
    if isinstance(row['portable'], list):
        for p in row['portable']:
            # a plain relative path: joining an absolute one would discard the base
            if not isinstance(p, str) or p.startswith('/') or '..' in p.split('/') \
                    or not (root / 'src' / 'portable' / p).is_file():
                out.append(f'portable entry {p!r} is not a relative path to a file under src/portable/')
    if isinstance(row['family_mcus'], list) and isinstance(row['options'], dict):
        if ('MAX3421' in row['family_mcus']) != (row['options'].get('MAX3421_HOST') == '1'):
            out.append('MAX3421 belongs in family_mcus exactly when options.MAX3421_HOST is "1"')
    return out


def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = p.add_subparsers(dest='cmd', required=True)
    sub.add_parser('check', help='every violation, exit 1 if any; no toolchain needed')
    sub.add_parser('fix', help='repair the catalog in place (pre-commit); exit 1 when it changed or cannot')
    sub.add_parser('refresh', help='re-observe every cmake board (release); exit 1 when one could not be')
    sub.add_parser('format', help='rewrite the catalog in canonical form')
    o = sub.add_parser('observe', help="print a configured build dir's row without writing it")
    o.add_argument('build_dir', nargs='+')
    a = p.parse_args(argv)
    if a.cmd == 'check':
        problems = check()
        for line in problems:
            print(line)
        if problems:
            print(f'repair: {FIX}')
        return 1 if problems else 0
    if a.cmd == 'fix':
        changed, problems = fix(CATALOG, ROOT, subprocess.run)
        for line in problems:
            print(line)
        if changed:
            print(f'{CATALOG.relative_to(ROOT)} updated: stage it and commit again')
        return 1 if changed or problems else 0
    if a.cmd == 'refresh':
        asked, failed, problems = refresh(CATALOG, ROOT, subprocess.run)
        for line in problems:
            print(line)
        for family, board, why in failed:
            print(f'{family}/{board}: not observed: {why}')
        print(f'{len(asked) - len(failed)} of {len(asked)} boards re-observed')
        return 1 if failed or problems else 0
    if a.cmd == 'format':
        try:
            CATALOG.write_text(canonical_text(load()), encoding='utf-8')
        except Failure as e:
            print(f'family.json: not formatted: {e}', file=sys.stderr)
            return 1
        return 0
    try:
        family, board, row, entries = join(a.build_dir)
        note = validate(row, entries)
        print(json.dumps({family: {board: {'cmake': row}}}, sort_keys=True, indent=2))
        if note:
            print(note, file=sys.stderr)
    except Failure as e:
        print(f'family.json: not observed: {e}', file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
