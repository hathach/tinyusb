#!/usr/bin/env python3
"""Code size of TinyUSB examples.

`report` builds the working tree in cmake-code-size/<board>/build and tabulates each elf's
per-file section sizes in cmake-code-size/<board>/report[_<ex>].md.

`diff` builds the base branch (master) and the current tree in
cmake-code-size/<board>/{base,build}, pairs their elfs by (board, elf path) and reports
each pair's per-file flash/RAM deltas in cmake-code-size/<board>/diff[_<ex>].md; with
--combined, cmake-code-size/_combined/diff.md covers every board's pairs.

The sizes come from --engine; each ENGINES entry maps (elf, filters) to
{'files': {key: {'flash', 'ram'}}, 'all': {'flash', 'ram'}, 'sections': {key: {section:
size}}, 'symbols': {key: {section: {name: size}}}}, keyed by the source path after the
matching filter, the same for every engine.
- membrowse: `membrowse report` symbols. Membrowse 1.2.9 truncates `source_file`,
  so paths come from `object_file`.
- linkermap: input sections of the elf's GNU ld map, by object path; its symbols are
  those input sections.
- bloaty: `bloaty -d compileunits,sections,symbols` VM sizes, by DWARF compile unit.
Every engine takes flash/RAM from the elf's section and program headers
(section_buckets()).

Usage:
  python tools/code_size.py report -b raspberry_pi_pico -e device/cdc_msc --symbols
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
import time

import build_utils
from membrowse_cli import extract_ld_scripts, extract_defsyms, link_command

TINYUSB_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CODE_SIZE_DIR = os.path.join(TINYUSB_ROOT, 'cmake-code-size')
CI_PINNED_BOARDS = os.path.join(TINYUSB_ROOT, '.github', 'ci-pinned-boards.json')
# a diff's side names when git cannot give their commit hashes
SIDE_LABELS = ('base', 'new')
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
    """Accumulates one elf's filtered per-file flash/RAM, section and symbol sizes,
    and its total flash/RAM."""
    def __init__(self, filters):
        self.filters, self.files, self.all = filters, {}, {'flash': 0, 'ram': 0}
        self.sections, self.symbols = {}, {}

    def add(self, path, section, buckets, size, name):
        key = _relative_key(path, self.filters) if path else None
        for b in buckets:
            self.all[b] += size
            if key is not None:
                self.files.setdefault(key, {'flash': 0, 'ram': 0})[b] += size
        if key is not None and buckets:
            per_file = self.sections.setdefault(key, {})
            per_file[section] = per_file.get(section, 0) + size
            per_section = self.symbols.setdefault(key, {}).setdefault(section, {})
            per_section[name] = per_section.get(name, 0) + size

    def result(self):
        return {'files': self.files, 'all': self.all, 'sections': self.sections,
                'symbols': self.symbols}


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
        sizes.add(sym.get('object_file') or sym.get('source_file'), section, buckets[section], sym['size'],
                  sym.get('name'))
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
    (`lib.a(x.o)`) has no source dir, so it counts in 'all' only. The symbols are the
    input sections (`.text.foo`), not the labels inside one."""
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
            sizes.add(obj.path[0], out.section, buckets[out.section], obj.size, obj.section)
    return sizes.result()


def bloaty_sizes(elf, filters):
    """VM size per DWARF compile unit, section and symbol; bytes outside any section
    (`[LOAD #0 [RX]]` padding, loaded ELF headers) are not counted."""
    r = subprocess.run(['bloaty', '--csv', '-n', '0', '--domain=vm',
                        '-d', 'compileunits,sections,symbols', elf], capture_output=True, text=True)
    if r.returncode != 0:
        raise RuntimeError(f'bloaty failed for {elf}: {r.stderr.strip()}')
    rows = csv.DictReader(io.StringIO(r.stdout))
    if not {'compileunits', 'sections', 'symbols', 'vmsize'} <= set(rows.fieldnames or ()):
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
        sizes.add(None if unit.startswith('[') else unit, section, buckets[section], size, row['symbols'])
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


def _md_escape(text):
    """`text` safe in Markdown prose: gcc quotes as `name', which would open a code span."""
    return text.replace('`', '\\`')


def _fmt(delta):
    return f'+{delta}' if delta > 0 else str(delta)


def md_table(header, rows, total=None):
    """Markdown table padded so its columns also line up as plain text: the first
    column left-aligned, the others right-aligned. Cells are strings; a `total` row
    follows the rows under a plain rule."""
    body = rows + [total] if total else rows
    widths = [max(len(r[i]) for r in [header] + body) for i in range(len(header))]

    def line(cells):
        return '| ' + ' | '.join(c.ljust(w) if i == 0 else c.rjust(w)
                                 for i, (c, w) in enumerate(zip(cells, widths))) + ' |'
    sep = '|' + '|'.join('-' * (w + 2) if i == 0 else '-' * (w + 1) + ':' for i, w in enumerate(widths)) + '|'
    # only the header's rule is a delimiter; this one renders as a row, so colons would show as text
    rule = sep.replace(':', '-')
    return '\n'.join([line(header), sep] + [line(r) for r in rows] + ([rule, line(total)] if total else []))


def compare_reports(base_by_file, cur_by_file, labels=SIDE_LABELS):
    """Markdown per-file delta table; files sorted by |flash delta| desc, TOTAL over all files.
    Each region cell is `base → new`, both values padded so they line up down the column;
    `labels` name the two sides in the header."""
    zero = {'flash': 0, 'ram': 0}
    header = [f'File ({labels[0]} → {labels[1]})', 'Flash', 'Flash Δ', 'RAM', 'RAM Δ']
    rows = [(path, base_by_file.get(path, zero), cur_by_file.get(path, zero))
            for path in sorted(set(base_by_file) | set(cur_by_file))]
    changed = sorted((r for r in rows if any(_delta(r[1], r[2]))), key=lambda r: -abs(_delta(r[1], r[2])[0]))
    shown = changed + [('TOTAL', _files_total(base_by_file), _files_total(cur_by_file))]
    bw = {k: max(len(str(b[k])) for _, b, _ in shown) for k in zero}
    cw = {k: max(len(str(c[k])) for _, _, c in shown) for k in zero}

    def cells(b, c):
        return [cell for k in zero for cell in (f'{b[k]:>{bw[k]}} → {c[k]:>{cw[k]}}', _fmt(c[k] - b[k]))]
    *table, total_row = [[path, *cells(b, c)] for path, b, c in shown]
    if not changed and rows:
        table.append(['_no per-file changes_'] + [''] * (len(header) - 1))
    return md_table(header, table, total_row) + '\n'


def _section_deltas(b, c, path):
    """{section: Δ} of one file."""
    bs, cs = b['sections'].get(path, {}), c['sections'].get(path, {})
    return {sec: cs.get(sec, 0) - bs.get(sec, 0) for sec in set(bs) | set(cs)}


def _symbol_deltas(b, c, path):
    """{(section, name): Δ} of one file's changed symbols."""
    bs, cs = b['symbols'].get(path, {}), c['symbols'].get(path, {})
    deltas = {}
    for sec in set(bs) | set(cs):
        bn, cn = bs.get(sec, {}), cs.get(sec, {})
        for name in set(bn) | set(cn):
            if cn.get(name, 0) != bn.get(name, 0):
                deltas[(sec, name)] = cn.get(name, 0) - bn.get(name, 0)
    return deltas


def _changed_files(b, c, symbols):
    """(path, section Δs, symbol Δs) of each file whose sections, or with `symbols`
    whose symbols, changed; a file's changes can cancel in flash/RAM."""
    rows = []
    for path in sorted(set(b['sections']) | set(c['sections'])):
        secs = _section_deltas(b, c, path)
        syms = _symbol_deltas(b, c, path) if symbols else {}
        if any(secs.values()) or syms:
            rows.append((path, secs, syms))
    return rows


def _row_label(symbols, engine):
    if not symbols:
        return 'File'
    return 'File / input section' if engine == 'linkermap' else 'File / symbol'


def size_table(sizes, symbols, engine):
    """linkermap's table: a row per file, a column per output section, then size, % of
    the filtered total and TOTAL; with `symbols` each file's symbols follow it."""
    by_file = sizes['sections']
    cols = sorted({sec for secs in by_file.values() for sec in secs}, reverse=True)
    total = sum(sum(secs.values()) for secs in by_file.values())

    def row(name, secs):
        size = sum(secs.values())
        pct = f'{100 * size / total:.1f}%' if total else '-'
        return [name] + [str(secs.get(sec, 0)) for sec in cols] + [str(size), pct]

    lines = []
    for path, secs in sorted(by_file.items(), key=lambda kv: (-sum(kv[1].values()), kv[0])):
        lines.append(row(path, secs))
        if symbols:
            syms = [((sec, name), n) for sec, names in sizes['symbols'].get(path, {}).items()
                    for name, n in names.items()]
            for (sec, name), n in sorted(syms, key=lambda kv: (-kv[1], kv[0])):
                lines.append(row(f'└ {name}', {sec: n}))
    total_row = row('TOTAL', {sec: sum(secs.get(sec, 0) for secs in by_file.values()) for sec in cols})
    return md_table([_row_label(symbols, engine)] + cols + ['size', '%'], lines, total_row) + '\n'


def render_report(sizes, engine, failures=(), boards=(), symbols=False):
    """Markdown report of one tree's elfs keyed by (board, elf path), None for a
    failed one; `failures` are (elf id, stage, message). One elf is inline, several
    get a summary table and each its own table in <details>."""
    sized = {i: s for i, s in sizes.items() if s is not None}
    status = 'INCOMPLETE' if failures or not sized else 'complete'
    all_label = ENGINES[engine].all_label
    lines = [f'**Coverage ({status}, {engine}):** {len(sized)} of {len(sizes)} elfs sized']
    if boards:
        lines.append('- boards: ' + ', '.join(f'`{b}`' for b in boards))
    for elf_id, stage, message in failures:
        lines.append(f'- FAILED `{_label(elf_id)}` {stage}: {_md_escape(message)}')
    lines.append('')
    if not sized:
        return '\n'.join(lines + ['_no sized elfs_', ''])

    def totals(s):
        src = _files_total(s['files'])
        return (f'filtered Flash {src["flash"]}, RAM {src["ram"]}; '
                f'{all_label} Flash {s["all"]["flash"]}, RAM {s["all"]["ram"]}')

    if len(sized) == 1:
        (elf_id, s), = sized.items()
        return '\n'.join(lines + [f'`{_label(elf_id)}` {totals(s)}', '', size_table(s, symbols, engine)])
    lines.append(md_table(['Elf', 'filtered Flash', 'filtered RAM', f'{all_label} Flash', f'{all_label} RAM'],
                          [[_label(i)] + [str(t[k]) for t in (_files_total(s['files']), s['all']) for k in ('flash', 'ram')]
                           for i, s in sized.items()]))
    for elf_id, s in sized.items():
        lines += ['', f'<details><summary>{_label(elf_id)}</summary>', '',
                  f'{totals(s)}', '', size_table(s, symbols, engine), '</details>']
    return '\n'.join(lines) + '\n'


def delta_table(b, c, symbols, engine):
    """linkermap's table shape as deltas: a row per changed file, a column per changed
    output section, then size Δ and TOTAL; with `symbols` each file's changed
    symbols follow it."""
    rows = _changed_files(b, c, symbols)
    if not rows:
        return '_no section changes_\n'
    cols = sorted({sec for _, secs, _ in rows for sec, d in secs.items() if d}
                  | {sec for _, _, syms in rows for sec, _ in syms}, reverse=True)
    label = _row_label(symbols, engine)

    def row(name, deltas):
        return [name] + [_fmt(deltas.get(sec, 0)) for sec in cols] + [_fmt(sum(deltas.values()))]

    lines = []
    total = {}
    for path, secs, syms in sorted(rows, key=lambda r: (-abs(sum(r[1].values())), r[0])):
        lines.append(row(path, secs))
        for (sec, name), d in sorted(syms.items(), key=lambda kv: (-abs(kv[1]), kv[0])):
            lines.append(row(f'└ {name}', {sec: d}))
        for sec, d in secs.items():
            total[sec] = total.get(sec, 0) + d
    return md_table([label] + cols + ['size Δ'], lines, row('TOTAL', total)) + '\n'


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


def _files_total(by_file):
    return {k: sum(f[k] for f in by_file.values()) for k in ('flash', 'ram')}


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


def _pair_tables(b, c, symbols, engine, labels):
    """A pair's Flash/RAM file table and its section delta table."""
    return compare_reports(b['files'], c['files'], labels), delta_table(b, c, symbols, engine)


def _pair_changed(b, c, symbols):
    return (any(_delta(b['all'], c['all'])) or any(any(d) for d in _file_deltas(b, c).values())
            or bool(_changed_files(b, c, symbols)))


def render_pairs(pairs, matched, engine, base_only=(), cur_only=(), failures=(), boards=(), symbols=False,
                 labels=SIDE_LABELS):
    """Markdown report over paired elfs keyed by (board, elf path).

    Every statistic is over per-pair deltas, sized by `engine`. `matched` counts
    ids built on both sides, compared or not;
    `failures` are (elf id, side, stage, message), the elf path None for a
    board-level failure. Failures or unmatched elfs mark the report INCOMPLETE.
    `boards` lists the requested boards, so an unchanged or failed one is named.
    `symbols` adds each file's changed symbols to each pair's section table; `labels`
    name the base and current sides.
    """
    file_deltas = {i: _file_deltas(b, c) for i, (b, c) in pairs.items()}
    changed = [i for i, (b, c) in pairs.items() if _pair_changed(b, c, symbols)]
    status = _status(failures, base_only, cur_only, pairs)
    all_label = ENGINES[engine].all_label
    lines = [f'**Coverage ({status}, {engine}):** {len(pairs)} of {matched} matched elf pairs '
             f'compared, {len(changed)} changed']
    if boards:
        lines.append('- boards: ' + ', '.join(f'`{b}`' for b in boards))
    for label, ids in (('base-only', base_only), ('current-only', cur_only)):
        if ids:
            lines.append(f'- {label}: ' + ', '.join(f'`{_label(i)}`' for i in ids))
    for elf_id, side, stage, message in failures:
        lines.append(f'- FAILED `{_label(elf_id)}` {side} {stage}: {_md_escape(message)}')
    lines.append('')

    if len(pairs) == 1:
        (elf_id, (b, c)), = pairs.items()
        df, dr = _delta(b['all'], c['all'])
        lines += [f'`{_label(elf_id)}` {all_label}: Flash Δ {_fmt(df)}, RAM Δ {_fmt(dr)}', '',
                  *_pair_tables(b, c, symbols, engine, labels)]
        return '\n'.join(lines)
    if not pairs:
        return '\n'.join(lines + ['_no comparable pairs_', ''])
    if not changed:
        return '\n'.join(lines + ['_no changes_', ''])

    pair_rows = []
    for elf_id in changed:
        b, c = pairs[elf_id]
        src = _delta(_files_total(b['files']), _files_total(c['files']))
        syms = _delta(b['all'], c['all'])
        pair_rows.append([_label(elf_id)] + [_fmt(d) for d in src + syms])
    lines.append(md_table(['Pair', 'filtered Flash Δ', 'filtered RAM Δ', f'{all_label} Flash Δ',
                           f'{all_label} RAM Δ'], pair_rows))

    stats = _file_stats(file_deltas)
    rows = sorted((p for p, s in stats.items() if s['changed']),
                  key=lambda p: (-max(abs(v[0]) for k in ('flash', 'ram') for v in stats[p][k]), p))
    lines += ['', '_Changed / present: pairs where the file\'s Flash or RAM total changed / compared pairs '
              'that contain it._', '',
              md_table(['File', 'Changed / present', 'Flash Δ min', 'Flash Δ max', 'RAM Δ min', 'RAM Δ max'],
                       [[path, f'{stats[path]["changed"]}/{stats[path]["present"]}']
                        + [_extreme(v) for k in ('flash', 'ram') for v in stats[path][k]] for path in rows])]

    for elf_id in changed:
        b, c = pairs[elf_id]
        lines += ['', f'<details><summary>{_label(elf_id)}</summary>', '',
                  *_pair_tables(b, c, symbols, engine, labels), '</details>']
    return '\n'.join(lines) + '\n'


def _status(failures, base_only, cur_only, pairs):
    return 'INCOMPLETE' if failures or base_only or cur_only or not pairs else 'complete'


def _elf_record(elf_id):
    board, elf = elf_id
    return {'board': board, 'elf': elf}


def _json_sizes(sizes, symbols):
    """An elf's sizes for JSON, symbols included only when `symbols` is set."""
    return sizes if symbols else {k: v for k, v in sizes.items() if k != 'symbols'}


def _filter_failure(engine):
    return (f'no {engine} sizes matched filters - check them, or a change in '
            f'{engine} output broke its parsing (try another --engine to isolate)')


def compare_sides(base, cur, engine, failures=(), boards=(), scope=None, symbols=False, labels=SIDE_LABELS):
    """Pair two sides and render them. Returns (md, failures, ok, data).

    `failures` comes back with a filter failure for `scope` added when no
    compared pair matched a file (wrong filters, or an engine output change
    broke its parsing); pass `scope=None` when each scope was already checked.
    `ok` is False on any failure or when nothing was compared. `data` is the
    report's raw paired sizes, for JSON, symbols included only when `symbols` is set.
    """
    pairs, base_only, cur_only = pair_elfs(base, cur)
    failures = list(failures)
    if scope and pairs and not any(b['files'] or c['files'] for b, c in pairs.values()):
        failures.append((scope, 'both', 'filter', _filter_failure(engine)))
    md = render_pairs(pairs, len(base.keys() & cur.keys()), engine, base_only, cur_only, failures, boards,
                      symbols, labels)
    data = {
        'engine': engine,
        'boards': list(boards),
        'status': _status(failures, base_only, cur_only, pairs),
        'pairs': [{**_elf_record(i), 'base': _json_sizes(b, symbols), 'current': _json_sizes(c, symbols)}
                  for i, (b, c) in pairs.items()],
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
        # captured output is bytes even with text=True
        out, err = (v.decode(errors='replace') if isinstance(v, bytes) else (v or '')
                    for v in (e.stdout, e.stderr))
        msg = f'Command timed out after {e.timeout}s: {" ".join(shlex.quote(str(c)) for c in cmd)}'
        return subprocess.CompletedProcess(cmd, 124, stdout=out, stderr=err + ('\n' if err else '') + msg)


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


def short_hash(checkout):
    """`checkout`'s short HEAD hash, `-dirty` when tracked files differ from it; None
    when git cannot tell. `--exclude='*'` keeps tag names out."""
    ret = run(['git', '-C', checkout, 'describe', '--always', '--dirty', '--exclude=*'])
    return ret.stdout.strip() or None if ret.returncode == 0 else None


def ci_pinned_boards():
    """Boards of .github/ci-pinned-boards.json: CI's membrowse set, which covers every
    dcd/hcd driver not waived in its `uncovered` list (drivers-coverage hook)."""
    with open(CI_PINNED_BOARDS) as f:
        return [entry['board'] for entry in json.load(f)['boards']]


class Phase:
    """A progress line `  label… 1.2s`, or `FAILED after 1.2s`; with -v the commands
    print between the label and its time, so the time gets a line of its own."""
    def __init__(self, label):
        self.label, self.start = label, time.monotonic()
        print(f'  {label}…', end='\n' if verbose else ' ', flush=True)

    def done(self, failed=False):
        took = f'{"FAILED after " if failed else ""}{time.monotonic() - self.start:.1f}s'
        print(f'  {self.label}: {took}' if verbose else took)


_NINJA_PROGRESS = re.compile(r'\[\d+/\d+\] ')


def output_excerpt(ret, lines=20):
    """Up to `lines` of each nonempty stream of a failed command: from ninja's first
    `FAILED:` block when there is one, else the stream's tail. ninja reports a compile
    error on stdout, leaving stderr empty, and the jobs already running when it failed
    finish after it, so their `[n/m]` progress lines are dropped."""
    def excerpt(stream):
        kept = [line for line in stream.rstrip().splitlines() if not _NINJA_PROGRESS.match(line)]
        first = next((i for i, line in enumerate(kept) if line.startswith('FAILED:')), None)
        return kept[-lines:] if first is None else kept[first:first + lines]
    return [line for stream in (ret.stdout, ret.stderr) if stream.strip() for line in excerpt(stream)]


def build_error(ret, src_dir):
    """A failed build's first error for the report, from its whole output: the first
    diagnostic, else its last line that is not ninja's own, with paths relative to `src_dir`."""
    out = '\n'.join(stream for stream in (ret.stdout, ret.stderr) if stream)
    lines = [line.strip() for line in out.splitlines()
             if line.strip() and not _NINJA_PROGRESS.match(line) and not line.startswith(('ninja: ', 'FAILED: '))]
    error = build_utils.first_error(out) or (lines[-1] if lines else 'no output')
    return error.replace(src_dir.rstrip(os.sep) + os.sep, '')


def build_board(src_dir, build_dir, board, example, label):
    """Configure and build examples for a board as a `label` progress phase, printing
    an excerpt of the output on failure. Returns None on success, else build_error().

    When `example` is given, only that target is built (`cmake --build --target NAME`),
    keeping single-example workflows fast.
    """
    phase = Phase(label)
    os.makedirs(build_dir, exist_ok=True)
    ret = run(['cmake', '-B', build_dir, '-G', 'Ninja',
               f'-DBOARD={board}', '-DCMAKE_BUILD_TYPE=MinSizeRel',
               os.path.join(src_dir, 'examples')])
    if ret.returncode == 0:
        cmd = ['cmake', '--build', build_dir]
        if example:
            cmd += ['--target', os.path.basename(example)]
        ret = run(cmd, timeout=600)
    failed = ret.returncode != 0
    phase.done(failed=failed)
    if not failed:
        return None
    print('\n'.join('    ' + line for line in output_excerpt(ret)))
    return build_error(ret, src_dir)


def report_path(board, example, kind='diff'):
    """A scope's report path without extension: cmake-code-size/<board>/<kind>[_<ex>]."""
    suffix = f'_{example.replace("/", "_")}' if example else ''
    return os.path.join(CODE_SIZE_DIR, board, f'{kind}{suffix}')


def drop_stale_reports(boards, examples, kind):
    """Remove the reports a run will write before anything can fail: a run that stops
    early must not leave a previous run's report for a reader to take for this one's,
    since cmake-code-size/ is gitignored and persists. .json too: a run without --json
    must not leave an older one beside a newer .md."""
    for board in boards:
        for example in examples:
            for ext in ('md', 'json'):
                stale_path = f'{report_path(board, example, kind)}.{ext}'
                if os.path.isfile(stale_path):
                    os.remove(stale_path)


def generate_sizes(build_dir, filters, example=None, engine='membrowse'):
    """Return (sizes, errors) for the scope's elfs.

    `sizes` maps each elf path relative to `build_dir` to its
    ENGINES[engine] sizes, or to None when that failed; `errors`
    lists (relative elf path, message), the path None when no elf can be
    sized at all.
    """
    # escape the dir, not the wildcards: a checkout path is a path, not a pattern
    root = glob.escape(build_dir)
    # <role>/<example>/*.elf: deeper elfs are helpers, e.g. pico-sdk's bs2_default.elf
    elfs = sorted(glob.glob(f'{root}/{example or "*/*"}/*.elf'))
    if not elfs:
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
        return {}, [(None, f'{engine} not found ({e}) - install {ENGINES[engine].install}, '
                           f'or pick another --engine')]
    sizes, errors = {}, []
    for elf, (elf_sizes, error) in zip(elfs, results):
        rel = os.path.relpath(elf, build_dir)
        sizes[rel] = elf_sizes
        if error:
            errors.append((rel, error))
    return sizes, errors


def diff_summary(data, symbols):
    """Console lines of one diff from its JSON-shaped `data`: coverage, the single
    pair's filtered Δ, unmatched elfs and failures."""
    pairs = {(p['board'], p['elf']): (p['base'], p['current']) for p in data['pairs']}
    changed = sum(_pair_changed(b, c, symbols) for b, c in pairs.values())
    line = f'{len(pairs)} pair{"" if len(pairs) == 1 else "s"}, {changed} changed'
    if len(pairs) == 1:
        (b, c), = pairs.values()
        df, dr = _delta(_files_total(b['files']), _files_total(c['files']))
        line += f'; filtered Flash Δ {_fmt(df)}, RAM Δ {_fmt(dr)}'
    lines = [line if data['status'] == 'complete' else f'INCOMPLETE: {line}']
    for key in ('base_only', 'current_only'):
        if data[key]:
            lines.append(f'{key.replace("_", "-")}: ' + ', '.join(_label((r['board'], r['elf'])) for r in data[key]))
    lines += [f'FAILED {_label((f["board"], f["elf"]))} {f["side"]} {f["stage"]}: {f["message"]}'
              for f in data['failures']]
    return lines


def report_summary(sizes, failures, ok):
    """Console lines of one report: coverage, the single elf's filtered total, failures."""
    sized = [s for s in sizes.values() if s is not None]
    line = f'{len(sized)} of {len(sizes)} elfs sized'
    if len(sized) == 1:
        src = _files_total(sized[0]['files'])
        line += f'; filtered Flash {src["flash"]}, RAM {src["ram"]}'
    lines = [line if ok else f'INCOMPLETE: {line}']
    return lines + [f'FAILED {_label(i)} {stage}: {message}' for i, stage, message in failures]


def print_result(lines, prefix='', tables=()):
    """A scope's result lines, then `tables` (Markdown, or an elf's label heading its
    tables), indented under its phases."""
    for i, line in enumerate(lines):
        print(f'  {prefix}{line}' if i == 0 else f'    {line}')
    for table in tables:
        print('\n' + '\n'.join(f'  {row}' for row in table.rstrip('\n').split('\n')))
    if tables:
        print()


def _shown(path):
    """`path` relative to the working directory when under it."""
    rel = os.path.relpath(path)
    return path if rel.startswith('..') else rel


def write_report(path, md, data=None):
    """Write a report to `path`.md, and `data` to `path`.json when given, printing
    their paths."""
    with open(f'{path}.md', 'w') as f:
        f.write(md)
    print(f'  report: {_shown(path)}.md')
    if data is not None:
        with open(f'{path}.json', 'w') as f:
            json.dump(data, f, indent=1, sort_keys=True)
            f.write('\n')
        print(f'  json: {_shown(path)}.json')


def _focused(boards, examples):
    """One board and one example: the console also gets that scope's tables."""
    return len(boards) == 1 and bool(_single_example(examples))


def _labelled(tables_by_elf, elfs):
    """Console tables of {elf id: tables}, each elf's under its label when the scope
    has several `elfs`, shown or not."""
    many = elfs > 1
    return [t for elf_id, tables in tables_by_elf.items()
            for t in ([f'{_label(elf_id)}:'] if many else []) + list(tables)]


def _single_example(examples):
    """The board header's ` / <example>` when one example is sized."""
    return f' / {examples[0]}' if len(examples) == 1 and examples[0] else ''


def _scope_label(examples, example):
    """A phase label's ` <example>` suffix, needed only when a board has several."""
    return f' {example}' if example and len(examples) > 1 else ''


def _build_failed(example, error):
    return f'build failed{f" ({example})" if example else ""}: {error}'


def run_report(args):
    """`report`: build and size the working tree, one report per board and example."""
    filters = args.filter or [tinyusb_src_filter(TINYUSB_ROOT)]
    examples = args.example or [None]
    drop_stale_reports(args.board, examples, 'report')
    focused = _focused(args.board, examples)
    print(f'report working tree · {args.engine}')
    failed = False
    for n, board in enumerate(args.board, 1):
        print(f'[{n}/{len(args.board)}] {board}{_single_example(examples)}')
        build = os.path.join(CODE_SIZE_DIR, board, 'build')
        shutil.rmtree(build, ignore_errors=True)
        # each scope is its own report, so one example's build failure spares the others
        for example in examples:
            scope = _scope_label(examples, example)
            sizes, failures = {}, []
            error = build_board(TINYUSB_ROOT, build, board, example, f'build{scope}')
            if error:
                failures.append(((board, None), 'build', _build_failed(example, error)))
            else:
                phase = Phase(f'size{scope}')
                rel_sizes, errors = generate_sizes(build, filters, example, args.engine)
                sizes = {(board, rel): v for rel, v in rel_sizes.items()}
                failures += [((board, rel), 'report', msg) for rel, msg in errors]
                # per scope, as diff: an example need not link TinyUSB (board_test)
                sized = [s for s in sizes.values() if s is not None]
                if sized and not any(s['files'] for s in sized):
                    failures.append(((board, None), 'filter', _filter_failure(args.engine)))
                phase.done(failed=bool(failures))
            md = render_report(sizes, args.engine, failures, [board], args.symbols)
            ok = not failures and any(s is not None for s in sizes.values())
            failed |= not ok
            data = None
            if args.json:
                data = {'engine': args.engine, 'boards': [board], 'filters': filters,
                        'status': 'complete' if ok else 'INCOMPLETE',
                        'elfs': [{**_elf_record(i), 'sizes': _json_sizes(s, args.symbols)}
                                 for i, s in sizes.items() if s is not None],
                        'failures': [{**_elf_record(i), 'stage': stage, 'message': message}
                                     for i, stage, message in failures]}
            tables = _labelled({i: [size_table(s, args.symbols, args.engine)] for i, s in sizes.items() if s},
                               len(sizes)) if focused else ()
            print_result(report_summary(sizes, failures, ok), f'{example}: ' if scope else '', tables)
            write_report(report_path(board, example, 'report'), md, data)
    return 1 if failed else 0


def main():
    global verbose

    common = argparse.ArgumentParser(add_help=False)
    common.add_argument('-b', '--board', action='append', default=[],
                        help='Board name (repeatable). Required unless diff --ci is given.')
    common.add_argument('-f', '--filter', action='append', default=None,
                        help='Path-substring filter (repeatable). When given, '
                             'overrides the default and is applied to every build. '
                             'Default: each build\'s own absolute <checkout>/src/ path, '
                             'which uniquely matches TinyUSB stack code without colliding '
                             'with vendored deps.')
    common.add_argument('-e', '--example', action='append', default=None,
                        help='Size specific example (repeatable, e.g. -e device/cdc_msc -e host/cdc_msc_hid)')
    common.add_argument('--engine', choices=sorted(ENGINES),
                        default='membrowse',
                        help='Where per-file sizes come from: membrowse (default, symbols with '
                             'ELF-header flash/RAM), linkermap (the GNU ld map\'s input '
                             'sections) or bloaty (DWARF compile units)')
    common.add_argument('--symbols', action='store_true',
                        help='Show each file\'s symbols under it (linkermap: its input sections); '
                             'with --json, include them in the JSON')
    common.add_argument('--json', action='store_true',
                        help='Also write each report\'s sizes as .json next to its .md')
    common.add_argument('-v', '--verbose', action='store_true',
                        help='Print build commands')

    top = argparse.ArgumentParser(description='Code size of TinyUSB examples')
    sub = top.add_subparsers(dest='command', required=True)
    report_parser = sub.add_parser('report', parents=[common],
                                   help='size the working tree (cmake-code-size/<board>/report*.md)')
    parser = sub.add_parser('diff', parents=[common],
                            help='diff code size against a base ref (cmake-code-size/<board>/diff*.md)')
    parser.add_argument('--base-branch', default='master',
                        help='Base branch to compare against (default: master)')
    parser.add_argument('--bloaty', action='store_true',
                        help='Also print bloaty\'s section and symbol diff of each -e example '
                             '(console only, whatever --engine)')
    parser.add_argument('--ci', action='store_true',
                        help='Add the CI-pinned boards (.github/ci-pinned-boards.json, covering '
                             'every dcd/hcd driver not waived there). Implies --combined.')
    parser.add_argument('--combined', action='store_true',
                        help='Also write one comparison over every board '
                             '(cmake-code-size/_combined/diff.md), in addition to per-board.')
    args = top.parse_args()
    verbose = args.verbose

    if args.command == 'report':
        if not args.board:
            report_parser.error('at least one -b BOARD is required')
        return run_report(args)

    if args.bloaty and not args.example:
        parser.error('--bloaty requires -e/--example')

    if args.ci:
        args.combined = True
        args.board = list(dict.fromkeys(args.board + ci_pinned_boards()))

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

    examples = args.example or [None]
    combined_dir = os.path.join(CODE_SIZE_DIR, '_combined')
    if args.combined:
        shutil.rmtree(combined_dir, ignore_errors=True)
    drop_stale_reports(args.board, examples, 'diff')

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
    current_rev = short_hash(TINYUSB_ROOT)
    labels = (short_hash(worktree_dir) or SIDE_LABELS[0], current_rev or SIDE_LABELS[1])
    print(f'diff {args.base_branch} ({labels[0]}) vs working tree ({labels[1]}) · {args.engine}')
    focused = _focused(args.board, examples)

    def report_data(data):
        """The JSON report for --json, None without it."""
        if not args.json:
            return None
        return {**data, 'base_ref': args.base_branch, 'base_sha': base_sha, 'current_rev': current_rev,
                'filters': {'base': base_filters, 'current': cur_filters}}

    failed = False
    try:
        # --combined: every board's elf sizes and failures, paired at the end
        combined_sides = {'base': {}, 'current': {}}
        combined_failures = []

        for n, board in enumerate(args.board, 1):
            print(f'[{n}/{len(args.board)}] {board}{_single_example(examples)}')
            board_dir = os.path.join(CODE_SIZE_DIR, board)
            base_build = os.path.join(board_dir, 'base')
            cur_build = os.path.join(board_dir, 'build')
            shutil.rmtree(base_build, ignore_errors=True)
            shutil.rmtree(cur_build, ignore_errors=True)

            build_failure = None
            for example in examples:
                scope = _scope_label(examples, example)
                for side, src, build, label in (('base', worktree_dir, base_build, f'build {args.base_branch}{scope}'),
                                                ('current', TINYUSB_ROOT, cur_build, f'build current{scope}')):
                    error = build_board(src, build, board, example, label)
                    if error:
                        build_failure = ((board, None), side, 'build', _build_failed(example, error))
                        break
                if build_failure:
                    break
            if build_failure:
                failed = True
                # still write each scope's report, with the build failure in it
                combined_failures.append(build_failure)

            for example in examples:
                scope = _scope_label(examples, example)
                sides = {'base': {}, 'current': {}}
                failures = [build_failure] if build_failure else []
                if not build_failure:
                    phase = Phase(f'size and compare{scope}')
                    for side, build, filters in (('base', base_build, base_filters),
                                                 ('current', cur_build, cur_filters)):
                        sizes, errors = generate_sizes(build, filters, example, args.engine)
                        sides[side] = {(board, rel): v for rel, v in sizes.items()}
                        failures += [((board, rel), side, 'report', msg) for rel, msg in errors]

                md, failures, ok, data = compare_sides(
                    sides['base'], sides['current'], args.engine, failures, [board], scope=(board, None),
                    symbols=args.symbols, labels=labels)
                if not build_failure:
                    phase.done(failed=not ok)
                failed |= not ok
                if not build_failure:  # a build failure is recorded once, above
                    for side, sizes in sides.items():
                        combined_sides[side].update(sizes)
                    combined_failures += failures
                tables = _labelled({(p['board'], p['elf']): _pair_tables(p['base'], p['current'], args.symbols,
                                                                         args.engine, labels)
                                    for p in data['pairs'] if _pair_changed(p['base'], p['current'], args.symbols)},
                                   len(data['pairs'])) if focused else ()
                print_result(diff_summary(data, args.symbols), f'{example}: ' if scope else '', tables)
                write_report(report_path(board, example), md, report_data(data))

                if args.bloaty and example and not build_failure:
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
                        print('--- bloaty sections ---')
                        ret = run(bloaty_common + ['-d', 'compileunits,sections', cur_elf, '--', base_elf])
                        print(ret.stdout)
                        print('--- bloaty symbols ---')
                        ret = run(bloaty_common + ['-d', 'compileunits,symbols', '-s', 'vm',
                                                    cur_elf, '--', base_elf])
                        print(ret.stdout)
                    else:
                        print('  bloaty: ELF not found')

        if args.combined:
            os.makedirs(combined_dir, exist_ok=True)
            print(f'combined ({len(args.board)} boards)')
            # every scope was filter-checked above, and its failures carried over
            md, _failures, ok, data = compare_sides(
                combined_sides['base'], combined_sides['current'], args.engine,
                combined_failures, args.board, symbols=args.symbols, labels=labels)
            failed |= not ok
            print_result(diff_summary(data, args.symbols))
            write_report(os.path.join(combined_dir, 'diff'), md, report_data(data))
    finally:
        ret = run(['git', '-C', TINYUSB_ROOT, 'worktree', 'remove', '--force', worktree_dir])
        if ret.returncode != 0:
            print(f'Error removing worktree {worktree_dir}: {ret.stderr.strip()}')
            failed = True
    return 1 if failed else 0


if __name__ == '__main__':
    sys.exit(main())
