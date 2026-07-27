---
name: target-debug
description: Use when TinyUSB firmware — device or host stack — misbehaves on real hardware and capture from the other end can't explain it: a HIL test fails but usbmon shows only Submits with no Completes, the device silently NAKs, wedges, STALLs, babbles, or drops data, EP0 starves, tuh_ enumeration of an attached device fails, an ISR or DCD/HCD state bug is suspected — and you need target-side evidence: TU_LOG/RTT logs, GDB state dumps, a RAM ring-buffer event trace, or PC-sampling of where the core spins.
---

# target-debug — target-side capture & debugging on the HIL rig

The **target** is whichever MCU runs TinyUSB — device stack (`dcd_*`), host
stack (`hcd_*`/`tuh_*`), or both. Its link peer is not always a Linux PC: a
TinyUSB host may face another TinyUSB board or a Linux gadget (e.g. a
Raspberry Pi). Pick capture channels by which end runs Linux, not by habit:

| Skill              | Answers                                            | Exists when                                       |
|--------------------|----------------------------------------------------|---------------------------------------------------|
| `usbmon`           | what the Linux host exchanged (URBs)               | a Linux PC is the link's host                     |
| `usb-kernel-debug` | why the Linux kernel acted (dmesg / dynamic debug) | Linux on either end: PC host or Linux gadget peer |
| **`target-debug`** | **what the target did** (logs, driver state, PC)   | always — either role, needs a debug probe         |
| `usb-sniffer`      | what crossed the wire (PIDs, handshakes, resets)   | hardware tap cabled in — role-agnostic            |
| `etm-trace`        | exactly which instructions executed (profile, coverage, history) | SEGGER J-Trace wired to this board's trace header — confirm with the user first |

For enumeration/transfer bugs the default posture is **dual-side capture** —
both ends simultaneously: usbmon + a target
channel when a Linux PC is the host; TinyUSB-as-host has no usbmon on either
end — target channel + the wire (`usb-sniffer`), plus `usb-kernel-debug` on a
Linux gadget peer.

## Rig discipline — lock first, always

Hold the board lock for the WHOLE manual session; never stop the
actions-runner (see the `hil` skill for the full lock protocol):

```bash
python3 test/hil/board_lock.py hold <board> --reason "target debug: <bug>"
# ... instrument / build / flash / capture / GDB ...
python3 test/hil/board_lock.py release <board>
```

Board → probe mapping: `test/hil/tinyusb.json` — `flasher.name` is the probe
family, `flasher.uid` the **probe serial** (many identical probes on the rig):

- Select the probe by serial: J-Link `-SelectEmuBySN <uid>`, its GDB server
  `-select usb=<uid>`, OpenOCD `-c 'adapter serial <uid>'`.
- `JLINK_DEVICE` / `OPENOCD_OPTION`: from
  `hw/bsp/<family>/boards/<board>/board.cmake` (or `board.mk`); family via
  `ls -d hw/bsp/*/boards/<board>`.
- Run on the host that owns the probe — config `test/hil/tinyusb.json` on ci,
  `local.json` on htpc (`hil` skill).
- Espressif boards (S3/P4): different toolchain, probe model, and PHY
  constraints entirely — read `esp-target-debug` first.

## Pick the least intrusive technique that can answer the question

Observation can mask the bug — the ch32v307 Heisenbug changed behavior under
logging *and* under the debugger. If the bug disappears when instrumented,
that IS a finding (timing-sensitive): move down in intrusiveness, not up.

| Technique                          | Intrusiveness                  | Reach for it when                                       |
|------------------------------------|--------------------------------|---------------------------------------------------------|
| PC-sampling                        | none — no halt, no code change | core wedged/spinning somewhere unknown (rusb2 FRDY)     |
| SWO exception trace / hw PC-sample | none — needs SWO pin wired     | ISR ordering/timing with zero code change               |
| DWT data trace                     | none — needs SWO pin wired     | stream one address's accesses: value + accessor PC      |
| Vector catch                       | none until a fault fires       | crash-shaped wedges — autopsy AT the faulting pc        |
| RAM ring-buffer                    | ~tens of cycles per event      | ISR ordering/timing bugs (musb babble)                  |
| TU_LOG (RTT)                       | µs per line                    | logic bugs that survive logging (J-Link or OpenOCD rtt) |
| TU_LOG (UART)                      | ms per line — blocking write   | same, when no debug-probe RTT path                      |
| dprintf / conditional breakpoint   | halt+resume per hit (~ms)      | low-rate probes post-wedge; never ISR-rate events       |
| GDB halt / breakpoints             | stops USB service entirely     | post-mortem state autopsy once wedged                   |

## PC-sampling, SWO trace & DWT data trace — watch without halting

`DWT_PCSR` (0xE000101C) returns the current PC on every read, target running
(Cortex-M3+; optional on M0+, reads 0 if absent; 0xFFFFFFFF = core halted or
WFI-asleep — `mem32 E000EDF0, 1`, DHCSR bit 17 S_HALT, tells which). One
probe serves one client: quit JLinkExe before starting JLinkGDBServer on the
same probe. Nailed the rusb2 FRDY wedge:

```bash
for i in $(seq 300); do echo 'mem32 E000101C, 1'; done \
  | JLinkExe -device $JLINK_DEVICE -SelectEmuBySN <uid> -if swd -speed 4000 -autoconnect 1 -nogui 1 \
  | awk '/E000101C = /{print $3}' | sort | uniq -c | sort -rn | head
arm-none-eabi-addr2line -e <firmware.elf> -f -a 0x<hot-pc> ...   # PCs → functions
```

OpenOCD variant: repeat `mdw 0xE000101C` over telnet :4444. The histogram's
top entries are the spin site; a flat histogram = core is servicing normally.

### SWO — hardware-timed trace on one pin (J-Link; verified on F407)

If SWO (TRACESWO) is wired, DWT emits packets with ZERO code change:
**exception trace** (DWT_CTRL bit16 — every IRQ enter/exit, timestamped) and
**hardware PC sampling** (bit12), better histograms than DWT_PCSR polling.
SWOViewer tools decode only ITM *stimulus* (TinyUSB emits none) — capture
raw:

```bash
# JLinkExe -CommandFile:
w4 E0001000, 0x00011401      # EXCTRCENA|PCSAMPLENA|SYNCTAP|CYCCNTENA
SWOStart 4000000             # explicit speed — autodetect fails headless
Sleep 3000
SWORead                      # hex: 0x17+4B LE = PC sample, 0x0E+2B = IRQ enter/exit
```

Verified: 680 KB in 3 s (flash-range PC samples + SysTick enter/exit).
SWORead stuck at 0 = SWO pin not wired (many boards route only SWDIO/SWCLK).
Restore DWT_CTRL when done.

### DWT data trace — stream one variable's accesses (value + PC), zero code

The watchpoint comparators' non-halting sibling (ARMv7-M ARM Table C1-21;
absent on ARMv6-M): emit a packet on every access to a watched address
instead of halting. Verified on F407 (J-Link) and H743 (OpenOCD/ST-Link) —
both streamed `system_ticks`' live value plus the accessor PC
(`tusb_time_millis_api`):

```bash
w4 E0001020, <&variable>   # DWT_COMP0 (JLinkExe shown; OpenOCD: same via mww)
w4 E0001024, 0             # DWT_MASK0 = exact address
w4 E0001028, 0x3           # FUNCTION 0b0011: value + accessor-PC packets (0b0010: value only)
# stream: 0x47+4B = accessor PC, 0x87+4B = value, 0x70 = timestamp
```

Caveats: traces reads AND writes (no write-only encoding) — a variable the
main loop polls floods the pipe with read packets and squeezes out value
packets (seen on F407); disarm (`FUNCTION=0`) when done; costs one of the
DWT comparators.

### Enabling SWO — the chain, and the vendor part that bites

DEMCR.TRCENA → ITM (TCR/TER) → SWO/TPIU (protocol + prescaler) → pin mux.
Tools set the first three (`SWOStart` on SEGGER; `swo`/`tpiu` object
`enable` + `itm ports on` on OpenOCD) — pin mux and trace clocks are
per-family:

- STM32F4: debug pins default to trace — nothing to configure.
- STM32H7 (verified, ST-Link): DBGMCU trace clocks + **PB3 muxed to AF0 by
  hand** + native `stlink-dap.cfg` (the hla transport's tpiu path silently
  does nothing) + the cfg-provided `stm32h7x.swo` object (`stm32h7x.tpiu`
  is the parallel port — rejects uart). traceclk = c_ck 400 MHz, not HCLK:
  too-slow guesses give ratio-garbled bytes, too-fast gives silence.

```bash
openocd -f interface/stlink-dap.cfg -c 'adapter serial <uid>' -f target/stm32h7x.cfg -c init \
 -c "mww 0x5C001004 0x00700000" \
 -c 'set m [read_memory 0x58020400 32 1]; mww 0x58020400 [expr {([lindex $m 0] & ~0xC0) | 0x80}]' \
 -c 'set a [read_memory 0x58020420 32 1]; mww 0x58020420 [expr {[lindex $a 0] & ~0xF000}]' \
 -c "stm32h7x.swo configure -protocol uart -traceclk 400000000 -pin-freq 2000000 -output /tmp/swo.bin" \
 -c "stm32h7x.swo enable" -c "itm ports on" \
 -c "sleep 3000" -c "stm32h7x.swo disable" -c shutdown   # then decode /tmp/swo.bin
```

## Vector catch + fault autopsy — catch the crash, not the wedge

A wedge that is really a fault (HardFault loop, lockup) autopsies best AT
the faulting instruction. Two hardware-proven gotchas: **FPB/DWT comparators
survive reflash and dead sessions** — a stale one fires as a phantom SIGTRAP
at an unrelated line of NEW firmware — and J-Link's reset strategy manages
vector-catch bits: scrub first, arm AFTER reset:

```gdb
# scrub: FP_COMP0..5 = 0xE0002008..201C, DWT_FUNCTIONn = 0xE0001028 + n*0x10
set *(unsigned*)0xE0002008 = 0
# ... (repeat per comparator; count = the GDB section's budget reads)
# arm (after monitor reset; tool-agnostic — works via JLinkExe w4 too):
set *(unsigned*)0xE000EDFC |= (1<<10)|(1<<9)|(1<<8)|(1<<7)|(1<<6)|(1<<5)|(1<<4)
# = VC_HARDERR|INTERR|BUSERR|STATERR|CHKERR|NOCPERR|MMERR; bit0 VC_CORERESET halts at reset
```

OpenOCD native: `cortex_m vector_catch hard_err bus_err state_err chk_err mm_err`.
It halts at exception ENTRY (pc = handler, LR = EXC_RETURN 0xFFFFFFFx); decode:

```gdb
p/x *(unsigned*)0xE000ED28   # CFSR — low byte MemManage, byte1 BusFault, top half UsageFault
p/x *(unsigned*)0xE000ED2C   # HFSR — bit30 FORCED = an escalated lower-priority fault
p/x *(unsigned*)0xE000ED38   # BFAR — faulting address (valid if CFSR bit15 BFARVALID)
x/8wx $msp                   # stacked frame: r0 r1 r2 r3 r12 lr pc xpsr — pc = culprit
# frame is on PSP when EXC_RETURN bit2 is set (LR = 0xFFFFFFFD — FreeRTOS
# tasks run on PSP): then x/8wx $psp instead. LR 0xFFFFFFF1/E9 = MSP.
```

`addr2line -e <elf> <stacked pc>` names the line (verified: CFSR 0x8200,
BFAR = the bad address, stacked pc = the faulting ldr). Loads fault
precisely; stores usually IMPRECISERR (BFAR invalid, pc late). ARMv6-M has no
CFSR/BFAR, only VC_HARDERR|VC_CORERESET — stacked frame alone. Still a halt
(host URB timeouts apply); clear DEMCR (`&= ~0x7F0`) before handing back;
RISC-V: breakpoint the trap handler; mcause/mepc/mtval are the CFSR/BFAR analogs.

## RAM ring-buffer trace

The zero-print instrument (cracked the musb babble): a small event ring in the
dcd/hcd, dumped over GDB after the failure. Single-writer (ISR) — no locking:

```c
typedef struct { uint16_t ev; uint16_t a; uint32_t b; } dbg_ev_t;
#define DBG_N 512                          // power of two
static volatile dbg_ev_t dbg_ring[DBG_N];  // volatile REQUIRED: -Os dead-store-
static volatile uint32_t dbg_wr;           // eliminates a write-only static array
static inline void DBG_EV(uint16_t ev, uint16_t a, uint32_t b) {
  uint32_t i = dbg_wr++;
  dbg_ring[i & (DBG_N - 1)] = (dbg_ev_t){ ev, a, b };
}
// call sites: DBG_EV(__LINE__, ep_addr, count);  — __LINE__ as event id
```

After building, `nm` the ELF for `dbg_ring`/`dbg_wr` — if they're missing the
compiler deleted your instrument and the run will "reproduce" with an empty ring.

Order is the index; if durations matter add a `uint32_t t = DWT->CYCCNT` field
(enable once: `CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk; DWT->CTRL |= 1;`
RISC-V: read `mcycle`). Let the failure happen, halt, then:

```gdb
p dbg_wr                          # total events; oldest slot = dbg_wr & (DBG_N-1) once wrapped
p dbg_ring
dump binary memory /tmp/ring.bin &dbg_ring[0] &dbg_ring[512]
```

## TU_LOG capture

Build with `LOG=2` (`LOG=3` adds per-transfer noise and much more timing skew).
`LOGGER=rtt` routes it over the debug probe — no UART wiring. SEGGER's host
tools need a J-Link, but OpenOCD serves the same RTT buffer on ST-Link /
CMSIS-DAP / WCH-Link boards:

```bash
# RTT: JLinkGDBServer from CLAUDE.md "GDB Debugging" + -RTTTelnetPort, then:
timeout 20s JLinkRTTClient > /tmp/rtt.log        # non-interactive capture
# UART (board's debug serial, if wired):
stty -F /dev/ttyACM<N> 115200 raw && timeout 20s cat /dev/ttyACM<N> | tee /tmp/uart.log
```

```bash
# OpenOCD RTT (any probe OpenOCD drives) — in telnet :4444 (or -c equivalents):
rtt setup 0x20000000 0x8000 "SEGGER RTT"   # RAM ORIGIN + LENGTH (from the .ld/map)
rtt start                                  # after firmware booted; rerun after each reflash
rtt server start 19021 0
# then:  timeout 20s nc localhost 19021 > /tmp/rtt.log
```

OpenOCD polls — bursty logs can drop lines; prefer J-Link where both
exist. The drain-model warning below applies unchanged.

An RTT-built firmware that has since wedged still holds a log tail in RAM —
but ONLY what fits the drain model: the default SEGGER mode (NO_BLOCK_SKIP)
**drops** writes once the ring fills with no reader, so an undrained target
holds the first KB after boot, not the wedge tail. There is no overwrite mode
in stock SEGGER RTT (only SKIP/TRIM/BLOCK): post-mortem RTT is evidence only
if a live drain was running — otherwise instrument with the RAM ring above.
Use `JLinkGDBServer -RTTTelnetPort 19021` + `JLinkRTTClient` for the drain
(proven; note the server briefly halts the core on connect). `JLinkRTTLogger`
fails to find the control block on some parts (LPC4088) even when it exists
and even given `-RTTAddress`; don't fight it — `nm` the ELF for `_SEGGER_RTT`,
read the aUp[0] descriptor (`mem32`), `savebin` the buffer — debug-AP RAM
reads don't halt the target.

## GDB — state autopsy and watchpoints

Connect/load recipes per probe family (J-Link, OpenOCD for ST-Link /
CMSIS-DAP / WCH-Link) are in CLAUDE.md "GDB Debugging"; script sessions with
JLinkGDBServer `-singlerun` — the server exits with the connection, and
back-to-back relaunches race the probe handle and hang. Release builds keep
DWARF (`MinSizeRel`), so `p`/struct access works on HIL firmware.

**Autopsy of a wedged board: attach and halt ONLY** — skip CLAUDE.md's
`monitor reset halt` + `load` (those are for fresh starts; a reset destroys
the evidence). Symbolize with the ELF that is actually flashed —
`<build root>/cmake-build-<board>/<example>/<example>.elf` from the run that
wedged; do not rebuild while the wedge is still on the board. The debug-loop
specifics:

```gdb
p/x _usbd_dev.ep_status          # device stack: usbd [epnum][dir] (1=IN): busy/stalled/claimed
p _usbh_devices[0]               # host stack: usbh per-device state (addr, enum/config)
p/x <port's private state>       # per-port names — read the board's dcd_*.c first
x/32wx <USB peripheral base>     # raw EP/FIFO regs; base = the macro the dcd uses
watch  xfer_status[2][1].total_len    # HW watchpoint (Cortex-M: ~4); dwc2 names shown
break dcd_int_handler            # works, but see warning below
```

**Hardware budget — read it off the chip** (verified: F407/M4 = 6 bp + 4 wp,
rp2040/M0+ = 4 + 2; M7 typically 8/4):

```gdb
p ((*(unsigned*)0xE0002000)>>4) & 0xF   # FPB NUM_CODE = hw breakpoints (M7 adds bits[14:12])
p (*(unsigned*)0xE0001000)>>28          # DWT_CTRL NUMCOMP = watchpoint comparators
```

- `hbreak`/`thbreak` force a hardware breakpoint (software breaks in flash
  need flash-breakpoint support — J-Link has it; OpenOCD: `bp <addr> 2 hw`);
  `tbreak` = one-shot.
- `watch -l <expr>` watches the address expr evaluates to once — almost
  always what you want; `rwatch`/`awatch` trap reads/any access (hardware-
  only — they error, never fall back). OpenOCD adds a data-VALUE match GDB
  can't express: `wp <addr> 4 w <value> [mask]` — catch who writes 0 into a
  busy flag, ignoring writes of 1.
- **Demand the word "Hardware" in the confirmation.** `watch` silently falls
  back to a software watchpoint when no DWT comparator fits — GDB then
  single-steps the whole program, hundreds of times slower: certain USB
  death. Plain `Watchpoint 2:` = delete it and narrow the expression
  (`watch -l`, cast to a 4-byte int).
- Conditional breaks (`break ... if ep_addr==0x81`) and `dprintf
  <loc>,"fmt",args` (printf without recompiling; keep `dprintf-style gdb`)
  are host-evaluated — no Cortex-M agent expressions in our stubs: every hit
  halts+resumes (~ms) even when the condition is false. Post-wedge/cold
  paths only; ISR-rate events belong in the RAM ring.
- `commands <bpnum> ... end` (start `silent`, end `continue`) auto-collects
  evidence per hit — same halt-per-hit cost.
- Stepping with the USB ISR firing between steps is chaos: OpenOCD
  `cortex_m maskisr steponly`. The bus runs either way — the host may still
  reset a halted-looking device.
- Poking state while halted (`set var _usbd_dev.ep_status[2][1].busy = 0`)
  tests a hypothesis but invalidates the post-mortem — dump first, poke after.
- FreeRTOS examples: `-rtos GDBServer/RTOSPlugin_FreeRTOS` (OpenOCD: `-rtos
  FreeRTOS`) → `info threads` lists every task with state/prio/frame
  (verified: 6 tasks). It populates only after a
  run→stop cycle — plain attach shows one 0xDEAD placeholder. Semihosting is
  never the answer (traps + halts per call — RTT instead). **Monitor-mode
  debugging** (J-Link, M3+) keeps chosen IRQs serviced at a breakpoint —
  needs SEGGER's JLINK_MONITOR files + `SetMonModeDebug=1`; not set up here:
  <https://kb.segger.com/Monitor_Mode_Debugging> (untested).

While halted the device answers **nothing**: host control transfers time out
in ~5 s and the OS may reset/re-enumerate — after `continue`, the bus traffic
shows recovery, not the original bug. Prefer one halt for a post-mortem dump
over stepping through live USB traffic.

## Dual-side capture — the default for enumeration/transfer bugs

Start both channels, then trigger the failing test (Linux-PC-host shown;
TinyUSB-as-host: swap usbmon for `usb-sniffer`, + `usb-kernel-debug` on a
Linux gadget peer):

```bash
.claude/skills/usbmon/scripts/usbcap.sh cafe: 30 /tmp/host.pcapng &   # host URBs (usbmon skill)
timeout 30s JLinkRTTClient > /tmp/target.rtt &                        # target (or ring dump after)
wait
```

RTT lines and ring events carry no wall-clock: correlate on unambiguous
anchors — bus reset, SET_ADDRESS, the first transfer on the failing EP — then
lay device events between anchors in host-URB order. Logging the SOF/frame
number on the target gives a shared clock when you need finer alignment.
When host and target evidence disagree, or the host sees nothing at all, add
the wire itself: `usb-sniffer` skill (hardware tap, PID-level).

## Manuals

- J-Link (UM08001): <https://kb.segger.com/UM08001_J-Link_/_J-Trace_User_Guide> — flash breakpoints, RTT, SWO, monitor mode, Commander.
- OpenOCD: <https://openocd.org/doc/html/index.html> — `rtt`, `bp`/`wp`, `cortex_m vector_catch`/`maskisr`, `itm`/`tpiu`.
- "Debugging with GDB" (§5.1 = break/watch/dprintf): Tenth Edition (GDB 18)
  via calibre/`read-doc`, or
  `curl -sL -o /tmp/gdb.pdf https://sourceware.org/gdb/current/onlinedocs/gdb.pdf`
  (the HTML mirror blocks fetchers). Installed `arm-none-eabi-gdb`
  `help <cmd>` is authoritative here.

## Warnings

- **Halting/resetting via the probe does NOT disconnect the device**: a DWC2
  soft-connect pullup stays up through core halt *and* reset, so the host's
  stuck URBs stay stuck and a wedged DUT stays wedged — recover the Linux
  host side with the `usb-kernel-recover` skill.
- **A bug that vanishes under LOG=2 is a timing bug**, not fixed: switch to
  the ring buffer; if it vanishes under GDB too, PC-sampling only.
- **UART TU_LOG blocks in the write path** (worst perturbation, including
  inside the ISR); RTT is much cheaper but not free; `LOG=3` multiplies both.
- Flash/GDB only with the board lock held; a `hold` refused with reason
  `hil_test.py` means CI is mid-test on that board — wait, don't force.
- **Instrumentation is temporary**: before `release`, reflash pristine
  firmware (the next CI run must not inherit a debug build) and revert the
  instrumentation diff — or hand it over explicitly with the diagnosis.
- **A register snapshot without a validity anchor lies**: J-Link tool sessions
  can reset or briefly halt the DUT as a side effect, and a snapshot of a
  freshly-reset chip (e.g. NVIC ISER = 0) reads like a smoking gun. Read DHCSR
  (0xE000EDF0: bit 17 S_HALT, bit 25 S_RESET_ST) with every snapshot, and
  cross-check against something the device demonstrably still does.
- **"Flash OK" can lie** (silent no-op — old firmware keeps running). When
  behavior contradicts the flashed code: `objcopy -O binary fw.elf
  /tmp/fw.bin`, then `verifybin /tmp/fw.bin,<flash-base>` (J-Link, verified)
  or `verify_image` (OpenOCD); on mismatch reflash before debugging further.
- **A marginal link can fake a deterministic firmware bug** — down to failing
  the same test at the same iteration twice. "USB disconnect" in dmesg on a
  freshly re-cabled port (high devnum = churn) means the plug, not the code:
  first sustained bulk traffic is when a bad contact drops. Before declaring a
  regression, re-run the OLD build on the SAME link state — and if a bisect
  exonerates every hunk, believe it: re-test the exact failing binary.
- **Release your manual lock before `hil_test.py`** — it self-locks each board
  and fails immediately on your own hold (`hil` skill).
