---
name: code-size
description: Use when sizing TinyUSB examples per file, section or symbol (report), or diffing them between a base ref (master by default) and the working tree to evaluate the size impact of changes (diff) — one example on one board, all examples on one board, or every CI-pinned board combined — sized by membrowse, linkermap or bloaty.
---

# Code Size

`tools/code_size.py` has two subcommands:

- **`report`** builds the working tree, uncommitted changes included, and tabulates each elf as linkermap does: a row per file, a column per output section, size and % of that elf's filtered total.
- **`diff`** builds a base ref (default `master`) in a temporary worktree and the working tree, pairs each base elf with the current elf of the same (board, elf path) and reports per-file deltas per pair: a Flash/RAM table and the same section table as signed deltas, changed rows only.

Sizes are never averaged or summed across examples or boards. Pick the narrowest scope that exercises the change:

| Scope                             | Command                                                        |
|-----------------------------------|----------------------------------------------------------------|
| one example, one board            | `python3 tools/code_size.py diff -b BOARD -e device/cdc_msc`   |
| all examples, one board           | `python3 tools/code_size.py diff -b BOARD`                     |
| all examples, CI-pinned, combined | `python3 tools/code_size.py diff --ci`                         |
| one tree, no diff                 | `python3 tools/code_size.py report -b BOARD -e device/cdc_msc` |

The base worktree symlinks this checkout's fetched dependencies, so a `tools/get_deps.py` pin bump's own size change is not in the diff.

## Arguments

- **`-b BOARD`**, repeatable. For an iterative check with no board named, pass `-b raspberry_pi_pico`.
- **`-e <group>/<name>`**, repeatable; omit for all examples.
- **`--engine`**, membrowse unless asked:

  | Engine                | Per-file sizes from                                                 | Needs                                       |
  |-----------------------|---------------------------------------------------------------------|---------------------------------------------|
  | `membrowse` (default) | `membrowse report --json --all-symbols` symbols                     | `pip install membrowse`                     |
  | `linkermap`           | the GNU ld map's input sections, by object path                     | `python3 tools/get_deps.py tools/linkermap` |
  | `bloaty`              | `bloaty -d compileunits,sections,symbols` VM sizes, by compile unit | `bloaty` on PATH                            |

  Every engine takes flash/RAM from the elf's headers (a section copied from flash counts in both). The whole-elf total counts different things per engine (membrowse's all-symbol sum overlaps aliases and omits padding), so compare it only within one.
- **`--symbols`**: lists each file's symbols under it (`└ name`); without it the table stops at files. linkermap's rows are input sections (`.text.cdcd_open`), not symbols. A diff counts a pair changed when its sections or, with `--symbols`, symbols moved even if Flash/RAM cancel.
- **`--json`**: also writes each report's raw sizes (paired, with the base SHA, for diff), filters and failures as `.json` beside the `.md`; symbols only with `--symbols`.
- **`-f SUBSTRING`**, only if asked: replaces the default filter, each build's absolute `<checkout>/src/` path, which matches TinyUSB code and no vendored `src/`.
- diff only:
  - **`--base-branch <ref>`**: any branch, tag or commit.
  - **`--combined`**: also one report over every `-b` board.
  - **`--ci`**, for "all boards" / "CI": adds the `.github/ci-pinned-boards.json` boards, covering every dcd/hcd driver not waived there, and implies `--combined`; needs the arm, riscv and msp430 toolchains.
  - **`--bloaty`**, with `-e` only: also prints bloaty's section and symbol diff to stdout.

## Outputs and timing

Reports go to `cmake-code-size/<board>/{report,diff}[_<example>].md` and, when combined, `cmake-code-size/_combined/diff.md`. The exit code is nonzero on a failure, when no pair was compared (diff) or no elf of a scope matched a file. The console shows each phase's time, one result line per scope and, for one board and one `-e` example, its changed tables; a failed build prints an excerpt of its output there, and its report and JSON record its first compiler, linker or CMake error, otherwise a fallback message.

A report builds one tree, about half a diff's time. One diff example ~30 s; one board ~60-90 s; `--ci` ~7-8 min, boards built one after another — run it in the background, it nears the 10-minute command timeout.

## Reporting results

Each report opens with a coverage line; `INCOMPLETE` means a build, report or filter match failed, or an elf exists on one side only (listed, outside every statistic). A many-pair diff counts each file whose Flash or RAM changed as `Changed / present` pairs with its min/max Δ naming the pair; a many-elf report lists each elf's totals. Per-elf tables are then in the `.md`'s `<details>` only.

Show the coverage line and the relevant tables, then:
- A filtered delta suggests a TinyUSB size impact in that configuration. A whole-elf Δ with a zero filtered total is outside the filter (inlined headers, example/BSP code); check the per-file table for changes that cancel.
- min > 0 means growth in every present pair, max > 0 in at least one; name the worst-growth pair.
- Corroborate a surprising delta on its own board and example, not the whole sweep: `-b <board> -e <example> --engine linkermap` (~30 s; every run rebuilds both trees); add `--bloaty` if bloaty is on PATH to see the sections and symbols behind it.
