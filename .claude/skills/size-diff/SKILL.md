---
name: size-diff
description: Use when diffing TinyUSB code size between a base ref (master by default) and the working tree to evaluate the size impact of changes — one example on one board, all examples on one board, or every CI-pinned board combined — sized by membrowse, linkermap or bloaty.
---

# Size Diff

`tools/size_diff.py` builds a base ref (default `master`) in a temporary worktree and the working tree, uncommitted changes included, then pairs each base elf with the current elf of the same (board, elf path) and reports per-file deltas per pair. Sizes are never averaged or summed across examples or boards. Pick the narrowest scope that exercises the change:

| Scope                             | Command                                                 |
|-----------------------------------|---------------------------------------------------------|
| one example, one board            | `python3 tools/size_diff.py -b BOARD -e device/cdc_msc` |
| all examples, one board           | `python3 tools/size_diff.py -b BOARD`                   |
| all examples, CI-pinned, combined | `python3 tools/size_diff.py --ci`                       |

The base worktree symlinks this checkout's fetched dependencies, so a `tools/get_deps.py` pin bump's own size change is not in the diff.

## Arguments

- **`-b BOARD`**, repeatable; `--combined` adds one report over them. For an iterative check with no board named, pass `-b raspberry_pi_pico`. "All boards" / "CI" → `--ci`: the `.github/ci-pinned-boards.json` boards, covering every dcd/hcd driver not waived there; needs the arm, riscv and msp430 toolchains.
- **`-e <group>/<name>`**, repeatable; omit for all examples.
- **`--engine`**, membrowse unless asked:

  | Engine                | Per-file sizes from                                         | Needs                                           |
  |-----------------------|-------------------------------------------------------------|-------------------------------------------------|
  | `membrowse` (default) | `membrowse report --json --all-symbols` symbols             | `pip install membrowse`                         |
  | `linkermap`           | the GNU ld map's input sections, by object path             | `python3 tools/get_deps.py tools/linkermap`     |
  | `bloaty`              | `bloaty -d compileunits,sections` VM sizes, by compile unit | `bloaty` on PATH                                |

  Every engine takes flash/RAM from the elf's headers (a section copied from flash counts in both). The whole-elf total counts different things per engine (membrowse's all-symbol sum overlaps aliases and omits padding), so compare it only within one.
- **`--bloaty`**, with `-e` only: also prints bloaty's section and symbol diff to stdout.
- **`--json`**: also writes each report's raw paired sizes, base SHA, filters and failures as `.json` beside the `.md`.
- **`--base-branch <ref>`**: any branch, tag or commit.
- **`-f SUBSTRING`**, only if asked: replaces the default filter, each side's absolute `<checkout>/src/` path, which matches TinyUSB code and no vendored `src/`.

## Outputs and timing

Reports go to `cmake-size-diff/<board>/size_diff[_<example>].md` and, when combined, `cmake-size-diff/_combined/size_diff.md`. The exit code is nonzero on a failure or when no pair was compared.

One example ~30 s; one board ~60-90 s; `--ci` ~7-8 min, boards built one after another — run it in the background, it nears the 10-minute command timeout.

## Reporting results

Each report opens with a coverage line; `INCOMPLETE` means a build, report or filter match failed, or an elf exists on one side only (listed, outside every statistic). A many-pair report counts each changed file as `Changed / present` pairs with its min/max Δ naming the pair; per-pair file tables are in the `.md` only.

Show the coverage line and the relevant tables, then:
- A filtered delta suggests a TinyUSB size impact in that configuration. A whole-elf Δ with a zero filtered total is outside the filter (inlined headers, example/BSP code); check the per-file table for changes that cancel.
- min > 0 means growth in every present pair, max > 0 in at least one; name the worst-growth pair.
- Corroborate a surprising delta on its own board and example, not the whole sweep: `-b <board> -e <example> --engine linkermap` (~30 s; every run rebuilds both trees); add `--bloaty` if bloaty is on PATH to see the sections and symbols behind it.
