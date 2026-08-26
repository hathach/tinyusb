---
name: code-size
description: Use when comparing TinyUSB code size between a base ref (master by default) and the current branch to evaluate the size impact of changes. Three granularities — single example on one board (with optional bloaty), all examples on one board, or all examples across CI families combined.
---

# Code Size Comparison

Compare TinyUSB code size between a base ref (default `master`) and the current branch using `tools/metrics_compare_base.py`. Three granularities — pick the narrowest one that exercises your change:

| Granularity | When to use | Command |
|---|---|---|
| **single example, one board** | Focused change touching one feature | `-b BOARD -e device/cdc_msc` |
| **all examples, one board** | Per-board regression sweep | `-b BOARD` |
| **all examples, all CI families (combined)** | Pre-merge full check | `--ci --engine linkermap` |

The script does the whole base-vs-branch dance itself: a temporary git worktree of the base ref under `cmake-metrics/_worktree/` (removed on exit), base + branch builds under `cmake-metrics/<board>/{base,build}/`, then a per-file compare — `tools/membrowse_compare.py` for the default engine, or `tools/metrics.py compare` for `--engine linkermap` (report paths under Outputs).

Default size-diff engine is membrowse (local `membrowse report --json --all-symbols` per elf); pass `--engine linkermap` for the legacy map.json-based engine, which needs `python3 tools/get_deps.py` for `tools/linkermap` and is required today for `--combined`/`--ci` (the membrowse engine doesn't support `--combined` yet). The linkermap engine now gets its map.json files via the explicit `examples-linkermap` (or `<example>-linkermap`) cmake target, which the script builds itself — no user action needed. See the `membrowse` skill for engine details.

## Choosing arguments

Infer from the user's request:

- **Board(s):** named board → `-b BOARD` (repeatable). "All boards" / "CI" / "full sweep" → `--ci --engine linkermap` (first board of each arm-gcc family; `--ci` implies `--combined`, which the membrowse engine doesn't support yet). Default to a fast board (`raspberry_pi_pico`) if unspecified for an iterative check.
- **Example:** named example → `-e <group>/<name>` (e.g. `-e device/cdc_msc`). "All examples" → omit `-e`.
- **Bloaty:** only with `-e`. Use when the user wants a section/symbol-level breakdown for a single binary.
- **Base ref:** default `master`. Override with `--base-branch <ref>` (tag or commit also works).
- **Filter:** default is the absolute path of each side's `<checkout>/src/` directory, which uniquely identifies TinyUSB stack code without matching vendored deps that also have a `src/` (e.g. `pico-sdk/src/`). Override with one or more `-f SUBSTRING` flags to use repo-relative substrings instead. Change only if asked.

## Common invocations

```bash
# Single example, one board (add --bloaty for section/symbol breakdown):
python3 tools/metrics_compare_base.py -b raspberry_pi_pico -e device/cdc_msc

# All examples for one board (repeat -b for several boards):
python3 tools/metrics_compare_base.py -b raspberry_pi_pico

# Full CI sweep (first board per arm-gcc family, combined; needs --engine linkermap):
python3 tools/metrics_compare_base.py --ci --engine linkermap
```

## Outputs

- **Per-board:** `cmake-metrics/<board>/metrics_compare.md` (and `_<example>.md` when `-e` is set)
- **Combined (`--combined`, auto-set by `--ci`):** `cmake-metrics/_combined/metrics_compare.md`, aggregating all boards
- **Bloaty:** printed to stdout as section + symbol diffs

## Timing

- Single example, single board: ~30 s
- All examples, single board: ~60-90 s
- `--ci` (all arm-gcc families, first board each): 4-8 minutes — sequential sweep across boards (Ninja parallelizes within each board, not across)

Use timeouts ≥ 10 minutes (600000 ms) for `--ci`.

## Reporting results

After running:
- Show the markdown report's summary table to the user.
- Highlight any row with a non-zero delta (`Flash Δ`/`RAM Δ` for the membrowse engine, `% diff` for `--engine linkermap`) — under the default filter every row is a TinyUSB stack source file (e.g. `usbd.c`, `cdc_device.c`, `dcd_<port>.c`), so any non-zero delta is a real stack-size impact.
- If the diff is unexpected, follow up with a single-example `--bloaty` run to localize.
