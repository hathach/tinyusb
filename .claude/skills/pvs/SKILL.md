---
name: pvs
description: Use when running PVS-Studio static analysis (SAST + MISRA C:2023 / C++:2008) on TinyUSB for a given board. Wraps building the examples with an exported compile_commands.json and running pvs-studio-analyzer against .PVS-Studio/.pvsconfig, then converts the log to readable + SARIF output.
---

# PVS-Studio Static Analysis

Run PVS-Studio on TinyUSB for one board. The skill bundles `run_pvs.sh`, which
follows the "Static Analysis (PVS-Studio)" section of AGENTS.md / CLAUDE.md:
build all examples for the board with `compile_commands.json` exported, run
`pvs-studio-analyzer` against it using `.PVS-Studio/.pvsconfig`, then convert the
log to an `errorfile` view and a SARIF report.

## Quick start

```bash
# Whole project for a board:
.claude/skills/pvs/run_pvs.sh raspberry_pi_pico

# Specific files only — -S takes a plaintext list (one path per line), NOT a
# source file directly (the AGENTS.md "-S src/foo.c" snippet is inaccurate):
printf 'src/tusb.c\nsrc/class/cdc/cdc_device.c\n' > /tmp/files.txt
.claude/skills/pvs/run_pvs.sh stm32f407disco -S /tmp/files.txt
```

The first positional argument is **BOARD** (required). Everything after it is
forwarded verbatim to `pvs-studio-analyzer analyze`.

## What the script does

1. **License** — if no license file is registered, materializes one from the
   `PVS_STUDIO_CREDENTIALS` env var (`<name> <key>`, same secret CI uses).
2. **Build** — `cmake examples -B examples/cmake-build-<board> -G Ninja
   -DBOARD=<board> -DCMAKE_BUILD_TYPE=MinSizeRel -DCMAKE_EXPORT_COMPILE_COMMANDS=ON`
   then `cmake --build`. The exported `compile_commands.json` is what PVS reads.
3. **Analyze** — `pvs-studio-analyzer analyze -f <compile_db>
   -R .PVS-Studio/.pvsconfig -o pvs-<board>.log -j<nproc>
   --misra-c-version 2023 --misra-cpp-version 2008 --use-old-parser`.
   (AGENTS.md shows `--dump-files`; it's omitted because it scatters
   `.PVS-Studio.i/.cfg` dumps across the tree — only useful for debugging FPs.)
4. **Report** — `plog-converter -a GA:1,2 -t errorfile` (printed) and
   `-t sarif` → `pvs-<board>.sarif`.

`.PVS-Studio/.pvsconfig` already excludes vendored code (`lib/`, `hw/mcu/`,
`pico-sdk/`, `esp-idf/`, IAR runtime) and suppresses the project's accepted
MISRA deviations — don't duplicate those excludes on the command line.

## Choosing a board

Match the build prerequisites (same as the rest of the repo):

- `raspberry_pi_pico` — what CI's PVS-Studio job uses (needs Pico SDK deps).
- `stm32f407disco` — no external SDK; fastest to get a clean compile DB.
- Anything under `hw/bsp/<family>/boards/` works if its deps are fetched
  (`python3 tools/get_deps.py -b <board>`).

## Outputs

- `pvs-<board>.log` — raw analyzer log (input to plog-converter).
- `pvs-<board>.sarif` — SARIF report (same format CI uploads).
- errorfile findings are printed to stdout for a quick read.

## Timing

Dominated by the example build (tens of seconds to a few minutes depending on
board/deps). The analysis pass itself is ~10-30 s. Use a timeout ≥ 10 minutes
for boards whose deps must be fetched/built first.

## Reporting results

After a run, summarize the findings by rule/severity from the errorfile output
and point the user at `pvs-<board>.sarif`. Cross-check anything flagged in
`src/` against `.pvsconfig` — if it's an already-accepted deviation it will have
been suppressed, so surviving findings are genuinely new.
