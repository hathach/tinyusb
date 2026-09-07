# membrowse Engine: `--combined`/`--ci` Support

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
  (see `pr3887-drop-linkermap.md`'s +40 B `tusb.c` equivalence result). The gap is
  purely the missing aggregation-across-boards step, not the per-board diff logic.
- **The linkermap combine path is the model to match the *output* of, not necessarily the
  implementation**: `metrics_compare_base.py:371-414` builds a `_combined` dir, concatenates
  metrics JSON across boards, and calls `tools/metrics.py compare` on the aggregate. The
  membrowse analog needs its own aggregation (there is no shared JSON format between the two
  engines to reuse), but the end product — one `cmake-metrics/_combined/metrics_compare.md` —
  should read the same way.

## What remains (not started)

1. **Design the per-board intermediate.** Likely: `per_file_sizes()`'s output dict, plus
   region-classified totals, written to `cmake-metrics/<board>/membrowse_metrics.json` per
   side (base/current) — the membrowse-engine analog of what `generate_metrics()` writes for
   linkermap today.
2. **Implement the combine step**: sum the per-file dicts across all boards' JSON, render with
   the same `compare_reports()` markdown shape, write to
   `cmake-metrics/_combined/metrics_compare.md`.
3. **Drop the `parser.error`** at `metrics_compare_base.py:254-257`; make `--ci`/`--combined`
   work with the (now default) membrowse engine.
4. **Tests**: extend `test/hil/test/test_membrowse_compare.py` for the combine aggregation;
   extend `test/hil/test/test_metrics_compare_base.py` for the `--combined`+membrowse path
   no longer erroring.
5. **Docs**: `.claude/skills/code-size/SKILL.md` and `.claude/skills/membrowse/SKILL.md` both
   currently tell the user `--ci`/`--combined` needs `--engine linkermap` — update once this
   lands (this is also part of what unblocks `pr3887-drop-linkermap.md`).
6. Validate with a real `--ci` run and compare the combined report's totals against the
   equivalent `--engine linkermap --ci` run for the same branch, board-for-board, before
   trusting the new path in CI.

`_bucket()`'s section-name-prefix guessing (the "related, independently-reproducible
correctness bug" this doc used to document here) is fixed — it now buckets by address against
`memory_layout` when a report has one, falling back to the old name-prefix table otherwise
(`tools/membrowse_compare.py`, tested in `test/hil/test/test_membrowse_compare.py`). That
landed as its own commit outside this follow-up's scope; nothing here depends on it further.

## Why this is a separate PR

- It is genuinely unimplemented, not a bug fix on top of working code — `--combined` and
  `--engine membrowse` have never coexisted, so this is new aggregation logic, not a small
  patch.
- It is the direct blocker for `pr3887-drop-linkermap.md`; keeping it as its own
  PR means that removal PR can cite "membrowse combined has shipped and been used for N CI
  runs" as its own established fact rather than bundling an untested new code path with a
  deletion of the fallback for it.
