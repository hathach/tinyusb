#!/usr/bin/env python3
"""Code size of TinyUSB examples.

`diff` builds the base branch (master) and the current tree in
cmake-code-size/<board>/{base,build}, pairs their elfs by (board, elf path) and reports
each pair's per-file flash/RAM deltas in cmake-code-size/<board>/diff[_<ex>].md; with
--combined, cmake-code-size/_combined/diff.md covers every board's pairs.

The sizes come from --engine; each ENGINES entry maps (elf, filters) to
{'files': {key: {'flash', 'ram'}}, 'all': {'flash', 'ram'}}, keyed by the
source path after the matching filter, the same for every engine.
- membrowse: `membrowse report` symbols. Membrowse 1.2.9 truncates `source_file`,
  so paths come from `object_file`.
- linkermap: input sections of the elf's GNU ld map, by object path.
- bloaty: `bloaty -d compileunits,sections` VM sizes, by DWARF compile unit.
Every engine takes flash/RAM from the elf's section and program headers
(section_buckets()).

Usage:
  python tools/code_size.py diff -b raspberry_pi_pico
  python tools/code_size.py diff -b raspberry_pi_pico -b raspberry_pi_pico2
  python tools/code_size.py diff -b raspberry_pi_pico -f portable/raspberrypi
  python tools/code_size.py diff -b raspberry_pi_pico -e device/cdc_msc
  python tools/code_size.py diff -b raspberry_pi_pico -e device/cdc_msc --bloaty
  python tools/code_size.py diff -b raspberry_pi_pico --engine linkermap --json
  python tools/code_size.py diff --ci                                  # CI-pinned boards, combined
  python tools/code_size.py diff -b raspberry_pi_pico -b raspberry_pi_pico2 --combined  # combine listed boards
"""
import argparse
import collections
import concurrent.futures
import csv
import functools
import glob
import importlib.util
import io
import json
import os
import re
import runpy
import shlex
import shutil
import struct
import subprocess
import sys

from membrowse_cli import extract_ld_scripts, extract_defsyms, link_command

TINYUSB_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CODE_SIZE_DIR = os.path.join(TINYUSB_ROOT, 'cmake-code-size')
CI_PINNED_BOARDS = os.path.join(TINYUSB_ROOT, '.github', 'ci-pinned-boards.json')
_RAM_REGION_HINTS = ('ram', 'tcm', 'ddr')
_FLASH_REGION_HINTS = ('flash', 'rom')


def _find_ninja_build_dir(elf_path):
    """Nearest ancestor of `elf_path` containing build.ninja, or None."""
    d = os.path.dirname(os.path.abspath(elf_path))
    while True:
        if os.path.isfile(os.path.join(d, 'build.ninja')):
            return d
        parent = os.path.dirname(d)
        if parent == d:
            return None
        d = parent


def _link_settings(elf_path):
    """Return the ELF's build dir, and the linker scripts and defsyms of its link."""
    build_dir = _find_ninja_build_dir(elf_path)
    if build_dir is None:
        raise RuntimeError(f'no build.ninja found above {elf_path} - cannot '
                            f'determine its linker scripts')
    commands = link_command('ninja', build_dir, elf_path)
    ld_scripts = extract_ld_scripts(commands)
    if not ld_scripts:
        raise RuntimeError(f'no linker script found in the ninja build graph '
                            f'for {elf_path}')
    return build_dir, ld_scripts, extract_defsyms(commands)


def report_for_elf(elf_path):
    """Run membrowse local report on one elf, return parsed JSON dict."""
    build_dir, ld_scripts, defsyms = _link_settings(elf_path)
    cmd = ['membrowse', 'report', elf_path, ' '.join(ld_scripts),
           '--json', '--all-symbols']
    for sym in defsyms:
        cmd += ['--def', sym]
    map_path = elf_path + '.map'
    if os.path.isfile(map_path):
        cmd += ['--map-file', map_path]
    # from the link's working dir, as membrowse_cli.report() does
    r = subprocess.run(cmd, capture_output=True, text=True, cwd=build_dir)
    if r.returncode != 0:
        raise RuntimeError(f'membrowse report failed for {elf_path}: {r.stderr}')
    try:
        return json.loads(r.stdout)
    except json.JSONDecodeError as e:
        raise RuntimeError(f'malformed membrowse report for {elf_path}: {e}') from e


def _classify_region(name):
    """Classify by region name; membrowse 1.2.9 reports parsed regions as UNKNOWN."""
    n = (name or '').lower()
    if any(h in n for h in _FLASH_REGION_HINTS):
        return 'flash'
    if any(h in n for h in _RAM_REGION_HINTS):
        return 'ram'
    return None


def _relative_key(src, filters):
    """Source path relative to the first matching filter, object suffix stripped,
    or None when no filter matches."""
    for f in filters:
        idx = src.find(f)
        if idx < 0:
            continue
        key = src[idx + len(f):]
        for suffix in ('.obj', '.o'):
            if key.endswith(suffix):
                return key[:-len(suffix)]
        return key
    return None


SHT_NOBITS = 8
SHT_ARRAYS = (14, 15, 16)  # INIT_ARRAY, FINI_ARRAY, PREINIT_ARRAY
SHF_WRITE, SHF_ALLOC = 0x1, 0x2
PT_LOAD = 1
_NOT_COUNTED = frozenset()


def elf_layout(path):
    """Section headers [(name, type, flags, addr, size)] and PT_LOAD segments
    [(vaddr, paddr, memsz)] of an ELF32/64 file of either byte order. Raises
    RuntimeError for an unreadable or malformed file."""
    try:
        with open(path, 'rb') as f:
            data = f.read()
        if data[:4] != b'\x7fELF':
            raise RuntimeError(f'{path} is not an ELF file')
        return _elf_layout(data)
    except (OSError, struct.error, IndexError, ValueError, UnicodeDecodeError) as e:
        raise RuntimeError(f'cannot read ELF headers of {path}: {e}') from e


def _elf_layout(data):
    e = '<' if data[5] == 1 else '>'
    if data[4] == 2:
        phoff, shoff = struct.unpack_from(e + 'QQ', data, 0x20)
        phentsize, phnum, shentsize, shnum, shstrndx = struct.unpack_from(e + '5H', data, 0x36)
        phdr = lambda o: struct.unpack_from(e + 'II6Q', data, o)  # noqa: E731
        load = lambda p: (p[3], p[4], p[6])  # noqa: E731
        shdr = lambda o: struct.unpack_from(e + 'IIQQQQIIQQ', data, o)  # noqa: E731
    else:
        phoff, shoff = struct.unpack_from(e + 'II', data, 0x1c)
        phentsize, phnum, shentsize, shnum, shstrndx = struct.unpack_from(e + '5H', data, 0x2a)
        phdr = lambda o: struct.unpack_from(e + '8I', data, o)  # noqa: E731
        load = lambda p: (p[2], p[3], p[5])  # noqa: E731
        shdr = lambda o: struct.unpack_from(e + '10I', data, o)  # noqa: E731
    loads = [load(p) for p in (phdr(phoff + i * phentsize) for i in range(phnum)) if p[0] == PT_LOAD]
    headers = [shdr(shoff + i * shentsize) for i in range(shnum)]
    strtab = headers[shstrndx][4]

    def name(n):
        return data[strtab + n:data.index(b'\0', strtab + n)].decode()
    return [(name(h[0]), h[1], h[2], h[3], h[5]) for h in headers[1:]], loads


def map_regions(map_path):
    """[(name, origin, length)] of a GNU ld map's Memory Configuration."""
    if not os.path.isfile(map_path):
        raise RuntimeError(f'no linker map {map_path}')
    regions, inside = [], False
    with open(map_path, encoding='utf-8', errors='replace') as f:
        for line in f:
            if line.startswith('Memory Configuration'):
                inside = True
            elif line.startswith('Linker script and memory map'):
                break
            elif inside:
                m = re.match(r'(\S+)\s+0x([0-9a-fA-F]+)\s+0x([0-9a-fA-F]+)', line)
                if m and m.group(1) != '*default*':
                    regions.append((m.group(1), int(m.group(2), 16), int(m.group(3), 16)))
    return regions


def section_buckets(elf, map_path):
    """{section: flash/RAM bucket set} for every section of `elf`, empty when not
    allocated. A file-backed section loaded from elsewhere is a flash copy plus its
    run location; one running where it loads is flash unless writable, when the
    map region holding it decides. Raises RuntimeError rather than guess."""
    sections, loads = elf_layout(elf)
    regions = map_regions(map_path)
    buckets = {}
    for name, sh_type, flags, addr, size in sections:
        if not flags & SHF_ALLOC:
            buckets[name] = _NOT_COUNTED
            continue
        seg = next((s for s in loads if s[0] <= addr < s[0] + s[2]), None)
        if seg is None:
            if size:
                raise RuntimeError(f'{elf}: allocated section {name} lies in no PT_LOAD segment')
            buckets[name] = _NOT_COUNTED
            continue
        if sh_type == SHT_NOBITS:
            buckets[name] = frozenset({'ram'})
            continue
        kinds = {_classify_region(r) for r, origin, length in regions if origin <= addr < origin + length}
        if seg[1] + addr - seg[0] != addr:
            # a run address in flash is an alias (xmc4500 cached/uncached), not a RAM copy
            buckets[name] = frozenset({'flash'}) if kinds == {'flash'} else frozenset({'flash', 'ram'})
        elif not flags & SHF_WRITE:
            buckets[name] = frozenset({'flash'})
        elif len(kinds) == 1 and None not in kinds:
            buckets[name] = frozenset(kinds)  # RAM-only images (raspberrypi_zero .data)
        elif sh_type in SHT_ARRAYS:
            buckets[name] = frozenset({'flash'})  # NXP .init_array in m_text
        else:
            raise RuntimeError(f'{elf}: writable section {name} at {addr:#x} runs where it '
                               f'loads, in no map region recognized as flash or RAM')
    return buckets


class _Sizes:
    """Accumulates one elf's filtered per-file and total sizes."""
    def __init__(self, filters):
        self.filters, self.files, self.all = filters, {}, {'flash': 0, 'ram': 0}

    def add(self, path, buckets, size):
        key = _relative_key(path, self.filters) if path else None
        for b in buckets:
            self.all[b] += size
            if key is not None:
                self.files.setdefault(key, {'flash': 0, 'ram': 0})[b] += size

    def result(self):
        return {'files': self.files, 'all': self.all}


def membrowse_sizes(elf, filters):
    """Symbols of `membrowse report`; linker-defined symbols without a section
    (`__StackLimit`) are not counted."""
    buckets = section_buckets(elf, elf + '.map')
    sizes = _Sizes(filters)
    for sym in report_for_elf(elf).get('symbols', []):
        section = sym.get('section')
        if not sym.get('size') or not section:
            continue
        if section not in buckets:
            raise RuntimeError(f'membrowse symbol {sym.get("name")} is in section {section}, not in {elf}')
        sizes.add(sym.get('object_file') or sym.get('source_file'), buckets[section], sym['size'])
    return sizes.result()


@functools.cache
def _linkermap():
    path = os.path.join(TINYUSB_ROOT, 'tools', 'linkermap', 'linkermap.py')
    if not os.path.isfile(path):
        raise FileNotFoundError(f'{path} not found - run `python3 tools/get_deps.py tools/linkermap`')
    spec = importlib.util.spec_from_file_location('linkermap', path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def linkermap_sizes(elf, filters):
    """Input sections of `<elf>.map` by full object path; analyze_map() is not used,
    it keeps only four sections. An archive member
    (`lib.a(x.o)`) has no source dir, so it counts in 'all' only."""
    map_path = elf + '.map'
    buckets = section_buckets(elf, map_path)
    with open(map_path, encoding='utf-8', errors='replace') as f:
        sections = _linkermap().parseSections(f)
    if sections is None:
        raise RuntimeError(f'{map_path}: no Memory Configuration, not a GNU ld map')
    sizes = _Sizes(filters)
    for out in sections:
        if not out.children:  # memory regions
            continue
        if out.section not in buckets:
            raise RuntimeError(f'{map_path}: output section {out.section} is not in {elf}')
        for obj in out.children:
            sizes.add(obj.path[0], buckets[out.section], obj.size)
    return sizes.result()


def bloaty_sizes(elf, filters):
    """VM size per DWARF compile unit and section; bytes outside any section
    (`[LOAD #0 [RX]]` padding, loaded ELF headers) are not counted."""
    r = subprocess.run(['bloaty', '--csv', '-n', '0', '--domain=vm',
                        '-d', 'compileunits,sections', elf], capture_output=True, text=True)
    if r.returncode != 0:
        raise RuntimeError(f'bloaty failed for {elf}: {r.stderr.strip()}')
    rows = csv.DictReader(io.StringIO(r.stdout))
    if not {'compileunits', 'sections', 'vmsize'} <= set(rows.fieldnames or ()):
        raise RuntimeError(f'unexpected bloaty csv columns for {elf}: {rows.fieldnames}')
    buckets = section_buckets(elf, elf + '.map')
    sizes = _Sizes(filters)
    for row in rows:
        section = row['sections']
        try:
            size = int(row['vmsize'])
        except (TypeError, ValueError) as e:
            raise RuntimeError(f'malformed bloaty csv row for {elf}: {row}') from e
        if not size or not section or section.startswith('['):
            continue
        if section not in buckets:
            raise RuntimeError(f'bloaty section {section} is not in {elf}')
        unit = row['compileunits']
        sizes.add(None if unit.startswith('[') else unit, buckets[section], size)
    return sizes.result()


# all_label: what the engine's 'all' total sums (totals of different engines are
# not comparable); install: how to get its tool when `sizes` raises FileNotFoundError
Engine = collections.namedtuple('Engine', 'sizes all_label install')
ENGINES = {
    'membrowse': Engine(membrowse_sizes, 'all symbols', '`pip install membrowse`'),
    'linkermap': Engine(linkermap_sizes, 'all input sections',
                        '`python3 tools/get_deps.py tools/linkermap`'),
    'bloaty': Engine(bloaty_sizes, 'all accounted sections',
                     'bloaty on PATH (https://github.com/google/bloaty)'),
}


def _fmt(delta):
    return f'+{delta}' if delta > 0 else str(delta)


def compare_reports(base_by_file, cur_by_file):
    """Markdown per-file delta table; files sorted by |flash delta| desc."""
    rows = []
    for path in sorted(set(base_by_file) | set(cur_by_file)):
        b = base_by_file.get(path, {'flash': 0, 'ram': 0})
        c = cur_by_file.get(path, {'flash': 0, 'ram': 0})
        df, dr = c['flash'] - b['flash'], c['ram'] - b['ram']
        rows.append((path, b, c, df, dr))
    rows.sort(key=lambda r: abs(r[3]), reverse=True)

    lines = ['| File | Flash base | Flash new | Flash Δ | RAM base | RAM new | RAM Δ |',
             '|------|-----------:|----------:|--------:|---------:|--------:|------:|']
    tb = {'flash': 0, 'ram': 0}
    tc = {'flash': 0, 'ram': 0}
    for path, b, c, df, dr in rows:
        # totals run over ALL rows; the table prints only changed ones
        tb['flash'] += b['flash']; tb['ram'] += b['ram']
        tc['flash'] += c['flash']; tc['ram'] += c['ram']
        if df == 0 and dr == 0:
            continue
        lines.append(f'| {path} | {b["flash"]} | {c["flash"]} | {_fmt(df)} '
                     f'| {b["ram"]} | {c["ram"]} | {_fmt(dr)} |')
    lines.append(f'| **TOTAL** | {tb["flash"]} | {tc["flash"]} | '
                 f'{_fmt(tc["flash"] - tb["flash"])} | {tb["ram"]} | {tc["ram"]} | '
                 f'{_fmt(tc["ram"] - tb["ram"])} |')
    if len(lines) == 3 and rows:
        lines.insert(2, '| _no per-file changes_ | | | | | | |')
    return '\n'.join(lines) + '\n'


def pair_elfs(base, cur):
    """Pair two {elf_id: elf_sizes, or None for a failed report} maps.

    Returns (pairs, base_only, cur_only): `pairs` maps each id with a report on
    both sides to (base, cur); ids on one side only are listed, never paired.
    """
    pairs = {i: (base[i], cur[i]) for i in sorted(base.keys() & cur.keys())
             if base[i] is not None and cur[i] is not None}
    return pairs, sorted(base.keys() - cur.keys()), sorted(cur.keys() - base.keys())


def _delta(b, c):
    return c['flash'] - b['flash'], c['ram'] - b['ram']


def _file_deltas(b, c):
    """{path: (flash Δ, RAM Δ)} over the union of both sides' files."""
    zero = {'flash': 0, 'ram': 0}
    return {path: _delta(b['files'].get(path, zero), c['files'].get(path, zero))
            for path in sorted(set(b['files']) | set(c['files']))}


def _src_total(sizes):
    return {k: sum(f[k] for f in sizes['files'].values()) for k in ('flash', 'ram')}


def _label(elf_id):
    board, elf = elf_id
    return f'{board}: {elf}' if elf else board


def _extreme(value_id):
    delta, elf_id = value_id
    return f'{_fmt(delta)} ({_label(elf_id)})' if delta else '0'


def _file_stats(file_deltas):
    """Per file: pairs present/changed and the (Δ, elf id) extremes of each
    metric, the first id winning ties. `file_deltas` maps sorted elf ids to
    _file_deltas()."""
    entries = {}
    for elf_id, deltas in file_deltas.items():
        for path, d in deltas.items():
            entries.setdefault(path, []).append((elf_id, d))
    stats = {}
    for path, es in entries.items():
        stats[path] = {'present': len(es), 'changed': sum(any(d) for _, d in es)}
        for i, k in enumerate(('flash', 'ram')):
            lo = min(es, key=lambda e: e[1][i])
            hi = max(es, key=lambda e: e[1][i])
            stats[path][k] = [(lo[1][i], lo[0]), (hi[1][i], hi[0])]
    return stats


def render_pairs(pairs, matched, engine, base_only=(), cur_only=(), failures=(), boards=()):
    """Markdown report over paired elfs keyed by (board, elf path).

    Every statistic is over per-pair deltas, sized by `engine`. `matched` counts
    ids built on both sides, compared or not;
    `failures` are (elf id, side, stage, message), the elf path None for a
    board-level failure. Failures or unmatched elfs mark the report INCOMPLETE.
    `boards` lists the requested boards, so an unchanged or failed one is named.
    """
    file_deltas = {i: _file_deltas(b, c) for i, (b, c) in pairs.items()}
    changed = [i for i, (b, c) in pairs.items()
               if any(_delta(b['all'], c['all'])) or any(any(d) for d in file_deltas[i].values())]
    status = _status(failures, base_only, cur_only)
    all_label = ENGINES[engine].all_label
    lines = [f'**Coverage ({status}, {engine}):** {len(pairs)} of {matched} matched elf pairs '
             f'compared, {len(changed)} changed']
    if boards:
        lines.append('- boards: ' + ', '.join(f'`{b}`' for b in boards))
    for label, ids in (('base-only', base_only), ('current-only', cur_only)):
        if ids:
            lines.append(f'- {label}: ' + ', '.join(f'`{_label(i)}`' for i in ids))
    for elf_id, side, stage, message in failures:
        lines.append(f'- FAILED `{_label(elf_id)}` {side} {stage}: {message}')
    lines.append('')

    if len(pairs) == 1:
        (elf_id, (b, c)), = pairs.items()
        df, dr = _delta(b['all'], c['all'])
        lines += [f'`{_label(elf_id)}` {all_label}: Flash Δ {_fmt(df)}, RAM Δ {_fmt(dr)}', '',
                  compare_reports(b['files'], c['files'])]
        return '\n'.join(lines)
    if not pairs:
        return '\n'.join(lines + ['_no comparable pairs_', ''])
    if not changed:
        return '\n'.join(lines + ['_no changes_', ''])

    lines += [f'| Pair | filtered Flash Δ | filtered RAM Δ | {all_label} Flash Δ | {all_label} RAM Δ |',
              '|------|-----------------:|---------------:|------------------:|----------------:|']
    for elf_id in changed:
        b, c = pairs[elf_id]
        src = _delta(_src_total(b), _src_total(c))
        syms = _delta(b['all'], c['all'])
        lines.append(f'| {_label(elf_id)} | ' + ' | '.join(_fmt(d) for d in src + syms) + ' |')

    stats = _file_stats(file_deltas)
    rows = sorted((p for p, s in stats.items() if s['changed']),
                  key=lambda p: (-max(abs(v[0]) for k in ('flash', 'ram') for v in stats[p][k]), p))
    lines += ['', '_Changed / present: pairs where the file changed / compared pairs that contain it._',
              '', '| File | Changed / present | Flash Δ min | Flash Δ max | RAM Δ min | RAM Δ max |',
              '|------|------------------:|------------:|------------:|----------:|----------:|']
    for path in rows:
        s = stats[path]
        lines.append(f'| {path} | {s["changed"]}/{s["present"]} | '
                     + ' | '.join(_extreme(v) for k in ('flash', 'ram') for v in s[k]) + ' |')

    for elf_id in changed:
        b, c = pairs[elf_id]
        lines += ['', f'<details><summary>{_label(elf_id)}</summary>', '',
                  compare_reports(b['files'], c['files']), '</details>']
    return '\n'.join(lines) + '\n'


def _status(failures, base_only, cur_only):
    return 'INCOMPLETE' if failures or base_only or cur_only else 'complete'


def _elf_record(elf_id):
    board, elf = elf_id
    return {'board': board, 'elf': elf}


def compare_sides(base, cur, engine, failures=(), boards=(), scope=None):
    """Pair two sides and render them. Returns (md, failures, ok, data).

    `failures` comes back with a filter failure for `scope` added when no
    compared pair matched a file (wrong filters, or an engine output change
    broke its parsing); pass `scope=None` when each scope was already checked.
    `ok` is False on any failure or when nothing was compared. `data` is the
    report's raw paired sizes, for JSON.
    """
    pairs, base_only, cur_only = pair_elfs(base, cur)
    failures = list(failures)
    if scope and pairs and not any(b['files'] or c['files'] for b, c in pairs.values()):
        failures.append((scope, 'both', 'filter',
                         f'no {engine} sizes matched filters - check them, or a change in '
                         f'{engine} output broke its parsing (try another --engine to isolate)'))
    md = render_pairs(pairs, len(base.keys() & cur.keys()), engine, base_only, cur_only, failures, boards)
    data = {
        'engine': engine,
        'boards': list(boards),
        'status': _status(failures, base_only, cur_only),
        'pairs': [{**_elf_record(i), 'base': b, 'current': c} for i, (b, c) in pairs.items()],
        'base_only': [_elf_record(i) for i in base_only],
        'current_only': [_elf_record(i) for i in cur_only],
        'failures': [{**_elf_record(i), 'side': side, 'stage': stage, 'message': message}
                     for i, side, stage, message in failures],
    }
    return md, failures, bool(pairs) and not failures, data


def tinyusb_src_filter(checkout_dir):
    """Return a path-substring filter that uniquely matches TinyUSB stack source files
    in `checkout_dir`. The substring is the absolute path to the checkout's `src/`
    dir — collision-free with vendored deps (pico-sdk, lwip, FreeRTOS, etc.) which
    live at unrelated paths."""
    return os.path.realpath(os.path.join(checkout_dir, 'src')) + os.sep


verbose = False


def run(cmd, **kwargs):
    """Run a command. cmd must be a list (no shell=True). On `timeout=`-induced
    TimeoutExpired, return a CompletedProcess with rc=124 instead of letting the
    exception propagate, so the caller can fall through to error reporting and
    worktree cleanup rather than crashing with a traceback."""
    if not isinstance(cmd, list):
        raise TypeError('run() requires a list, got str — fix the caller')
    if verbose:
        print(f'  $ {" ".join(shlex.quote(str(c)) for c in cmd)}')
    try:
        return subprocess.run(cmd, capture_output=True, text=True, **kwargs)
    except subprocess.TimeoutExpired as e:
        msg = f'Command timed out after {e.timeout}s: {" ".join(shlex.quote(str(c)) for c in cmd)}'
        stderr = (e.stderr or '') + ('\n' if e.stderr else '') + msg
        return subprocess.CompletedProcess(cmd, 124, stdout=(e.stdout or ''), stderr=stderr)


def symlink_deps(main_root, worktree_dir):
    """Symlink each dependency the worktree's own tools/get_deps.py lists, when fetched in
    the main checkout: the worktree lacks these untracked dirs, and a base revision can
    name paths the current manifest has renamed or dropped."""
    manifest = runpy.run_path(os.path.join(worktree_dir, 'tools', 'get_deps.py'))
    for rel in manifest['deps_all']:
        src = os.path.join(main_root, rel)
        dst = os.path.join(worktree_dir, rel)
        if os.path.isdir(src) and not os.path.lexists(dst):
            os.makedirs(os.path.dirname(dst), exist_ok=True)
            os.symlink(src, dst)


def ci_pinned_boards():
    """Boards of .github/ci-pinned-boards.json: CI's membrowse set, which covers every
    dcd/hcd driver not waived in its `uncovered` list (drivers-coverage hook)."""
    with open(CI_PINNED_BOARDS) as f:
        return [entry['board'] for entry in json.load(f)['boards']]


def build_board(src_dir, build_dir, board, example=None):
    """Configure and build examples for a board. Returns True on success.

    When `example` is given, only that target is built (`cmake --build --target NAME`),
    keeping single-example workflows fast.
    """
    os.makedirs(build_dir, exist_ok=True)
    ret = run(['cmake', '-B', build_dir, '-G', 'Ninja',
               f'-DBOARD={board}', '-DCMAKE_BUILD_TYPE=MinSizeRel',
               os.path.join(src_dir, 'examples')])
    if ret.returncode != 0:
        print(f'  Error configuring {board}: {ret.stderr}')
        return False
    cmd = ['cmake', '--build', build_dir]
    if example:
        cmd += ['--target', os.path.basename(example)]
    ret = run(cmd, timeout=600)
    if ret.returncode != 0:
        print(f'  Error building {board}: {ret.stderr}')
        return False
    return True


def report_path(board, example):
    """A scope's report path without extension: cmake-code-size/<board>/diff[_<ex>]."""
    suffix = f'_{example.replace("/", "_")}' if example else ''
    return os.path.join(CODE_SIZE_DIR, board, f'diff{suffix}')


def generate_sizes(build_dir, filters, example=None, engine='membrowse'):
    """Return (sizes, errors) for the scope's elfs.

    `sizes` maps each elf path relative to `build_dir` to its
    ENGINES[engine] sizes, or to None when that failed; `errors`
    lists (relative elf path, message), the path None when no elf can be
    sized at all.
    """
    # escape the dir, not the wildcards: a checkout path is a path, not a pattern
    root = glob.escape(build_dir)
    pattern = f'{root}/{example}/*.elf' if example \
        else f'{root}/**/*.elf'
    elfs = sorted(glob.glob(pattern, recursive=True))
    if not elfs:
        print(f'  Error: no .elf files in {build_dir}')
        return {}, [(None, f'no .elf files in {build_dir}')]

    sizer = ENGINES[engine].sizes

    def report(elf):
        try:
            return sizer(elf, filters), None
        except RuntimeError as e:
            return None, str(e)

    # report errors are caught here, not as tracebacks after both builds already ran
    try:
        with concurrent.futures.ThreadPoolExecutor() as pool:
            results = list(pool.map(report, elfs))
    except FileNotFoundError as e:
        print(f'  Error: {engine} not found ({e}) - install {ENGINES[engine].install}, '
              f'or pick another --engine')
        return {}, [(None, f'{engine} not found')]
    sizes, errors = {}, []
    for elf, (elf_sizes, error) in zip(elfs, results):
        rel = os.path.relpath(elf, build_dir)
        sizes[rel] = elf_sizes
        if error:
            print(f'  Error: {error}')
            errors.append((rel, error))
    return sizes, errors


def write_report(path, md, data=None):
    """Write a report to `path`.md, and `data` to `path`.json when given; stdout
    gets the Markdown without the per-pair details."""
    with open(f'{path}.md', 'w') as f:
        f.write(md)
    print(md.split('\n<details>')[0].rstrip())
    print(f'  report: {path}.md')
    if data is not None:
        with open(f'{path}.json', 'w') as f:
            json.dump(data, f, indent=1, sort_keys=True)
            f.write('\n')
        print(f'  json: {path}.json')


def main():
    global verbose

    top = argparse.ArgumentParser(description='Code size of TinyUSB examples')
    sub = top.add_subparsers(dest='command', required=True)
    parser = sub.add_parser('diff', help='diff code size against a base ref')
    parser.add_argument('-b', '--board', action='append', default=[],
                        help='Board name (repeatable). Required unless --ci is given.')
    parser.add_argument('-f', '--filter', action='append', default=None,
                        help='Path-substring filter (repeatable). When given, '
                             'overrides the default and is applied to BOTH base and '
                             'current builds. Default: each side\'s own absolute '
                             '<checkout>/src/ path, which uniquely matches TinyUSB '
                             'stack code without colliding with vendored deps.')
    parser.add_argument('--base-branch', default='master',
                        help='Base branch to compare against (default: master)')
    parser.add_argument('-e', '--example', action='append', default=None,
                        help='Compare specific example (repeatable, e.g. -e device/cdc_msc -e host/cdc_msc_hid)')
    parser.add_argument('--bloaty', action='store_true',
                        help='Also print bloaty\'s section and symbol diff of each -e example '
                             '(console only, whatever --engine)')
    parser.add_argument('--engine', choices=sorted(ENGINES),
                        default='membrowse',
                        help='Where per-file sizes come from: membrowse (default, symbols with '
                             'ELF-header flash/RAM), linkermap (the GNU ld map\'s input '
                             'sections) or bloaty (DWARF compile units)')
    parser.add_argument('--json', action='store_true',
                        help='Also write each report\'s paired sizes as diff*.json '
                             'next to its .md')
    parser.add_argument('--ci', action='store_true',
                        help='Add the CI-pinned boards (.github/ci-pinned-boards.json, covering '
                             'every dcd/hcd driver not waived there). Implies --combined.')
    parser.add_argument('--combined', action='store_true',
                        help='Also write one comparison over every board '
                             '(cmake-code-size/_combined/diff.md), in addition to per-board.')
    parser.add_argument('-v', '--verbose', action='store_true',
                        help='Print build commands')
    args = top.parse_args()
    verbose = args.verbose

    if args.bloaty and not args.example:
        parser.error('--bloaty requires -e/--example')

    if args.ci:
        args.combined = True
        ci_boards = ci_pinned_boards()
        # Append, dedup, preserve order
        seen = set(args.board)
        for b in ci_boards:
            if b not in seen:
                args.board.append(b)
                seen.add(b)

    if not args.board:
        parser.error('at least one -b BOARD is required (or pass --ci)')

    worktree_dir = os.path.join(CODE_SIZE_DIR, '_worktree')

    # Per-side filters: when no override is given, each build uses its own
    # absolute <checkout>/src/ path so we only match TinyUSB stack code from that
    # checkout (and never vendored-dep `src/` like pico-sdk/src/...).
    if args.filter:
        base_filters = cur_filters = list(args.filter)
    else:
        base_filters = [tinyusb_src_filter(worktree_dir)]
        cur_filters = [tinyusb_src_filter(TINYUSB_ROOT)]

    # Drop every report this run will write before anything can fail - a run that
    # stops early (worktree setup, a build) must not leave a previous run's report
    # for a reader to take for this one's: cmake-code-size/ is gitignored and persists.
    # .json too: a run without --json must not leave an older one beside a newer .md.
    examples = args.example or [None]
    combined_dir = os.path.join(CODE_SIZE_DIR, '_combined')
    if args.combined:
        shutil.rmtree(combined_dir, ignore_errors=True)
    for board in args.board:
        for example in examples:
            for ext in ('md', 'json'):
                stale_path = f'{report_path(board, example)}.{ext}'
                if os.path.isfile(stale_path):
                    os.remove(stale_path)

    # Step 1: Create worktree for base branch
    print(f'[1/5] Setting up {args.base_branch} worktree...')
    if os.path.isdir(worktree_dir):
        run(['git', '-C', TINYUSB_ROOT, 'worktree', 'remove', '--force', worktree_dir])
    # --detach: check out the ref at a detached HEAD instead of trying to claim the
    # branch. Lets us add a worktree of `master` even if master is already checked
    # out elsewhere (main repo, another worktree).
    ret = run(['git', '-C', TINYUSB_ROOT, 'worktree', 'add', '--detach',
               worktree_dir, args.base_branch])
    if ret.returncode != 0:
        print(f'Error creating worktree: {ret.stderr}')
        sys.exit(1)

    symlink_deps(TINYUSB_ROOT, worktree_dir)

    # the commit actually built, which the ref may no longer name later
    base_sha = run(['git', '-C', worktree_dir, 'rev-parse', 'HEAD']).stdout.strip()

    def report_data(data):
        """The JSON report for --json, None without it."""
        if not args.json:
            return None
        return {**data, 'base_ref': args.base_branch, 'base_sha': base_sha,
                'filters': {'base': base_filters, 'current': cur_filters}}

    failed = False
    try:
        # --combined: every board's elf sizes and failures, paired at the end
        combined_sides = {'base': {}, 'current': {}}
        combined_failures = []

        for board in args.board:
            print(f'\n=== {board} ===')
            board_dir = os.path.join(CODE_SIZE_DIR, board)
            base_build = os.path.join(board_dir, 'base')
            cur_build = os.path.join(board_dir, 'build')
            shutil.rmtree(base_build, ignore_errors=True)
            shutil.rmtree(cur_build, ignore_errors=True)

            # Build only the requested examples (or all if -e not given). Single-example
            # mode used to build everything and filter at metric time — that was wasted work.
            board_failed = None  # the side whose build failed
            for example in examples:
                build_label = f' --target {os.path.basename(example)}' if example else ''
                print(f'[2/5] Building {args.base_branch} for {board}{build_label}...')
                if not build_board(worktree_dir, base_build, board, example):
                    board_failed = 'base'
                    break
                print(f'[3/5] Building current for {board}{build_label}...')
                if not build_board(TINYUSB_ROOT, cur_build, board, example):
                    board_failed = 'current'
                    break
            if board_failed:
                failed = True
                # still write each scope's report, with the build failure in it
                build_failure = ((board, None), board_failed, 'build', f'build failed{build_label}, see log')
                combined_failures.append(build_failure)

            for example in examples:
                label = f' ({example})' if example else ''

                # Step 4/5: Generate sizes and compare
                sides = {'base': {}, 'current': {}}
                failures = [build_failure] if board_failed else []
                if not board_failed:
                    print(f'[4/5] Sizing {board}{label} with {args.engine}...')
                    for side, build, filters in (('base', base_build, base_filters),
                                                 ('current', cur_build, cur_filters)):
                        sizes, errors = generate_sizes(build, filters, example, args.engine)
                        sides[side] = {(board, rel): v for rel, v in sizes.items()}
                        failures += [((board, rel), side, 'report', msg) for rel, msg in errors]

                print(f'[5/5] Comparing {board}{label}...')
                md, failures, ok, data = compare_sides(
                    sides['base'], sides['current'], args.engine, failures, [board], scope=(board, None))
                failed |= not ok
                if not board_failed:  # a build failure is recorded once, above
                    for side, sizes in sides.items():
                        combined_sides[side].update(sizes)
                    combined_failures += failures
                write_report(report_path(board, example), md, report_data(data))

                # Optional: bloaty diff
                if args.bloaty and example and not board_failed:
                    elf_name = os.path.basename(example)
                    base_elf = os.path.join(base_build, example, f'{elf_name}.elf')
                    cur_elf = os.path.join(cur_build, example, f'{elf_name}.elf')
                    if os.path.exists(base_elf) and os.path.exists(cur_elf):
                        # Bloaty expects one regex; OR-join all filters (current side
                        # for the new ELF, base side for the base ELF).
                        bloaty_regex = '(' + '|'.join(
                            re.escape(f) for f in (cur_filters + base_filters)
                        ) + ')'
                        bloaty_common = ['bloaty', '--domain=vm', f'--source-filter={bloaty_regex}']
                        print(f'--- bloaty sections ---')
                        ret = run(bloaty_common + ['-d', 'compileunits,sections', cur_elf, '--', base_elf])
                        print(ret.stdout)
                        print(f'--- bloaty symbols ---')
                        ret = run(bloaty_common + ['-d', 'compileunits,symbols', '-s', 'vm',
                                                    cur_elf, '--', base_elf])
                        print(ret.stdout)
                    else:
                        print(f'  bloaty: ELF not found')

        # Optional combined comparison across all boards.
        if args.combined:
            os.makedirs(combined_dir, exist_ok=True)
            print(f'\n=== combined ({len(args.board)} boards) ===')
            # every scope was filter-checked above, and its failures carried over
            md, _failures, ok, data = compare_sides(
                combined_sides['base'], combined_sides['current'], args.engine,
                combined_failures, args.board)
            failed |= not ok
            write_report(os.path.join(combined_dir, 'diff'), md, report_data(data))
    finally:
        print(f'\nCleaning up worktree...')
        run(['git', '-C', TINYUSB_ROOT, 'worktree', 'remove', '--force', worktree_dir])
    return 1 if failed else 0


if __name__ == '__main__':
    sys.exit(main())
