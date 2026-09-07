#!/usr/bin/env python3
"""Diff membrowse JSON reports per source file.

Membrowse 1.2.9 truncates `source_file`, so paths come from `object_file`.
Memory-layout regions classify flash/RAM; known section names are the fallback.
"""
import json
import os
import shutil
import subprocess

from membrowse_report import extract_ld_scripts, extract_defsyms

# Fallback names verified against in-repo linker scripts.
FLASH_SECTIONS = ('.text', '.rodata', '.isr_vector', '.vector', '.init', '.fini',
                  '.interrupts', '.flash_config', '.ivt', '.id_code', '.option_setting')
RAM_SECTIONS = ('.bss', '.noinit', '.stack', '.heap',
                'NonCacheable', 'm_usb_global')
# These sections have a flash load image and RAM run address.
BOTH_SECTIONS = ('.data', '.relocate', '.ramfunc', '.fastrun', '.itcm', '.dtcm', '.ccmram',
                 '.time_critical', '.fast')

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


def _ld_scripts_and_defsyms(elf_path):
    """Return linker scripts and defsyms from the ELF's ninja build graph."""
    build_dir = _find_ninja_build_dir(elf_path)
    if build_dir is None:
        raise RuntimeError(f'no build.ninja found above {elf_path} - cannot '
                            f'determine its linker scripts')
    target = os.path.relpath(os.path.abspath(elf_path), build_dir)
    ninja_exe = shutil.which('ninja') or 'ninja'
    r = subprocess.run([ninja_exe, '-C', build_dir, '-t', 'commands', target],
                        capture_output=True, text=True)
    if r.returncode != 0:
        raise RuntimeError(f"'{ninja_exe} -C {build_dir} -t commands {target}' "
                            f'failed (exit {r.returncode}): {r.stderr}')
    ld_scripts = extract_ld_scripts(r.stdout)
    if not ld_scripts:
        raise RuntimeError(f'no linker script found in the ninja build graph '
                            f'for {elf_path}')
    return ld_scripts, extract_defsyms(r.stdout)


def report_for_elf(elf_path, map_path=None):
    """Run membrowse local report on one elf, return parsed JSON dict."""
    ld_scripts, defsyms = _ld_scripts_and_defsyms(elf_path)
    cmd = ['membrowse', 'report', elf_path, ' '.join(ld_scripts),
           '--json', '--all-symbols']
    for sym in defsyms:
        cmd += ['--def', sym]
    if map_path and os.path.isfile(map_path):
        cmd += ['--map-file', map_path]
    r = subprocess.run(cmd, capture_output=True, text=True)
    if r.returncode != 0:
        raise RuntimeError(f'membrowse report failed for {elf_path}: {r.stderr}')
    return json.loads(r.stdout)


def _bucket_by_name(section):
    """Guess flash/RAM from a symbol's section name."""
    s = section or ''
    if any(s.startswith(p) for p in BOTH_SECTIONS):
        return ('flash', 'ram')
    if any(s.startswith(p) for p in RAM_SECTIONS):
        return ('ram',)
    if any(s.startswith(p) for p in FLASH_SECTIONS):
        return ('flash',)
    return ('flash',)  # unknown allocated section: count as flash, never drop


def _classify_region(name):
    """Classify by region name; membrowse 1.2.9 reports parsed regions as UNKNOWN."""
    n = (name or '').lower()
    if any(h in n for h in _FLASH_REGION_HINTS):
        return 'flash'
    if any(h in n for h in _RAM_REGION_HINTS):
        return 'ram'
    return None


def _section_regions(memory_layout):
    """Map each section to its memory-layout regions and ELF section types."""
    index = {}
    for region_name, region in memory_layout.items():
        for entry in region.get('sections') or []:
            name = entry.get('name')
            if not name:
                continue
            index.setdefault(name, []).append((region_name, entry.get('type')))
    return index


def _bucket_from_layout(section_name, section_regions, region_bucket):
    """flash/ram/both tuple for `section_name` from the layout, or None if the
    layout has no usable answer (caller falls back to _bucket_by_name())."""
    regions = section_regions.get(section_name)
    if not regions:
        return None
    distinct = {r for r, _ in regions}
    if len(distinct) > 1:
        # A split may span flash/RAM or multiple RAM banks; union classifications.
        buckets = {b for b in (region_bucket.get(r) for r in distinct) if b}
        if buckets:
            return tuple(sorted(buckets))
        return None
    region_name, section_type = regions[0]
    if any(section_name.startswith(p) for p in BOTH_SECTIONS):
        return ('flash', 'ram')
    bucket = region_bucket.get(region_name)
    if bucket is not None:
        return (bucket,)
    # Vendor region names fall back to membrowse's ELF section classification.
    if section_type == 'data':
        return ('ram',)
    if section_type in ('code', 'rodata'):
        return ('flash',)
    return None  # SECTION_TYPE_UNKNOWN (not SHF_ALLOC) or missing - no signal


def _bucket(section_name, section_regions, region_bucket):
    """Classify from memory layout, falling back to the section name."""
    layout_result = _bucket_from_layout(section_name, section_regions, region_bucket)
    if layout_result is not None:
        return layout_result
    return _bucket_by_name(section_name)


def per_file_sizes(report, filters):
    """Return flash/RAM sizes keyed by filtered relative source path."""
    layout = report.get('memory_layout') or {}
    section_regions = _section_regions(layout)
    region_bucket = {name: _classify_region(name) for name in layout}
    by_file = {}
    for sym in report.get('symbols', []):
        src = sym.get('object_file') or sym.get('source_file') or ''
        if not src or not sym.get('size'):
            continue
        key = None
        for f in filters:
            idx = src.find(f)
            if idx >= 0:
                key = src[idx + len(f):]
                break
        if key is None:
            continue
        if key.endswith('.obj'):
            key = key[:-len('.obj')]
        elif key.endswith('.o'):
            key = key[:-len('.o')]
        entry = by_file.setdefault(key, {'flash': 0, 'ram': 0})
        for b in _bucket(sym.get('section'), section_regions, region_bucket):
            entry[b] += sym['size']
    return by_file


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
