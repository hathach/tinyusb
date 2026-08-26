# membrowse Engine: `--combined`/`--ci` Support

> rename to pr<NNN>-membrowse-combined.md when the PR opens

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Let `tools/metrics_compare_base.py --combined` (and `--ci`, which implies it)
aggregate per-board membrowse deltas into one report, the way it already does for the
linkermap engine, so `--engine linkermap` is no longer required for a full CI-family sweep.

**Architecture:** `--combined`'s linkermap path aggregates `map.json` metrics files that
`tools/metrics.py` already writes per board. The membrowse path has no equivalent per-board
JSON to aggregate today — `membrowse_compare.compare_reports()` only ever produces a markdown
table for one board pair. This follow-up needs a per-board machine-readable intermediate
(likely `per_file_sizes()`'s `{path: {'flash', 'ram'}}` dict, dumped to JSON per board) and a
combine step that sums those dicts across boards before rendering the same markdown table
shape `compare_reports()` produces today.

**Tech Stack:** Python 3.13 stdlib (`json`, `subprocess`) — `tools/membrowse_compare.py`,
`tools/metrics_compare_base.py`.

**Spec:** none — split out of the rework-metrics branch; see
`docs/superpowers/specs/2026-08-25-rework-metrics-design.md` for the original membrowse
engine design.

**Origin:** split out of the `rework-metrics` branch's final-review fix wave. Delete this
file when its own PR lands.

## What is already established

- **The gap is a hard `parser.error`, not a silent limitation.**
  `tools/metrics_compare_base.py:254-257`:
  ```python
  if args.combined and args.engine == 'membrowse':
      parser.error('--combined is not yet supported with --engine membrowse '
                    '(it aggregates the linkermap engine\'s per-board metrics '
                    'JSONs); pass --engine linkermap')
  ```
  and `--ci` unconditionally sets `args.combined = True`
  (`tools/metrics_compare_base.py:239-240`), so today `--ci` **requires**
  `--engine linkermap` — there is no membrowse path to reach at all for a full sweep.
- **Per-board membrowse comparison already works and is proven correct for single boards**
  (see `pr-rework-metrics-drop-linkermap.md`'s +40 B `tusb.c` equivalence result). The gap is
  purely the missing aggregation-across-boards step, not the per-board diff logic.
- **The linkermap combine path is the model to match the *output* of, not necessarily the
  implementation**: `metrics_compare_base.py:371-414` builds a `_combined` dir, concatenates
  metrics JSON across boards, and calls `tools/metrics.py compare` on the aggregate. The
  membrowse analog needs its own aggregation (there is no shared JSON format between the two
  engines to reuse), but the end product — one `cmake-metrics/_combined/metrics_compare.md` —
  should read the same way.

## A related, independently-reproducible correctness bug in the same module

Found during this branch's review and not yet fixed (documenting per the reviewer's note so
it doesn't get lost, and because fixing it is naturally part of "get the membrowse engine
production-ready for a combined sweep"):

**`_bucket()` mis-buckets any RAM section whose name isn't in its hardcoded list.**
`tools/membrowse_compare.py:26-28,43-51`:
```python
FLASH_SECTIONS = ('.text', '.rodata', '.isr_vector', '.vector', '.init', '.fini')
RAM_SECTIONS = ('.bss', '.noinit', '.stack', '.heap')
BOTH_SECTIONS = ('.data', '.ramfunc', '.fastrun', '.itcm', '.dtcm')

def _bucket(section):
    s = section or ''
    if any(s.startswith(p) for p in BOTH_SECTIONS):
        return ('flash', 'ram')
    if any(s.startswith(p) for p in RAM_SECTIONS):
        return ('ram',)
    if any(s.startswith(p) for p in FLASH_SECTIONS):
        return ('flash',)
    return ('flash',)  # unknown allocated section: count as flash, never drop
```
A symbol whose section name is not a prefix match for any of the three lists falls through to
the final `return ('flash',)` — silently counted as **flash**, even when it is actually RAM.
Real MCU linker scripts use RAM section names this list does not cover, e.g. `NonCacheable`
(STM32H7/RT cache-disabled RAM), `m_usb_global` (NXP MCX/RT USB-controller SRAM), and
`.ccmram` (STM32F4 core-coupled RAM). Each of those is genuine RAM budget, misreported as
flash budget in every table `compare_reports()` produces today.

**Suggested fix — bucket by address against `memory_layout`, not by section-name prefix.**
`membrowse report --json` (verified against the installed `membrowse` 1.2.9 package,
`membrowse/core/models.py`'s `MemoryReport`/`MemoryRegionDict`) returns, alongside `symbols`,
a top-level `memory_layout: {region_name: {address, limit_size, used_size, sections, ...}}`
describing every linker-script memory region by its real address range. Each symbol dict
carries its own `address` (`SymbolDict.address`). So the ground truth for "is this symbol in
RAM or flash" is: find the `memory_layout` region whose `[address, address + limit_size)`
range contains the symbol's address, and use that region's identity — **not** its `type`
field, which the installed membrowse version always reports as `"UNKNOWN"`
(`MemoryRegion.type` defaults to `"UNKNOWN"` and no parser in `membrowse/linker/*.py` or
`membrowse/core/generator.py` overrides it — confirmed by reading every `MemoryRegion(...)`
construction site in the installed package). In practice this means classifying by the
region's **name** (`FLASH`, `RAM`, `CCMRAM`, `NonCacheable`, `m_usb_global`, ...) after
resolving the containing region by address, which is far more robust than guessing from an
open-ended set of section names: a chip's linker script typically names only 2-5 regions, vs.
however many section names toolchains and vendor SDKs invent.

## What remains (not started)

1. **Design the per-board intermediate.** Likely: `per_file_sizes()`'s output dict, plus
   region-classified totals, written to `cmake-metrics/<board>/membrowse_metrics.json` per
   side (base/current) — the membrowse-engine analog of what `generate_metrics()` writes for
   linkermap today.
2. **Implement the address-based `_bucket()` fix** (see above) — needed regardless of
   `--combined`, but do it here since it changes the per-board numbers the combine step will
   aggregate; fixing it after combined ships would mean re-validating combined output twice.
3. **Implement the combine step**: sum the per-file dicts across all boards' JSON, render with
   the same `compare_reports()` markdown shape, write to
   `cmake-metrics/_combined/metrics_compare.md`.
4. **Drop the `parser.error`** at `metrics_compare_base.py:254-257`; make `--ci`/`--combined`
   work with the (now default) membrowse engine.
5. **Tests**: extend `test/hil/test/test_membrowse_compare.py` for the new `_bucket()`
   behavior (a case per un-covered RAM section name above) and for the combine aggregation;
   extend `test/hil/test/test_metrics_compare_base.py` for the `--combined`+membrowse path
   no longer erroring.
6. **Docs**: `.claude/skills/code-size/SKILL.md` and `.claude/skills/membrowse/SKILL.md` both
   currently tell the user `--ci`/`--combined` needs `--engine linkermap` — update once this
   lands (this is also part of what unblocks `pr-rework-metrics-drop-linkermap.md`).
7. Validate with a real `--ci` run and compare the combined report's totals against the
   equivalent `--engine linkermap --ci` run for the same branch, board-for-board, before
   trusting the new path in CI.

## Why this is a separate PR

- It is genuinely unimplemented, not a bug fix on top of working code — `--combined` and
  `--engine membrowse` have never coexisted, so this is new aggregation logic plus a
  correctness fix in the bucketing that touches every membrowse report, not a small patch.
- It is the direct blocker for `pr-rework-metrics-drop-linkermap.md`; keeping it as its own
  PR means that removal PR can cite "membrowse combined has shipped and been used for N CI
  runs" as its own established fact rather than bundling an untested new code path with a
  deletion of the fallback for it.
- The `_bucket()` fix changes reported numbers for any board with an unusual RAM region name
  (STM32H7/RT `NonCacheable`, MCX/RT `m_usb_global`, F4 `.ccmram` at minimum) — that is a
  correctness-sensitive change worth its own review and its own before/after evidence, not a
  drive-by inside an unrelated final-review fix wave.
