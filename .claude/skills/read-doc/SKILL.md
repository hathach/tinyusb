---
name: read-doc
description: Use when you need authoritative hardware/protocol facts from a primary source rather than model memory — an MCU/peripheral datasheet, reference manual (RM/TRM), errata, pinout, register/bitfield layout, memory map, schematic, or the USB spec — before answering register/electrical/timing/protocol questions from training knowledge or the web; or when the user asks to read/open/look up a manual, datasheet, book, or PDF/EPUB from their Calibre library. Requires a local Calibre library at ~/Documents/calibre-library; no-ops if absent.
---

# Read Doc

## Overview

Some maintainers keep datasheets, manuals, and books in a Calibre library at
`$HOME/Documents/calibre-library/`, laid out as
`AUTHOR/TITLE (id)/TITLE - AUTHOR.pdf|.epub`. For hardware/protocol facts —
registers, bitfields, memory maps, pinouts, electrical/timing specs, errata, USB
spec — read the doc instead of answering from training knowledge or the web.

## Gate first

The library is per-user. Check it exists before anything else:

```bash
[ -d "$HOME/Documents/calibre-library" ] && echo present || echo absent
```

Absent → the skill does not apply; fall back to normal sources silently (don't
mention the library unless the user named it).

## When to use

- About to state a register/bitfield/reset-value/memory-map/pinout/timing spec
  for a specific MCU or peripheral.
- User says "read the RP2040 datasheet", "open the CH569 manual", "what does the
  STM32H7 RM say about…".

Not for general concepts, repo/code questions, or when no such doc is likely.

## Find

Keywords from `/read-doc <keywords>`, else derived from the question (part number,
peripheral, spec name). AND them with chained case-insensitive grep:

```bash
find "$HOME/Documents/calibre-library/" -maxdepth 3 \( -iname '*.pdf' -o -iname '*.epub' \) | grep -i "kw1" | grep -i "kw2"
```

One match → read it. Several → list and ask via AskUserQuestion. None → drop the
weakest keyword and broaden (filenames hold title+author, not tags); still none →
list the closest author/title matches.

## Read

- **PDF:** Read with `pages`; for >10 pages start `pages: "1-20"` (TOC/overview),
  report the page count, then read sections on demand.
- **EPUB:** Read the path directly.
- Summarize in one line (title, pages, coverage) and keep as reference context.

## Common mistakes

- Skipping the gate on a machine with no library.
- Answering a register/spec question from memory when the datasheet is on disk.
- Loading a 1000-page PDF up front instead of TOC-first.
- Requiring all keywords to match — broaden on zero hits.
