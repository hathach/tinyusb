---
name: code-size
description: Use when comparing TinyUSB code size between a base ref (master by default) and the current branch to evaluate the size impact of changes. Three granularities — single example on one board (with optional bloaty), all examples on one board, or all examples across the CI-pinned boards combined — sized by membrowse, linkermap or bloaty.
---

# Code Size Comparison

Compare TinyUSB code size between a base ref (default `master`) and the current branch using `tools/metrics_compare_base.py`. Three granularities — pick the narrowest one that exercises your change:

| Granularity | When to use | Command |
|---|---|---|
| **single example, one board** | Focused change touching one feature | `-b BOARD -e device/cdc_msc` |
| **all examples, one board** | Per-board regression sweep | `-b BOARD` |
| **all examples, CI-pinned boards (combined)** | Pre-merge full check | `--ci` |

The script does the whole base-vs-branch dance itself: a temporary git worktree of the base ref under `cmake-metrics/_worktree/` (removed on exit), base + branch builds under `cmake-metrics/<board>/{base,build}/`, then a compare via `tools/metrics_compare.py` (report paths under Outputs).

It **pairs** each base elf with the current elf of the same (board, elf path) and reports deltas per pair; sizes are never averaged or summed across examples or boards, since TinyUSB's size depends on each board's port and each example's `tusb_config.h`. `--engine` picks where each elf's per-file sizes come from:

| Engine                | Sizes                                                             | Needs                                                                                                     |
|-----------------------|-------------------------------------------------------------------|-----------------------------------------------------------------------------------------------------------|
| `membrowse` (default) | symbols of `membrowse report --json --all-symbols`                | `pip install membrowse` (CI gets it from `.github/actions/get_deps/action.yml`, which never runs locally) |
| `linkermap`           | input sections of the elf's GNU ld map, by object path            | `python3 tools/get_deps.py` (`tools/linkermap`)                                                           |
| `bloaty`              | `bloaty -d compileunits,sections` VM sizes, by DWARF compile unit | `bloaty` on PATH                                                                                          |

Every engine takes flash/RAM from the elf's section and program headers (a section copied from flash counts in both). Per-file sizes agree closely across engines; the whole-elf total counts different things per engine (its column is named after them: all symbols, all input sections, all accounted sections), so compare totals only within one engine. See the `membrowse` skill for membrowse itself.

## Choosing arguments

Infer from the user's request:

- **Board(s):** named board → `-b BOARD` (repeatable; add `--combined` for one report over them). "All boards" / "CI" / "full sweep" → `--ci` (the membrowse CI-pinned boards in `.github/ci-pinned-boards.json`, which cover every dcd/hcd driver not waived in its `uncovered` list; implies `--combined`; needs the arm, riscv and msp430 toolchains). Default to a fast board (`raspberry_pi_pico`) if unspecified for an iterative check.
- **Example:** named example → `-e <group>/<name>` (e.g. `-e device/cdc_msc`). "All examples" → omit `-e`.
- **Engine:** membrowse unless asked. Rerun with `--engine linkermap` or `--engine bloaty` to cross-check a surprising delta, or when membrowse is not installed.
- **Bloaty:** `--bloaty` only with `-e`, whatever the engine. Use when the user wants a section/symbol-level breakdown for a single binary.
- **JSON:** `--json` when the numbers feed a script; it adds a `.json` of the paired raw sizes next to each report.
- **Base ref:** default `master`. Override with `--base-branch <ref>` (tag or commit also works).
- **Filter:** default is the absolute path of each side's `<checkout>/src/` directory, which uniquely identifies TinyUSB stack code without matching vendored deps that also have a `src/` (e.g. `pico-sdk/src/`). Override with one or more `-f SUBSTRING` flags to use repo-relative substrings instead. Change only if asked.

## Common invocations

```bash
# Single example, one board (add --bloaty for section/symbol breakdown):
python3 tools/metrics_compare_base.py -b raspberry_pi_pico -e device/cdc_msc

# All examples for one board (repeat -b for several boards):
python3 tools/metrics_compare_base.py -b raspberry_pi_pico

# Full CI sweep (CI-pinned boards, combined):
python3 tools/metrics_compare_base.py --ci

# Cross-check with another engine, keeping the paired sizes as JSON:
python3 tools/metrics_compare_base.py -b raspberry_pi_pico --engine linkermap --json
```

## Outputs

- **Per-board:** `cmake-metrics/<board>/metrics_compare.md`, or `metrics_compare_<example>.md` instead when `-e` is set
- **Combined (`--combined`, auto-set by `--ci`):** `cmake-metrics/_combined/metrics_compare.md`, over every board's pairs
- **JSON (`--json`):** `metrics_compare[_<example>].json` beside each `.md`: engine, base ref and built SHA, filters, every compared pair's raw base/current sizes, unmatched elfs and failures
- **Bloaty:** printed to stdout as section + symbol diffs

## Timing

- Single example, single board: ~30 s
- All examples, single board: ~60-90 s
- `--ci` (the CI-pinned boards): ~7-8 minutes — sequential sweep across boards (Ninja parallelizes within each board, not across)

Run `--ci` in the background: it comes close to a 10-minute (600000 ms) command timeout.

## Reporting results

Each report opens with a coverage line; `INCOMPLETE` means a build, report or filter match failed, or an elf exists on one side only (listed as base-only / current-only, outside every statistic). Then:
- **One pair** (one board, one elf): the per-file base/new/Δ table and the whole-elf delta.
- **Many pairs:** changed pairs with their filtered (default: TinyUSB `src/`) and whole-elf Flash/RAM Δ; changed files with `Changed / present` pair counts and the min/max Δ, each nonzero one naming the pair it came from; a collapsed per-file table for each changed pair (in the `.md` only, not on stdout).

After running, show the coverage line and both tables (say so if the report is `INCOMPLETE`), then:
- A non-zero delta under the default filter is a stack-size impact in that configuration, unless it is membrowse attribution noise (next bullets). A whole-elf Δ with a zero filtered total is a change outside the filter (e.g. inlined headers, example/BSP code); check the per-file table for cancelling filtered changes. Membrowse's all-symbols sums cover symbols in allocated sections only, overlap through aliases and exclude padding, so they are not whole-elf bytes.
- Per file and metric: min > 0 means growth in every present pair, max > 0 growth in at least one; name the worst-growth pair.
- Unexpected filtered Δ with an unchanged all-symbols total may be the known map-attribution bug: membrowse ≤ 1.2.9 credits a symbol to the wrong object when a `.debug_*` map offset collides with its address ([membrowse-action#168](https://github.com/membrowse/membrowse-action/pull/168)). A zero-change `--ci` run shows it on 32 of 857 pairs, all zero under `--engine linkermap` and `--engine bloaty`. Confirm with either engine, a zero-change run (`--base-branch HEAD`), or the pair's symbol/object evidence before dismissing it; real growth inside the filter can also cancel shrinkage outside it.
- If the diff is unexpected, follow up with a single-example `--bloaty` run to localize.
