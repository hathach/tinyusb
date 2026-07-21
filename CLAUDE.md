# TinyUSB Agent Instructions

TinyUSB is a cross-platform USB Host/Device stack for embedded systems: memory-safe (no dynamic allocation) and thread-safe (ISR events deferred to task context).

Reference these instructions first; fall back to search/bash only when reality diverges.

## Behavioral Guidelines

Bias toward caution over speed. For trivial tasks, use judgment.

- **Think first** — state assumptions; ask if unclear; present alternatives instead of picking silently.
- **Simplicity** — no features, abstractions, flexibility, or error handling beyond what was asked. If 200 lines could be 50, rewrite.
- **Surgical changes** — touch only what the task requires; match existing style; don't refactor working code; mention unrelated dead code rather than deleting it. Remove only orphans *your* changes created.
- **Goal-driven** — turn tasks into verifiable goals ("write failing test, make it pass"). For multi-step work, state a brief `step → verify` plan.
- **Worktrees** — default to a git worktree for any branch or multi-step work; never switch the shared primary checkout's branch. Sessions run concurrently: switching the primary checkout mid-flight disrupts other sessions and can silently point a review, build, or commit at the wrong diff. Only trivial one-shot fixes may skip this. Standard location: `.worktrees/<branch-name>` at the repo root (gitignored), e.g. `git worktree add .worktrees/my-branch -b my-branch`.

## Ground Rules

- **Language/style:** C99, 2-space indent (no tabs), snake_case helpers, `UPPER_CASE` macros. Public APIs use `tud_`/`tuh_`; macros use `TU_`. Headers self-contained with `#if CFG_TUSB_MCU` guards.
- **Safety:** no dynamic allocation; defer ISR work to task context; use `TU_ASSERT()` for error checks; always check return values; include order: C stdlib → tusb common → drivers → classes.
- **Layout:** `src/` core, `hw/{mcu,bsp}/` MCU+BSP, `examples/{device,host,dual}/`, `test/{unit-test,fuzz,hil}/`, `docs/`, `tools/`.
- **Commits/PRs:** imperative mood, scoped changes, link issues, include test/build evidence. After opening a PR, drive it to green: address automated review comments (Copilot/Codex/Claude) and fix failing CI, pushing follow-ups until checks pass and threads resolve. Useful: `gh pr checks <num> --watch`, `gh pr view <num> --comments`.
- **Formatting/lint:** `clang-format` (`.clang-format`), `codespell` (`.codespellrc`); run `pre-commit run --all-files` before submitting.

## Bootstrap

```bash
sudo apt-get install -y gcc-arm-none-eabi          # ARM toolchain (2-5 min, one-time)
python3 tools/get_deps.py [FAMILY|-b BOARD]        # fetch deps into lib/, hw/mcu/ (<1 s)
. $HOME/code/esp-idf/export.sh                     # Espressif only: before any build/flash/monitor
```

## Build

Single example (CMake+Ninja, recommended, 1-3 s):
```bash
cd examples/device/cdc_msc && mkdir -p build && cd build
cmake -DBOARD=raspberry_pi_pico -G Ninja -DCMAKE_BUILD_TYPE=MinSizeRel .. && cmake --build .
```

All examples for a board (15-20 s; some objcopy failures are non-critical). The build dir **must** be `cmake-build-<board>` — HIL tests expect that exact name:
```bash
cd examples
cmake -B cmake-build-raspberry_pi_pico -DBOARD=raspberry_pi_pico -G Ninja -DCMAKE_BUILD_TYPE=MinSizeRel . && cmake --build cmake-build-raspberry_pi_pico
```

- **Make:** `cd examples/device/cdc_msc && make BOARD=raspberry_pi_pico all`
- **Espressif** (ESP-IDF examples only, e.g. `cdc_msc_freertos`): after `export.sh`, `cd examples/device/cdc_msc_freertos && idf.py -DBOARD=espressif_s3_devkitc build`
- **Options** (CMake `-D…` / Make `…=…`): `CMAKE_BUILD_TYPE=Debug`/`DEBUG=1`; `LOG=2` (`LOGGER=rtt` for RTT); `RHPORT_DEVICE=1`; `RHPORT_DEVICE_SPEED=OPT_MODE_FULL_SPEED`

## Flash

```bash
ninja cdc_msc-jlink       # CMake; Make: make BOARD=<board> flash-jlink
ninja cdc_msc-openocd     # CMake; Make: make BOARD=<board> flash-openocd
ninja cdc_msc-uf2         # CMake; Make: make BOARD=<board> all uf2
ninja -t targets          # list CMake targets
```
Espressif (after `export.sh`): `idf.py -DBOARD=<board> flash` / `… monitor`.

## GDB Debugging

Look up `JLINK_DEVICE` / `OPENOCD_OPTION` in `hw/bsp/*/boards/*/board.cmake` (CMake) or `board.mk` (Make).

Terminal 1 — start a gdbserver:
```bash
JLinkGDBServer -device stm32h743xi -if SWD -speed 4000 -port 2331 -nogui   # JLink → :2331
openocd -f interface/stlink.cfg -f target/stm32h7x.cfg                     # OpenOCD → :3333
openocd -f interface/cmsis-dap.cfg -f target/rp2040.cfg -c "adapter speed 5000"   # rp2040/rp2350
```
Terminal 2 — connect (`<port>`: 2331 JLink, 3333 OpenOCD):
```bash
arm-none-eabi-gdb build/your_app.elf
(gdb) target remote :<port>    # then: monitor reset halt → load → continue
```
**RTT:** build `LOG=2 LOGGER=rtt`, run JLinkGDBServer with `-RTTTelnetPort 19021`, then `JLinkRTTClient` (`timeout 20s JLinkRTTClient > rtt.log` for non-interactive capture).

## Testing

**Unit (Ceedling, Unity+CMock, ~4 s):**
```bash
sudo gem install ceedling
cd test/unit-test && ceedling test:all        # or ceedling test:test_fifo
```

**HIL (2-5 min):** invoke the `hil` skill (`.claude/skills/hil/SKILL.md`) — local vs remote mode, config selection, SSH copy steps, debugging. Requires pre-built examples (Build → "All examples for a board").

## Documentation

Sphinx docs in `docs/` (`.rst`, or `.md` via MyST). Use the `build-doc` skill (`.claude/skills/build-doc/SKILL.md`) to build/preview locally and regenerate auto-generated files (`tools/gen_doc.py` + `tools/gen_presets.py`) after adding a board or dependency.

## Code Size Metrics

Verify size impact before committing with the `code-size` skill (`.claude/skills/code-size/SKILL.md`) — it wraps `tools/metrics_compare_base.py` for the base-vs-branch worktree + build + compare. Scopes: single example (`-e device/cdc_msc -b <board>`, add `--bloaty`), all examples on a board (`-b <board>`), or all arm-gcc CI families (`--ci`). Reports land in `cmake-metrics/<board>/metrics_compare.md` (and `_combined/` for `--ci`).

## Static Analysis (PVS-Studio)

Use the `pvs` skill (`.claude/skills/pvs/SKILL.md`) — it builds the examples with an exported `compile_commands.json` and runs SAST + MISRA C:2023/C++:2008 for a board, emitting readable + SARIF output (~10-30 s). The examples build exports `compile_commands.json` by default.

## Validation After Changes

1. `pre-commit run --all-files` — format, spell, unit tests (10-15 s).
2. Build at least one board's full example set (Build → "All examples for a board") for modules you touched.
3. Run relevant unit tests; add fuzz/HIL coverage for parsers or protocol state machines.

**Boards good for local testing:**
- `stm32f407disco` — no external SDK
- `raspberry_pi_pico` — Pico SDK required
- Others: see `hw/bsp/FAMILY/boards/`

Device examples need real hardware to validate runtime behavior; must at least build.

## Release

Cutting a release — version bump, regenerated files, the per-release changelog, validation, and the maintainer's commit/tag/GitHub-release — is handled by the `make-release` skill (`.claude/skills/make-release/SKILL.md`).

## References

- MCU reference manuals, datasheets, schematics: before answering register/bitfield/pinout/errata/timing questions from memory or the web, use the `read-doc` skill (`.claude/skills/read-doc/SKILL.md`) to search and read them from `$HOME/Documents/calibre-library` (skill no-ops if the library is absent).
- Supported MCUs/boards: `hw/bsp/` and `docs/reference/boards.rst`.
- USB classes: `src/class/{cdc,hid,msc,audio,…}/` — each has `*_device.c` and `*_host.c`.
- Key files: `src/tusb.h`, `src/tusb_config.h`, `tools/get_deps.py`, `tools/build.py`, `test/unit-test/project.yml`.

## Common Build Issues

- Missing compiler → install `gcc-arm-none-eabi`.
- Missing deps → `python3 tools/get_deps.py FAMILY`.
- Unknown board → check `hw/bsp/FAMILY/boards/`.
- `objcopy` errors in full builds are often non-critical; retry the single example.
