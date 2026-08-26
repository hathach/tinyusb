---
name: membrowse
description: Use when analyzing firmware memory footprint with membrowse — local
  size reports for an elf, base-vs-branch size diffs, uploading pinned targets
  to the membrowse dashboard, backfilling history (onboard), or editing
  .github/membrowse-targets.json (pinned boards covering all dcd/hcd drivers).
---

# Membrowse Size Analytics

Membrowse is TinyUSB's first-class size-analytics system. CI uploads every
example of the pinned boards in `.github/membrowse-targets.json` on master
pushes and (non-fork) PR builds; the PR upload feeds membrowse-comment.yml's
size comment. Target names are `<board>/<cmake-target>` — the example
BASENAME (`stm32f407disco/cdc_msc`, never `.../device/cdc_msc`) — and must
never change: history is keyed on them.

## Local report (no API key)

    cmake --build <dir> --target <example>-membrowse     # Ninja only

Runs `tools/membrowse_report.py` (linker-script + `--defsym` extraction from
the ninja graph, then `membrowse report`). For a bare elf: `membrowse report
--help`.

## Base-vs-branch size diff (default engine of the code-size skill)

    python3 tools/metrics_compare_base.py -b <board> [-e device/cdc_msc]

`--engine linkermap` is the legacy fallback (needs `tools/get_deps.py`) and
is required for `--combined`/`--ci`, which error under the membrowse engine.

## Pinned targets

One entry per pinned board with the drivers it covers; `uncovered` documents
drivers with no CI-buildable board. To pin a board: add the entry, then

    pre-commit run membrowse-targets --all-files
    python3 tools/build.py --board-pins .github/membrowse-targets.json --pins-only -e device/cdc_msc <family>

The hook enforces full dcd/hcd/ehci/ohci coverage and that hcd claims build a
host example.

## Upload (needs MEMBROWSE_API_KEY)

CI-only under normal operation (build_util.yml). Manual, matching CI:

    MEMBROWSE_API_KEY=... python3 tools/build.py --board-pins .github/membrowse-targets.json --pins-only --target examples-membrowse-upload -j 1 <family>

The key is read at build time by `tools/membrowse_report.py`, never baked at
configure time, never printed.

## History backfill (onboard)

A newly pinned target has no history before its first upload. Configure the
board's build dir once (`cmake -B examples/cmake-build-<board> -DBOARD=<board>
-G Ninja -DCMAKE_BUILD_TYPE=MinSizeRel examples`), then from the repo root:

    python3 tools/membrowse_onboard.py <board> <role>/<example> [-n 30]           # dry-run
    python3 tools/membrowse_onboard.py <board> <role>/<example> [-n 30] --upload  # real run

The wrapper derives the build script, elf path and CI-matching target name,
and skips rebuilds for commits touching neither `src/` nor `hw/`. Extra flags
pass through to `membrowse onboard` (see its `--help`). Run once per pinned
board/example.
