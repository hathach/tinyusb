# TinyUSB Agent Instructions

TinyUSB is a cross-platform USB Host/Device stack for embedded systems: memory-safe (no dynamic allocation) and thread-safe (ISR events deferred to task context).

Reference these instructions first; fall back to search/bash only when reality diverges.

## Behavioral Guidelines

Bias toward caution over speed. For trivial tasks, use judgment.

- **Think first** — state assumptions; ask if unclear; present alternatives instead of picking silently.
- **Simplicity** — no features, abstractions, flexibility, or error handling beyond what was asked. If 200 lines could be 50, rewrite.
- **Surgical changes** — touch only what the task requires; match existing style; don't refactor working code; mention unrelated dead code rather than deleting it. Remove only orphans *your* changes created.
- **Goal-driven** — turn tasks into verifiable goals ("write failing test, make it pass"). For multi-step work, state a brief `step → verify` plan.

## Ground Rules

- **Language/style:** C99, 2-space indent (no tabs), snake_case helpers, `UPPER_CASE` macros. Public APIs use `tud_`/`tuh_`; macros use `TU_`. Headers self-contained with `#if CFG_TUSB_MCU` guards.
- **Safety:** no dynamic allocation; defer ISR work to task context; use `TU_ASSERT()` for error checks; always check return values; include order: C stdlib → tusb common → drivers → classes.
- **Layout:** `src/` core, `hw/{mcu,bsp}/` MCU+BSP, `examples/{device,host,dual}/`, `test/{unit-test,fuzz,hil}/`, `docs/`, `tools/`.
- **Commits/PRs:** imperative mood, scoped changes, link issues, include test/build evidence.
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

```bash
pip install -r docs/requirements.txt
cd docs && sphinx-build -b html . _build        # ~2.5 s
```

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

Requires `compile_commands.json` (CMake `-DCMAKE_EXPORT_COMPILE_COMMANDS=ON`).

```bash
# Whole project:
pvs-studio-analyzer analyze \
  -f examples/cmake-build-raspberry_pi_pico/compile_commands.json \
  -R .PVS-Studio/.pvsconfig \
  -o pvs-report.log -j12 --dump-files \
  --misra-c-version 2023 --misra-cpp-version 2008 --use-old-parser

# Specific files (add one or more `-S <file>`):
pvs-studio-analyzer analyze \
  -f examples/cmake-build-raspberry_pi_pico/compile_commands.json \
  -R .PVS-Studio/.pvsconfig \
  -S src/foo.c -S src/bar.c \
  -o pvs-report.log -j12 --dump-files \
  --misra-c-version 2023 --misra-cpp-version 2008 --use-old-parser

plog-converter -a GA:1,2 -t errorfile pvs-report.log     # view results
```

Takes ~10-30 s.

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

**Do not commit automatically — leave changes for maintainer review.**

1. Bump version at top of `tools/make_release.py`.
2. Run `python3 tools/make_release.py` to refresh: `src/tusb_option.h`, `repository.yml`, `library.json`, `sonar-project.properties`, `docs/reference/boards.rst`, `hw/bsp/BoardPresets.json`.
3. Changelog `docs/info/changelog.rst`:
   - `git log <last-tag>..HEAD --oneline` for commit list.
   - Read merged PRs for context (`gh pr view`, or github MCP tools).
   - Follow existing format: version + `======` underline, italic date, sections (General, API Changes, DCD & HCD, Device Stack, Host Stack, Testing), RST inline code for symbols.
4. Validate: `ceedling test:all`, build `cdc_msc` for `stm32f407disco`, review `git diff --stat`.
5. Leave unstaged. Maintainer commits `Bump version to X.Y.Z`, then: `git tag -a vX.Y.Z -m "Release X.Y.Z" && git push origin <branch> vX.Y.Z`. Create GitHub release from tag.

## References

- MCU reference manuals, datasheets, schematics: `$HOME/Documents/Calibre Library`.
- Supported MCUs/boards: `hw/bsp/` and `docs/reference/boards.rst`.
- USB classes: `src/class/{cdc,hid,msc,audio,…}/` — each has `*_device.c` and `*_host.c`.
- Key files: `src/tusb.h`, `src/tusb_config.h`, `tools/get_deps.py`, `tools/build.py`, `test/unit-test/project.yml`.

## Common Build Issues

- Missing compiler → install `gcc-arm-none-eabi`.
- Missing deps → `python3 tools/get_deps.py FAMILY`.
- Unknown board → check `hw/bsp/FAMILY/boards/`.
- `objcopy` errors in full builds are often non-critical; retry the single example.
