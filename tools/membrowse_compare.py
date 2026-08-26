#!/usr/bin/env python3
"""Diff two membrowse local reports per source file.

The membrowse CLI has report generation but no compare subcommand; this module
runs `membrowse report --json --all-symbols` per elf and diffs the results.
Key normalization: paths are keyed RELATIVE to the first matched filter
substring, so base and current checkouts (different absolute prefixes)
compare under the same key.

membrowse (>=1.2.9) unconditionally truncates the JSON report's `source_file`
field to a bare basename (membrowse/analysis/sources.py, _get_basename()) -
DWARF resolution computes the full absolute path internally but never surfaces
it. An absolute-path substring filter can therefore never match `source_file`.
`object_file` still carries what we need: CMake lays object files out as
`<target>.dir/<abs-source-path>.obj`, so the full source path survives inside
it. per_file_sizes() filters/keys on `object_file`, falling back to
`source_file` only for the symbols that have no object_file (mostly
archive-linked libc/libgcc code, where membrowse reports `archive` instead) -
those basenames won't match an absolute-path filter either, which is the
correct outcome since that code isn't part of the checkout being measured.
"""
import json
import os
import subprocess

# section-name -> which budgets a symbol counts against
FLASH_SECTIONS = ('.text', '.rodata', '.isr_vector', '.vector', '.init', '.fini')
RAM_SECTIONS = ('.bss', '.noinit', '.stack', '.heap')
BOTH_SECTIONS = ('.data', '.ramfunc', '.fastrun', '.itcm', '.dtcm')


def report_for_elf(elf_path, map_path=None):
    """Run membrowse local report on one elf, return parsed JSON dict."""
    cmd = ['membrowse', 'report', elf_path, '--json', '--all-symbols']
    if map_path and os.path.isfile(map_path):
        cmd += ['--map-file', map_path]
    r = subprocess.run(cmd, capture_output=True, text=True)
    if r.returncode != 0:
        raise RuntimeError(f'membrowse report failed for {elf_path}: {r.stderr}')
    return json.loads(r.stdout)


def _bucket(section):
    s = section or ''
    if any(s.startswith(p) for p in BOTH_SECTIONS):
        return ('flash', 'ram')
    if any(s.startswith(p) for p in RAM_SECTIONS):
        return ('ram',)
    if any(s.startswith(p) for p in FLASH_SECTIONS):
        return ('flash',)
    return ('flash',)  # unknown allocated section: count as flash, never drop


def per_file_sizes(report, filters):
    """{relative source path: {'flash': n, 'ram': n}} for symbols matching filters.

    Filters/keys on `object_file` (falls back to `source_file` when a symbol has
    no object_file) - see the module docstring for why. A trailing `.obj`/`.o` is
    stripped from the derived key so the table reads as source files, not objects.
    """
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
        for b in _bucket(sym.get('section')):
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
