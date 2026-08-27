---
name: rtt
description: Use when you need console or printf I/O, TU_LOG capture, or a raw byte channel over a debug probe on real hardware — the board has no UART wired or its probe no VCOM, a LOGGER=rtt build needs reading or writing, "RTT Control Block not found", an RTT server won't come up or drops output, JLinkRTTLogger/JLinkRTTClient/JLinkGDBServer/openocd rtt misbehave, or another workflow (HIL console, SystemView capture) needs RTT stood up on a J-Link, ST-Link, CMSIS-DAP or WCH-Link probe.
---

# rtt — SEGGER RTT transport and console

RTT is nothing but RAM: a control block `_SEGGER_RTT` (starts with the magic
string `"SEGGER RTT"`) plus per-channel ring buffers
`{sName, pBuffer, SizeOfBuffer, WrOff, RdOff, Flags}`. The target advances
`WrOff`; the host must **write `RdOff` back** to free space — a reader that
only reads never drains the ring. Channel 0 is the "Terminal" console;
SystemView claims its own `"SysView"` up-buffer on the same control block —
they coexist. The debug probe reads/writes this RAM while the core runs, so
everything here is zero-wiring: no UART, no VCOM.

Scope: byte transport and console. Timing/profiling → `etm-trace`/`sysview`;
debugging decision flows and the wedged-target drain model → `target-debug`;
Espressif consoles → `esp-target-debug` (USB-Serial-JTAG, no SEGGER RTT).

## Quick start — console on a J-Link probe

Use the skill's tool `tools/rtt.py` for every route; do not hand-roll
JLinkExe/JLinkGDBServer/openocd/telnet pipelines (`--help` for all modes):

```bash
# firmware: TU_LOG + stdio → RTT channel 0 (hw/bsp/board.c routes sys_read too)
cmake -DBOARD=<board> -DLOG=2 -DLOGGER=rtt ...   # Make: LOG=2 LOGGER=rtt

# flash + reset FIRST (the console owns the probe once open), then:
python3 tools/rtt.py --backend jlink --probe <serial> --device <JLINK_DEVICE> --seconds 20
#   -i forwards stdin to the target; --seconds 0 streams until Ctrl-C/EOF
```

`JLINK_DEVICE` comes from `hw/bsp/<family>/boards/<board>/board.cmake` (or
`family.cmake`). Always pass the probe serial — rigs and benches run several
probes, and the `ninja <example>-jlink` flash target grabs whichever J-Link
enumerates first: pin it (`-DJLINK_OPTION="-USB <serial>"`) or flash with
`JLinkExe -SelectEmuBySN`. The HIL harness uses the same implementation
(`hil_util.JlinkRtt`) via a board's `"logger": "rtt"` (jlink flashers
only) plus a single self-named variant carrying the define —
`"variant": [{"name": "<board>", "defines": ["LOGGER=rtt"]}]`, the roster's
one shape for always-on defines — variant defines feed `hil_test.py
--build` and the CI matrix; a prebuilt `cmake-build-<board>` set must be
configured with the same `-DLOGGER=rtt` itself. Keep harness console builds
quiet (`LOGGER=rtt` WITHOUT `LOG=2`): reset-then-attach only preserves what
fits the up-buffer (stock 1 KB, NO_BLOCK_SKIP), and a chatty boot burst
truncates at the ring boundary before the drain attaches — measured
1022-1023 B captures on ea4088 with `LOG=2`, enumeration lines falling off
the end. `BUFFER_SIZE_UP` is the knob when verbose logs are really needed.
Rig boards need `hil_lock.py` held first — see the `hil` skill.

To validate bidirectionality end-to-end you need firmware that both polls
the console AND replies via printf. `board_test` polls `board_getchar()`
(RTT-aware via `sys_read`) but echoes through `board_putchar` →
`board_uart_write`, which is NOT LOGGER-aware — on a UART-less board the
echo hits the `-1` stub and vanishes (measured on ea4088). For a validation
run, patch its echo to `printf` locally, or drive a host example's menu
(`msc_file_explorer`, `cdc_msc_hid` — they reply via printf). Sending
keystrokes to `cdc_msc` and expecting an echo proves nothing: it never polls
the console.

## Transport matrix

| Transport / tool                                | Live read  | Write    | Notes                                                                                                                                                                                                                                                                                          |
| ----------------------------------------------- | ---------- | -------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| ARM memory-AP (any J-Link/ST-Link/CMSIS-DAP)    | yes        | yes      | zero intrusion; core keeps running                                                                                                                                                                                                                                                             |
| RISC-V SBA (where implemented)                  | yes        | yes      | autonomous like memory-AP                                                                                                                                                                                                                                                                      |
| WCH QingKe SDI                                  | **NO**     | no       | DM abstract-command reads perturb the running core: A/B-proven firmware kill ~1.9 s into USB traffic. Halt→read→resume or post-mortem dump ONLY                                                                                                                                                |
| OpenOCD/jaylink on a genuine SEGGER J-Link      | yes        | untested | routine in the sysview campaigns (metro_m4_express, dozens of attaches, zero wedges); prefer SEGGER tools where both exist (drain rate)                                                                                                                                                        |
| OpenOCD/jaylink on the LPC-Link2 (J-Link OB fw) | forbidden  | —        | measured on ea4088's LPC-Link2 (2023 OB image): transport fails (`jaylink_swd_io`) and knocks the probe off USB; physical replug to recover — SEGGER tools only THERE. Verdict is for that probe only: other J-Link-OB firmware probes are untested — hardware-test before assuming either way |
| `JLinkRTTLogger`                                | unreliable | —        | searches for the control block once at attach and gives up — on some parts it never finds it ("RTT Control Block not found" even with `-RTTAddress`; measured 0/6 on LPC4088). May work elsewhere, but don't build automation on a single-search tool                                          |

Validated boards, directions and per-board caveats: [boards.md](boards.md).

## Capture: J-Link route

`rtt.py` above is this route packaged. Raw form (what it runs):

```bash
JLinkExe -USB <serial> -device <dev> -if swd -speed 4000 -NoGui 1 -AutoConnect 1 \
         -RTTTelnetPort <port>     # keep stdin open; 'exit' tears it down
nc localhost <port>                # JLinkRTTClient minus the banner; carries input too
```

Commander keeps hunting for the control block and delivers the buffered boot
burst once the target's first printf creates it. `JLinkGDBServer
-RTTTelnetPort` also serves the port but on some parts (measured: LPC4088)
never locates the control block **unless a GDB client attaches** — fine
inside a GDB session, a silent failure headless — and it briefly halts the
core on connect (measured), which matters for timing-sensitive repros;
Commander does not. One telnet client per port at a time.

## Capture: OpenOCD route (native probes: ST-Link, CMSIS-DAP)

This is the LIVE route — WCH-Link targets are SDI and get only the halt→dump
route (transport matrix). Same script, openocd backend (`--elf` = the
FLASHED elf; the script takes the exact control-block address from `nm` —
a full-RAM scan is slower and can match stale RAM after a soft reset):

```bash
python3 tools/rtt.py --backend openocd --probe <serial> \
  --cfg "-f interface/stlink.cfg -f target/stm32h7x.cfg" --elf <flashed.elf> --seconds 20
#   --channel: up-buffer index (0 = "Terminal" console, 1 = SystemView's "SysView"
#   buffer in TinyUSB builds); -i forwards stdin → down-buffer 0
#   --vid-pid "0x2e8a 0x000c": pin the probe by USB IDs (with or instead of --probe;
#   also keeps openocd discovery off foreign usbfs nodes)
#   --addr 0x2000xxxx: explicit control-block address when the flashed elf is not at hand
#   --reset-before-attach: reset the target INSIDE the session (2 s settle, then
#   attach — the control block must exist before `rtt start` can find it; the ring's
#   NO_BLOCK_SKIP head-retention is what preserves byte 0 across the settle) —
#   required for streams that only decode from byte 0
#   (SystemView emits its Init record, carrying the timestamp frequency, once at boot;
#   a mid-flight attach yields a stream no decoder can lock onto). Verified on
#   stm32h743nucleo: after the ring is drained, a plain attach misses the boot preamble
#   entirely and this flag captures it. NOT for SAMD5x (an in-session reset via the DSU
#   leaves the core held) or WCH SDI.
```

What it runs: `openocd <cfg> -c "adapter serial <sn>" -c init -c "rtt setup
<nm-addr> 0x800 \"SEGGER RTT\"" -c "rtt polling_interval 1" -c "rtt start"
-c "rtt server start <port> <ch>"`, then a socket on that port.

Attach WITHOUT reset when the flash step already reset the board (on SAMD5x,
an in-session `reset run` goes through the DSU CPU Reset Extension and leaves
the core held). After any reset the target's offsets restart at zero while
the server holds stale ones, and the tool exposes no console to type into (it
launches openocd with tcl/gdb/telnet ports disabled): stop the capture and
run it again to resync — do not reset mid-capture if you can avoid it. `rtt start`
fails while the block doesn't exist yet: it appears at the firmware's first
RTT write, so reset, settle ~500 ms, then start. Read AND write validated on
the ci rig's 8 native-probe boards (ST-Link + CMSIS-DAP, incl. RP2350),
end-to-end through this script's backend on all 8 — per-board rows in
boards.md. OpenOCD polls, and host-side loss is invisible
to the target's overflow counter: at the default 100 ms interval a busy
stream loses most samples (measured 2066 of 5064 events/s delivered on
stm32f407disco) — `rtt polling_interval 1` is mandatory for quantitative
capture, not a tuning nicety. Prefer SEGGER tools where a J-Link exists.

## Post-mortem: reading the ring without a live server

Default log mode is `NO_BLOCK_SKIP`: with no reader draining, the ring holds
the **first KB after boot, not the tail** — interpretation rules in
`target-debug`. To keep the last N bytes instead, the firmware must log via
`SEGGER_RTT_WriteWithOverwriteNoLock` (target drags `RdOff` itself; no host
needed) — but SEGGER's own restriction comes with it: *"Do not use
SEGGER_RTT_WriteWithOverwriteNoLock if a J-Link connection reads RTT data"*
(`lib/SEGGER_RTT/RTT/SEGGER_RTT.c`), because the target moving `RdOff` races
the host reader. So it is for firmware you dump post-mortem, never for a
board that also runs a live console (every HIL rtt board does). Reading a wedged target's ring — debug-AP RAM reads don't halt the
core:

```bash
python3 tools/rtt.py --backend jlink --dump ring.bin \
  --probe <serial> --device <JLINK_DEVICE> --elf <flashed.elf>   # or --addr 0x...
# prints pBuffer/Size/WrOff/RdOff; WrOff/RdOff delimit the valid bytes
```

(What it runs, for hand-driving JLinkExe: `nm` the ELF for `_SEGGER_RTT`,
`mem32 <addr+0x18>, 6` = aUp[0] {sName,pBuffer,Size,WrOff,RdOff,Flags},
then `savebin <file> <pBuffer> <SizeOfBuffer>`.)

## Buffer modes and locking (target side)

- Modes: `NO_BLOCK_SKIP` (default for logs — drops whole writes when full),
  `NO_BLOCK_TRIM`, `BLOCK_IF_FIFO_FULL` (target spins — dangerous in ISRs).
- Throughput is drain-limited: measured 24.6 KiB/s over a J-Link console
  against a saturating printf loop, with the drops happening at the target.
  RTT console output is NOT lossless under load; for high-bandwidth streams
  size the buffer up (SystemView needs 2048–8192) and watch for overflow.
- Non-ARM ports must supply `SEGGER_RTT_LOCK/UNLOCK`: the vendored generic
  RISC-V lock uses `mstatus` CSRs that trap (mcause=2) on WCH QingKe. Worked
  port on branch `claude/add-systemview-debug`: `hw/bsp/ch583/
  sysview_rtt_lock_wch.h` (brace-scoped save/restore of CSR 0x800), and the
  shared `hw/bsp/sysview_rtt_conf_wch.h` that ch32v20x/ch32v30x family.cmake
  force-include to win the include-guard race against the vendored conf.

## Common mistakes

- **Attaching before the first printf** — the control block is zeroed `.bss`
  until the firmware's first RTT write; early readers see nothing (and
  RTTLogger gives up for good). Commander/`rtt.py` keep hunting.
- **Sending input before the server finds the control block** — the J-Link
  telnet route silently DROPS client bytes until then (measured on the rig:
  an instant `ping` vanished, a delayed one echoed). `rtt.py -i`
  holds stdin until target output flows (or 5 s); when driving the raw
  socket yourself, wait for output before writing.
- **Resetting while a console is attached** — flash and reset first; the
  console owns the probe until closed.
- **Killing servers with `pkill -f`** — the pattern matches your own shell's
  cmdline (and unrelated sessions): a compound command that pkills its
  wrapper then re-reads a stale log misdiagnosed a healthy probe for an
  hour. Close `rtt.py` with Ctrl-C/`--seconds` (its teardown reaps
  the whole process group); if you must pattern-kill, bracket a char:
  `pkill -f '[J]LinkExe -USB <serial>'`.
- **Unpinned flash with several probes attached** — pin by serial, always.
- **Two probes wired to one SWD header** — wedges the target; rewire.
- **Expecting an echo from firmware that never reads the console** — only
  code polling `board_getchar()` consumes down-buffer 0 (`board_test` does).
- **Full-RAM `rtt setup` scans** — can lock onto a stale pre-reset block;
  use the `nm` address.
