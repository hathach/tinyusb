---
name: usb-target-debug
description: Use when a TinyUSB device misbehaves on real hardware and host-side capture can't explain it — a HIL test fails but usbmon shows only Submits with no Completes, the device silently NAKs, wedges, STALLs, babbles, or drops data, EP0 starves, an ISR or DCD/HCD state bug is suspected — and you need device-side evidence: TU_LOG/RTT logs, GDB state dumps, a RAM ring-buffer event trace, or PC-sampling of where the core spins.
---

# usb-target-debug — device-side capture & debugging on the HIL rig

Completes the debugging trio (the `usb-sniffer` skill adds a fourth,
wire-level view when hardware tapping is available):

| Skill | Answers |
|---|---|
| `usbmon` | what the host actually exchanged (URBs) |
| `usb-debug` | why the host acted (dmesg / dynamic debug) |
| **`usb-target-debug`** | **what the device did** (logs, driver state, PC) |
| `usb-sniffer` | what crossed the wire (PIDs, handshakes, resets — hardware tap) |

For enumeration/transfer bugs the default posture is **dual-side capture** —
usbmon on the host *and* a target-side channel, simultaneously — not
host-first-then-escalate.

## Rig discipline — lock first, always

Hold the board lock for the WHOLE manual session; never stop the
actions-runner (see the `hil` skill for the full lock protocol):

```bash
python3 test/hil/board_lock.py hold <board> --reason "target debug: <bug>"
# ... instrument / build / flash / capture / GDB ...
python3 test/hil/board_lock.py release <board>
```

Board → probe mapping: `test/hil/tinyusb.json` — `flasher.name` is the probe
family, `flasher.uid` the **probe serial** (many identical probes on the rig:
J-Link needs `-SelectEmuBySN <uid>` / GDB server `-select usb=<uid>`; OpenOCD
`-c 'adapter serial <uid>'`). `JLINK_DEVICE` / `OPENOCD_OPTION` come from
`hw/bsp/<family>/boards/<board>/board.cmake` (or `board.mk`); find the family
with `ls -d hw/bsp/*/boards/<board>`. Run on the host that owns the probe —
config is `test/hil/tinyusb.json` on ci, `local.json` on htpc (`hil` skill).

## Pick the least intrusive technique that can answer the question

Observation can mask the bug — the ch32v307 Heisenbug changed behavior under
logging *and* under the debugger. If the bug disappears when instrumented,
that IS a finding (timing-sensitive): move down in intrusiveness, not up.

| Technique | Intrusiveness | Reach for it when |
|---|---|---|
| PC-sampling | none — no halt, no code change | core wedged/spinning somewhere unknown (rusb2 FRDY) |
| RAM ring-buffer | ~tens of cycles per event | ISR ordering/timing bugs (musb babble) |
| TU_LOG (RTT) | µs per line | logic bugs that survive logging |
| TU_LOG (UART) | ms per line — blocking write | same, when no J-Link on the board |
| GDB halt / breakpoints | stops USB service entirely | post-mortem state autopsy once wedged |

## TU_LOG capture

Build with `LOG=2` (`LOG=3` adds per-transfer noise and much more timing skew).
`LOGGER=rtt` routes it over the debug probe (J-Link only) — no UART wiring:

```bash
# RTT: JLinkGDBServer from CLAUDE.md "GDB Debugging" + -RTTTelnetPort, then:
timeout 20s JLinkRTTClient > /tmp/rtt.log        # non-interactive capture
# UART (board's debug serial, if wired):
stty -F /dev/ttyACM<N> 115200 raw && timeout 20s cat /dev/ttyACM<N> | tee /tmp/uart.log
```

An RTT-built firmware that has since wedged still holds a log tail in RAM —
but ONLY what fits the drain model: the default SEGGER mode (NO_BLOCK_SKIP)
**drops** writes once the ring fills with no reader, so an undrained target
holds the first KB after boot, not the wedge tail. There is no overwrite mode
in stock SEGGER RTT (only SKIP/TRIM/BLOCK): post-mortem RTT is evidence only
if a live drain was running — otherwise instrument with the RAM ring below.
Use `JLinkGDBServer -RTTTelnetPort 19021` + `JLinkRTTClient` for the drain
(proven; note the server briefly halts the core on connect). `JLinkRTTLogger`
fails to find the control block on some parts (LPC4088) even when it exists
and even given `-RTTAddress`; don't fight it — `nm` the ELF for `_SEGGER_RTT`,
read the aUp[0] descriptor (`mem32`), `savebin` the buffer — debug-AP RAM
reads don't halt the target.

## GDB — state autopsy and watchpoints

Connect/load recipes per probe family (J-Link, OpenOCD for ST-Link /
CMSIS-DAP / WCH-Link) are in CLAUDE.md "GDB Debugging". Release builds keep
DWARF (`MinSizeRel`), so `p`/struct access works on HIL firmware.

**Autopsy of a wedged board: attach and halt ONLY** — skip CLAUDE.md's
`monitor reset halt` + `load` (those are for fresh starts; a reset destroys
the evidence). Symbolize with the ELF that is actually flashed —
`<build root>/cmake-build-<board>/<example>/<example>.elf` from the run that
wedged; do not rebuild while the wedge is still on the board. The debug-loop
specifics:

```gdb
p/x _usbd_dev.ep_status          # usbd core [epnum][dir] (1=IN): busy/stalled/claimed
p/x <port's private state>       # per-port names — read the board's dcd_*.c first
x/32wx <USB peripheral base>     # raw EP/FIFO regs; base = the macro the dcd uses
watch  xfer_status[2][1].total_len    # HW watchpoint (Cortex-M: ~4); dwc2 names shown
break dcd_int_handler            # works, but see warning below
```

While halted the device answers **nothing**: host control transfers time out
in ~5 s and the OS may reset/re-enumerate — after `continue`, the bus traffic
shows recovery, not the original bug. Prefer one halt for a post-mortem dump
over stepping through live USB traffic.

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

## PC-sampling (J-Link) — find where the core spins, without halting

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

## Dual-side capture — the default for enumeration/transfer bugs

Start both channels, then trigger the failing test:

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

## Warnings

- **Halting/resetting via the probe does NOT disconnect the device**: a DWC2
  soft-connect pullup stays up through core halt *and* reset, so the host's
  stuck URBs stay stuck and a wedged DUT stays wedged — recover the host side
  with the `usb-recover` skill.
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
- **A marginal link can fake a deterministic firmware bug** — down to failing
  the same test at the same iteration twice. "USB disconnect" in dmesg on a
  freshly re-cabled port (high devnum = churn) means the plug, not the code:
  first sustained bulk traffic is when a bad contact drops. Before declaring a
  regression, re-run the OLD build on the SAME link state — and if a bisect
  exonerates every hunk, believe it: re-test the exact failing binary.
- **Release your manual lock before `hil_test.py`** — it self-locks each board
  and fails immediately on your own hold (`hil` skill).
