# Drop the linkermap Metrics Pipeline

> rename to pr<NNN>-drop-linkermap.md when the PR opens

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

**Spec:** none — split out of the rework-metrics branch (`docs/superpowers/specs/2026-08-25-rework-metrics-design.md` / `docs/superpowers/plans/2026-08-25-rework-metrics.md`), which made membrowse the default engine but kept linkermap alive as the only engine `--combined`/`--ci` can use.

**Origin:** split out of the `rework-metrics` branch's final-review fix wave. Delete this
file when its own PR lands.

## What is already established

- **membrowse is already the default engine.** Commit `4d9629354` ("ci: membrowse-only size
  analytics - drop linkermap metrics pipeline, gate uploads on pinned boards") switched CI
  uploads to membrowse; `tools/metrics_compare_base.py --engine` defaults to `membrowse`
  (`tools/metrics_compare_base.py:222-225`).
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
- **The remaining necessary use of `--engine linkermap` is narrow and identified.** It is
  `--combined`/`--ci` only: `metrics_compare_base.py:254-257` hard-refuses
  `--combined` with the membrowse engine (`parser.error('--combined is not yet supported with
  --engine membrowse ...')`), and `--ci` implies `--combined`
  (`metrics_compare_base.py:239-240`). Every other invocation (single example, all examples on
  one board) already defaults to membrowse. `.claude/skills/code-size/SKILL.md` and
  `.claude/skills/membrowse/SKILL.md` both point users at `--engine linkermap` for exactly
  this one case.
- **Inventory of what removal touches** (`grep -rl linkermap` across the tree, minus this
  branch's own planning docs):
  - `hw/bsp/family_support.cmake:271-293` — `family_add_linkermap()`, the `<target>-linkermap`
    / `examples-linkermap` CMake targets, and its call site inside `family_add_bloaty`'s sibling
    block (`hw/bsp/family_support.cmake:560`).
  - `tools/metrics.py` — the whole file (report generation + `compare` subcommand); nothing
    else imports it once the linkermap branch of `metrics_compare_base.py` is gone.
  - `tools/get_deps.py:23` — the `tools/linkermap` entry in `deps_all`.
  - `tools/metrics_compare_base.py` — the `--engine` flag (collapses to membrowse-only), the
    `want_linkermap` branch in the build loop, `generate_metrics()`, and the combined-mode
    linkermap aggregation block (`metrics_compare_base.py:183-201`, `:371-414` per the current
    line numbers referenced in `.claude/skills/code-size/SKILL.md`).
  - `test/hil/test/test_metrics_compare_base.py` — whatever linkermap-specific cases exist
    there today need triage (drop, or repoint at membrowse if they test shared plumbing).
  - `CLAUDE.md`, `.claude/skills/code-size/SKILL.md`, `.claude/skills/membrowse/SKILL.md` —
    every `--engine linkermap` / `tools/linkermap` mention.

## What remains (not started)

1. Land `pr-rework-metrics-membrowse-combined.md` first (the blocker below).
2. Delete `family_add_linkermap` and its two custom targets from
   `hw/bsp/family_support.cmake`; drop the call to it inside the block that currently calls
   `family_add_bloaty(${TARGET})` / `family_add_linkermap(${TARGET})` /
   `family_add_membrowse(${TARGET})` at `hw/bsp/family_support.cmake:557-562`.
3. Delete `tools/metrics.py`.
4. Delete the `tools/linkermap` entry from `deps_all` in `tools/get_deps.py`; update
   `CLAUDE.md`'s worktree-symlink note (it lists `tools/linkermap` as an example
   `deps_all` key — pick a different example or drop the parenthetical).
5. Collapse `tools/metrics_compare_base.py` to membrowse-only: drop `--engine`, the
   `want_linkermap` branch, `generate_metrics()`, and the combined-mode linkermap
   aggregation — `--combined`/`--ci` then call the membrowse combine path added by the other
   follow-up instead of erroring.
6. Update `test/hil/test/test_metrics_compare_base.py`, `.claude/skills/code-size/SKILL.md`,
   `.claude/skills/membrowse/SKILL.md` to match.
7. Full sweep validation: `pre-commit run --all-files`, plus one real `--ci` run to confirm
   the membrowse combined path (once it exists) produces the same shape of report the
   linkermap combined path did.

## Why this is a separate PR

- **It is blocked**, not merely deferred: `--combined`/`--ci` has no membrowse
  implementation yet (`pr-rework-metrics-membrowse-combined.md`), and linkermap is the only
  engine that path can use today. Removing linkermap before that lands would delete
  the only way to run a full CI-family size sweep.
- **"Proven in daily use" is a time-based gate, not a code-based one.** The rework-metrics
  branch is membrowse's first real deployment; the right trigger for this removal is
  "membrowse has been uploading real CI numbers without a regression for some weeks," which
  cannot be established inside the PR that just switched the default.
- **Bundling it into this PR would blow up an otherwise-reviewable diff.** This branch's
  scope is "rework metrics to run on membrowse"; deleting a second engine wholesale, plus its
  CMake targets, plus a getdeps entry, plus a doc sweep, is a distinct, independently
  reviewable change with its own regression risk (anyone who has `--engine linkermap` in a
  local script or bookmark loses it).
