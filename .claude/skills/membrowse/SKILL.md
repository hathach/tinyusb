---
name: membrowse
description: Use when analyzing firmware memory footprint with membrowse — local
  size reports for an elf, base-vs-branch size diffs, uploading CI boards
  to the membrowse dashboard, backfilling history (onboard), or editing
  .github/ci-boards.json (CI boards covering all dcd/hcd drivers).
---

# Membrowse Size Analytics

Membrowse is TinyUSB's first-class size-analytics system. CI uploads every
example of the CI boards in `.github/ci-boards.json` on master
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

## CI boards

One entry per CI board with the drivers it covers; `uncovered` documents
drivers with no CI-buildable board. To add a CI board: add the entry, then

    pre-commit run drivers-coverage --all-files
    python3 tools/build.py --ci-boards .github/ci-boards.json --ci-boards-only -e device/cdc_msc <family>

The hook (`tools/drivers_coverage_check.py`) enforces name/claim validity —
unknown driver/board/family, a driver claimed as both covered and uncovered,
an hcd claim on a board that builds no host/dual example — and reports
membrowse and HIL-pool coverage gaps as INFO/WARNING without failing.

## Upload (needs MEMBROWSE_API_KEY)

CI-only under normal operation (build_util.yml). Manual, matching CI:

    MEMBROWSE_API_KEY=... python3 tools/build.py --ci-boards .github/ci-boards.json --ci-boards-only --target examples-membrowse-upload -j 1 <family>

The key is read at build time by `tools/membrowse_report.py`, never baked at
configure time, never printed.

## History backfill (onboard)

A newly added CI board has no history before its first upload. Configure the
board's build dir once (`cmake -B examples/cmake-build-<board> -DBOARD=<board>
-G Ninja -DCMAKE_BUILD_TYPE=MinSizeRel examples`), then from the repo root:

    python3 tools/membrowse_onboard.py <board> <role>/<example> [-n 30]           # dry-run
    python3 tools/membrowse_onboard.py <board> <role>/<example> [-n 30] --upload  # real run

The wrapper derives the build script, elf path and CI-matching target name,
and skips rebuilds for commits touching neither `src/` nor `hw/`. Extra flags
pass through to `membrowse onboard` (see its `--help`). Run once per CI
board/example.
