---
name: read-doc
description: Use when you need authoritative hardware/protocol facts from a primary source rather than model memory — an MCU/peripheral datasheet, reference manual (RM/TRM), errata, pinout, register/bitfield layout, memory map, schematic, or the USB spec — before answering register/electrical/timing/protocol questions from training knowledge or the web; or when the user asks to read/open/look up a manual, datasheet, book, or PDF/EPUB from their Calibre library. Requires a local Calibre library at ~/Documents/calibre-library; no-ops if absent.
---

# Read Doc

## Overview

Some maintainers keep datasheets, manuals, and books in a Calibre library at
`$HOME/Documents/calibre-library/`. For hardware/protocol facts — registers,
bitfields, memory maps, pinouts, electrical/timing specs, errata, USB spec —
read the doc instead of answering from training knowledge or the web.

Search the library's `metadata.db`, never the filesystem. The database indexes
title, authors, tags, series, publisher, description and the stored filename;
most part numbers live in the tags, which the filesystem does not carry.

## Gate first

The library is per-user and usually on a network mount, so test the database
file, not the directory — an unmounted or half-synced mountpoint is still a
directory:

```bash
[ -f "${CALIBRE_LIBRARY:-$HOME/Documents/calibre-library}/metadata.db" ] && echo present || echo absent
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

Keywords from `/read-doc <keywords>`, else derived from the question (part
number, peripheral, spec name). `search.py` ANDs them across every metadata
field and prints the best matches first — at most 40, and the header says when
more matched:

```bash
python3 .claude/skills/read-doc/search.py errata RT1064      # AND (default)
python3 .claude/skills/read-doc/search.py RT1060 RT1064 --any
```

Exit 0 matched, 1 nothing matched, 2 bad usage or no library — 2 means the
search never ran, so fix the invocation instead of broadening.

One match → read it. Several → list and ask via AskUserQuestion. Nothing
(exit 1) → retry with fewer keywords; the part number alone often works where
`<part> datasheet` does not, because words like "datasheet" and "manual" are
rarely in the metadata. `--any` only changes anything with two or more
keywords. Still nothing → say the document is missing rather than answering
from memory.

Set `CALIBRE_LIBRARY` to search a library elsewhere.

## Read

`search.py` prints one `FORMAT path` line per stored file:

- **PDF** — Read with `pages`; for >10 pages start `pages: "1-20"` (TOC/overview),
  report the page count, then read sections on demand.
- **Any other format** (EPUB, MOBI, CHM, ZIP…) — Read has no decoder for these
  and returns mojibake rather than an error. Say the document is not in a
  readable format; do not paste what Read returned.
- **`MISSING`** — the metadata is real but the file is not on disk (library
  mid-sync, or the file was deleted). Report the file as unavailable, not the
  document as nonexistent.

Summarize in one line (title, pages, coverage) and keep as reference context.

## Common mistakes

- Searching with `find`/`grep` over the library tree. It sees only truncated
  filenames, missing the tags, series and descriptions where part numbers and
  errata IDs actually live. Query the database.
- Skipping the gate on a machine with no library.
- Answering a register/spec question from memory when the datasheet is on disk.
- Loading a 1000-page PDF up front instead of TOC-first.
- Requiring all keywords to match — broaden, or use `--any`, on zero hits.
- Treating a `MISSING` file, or an exit 2, as proof the document is absent.
