# sysview — per-board reference

Every board validated with `-DSYSVIEW` has a row below plus, where it has one,
a caveat entry. **Read a board's row AND its caveat before capturing on it**;
a new validation adds both.

`JLINK_DEVICE` is the `--device` value `sysview_record.py` and `sysview_dump.py`
need — the same string `hw/bsp/<family>/boards/<board>/board.cmake` (or the
family's `family.cmake`) sets. `—` means the board has no J-Link device string
because it was captured over OpenOCD or a post-mortem dump instead.

"1.000 s reads as" is timestamp-rate accuracy: ten host-timed 1.000 s silences
were driven and the median gap read back out of the trace, so a rate error shows
up directly as a ratio. `stm32f407disco` is the control — its DWT scale is
unarguably cycles/`SystemCoreClock`. The 1-3% on OpenOCD rows is that route's
record loss inflating measured gaps, not the timers (same TIM2 code as the
J-Link `stm32g0` row). WCH parts are measured differently — `system_ticks` read
twice a known interval apart over `openocd`, run twice (once with no sleep) and
differenced, because the probe halts the core to read memory and that stops the
tick.

"Route" is the route this board was validated on: `J-Link` = live recorder,
`OpenOCD` = `rtt server` streaming, `dump` = post-mortem `dump_image` (last
ring-full only). Commands for each are in `SKILL.md`.

"Buffer" is the effective `SYSVIEW_BUFFER_SIZE` this board is known to need or
get: RAM-rich families default to 65536 in their `family.cmake`
(`SYSVIEW_BUFFER_SIZE_DEFAULT`), small parts override down per board, and
`-DSYSVIEW_BUFFER_SIZE` beats both. Blank means unmeasured on the 4096
fallback — not proven sufficient. If `overflow` climbs during a capture, raise
it regardless of the column.

**WCH boards additionally inherit the family-wide `All WCH parts` caveat below**
— check it as well as the board's own bullet.

| Board                    | Family     | JLINK_DEVICE      | Timestamp source | 1.000 s reads as       | Route   | Buffer | Window | Ovfl  | USB ISR (p50 µs) | tud_task p50/p99 µs |
|--------------------------|------------|-------------------|------------------|------------------------|---------|--------|--------|-------|------------------|---------------------|
| stm32f407disco           | stm32f4    | stm32f407vg       | DWT (control)    | 1.0003 s (+0.03%)      | OpenOCD | 65536  | 16.6 s | 0     | 83, 6.9          | 7.4 / 7.9           |
| stm32l476disco           | stm32l4    | stm32l476vg       | DWT              | 1.0003 s (+0.03%)      | OpenOCD | 4096   | 8.9 s  | 2339  | 83, 16.5         | 17.3 / 45.2         |
| stm32f723disco           | stm32f7    | stm32f723ie       | DWT              | 1.0016 s (+0.16%)      | OpenOCD | 65536  | 16.6 s | 33331 | 83, 7.6          | 7.7 / 8.5           |
| stm32f072disco           | stm32f0    | stm32f072rb       | TIM2 @ 1 MHz     | 1.0022 s (+0.22%)      | OpenOCD | 2048   | 17.2 s | 9495  | 47, 29.0         | 33.0 / 72.0         |
| lpcxpresso11u37          | lpc11      | LPC11U37          | CT32B0 @ 1 MHz   | 1.0038 s (+0.38%)      | OpenOCD | 2048   | 16.9 s | 225   | 38, 33.0         | 43.0 / 143.0        |
| ra4m1_ek                 | ra         | R7FA4M1AB         | DWT              | 1.0097 s (+0.97%)      | J-Link  | 2048   | —      | —     | —                | — (see caveat)      |
| stm32u083nucleo          | stm32u0    | stm32u083rc       | TIM2 @ 1 MHz     | 1.0118 s (+1.18%)      | OpenOCD | 4096   | 16.8 s | 3074  | 24, 21.0         | 23.0 / 69.0         |
| adafruit_fruit_jam       | rp2040     | rp2350_m33_0      | DWT              | 1.0185 s (+1.85%)      | OpenOCD | 65536  | 16.7 s | 24913 | 30, 9.0          | 5.8 / 36.9          |
| stm32g0b1nucleo          | stm32g0    | stm32g0b1re       | TIM2 @ 1 MHz     | 1.0222 s (+2.2%)       | OpenOCD | 4096   | 17.3 s | 16978 | 24, 17.0         | 19.0 / 22.0         |
| stm32h743nucleo          | stm32h7    | stm32h743xi       | DWT              | 1.0259 s (+2.6%)       | OpenOCD | 65536  | 17.4 s | 0     | 117, 4.3         | 4.5 / 4.7           |
| raspberry_pi_pico        | rp2040     | rp2040_m0_0       | `time_us_32`     | 1.0304 s (+3.0%)       | OpenOCD | 65536  | 17.1 s | 0     | 21, 11.0         | 6.0 / 40.0          |
| feather_nrf52840_express | nrf        | nrf52840_xxaa_app | DWT              | 1.0536 s (+5.4%)       | OpenOCD | 65536  | 16.8 s | 29693 | 55, 23.5         | 16.5 / 90.8         |
| metro_m4_express         | samd5x_e5x | ATSAMD51J19       | DWT              | 1.0100 s (+1.00%)      | OpenOCD | 65536  | 2.4 s  | 0     | 99, 12.2         | 15.8 / 62.6         |
| max32666fthr             | maxim      | (family.cmake)    | TMR0 @ 48 MHz    | 1.0005 s (+0.05%)      | OpenOCD | 65536  | 11.6 s | 746   | 18, 8.5          | 5.8 / 61.1          |
| raspberry_pi_pico2       | rp2040     | rp2350_m33_0      | DWT              | validated — see caveat | OpenOCD |        | —      | —     | —                | —                   |
| same54_xplained          | samd5x_e5x | ATSAME54P20       | DWT              | validated end-to-end   | J-Link  |        | —      | —     | —                | — (prose below)     |
| mimxrt1064_evk           | imxrt      | MIMXRT1064xxx6A   | DWT              | validated (dual)       | J-Link  | 65536  | —      | —     | —                | —                   |
| ch32v307v_r1_1v0         | ch32v30x   | —                 | QingKe SysTick   | tick 996.1 Hz (−0.39%) | dump    |        | —      | —     | —                | —                   |
| ch582m_evt               | ch583      | —                 | QingKe SysTick   | tick 997.2 Hz (−0.28%) | dump    | 2048   | —      | —     | —                | —                   |
| nanoch32v203             | ch32v20x   | —                 | QingKe SysTick   | tick 838.6 Hz (−16.1%) | dump    | 2048   | —      | —     | —                | —                   |

`cdc_msc` at `SYSVIEW=4` builds on all 23 arm-gcc/riscv boards in the ci.lan
pool; the rows above are the ones whose timestamp source was measured. Route,
Buffer and the capture columns (Window/Ovfl/ISR/`tud_task`) show the latest
validated capture — the 2026-08-12 full-pool campaign for the OpenOCD rows
(`cdc_msc` `SYSVIEW=4`, `cdc_burst` 15 s, artifacts
`ci.lan:~/sysview-v2/out/campaign-final/`); the rate column keeps each board's
original measurement, some of which predate the route switch. `—` capture
cells: the board was not in the campaign (J-Link-only validations, host-role
pico2, dump-route WCH). Reading the overflow column: zero on 65536 with a fast
probe path; small-buffer boards (2048/4096) drop by design; f723/fruit_jam/
feather lose on drain-path throughput (drain-rate caveat below). metro's short
window is the attach_only mid-stream join (2.4–5.6 s across runs).

- **Killing the SystemView GUI can cost you its registry.** SIGKILLing `systemview` (which the
  capture path does on teardown) has been seen to rewrite `SEGGER_REG_HKEY_CURRENT_USER.xml`
  without its `License` element — a registered key silently disappears and every later launch
  falls back to the SFL dialog. Re-add it via License Manager if headless captures start
  failing. Observed on ci.lan after a long unattended run.

## Per-board caveats

- **Local-board evidence (htpc, OpenOCD route)**: stm32u5 lost 13912 events (~19% of the
  stream) at the 4096 fallback under a 12 s cdc_msc workload — now defaults to 65536. lpc55 lost
  34 on the same route, an order of magnitude lighter, so it keeps the fallback until something
  measures otherwise. `lpcxpresso55s69` has no `lpc55*.cfg` in either OpenOCD build here; a
  hand-rolled attach-only Cortex-M33 target works, but `transport select swd` must be an explicit
  `-c` BEFORE `-f interface/jlink.cfg` or OpenOCD errors "Can't change session's transport".
- **ra4m1_ek no longer links at `SYSVIEW=4`** — the 2026-08-11 build sweep hit
  `region RAM overflowed by 960 bytes` even at `-DSYSVIEW_BUFFER_SIZE=2048`; its
  rate row predates that. Re-validating needs a lower level or freed RAM.
- **nrf5340dk cannot be captured at present**: it HardFaults inside `vTaskStartScheduler()`
  before any task runs — reproduced on a plain non-instrumented build and after a full
  `nrfjprog --recover`, so it is a board/boot issue, not a SystemView one. The RTT control block
  is never written (that happens once `usb_device_task` runs), so no capture route can reach it.
- **Buffer evidence from the dual-role dogfood**: at the default 4096, mimxrt1064_evk lost 96.7%
  of ISR exit contexts at only ~109 Hz, metro_m4_express 99.1% on its EIC vector, and
  adafruit_fruit_jam's captures came back sparse for the same reason. 65536 is the measured-safe
  value on RAM-rich parts; treat a blank Buffer cell as "unmeasured", not "default suffices".
- **Overflow depends on the probe's drain rate, not just the buffer** (post-PCIe-rework
  campaign, 2026-08-11, 15 s cdc_burst): at the same 65536, raspberry_pi_pico and
  stm32h743nucleo captured with zero loss while feather_nrf52840_express lost 73 bursts
  (median ~29k events) over its openocd-jlink path and adafruit_fruit_jam 14 bursts —
  and stm32f407disco went from zero loss to 3-4 small bursts (~830 events, 0.45%) after
  the rig's USB controllers were re-arranged. Re-baseline per board after any bus
  topology change before reading overflow as a firmware regression.
- **Dual-role ISR bracket status**: the outer bracket in the ten shared-vector BSPs remains
  hardware-unexercised — metro_m4_express's only real dual config (MAX3421) compiles the
  shared-vector `tuh_int_handler` call out (`#if CFG_TUH_ENABLED && !CFG_TUH_MAX3421`), routing
  host interrupts via a separate EIC vector, and the other rig dual boards are not
  bracket-family. Doubling is structurally impossible in every rig-buildable config; the bracket
  is covered by the ceedling behavioral test and nm-verified wiring only.

- **ch32v307v_r1_1v0** (also read `All WCH parts` below) — at `SYSVIEW=4` it
  drops off the USB bus under sustained CDC traffic and needs a reset to recover
  (the `SEGGER_SYSVIEW_LOCK` interrupt-masking cost, not a capture-route
  problem). Profile it with light load or a lower `-DSYSVIEW` level.
- **ch32v10x (`ch32v103r_r1_1v0`)** — `-DSYSVIEW` is a configure-time
  `FATAL_ERROR`. The part runs in U-mode where `csrr mstatus` traps, and writing
  the alternate CSR 0x800 directly corrupts the QingKe V3 interrupt-mode config
  (hung the board), so no safe RTT lock exists and unserialized RTT writes would
  produce plausible but corrupt traces. Its WCH-Link also resets the target on
  every attach, wiping the ring, so the post-mortem route is out too.
- **ch582m_evt** — links with essentially zero free RAM at `SYSVIEW=4` even at
  `-DSYSVIEW_BUFFER_SIZE=2048`. It builds; treat any added instrumentation as
  likely to push it over.
- **nanoch32v203** — the −16% is a **board clock bug, not a SystemView one**: a
  build with no SYSVIEW at all measures the same 838.5 Hz, so `board_millis()`
  and every timeout derived from it are 16% slow. `board.cmake` asks for
  `SYSCLK_FREQ_144MHz_HSE`; the tick implies an actual HCLK near 120.7 MHz.
- **All WCH parts** (`ch32v20x`, `ch32v30x`, `ch583`) — the dump route is the
  only capture route TODAY, but the blocker is the OpenOCD driver, not the
  silicon:
  - *Mechanism* (isolated on nanoch32v203, 2026-08-11, unified OpenOCD
    0.12.0+dev-02620): the SDI **attach/examine is harmless** — the CDC stayed
    on the bus through `init` + examination in two controlled runs. Death comes
    when **RTT polling starts**: the fork's custom `wch_riscv` target reads
    memory by halting the hart (`curstate: halted` afterwards; it ignores
    `riscv set_mem_access`, and `sysbus` is unimplemented in the DM), and at
    `polling_interval 1` a halted core cannot service USB — host drops the
    device within a second. ch582m additionally wipes its ring on the
    driver's attach-reset (like ch32v10x).
  - *The silicon CAN stream*: `wlink dump` reads RAM from the RUNNING target
    without disturbing it — stress-proven: 194 consecutive 64-byte reads
    during 12 s of saturated CDC traffic, **42130/42130 echoes**, zero read
    failures, device still enumerated (no x9 corruption manifested either —
    that hazard may be CH569- or hart-mediated-read-specific).
  - *Fork fix implemented* (`hathach/openocd` tinyusb branch, `584faee80`,
    unpushed): `wch_riscv` reads a RUNNING hart via the probe's native bulk
    read (0x03/0x02-0x0c + data EP), preceded by a one-shot AttachChip
    (0x0d/0x02) session refresh — without it running-hart reads return a
    repeated junk word (`0x4003b0c3`/`0xbeef0080` fills). Stable session:
    `init; poll off; reset halt` (examine on the halted core — examine on a
    RUNNING core is the near-always killer, dcsr.cause=haltreq observed);
    `reset run`. Verified: control block read live off the running chip,
    417 read-polls/s with USB enumerated. Running-hart writes: no DIRECT
    primitive is safe (DM abstract word write and probe bulk write both kill
    the firmware, the latter without landing data — and note the probe
    0x01+0x05 sequence is flash-loader staging, not a generic RAM write), but
    **brief halt-write-resume is** (fork `5e33b27c4`): 8 ms per cycle, 10
    cycles under saturated CDC traffic, 49530/49530 echoes — the USB
    peripheral NAKs in hardware while the core is halted.
  - *Streaming status — RULED OUT; root cause is the debug transport, not
    firmware/lock/PM* (fork `60efbf3b3`, root-caused 2026-08-12). The
    decisive A/B: normal and POST_MORTEM firmware, identical stable attach +
    sustained persistent-session reads under CDC load, **both die at t+1.9 s**
    — same lock, same death, so the SystemView lock and the PM overwrite path
    are BOTH exonerated (an earlier "PM wedges under USB load" note was wrong;
    PM firmware alone is flawless: 37361/37361 echoes, WrOff climbing to 3797,
    no debugger). The real mechanism: ARM probes read RAM through the
    **memory-AP**, an autonomous DAP bus master that touches RAM without
    engaging the CPU, so the core runs untouched while the host drains RTT —
    that is why every ARM board streams. WCH's QingKe **SDI single-wire link
    has no memory-AP**; every read goes through the Debug Module's abstract
    commands, and a **persistently active DM corrupts the running core within
    ~2 s during USB traffic** — the same SDI DM-active register corruption
    documented on CH569 (zeroes x9 during USB2-HS). `wlink` survives only by
    attaching/detaching per call (DM never persistently active). Since RTT
    streaming needs a persistent draining session, it cannot work on WCH,
    full stop. **Dump route remains the only WCH capture.** Banked regardless:
    brief non-destructive RAM inspection of a running WCH hart (fork reads,
    good for seconds — enough for a live peek, not a stream). NOTE on asserts:
    `TU_ASSERT` under an attached debugger halts the core via the stock riscv
    `ebreak` (ebreakm set during examine) — that is intended, so you can trap
    and inspect the fault; it is NOT the streaming killer (that is the DM
    corruption above), so do not silence it.
  - *Unified-OpenOCD note kept for the dump/flash paths*: `hathach/openocd`,
    `tinyusb` branch, is the rig's `/usr/local/bin/openocd`; it registers
    `rtt setup`/`rtt start` for WCH targets where stock WCH/MounRiver forks
    only register `rtt server`.
  - *Any route*: never `reset run` inside an RTT session — under SDI the target
    does not come back and USB never re-enumerates.
- **max32666fthr** — a Cortex-M4 with NO DWT cycle counter: read live 2026-08-11
  with the sysview build running, `DEMCR=0x01000000` (TRCENA set by our init) but
  `DWT_CTRL=0x4F000000` (`NOCYCCNT`=1 — Maxim implemented only the 4 watchpoint
  comparators) and `CYCCNT` frozen at 0; UG6971 documents no DWT/trace at all.
  This was the mechanism behind the historical "no usable rate number" (every
  duration silently zero — a CM4-with-NOCYCCNT *links fine*, so the "fails to
  link = not ported" signal never fires). **Ported** the same day:
  `hw/bsp/maxim/sysview_max32_tmr.h` — TMR0 free-running at 48 MHz
  (f_PCLK = f_SYS_CLK/2, Continuous mode, `CMP=0xFFFFFFFF`, prescaler 1;
  `TMRn_CNT` is documented always-readable while counting; the hardware wrap
  reloads CNT to 1, a ~0.23 ppb slip — ignore). The family builds SystemView
  with `SEGGER_SYSVIEW_CORE_OTHER` and sets `CFG_TUSB_SYSVIEW_TIMESTAMP_BSP`,
  the tusb_sysview.c opt-in for ARMv7-M parts without CYCCNT where the BSP also
  reports the rate (the 1 MHz microsecond contract is unreachable: MAX32
  prescalers are powers-of-two only). Timer instance is `-DSYSVIEW_MAX32_TMR=n`
  (default 0; TinyUSB examples use no TMR and FreeRTOS ticks on SysTick, so
  TMR0 is free). MAX3266x only — MAX32650/32690 use different GCR clock-gate
  register names and stay unported. Validated on the rig: window 21 s, USB
  ISR 18 p50 8.5 µs / p99 13.1 µs, and the rate test measured seven clean
  1.000 s gaps at 1.00044–1.00091 (median +0.05%).
- **raspberry_pi_pico2** — host-role on the rig (device port not PC-wired), so
  its old "no usable rate number" is a wiring limit, not a chip one. Validated
  2026-08-11 with a `host/device_info` SYSVIEW capture over the standard
  OpenOCD route: ISR 30 (same RP2350 USBCTRL exception as adafruit_fruit_jam)
  n=173 p50 7.2 µs, `tuh_task` p50 9.6 µs / p99 44.3 µs, overflow 0 — M33 DWT
  timestamps are sane. A precise rate number needs a host-timed stimulus this
  wiring cannot provide; the example idles after its single enumeration
  (0.82 s of activity), so window-vs-wall-clock is no substitute. For an
  RP2350 rate reference use fruit_jam's measured +1.85% — but crystals differ
  per board, so treat it as indicative only.
- **metro_m4_express** — set `"attach_only": true` in its `sysview` roster block:
  after openocd's own `reset run` (jlink interface + `atsame5x.cfg`) the core never
  comes back — the RTT control block never appears and the CDC never re-enumerates
  (SAMD5x DSU CPU Reset Extension is the prime suspect). Measured 2026-08-11: two
  reset-run captures died with `rtt: No control block found` while attach-without-
  reset streamed immediately with the CDC still on the bus. The J-Link flash is
  NOT the problem (erase+program+verify all real, enumeration 1.0 s after flash) —
  and its own post-flash reset is what makes attach-only sound: the capture still
  sees a boot that is only seconds old. Rate measured over this route 2026-08-11:
  five independent host-timed gaps at 1.00992–1.01002 (spread ±0.005%), with the
  traffic-landed check explicit this time (11/11 bursts echoed) — replacing the
  old "no usable rate number", which was a no-traffic measurement artifact, not
  a clock fault. When rate-testing here, ignore the device's own ~2 s periodic
  event cluster (~28 events): it interleaves with host bursts and, unfiltered,
  produces alternating ±10% gaps whose pairs sum correctly — the giveaway.
  Attach-only quality caveat (validated
  end-to-end 2026-08-11, `metro_m4_express: ok`): joining mid-stream shortens the
  clean window (2.4–5.6 s decoded of a 15 s workload across runs) and leaves a few nonsense head
  rows (`ISR 3`/`ISR 512`, n≈3) before the decoder syncs — real rows (ISR 96/98/99,
  `tud_task` n=6048) follow.
- **stm32u083nucleo** — its OpenOCD build ships `stm32u5x.cfg` but no
  `stm32u0x.cfg`, and its ST-Link runs stock firmware (so the J-Link route is out
  too). Capture with a hand-written generic Cortex-M target; flash separately
  with `STM32_Programmer_CLI`. `source [find target/swj-dp.tcl]` explicitly —
  it defines `swj_newdap`, and without it the tap declaration fails with a
  confusing `invalid command name`:
  ```tcl
  source [find target/swj-dp.tcl]
  transport select hla_swd
  set _CHIPNAME stm32u0
  swj_newdap $_CHIPNAME cpu -irlen 4 -expected-id 0x6ba02477
  dap create $_CHIPNAME.dap -chain-position $_CHIPNAME.cpu
  target create $_CHIPNAME.cpu cortex_m -endian little -dap $_CHIPNAME.dap
  ```
- **stm32h743nucleo** — OpenOCD RAM_D1 is `0x24000000`, length `0x80000`
  (`hw/bsp/stm32h7/linker/stm32h743xx_flash.ld`); those are the `rtt setup`
  arguments. Plain `interface/stlink.cfg` works — no `stlink-dap.cfg` fallback
  needed.

## Reference numbers from validated captures

Use these to sanity-check a new capture on the same board.

- **same54_xplained** (`cdc_msc_freertos`, `SYSVIEW=4`, live J-Link, dogfooded
  end-to-end):
  - Contexts/ISR: `cdc` 3.9-5.3% CPU, `usbd` avg 181-186 µs, ISR 96/98/99
    (USB IRQ, exception# = NVIC IRQn+16) p50 27-33 µs, `Idle` ~89%, overflow 0
    at a 16 KB buffer.
  - Functions (level 4, CDC+MSC load): `tud_task` p50 23 µs, `usbd_edpt_xfer`
    13 µs, `dcd_edpt_xfer` 5 µs, `tud_cdc_write_flush` 4.7 µs, `tud_cdc_read`
    118-135 µs (includes FreeRTOS scheduling overhead — tight polling loop),
    `mscd_xfer_cb` 24-34 µs; all n>100. Markers n=320, p50 ~770 µs.
  - Stack (one capture, round-robin): `usbd` 684/1024 B (67%), `cdc` 308/1024 B
    (30%), `IDLE` 40/512 B, `blinky` 148/512 B, `Tmr Svc` 204 B, `io` 228 B.
  - Heap: plumbing proven with a temporary dynamic-alloc probe (`allocs=1
    frees=1 net_bytes=0`); dormant on the stock static-allocation example.
  - Post-mortem: mid-load halt-and-dump decoded with `cdc`/`usbd`/ISR
    98/99/`Scheduler`/`Idle` all present and sane, overflow 0.
- **stm32h743nucleo** — OpenOCD route validated end-to-end: contexts, ISR 117
  (= OTG_FS IRQn 101 + 16, independently verified) and the function table all
  populated and correctly mapped from a raw `capture.SVDat`. Live-J-Link build
  validates green but is build-only (no J-Link wired to this board on the rig).

## Not reachable on the ci.lan pool

`frdm_k64f` and `raspberry_pi_pico_w` (device port does not enumerate — **stock
firmware behaves identically**, so rig cabling), `ch32v103r_r1_1v0` (see caveat
above), `nrf5340dk` (probe USB port), `ek_tm4c123gxl` (lm4flash only), and both
Espressif boards (ESP-IDF build).

## Family support status

- **Timestamp source.** Cores with DWT (ARMv7-M, ARMv8-M mainline: M3/M4/M7/M33)
  need nothing. Cores without it (ARMv6-M M0/M0+, RISC-V) need
  `SEGGER_SYSVIEW_X_GetTimestamp()` in `hw/bsp/<family>/family.c` returning
  **free-running microseconds**. Ported: `stm32f0`/`g0`/`u0` (TIM2), `lpc11`
  (CT32B0), `rp2040` (`time_us_32`), `ch32v20x`/`v30x`/`ch583` (QingKe SysTick).
  An ARMv7-M part whose DWT lacks CYCCNT (MAX3266x) instead sets
  `CFG_TUSB_SYSVIEW_TIMESTAMP_BSP` + `SEGGER_SYSVIEW_CORE_OTHER` in its
  family.cmake and additionally provides `SEGGER_SYSVIEW_X_GetTimestampFreq()`
  — the fixed-1MHz contract is waived where the prescaler can't reach it
  (see the max32666fthr caveat).
  A family with neither fails to link — the intended "not ported yet" signal.
  Two families refuse `-DSYSVIEW` at **configure** time instead, with a
  FATAL_ERROR naming the reason: `ch32v10x` (no safe RTT lock, see its caveat)
  and any `stm32f0` variant without TIM2 (e.g. `stm32f070rbnucleo` — the hook is
  gated on `defined(TIM2)`, so it would otherwise fail at link with six undefined
  references). The refusal is level-independent — it is the timestamp hook that is
  missing, not the instrumentation, so `-DSYSVIEW=1` hits the same wall as 4. A
  clean configure error is the signal; treat it as "port the timestamp first",
  not as a build regression.
- **ISR coverage is per-port** (mechanism and cases: SKILL.md's Build options).
  Ports with NO coverage: `ch32v20x`'s FSDEV port 0 (naked-asm tail-call into
  `dcd_int_handler` reaches neither the macros nor an instrumented handler) and
  the PIO-USB host driver (`hcd_pio_usb.c`, `CFG_TUH_RPI_PIO_USB=1` — a
  different path from the instrumented `hcd_rp2040.c`; measured on
  adafruit_fruit_jam: full function tables, zero ISR rows). Coverage gaps, not
  faults — function-level timing still works on both.
- **Known limitation: the ISR depth counter collapses genuine nesting.**
  `tusb_sysview_isr_enter()`/`_exit()`'s depth counter
  (`src/common/tusb_sysview.c`) exists to fold each dual-role BSP's
  back-to-back self-wrapped `tud_int_handler()`+`tuh_int_handler()` calls into
  one ENTER/EXIT pair — but it collapses *real* nesting the same way: a USB
  IRQ that genuinely preempts another (e.g. `ch32v20x`'s documented
  HP-preempting-LP) is folded into the span of the ISR it preempted instead
  of recorded as its own nested entry. A real tradeoff of the outer-bracket
  design (now hardware-verified), not a bug to re-architect away.
- **FreeRTOS per-task CPU-load table** needs the hook block in the family's
  `FreeRTOSConfig.h`. Wired today: `nrf`, `samd5x_e5x`, `stm32f4`, `stm32f7`,
  `stm32h7`. To add a family, append `#include "sysview_freertos_hooks.h"` at the
  tail of `hw/bsp/<family>/FreeRTOSConfig/FreeRTOSConfig.h` — both build systems
  already reach that header. Any other FreeRTOS family still builds, links and
  emits every other table; it just prints a CMake `message(WARNING)` naming the
  file to patch and leaves the per-task table empty.
- **RAM base for named-object ids.** `SEGGER_SYSVIEW_SetRAMBase()` shrinks
  RAM-resident pointers into `base+offset` ids; `family_support.cmake`'s SYSVIEW
  block defaults `SYSVIEW_RAM_BASE` to the Cortex-M-canonical `0x20000000`, which
  underflows the shrink (garbage mutex/task names) on any family whose SRAM
  starts elsewhere. `lpc11`/`lpc13`/`lpc17`/`lpc40`/`lpc43` set
  `SYSVIEW_RAM_BASE_DEFAULT` to `0x10000000` and `lpc15` to `0x02000000`, each
  in its own `family.cmake`, which the SYSVIEW block above then adopts unless
  `SYSVIEW_RAM_BASE` is already defined. Every other validated family's SRAM starts at the default
  `0x20000000`, so this list is complete — no override needed elsewhere in the
  table above. A family not yet covered can override with
  `-DSYSVIEW_RAM_BASE=<addr>` (SRAM origin from its linker script).
- **Heap alloc/free table** needs `configSUPPORT_DYNAMIC_ALLOCATION=1` in the
  app's own `FreeRTOSConfig.h` — none of the five families wired for the
  CPU-load table above enable it, so `heap` reads `null` in every stock capture
  regardless of `-DSYSVIEW` level. The plumbing
  (`tusb_sysview_heap_alloc`/`_free`, `sysview_freertos_hooks.h`'s
  `traceMALLOC`/`traceFREE`) is proven and costs nothing when unused; an app
  that turns dynamic allocation on gets the table for free.

## Adding a board

1. Build `cdc_msc` (or `cdc_msc_freertos`) at `-DSYSVIEW=4`. A link failure on
   the timestamp symbol means the family needs a `SEGGER_SYSVIEW_X_GetTimestamp()`
   — port it before going further. A `.bss` overflow means the board needs a
   `-DSYSVIEW_BUFFER_SIZE` override.
2. Capture on whichever route the board's probe supports — OpenOCD streaming
   first (routine/CI default), J-Link live for depth or missing target cfgs,
   dump only when live attach cannot work (SKILL.md's route table has the
   preference rationale; WCH is dump-only, see the caveat).
3. Verify the timestamp rate before trusting any duration: drive ten host-timed
   1.000 s silences and read the median gap. **Confirm traffic actually landed
   first** — an idle enumerated TinyUSB CDC device emits a USB event about every
   2.016 s, so with no traffic the median reads as almost exactly 2× the intended
   1.000 s. `metro_m4_express` (a DWT board, where a 2× error is impossible)
   showed exactly this, as did `ch32v307v_r1_1v0`.
4. Add the row here, plus a caveat bullet for anything hard-won.
