# rtt — per-board validation matrix

A row appears here only after the board was exercised on real hardware; a new
validation adds the row AND any caveat it surfaced. "Read" = console/log
capture reached the host; "Write" = the target demonstrably consumed console
input (a printf-echo `board_test` returned the sent bytes — stock
`board_test` cannot, see SKILL.md's echo-validation note). Routes match
SKILL.md's capture sections; `Device/cfg` is the J-Link `--device` string or
the openocd target cfg. Rig rows (ci.lan) were validated 2026-08-24 by a
flash→capture→`ping`-echo sweep under per-board `hil_lock` flocks, and
re-validated 2026-08-25 end-to-end through the skill's own CLI
(`tools/rtt.py`, jlink + openocd backends): 20/20 read+write —
including CONCURRENTLY at 8 parallel consoles (20 boards in 39 s, mixed
routes, no port collisions or cross-board output bleed: one server per
probe on its own ephemeral port). htpc rows on the local bench. The openocd backend's `--reset-before-attach`
is decode-validated: a channel-1 SystemView capture on stm32h743nucleo
(byte-identical boot preamble to the sysview campaign's golden reference,
49765 events decoded, ISR/task timings matching to 0.1 µs, overflow 0).

| Board                    | Rig  | Probe                  | Route   | Read | Write | Device/cfg            |
| ------------------------ | ---- | ---------------------- | ------- | ---- | ----- | --------------------- |
| ea4088_quickstart        | htpc | LPC-Link2 J-Link fw    | J-Link  | yes  | yes   | `LPC4088`             |
| raspberry_pi_pico2       | htpc | J-Trace PRO            | J-Link  | yes  | —     | `rp2350_m33_0`        |
| frdm_k64f                | ci   | J-Link                 | J-Link  | yes  | yes   | `MK64FN1M0xxx12`      |
| feather_nrf52840_express | ci   | J-Link                 | J-Link  | yes  | yes   | `nrf52840_xxaa`       |
| metro_m4_express         | ci   | J-Link                 | J-Link  | yes  | yes   | `ATSAMD51J19`         |
| lpcxpresso11u37          | ci   | J-Link                 | J-Link  | yes  | yes   | `LPC11U37/401`        |
| lpcxpresso55s28          | ci   | J-Link                 | J-Link  | yes  | yes   | `LPC55S28`            |
| ra4m1_ek                 | ci   | J-Link                 | J-Link  | yes  | yes   | `R7FA4M1AB`           |
| stm32f072disco           | ci   | J-Link                 | J-Link  | yes  | yes   | `stm32f072rb`         |
| stm32f407disco           | ci   | J-Link                 | J-Link  | yes  | yes   | `stm32f407vg`         |
| stm32f723disco           | ci   | J-Link                 | J-Link  | yes  | yes   | `stm32f723ie`         |
| stm32l476disco           | ci   | J-Link                 | J-Link  | yes  | yes   | `STM32L476VG`         |
| mimxrt1064_evk           | ci   | J-Link                 | J-Link  | yes  | yes   | `MIMXRT1064xxx6A`     |
| nrf54lm20dk              | ci   | J-Link                 | J-Link  | yes  | yes   | `NRF54LM20A_M33`      |
| max32666fthr             | ci   | CMSIS-DAP              | OpenOCD | yes  | yes   | `target/max32665.cfg` |
| raspberry_pi_pico        | ci   | debugprobe (CMSIS-DAP) | OpenOCD | yes  | yes   | `target/rp2040.cfg`   |
| raspberry_pi_pico_w      | ci   | debugprobe (CMSIS-DAP) | OpenOCD | yes  | yes   | `target/rp2040.cfg`   |
| raspberry_pi_pico2       | ci   | debugprobe (CMSIS-DAP) | OpenOCD | yes  | yes   | `target/rp2350.cfg`   |
| adafruit_fruit_jam       | ci   | debugprobe (CMSIS-DAP) | OpenOCD | yes  | yes   | `target/rp2350.cfg`   |
| stm32h743nucleo          | ci   | ST-Link                | OpenOCD | yes  | yes   | `target/stm32h7x.cfg` |
| stm32g0b1nucleo          | ci   | ST-Link                | OpenOCD | yes  | yes   | `target/stm32g0x.cfg` |
| stm32u083nucleo          | ci   | ST-Link                | OpenOCD | yes  | yes   | `target/stm32u0x.cfg` |

Probe serials live in the rig configs (`test/hil/tinyusb.json`, bench
`local.json`) — always pass them (`--probe` / `adapter serial`).

## Caveats

- **ea4088_quickstart**: probe has no VCOM and the BSP has no UART — RTT is
  the ONLY console; measured there: 6/6 JLinkExe attaches, boot burst
  delivered, 24.6 KiB/s drain; the HIL suite runs over the RTT console
  (device_info-class tests — the cdc/msc-fixture host tests don't speak RTT
  yet, see the follow-up doc).
  NEVER point OpenOCD at this J-Link-firmware probe (jaylink knocks it off
  USB; physical replug). JLinkGDBServer never finds the CB headless on this
  part; JLinkRTTLogger 0/6.
- **raspberry_pi_pico2 (htpc, J-Trace)**: pin the probe by serial — that
  bench runs two J-Links (`-DJLINK_OPTION="-USB <sn>"` for the flash
  target). Never set a custom JLinkScript for RP2350 over J-Link. Write path
  untested there only because the flashed example doesn't poll the console
  (the ci row's debugprobe sweep validated RP2350 writes).
- **ST-Link rows**: flashed by `STM32_Programmer_CLI`; RTT capture is a
  separate openocd session (`interface/stlink.cfg` + the target cfg above),
  attach without reset.

## Excluded (recorded so absence is never read as "works")

- `espressif_s3_devkitm`, `espressif_p4_function_ev` — no SEGGER RTT path in
  our builds (console is the chip's USB-Serial-JTAG; see `esp-target-debug`).
- `ek_tm4c123gxl` — flashed by `lm4flash`; no debug-probe path configured on
  the rig.
- `nanoch32v203`, `ch32v103r_r1_1v0`, `ch32v307v_r1_1v0`, `ch582m_evt` — a
  `LOGGER=rtt` build traps on WCH QingKe (the vendored generic RISC-V
  `SEGGER_RTT_LOCK` reads `mstatus` CSRs → mcause=2; the working lock port
  `sysview_rtt_lock_wch.h` lives only on branch `claude/add-systemview-debug`),
  and SDI permits no live streaming anyway (transport matrix). Revisit after
  that branch merges.
