#!/usr/bin/env python3
"""Diff membrowse JSON reports per source file.

Membrowse 1.2.9 truncates `source_file`, so paths come from `object_file`.
Memory-layout regions classify flash/RAM; known section names are the fallback.
"""
import json
import os
import subprocess

from membrowse_cli import extract_ld_scripts, extract_defsyms, link_command

# Fallback names verified against in-repo linker scripts; anything else counts as flash.
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


def report_for_elf(elf_path, map_path=None):
    """Run membrowse local report on one elf, return parsed JSON dict."""
    build_dir, ld_scripts, defsyms = _link_settings(elf_path)
    cmd = ['membrowse', 'report', elf_path, ' '.join(ld_scripts),
           '--json', '--all-symbols']
    for sym in defsyms:
        cmd += ['--def', sym]
    if map_path and os.path.isfile(map_path):
        cmd += ['--map-file', map_path]
    # from the link's working dir, as membrowse_cli.report() does
    r = subprocess.run(cmd, capture_output=True, text=True, cwd=build_dir)
    if r.returncode != 0:
        raise RuntimeError(f'membrowse report failed for {elf_path}: {r.stderr}')
    return json.loads(r.stdout)


def _bucket_by_name(section):
    """Guess the flash/RAM bucket set from a symbol's section name."""
    s = section or ''
    if any(s.startswith(p) for p in BOTH_SECTIONS):
        return frozenset({'flash', 'ram'})
    if any(s.startswith(p) for p in RAM_SECTIONS):
        return frozenset({'ram'})
    return frozenset({'flash'})


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
    """Bucket set for `section_name` from the layout, or None if the layout has
    no usable answer (caller falls back to _bucket_by_name())."""
    regions = section_regions.get(section_name)
    if not regions:
        return None
    distinct = {r for r, _ in regions}
    if len(distinct) > 1:
        # A split may span flash/RAM or multiple RAM banks; union classifications.
        buckets = {b for b in (region_bucket.get(r) for r in distinct) if b}
        return frozenset(buckets) or None
    region_name, section_type = regions[0]
    if any(section_name.startswith(p) for p in BOTH_SECTIONS):
        return frozenset({'flash', 'ram'})
    bucket = region_bucket.get(region_name)
    if bucket is not None:
        return frozenset({bucket})
    # Vendor region names fall back to membrowse's ELF section classification.
    if section_type == 'data':
        return frozenset({'ram'})
    if section_type in ('code', 'rodata'):
        return frozenset({'flash'})
    return None  # SECTION_TYPE_UNKNOWN (not SHF_ALLOC) or missing - no signal


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


def _bucketer(report):
    """Return section name -> flash/RAM bucket set for one report."""
    layout = report.get('memory_layout') or {}
    section_regions = _section_regions(layout)
    region_bucket = {name: _classify_region(name) for name in layout}
    return lambda section: (_bucket_from_layout(section, section_regions, region_bucket)
                            or _bucket_by_name(section))


def per_file_sizes(report, filters):
    """Return flash/RAM sizes keyed by filtered relative source path."""
    buckets = _bucketer(report)
    by_file = {}
    for sym in report.get('symbols', []):
        src = sym.get('object_file') or sym.get('source_file') or ''
        if not src or not sym.get('size'):
            continue
        key = _relative_key(src, filters)
        if key is None:
            continue
        entry = by_file.setdefault(key, {'flash': 0, 'ram': 0})
        for b in buckets(sym.get('section')):
            entry[b] += sym['size']
    return by_file


def all_symbol_sizes(report):
    """Flash/RAM summed over every sized symbol, attributed or not. Aliases can
    overlap, so this is a symbol-size sum, not occupied bytes."""
    buckets = _bucketer(report)
    total = {'flash': 0, 'ram': 0}
    for sym in report.get('symbols', []):
        if sym.get('size'):
            for b in buckets(sym.get('section')):
                total[b] += sym['size']
    return total


def elf_sizes(report, filters):
    """Per-file (filtered) and all-symbol sizes of one elf."""
    return {'files': per_file_sizes(report, filters), 'all': all_symbol_sizes(report)}


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


def render_pairs(pairs, matched, base_only=(), cur_only=(), failures=(), boards=()):
    """Markdown report over paired elfs keyed by (board, elf path).

    Every statistic is over per-pair deltas. `matched` counts ids built on both
    sides, compared or not;
    `failures` are (elf id, side, stage, message), the elf path None for a
    board-level failure. Failures or unmatched elfs mark the report INCOMPLETE.
    `boards` lists the requested boards, so an unchanged or failed one is named.
    """
    file_deltas = {i: _file_deltas(b, c) for i, (b, c) in pairs.items()}
    changed = [i for i, (b, c) in pairs.items()
               if any(_delta(b['all'], c['all'])) or any(any(d) for d in file_deltas[i].values())]
    status = 'INCOMPLETE' if failures or base_only or cur_only else 'complete'
    lines = [f'**Coverage ({status}):** {len(pairs)} of {matched} matched elf pairs '
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
        lines += [f'`{_label(elf_id)}` all symbols: Flash Δ {_fmt(df)}, RAM Δ {_fmt(dr)}', '',
                  compare_reports(b['files'], c['files'])]
        return '\n'.join(lines)
    if not pairs:
        return '\n'.join(lines + ['_no comparable pairs_', ''])
    if not changed:
        return '\n'.join(lines + ['_no changes_', ''])

    lines += ['| Pair | filtered Flash Δ | filtered RAM Δ | all-symbols Flash Δ | all-symbols RAM Δ |',
              '|------|-----------------:|---------------:|--------------------:|------------------:|']
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


def compare_sides(base, cur, failures=(), boards=(), scope=None):
    """Pair two sides and render them. Returns (md, failures, ok).

    `failures` comes back with a filter failure for `scope` added when no
    compared pair matched a file (wrong filters, or a membrowse report shape
    change broke per_file_sizes()); pass `scope=None` when each scope was
    already checked. `ok` is False on any failure or when nothing was compared.
    """
    pairs, base_only, cur_only = pair_elfs(base, cur)
    failures = list(failures)
    if scope and pairs and not any(b['files'] or c['files'] for b, c in pairs.values()):
        failures.append((scope, 'both', 'filter',
                         'no symbols matched filters - check them, or a membrowse report '
                         'format change broke per_file_sizes() (try --engine linkermap to isolate)'))
    md = render_pairs(pairs, len(base.keys() & cur.keys()), base_only, cur_only, failures, boards)
    return md, failures, bool(pairs) and not failures
