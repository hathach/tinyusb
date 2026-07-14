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
- **Commits/PRs:** imperative mood, scoped changes, link issues, include test/build evidence. After opening a PR, monitor it and drive it to green: address automated review comments (Copilot/Codex/Claude) and fix any failing CI builds, pushing follow-up commits until checks pass and review threads are resolved. Useful: `gh pr checks <num> --watch`, `gh pr view <num> --comments`.
- **Formatting/lint:** `clang-format` (`.clang-format`), `codespell` (`.codespellrc`), run `pre-commit run --all-files` before submitting.

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
cmake -DBOARD=raspberry_pi_pico -G Ninja -DCMAKE_BUILD_TYPE=MinSizeRel ..
cmake --build .
```

All examples for a board (15-20 s; some objcopy failures are non-critical). Use `cmake-build-<board>` as the build dir — HIL tests expect that exact name:
```bash
cd examples
cmake -B cmake-build-raspberry_pi_pico -DBOARD=raspberry_pi_pico -G Ninja -DCMAKE_BUILD_TYPE=MinSizeRel .
cmake --build cmake-build-raspberry_pi_pico
```

Single example with Make:
```bash
cd examples/device/cdc_msc && make BOARD=raspberry_pi_pico all
```

Espressif (only ESP-IDF examples like `cdc_msc_freertos`):
```bash
. $HOME/code/esp-idf/export.sh
cd examples/device/cdc_msc_freertos
idf.py -DBOARD=espressif_s3_devkitc build
```

**Build options** (CMake `-D…` / Make `…=…`):
- Debug: `CMAKE_BUILD_TYPE=Debug` / `DEBUG=1`
- Logging: `LOG=2` (add `LOGGER=rtt` for RTT)
- Root hub port: `RHPORT_DEVICE=1`
- Speed: `RHPORT_DEVICE_SPEED=OPT_MODE_FULL_SPEED`

## Flash

```bash
# JLink
ninja cdc_msc-jlink                                       # CMake
make BOARD=<board> flash-jlink                            # Make

# OpenOCD
ninja cdc_msc-openocd                                     # CMake
make BOARD=<board> flash-openocd                          # Make

# UF2
ninja cdc_msc-uf2                                         # CMake
make BOARD=<board> all uf2                                # Make

ninja -t targets                                          # list CMake targets

# Espressif (after . $HOME/code/esp-idf/export.sh)
idf.py -DBOARD=<board> flash
idf.py -DBOARD=<board> monitor
```

## GDB Debugging

Look up `JLINK_DEVICE` / `OPENOCD_OPTION` in `hw/bsp/*/boards/*/board.cmake` (CMake builds) or `board.mk` (Make builds).

**JLink — Terminal 1:**
```bash
JLinkGDBServer -device stm32h743xi -if SWD -speed 4000 -port 2331 -swoport 2332 -telnetport 2333 -nogui
```

**OpenOCD — Terminal 1:**
```bash
openocd -f interface/stlink.cfg -f target/stm32h7x.cfg
# or with a J-Link interface:
openocd -f interface/jlink.cfg -f target/stm32h7x.cfg
# rp2040/rp2350 via CMSIS-DAP:
openocd -f interface/cmsis-dap.cfg -f target/rp2040.cfg -c "adapter speed 5000"
```

**Terminal 2 — connect GDB** (replace `<port>` with `2331` for JLinkGDBServer or `3333` for OpenOCD):
```bash
arm-none-eabi-gdb /tmp/build/firmware.elf
(gdb) target remote :<port>
(gdb) monitor reset halt
(gdb) load
(gdb) break main         # optional, to stop at entry
(gdb) continue
```

**RTT logging:** build with `LOG=2 LOGGER=rtt`, flash, then run JLinkGDBServer with `-RTTTelnetPort 19021`, and in another terminal `JLinkRTTClient` (pipe to `tee rtt.log` or use `timeout 20s JLinkRTTClient > rtt.log` for non-interactive capture).

## Testing

**Unit (Ceedling, Unity+CMock, ~4 s):**
```bash
sudo gem install ceedling
cd test/unit-test && ceedling test:all        # or ceedling test:test_fifo
```

**HIL (2-5 min):** invoke the `hil` skill (`.claude/skills/hil/SKILL.md`) for the full procedure (local vs remote mode, config selection, SSH copy steps, debugging tips). Requires pre-built examples — see Build → "All examples for a board".

## Documentation

Sphinx docs in `docs/` (reStructuredText `.rst` or Markdown `.md` via MyST). Use the `build-doc` skill (`.claude/skills/build-doc/SKILL.md`) to build/preview locally (`sphinx-build`) and to regenerate auto-generated files (`tools/gen_doc.py` + `tools/gen_presets.py`) after adding a board or dependency.

## Code Size Metrics

Verify size impact before committing. Invoke the `code-size` skill (`.claude/skills/code-size/SKILL.md`) — it wraps `tools/metrics_compare_base.py` to handle the base-vs-branch worktree + build + compare flow.

Quick reference:
```bash
# Single example, one board:
python3 tools/metrics_compare_base.py -b raspberry_pi_pico -e device/cdc_msc
# Add --bloaty for section/symbol breakdown.

# All examples, one board:
python3 tools/metrics_compare_base.py -b raspberry_pi_pico

# All arm-gcc CI families combined (pre-merge sweep, 4-8 min):
python3 tools/metrics_compare_base.py --ci
```

Reports land in `cmake-metrics/<board>/metrics_compare.md` (per-board) and `cmake-metrics/_combined/metrics_compare.md` (with `--combined`/`--ci`).

## Static Analysis (PVS-Studio)

Requires `compile_commands.json`, which the examples build exports by default
(`hw/bsp/family_support.cmake` sets `CMAKE_EXPORT_COMPILE_COMMANDS ON`). The
`pvs` skill (`.claude/skills/pvs/SKILL.md`) wraps the build + analyze flow for a
board; the commands below are the underlying steps.

```bash
# Whole project:
pvs-studio-analyzer analyze \
  -f examples/cmake-build-raspberry_pi_pico/compile_commands.json \
  -R .PVS-Studio/.pvsconfig \
  -o pvs-report.log -j12 \
  --security-related-issues \
  --misra-c-version 2023 --misra-cpp-version 2008 --use-old-parser

# Specific files: -S takes a plaintext list (one path per line), not paths directly:
printf 'src/foo.c\nsrc/bar.c\n' > files.txt
pvs-studio-analyzer analyze \
  -f examples/cmake-build-raspberry_pi_pico/compile_commands.json \
  -R .PVS-Studio/.pvsconfig \
  -S files.txt \
  -o pvs-report.log -j12 \
  --security-related-issues \
  --misra-c-version 2023 --misra-cpp-version 2008 --use-old-parser

plog-converter -a GA:1,2 -t errorfile pvs-report.log     # view results
```

Takes ~10-30 s. (`--dump-files` adds preprocessed `.PVS-Studio.i/.cfg` dumps next
to every source for false-positive debugging — omit it for normal runs.)

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

- MCU reference manuals, datasheets, schematics: `$HOME/Documents/calibre-library`.
- Supported MCUs/boards: `hw/bsp/` and `docs/reference/boards.rst`.
- USB classes: `src/class/{cdc,hid,msc,audio,…}/` — each has `*_device.c` and `*_host.c`.
- Key files: `src/tusb.h`, `src/tusb_config.h`, `tools/get_deps.py`, `tools/build.py`, `test/unit-test/project.yml`.

## Common Build Issues

- Missing compiler → install `gcc-arm-none-eabi`.
- Missing deps → `python3 tools/get_deps.py FAMILY`.
- Unknown board → check `hw/bsp/FAMILY/boards/`.
- `objcopy` errors in full builds are often non-critical; retry the single example.
