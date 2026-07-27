# etm-trace — per-board reference

Validated boards: trace config table + hard-won caveats. Read the row AND the
caveat for a board before capturing on it; add a row + caveat when a new board
is validated (jdebug reference, board.cmake/board.h clock selection and this
file must agree).

"Core (trace build)" is the CPU clock a `TRACE_ETM=1` build runs — where it
differs from the stock clock, board.h selects it automatically. Timing
"0 (unset)" = the reference sets no SetTraceTiming and Ozone then sends
`TraceSampleAdjust TD = 0`; J-Link's own +2 ns default (UM08001) applies only
outside Ozone-driven captures. Explicit values live in the committed
reference.

| Board              | Core (trace build)   | TRACECLK pin          | Width | Timing  | Physical setup                | TODO                                        |
|--------------------|----------------------|-----------------------|-------|---------|-------------------------------|---------------------------------------------|
| stm32h743eval      | 400 MHz              | 50 MHz (PLL1R, fixed) | 4     | +100 ps | —                             | —                                           |
| stm32n657nucleo    | 300 MHz              | 18.75 MHz (cpu/16)    | 4     | 0 (unset) | none — CN1 MIPI20; JP2=1      | —                                           |
| stm32h7s3nucleo    | 300 MHz              | 50 MHz (cpu/3/2)      | 2     | 0 (unset) | none — native CN1 MIPI20      | remove SB11/SB12 → width 4 @ 600 MHz        |
| stm32h563nucleo    | 100 MHz              | follows core          | 1     | +5 ns   | remove SB8/9/64/68/70/71/78   | retest width 4 / 250 MHz after SB removal   |
| metro_m7_1011      | 500 MHz              | 66 MHz (root/2)       | 4     | +50 ps  | custom rev A ETM-header rework | —                                          |
| mcb1800            | 120 MHz              | 60 MHz (CCLK/2)       | 4     | 0 (unset) | fit J5 DBG_EN                 | —                                           |
| ea4088_quickstart  | 120 MHz              | 120 MHz               | 4     | 0 (unset) | J7 (fully wired)              | —                                           |
| nrf52840dk         | 64 MHz               | 16 MHz (hw cap)       | 4     | 0 (unset) | solder P25, SW7 → Alt         | —                                           |
| nrf5340dk (M33)    | 64 MHz               | 16 MHz (TAD, forced)  | 4     | +3 ns   | mount P25; cut SB27/SB28      | —                                           |
| mimxrt1170_evkb    | 996 MHz              | 50 MHz (root/2)       | 1     | 0       | weld 0 Ω R1881-R1886; JP4 shorted; J58 (populated) | re-weld R1884 (D3 open; D1/D2 meter-verified good) → width 4 |
| ra6m5_ek (M33)     | 200 MHz              | 25 MHz (TRCLK/4 /2)   | 4     | 0 (unset) | J9 closed; native J20 trace | —                                           |
| ra8m1_ek (M85)     | 480 MHz              | 60 MHz (TRCLK/4 /2)   | 4     | 0 (unset) | J9 closed + Table 7 jumpers | —                                           |
| raspberry_pi_pico2 (RP2350 M33) | 48 MHz  | 24 MHz (clk_sys/2)    | 4     | 0 (unset) | fly-wire GPIO1-5 → MIPI20 (map in jdebug) | 72-80 MHz per seating (re-qualify); >80 needs V3 probe + trace board |
| same54_xplained (E54 M4F) | 120 MHz       | 60 MHz (CPU/2)        | 4     | 0 (unset) | none — populated 20-pin ETM header | —                                      |
| same70_xplained (E70 M7) | 300 MHz         | 37.5 MHz (PCK3/2)     | 1     | 0 (unset) | solder 20-pin header on J403 (bottom) | width 4 blocked: D1 (J403.16) dead at speed — probe-channel crosscheck pending |
| SEGGER H7/F407 ref | demo defaults        | demo                  | 4     | demo    | probe-powered: add `--power`  | —                                           |

Board caveats (beyond the table):

- **stm32h743eval**: startup-burst overflow at 400 MHz is normal (reduce
  PLLN in board.h for overflow-free capture); timestamp ref 200 MHz.
- **stm32n657nucleo** (M55, flashless): **JP2 (BOOT1) must be 1** — the app
  is a RAM image the debugger loads (Development boot); in flash boot the
  bootROM parks the chip un-attachable ("Can not attach to CPU"). SEGGER's
  KB says BOOT0/BOOT1 = 0/0 for their example — that is flash boot and it
  does NOT attach; the board manual's Table 11 is right. 600 MHz core kills
  the stream in the startup burst (timestamp flux, not overflow) — TRACE_ETM
  builds run 300 MHz; at 600 use `--no-timestamps`. No J-Link script needed:
  N6 trace components are ROM-table-discoverable.
- **stm32h7s3nucleo**: width 4 is clean at idle but SB11/SB12 (default ON)
  stub D2/D3 onto Zio CN8 and the stream dies under IRQ-heavy USB traffic —
  remove them to try width 4 / 600 MHz. `--attach` while a USB host is
  actively polling the device wedges its USB session (needs target reset).
- **stm32h563nucleo**: width 4 or 250 MHz corrupts. H5 hangs its debug AP if
  trace CoreSight is touched unclocked — handled by the committed
  `AfterTargetConnect` hook; un-attachable after a killed session →
  power-cycle.
- **metro_m7_1011** (RT1011): a custom Adafruit rev with a hand-added 2x10
  ETM header (KiCad schematic in the calibre library). No SEGGER RT1011
  example exists — the committed .jdebug (tuned +50 ps) is the known-good
  reference. BOARD_BootClockRUN sets the 132 MHz trace root but leaves it
  gated; `trace_etm_init` ungates it. The first Ozone run after a fresh
  flash can fail to reach main — transient, retry once.
- **mcb1800**: a bad ribbon mating yields register-perfect silence — re-seat
  BOTH ribbon ends first; SWD working proves nothing about the trace lines.
  `--isr USB0_IRQHandler,dcd_int_handler`.
- **nrf5340dk**: the interface MCU's UART1 flow control rides the trace
  pins — it actively drives CTS onto P0.10/TRACEDATA1 (dead line, any
  timing) and loads P0.11/TRACEDATA0: cut SB27/SB28 (or flip SW7 to FC-off).
  SB57's SWO stub can stay — harmless at the +3 ns sample point. TRACE_ETM
  builds force the TAD port to 16 MHz (SystemInit's 64 MHz is marginal).
- **ea4088_quickstart**: FS enumeration ends < 100 ms — for `--isr` use
  `--duration-ms 150`. Boot-ROM address warning (0x1FFF1FF0) is normal.
- **mimxrt1170_evkb**: width 1 only — D1-D3 are stone silent at any config
  (pinmux register-perfect, PHY quieted, funnel enabled): the welded 0402s
  R1882/R1883/R1884 are electrically open — reflow to unlock width 4.
  `trace_etm_init` fixes the JTAG_nTRST/DMIC_DATA1 pad, holds the 100M
  RTL8201 PHY in reset (ENET_RST_B = GPIO_LPSR_04 — its RMII lines share
  the trace pads; with it quiet the CLK line runs a 100 MHz root/50 MHz
  pin, 2x the pre-lever rate; 133 MHz root is marginal, stock 132 corrupts)
  and enables the CM7 platform trace-funnel port, which J-Link doesn't
  program: without it everything reads register-perfect yet zero data
  arrives. JP4 must be shorted (disables MCU-Link SWD) for the external
  J-Trace on J58; a powered MCU-Link USB breaks the external probe's connect
  even with JP4 shorted - power the board from another port. **Width is 1 or
  4 only**: a width-2 request arms the CSSYS TPIU (E004_6000) and the probe
  sampler at 4-bit anyway (CSPSR reads 0x8 after a width-2 session; a live
  CSPSR=2 poke with LAR unlock + Trace.Clear still captures nothing because
  the probe keeps sampling 4-bit). FlexSPI apps: ROM bootloader must set SP/PC — the
  committed reset/download hooks handle this. Startup-burst overflow at
  996 MHz is normal. No Ethernet (100M) while tracing.
- **ra6m5_ek**: TRCKCR div-2 (100 MHz TRCLK = 50 MHz pin, the chip max) is
  unusable on this board — swept widths 1/4 across -2..+4 ns, all dead;
  TRACE_ETM builds use div-4 (25 MHz pin). The TCLK pin runs TRCLK/2. 50 MHz SWD TIF
  caused intermittent "Failed to initialize DAP" — the reference runs 4 MHz.
  ISR entry: `--isr tusb_int_handler,dcd_int_handler` (FSP's
  usbfs_interrupt_handler symbol never actually executes).
- **ra6m5_ek / ra8m1_ek — `--attach` needs a debugger-booted target**: the
  firmware TRCKCR setup is gated on DHCSR.C_DEBUGEN (an unguarded write
  wedges a standalone boot un-attachable until power-cycle), so a board
  booted WITHOUT a debugger has no trace clock and an `--attach` capture
  reads silence. Reflash/reset through the capture default flow first.
- **ra8m1_ek**: **J9 must be closed** (holds the on-board J-Link OB in
  reset — open = SWD contention, intermittent "Failed to initialize DAP",
  even an apparent brick recoverable only by power-cycle/J16 boot mode).
  J-Link's RA8 support enables trace from reset; the committed JLinkScript's
  empty `OnTraceStart` suppresses that so the firmware enables the trace
  clock after the FSP clock switch — without it the MOCO→PLL step desyncs
  the decoder at t≈0.05 s every run. Runs both chip maxima (120 MHz TRCLK,
  60 MHz pin) clean. `ReadIntoTraceCache 0x0 0x10000` in the download hook
  covers runtime chip-ROM execution. ISR entry: `tusb_int_handler`.
- **raspberry_pi_pico2** (RP2350): TRACECLK is a fixed clk_sys/2, no divider
  (DDR data, like every ARM TPIU pin port). **Measured cliff on this rig:**
  80 MHz core (40 MHz TRACECLK) traces idle code but dies under dense data;
  88 MHz+ dies instantly at any width/global-timing/TIF/pad setting. Cause
  not pinned down: the same V2 probe samples 66 MHz TRACECLK (132 Msample/s)
  on metro_m7_1011, so it is NOT a plain probe sample-rate ceiling. The
  cliff at >40 MHz TRACECLK (84+ MHz core) survived a full sweep - global
  AND per-pin `--trace-timing`, pad drive 2/4/8/12 mA + slew, width 4/2/1,
  TIF 1-25 MHz, newer J-Link library - all flat, so it is V3-probe / real-
  trace-board territory (SEGGER's Pico 2 KB requires J-Trace PRO **V3.0+**
  and recommends a proper trace board; community reports fly-wires fail at
  75 MHz for everyone, PCBs work). Separately, fly-wire seating quality
  sets the width-4 DENSE-data ceiling (48-72 MHz observed across seatings):
  after ANY rewiring re-qualify with idle blinky at the target clock, then
  cdc_msc x3. Random unknown-packet deaths KB into a clean stream = one
  marginal wire; `--trace-width` 1 vs 2 vs 4 bisects which (width 1 =
  CLK+D0 only; D1 = GPIO3->MIPI20 pin 16 has gone marginal twice on this
  rig). Width-1 is a full-quality fallback: complete cdc_msc profiles at up
  to 80 MHz core even when width 4 is broken.
  **Never set a custom JLinkScript** — it
  replaces J-Link's built-in RP2350 device script, which both declares the
  trace component map (funnel/TPIU/ETM are not in the ROM table → "Required
  trace components for pin trace not found", 0 fetches) and re-arms the whole
  chip-side path via `OnTraceStart` at every resume. Firmware therefore does
  no trace setup; TRACE_ETM builds only (a) pin clk_sys to 48 MHz from crt0
  (board.cmake) — the fly-wire ceiling: 96/150 MHz kill the stream in the
  startup burst at any sample timing (and at 150 MHz the saturated probe
  stops answering halts, "CPU could not be halted"); any post-arm clock
  change steps TRACECLK mid-stream and kills the decoder — and (b)
  clear TIMER0/1 DBGPAUSE (family.c): debug sessions leave cores
  halted-at-reset and the default DBGPAUSE freezes the µs timer, so every
  `sleep_ms()` spins forever (looks like a dead board; watchdog-scratch
  breadcrumbs survive warm resets but not POR when diagnosing). UART console
  is TX-only (GPIO1 = TRACECLK). Empty reset/download hooks: the bootrom
  must run the IMAGE_DEF. If the chip ends up wedged/un-attachable:
  J-Link `erase` + reset drops it into BOOTSEL (2e8a:000f) for picotool.
- **same54_xplained**: the CM4 trace unit is clocked from **GCLK channel 47
  (GCLK_CM4_TRACE)** — with it disabled the pins mux fine, TPIU/ETM arm
  fine, and the port stays perfectly silent (zero fetches, no errors);
  `trace_etm_init` feeds it GCLK0. Pins PC24-28 mux to function H. The
  populated 20-pin header runs chip-max 60 MHz TRACECLK width 4 with no
  timing adjustment - the connector-vs-flywire contrast board.
- **same70_xplained**: J403 is a bottom-side bare footprint — solder the
  header. Trace pins PD4-7 double as the KSZ8081 PHY's RMII receive outputs:
  TRACE_ETM builds hold it in reset (PHY_RESET=PC10) or it drives against
  the stream. TPIU clock = PCK3 (datasheet 16.7.4), run at MCK/2; TPIU
  programming while PCK3 is stopped is silently LOST — the reference starts
  PCK3 in the post-reset/download hooks (a reset wipes the PMC, so
  AfterTargetConnect is too early). Width 1 validated at the stock 300 MHz
  core; width 2/4 blocked on a dead D1 line at J403.16 (clean at DC by
  meter, dead at speed — probe-channel crosscheck on a known-good width-4
  board pending). No Ethernet while tracing.
- **SEGGER ref boards**: run their own demo (ladder step 3 flags); `--isr`
  degrades gracefully without a live SysTick.
