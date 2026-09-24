# Drop the linkermap Metrics Pipeline

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Remove the legacy linkermap-based code-size pipeline (CMake targets,
`tools/metrics.py`, the `tools/linkermap` dependency, and `--engine linkermap`) once the
membrowse engine has proven itself in daily CI use, leaving membrowse as the only size-diff
engine.

**Architecture:** No new architecture — this is a subtraction. `family_add_linkermap`
(CMake), `tools/metrics.py`, the `tools/linkermap` entry in `tools/get_deps.py`, and the
`--engine`/`--combined` linkermap code paths in `tools/metrics_compare_base.py` all go away;
`family_add_bloaty` and `family_add_membrowse` are untouched (bloaty is a separate,
independent analysis tool with its own target).

**Tech Stack:** CMake (`hw/bsp/family_support.cmake`), Python 3.13 stdlib
(`tools/metrics_compare_base.py`, `tools/get_deps.py`).

**Spec:** none — split out of the rework-metrics branch (`docs/superpowers/specs/2026-08-25-rework-metrics-design.md`), which made membrowse the default engine but kept linkermap alive as the only engine `--combined`/`--ci` can use.

**Origin:** split out of the `rework-metrics` branch's final-review fix wave. Delete this
file when its own PR lands.

## What is already established

- **membrowse is already the default engine.** Commit `4d9629354` ("ci: membrowse-only size
  analytics - drop linkermap metrics pipeline, gate uploads on pinned boards") switched CI
  uploads to membrowse; `tools/metrics_compare_base.py --engine` defaults to `membrowse`
  (see its `--engine` argument).
- **Per-file equivalence between the two engines is proven, not assumed.** During this
  branch's build, an object-file-keying bug (`membrowse`'s `source_file` truncates to a bare
  basename, so an absolute-path filter matched nothing) was found and fixed by keying/filtering
  on `object_file` instead (`tools/membrowse_compare.py` module docstring + `per_file_sizes`).
  After the fix, an equivalence re-run passed: **`tusb.c` attributed +40 B on both engines**
  (`.superpowers/sdd/2026-08-25-rework-metrics/progress.md:45`, commits
  `314520011..39c53cdd4`). That is the evidence a wholesale engine swap is safe for the
  per-file delta table, not just for totals.
- **CI no longer asks linkermap to run.** `tools/build.py`'s cmake configure step used to pass
  `-DLINKERMAP_OPTION=-q -f tinyusb/src` on every build (removed in this same fix wave, see F7
  of the final-review report) and no CI build target requests `<example>-linkermap` or
  `examples-linkermap` any more — those targets are dead weight in a normal CI run, built only
  when `tools/metrics_compare_base.py --engine linkermap` explicitly asks for them.
- **No mode needs `--engine linkermap` any more.** The membrowse engine now covers
  `--combined`/`--ci` too, pairing base and current elfs by (board, elf path) instead of
  averaging sizes; the old `parser.error` is gone. Its per-pair deltas do not match
  linkermap's averaged numbers by design, so validation is "a zero-change run reports every
  pair unchanged", not number-for-number agreement.
- **Inventory of what removal touches** (`grep -rl linkermap` across the tree, minus this
  branch's own planning docs):
  - `hw/bsp/family_support.cmake` — `family_add_linkermap()`, the `<target>-linkermap`
    / `examples-linkermap` CMake targets, and its call site beside `family_add_bloaty()`.
  - `tools/metrics.py` — the whole file (report generation + `compare` subcommand); nothing
    else imports it once the linkermap branch of `metrics_compare_base.py` is gone.
  - `tools/get_deps.py` — the `tools/linkermap` entry in `deps_all`.
  - `tools/metrics_compare_base.py` — the `--engine` flag (collapses to membrowse-only), the
    `want_linkermap` branch in the build loop, `generate_metrics()`, and the combined-mode
    linkermap aggregation block.
  - `test/hil/test/test_metrics_compare_base.py` — whatever linkermap-specific cases exist
    there today need triage (drop, or repoint at membrowse if they test shared plumbing).
  - `.claude/skills/code-size/SKILL.md`, `.claude/skills/membrowse/SKILL.md` —
    every `--engine linkermap` / `tools/linkermap` mention.

## What remains (not started)

1. Delete `family_add_linkermap` and its two custom targets from
   `hw/bsp/family_support.cmake`; drop the call to it inside the block that currently calls
   `family_add_bloaty(${TARGET})` / `family_add_linkermap(${TARGET})` /
   `family_add_membrowse(${TARGET})`.
2. Delete `tools/metrics.py`.
3. Delete the `tools/linkermap` entry from `deps_all` in `tools/get_deps.py`.
4. Collapse `tools/metrics_compare_base.py` to membrowse-only: drop `--engine`, the
   `want_linkermap` branch, `generate_metrics()`, and the combined-mode linkermap
   aggregation; `--combined`/`--ci` already use the membrowse path.
5. Update `test/hil/test/test_metrics_compare_base.py`, `.claude/skills/code-size/SKILL.md`,
   `.claude/skills/membrowse/SKILL.md` to match.
6. Full sweep validation: `pre-commit run --all-files`, plus one real `--ci` run.

## Why this is a separate PR

- **It is no longer blocked** on code: membrowse covers every mode. The remaining gate is
  the time-based one below.
- **"Proven in daily use" is a time-based gate, not a code-based one.** The rework-metrics
  branch is membrowse's first real deployment; the right trigger for this removal is
  "membrowse has been uploading real CI numbers without a regression for some weeks," which
  cannot be established inside the PR that just switched the default.
- **Bundling it into this PR would blow up an otherwise-reviewable diff.** This branch's
  scope is "rework metrics to run on membrowse"; deleting a second engine wholesale, plus its
  CMake targets, plus a getdeps entry, plus a doc sweep, is a distinct, independently
  reviewable change with its own regression risk (anyone who has `--engine linkermap` in a
  local script or bookmark loses it).
