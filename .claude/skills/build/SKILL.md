---
name: build
description: The project build contract. Use when a change must be verified by building, when an agent needs the boards a diff affects, or when a workflow asks for "the project's build". Resolves scope to boards, builds in a private dir, reports pass/fail with the first error.
---

# Build contract

`scripts/build.py` is the one entry point; every agent and workflow that verifies a change calls it rather than composing cmake lines.

```bash
B=.claude/skills/build/scripts/build.py
python3 $B --scope <changed paths or dirs>   # boards a change affects, one per family
python3 $B --base master                      # same, from the branch diff
python3 $B --board stm32f407disco [-e device/cdc_msc] [-T target]
python3 $B --board stm32f407disco -e host/cdc_msc_hid --cflag=-DCFG_TUH_CDC_FTDI_LATENCY=16
python3 $B ... --shared                       # canonical cmake-build-<board>, the dir HIL flashes from
python3 $B ... --fetch-deps                   # fetch a family's missing deps instead of failing
```

The last stdout line is JSON: `pass`, per-board `status` (`ok`, `failed`, `skipped`, `error`), `built` (elf files this run wrote), `firstError`, `buildDir`, and how the boards were resolved. Exit 0 pass, 1 a board failed, 2 usage or resolution error, 3 no board builds this scope. `-v` streams the build to stderr.

## Judgment

- **Full example set by default.** `-e` narrows to named examples for a quick check; a change in `src/` or a class driver is verified by the full set on every resolved board before it is called green.
- **Scope, not guesswork.** `--scope` takes changed paths, expanding a directory into its tracked files first, and runs `tools/ci_select.py`: one board per affected family; a board whose own `hw/bsp/<family>/boards/<board>/` files changed is built itself, else a rig-roster board of the family, else the first in the family. A full-matrix answer (core or unclassified paths) builds `stm32f407disco` and `raspberry_pi_pico`, plus any board whose own files changed. A special target the changed path needs (for example `tinyusb_metrics` for `tools/metrics.py`) is `-T`; a default sweep is no evidence for it.
- **No board builds the scope** is exit 3, neither a pass nor a usage error, and `nothingToBuild` carries `ci_select`'s reason per path. Report it by kind, and quote the reason verbatim either way, so that a reader of your report can never mistake it for a build that ran:
  - every reason is `non-code, no build contribution` (docs, `.claude/`, `*.md`, unit tests): there is nothing a build could verify, which is not a failure. A boolean verdict (`buildOk`, `pass`) is true, and the reason goes in your notes.
  - any other reason (a class no example enables, a lib nothing builds, a port mapping to no board family): that is firmware nothing compiles. The verdict is false and the reason is the finding — a human decides whether to add coverage or accept the gap.
- **Private dirs for parallel agents.** Builds land in `cmake-build/cmake-build-agent-<pid>-<board>`; remove yours when done. `--shared` writes `cmake-build/cmake-build-<board>`, the dir `hil_test.py` flashes from by default, and needs exclusive ownership of that board: never while a HIL run or another agent is on it.
- **Dependencies.** The family's entries in `tools/get_deps.py` must exist. In a worktree, symlink them from the primary checkout (`CLAUDE.md`, Working Rules); `--fetch-deps` is for a fresh clone. The script never fetches on its own.
- **Code no example enables.** A branch behind a `CFG_*` option no board sets is not compiled by any sweep, so a change there is unverified until you build it with the option on: `--cflag=-D<OPTION>=<value>` (repeatable), narrowed with `-e` to one example that reaches the code. `-D` passes a build-system define (`LOG=2`) the same way, except on Espressif boards, where `tools/build.py` hands defines to cmake only, so the run is refused rather than built without them. Never hand-roll a cmake line for this.
- **Espressif** boards go through `tools/build.py`'s ESP-IDF path; `. "$IDF_PATH/export.sh"` must be in the environment first, and only ESP-IDF examples build for them.
- **Debug builds**: `-DCMAKE_BUILD_TYPE=Debug -DLOG=2 -DLOGGER=rtt` via `tools/build.py -D` directly; this script builds MinSizeRel.
- **Printing a command is not verification.** A verdict is the JSON of a run that happened; an agent that cannot run the script reports the build as not run.
