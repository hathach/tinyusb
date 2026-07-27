---
name: etm-trace
description: Use when you need instruction-level execution data from real hardware via a SEGGER J-Trace — cycle-accurate hot-function profiling, on-target code coverage, or raw instruction history (e.g. what ran right before a fault/hang) — beyond what logs, GDB, or DWT PC-sampling can answer. Covers unattended (headless) capture and analysis on ETM-capable TinyUSB boards.
---

# etm-trace — unattended ETM instruction trace via J-Trace + Ozone

Streams full instruction (ETM) trace from a board wired to a SEGGER J-Trace,
headlessly: no GUI, scripted end to end. Produces hot-function profile, code
coverage, and optionally the raw instruction history.

| Skill           | Answers                                                                    |
|-----------------|----------------------------------------------------------------------------|
| `usbmon`        | what the host actually exchanged (URBs)                                    |
| `target-debug`  | what the target did (logs, driver state, sampled PCs)                      |
| `usb-sniffer`   | what crossed the wire                                                      |
| **`etm-trace`** | **exactly which instructions executed, when** (profile, coverage, history) |

Use `target-debug`'s DWT PC-sampling for a quick statistical profile; use
this skill for exact counts, coverage, or instruction-by-instruction history.

## Requirements

- J-Trace on USB (`lsusb -d 1366:1020`) wired to the board's trace header;
  select it by J-Link USB **nickname** (this rig: `jtrace`) — never commit
  serials.
- **Physical setup is per-board and exclusive** (one J-Trace, moved between
  boards; some rigs are fly-wired): unless the user just asked for trace on
  this board or your task states it is wired, **confirm with the user** that
  the J-Trace is connected to the target before flashing or capturing.
- `ozone` on PATH (≥ V3.38 for the automation socket) and `xvfb-run`.
- Firmware built with **`-DTRACE_ETM=1`** (BSP trace-pin + trace-clock init).
- Boards with a reference `hw/bsp/*/boards/<board>/ozone/*.jdebug` work out of
  the box (`ls` that glob for the list); others fall back to `JLINK_DEVICE`
  from `board.cmake` + default trace config. Verified boards: `boards.md` in
  this skill directory.

## Rig discipline

- One probe, one client: quit interactive Ozone/JLinkExe/GDB on the probe
  first. Kill only processes you started — if it's held by someone else's
  session (check `fuser /dev/bus/usb/<bus>/<dev>`), surface it and ask. The
  capture script uses automation port **19201**, never an interactive Ozone's
  19200.
- Hold the board lock (see the `hil` skill):
  `python3 test/hil/board_lock.py hold <board> --reason "etm capture"`.
- Committed `hw/bsp/**/ozone/*.jdebug` are the maintainer's interactive
  projects — automation never opens them (Ozone rewrites project files); the
  script generates a throwaway project.
- The default capture reflashes and resets the target (`--attach` doesn't).

## Capture and analyze

```bash
# 1. Build with trace support:
cd examples && cmake -B cmake-build-<board> -DBOARD=<board> -G Ninja \
  -DCMAKE_BUILD_TYPE=MinSizeRel -DTRACE_ETM=1 . \
  && cmake --build cmake-build-<board> --target <example>

# 2. Capture (all options + defaults: etm_capture.py --help):
python3 .claude/skills/etm-trace/scripts/etm_capture.py \
  --board <board> --probe jtrace --duration-ms 10000 --out <dir>

# 3. Analyze (hot functions, coverage, hottest lines, optimization hints):
python3 .claude/skills/etm-trace/scripts/etm_profile.py <dir> --elf <elf>
```

Every capture — TinyUSB firmware or vendor demo — goes through
`etm_capture.py`; extend it when a board needs something new, never
hand-roll Ozone drivers.

Choosing capture flags (semantics in `--help`):
- fresh-boot profile/coverage: defaults (flash + reset + trace from startup)
- narrowing debug on a LIVE target: `--attach` — no reflash/reset (flashed
  firmware must match `--elf` and be TRACE_ETM-built)
- raw history: `--trace-csv` (~80 MB/1M instructions) — when sequence/timing
  matters, e.g. feeding `--isr`
- stream dies (overflow/unknown-packet): `--no-timestamps`, then reduce the
  core clock (`boards.md`); marginal wiring: sweep `--trace-timing`,
  isolate lines with `--trace-width`. "capture OK" requires nonzero profile
  totals — silence (no trace at all) fails with its own error
- deeper data: `--profile-lines-csv` (hottest lines), `--profile-insts-csv`
  (branch bias), `--sample "expr,.."` (data sampling), `--power` (probe-powered
  targets only), `--os-plugin` (RTOS timeline), `--trace-only` (experimental,
  see Warnings)
- non-TinyUSB targets: `--device` + `--elf`, plus `--jlink-script` when the
  firmware doesn't init the trace pins

First trace on a board — or after any rewiring — is a bring-up, not a plain
capture: follow "Adding a new board" below (vendor example first).

Analyzer: `--isr ENTRY[,BODY..]` gives ISR min/median/avg/worst from a
`--trace-csv` capture with timestamps (fast-enumerating boards need a short
no-eviction run); `--exclude REGEX` drops idle/poll loops from the load
ranking.

Outputs in `<dir>`: `code_profile.txt` (run/fetch counts + coverage); on
request `itrace.csv`, `profile_lines.csv`, `profile_insts.csv`, `samples.csv`,
`power.csv`; `session.log` / `ozone_console.log` / `jlink.log` as evidence.

## Reading results

- **Load %** = share of instruction **fetches** — Ozone has no per-function
  time; time comes only from itrace timestamps (`--isr`, time-share table).
- ISR timing: sub-µs values are approximate (interpolated timestamps — hence
  the SysTick calibration); instruction counts are exact. Time-share ≫
  instruction-share = stalled/waiting (e.g. slave-mode FIFO at wire pace).
- "Fully covered" needs both branch directions — 100% is not expected from an
  idle run.
- itrace timestamps scale by `VAR_TRACE_CORE_CLOCK` (from the board reference;
  `--core-clock` overrides): ordering is exact, absolute times approximate.
- A ms+ "largest gap" or `Trace overflow detected` beyond the startup burst =
  lost packets — reduce the core clock or trace a quieter phase.
- One `Invalid trace timestamp` line at `Debug.Halt` is a normal decoder
  artifact.
- `Unknown trace data packet … Trace collection stopped!` = stream dead from
  that point (the script exits non-zero): retry with `--no-timestamps`, then
  reduce the core clock.

## Timing

- Capture ≈ `--duration-ms` + 15 s overhead; add ~5 s per 1M instructions with
  `--trace-csv`. Bash timeout: duration + 120000 ms.
- Analyzer: < 5 s for a 2M-row itrace.csv.

## Warnings

- **Trace starts at `trace_etm_init()`**, not at reset: earlier `board_init()`
  code shows as never-executed and Ozone logs `No trace clock present` — both
  expected. Trace-from-reset needs a SEGGER J-Link script (`.pex`) instead of
  firmware init.
- Never commit capture output (`itrace.csv` can exceed 100 MB) — keep `--out`
  in scratchpad/`/tmp`; `*.jdebug.user` files stay untracked.
- The automation socket can't evaluate symbolic constants (`EXPORT_AS_CSV`):
  the scripts send numeric/plain commands only — keep it that way when
  extending them (UM08025 §6.7).
- Without `xvfb-run`, Ozone opens on `DISPLAY` and steals keyboard focus.
  Ozone has no `--help`/`--version` — any such probe opens the GUI; check
  with `which ozone` only.
- **`--trace-only` is experimental**: ETM start/stop comparators are scarce
  and erratic — low-rate handler windows may silently not record, adjacent
  instructions leak in, timestamps are invalid across gaps, and the profile
  becomes share-of-traced-stream. Use only for instruction-exact inventories
  of high-rate symbols; for ISR timing use full trace + `--isr`.

## Adding a new board

Bring-up ladder — each step gates the next:

1. **Docs before hardware** (calibre library first, then vendor site): board
   manual, schematics, MCU reference manual. Establish the trace clock
   source and max — chip side and probe side (J-Trace PRO Cortex-M tops out
   at a 150 MHz trace clock) — the pins carrying TRACE_CLK/D0-D3 (read the board's
   debug-connector table — boards often route trace on alternate pins), and
   required rework (jumpers, solder bridges, 0 Ω resistors to add/remove).
   Hunt shared-net hazards: PHYs or other active drivers on trace nets,
   boot straps, connector stubs.
2. **Confirm with the user before any hardware change**: present the rework
   findings as **[ACTION]** items and wait — the user solders/jumpers, you
   verify afterward.
3. **Vendor example before TinyUSB**: fetch SEGGER's trace example for the
   same/similar MCU
   (<https://www.segger.com/products/debug-probes/j-trace/technology/tested-devices/>)
   and run it with `--device <MCU> --elf <demo ELF> --jlink-script
   <demo .pex>`. Streaming proves the physical path — and only that: demo
   firmware often runs reset-default clocks (the RA6M5 one traces at a few
   MHz), so its success says nothing about your target's trace rate. The
   example may target a different board (the LPC4357 one is tested on a
   Keil MCB4300), so silence isn't final proof — but its J-Link
   script/config is often borrowable.
4. **TinyUSB support**: `trace_etm_init()` in the family BSP — mux trace
   pins AFTER the final core-clock switch, enable the trace clock, enable
   any funnel between ETM and TPIU; committed `ozone/*.jdebug` reference,
   plus a `.JLinkScript` declaring off-ROM-table CSTF/TMC/TPIU (addresses
   from the vendor demo's script); build with `TRACE_ETM=1`, validate with
   `--board <board>`.
5. **Still silent or corrupt?** In order: chip-side register audit (pinmux,
   TPIU, ETM, DEMCR — and EVERY funnel in the path; an unprogrammed funnel
   reads register-perfect and eats the stream), physically re-seat both
   connector ends, then SEGGER's procedure (UM08001): find a stable
   `--trace-timing` at `--trace-width 1`, step up to 2, then 4 (sampling
   default is +2 ns) — then search the
   MCU vendor's application notes and community forums for the chip's trace
   recipe: more than one board's fix lived only in a forum thread.
6. **Board note**: add the table row (core clock, TRACECLK pin + max,
   width, timing, physical setup, TODO for anything left unvalidated) plus
   a caveat bullet — both in `boards.md`.

## Per-board notes

Every validated board has a row (config: core clock, TRACECLK, width, timing,
physical setup, TODO) and a caveat entry in `boards.md` (same directory) —
**read a board's row and caveat before capturing on it**; new validations add
both. Timing semantics and clock columns are explained at the top of that file.

## References

- Ozone manual (UM08025, automation socket §6.7, project commands §7):
  <https://www.segger.com/downloads/jlink/UM08025_Ozone.pdf> — V3.50, same as
  the installed Ozone (web is rev 1 vs the local copy's rev 0; the local PDF
  under /opt/SEGGER/Ozone_V350/Doc remains the offline fallback).
- J-Link / J-Trace manual (UM08001, trace ch. 10, timing troubleshooting):
  <https://kb.segger.com/UM08001_J-Link_/_J-Trace_User_Guide>
