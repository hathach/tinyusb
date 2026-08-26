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

Flash-vs-RAM bucketing (_bucket()) is layout-driven, name-table fallback only:
see _bucket()'s own docstring for the rule and why, verified against a real
`membrowse report --json --all-symbols` output, not the section-name-prefix
guessing this replaced.
"""
import json
import os
import subprocess

# Fallback only (see _bucket()): section-name -> which budgets a symbol counts
# against, used when a report has no usable memory_layout, or a symbol's own
# section matches no region in it. The layout-based rule below is the real fix
# for vendor names this table can't enumerate; these entries are the ones this
# repo's own linker scripts and TinyUSB sources confirm often enough to be
# worth naming explicitly, so the fallback doesn't need a layout to get them
# right too - each group's origin is noted so a future edit can re-verify it:
#  - .isr_vector/.vector/.init/.fini/.text/.rodata: original entries, unchanged
#  - .interrupts: the vector-table output section used instead of (or to hold)
#    .isr_vector by 126 in-repo NXP/Kinetis/LPC/RA linker scripts - e.g.
#    hw/bsp/imxrt/boards/metro_m7_1011/metro_m7_1011.ld
#  - .flash_config, .ivt: imxrt's FlexSPI NOR boot header output sections (the
#    `.boot_hdr.*` input patterns are merged into these, never their own output
#    section) - hw/bsp/imxrt/boards/metro_m7_1011/metro_m7_1011.ld
#  - .id_code, .option_setting: RA's MCU option-setting/ID-code flash config -
#    hw/bsp/ra/boards/ra6m5_ek/script/fsp.ld
FLASH_SECTIONS = ('.text', '.rodata', '.isr_vector', '.vector', '.init', '.fini',
                  '.interrupts', '.flash_config', '.ivt', '.id_code', '.option_setting')
# - NonCacheable: imxrt's CFG_TUSB_MEM_SECTION (EHCI/DMA buffers) -
#   hw/bsp/imxrt/family.cmake
# - .fast: hpmicro's TU_ATTR_FAST_FUNC - src/common/tusb_mcu.h (OPT_MCU_HPM)
# - m_usb_global: LPC55's USB-controller SRAM output section (inside the
#   m_usb_sram MEMORY region) - hw/mcu/nxp/mcux-devices-lpc/LPC5500/*/gcc/*.ld
# - .time_critical: rp2040's TU_ATTR_FAST_FUNC expands to pico-sdk's
#   __not_in_flash("tinyusb") (src/common/tusb_mcu.h, OPT_MCU_RP2040), which
#   pico-sdk defines as __attribute__((section(".time_critical." group))) -
#   pico-sdk is a fetched dependency not present in every checkout, so this one
#   is verified via TinyUSB's own macro choice and pico-sdk's documented
#   expansion, not a `.time_critical` string grepped from a linker script here
RAM_SECTIONS = ('.bss', '.noinit', '.stack', '.heap',
                'NonCacheable', '.fast', 'm_usb_global', '.time_critical')
# .ccmram (stm32f4 core-coupled RAM) is BOTH, not RAM-only: its own linker
# script loads it via `AT> FLASH` exactly like .data (verified:
# hw/bsp/stm32f4/boards/stm32f407disco/STM32F407VGTx_FLASH.ld declares
# `.ccmram : { ... } >CCMRAM AT> FLASH`), so it has a flash-resident load image
# in addition to its CCMRAM run address.
BOTH_SECTIONS = ('.data', '.ramfunc', '.fastrun', '.itcm', '.dtcm', '.ccmram')

# Best-effort flash/ram guess from a MEMORY region's own NAME (RAM, RAM_D1,
# DTCMRAM, CCMRAM, FLASH, ROM, ...) - see _classify_region() for why this, and
# not the region's `type` field, is the real signal.
_RAM_REGION_HINTS = ('ram', 'tcm', 'ddr')
_FLASH_REGION_HINTS = ('flash', 'rom')


def report_for_elf(elf_path, map_path=None):
    """Run membrowse local report on one elf, return parsed JSON dict."""
    cmd = ['membrowse', 'report', elf_path, '--json', '--all-symbols']
    if map_path and os.path.isfile(map_path):
        cmd += ['--map-file', map_path]
    r = subprocess.run(cmd, capture_output=True, text=True)
    if r.returncode != 0:
        raise RuntimeError(f'membrowse report failed for {elf_path}: {r.stderr}')
    return json.loads(r.stdout)


def _bucket_by_name(section):
    """Fallback: guess flash/ram from the SYMBOL's own section name (see the
    FLASH_SECTIONS/RAM_SECTIONS/BOTH_SECTIONS comment). Used when a report has no
    usable memory_layout, or a symbol's section is not in it - so behavior is never
    worse than before the layout-based rule in _bucket() existed."""
    s = section or ''
    if any(s.startswith(p) for p in BOTH_SECTIONS):
        return ('flash', 'ram')
    if any(s.startswith(p) for p in RAM_SECTIONS):
        return ('ram',)
    if any(s.startswith(p) for p in FLASH_SECTIONS):
        return ('flash',)
    return ('flash',)  # unknown allocated section: count as flash, never drop


def _classify_region(name):
    """Best-effort flash/ram guess from a MEMORY region's own name, or None if
    unrecognized.

    NOT the region's `type` field: verified against a real
    `membrowse report --json --all-symbols` (membrowse 1.2.9) that every region
    built from a parsed linker script reports type 'UNKNOWN' regardless of the
    script's own (rx)/(rw) attributes - membrowse/core/models.py's MemoryRegion
    hardcodes that default, and no linker-script parser in the installed package
    ever overrides it (only membrowse's own no-linker-script fallback, which
    TinyUSB's build never hits, sets a real type). The region's name - FLASH,
    RAM, RAM_D1, DTCMRAM, CCMRAM, ROM, ... - is the only region-level signal
    membrowse still surfaces.
    """
    n = (name or '').lower()
    if any(h in n for h in _FLASH_REGION_HINTS):
        return 'flash'
    if any(h in n for h in _RAM_REGION_HINTS):
        return 'ram'
    return None


def _section_regions(memory_layout):
    """{section name: [(region name, elf-derived section type), ...]}, from every
    region's own `sections` list in a membrowse --json report's `memory_layout`.

    A section placed by the linker via `AT()` (its load image lives in flash, it
    runs from ram - `.data`, or TinyUSB's TU_ATTR_FAST_FUNC/.time_critical-style
    RAM functions) is listed under BOTH regions, once per address (LMA in the
    flash-side region's list, VMA in the ram-side one) - membrowse computes this
    from the ELF's own LMA/VMA split (MemorySection.lma), so cross-referencing it
    here is reading that placement back, not re-deriving it. A section with no
    such split (`.bss`, `.text`, ...) appears in exactly one region's list.
    """
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
        # placed in more than one region: a flash-side load copy and a ram-side
        # run copy (see _section_regions()) - counts against both budgets
        return ('flash', 'ram')
    region_name, section_type = regions[0]
    bucket = region_bucket.get(region_name)
    if bucket is not None:
        return (bucket,)
    # Region name unrecognized (e.g. NXP imxrt's m_data2/m_text): fall back to the
    # ELF section classification membrowse already computed from sh_flags for this
    # entry. SHF_WRITE ('data') has no meaning other than "writable at runtime" -
    # with no separate load copy found above, it must be the section's only
    # (runtime) placement, i.e. RAM. SHF_EXECINSTR / read-only ('code'/'rodata')
    # with no separate load copy is resident where it runs: flash.
    if section_type == 'data':
        return ('ram',)
    if section_type in ('code', 'rodata'):
        return ('flash',)
    return None  # SECTION_TYPE_UNKNOWN (not SHF_ALLOC) or missing - no signal


def _bucket(section_name, section_regions, region_bucket):
    """flash/ram/both budget(s) a symbol in `section_name` counts against.

    Primary: membrowse's own `memory_layout` (see _bucket_from_layout()) - which
    MEMORY region(s) the linker actually placed this section's address(es) in,
    rather than guessing from the section's name. This is what fixes RAM sections
    under names FLASH_SECTIONS/RAM_SECTIONS never enumerate (see the module
    docstring) - they still show up in whichever region their address falls in.
    Falls back to _bucket_by_name() - the old name-prefix table - when the report
    has no usable memory_layout, or this section matches no region in it, so
    behavior is never worse than before this rule existed.
    """
    layout_result = _bucket_from_layout(section_name, section_regions, region_bucket)
    if layout_result is not None:
        return layout_result
    return _bucket_by_name(section_name)


def per_file_sizes(report, filters):
    """{relative source path: {'flash': n, 'ram': n}} for symbols matching filters.

    Filters/keys on `object_file` (falls back to `source_file` when a symbol has
    no object_file) - see the module docstring for why. A trailing `.obj`/`.o` is
    stripped from the derived key so the table reads as source files, not objects.
    """
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
