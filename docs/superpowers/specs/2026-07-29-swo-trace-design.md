# swo-trace skill — design

Status: draft for review · Session: swo-trace · Branch: `claude/swo-trace` (base: `claude/hil-pool-check`)

## What it is

Unattended SWO capture + decode on the bench: ISR timing, PC profiling, DWT data
trace, and `LOGGER=swo` text — read off a *running* target with **no
instrumentation of the code under test**. The only firmware change is one BSP
pin-mux behind a build flag (`-DTRACE_SWO=1`); nothing in the ISRs, nothing in
the driver being debugged.

Promotion of target-debug's SWO sections into a standalone skill per the
promotion criteria (tooling exists → 3/4). Routed from target-debug's existing
capture-channel rows, not a new question row.

| In scope                                            | Out of scope                                    |
|-----------------------------------------------------|-------------------------------------------------|
| Cortex-M3/M4/M7/M33/M55/M85 with the SWO pin wired  | M0/M0+/M23, RISC-V, Xtensa (84 of 236 boards)   |
| Exception trace, hw PC sampling, profiling counters | RP2350: ITM but parallel-trace-only → etm-trace |
| DWT data trace (up to NUMCOMP watchpoints)          | Instruction-level trace → etm-trace             |
| ITM stimulus ports 0-31 text (`LOGGER=swo` demux)   | Task/scheduler semantics → sysview              |

## Why not RTT (the logger comparison, condensed)

- RTT masks interrupts around every write (`SEGGER_RTT_Conf.h:152-175`, BASEPRI
  0x20) — it adds jitter to the very ISR latencies being measured. ITM stimulus
  writes are single stores; per-context ports need no lock at all.
- RTT's control block is RAM a wild pointer can trash; SWO bytes leave the chip.
- SWO text rides the same stream as DWT packets → one timeline: a log line can
  be placed *inside* an ISR. RTT can never correlate.
- RTT stays the default logging channel; swo-trace is for timing-sensitive
  bugs, RAM-integrity doubts, ST-Link benches, and log+timing correlation.

## Repo footprint

| Path                                    | Change                                                                 |
|-----------------------------------------|------------------------------------------------------------------------|
| `.claude/skills/swo-trace/SKILL.md`     | new — structure-model: sysview SKILL.md                                |
| `.claude/skills/swo-trace/scripts/`     | new — `swo_capture.py`, `swo_decode.py`                                |
| `hw/bsp/family_support.cmake`           | +4 — `TRACE_SWO` option, exact sibling of the `TRACE_ETM` block (:469) |
| `hw/bsp/samd5x_e5x/family.c`            | +~14 — `trace_swo_init()`: GCLK47 gate + PB30 → peripheral H            |
| `hw/bsp/stm32u5/family.c`               | +~10 — `trace_swo_init()`: PB3 → AF0 + DBGMCU trace enable              |
| `.claude/skills/target-debug/SKILL.md`  | SWO + DWT-data-trace sections (:87-125) → routing lines; **:127-149 "Enabling SWO" (H743/OpenOCD) kept verbatim**; intrusiveness rows :60-61 point here |
| `.claude/agents/target-debugger.md`     | +1 capture-channel table row                                            |

SAME54 note: `trace_swo_init()` must keep the GCLK channel-47 gate (same as
`trace_etm_init()`, `samd5x_e5x/family.c:96-97`) — the CM4 trace unit clock
feeds SWO too; without it the pin muxes fine and stays silent.

Conflict note: `target-debug/SKILL.md` is also edited by unmerged branch
`claude/add-systemview-debug`; whichever merges second resolves a small known
conflict in the SWO sections.

## swo_capture.py

```
swo_capture.py --board <name> --probe <nick|serial> --backend {jlink,openocd}
               --duration-ms 3000 --out cap.bin [--protocol {uart,manchester}]
               --caps
               [--exc-trace] [--pc-sample[=CYCTAP:POSTPRESET]]
               [--profile[=cpi,exc,sleep,lsu,fold,cyc]]
               [--itm-ports 0-31|0,1,2] [--timestamps {local,local+global}]
               [--watch <addr|sym>[/<mask-bits>][:{rw,ro,wo}][:{pc,val,pcval,offset}]]
               [--cycle-match <n>]
```

- `--caps`: read `DWT_CTRL[31:24]` (NUMCOMP, NOTRCPKT, NOEXTTRIG, NOCYCCNT,
  NOPRFCNT — RO capability bits, ARMv7-M ARM Table C1-29) and report what the
  silicon supports. First rung of the capability ladder: interrogate, don't
  trust tables.
- Full DWT/ITM/TPIU feature surface: exception trace (EXCTRCENA bit 16), PC
  sampling with prescaler (PCSAMPLENA bit 12 + CYCTAP/POSTPRESET), profiling
  counters (bits 17-22, 256-cycle event packets), data trace ×NUMCOMP with
  `DWT_MASKn` ranges and **read-only/write-only access filtering** (FUNCTION
  0b1100/0b1101/0b1110/0b1111 per Table C1-21 — corrects target-debug's "no
  write-only encoding" claim), EMITRANGE offset packets, CYCMATCH on COMP0,
  ITM ports 0-31, local + global timestamps, sync (SYNCTAP), UART or
  Manchester pin encoding (TPIU SPPR).
- Backends: **jlink** (JLinkExe command file, `SWOStart <speed>` always
  explicit — autodetect fails headless) and **openocd** (`stlink-dap.cfg` +
  `<target>.swo configure … -output`, `itm ports on`). Probe resolved by
  nickname/serial from `test/hil/*.json`.
- Always restores DWT_CTRL and disarms comparators on exit including Ctrl-C
  (stale comparators survive reflash).
- Refuses capability-ladder-excluded boards before touching hardware; 0 bytes
  captured → triage split (pin not wired / not muxed / TRACE_SWO not built).

## swo_decode.py

```
swo_decode.py cap.bin --elf f.elf [--all|--exceptions|--pc|--data|--text] --cpu-hz <hz>
```

- One packet walker per ARMv7-M ARM Appendix D4 (Tables D-2/D-3) decoding
  every packet type — sync, overflow, LTS1/LTS2, GTS1/GTS2, extension,
  instrumentation (port + SS size), event counter (ID0), exception trace
  (ID1), PC sample + PC sleep (ID2), data-trace PC/offset/value (IDs 8-23) —
  so unknown headers are skipped by size, never desyncing the stream. One
  decoder serves both backends (both capture raw SWO UART bytes).
- Local-timestamp accumulation → µs is the correctness-critical path
  (enter/exit alone gives only ordering). Symbolication via one batched
  `arm-none-eabi-addr2line` call.
- Report blocks: `EXCEPTIONS` (per-IRQ n/p50/p99/max/rate), `PC` (histogram +
  sleep fraction), `PROFILE` (CPI/exc/sleep/LSU/fold), `DATA` (CSV per
  comparator: t, value, PC-or-offset), `TEXT` (per-port; port 0 =
  `LOGGER=swo`), `INTEGRITY` (overflow + sync counts — always printed;
  overflow ≠ 0 warns timing numbers are suspect).

## Board coverage model

SKILL.md carries a capability ladder + probe-it recipe, and a table of
hardware-verified boards only (no 236-row file):

1. Core M0/M0+/M23/RISC-V/Xtensa → no ITM, stop.
2. RP2350 → parallel trace port only, no SWO pin → etm-trace.
3. Otherwise `swo_capture.py --caps`, then a short capture: bytes → go;
   stuck at 0 → wired/muxed/built triage.

Survey ground truth to preserve in the skill (from the 2026-07-29 scan of 236
boards + 325 board PDFs): Nucleo-144s route PB3 → ST-LINK ungated; **Nucleo-64s
gate SWO behind a default-open solder bridge** (~9 boards need a blob);
STM32N6 uses PB5 not PB3; `uno_r4` repurposes P109 (SWO) as UART TX;
Adafruit/Teensy/WeAct boards lack SWO on any header (flying lead only);
`frdm_mcxa153` is already muxed in `pin_mux.c`. Full out-of-the-box list is in
the appendix below; SKILL.md gets only the verified table.

## Validation (all on this htpc bench)

Probes: J-Trace PRO (`jtrace`) + STLINK-V3 (`0013000B3156501920323443`).
Workload: `cdc_msc_freertos` under CDC bulk load (sysview's traffic one-liner).

| Feature                          | same54_xplained (jlink)                        | stm32u575nucleo (openocd) |
|----------------------------------|------------------------------------------------|---------------------------|
| `--caps` probe                   | NUMCOMP=4 expected                             | confirm M33 values        |
| Exception trace + timestamps     | **golden: IRQ 96/98/99 = 21-36 µs, p50 ~34 µs** | second backend            |
| PC sample + sleep fraction       | top symbols plausible (dcd_int_handler…)       | ✓                         |
| Profiling counters               | CPI/LSU/fold non-zero, sleep tracks idle       | ✓                         |
| Data trace ×4, wo-filter, MASK   | rw floods on polled var, wo doesn't — the proof | ✓                         |
| EMITRANGE offsets, CYCMATCH      | ✓                                              | —                         |
| ITM multi-port + `LOGGER=swo`    | port 0 text + synthetic multi-port             | ✓                         |
| Global timestamps                | ✓                                              | —                         |
| Manchester                       | ✓                                              | ✓                         |
| Overflow handling                | force via max PC rate, no prescaler            | ✓                         |

The golden cross-check gates the timestamp decoder: sysview measured THIS
board under THIS load at 21-36 µs (p50 ~34 µs) for the USB ISRs. Disagreement
= decoder bug, not board variance. U575 bring-up needs its own traceclk
derivation (M33; the H743 recipe's 400 MHz c_ck does not transfer).

Deferred: re-validating the H743/OpenOCD recipe — it stays in target-debug,
verbatim, labeled as verified there.

## Process (TDD-for-skills)

1. **RED** — baseline subagent WITHOUT the skill: "measure USB IRQ duration
   and find the hot spot on same54_xplained with zero firmware changes,
   headless." Document gaps verbatim (expected: SWOStart speed, timestamp
   decoding, DWT bits, GCLK47).
2. Build tooling + validate on hardware (matrix above) — ground truth first.
3. **GREEN** — write SKILL.md addressing the baseline gaps; trigger-only
   description.
4. Same scenario WITH the skill; **REFACTOR** the gaps it surfaces.
5. Dogfood end-to-end following only the SKILL.md text.

Traps pre-learned (do not rediscover): explicit SWOStart speed; local
timestamps required for durations; PC-sample overflow at high core clocks
(expose prescaler); captures stay in scratchpad, never committed; one probe
one client; no Claude commit trailers; `claude/` branch prefix.

## Appendix: SWO out-of-the-box survey (2026-07-29)

"Out of the box" = SWO reaches the on-board probe or a fitted debug header, no
soldering. Sources: board schematics/user manuals in the Calibre library, read
in context. Firmware pin-mux may still apply (that is `-DTRACE_SWO`).

Schematic/UM-verified:

| Family      | Boards                                                                                                    | Route                                            |
|-------------|-----------------------------------------------------------------------------------------------------------|--------------------------------------------------|
| ST Disco/DK | stm32f303disco stm32f407disco stm32f411disco stm32f412disco stm32f723disco stm32f769disco stm32l476disco stm32h745disco stm32h747disco stm32h750bdk stm32h573i_dk b_u585i_iot2a | PB3 → ST-LINK + ext header; SWO bridges ON        |
| ST Eval     | stm32h743eval                                                                                             | PB3 TDO/SWO → debug connector                    |
| ST Nucleo-144 | stm32f207nucleo stm32f412nucleo stm32f439nucleo stm32f746nucleo stm32f767nucleo stm32h563nucleo stm32h723nucleo stm32h743nucleo stm32h7s3nucleo stm32u575nucleo stm32u5a5nucleo | PB3 → ST-LINK, ungated                            |
| ST N6       | stm32n6570dk stm32n657nucleo                                                                              | **PB5** → header pin 6                            |
| Microchip   | same54_xplained same70_xplained                                                                           | PB30 / TRACESWO → Cortex Debug pin 6 + EDBG       |
| NXP         | ea4088_quickstart lpcxpresso55s28 lpcxpresso55s69 lpcxpresso54608 lpcxpresso54628 mimxrt1060_evk frdm_mcxa153 frdm_mcxa156 | TDO_SWO / SWO_TRGT → Link2/MCU-Link               |
| Renesas     | ra4m1_ek ra4m3_ek ra6m1_ek ra6m5_ek                                                                       | P109 TDO/SWO → J-Link OB + header                 |
| Nordic      | nrf5340dk                                                                                                 | SWD0_SWO → J-Link OB                              |
| ADI/Maxim   | max32666fthr                                                                                              | P1.15 SWO/TDO → SWD header                        |
| TI          | ek_tm4c123gxl ek_tm4c1294xl                                                                               | PC3 TDO/SWO — ICDI can't stream; ext probe needed |
| Infineon    | xmc4500_relax                                                                                             | P2.1 TDO/SWO → debug connector                    |

Caveat: lpcxpresso1549 wired (PIO1_2 → IF_SWO) but pin doubles as OLED_EN.

Same-design/proxy evidence (expected, not context-read): stm32l496nucleo
stm32l4p5nucleo stm32l4r5nucleo stm32f746disco b_g474e_dpow1 · frdm_k64f (PTA2,
via FRDM-K66F sch) · remaining mimxrt EVKs · lpcxpresso54114 lpcxpresso18s37
lpcxpresso43s67 mcb1800 ea4357 · ra8m1_ek · msp_exp432e401y · xmc4700_relax ·
nrf52840dk/nrf52833dk (TRACE switch SW7, slide not solder).

Excluded despite ITM: Nucleo-64s (default-open SWO bridge: stm32g474nucleo
stm32g491nucleo stm32l412nucleo stm32u545nucleo stm32h503nucleo stm32h533nucleo
stm32wb55nucleo stm32c542nucleo stm32wba_nucleo) · uno_r4 (P109 = UART TX) ·
Adafruit SAMD51/Feather/Teensy/Daisy (no SWO on header) · WeAct/bluepill
boards (PB3 edge pin, flying lead only).
