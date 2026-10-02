---
name: code-size
description: Use when sizing TinyUSB examples per file, section or symbol (report), diffing them against a base ref to judge a change's size impact (diff), or reading or debugging the PR's "Code size" comment (CI snapshot/compare).
---

# Code Size

`tools/code_size.py report` sizes the working tree, uncommitted changes included. `diff` builds a base ref (default `master`) in a temporary worktree and the working tree, pairs elfs by board and elf path, and reports per-file deltas. Pick the narrowest scope that exercises the change; sizes are never summed or averaged across examples or boards.

| Scope                             | Command                                                        |
|-----------------------------------|----------------------------------------------------------------|
| one example, one board            | `python3 tools/code_size.py diff -b BOARD -e device/cdc_msc`   |
| all examples, one board           | `python3 tools/code_size.py diff -b BOARD`                     |
| all examples, CI-pinned, combined | `python3 tools/code_size.py diff --ci`                         |
| one tree, no diff                 | `python3 tools/code_size.py report -b BOARD -e device/cdc_msc` |

The base worktree symlinks this checkout's fetched dependencies, so a `tools/get_deps.py` pin bump's own size change is not in the diff. A local `report` or `diff` cannot size a `-D` variant build.

## Setup and choices

Flags and syntax: `python3 tools/code_size.py <command> --help`. The choices:

- No board named for an iterative check: `-b raspberry_pi_pico`. `-f` only if asked.
- `--engine` membrowse unless asked (`pip install membrowse`; `membrowse==1.2.12` matches CI's attribution); linkermap needs `python3 tools/get_deps.py tools/linkermap`, bloaty needs `bloaty` on PATH. Flash/RAM come from the elf's headers for every engine. Membrowse's all-symbol total can overlap aliases and omit padding, so compare whole-elf totals only within one engine.
- `--symbols` when Flash/RAM cancel: a pair counts as changed when its sections or symbols moved.
- `--ci` for "all boards" / "CI"; it needs the arm, riscv, msp430 and ft9xx toolchains, and ESP-IDF or docker.
- An espressif board builds each example `tools/build.py` picks as its own ESP-IDF project: with the exported ESP-IDF (`. "$IDF_PATH/export.sh"`), else in CI's image through docker, as your user (once: `docker pull espressif/idf:v5.5.3 && docker tag espressif/idf:v5.5.3 espressif/idf:tinyusb`, about 8.4 GB).

## Results

Reports go under `cmake-code-size/<board>/`, combined diffs under `cmake-code-size/_combined/`. A local command exits nonzero on a build or report failure or when no TinyUSB file matched; `report` also when it sized no elf, `diff` when it compared no pair. A failed build prints an excerpt on the console; its report records the first compiler, linker or CMake error.

Approximate times: one diff example ~30 s, one board ~1-1.5 min, an espressif board 10+ min, `--ci` 30+ min. A report is about half a diff. A diff rebuilds both trees every run, so ask for `--symbols` or `--bloaty` in the same run. Run a long sweep in the background.

Each report opens with a coverage line. `INCOMPLETE` means a build, measurement or filter match failed, or an elf exists on one side only; those elfs are listed and excluded, never counted as zero. Show the coverage line and the relevant tables, then:

- A many-elf report summarizes each elf, a many-pair diff each file; per-elf tables are in the `.md`'s `<details>`. Diff statistics cover compared pairs only: `Changed / present` is the pairs whose Flash or RAM of the file changed over the compared pairs containing it. min > 0 means growth in every such pair, max > 0 in at least one; name the worst-growth pair.
- A whole-elf Δ with a zero TinyUSB Δ (`filtered` under a custom `-f`) is unfiltered code (inlined headers, example/BSP code) or an attribution difference; check the per-file table for rows that cancel.
- Corroborate a surprising delta on its own board and example, not the whole sweep: `-b <board> -e <example> --engine linkermap`, plus `--bloaty` if bloaty is on PATH.

An ESP-IDF app is sized by its image: initialized IRAM/DRAM counts in both Flash and RAM, `.flash*` NOBITS reservations are not counted, and its TinyUSB files come from DWARF.

## CI comment

CI never rebuilds for sizing. In a code-changing run, each selected pinned `cmake` build leg, and each `hil-build-esp` leg inside the ESP-IDF image, snapshots the boards it built and uploads `code-size-<toolchain>-<leg>`; it tries even after a failed build, which a missing membrowse does not stop it recording. A `-DMA` variant leg reports as a board of its own named by its `--build-name` (`espressif_s3_devkitm-DMA`), in the comment and on the Membrowse dashboard; reproducing its snapshot needs that name and the leg's `-D` defines, which can enable examples. A `code-size-scope` job uploads the legs and examples the run selected. Master push runs keep snapshots 90 days as baselines, PR runs 14.

`pr_comment.yml`'s `code-size-comment` job runs the default branch's scripts. `.github/scripts/code_size_ci.py baseline` takes the master push run of the merge commit's first parent, else its nearest first-parent ancestor with snapshots (up to 30), labelled approximate: master changes after it count as the PR's. `code_size.py compare` produces the report, which the job publishes as its summary and the `code-size-report` artifact; the sticky `code-size` comment is posted only when `code_size_ci.py is-current` finds no newer push, Build run or attempt for the PR. Runs of other PRs on the same commit are ignored; two open PRs from one head branch can still mask each other. A comment that did not update: read that step's reason and any superseding run it names. Neither the job nor `compare` fails on `INCOMPLETE`.

Reading the comment: it lists every changed TinyUSB file with its min to max Flash and RAM Δ across the builds containing it, so a small driver change shows beside a heavy one; per-build tables are in the full report. CI's `INCOMPLETE` adds a leg or board with no snapshot, a board with no baseline, and snapshots of mismatched commits. Compiler and membrowse differences are noted, not failed; baseline elfs of examples the PR did not build are outside coverage. To reproduce, `gh run download` both runs' `code-size-*` artifacts into two directories and run `python3 tools/code_size.py compare CUR BASE -o OUT --symbols`, adding `--baseline-info INFO` for the baseline annotations.
