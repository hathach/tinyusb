#!/usr/bin/env python3
"""Code size of TinyUSB examples.

`report` builds the working tree in cmake-code-size/<board>/build and tabulates each elf's
per-file section sizes in cmake-code-size/<board>/report[_<ex>].md.

`diff` builds the current tree in cmake-code-size/<board>/build and takes the base branch's
(master) sizes from CI's stored snapshots, else builds it in cmake-code-size/<board>/base
(--base-source); it pairs the elfs by (board, elf path) and reports
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
import contextlib
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
import signal
import struct
import subprocess
import sys
import time

import build_utils
from membrowse_cli import IDF_LD_SCRIPTS, extract_ld_scripts, extract_defsyms, link_command, report_inputs

# resolved like tinyusb_src_filter(), so a symlinked checkout still matches its filter
TINYUSB_ROOT = os.path.dirname(os.path.dirname(os.path.realpath(__file__)))
CODE_SIZE_DIR = os.path.join(TINYUSB_ROOT, 'cmake-code-size')
WINDOWS = os.name == 'nt'
# a master run's snapshots are the same for every checkout; XDG ignores a relative XDG_CACHE_HOME
_cache_home = os.environ.get('LOCALAPPDATA' if WINDOWS else 'XDG_CACHE_HOME', '')
BASELINE_CACHE_DIR = os.path.join(_cache_home if os.path.isabs(_cache_home) else os.path.expanduser('~/.cache'),
                                  'tinyusb', 'code-size-baseline')
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
    # ESP-IDF links its generated scripts by bare name via -L
    idf_ld = [os.path.join(build_dir, p) for p in IDF_LD_SCRIPTS]
    if any(map(os.path.isfile, idf_ld)):
        if not all(map(os.path.isfile, idf_ld)):
            raise RuntimeError(f'ESP-IDF build {build_dir} lacks one of {", ".join(idf_ld)}')
        return build_dir, idf_ld, extract_defsyms(commands)
    ld_scripts = extract_ld_scripts(commands)
    if not ld_scripts:
        raise RuntimeError(f'no linker script found in the ninja build graph '
                            f'for {elf_path}')
    return build_dir, ld_scripts, extract_defsyms(commands)


def report_for_elf(elf_path):
    """Run membrowse local report on one elf, return parsed JSON dict."""
    build_dir, ld_scripts, defsyms = _link_settings(elf_path)
    cmd = (['membrowse', 'report'] + report_inputs(elf_path, ld_scripts, defsyms)
           + ['--json', '--all-symbols'])
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
    if WINDOWS:
        src = src.replace('\\', '/')
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


def _map_path(elf):
    """The elf's GNU ld map: `<elf>.map`, or ESP-IDF's `<name>.map` beside it."""
    idf = os.path.splitext(elf)[0] + '.map'
    return idf if not os.path.isfile(elf + '.map') and os.path.isfile(idf) else elf + '.map'


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
    if any(s[0] == '.flash.appdesc' for s in sections):
        return _esp_idf_buckets(sections)
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


def _esp_idf_buckets(sections):
    """section_buckets() of an ESP-IDF app (it has `.flash.appdesc`), whose ELF runs every
    section where it loads. IDF's sections.ld names each output section in a flash-mapped region
    `.flash*`; esptool's elf2image stores the rest in the image too, for the bootloader to load
    into RAM (bin_image.py is_flash_addr())."""
    buckets = {}
    for name, sh_type, flags, _addr, _size in sections:
        if not flags & SHF_ALLOC or (sh_type == SHT_NOBITS and name.startswith('.flash')):
            buckets[name] = _NOT_COUNTED  # reserved flash-window address space, not storage
        elif sh_type == SHT_NOBITS:
            buckets[name] = frozenset({'ram'})
        elif name.startswith('.flash'):
            buckets[name] = frozenset({'flash'})
        else:
            buckets[name] = frozenset({'flash', 'ram'})
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


@functools.lru_cache(maxsize=None)
def _dwarf_sources(elf):
    """{basename: {full source path}} of the elf's DWARF compile units."""
    try:
        from elftools.elf.elffile import ELFFile  # pyelftools, a membrowse dependency
    except ImportError as e:
        raise RuntimeError(f'pyelftools is needed to resolve {elf}\'s source paths ({e})')
    sources = {}
    with open(elf, 'rb') as f:
        elffile = ELFFile(f)
        if not elffile.has_dwarf_info():
            return sources
        for cu in elffile.get_dwarf_info().iter_CUs():
            top = cu.get_top_DIE().attributes
            if 'DW_AT_name' in top:
                comp_dir = top['DW_AT_comp_dir'].value.decode() if 'DW_AT_comp_dir' in top else ''
                path = os.path.normpath(os.path.join(comp_dir, top['DW_AT_name'].value.decode()))
                sources.setdefault(os.path.basename(path), set()).add(path)
    return sources


def _source_path(sym, elf, filters):
    """A symbol's source path. An archive member has no object path and membrowse 1.2.9
    keeps only the basename of its source, so the DWARF compile unit of that name gives it
    back (ESP-IDF links TinyUSB from an archive)."""
    path = sym.get('object_file') or sym.get('source_file')
    if not path or '/' in path:
        return path
    candidates = _dwarf_sources(elf).get(path, set())
    if len(candidates) == 1:
        return next(iter(candidates))
    if any(_relative_key(c, filters) for c in candidates):
        raise RuntimeError(f'{elf}: {path} names several compile units ({", ".join(sorted(candidates))}), '
                           f'one of them filtered; cannot tell which holds {sym.get("name")}')
    return path


def membrowse_sizes(elf, filters):
    """Symbols of `membrowse report`; linker-defined symbols without a section
    (`__StackLimit`) are not counted."""
    buckets = section_buckets(elf, _map_path(elf))
    sizes = _Sizes(filters)
    for sym in report_for_elf(elf).get('symbols', []):
        section = sym.get('section')
        if not sym.get('size') or not section:
            continue
        if section not in buckets:
            raise RuntimeError(f'membrowse symbol {sym.get("name")} is in section {section}, not in {elf}')
        sizes.add(_source_path(sym, elf, filters), section, buckets[section], sym['size'], sym.get('name'))
    return sizes.result()


@functools.cache
def _load_module(name, path):
    """The Python file at `path`, imported once as `name`."""
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


@functools.cache
def _linkermap():
    path = os.path.join(TINYUSB_ROOT, 'tools', 'linkermap', 'linkermap.py')
    if not os.path.isfile(path):
        raise FileNotFoundError(f'{path} not found - run `python3 tools/get_deps.py tools/linkermap`')
    return _load_module('linkermap', path)


def linkermap_sizes(elf, filters):
    """Input sections of the elf's map by full object path; analyze_map() is not used,
    it keeps only four sections. An archive member
    (`lib.a(x.o)`) has no source dir, so it counts in 'all' only. The symbols are the
    input sections (`.text.foo`), not the labels inside one."""
    map_path = _map_path(elf)
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
    buckets = section_buckets(elf, _map_path(elf))
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
MEMBROWSE_CI_VERSION = '1.2.12'  # every CI install pins this; attribution can differ across versions
ENGINES = {
    'membrowse': Engine(membrowse_sizes, 'all symbols', f'`pip install membrowse=={MEMBROWSE_CI_VERSION}`'),
    'linkermap': Engine(linkermap_sizes, 'all input sections',
                        '`python3 tools/get_deps.py tools/linkermap`'),
    'bloaty': Engine(bloaty_sizes, 'all accounted sections',
                     'bloaty on PATH (https://github.com/google/bloaty)'),
}


def engine_missing(engine):
    """Whether `engine`'s tool is absent; membrowse and bloaty are commands of that name."""
    if engine == 'linkermap':
        try:
            _linkermap()
        except FileNotFoundError:
            return True
        return False
    return not shutil.which(engine)


def _md_escape(text):
    """`text` safe in Markdown prose: gcc quotes as `name', which would open a code span."""
    return text.replace('`', '\\`')


def _fmt(delta):
    return f'+{delta}' if delta > 0 else str(delta)


_MD_CELL = str.maketrans({**{c: f'&#{ord(c)};' for c in '&<>|`\\[]\r\n'}, '@': '@\u200b'})


def _md_cell(text):
    """A table cell safe from names that would open HTML, a link or code span, end or split
    the row or @mention someone (a zero-width space breaks the mention)."""
    return text.translate(_MD_CELL)


def md_table(header, rows, total=None):
    """Markdown table padded so its columns also line up as plain text: the first
    column left-aligned, the others right-aligned. Cells are strings; a `total` row
    follows the rows under a plain rule."""
    header = [_md_cell(c) for c in header]
    rows = [[_md_cell(c) for c in r] for r in rows]
    total = [_md_cell(c) for c in total] if total else total
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


def render_report(sizes, engine, failures=(), boards=(), symbols=False, files_label='TinyUSB'):
    """Markdown report of one tree's elfs keyed by (board, elf path), None for a
    failed one; `failures` are (elf id, stage, message). One elf is inline, several
    get a summary table and each its own table in <details>; `files_label` names the
    filtered files' totals."""
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
        return (f'{files_label} Flash {src["flash"]}, RAM {src["ram"]}; '
                f'{all_label} Flash {s["all"]["flash"]}, RAM {s["all"]["ram"]}')

    if len(sized) == 1:
        (elf_id, s), = sized.items()
        return '\n'.join(lines + [f'`{_label(elf_id)}` {totals(s)}', '', size_table(s, symbols, engine)])
    lines.append(md_table(['Elf', f'{files_label} Flash', f'{files_label} RAM', f'{all_label} Flash', f'{all_label} RAM'],
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


def _range(lo, hi):
    return _fmt(lo) if lo == hi else f'{_fmt(lo)} → {_fmt(hi)}'


COMMENT_TERMS = {'base-only': 'missing in this PR', 'current-only': 'new in this PR (no master size yet)',
                 'base': 'master', 'current': 'PR'}


def _problems(base_only, cur_only, failures, terms=None):
    """Report lines for one-sided elfs and failures, each id named; `terms` rewords the report's words."""
    word = lambda w: (terms or {}).get(w, w)  # noqa: E731
    lines = [f'- {word(label)}: ' + ', '.join(f'`{_label(i)}`' for i in ids)
             for label, ids in (('base-only', base_only), ('current-only', cur_only)) if ids]
    return lines + [f'- FAILED `{_label(elf_id)}` {word(side)} {stage}: {_md_escape(message)}'
                    for elf_id, side, stage, message in failures]


def _changed_paths(stats):
    """The changed files of _file_stats(), largest |Δ| first."""
    return sorted((p for p, s in stats.items() if s['changed']),
                  key=lambda p: (-max(abs(v[0]) for k in ('flash', 'ram') for v in stats[p][k]), p))


def render_comment(pairs, engine, base_only=(), cur_only=(), failures=(), symbols=False, files_label='TinyUSB'):
    """The PR comment's view as (body, footnote): a row per changed file with its min to max
    Δ over the builds containing it, so each changed driver shows however heavy the others
    are. The footnote explains the whole-firmware totals, when the body has them."""
    file_deltas = {i: _file_deltas(b, c) for i, (b, c) in pairs.items()}
    changed = [i for i, (b, c) in pairs.items() if _pair_changed(b, c, symbols)]
    head = (f'{len(pairs)} builds on {len({board for board, _elf in pairs})} boards compared, '
            f'{len(changed)} changed')
    footnote = ''
    if changed:
        flash, ram = zip(*(_delta(pairs[i][0]['all'], pairs[i][1]['all']) for i in changed))
        head += f'. Whole firmware¹ Flash Δ {_range(min(flash), max(flash))}, RAM Δ {_range(min(ram), max(ram))}'
        footnote = f'\n¹ Sum of {ENGINES[engine].all_label} measured by {engine}, not the exact image size.\n'
    problems = _problems(base_only, cur_only, failures, COMMENT_TERMS)
    lines = [head, ''] + (['Not compared:', '', *problems, ''] if problems else [])
    if not changed:
        return '\n'.join(lines + ['_no changes_' if pairs else '_no comparable pairs_', '']), footnote
    stats = _file_stats(file_deltas)
    rows = [[p] + [_range(*(v[0] for v in stats[p][k])) for k in ('flash', 'ram')] for p in _changed_paths(stats)]
    if rows:
        lines += [f'{files_label} file size: min to max change across builds containing the file.', '',
                  md_table(['File', 'Flash Δ', 'RAM Δ'], rows)]
    other = sum(not any(any(d) for d in file_deltas[i].values()) for i in changed)
    if other:
        lines += ['', f'{other} other build{"" if other == 1 else "s"} changed without a '
                      f'{files_label} file-size change.']
    return '\n'.join(lines) + '\n', footnote


def render_pairs(pairs, matched, engine, base_only=(), cur_only=(), failures=(), boards=(), symbols=False,
                 labels=SIDE_LABELS, files_label='TinyUSB'):
    """Markdown report over paired elfs keyed by (board, elf path).

    Every statistic is over per-pair deltas, sized by `engine`. `matched` counts
    ids built on both sides, compared or not;
    `failures` are (elf id, side, stage, message), the elf path None for a
    board-level failure. Failures or unmatched elfs mark the report INCOMPLETE.
    `boards` lists the requested boards, so an unchanged or failed one is named.
    `symbols` adds each file's changed symbols to each pair's section table; `labels`
    name the base and current sides; `files_label` names the filtered files' totals.
    """
    file_deltas = {i: _file_deltas(b, c) for i, (b, c) in pairs.items()}
    changed = [i for i, (b, c) in pairs.items() if _pair_changed(b, c, symbols)]
    status = _status(failures, base_only, cur_only, pairs)
    all_label = ENGINES[engine].all_label
    lines = [f'**Coverage ({status}, {engine}):** {len(pairs)} of {matched} matched elf pairs '
             f'compared, {len(changed)} changed']
    if boards:
        lines.append('- boards: ' + ', '.join(f'`{b}`' for b in boards))
    lines += _problems(base_only, cur_only, failures) + ['']

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

    src = {i: _delta(_files_total(pairs[i][0]['files']), _files_total(pairs[i][1]['files'])) for i in changed}
    pair_rows = [[_label(i)] + [_fmt(d) for d in src[i] + _delta(pairs[i][0]['all'], pairs[i][1]['all'])]
                 for i in changed]
    lines.append(md_table(['Pair', f'{files_label} Flash Δ', f'{files_label} RAM Δ', f'{all_label} Flash Δ',
                           f'{all_label} RAM Δ'], pair_rows))

    stats = _file_stats(file_deltas)
    rows = _changed_paths(stats)
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


def compare_sides(base, cur, engine, failures=(), boards=(), scope=None, symbols=False, labels=SIDE_LABELS,
                  files_label='TinyUSB'):
    """Pair two sides and render them. Returns (md, failures, ok, data).

    `failures` comes back with a filter failure for `scope` added when no
    compared pair matched a file (wrong filters, or an engine output change
    broke its parsing); pass `scope=None` when each scope was already checked.
    `ok` is False on any failure or when nothing was compared. `data` is the
    report's raw paired sizes, for JSON, symbols included only when `symbols` is set.
    `files_label` is render_pairs()'.
    """
    pairs, base_only, cur_only = pair_elfs(base, cur)
    failures = list(failures)
    if scope and pairs and not any(b['files'] or c['files'] for b, c in pairs.values()):
        failures.append((scope, 'both', 'filter', _filter_failure(engine)))
    md = render_pairs(pairs, len(base.keys() & cur.keys()), engine, base_only, cur_only, failures, boards,
                      symbols, labels, files_label)
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
    return filter_arg(os.path.realpath(os.path.join(checkout_dir, 'src')) + os.sep)


def filter_arg(path):
    """A path filter in the form sizes are matched in: on Windows, past the drive with '/',
    as CMake names an out-of-tree object D_/a/.../src/x.c.obj and DWARF D:/a/..."""
    return os.path.splitdrive(path)[1].replace('\\', '/') if WINDOWS else path


verbose = False
TERMINATE_GRACE = 10  # s a timed-out command gets to exit on SIGTERM before SIGKILL
KILL_DRAIN = 1  # s to read what a killed command left in its pipes


def run(cmd, timeout=None):
    """Run a command. cmd must be a list (no shell=True). On `timeout`, SIGTERM the
    command, SIGKILL it if it outlives a grace period, and return a CompletedProcess
    with rc=124 instead of raising TimeoutExpired, so the caller can fall through to
    error reporting and worktree cleanup rather than crashing with a traceback. Any
    other exception, e.g. exit_on_termination()'s exit, stops the command the same way
    and is re-raised: Popen's exit would otherwise wait for it."""
    if not isinstance(cmd, list):
        raise TypeError('run() requires a list, got str — fix the caller')
    if verbose:
        print(f'  $ {" ".join(shlex.quote(str(c)) for c in cmd)}')
    # the command stays in our process group, so Ctrl-C or a hangup reaches it directly
    with subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                          encoding='utf-8', errors='replace') as proc:
        try:
            out, err = proc.communicate(timeout=timeout)
        except BaseException as stopped:
            if WINDOWS:  # terminate() and kill() stop only the command, not ninja's compilers
                with contextlib.suppress(OSError, subprocess.SubprocessError):
                    subprocess.run(['taskkill', '/PID', str(proc.pid), '/T', '/F'],
                                   capture_output=True, timeout=TERMINATE_GRACE)
            # SIGTERM lets ninja stop its jobs, which it runs in process groups of their own;
            # a descendant can outlive the SIGKILL holding the pipes open, so stop reading then
            for stop, wait in ((proc.terminate, TERMINATE_GRACE), (proc.kill, KILL_DRAIN)):
                stop()
                try:
                    out, err = proc.communicate(timeout=wait)
                    break
                except subprocess.TimeoutExpired as e:
                    out, err = ((b or b'').decode(errors='replace') for b in (e.stdout, e.stderr))
            if not isinstance(stopped, subprocess.TimeoutExpired):
                raise
            msg = f'Command timed out after {timeout}s: {" ".join(shlex.quote(str(c)) for c in cmd)}'
            return subprocess.CompletedProcess(cmd, 124, stdout=out, stderr=err + ('\n' if err else '') + msg)
    return subprocess.CompletedProcess(cmd, proc.returncode, stdout=out, stderr=err)


def exit_on_termination():
    """Exit on a SIGTERM or SIGHUP sent to this process alone as on Ctrl-C: through run()'s
    stop of its command and main()'s worktree removal, instead of orphaning the build."""
    for name in ('SIGTERM', 'SIGHUP'):  # Windows has no SIGHUP
        if hasattr(signal, name):
            signal.signal(getattr(signal, name), lambda sig, _frame: sys.exit(128 + sig))


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
            try:
                os.symlink(src, dst, target_is_directory=True)
            except OSError as e:
                if getattr(e, 'winerror', None) == 1314:  # ERROR_PRIVILEGE_NOT_HELD
                    sys.exit(f'Error: cannot symlink {rel} into the base worktree: enable Windows Developer '
                             'Mode, or use --base-source ci when CI stored a baseline')
                raise


def short_hash(checkout):
    """`checkout`'s short HEAD hash, `-dirty` when tracked files differ from it; None
    when git cannot tell. `--exclude='*'` keeps tag names out."""
    ret = run(['git', '-C', checkout, 'describe', '--always', '--dirty', '--exclude=*'])
    return ret.stdout.strip() or None if ret.returncode == 0 else None


def invalid_boards(boards):
    """Boards that are not a plain name: a board names a cmake-code-size/<board> dir that
    gets removed before the build can reject it, so `..` or a path must not pass."""
    return [b for b in boards if not re.fullmatch(r'[A-Za-z0-9_-]+', b)]


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
    diagnostic, else its last line that is not ninja's progress, `FAILED:` or stop line
    (its own `ninja: error:` stays), with paths relative to `src_dir`."""
    out = '\n'.join(stream for stream in (ret.stdout, ret.stderr) if stream)
    lines = [line.strip() for line in out.splitlines()
             if line.strip() and not _NINJA_PROGRESS.match(line)
             and not line.startswith(('ninja: build stopped', 'ninja: Entering directory', 'FAILED: '))]
    error = build_utils.first_error(out) or (lines[-1] if lines else 'no output')
    root = src_dir.rstrip(os.sep)
    for prefix in {root + os.sep, root.replace(os.sep, '/') + '/'}:  # Windows: CMake passes the compiler D:/a/...
        error = error.replace(prefix, '')
    return error


ESP_IDF_IMAGE = 'espressif/idf:tinyusb'  # .github/actions/setup_toolchain/espressif's tag
ESP_IDF_MISSING = (f'ESP-IDF: source $IDF_PATH/export.sh, or docker with {ESP_IDF_IMAGE} '
                   f'(docker tag espressif/idf:v5.5.3 {ESP_IDF_IMAGE}, as CI does)')


def _esp_examples(src_dir, board, example):
    """The examples tools/build.py builds for espressif `board` (`example` alone when given):
    those get_examples('espressif') lists that `src_dir` has, less skip_example's."""
    import build  # tools/build.py; its import has no side effects
    with contextlib.chdir(src_dir):  # build.py and build_utils read examples/ and hw/bsp from the cwd
        return [e for e in build.get_examples('espressif')
                if example in (None, e) and os.path.isdir(os.path.join('examples', e))
                and not build_utils.skip_example(e, board)]


def _link_hops(path):
    """The paths a symlink resolves through, its real path last."""
    hops = []
    while os.path.islink(path):
        path = os.path.normpath(os.path.join(os.path.dirname(path), os.readlink(path)))
        hops.append(path)
    return hops


def is_espressif(board, src_dir=TINYUSB_ROOT):
    return os.path.isdir(os.path.join(src_dir, 'hw', 'bsp', 'espressif', 'boards', board))


def _idf_image():
    return bool(shutil.which('docker')) and run(['docker', 'image', 'inspect', ESP_IDF_IMAGE]).returncode == 0


def esp_without_idf(boards):
    """The error for espressif `boards` no exported ESP-IDF or CI's image can build, else None."""
    esp = [b for b in boards if is_espressif(b)]
    if esp and WINDOWS:
        return f'{", ".join(esp)} need ESP-IDF, which code_size.py does not support on Windows'
    if esp and not (shutil.which('idf.py') or _idf_image()):
        return f'{", ".join(esp)} need {ESP_IDF_MISSING}'
    return None


def _idf_command(src_dir, build_dir, name):
    """The argv that runs idf.py on `src_dir` into `build_dir`: an exported ESP-IDF, else CI's
    image as this user in a container `name`, with both mounted at their own paths so the
    elfs' DWARF matches the filters, and each symlinked dependency's real dir at every path
    its link goes through (a base worktree's links point at the checkout's, which may point
    at the main checkout's). None when neither is available."""
    if shutil.which('idf.py'):
        return ['idf.py']
    if not _idf_image():
        return None
    # the image enables ccache; kept here it serves the next run (its default hash_dir keeps
    # one tree's DWARF paths out of the other's objects)
    cache = os.path.join(CODE_SIZE_DIR, '_ccache')
    os.makedirs(cache, exist_ok=True)
    mounts = {p: os.path.realpath(p) for p in map(os.path.abspath, (src_dir, build_dir, cache))}
    for dep in runpy.run_path(os.path.join(src_dir, 'tools', 'get_deps.py'))['deps_all']:
        path = os.path.join(src_dir, dep)
        mounts.update((hop, os.path.realpath(path)) for hop in _link_hops(path))
    cmd = ['docker', 'run', '--rm', '--name', name, '--user', f'{os.getuid()}:{os.getgid()}',
           '-e', 'HOME=/tmp', '-e', f'CCACHE_DIR={os.path.abspath(cache)}']
    for target, source in sorted(mounts.items()):
        cmd += ['-v', f'{source}:{target}']
    return cmd + [ESP_IDF_IMAGE, 'idf.py']


def _build_idf(src_dir, build_dir, board, example):
    """Build each ESP-IDF example project's app, as tools/build.py does (its bootloader is
    not sized); the first failure stops."""
    examples = _esp_examples(src_dir, board, example)
    if not examples:
        return subprocess.CompletedProcess([], 1, '', f'{board} builds no {example or "example"}')
    container = f'tinyusb-code-size-{os.getpid()}'
    idf = _idf_command(src_dir, build_dir, container)
    if idf is None:
        return subprocess.CompletedProcess([], 1, '', f'{board} needs {ESP_IDF_MISSING}')
    for ex in examples:
        ret = None
        try:
            ret = run(idf + ['-C', os.path.join(src_dir, 'examples', ex), '-B', os.path.join(build_dir, ex),
                             '-GNinja', f'-DBOARD={board}', 'app'], timeout=600)
        finally:
            # stopping the docker CLI leaves its container building; --rm only removes an exited one
            if idf[0] == 'docker' and (ret is None or ret.returncode == 124):
                run(['docker', 'rm', '-f', container])
        if ret.returncode != 0:
            break
    return ret


def build_board(src_dir, build_dir, board, example, label):
    """Configure and build examples for a board as a `label` progress phase, printing
    an excerpt of the output on failure. Returns None on success, else build_error().

    When `example` is given, only that target is built (`ninja -C DIR NAME`),
    keeping single-example workflows fast. An espressif board builds each example as
    its own ESP-IDF project.
    """
    phase = Phase(label)
    os.makedirs(build_dir, exist_ok=True)
    if is_espressif(board, src_dir):
        ret = _build_idf(src_dir, build_dir, board, example)
    else:
        ret = run(['cmake', '-B', build_dir, '-G', 'Ninja',
                   f'-DBOARD={board}', '-DCMAKE_BUILD_TYPE=MinSizeRel',
                   os.path.join(src_dir, 'examples')])
        if ret.returncode == 0:
            # ninja itself, not `cmake --build`: cmake does not pass a timeout's SIGTERM on
            cmd = ['ninja', '-C', build_dir]
            if example:
                cmd.append(os.path.basename(example))
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
    # <role>/<example>/*.elf: deeper elfs are helpers, e.g. pico-sdk's bs2_default.elf, and
    # cmake's _deps/ holds fetched tools, e.g. picotool's enc_bootloader.elf
    elfs = sorted(e for e in glob.glob(f'{root}/{example or "*/*"}/*.elf')
                  if not os.path.relpath(e, build_dir).startswith('_deps' + os.sep))
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
        rel = os.path.relpath(elf, build_dir).replace(os.sep, '/')  # one elf id on every host
        sizes[rel] = elf_sizes
        if error:
            errors.append((rel, error))
    return sizes, errors


def diff_summary(data, symbols, files_label='TinyUSB'):
    """Console lines of one diff from its JSON-shaped `data`: coverage, the single
    pair's filtered Δ, unmatched elfs and failures."""
    pairs = {(p['board'], p['elf']): (p['base'], p['current']) for p in data['pairs']}
    changed = sum(_pair_changed(b, c, symbols) for b, c in pairs.values())
    line = f'{len(pairs)} pair{"" if len(pairs) == 1 else "s"}, {changed} changed'
    if len(pairs) == 1:
        (b, c), = pairs.values()
        df, dr = _delta(_files_total(b['files']), _files_total(c['files']))
        line += f'; {files_label} Flash Δ {_fmt(df)}, RAM Δ {_fmt(dr)}'
    lines = [line if data['status'] == 'complete' else f'INCOMPLETE: {line}']
    for key in ('base_only', 'current_only'):
        if data[key]:
            lines.append(f'{key.replace("_", "-")}: ' + ', '.join(_label((r['board'], r['elf'])) for r in data[key]))
    lines += [f'FAILED {_label((f["board"], f["elf"]))} {f["side"]} {f["stage"]}: {f["message"]}'
              for f in data['failures']]
    return lines


def report_summary(sizes, failures, ok, files_label='TinyUSB'):
    """Console lines of one report: coverage, the single elf's filtered total, failures."""
    sized = [s for s in sizes.values() if s is not None]
    line = f'{len(sized)} of {len(sizes)} elfs sized'
    if len(sized) == 1:
        src = _files_total(sized[0]['files'])
        line += f'; {files_label} Flash {src["flash"]}, RAM {src["ram"]}'
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
    try:
        rel = os.path.relpath(path)
    except ValueError:  # on another Windows drive
        return path
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
    files_label = 'filtered' if args.filter else 'TinyUSB'
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
            md = render_report(sizes, args.engine, failures, [board], args.symbols, files_label)
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
            print_result(report_summary(sizes, failures, ok, files_label), f'{example}: ' if scope else '',
                         tables)
            write_report(report_path(board, example, 'report'), md, data)
    return 1 if failed else 0


SNAPSHOT_SCHEMA = 1
_SHA_RE = re.compile(r'[0-9a-f]{40}')


def _cmake_compiler(build_dir):
    """{'id', 'version', 'name', 'build_type'} of a configured build dir, from CMake's own
    compiler detection (not the host gcc), '' where CMake recorded none."""
    info = {'id': '', 'version': '', 'name': '', 'build_type': ''}
    found = glob.glob(os.path.join(glob.escape(build_dir), 'CMakeFiles', '*', 'CMakeCCompiler.cmake'))
    if not found:  # ESP-IDF: one project per <role>/<example>, all on the board's one toolchain
        projects = sorted(glob.glob(os.path.join(glob.escape(build_dir), '*', '*', 'CMakeFiles')))
        return _cmake_compiler(os.path.dirname(projects[0])) if projects else info
    for path in found:
        with open(path) as f:
            text = f.read()
        for key, var in (('id', 'CMAKE_C_COMPILER_ID'), ('version', 'CMAKE_C_COMPILER_VERSION'),
                         ('name', 'CMAKE_C_COMPILER')):
            m = re.search(rf'^set\({var} "([^"]*)"\)', text, re.M)
            if m:
                info[key] = os.path.basename(m.group(1)) if key == 'name' else m.group(1)
    cache = os.path.join(build_dir, 'CMakeCache.txt')
    if os.path.isfile(cache):
        with open(cache) as f:
            m = re.search(r'^CMAKE_BUILD_TYPE:\w+=(.*)$', f.read(), re.M)
        info['build_type'] = m.group(1) if m else ''
    return info


def _git_shas(event):
    """(sha, base_sha, head_sha) of the checkout: a pull_request build checks out GitHub's
    merge commit, whose first parent is the base branch tip it was built against and
    second parent the PR head; any other build is its own base and head."""
    ret = run(['git', '-C', TINYUSB_ROOT, 'rev-parse', 'HEAD', 'HEAD^1', 'HEAD^2']
              if event == 'pull_request' else ['git', '-C', TINYUSB_ROOT, 'rev-parse', 'HEAD'])
    shas = ret.stdout.split() if ret.returncode == 0 else []
    if not shas or not all(_SHA_RE.fullmatch(s) for s in shas):
        raise RuntimeError(f'cannot resolve the checkout commits for a {event} build: {ret.stderr.strip()}')
    return (shas[0], shas[1], shas[2]) if len(shas) == 3 else (shas[0], shas[0], shas[0])


def _membrowse_version():
    """The installed membrowse's version, '' when it cannot be read."""
    try:
        from importlib.metadata import version
        return version('membrowse')
    except Exception:
        return ''


def snapshot_boards(families, boards, examples, defines=()):
    """The CI-pinned boards a build leg produced, as the Membrowse upload step picks them:
    each family's pinned boards that build one of `examples` (build.py's
    resolve_ci_boards, boards-only), plus each `-b` board that is pinned. `defines` are
    the leg's -D tokens, which can enable examples (MAX3421_HOST=1)."""
    import build  # tools/build.py; its import has no side effects
    pinned = ci_pinned_boards()
    # build.py skips a -b board that builds none of the examples, before configuring it
    picked = [b for b in boards if b in pinned and build.builds_any(b, examples, defines)]
    for family in families:
        picked += build.resolve_ci_boards(CI_PINNED_BOARDS, family, True, examples, extra_defines=defines)
    return list(dict.fromkeys(picked))


def board_snapshot(board, build_dir, examples, filters, defines=()):
    """(elfs, failures) of one board's build dir: {elf path: sizes} of every sized elf,
    and [{'elf', 'stage', 'message'}]. A missing build dir, an example of the scope the
    board builds but has no elf for, or an elf that failed to size is a failure, never a
    zero; a board whose elfs all match no TinyUSB file fails the filter check."""
    if not os.path.isdir(build_dir):
        return {}, [{'elf': None, 'stage': 'build', 'message': f'no build dir {_shown(build_dir)}'}]
    elfs, failures = {}, []
    scopes = [e for e in dict.fromkeys(examples)
              if not build_utils.skip_example(e, board, defines)] if examples else [None]
    for example in scopes:
        sizes, errors = generate_sizes(build_dir, filters, example)
        for rel, s in sizes.items():
            if s is not None and not _valid_sizes(s):
                errors.append((rel, 'sizes are not integer byte counts of files, sections and symbols'))
            elif s is not None:
                elfs[rel] = s
        failures += [{'elf': rel, 'stage': 'build' if rel is None else 'report',
                      'message': f'{example}: {msg}' if example and rel is None else msg}
                     for rel, msg in errors]
    if elfs and not any(s['files'] for s in elfs.values()):
        failures.append({'elf': None, 'stage': 'filter', 'message': _filter_failure('membrowse')})
    return elfs, failures


def run_snapshot(args):
    """`snapshot`: size the CI-pinned boards a build leg already built, one
    code-size-<board>.json each, for `compare` against another run's snapshots."""
    os.chdir(TINYUSB_ROOT)  # build_utils and build.py resolve hw/bsp and examples from the cwd
    sha, base_sha, head_sha = _git_shas(args.event)
    membrowse_version = _membrowse_version()
    filters = args.filter or [tinyusb_src_filter(TINYUSB_ROOT)]
    defines = tuple(args.define_symbol)
    boards = snapshot_boards(args.families, args.board, args.example, defines)
    # a variant leg (--build-name) reports its one board under the build name
    names = {args.build_name or b: b for b in boards}
    os.makedirs(args.output, exist_ok=True)
    # written even with no board: `compare` tells a leg that ran from one that never did
    with open(os.path.join(args.output, 'leg.json'), 'w') as f:
        json.dump({'schema': SNAPSHOT_SCHEMA, 'examples': args.example, 'boards': list(names),
                   'build_outcome': args.build_outcome, 'sha': sha, 'base_sha': base_sha, 'head_sha': head_sha},
                  f, sort_keys=True)
    if not boards:
        print('snapshot: no CI-pinned board in this leg')
        return 0
    failed = False
    for board, base_board in names.items():
        build_dir = os.path.join(args.build_root, f'cmake-build-{board}')
        elfs, failures = board_snapshot(base_board, build_dir, args.example, filters, defines)
        # the leg's commits, examples and build outcome are in its leg.json
        data = {'schema': SNAPSHOT_SCHEMA, 'board': board, 'engine': 'membrowse',
                'membrowse_version': membrowse_version, 'compiler': _cmake_compiler(build_dir),
                'elfs': {rel: _json_sizes(s, args.symbols) for rel, s in sorted(elfs.items())},
                'failures': failures}
        path = os.path.join(args.output, f'code-size-{board}.json')
        with open(path, 'w') as f:
            json.dump(data, f, sort_keys=True, separators=(',', ':'))
        failed |= bool(failures)
        print(f'  {board}: {len(elfs)} elfs sized, {len(failures)} failures -> {_shown(path)}')
        for fail in failures:
            print(f'    FAILED {fail["elf"] or board} {fail["stage"]}: {fail["message"]}')
    return 1 if failed else 0


_NAME_RE = re.compile(r'[A-Za-z0-9_.+/-]+')
COMMENT_LIMIT = 60000  # GitHub caps a comment at 65536 characters


def _clean(text, limit=300):
    """Snapshot text for a report: a snapshot is PR-built data, so nothing in it may open
    Markdown, HTML or an @mention."""
    return re.sub(r'[^A-Za-z0-9_.,:;=()/+ -]', '?', str(text))[:limit]


def _is_count(value):
    return isinstance(value, int) and not isinstance(value, bool)


def _region(value):
    return isinstance(value, dict) and set(value) == {'flash', 'ram'} and all(map(_is_count, value.values()))


def _int_map(value, depth):
    """Whether `value` is `depth` levels of str-keyed dicts over byte counts."""
    return isinstance(value, dict) and all(
        isinstance(k, str) and (_int_map(v, depth - 1) if depth > 1 else _is_count(v)) for k, v in value.items())


def _valid_sizes(s):
    return (isinstance(s, dict) and {'files', 'all', 'sections'} <= set(s) <= {'files', 'all', 'sections', 'symbols'}
            and _region(s['all']) and isinstance(s['files'], dict)
            and all(isinstance(k, str) and _region(v) for k, v in s['files'].items())
            and _int_map(s['sections'], 2) and ('symbols' not in s or _int_map(s['symbols'], 3)))


def _is_name(value):
    return isinstance(value, str) and bool(_NAME_RE.fullmatch(value)) and '..' not in value.split('/')


def _names(value, none_ok=False):
    return (none_ok and value is None) or (isinstance(value, list) and all(map(_is_name, value)))


def _commits(data):
    """(sha, base_sha, head_sha), or None unless all are 40-hex."""
    shas = tuple(data.get(k) for k in ('sha', 'base_sha', 'head_sha'))
    return shas if all(isinstance(s, str) and _SHA_RE.fullmatch(s) for s in shas) else None


def _scope_error(data):
    legs, fam = data.get('legs'), data.get('family_examples', {})
    if not isinstance(data.get('code_changed'), bool):
        return 'code_changed is not a boolean'
    if not (isinstance(legs, list) and all(isinstance(leg, dict) and isinstance(leg.get('toolchain'), str)
                                           and isinstance(leg.get('arg'), str) for leg in legs)):
        return 'legs are not {toolchain, arg} strings'
    if not (isinstance(fam, dict) and all(map(_names, fam.values()))):
        return 'family_examples is not a map of example lists'
    return None


def _leg_error(data):
    if not isinstance(data.get('build_outcome'), str):
        return 'build_outcome is not a string'
    if not _names(data.get('boards')):
        return 'boards is not a list of board names'
    if 'examples' not in data or not _names(data['examples'], none_ok=True):
        return 'examples is not a list of example names'
    return None if _commits(data) else 'commit hashes are not 40-hex'


def _shard_error(data, filename):
    board = data.get('board')
    if not (_is_name(board) and filename == f'code-size-{board}.json'):
        return 'board name does not match the file'
    if data.get('engine') != 'membrowse':
        return 'unsupported engine'
    elfs, failures = data.get('elfs'), data.get('failures')
    if not isinstance(elfs, dict) or not isinstance(failures, list):
        return 'no elfs/failures'
    for elf, sizes in elfs.items():
        if not _is_name(elf):
            return 'invalid elf path'
        if not _valid_sizes(sizes):
            return f'{elf}: sizes are not integer byte counts'
    if not all(isinstance(f, dict) and 'elf' in f and (f['elf'] is None or _is_name(f['elf']))
               and isinstance(f.get('stage'), str) and isinstance(f.get('message'), str) for f in failures):
        return 'failures are not {elf, stage, message} records'
    return None


def load_snapshots(root):
    """Every snapshot file under `root` (one directory per downloaded artifact):
    {'scope': scope.json or None, 'legs': {artifact dir: leg.json}, 'shards': {board: shard},
    'errors': [(file, why)]}. A shard records its artifact dir as '_leg'. An unusable file
    is an error, never guessed at."""
    run = {'scope': None, 'legs': {}, 'shards': {}, 'errors': []}
    for dirpath, _dirs, files in sorted(os.walk(root)):
        artifact = os.path.basename(dirpath)
        for name in sorted(f for f in files if f.endswith('.json')):
            rel = os.path.relpath(os.path.join(dirpath, name), root)
            try:
                with open(os.path.join(dirpath, name)) as f:
                    data = json.load(f)
            except (OSError, ValueError):
                run['errors'].append((rel, 'unreadable JSON'))
                continue
            if not isinstance(data, dict) or data.get('schema') != SNAPSHOT_SCHEMA:
                why = 'unsupported snapshot schema'
            elif name == 'scope.json':
                why = _scope_error(data)
                run['scope'] = None if why else data
            elif name == 'leg.json':
                why = _leg_error(data)
                if not why:
                    run['legs'][artifact] = data
            elif name.startswith('code-size-'):
                why = _shard_error(data, name)
                if not why and data['board'] in run['shards']:
                    why = f'second snapshot of {data["board"]}'
                if not why:
                    run['shards'][data['board']] = {**data, '_leg': artifact}
            else:
                why = 'not a snapshot file'
            if why:
                run['errors'].append((rel, why))
    return run


def _leg_examples(scope, arg):
    """The examples build_util.yml builds for a scope leg: its arg's own -e plus the scope's
    family map entry for that arg, None for all."""
    return sorted(set(re.findall(r' -e ([^ ]+)', arg) + scope.get('family_examples', {}).get(arg, []))) or None


def _in_scope(elf, examples):
    """Whether an elf path (<role>/<example>/<name>.elf) is one of `examples`, all when None."""
    return examples is None or '/'.join(elf.split('/')[:2]) in examples


def _usable_shards(run, side, failures, boards=None):
    """`run`'s shards listed in the boards of their own leg, whose build succeeded, each given
    its leg's examples as '_examples'. Returns ({board: shard}, {leg commits}); every
    rejection is added to `failures`, only of `boards` when given."""
    shards, commits = {}, {_commits(leg) for leg in run['legs'].values()}
    for rel, why in run['errors']:
        board = re.fullmatch(r'code-size-(.+)\.json', os.path.basename(rel))
        if boards is None or not board or board.group(1) in boards:
            failures.append(((_clean(rel), None), side, 'snapshot', _clean(why)))
    for board, shard in sorted(run['shards'].items()):
        if boards is not None and board not in boards:
            continue
        leg = run['legs'].get(shard['_leg'])
        if leg is None or board not in leg['boards']:
            failures.append(((board, None), side, 'snapshot', 'not a board of its leg\'s build'))
        elif leg['build_outcome'] != 'success':
            failures.append(((board, None), side, 'build', f'the build step ended {_clean(leg["build_outcome"])}'))
        else:
            shards[board] = {**shard, '_examples': leg['examples']}
    for _artifact, leg in sorted(run['legs'].items()):
        failures += [((board, None), side, 'snapshot', 'the leg left no snapshot of it') for board in leg['boards']
                     if board not in run['shards'] and (boards is None or board in boards)]
    return shards, commits


def _scope_failures(scope, legs):
    """Legs of the scope that left no leg.json, or measured other examples than it selected,
    and legs that are not of it."""
    failures, expected = [], set()
    for leg in scope['legs']:
        tag = re.sub(r' -e [^ ]+', '', leg['arg'])  # build_util.yml's ARTIFACT_TAG
        artifact = f'code-size-{leg["toolchain"]}-{tag}'
        expected.add(artifact)
        if artifact not in legs:
            failures.append(((_clean(artifact), None), 'current', 'snapshot', 'the build leg left no snapshot'))
        elif (legs[artifact]['examples'] is not None  # all examples covers any selection
              and sorted(set(legs[artifact]['examples'])) != _leg_examples(scope, leg['arg'])):
            failures.append(((_clean(artifact), None), 'current', 'scope', 'measured other examples than selected'))
    failures += [((_clean(a), None), 'current', 'scope', 'not a leg of this run') for a in sorted(set(legs) - expected)]
    return failures


def _base_commit_failure(commits, baseline):
    """The failure when a baseline run's snapshots are of several commits, or not of the
    selected `baseline` ({'sha'}, or None for any), else None."""
    shas = {c[0] for c in commits}
    if len(shas) > 1 or (baseline and shas and shas != {baseline.get('sha')}):
        return (('commits', None), 'base', 'snapshot', 'the snapshots are not of the selected baseline commit')
    return None


def _pair_base_shard(board, base, cur_elfs, examples):
    """One board's current elfs ({elf: sizes}) against its baseline shard, within
    `examples` (None for all): (base sizes, base failures, current sizes, baseline elfs
    outside `examples`). A current elf whose baseline failed is left out: that failure is
    the baseline's, never a new elf here."""
    sizes, failures, failed, outside = {}, [], set(), 0
    for f in base['failures']:
        if f['elf'] is None or _in_scope(f['elf'], examples):
            failures.append(((board, f['elf']), 'base', _clean(f['stage']), _clean(f['message'])))
            failed.add(f['elf'])
    for elf, s in base['elfs'].items():
        if _in_scope(elf, examples):
            sizes[(board, elf)] = s
        else:
            outside += 1
    return sizes, failures, {(board, elf): s for elf, s in cur_elfs.items() if elf not in failed}, outside


def compare_runs(base, cur, baseline=None, symbols=False):
    """Compare two load_snapshots() runs, `cur` defining the scope. Returns
    (full Markdown, comment Markdown, JSON data). `baseline` is code_size_ci.py's
    description of the base run ({'sha', 'url', 'exact', 'note'}), or None. Anything that
    leaves coverage short is a failure; a comparable difference is a note."""
    notes, failures = [], []
    scope = cur['scope']
    if scope is None:
        failures.append((('scope', None), 'current', 'snapshot', 'no usable scope manifest'))
    elif not (scope['legs'] or cur['legs'] or cur['shards'] or cur['errors']):
        md = 'Nothing to measure: no code change, or no CI-pinned build leg selected.\n'
        return md, f'## Code size\n\n{md}', {'status': 'nothing measured'}
    else:
        failures += _scope_failures(scope, cur['legs'])
    cur_shards, cur_commits = _usable_shards(cur, 'current', failures)
    base_shards, base_commits = _usable_shards(base, 'base', failures, set(cur_shards))
    labels = SIDE_LABELS
    if len(cur_commits) > 1:
        failures.append((('commits', None), 'current', 'snapshot', 'the snapshots are of different commits'))
        cur_shards = {}
    elif cur_commits:
        (_sha, base_sha, head_sha), = cur_commits
        labels = (base_sha[:10], head_sha[:10])
    if commit_failure := _base_commit_failure(base_commits, baseline):
        failures.append(commit_failure)
        base_shards = {}

    sides, no_baseline, outside = {'base': {}, 'current': {}}, [], 0
    for board, shard in sorted(cur_shards.items()):
        failures += [((board, f['elf']), 'current', _clean(f['stage']), _clean(f['message'])) for f in shard['failures']]
        b = base_shards.get(board)
        if b is None:
            no_baseline.append(board)
            failures.append(((board, None), 'base', 'snapshot', 'no baseline snapshot'))
            continue
        for key in ('membrowse_version', 'compiler'):
            if b.get(key) != shard.get(key):
                notes.append(f'{board}: {key} differs from the baseline ({_clean(b.get(key))} -> '
                             f'{_clean(shard.get(key))}): comparable, not exact')
        base_sizes, base_failures, cur_sizes, out = _pair_base_shard(board, b, shard['elfs'], shard['_examples'])
        sides['base'].update(base_sizes)
        sides['current'].update(cur_sizes)
        failures += base_failures
        outside += out
    if outside:
        notes.append(f'{outside} baseline elfs are of examples this PR did not build (outside coverage)')

    if symbols and not all('symbols' in s for side in sides.values() for s in side.values()):
        symbols = False
        notes.append('symbols omitted: some snapshots were taken without them')
    head = []
    if baseline:
        kind = 'exact' if baseline.get('exact') else 'approximate'
        head.append((f'Baseline: {_clean(baseline["sha"][:10])} ({kind}) {_clean(baseline.get("url"), 200)}'
                     if baseline.get('sha') else 'Baseline: unavailable')
                    + (f' - {_clean(baseline["note"])}' if baseline.get('note') else ''))
    head += [f'- {n}' for n in notes]
    boards = sorted(cur_shards)
    full, _fails, _ok, data = compare_sides(sides['base'], sides['current'], 'membrowse', failures, boards,
                                            symbols=symbols, labels=labels)
    pairs, base_only, cur_only = pair_elfs(sides['base'], sides['current'])
    body, footnote = render_comment(pairs, 'membrowse', base_only, cur_only, failures, symbols)
    preface = '\n'.join(head) + ('\n\n' if head else '')
    comment = f'## Code size\n\n{preface}{body}'
    truncated = '\n\n_Truncated: see the full report._\n'
    if len(comment) + len(footnote) > COMMENT_LIMIT:
        comment = comment[:COMMENT_LIMIT - len(footnote) - len(truncated)].rsplit('\n', 1)[0] + truncated
    comment += footnote
    data.update({'notes': notes, 'baseline': baseline, 'no_baseline': no_baseline, 'outside': outside})
    return preface + full, comment, data


def run_compare(args):
    """`compare`: compare a run's snapshots against a baseline run's, no build."""
    baseline = None
    if args.baseline_info:
        with open(args.baseline_info) as f:
            baseline = json.load(f)
    base = load_snapshots(args.base)  # os.walk of a missing directory yields nothing
    full, comment, data = compare_runs(base, load_snapshots(args.current), baseline, args.symbols)
    os.makedirs(args.output, exist_ok=True)
    write_report(os.path.join(args.output, 'code-size'), full, data)
    with open(os.path.join(args.output, 'comment.md'), 'w') as f:
        f.write(comment)
    print(f'  comment: {_shown(os.path.join(args.output, "comment.md"))}')
    return 0

BASELINE_REPO = 'hathach/tinyusb'
DOWNLOAD_TIMEOUT = 600  # seconds for one run's code-size artifacts


class BaselineUnavailable(Exception):
    """The CI base cannot be had: no gh, no auth, no baseline run, a base off master, or a
    failed transfer. Never a problem inside downloaded snapshots: those are failures."""


def _download_baseline(ci, repo, run_id):
    """The snapshot dir of run `run_id`'s code-size-* artifacts, downloaded once per run
    attempt into the cache and published whole; a re-run during the download is retried once."""
    attempt = ci.gh(f'repos/{repo}/actions/runs/{run_id}')['run_attempt']
    for _ in range(2):
        dest = os.path.join(BASELINE_CACHE_DIR, repo.replace('/', '_'), f'{run_id}-{attempt}')
        if os.path.isfile(os.path.join(dest, 'manifest.json')):
            return os.path.join(dest, 'snapshots')
        tmp = f'{dest}.tmp-{os.getpid()}'
        shutil.rmtree(tmp, ignore_errors=True)
        try:
            ret = run(['gh', 'run', 'download', str(run_id), '-R', repo, '-p', 'code-size-*',
                       '-D', os.path.join(tmp, 'snapshots')], timeout=DOWNLOAD_TIMEOUT)
            if ret.returncode != 0:
                raise BaselineUnavailable(f'gh run download {run_id}: {_clean(ret.stderr) or f"exit {ret.returncode}"}')
            now = ci.gh(f'repos/{repo}/actions/runs/{run_id}')['run_attempt']
            if now != attempt:  # a re-run replaced artifacts mid-download: this copy is of neither attempt
                attempt = now
                continue
            try:
                with open(os.path.join(tmp, 'manifest.json'), 'w') as f:
                    json.dump({'repo': repo, 'run_id': run_id, 'run_attempt': attempt}, f, indent=1, sort_keys=True)
                os.rename(tmp, dest)
            except OSError as e:
                if not os.path.isfile(os.path.join(dest, 'manifest.json')):  # else another run published it first
                    raise BaselineUnavailable(f'caching run {run_id}: {e}') from e
            return os.path.join(dest, 'snapshots')
        finally:
            shutil.rmtree(tmp, ignore_errors=True)  # gone after a successful rename
    raise BaselineUnavailable(f'run {run_id} was re-run during the download')


def ci_baseline(repo, sha):
    """(snapshot dir, baseline info) of `repo`'s stored master snapshots for `sha`, else
    its nearest first-parent ancestor's (approximate), as code_size_ci.py picks a PR's.
    Raises BaselineUnavailable."""
    if not shutil.which('gh'):
        raise BaselineUnavailable('gh not found')
    sys.modules.setdefault('code_size', sys.modules[__name__])  # its `import code_size` is this module, not a copy
    ci = _load_module('code_size_ci', os.path.join(TINYUSB_ROOT, '.github', 'scripts', 'code_size_ci.py'))
    try:
        info = ci.lookup_sha(repo, sha, 'yours')
        if info['sha'] is None:
            raise BaselineUnavailable(info['note'])
        return _download_baseline(ci, repo, info['run_id']), info
    except (RuntimeError, KeyError, TypeError, ValueError) as e:  # gh auth, transport or an unexpected reply
        raise BaselineUnavailable(f'the baseline lookup failed: {_clean(str(e))}') from e
    except OSError as e:  # gh could not be started, e.g. EMFILE
        raise BaselineUnavailable(f'running gh failed: {_clean(e.strerror or e)}') from e


def resolve_base(base_source, unsupported, repo, sha):
    """A diff's base as (source, reason, baseline info, snapshots): the CI snapshots of
    `sha`, unless `base_source` is 'local' or `unsupported` options need a local base. With
    no `base_source`, an unavailable CI base falls back to a local build and `reason` says
    why; with 'ci' it raises BaselineUnavailable."""
    if base_source == 'local':
        return 'local', None, None, None
    if unsupported:
        return 'local', f'{", ".join(unsupported)} needs a local base', None, None
    print(f'CI base: looking up {repo} snapshots of {sha[:10]}…', flush=True)
    try:
        snapshot_dir, baseline = ci_baseline(repo, sha)
    except BaselineUnavailable as e:
        if base_source == 'ci':
            raise
        return 'local', str(e), None, None
    return 'ci', None, baseline, load_snapshots(snapshot_dir)


def baseline_shards(snapshots, baseline, boards):
    """The base side of a diff from a downloaded run (load_snapshots()): ({board: shard},
    failures). Only shards of `boards` from a successful leg of the
    `baseline` commit count; anything else is a base failure, never a zero, and a failure
    of the run itself (an unreadable file, other commits) is keyed by what failed, not a
    board. Shards of other boards, e.g. -DMA variants, are excluded, never substituted."""
    failures = []
    shards, commits = _usable_shards(snapshots, 'base', failures, set(boards))
    if commit_failure := _base_commit_failure(commits, baseline):
        failures.append(commit_failure)
        shards = {}
    failures += [((board, None), 'base', 'snapshot', 'no baseline snapshot') for board in boards
                 if board not in shards and not any(f[0][0] == board for f in failures)]
    return shards, failures


def _commit_subject(sha):
    """`sha`'s subject from the local history, '' when it is not there (not fetched)."""
    ret = run(['git', '-C', TINYUSB_ROOT, 'log', '-1', '--format=%s', sha])
    return ret.stdout.strip() if ret.returncode == 0 else ''


def metadata_warnings(board, shard, compiler, membrowse_version):
    """A board's warnings when its CI base was measured unlike its current build: the
    board is still compared, the warning says what its deltas may include."""
    base_c = shard.get('compiler') if isinstance(shard.get('compiler'), dict) else {}
    warnings = []
    unknown = [side for side, c in (('base', base_c), ('current', compiler))
               if not all(c.get(k) for k in ('id', 'name', 'version'))]
    if unknown:
        warnings.append(f'{board}: compiler unknown on the {" and ".join(unknown)} side: '
                        'comparability cannot be established')
    elif any(base_c.get(k) != compiler.get(k) for k in ('id', 'name', 'version', 'build_type')):
        def show(c):
            return ' '.join(_clean(c[k]) for k in ('id', 'name', 'version', 'build_type') if c.get(k))
        warnings.append(f'{board}: base built with {show(base_c)}, current with {show(compiler)}: '
                        'deltas may include the toolchain change')
    base_m = shard.get('membrowse_version')
    unknown = [side for side, v in (('base', base_m), ('current', membrowse_version)) if not v]
    if unknown:
        warnings.append(f'{board}: membrowse version unknown on the {" and ".join(unknown)} side: '
                        'comparability cannot be established')
    elif base_m != membrowse_version:
        warnings.append(f'{board}: base measured with membrowse {_clean(base_m)}, current with '
                        f'{_clean(membrowse_version)}: per-file attribution may differ')
    return warnings


def example_arg(value):
    example = value.rstrip('/')
    if not example:
        raise argparse.ArgumentTypeError(f'{value!r} names no example')
    return example


def main():
    global verbose

    common = argparse.ArgumentParser(add_help=False)
    common.add_argument('-b', '--board', action='append', default=[],
                        help='Board name (repeatable). Required unless diff --ci is given.')
    common.add_argument('-f', '--filter', action='append', default=None, type=filter_arg,
                        help='Path-substring filter (repeatable). When given, '
                             'overrides the default and is applied to every build. '
                             'Default: each build\'s own absolute <checkout>/src/ path, '
                             'which uniquely matches TinyUSB stack code without colliding '
                             'with vendored deps.')
    common.add_argument('-e', '--example', action='append', default=None, type=example_arg,
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
    parser.add_argument('--base-source', choices=('ci', 'local'), default=None,
                        help='Where the base sizes come from. Default: try the stored CI snapshots of '
                             '--base-branch, fall back to a local build; ci: require CI; local: build '
                             'the base, no GitHub lookup.')
    parser.add_argument('--baseline-repo', default=BASELINE_REPO,
                        help=f'Repository whose master snapshots are the CI base (default: {BASELINE_REPO})')
    snap = sub.add_parser('snapshot', help='size the CI-pinned boards a CI build leg already built, '
                                           'one code-size-<board>.json each (no build)')
    snap.add_argument('families', nargs='*', default=[], help='Families the leg built (as tools/build.py)')
    snap.add_argument('-b', '--board', action='append', default=[], help='Board the leg built (repeatable)')
    snap.add_argument('-e', '--example', action='append', default=None, type=example_arg,
                      help='Examples the leg built (repeatable); omit when it built all')
    snap.add_argument('--build-name', help='A variant leg\'s build name (as tools/build.py): its build dir, and '
                                          'the board it reports as')
    snap.add_argument('-D', '--define-symbol', action='append', default=[],
                      help='A variant leg\'s build-system define (as tools/build.py); it needs --build-name')
    snap.add_argument('--cflag', action='append', default=[],
                      help='A variant leg\'s compiler flag (as tools/build.py); it needs --build-name')
    snap.add_argument('-o', '--output', required=True, help='Directory for the code-size-<board>.json files')
    snap.add_argument('--build-root', default=os.path.join(TINYUSB_ROOT, 'cmake-build'),
                      help='Where tools/build.py put cmake-build-<board> (default: cmake-build)')
    snap.add_argument('--event', default=os.environ.get('GITHUB_EVENT_NAME', 'push'),
                      help='CI event; pull_request reads base/head from the merge commit '
                           '(default: $GITHUB_EVENT_NAME, else push)')
    snap.add_argument('--build-outcome', default='success', help='The Build step\'s outcome, recorded as is')
    snap.add_argument('-f', '--filter', action='append', default=None, type=filter_arg,
                      help='Path-substring filter (repeatable); default: this checkout\'s <checkout>/src/')
    snap.add_argument('--symbols', action='store_true', help='Include each file\'s symbols')
    snap.add_argument('-v', '--verbose', action='store_true', help='Print commands')
    cmp = sub.add_parser('compare', help='compare a CI run\'s snapshots against a baseline run\'s '
                                         '(code-size.md, code-size.json, comment.md; no build)')
    cmp.add_argument('current', help='The run\'s downloaded artifacts, one directory each')
    cmp.add_argument('base', help='The baseline run\'s downloaded artifacts; a missing directory is no baseline')
    cmp.add_argument('-o', '--output', required=True, help='Directory for the reports')
    cmp.add_argument('--baseline-info', help='JSON describing the baseline run: {sha, url, exact, note}')
    cmp.add_argument('--symbols', action='store_true', help='Show each changed file\'s symbols in the full report')
    args = top.parse_args()
    verbose = getattr(args, 'verbose', False)

    if args.command == 'compare':
        return run_compare(args)

    if args.command == 'snapshot':
        # a leg whose build did not succeed may have skipped the membrowse install too;
        # its snapshot still records that outcome, and any elf it left fails to size
        if engine_missing('membrowse') and args.build_outcome == 'success':
            snap.error(f'membrowse not found - install {ENGINES["membrowse"].install}')
        if invalid := invalid_boards(args.board + ([args.build_name] if args.build_name else [])):
            snap.error(f'invalid board name: {", ".join(invalid)}')
        if args.build_name and (args.families or len(args.board) != 1):
            snap.error('--build-name names exactly one -b board and no families')
        if (args.define_symbol or args.cflag) and not args.build_name:
            snap.error('-D or --cflag makes a variant, which needs --build-name to report under')
        return run_snapshot(args)

    if engine_missing(args.engine):
        sub.choices[args.command].error(f'{args.engine} not found - install {ENGINES[args.engine].install}, '
                                        f'or pick another --engine')

    if args.command == 'report':
        if not args.board:
            report_parser.error('at least one -b BOARD is required')
        if invalid := invalid_boards(args.board):
            report_parser.error(f'invalid board name: {", ".join(invalid)}')
        if error := esp_without_idf(args.board):
            report_parser.error(error)
        return run_report(args)

    if args.bloaty and not args.example:
        parser.error('--bloaty requires -e/--example')
    if args.bloaty and not shutil.which('bloaty'):
        parser.error('--bloaty requires bloaty on PATH')

    if args.ci:
        args.combined = True
        args.board = list(dict.fromkeys(args.board + ci_pinned_boards()))

    if not args.board:
        parser.error('at least one -b BOARD is required (or pass --ci)')
    if invalid := invalid_boards(args.board):
        parser.error(f'invalid board name: {", ".join(invalid)}')
    # before any board builds: a --ci run would otherwise fail its espressif boards last
    if error := esp_without_idf(args.board):
        parser.error(error)

    # CI snapshots hold membrowse sizes under the TinyUSB filter, and no elf
    unsupported = [opt for opt, used in (('-f', args.filter), ('--bloaty', args.bloaty),
                                         (f'--engine {args.engine}', args.engine != 'membrowse')) if used]
    if args.base_source == 'ci' and unsupported:
        parser.error(f'--base-source ci cannot be used with {", ".join(unsupported)}')

    worktree_dir = os.path.join(CODE_SIZE_DIR, '_worktree')
    files_label = 'filtered' if args.filter else 'TinyUSB'

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
    ret = run(['git', '-C', TINYUSB_ROOT, 'rev-parse', '--verify', '--quiet', f'{args.base_branch}^{{commit}}'])
    if ret.returncode != 0:
        parser.error(f'--base-branch {args.base_branch} names no commit')
    requested_sha = ret.stdout.strip()
    try:
        base_source, reason, baseline, snapshots = resolve_base(args.base_source, unsupported, args.baseline_repo,
                                                                requested_sha)
    except BaselineUnavailable as e:
        print(f'Error: CI base unavailable: {e} (--base-source local builds it)')
        return 1
    provenance = {'requested': args.base_source or 'default', 'effective': base_source, 'reason': reason,
                  'requested_sha': requested_sha, 'base_sha': baseline['sha'] if baseline else requested_sha,
                  'baseline': baseline}
    if baseline:
        subject = _commit_subject(baseline['sha'])
        base_line = (f'Base: CI snapshots of {_clean(baseline["sha"][:10])}' + (f' "{subject}"' if subject else '')
                     + (f', {baseline["note"]}' if baseline.get('note') else '')
                     + f' {_clean(baseline.get("url"), 200)}')
    else:
        base_line = f'Base: local build of {requested_sha[:10]}' + (f' (CI base unavailable: {reason})' if reason else '')

    if os.path.isdir(worktree_dir):
        # twice: a killed `worktree add` leaves it locked `initializing`
        run(['git', '-C', TINYUSB_ROOT, 'worktree', 'remove', '--force', '--force', worktree_dir])
    failed = False
    try:
        if base_source == 'local':
            # --detach: check out the commit at a detached HEAD instead of trying to claim
            # the branch, which may be checked out elsewhere (main repo, another worktree)
            ret = run(['git', '-C', TINYUSB_ROOT, 'worktree', 'add', '--detach', worktree_dir, requested_sha])
            if ret.returncode != 0:
                print(f'Error creating worktree: {ret.stderr}')
                sys.exit(1)
            symlink_deps(TINYUSB_ROOT, worktree_dir)
        else:
            base_shards, base_failures = baseline_shards(snapshots, baseline, args.board)
            # a failure of the downloaded run itself, not of one board, belongs in every report
            run_failures = [f for f in base_failures if f[0][0] not in args.board]
            membrowse_version = _membrowse_version()
        base_sha = provenance['base_sha']
        current_rev = short_hash(TINYUSB_ROOT)
        base_label = short_hash(worktree_dir) if base_source == 'local' else base_sha[:10]
        labels = (base_label or SIDE_LABELS[0], current_rev or SIDE_LABELS[1])
        print(f'diff {args.base_branch} ({labels[0]}) vs working tree ({labels[1]}) · {args.engine}')
        if base_source == 'ci' or reason:
            print(f'  {base_line}')
        focused = _focused(args.board, examples)

        def report_data(data):
            """The JSON report for --json, None without it."""
            if not args.json:
                return None
            return {**data, 'base_ref': args.base_branch, 'base_sha': base_sha, 'current_rev': current_rev,
                    'base_source': provenance,
                    'filters': {'base': base_filters if base_source == 'local' else None, 'current': cur_filters}}

        def preface(warnings):
            """The report's base provenance and metadata warnings, above its tables."""
            return '\n'.join([base_line] + [f'- {w}' for w in warnings]) + '\n\n'

        # --combined: every board's elf sizes and failures, paired at the end
        combined_sides = {'base': {}, 'current': {}}
        combined_failures = []
        combined_warnings = []
        combined_symbols = args.symbols

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
                    if side == 'base' and base_source == 'ci':
                        continue
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

            warnings, symbols, shard = [], args.symbols, base_shards.get(board) if base_source == 'ci' else None
            if shard and not build_failure:
                warnings = metadata_warnings(board, shard, _cmake_compiler(cur_build), membrowse_version)
                if symbols and not all('symbols' in s for s in shard['elfs'].values()):
                    symbols = False
                    warnings.append(f'{board}: symbols omitted: the CI snapshot has none')
            combined_symbols = combined_symbols and symbols
            combined_warnings += warnings
            for w in warnings:
                print(f'  WARNING {w}')

            for example in examples:
                scope = _scope_label(examples, example)
                sides = {'base': {}, 'current': {}}
                failures = [build_failure] if build_failure else []
                if not build_failure:
                    phase = Phase(f'size and compare{scope}')
                    for side, build, filters in (('base', base_build, base_filters),
                                                 ('current', cur_build, cur_filters)):
                        if side == 'base' and base_source == 'ci':
                            continue
                        sizes, errors = generate_sizes(build, filters, example, args.engine)
                        sides[side] = {(board, rel): v for rel, v in sizes.items()}
                        failures += [((board, rel), side, 'report', msg) for rel, msg in errors]
                    if base_source == 'ci':
                        failures += run_failures + [f for f in base_failures if f[0][0] == board]
                        cur_elfs = {elf: s for (_b, elf), s in sides['current'].items()}
                        if shard:
                            sides['base'], scoped, sides['current'], _out = _pair_base_shard(
                                board, shard, cur_elfs, [example] if example else None)
                            failures += scoped
                        else:  # no usable baseline: nothing of this board is new, none compared
                            sides['current'] = {}

                md, failures, ok, data = compare_sides(
                    sides['base'], sides['current'], args.engine, failures, [board], scope=(board, None),
                    symbols=symbols, labels=labels, files_label=files_label)
                md = preface(warnings) + md
                data['warnings'] = warnings
                if not build_failure:
                    phase.done(failed=not ok)
                failed |= not ok
                if not build_failure:  # a build failure is recorded once, above
                    for side, sizes in sides.items():
                        combined_sides[side].update(sizes)
                    combined_failures += failures
                tables = _labelled({(p['board'], p['elf']): _pair_tables(p['base'], p['current'], symbols,
                                                                         args.engine, labels)
                                    for p in data['pairs'] if _pair_changed(p['base'], p['current'], symbols)},
                                   len(data['pairs'])) if focused else ()
                print_result(diff_summary(data, symbols, files_label), f'{example}: ' if scope else '',
                             tables)
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
                        for title, options in (('sections', ['-d', 'compileunits,sections']),
                                               ('symbols', ['-d', 'compileunits,symbols', '-s', 'vm'])):
                            print(f'--- bloaty {title} ---')
                            ret = run(bloaty_common + options + [cur_elf, '--', base_elf])
                            if ret.returncode == 0:
                                print(ret.stdout)
                            else:
                                print(f'  bloaty FAILED (exit {ret.returncode})')
                                print('\n'.join('    ' + line for line in output_excerpt(ret)))
                                failed = True
                    else:
                        print('  bloaty: ELF not found')

        if args.combined:
            os.makedirs(combined_dir, exist_ok=True)
            print(f'combined ({len(args.board)} boards)')
            # every scope was filter-checked above, and its failures carried over
            # a board's or the run's baseline failure is in each of its scopes' failures
            md, _failures, ok, data = compare_sides(
                combined_sides['base'], combined_sides['current'], args.engine, list(dict.fromkeys(combined_failures)),
                args.board, symbols=combined_symbols, labels=labels, files_label=files_label)
            md = preface(combined_warnings) + md
            data['warnings'] = combined_warnings
            failed |= not ok
            print_result(diff_summary(data, combined_symbols, files_label)
                         + ([f'  {len(combined_warnings)} metadata warnings, see the report'] if combined_warnings else []))
            write_report(os.path.join(combined_dir, 'diff'), md, report_data(data))
    finally:
        # an add stopped by a signal leaves it too, locked `initializing`
        ret = run(['git', '-C', TINYUSB_ROOT, 'worktree', 'remove', '--force', '--force', worktree_dir]) \
            if os.path.isdir(worktree_dir) else None
        if ret and ret.returncode != 0:
            print(f'Error removing worktree {worktree_dir}: {ret.stderr.strip()}')
            failed = True
    return 1 if failed else 0


if __name__ == '__main__':
    exit_on_termination()
    sys.exit(main())
