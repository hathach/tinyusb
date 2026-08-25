# target-debug Skill & target-debugger Agent Enhancement Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Extend `.claude/skills/target-debug/SKILL.md` (and its agent) with the full debugger facility arsenal from the J-Link, OpenOCD, and GDB manuals — breakpoint/watchpoint depth, OpenOCD RTT, vector catch + fault autopsy, SWO/ITM trace, flash verification — each recipe hardware-verified on the ci rig before it lands unmarked.

**Architecture:** The skill's organizing spine is its intrusiveness table ("pick the least intrusive technique that can answer the question"); every new facility slots into that model with an honest cost row. Recipes keep the existing dense, copy-paste style. The skill's value is that its recipes are *proven on this rig* — so each task pairs drafting with a bounded hardware verification, and anything unverifiable lands tagged `(untested)` or is dropped.

**Tech Stack:** arm-none-eabi-gdb 15.2, OpenOCD 0.12.0+dev, SEGGER J-Link V7.94b (`JLinkExe`, `JLinkGDBServer`, `JLinkSWOViewerCLExe`), ci rig boards from `test/hil/tinyusb.json` (10 jlink / 6 openocd / 1 stlink probes).

**Reference manual:** "Debugging with GDB", **Tenth Edition** (for GDB 18.0.50) — prefer the calibre-library copy via the `read-doc` skill, but **verify the edition on the title page first**: the library also holds an outdated Ninth Edition (2002, GDB 5.1.1, txt) that predates `dprintf`/`watch -l` — do not use it. Fallback fetch: `curl -sL -o /tmp/gdb.pdf https://sourceware.org/gdb/current/onlinedocs/gdb.pdf` (HTML pages block fetchers; the PDF does not). Sections used by this plan: §5.1.2 Setting Watchpoints, §5.1.6 Break Conditions, §5.1.7 Breakpoint Command Lists, §5.1.8 Dynamic Printf (PDF page = book page + 18). NOTE: the manual documents GDB 18; the rig runs 15.2 — the installed `arm-none-eabi-gdb`'s `help <cmd>` is authoritative for feature availability.

## Global Constraints

- Worktree: `/home/hathach/code/tinyusb/.claude/worktrees/improve-debug-skill-agent`, branch `claude/improve-debug-skill-agent`. All paths below are relative to it.
- J-Link User Guide link must be exactly `https://kb.segger.com/UM08001_J-Link_/_J-Trace_User_Guide` (user-specified, verified live 2026-07-23).
- **Hardware-verify before landing**: a recipe is committed unmarked only with captured evidence from a rig board; otherwise tag it `(untested)` inline or drop it. Record evidence (command + output snippet) in the task's commit message body.
- Rig discipline (from `hil` + `target-debug` skills): `python3 test/hil/board_lock.py hold <board> --reason "skill-enhance verify: <what>"` before touching hardware, `release` after; reflash pristine firmware before release; NEVER stop the actions-runner; one J-Link client per probe at a time; we are ON host `ci` (config `test/hil/tinyusb.json`).
- Hardware tasks are strictly serial (one board session at a time). Bash timeouts ≥ 10 min for flash+debug cycles.
- Style: match the skill's existing voice — dense, recipe-first, caveats inline. Skill word budget after all tasks: ≤ 2 700 words (`wc -w`, currently 1 763).
- Run `pre-commit run --files <changed>` before every commit. No Co-Authored-By trailers.
- Board selection is runtime data (boards come/go, locks): resolve with the exact python snippet in Task 2 Step 2 and reuse `$JB` (jlink board) / `$OB` (openocd board) thereafter.

---

### Task 1: Manuals reference block

**Files:**
- Modify: `.claude/skills/target-debug/SKILL.md` (insert new `## Manuals` section immediately before `## Warnings`)

**Interfaces:**
- Produces: `## Manuals` section that later tasks' text may reference as "see Manuals".

- [x] **Step 1: Insert the Manuals section**

In `.claude/skills/target-debug/SKILL.md`, find the line `## Warnings` and insert immediately before it:

```markdown
## Manuals

- J-Link / J-Trace User Guide (UM08001): <https://kb.segger.com/UM08001_J-Link_/_J-Trace_User_Guide> — flash breakpoints, RTT, SWO, monitor mode, Commander commands.
- OpenOCD User's Guide: <https://openocd.org/doc/html/index.html> — `rtt`, `bp`/`wp`, `cortex_m vector_catch` / `maskisr`, `itm`/`tpiu`.
- "Debugging with GDB" (the official manual; §5.1 covers break/watch/dprintf):
  calibre library first (`read-doc` skill) — use the **Tenth Edition (GDB 18)**
  copy, not the 2002 Ninth-Edition txt also present; fallback
  `curl -sL -o /tmp/gdb.pdf https://sourceware.org/gdb/current/onlinedocs/gdb.pdf`
  (the HTML mirror blocks fetchers; the PDF works). The installed
  `arm-none-eabi-gdb`'s `help <cmd>` is authoritative for what this rig runs.

```

- [x] **Step 2: Verify formatting and word count**

Run: `cd /home/hathach/code/tinyusb/.claude/worktrees/improve-debug-skill-agent && grep -A5 '^## Manuals' .claude/skills/target-debug/SKILL.md && wc -w .claude/skills/target-debug/SKILL.md`
Expected: section present before `## Warnings`; word count ≤ 1 830.

- [x] **Step 3: Commit**

```bash
cd /home/hathach/code/tinyusb/.claude/worktrees/improve-debug-skill-agent
pre-commit run --files .claude/skills/target-debug/SKILL.md
git add .claude/skills/target-debug/SKILL.md
git commit -m "docs(target-debug): link J-Link UM08001, OpenOCD and GDB manuals"
```

---

### Task 2: Breakpoint & watchpoint arsenal (GDB + OpenOCD)

**Files:**
- Modify: `.claude/skills/target-debug/SKILL.md` — extend the `## GDB — state autopsy and watchpoints` section
- Read-only reference: `test/hil/tinyusb.json` (board resolution)

**Interfaces:**
- Consumes: nothing from other tasks.
- Produces: board env vars `$JB`, `$OB` resolution snippet (reused by Tasks 3-6); the "halt-per-hit cost model" wording that Task 7's table row cites.

- [x] **Step 1: Draft the section extension**

In `.claude/skills/target-debug/SKILL.md`, the GDB section currently ends with the paragraph beginning `While halted the device answers **nothing**`. Insert immediately BEFORE that paragraph:

```markdown
**Hardware budget — read it off the chip, not from memory** (counts differ
per core: M0+ typically 4 bp/2 wp, M3/M4 6/4, M7 8/4):

```gdb
p ((*(unsigned*)0xE0002000)>>4) & 0xF   # FPB NUM_CODE = hw breakpoints (M7 adds bits[14:12])
p (*(unsigned*)0xE0001000)>>28          # DWT_CTRL NUMCOMP = watchpoint comparators
```

- `hbreak`/`thbreak` force a hardware breakpoint (code in flash can't take a
  software break unless the probe does flash breakpoints — J-Link does,
  OpenOCD needs `bp <addr> 2 hw`); `tbreak` = one-shot.
- `watch -l <expr>` watches the *address* the expression evaluates to once —
  cheap and what you almost always want; `rwatch`/`awatch` trap reads/any
  access (hardware-only — they error rather than fall back). OpenOCD (telnet
  :4444) adds a data-VALUE match GDB cannot express: `wp <addr> 4 w <value>
  [mask]` — fires only when the written value matches (e.g. catch who writes
  0 into a busy flag, ignoring writes of 1).
- **Demand the word "Hardware" in the confirmation.** `watch` silently falls
  back to a SOFTWARE watchpoint when no DWT comparator fits (expression too
  wide/complex, budget exhausted): GDB then single-steps the whole program —
  hundreds of times slower, certain USB death. `Watchpoint 2:` without
  "Hardware" = delete it; `set can-use-hw-watchpoints 1` is the default but
  narrowing the expression (`watch -l`, cast to a 4-byte int) is the real fix.
- Conditional breaks/watches (`break dcd_edpt_xfer if ep_addr==0x81`) are
  evaluated by GDB on the HOST with our stubs — neither JLinkGDBServer nor
  OpenOCD supports target-side agent expressions on Cortex-M — so every hit
  is a halt+resume (~ms) whether the condition matches or not: fine
  post-wedge or on cold paths, wrong under live USB traffic.
- `commands <bpnum> ... end` auto-runs GDB commands at each hit (start with
  `silent`, end with `continue` for hands-free evidence collection) — same
  halt-per-hit cost.
- `dprintf <loc>,"fmt",args` = printf without recompiling. Stay on the
  default `dprintf-style gdb` (host prints): the `call` style runs the
  target's own printf mid-halt and `agent` needs stub support — neither is
  viable on these probes. Same cost model as conditional breaks; for
  ISR-rate events use the RAM ring buffer instead.
- Stepping while the USB ISR fires between every step is chaos: OpenOCD
  `cortex_m maskisr steponly` masks interrupts during single-steps only.
  The bus keeps running either way — the host may still reset a device that
  stops responding mid-step.
- While halted you can poke state to test a hypothesis (`set var
  _usbd_dev.ep_status[2][1].busy = 0`) — but that invalidates the snapshot
  as post-mortem evidence; dump first, poke after.
```

- [x] **Step 2: Resolve verification boards (runtime data)**

```bash
cd /home/hathach/code/tinyusb
python3 - <<'EOF'
import json
cfg = json.load(open('test/hil/tinyusb.json'))
jl = [b['name'] for b in cfg['boards'] if b['flasher']['name']=='jlink']
oo = [b['name'] for b in cfg['boards'] if b['flasher']['name']=='openocd']
print('JLINK candidates:', jl)
print('OPENOCD candidates:', oo)
EOF
```
Pick the first candidate of each that `python3 test/hil/board_lock.py status` shows unlocked; export as `JB=<jlink board>` `OB=<openocd board>`. Look up `flasher.uid` for each in `test/hil/tinyusb.json` (`JB_UID`, `OB_UID`) and `JLINK_DEVICE`/`OPENOCD_OPTION` from `hw/bsp/*/boards/$JB/board.cmake` (family via `ls -d hw/bsp/*/boards/$JB`).

- [x] **Step 3: Hardware-verify the budget reads on both probe families**

```bash
python3 test/hil/board_lock.py hold $JB --reason "skill-enhance verify: bp/wp budget"
printf 'mem32 E0002000, 1\nmem32 E0001000, 1\nqc\n' | \
  JLinkExe -device $JLINK_DEVICE -SelectEmuBySN $JB_UID -if swd -speed 4000 -autoconnect 1 -nogui 1
python3 test/hil/board_lock.py release $JB
```
Expected: two register values; decode NUM_CODE and NUMCOMP by hand and check they are plausible (2-8 range). Repeat for `$OB` via `openocd $OPENOCD_OPTION -c init -c 'mdw 0xE0002000' -c 'mdw 0xE0001000' -c shutdown` under its own lock.
If a register reads 0 on one board, note which core and adjust the skill text's example counts if contradicted.

- [x] **Step 4: Hardware-verify dprintf + commands round-trip on $JB**

With the board lock held and an already-flashed example (any; do not reflash), a JLinkGDBServer on :2331 (per CLAUDE.md GDB Debugging), run bounded — `commands` blocks cannot be passed via `-ex`, so use a command file:

```bash
cat > /tmp/bpcmd.gdb <<'EOF'
target remote :2331
set var $count=0
watch -l *(unsigned*)&_usbd_dev
delete
dprintf tud_task_ext,"tick\n"
break tud_task_ext
commands 3
silent
set var $count=$count+1
continue
end
continue&
shell sleep 3
interrupt
print $count
EOF
timeout 120 arm-none-eabi-gdb -batch -x /tmp/bpcmd.gdb \
  $(find examples/cmake-build-$JB -name 'cdc_msc.elf' | head -1)
```
Expected: the `watch` line answers `Hardware watchpoint 1:` (the word
"Hardware" present — this is the skill's software-fallback check, then
deleted), "tick" lines printed, and `$count > 0`. (`tud_task_ext` is the real
symbol — `tud_task` is an inline wrapper; the breakpoint is number 3 after
the watchpoint and dprintf.) Kill the GDB server, reflash pristine
(`ninja`-flash target or `hil_test.py` flash path), release the lock.

- [x] **Step 5: Apply the Step-1 text, run pre-commit, commit**

```bash
cd /home/hathach/code/tinyusb/.claude/worktrees/improve-debug-skill-agent
pre-commit run --files .claude/skills/target-debug/SKILL.md
git add .claude/skills/target-debug/SKILL.md
git commit -m "docs(target-debug): breakpoint/watchpoint arsenal with halt-per-hit cost model

Verified on <JB> (J-Link) + <OB> (OpenOCD): FPB/DWT budget reads, dprintf,
breakpoint command lists. <paste the two register values here>"
```

---

### Task 3: OpenOCD RTT — RTT is not J-Link-only

**Files:**
- Modify: `.claude/skills/target-debug/SKILL.md` — `## TU_LOG capture` section

**Interfaces:**
- Consumes: `$OB`, `$OB_UID`, `$OPENOCD_OPTION` from Task 2 Step 2.
- Produces: the corrected claim "RTT works on any OpenOCD-driven probe" that Task 7's agent text repeats.

- [x] **Step 1: Replace the J-Link-only claim**

In the `## TU_LOG capture` section, replace:

```markdown
Build with `LOG=2` (`LOG=3` adds per-transfer noise and much more timing skew).
`LOGGER=rtt` routes it over the debug probe (J-Link only) — no UART wiring:
```

with:

```markdown
Build with `LOG=2` (`LOG=3` adds per-transfer noise and much more timing skew).
`LOGGER=rtt` routes it over the debug probe — no UART wiring. SEGGER's host
tools need a J-Link, but OpenOCD serves the same RTT buffer on ST-Link /
CMSIS-DAP / WCH-Link boards:
```

- [x] **Step 2: Add the OpenOCD RTT recipe**

Immediately after the existing J-Link/UART capture code block (ends with `... | tee /tmp/uart.log`), add:

```markdown
```bash
# OpenOCD RTT (any probe OpenOCD drives) — in telnet :4444 (or -c equivalents):
rtt setup 0x20000000 0x8000 "SEGGER RTT"   # search range = RAM ORIGIN + LENGTH (from the .ld / map file)
rtt start                                  # after firmware booted; rerun after each reflash
rtt server start 19021 0
# then:  timeout 20s nc localhost 19021 > /tmp/rtt.log
```

OpenOCD polls the buffer (default 10 ms): bursty logs can drop lines a J-Link
would keep — prefer J-Link where both exist; the drain-model warning below
applies unchanged.
```

- [x] **Step 3: Hardware-verify on $OB**

```bash
python3 test/hil/board_lock.py hold $OB --reason "skill-enhance verify: openocd rtt"
cd examples/device/cdc_msc && cmake -B build-rtt -DBOARD=$OB -G Ninja -DCMAKE_BUILD_TYPE=MinSizeRel -DLOG=2 -DLOGGER=rtt && cmake --build build-rtt
# flash it (ninja -C build-rtt cdc_msc-openocd), then:
openocd $OPENOCD_OPTION &   # gdb :3333, telnet :4444
{ echo 'rtt setup 0x20000000 0x8000 "SEGGER RTT"'; echo 'rtt start'; echo 'rtt server start 19021 0'; sleep 1; } | nc -q1 localhost 4444
timeout 10s nc localhost 19021 > /tmp/ob_rtt.log; head /tmp/ob_rtt.log
```
Expected: TinyUSB boot banner / log lines in `/tmp/ob_rtt.log`. Adjust the search range from the board's linker script if the control block isn't found ("rtt: No control block found") and mirror any correction into the Step-2 text. Kill openocd, reflash pristine cdc_msc (no LOG), release lock, delete `build-rtt`.

- [x] **Step 4: Commit**

```bash
pre-commit run --files .claude/skills/target-debug/SKILL.md
git add .claude/skills/target-debug/SKILL.md
git commit -m "docs(target-debug): RTT via OpenOCD on non-J-Link probes

Verified on <OB>: rtt setup/start/server + nc capture of boot log.
<paste first captured log line>"
```

---

### Task 4: Vector catch + fault autopsy

**Files:**
- Modify: `.claude/skills/target-debug/SKILL.md` — new section after `## GDB — state autopsy and watchpoints`

**Interfaces:**
- Consumes: `$JB` from Task 2. (Corrected during execution: $OB/rp2040 is ARMv6-M — no CFSR/BFAR and only VC_HARDERR, so the full autopsy verify needs the ARMv7-M $JB; the payload is a bad LOAD because stores fault imprecisely with BFAR invalid.)
- Produces: section title `## Vector catch + fault autopsy` cited by Task 7's table row.

- [x] **Step 1: Insert the new section**

After the GDB section (i.e. before `## RAM ring-buffer trace`), insert:

```markdown
## Vector catch + fault autopsy — catch the crash, not the wedge

A "wedge" that is really a fault (HardFault loop, lockup) autopsies best AT
the faulting instruction, not minutes later. Arm before reproducing:

```gdb
# tool-agnostic (any probe, incl. J-Link): DEMCR trap bits — halt on fault
set *(unsigned*)0xE000EDFC |= (1<<10)|(1<<9)|(1<<8)|(1<<7)|(1<<6)|(1<<5)|(1<<4)
# = VC_HARDERR|INTERR|BUSERR|STATERR|CHKERR|NOCPERR|MMERR; bit0 VC_CORERESET halts at reset
```

OpenOCD native form: `cortex_m vector_catch hard_err bus_err state_err chk_err mm_err`.
When it fires the core halts at the fault; decode:

```gdb
p/x *(unsigned*)0xE000ED28   # CFSR — low byte MemManage, byte1 BusFault, top half UsageFault
p/x *(unsigned*)0xE000ED2C   # HFSR — bit30 FORCED = an escalated lower-priority fault
p/x *(unsigned*)0xE000ED38   # BFAR — faulting address (valid if CFSR bit15 BFARVALID)
x/8wx $msp                   # stacked frame: r0 r1 r2 r3 r12 lr pc xpsr — pc = culprit
```

`arm-none-eabi-addr2line -e <elf> <stacked pc>` names the line. Caveats: a
vector-catch halt is still a halt (host-side URB timeouts apply); the bits
persist until power-cycle — clear them (`... &= ~0x7F1`) before handing the
board back; RISC-V ports have no DEMCR — use a breakpoint on the trap handler.
```

- [x] **Step 2: Hardware-verify with a deliberate fault on $JB (ARMv7-M)**

Create the fault build (NOT committed):

```bash
python3 test/hil/board_lock.py hold $JB --reason "skill-enhance verify: vector catch"
cd examples/device/cdc_msc   # executed on $JB (stm32f407disco, ARMv7-M) via JLinkExe — see commit evidence
# temporary patch — revert after: fault 5 s after boot
python3 - <<'EOF'
import pathlib
p = pathlib.Path('src/main.c'); s = p.read_text()
import re
s = re.sub(r'\\nint main\\(void\\)',
  '\\nstatic void _fault_after_5s(void){ static uint32_t t0=0; if(!t0) t0=tusb_time_millis_api();'
  ' if(tusb_time_millis_api()-t0>5000) (void)*(volatile uint32_t*)0xCF000000u; }\\n\\nint main(void)', s, count=1)  # board_millis is gone; helper must sit after the includes
s = s.replace('led_blinking_task();', 'led_blinking_task(); _fault_after_5s();', 1)
p.write_text(s)
EOF
grep -n '_fault_after_5s' src/main.c   # expect 3 hits: definition + call + (none in decl block)
cmake -B build-fault -DBOARD=$JB -G Ninja -DCMAKE_BUILD_TYPE=MinSizeRel && cmake --build build-fault
```
(If `app_led_task`/`board_millis` anchors differ in the current `main.c`, place the same 3-line helper on whatever per-loop task function exists — the fault line `*(volatile uint32_t*)0xCF000000u = 0;` is the payload.)
Flash `build-fault`, then:

```bash
# executed variant: DEMCR armed + autopsy via JLinkExe command file on $JB (see commit c1d2d305f evidence); OpenOCD-native form:
openocd $OPENOCD_OPTION -c init -c 'cortex_m vector_catch hard_err bus_err' &
timeout 60 arm-none-eabi-gdb -batch -ex 'target remote :3333' -ex 'monitor reset run' \
  -ex 'shell sleep 8' -ex 'interrupt' \
  -ex 'p/x *(unsigned*)0xE000ED28' -ex 'p/x *(unsigned*)0xE000ED38' -ex 'x/8wx $msp' \
  build-fault/cdc_msc.elf
```
Expected: halted in the fault path, CFSR BusFault bits set, **BFAR = 0xCF000000**, stacked pc addr2lines to `_fault_after_5s`. If the write is silently ignored on this core (some buses RAZ/WI), switch payload to a NULL-function call `((void(*)(void))0x1)();` and note UsageFault/INVSTATE instead.

- [x] **Step 3: Clean up hardware state**

`git checkout -- src/main.c`, delete `build-fault/`, clear DEMCR bits (`set *(unsigned*)0xE000EDFC &= ~0x7F1` via a final gdb attach or power-cycle note), reflash pristine cdc_msc, `board_lock.py release $JB`.

- [x] **Step 4: Commit**

```bash
pre-commit run --files .claude/skills/target-debug/SKILL.md
git add .claude/skills/target-debug/SKILL.md
git commit -m "docs(target-debug): vector catch + Cortex-M fault autopsy recipe

Verified on <OB>: deliberate bad-address write halted via vector_catch,
CFSR=<val> BFAR=0xCF000000, stacked pc resolved by addr2line."
```

---

### Task 5: SWO/ITM experiment — exception trace & hardware PC sampling

This is an EXPERIMENT task with an explicit gate: the section lands **unmarked only if packets are actually captured** on a rig board; otherwise it lands tagged `(untested — SWO wiring unconfirmed on this rig)`. Budget: 30 min of hardware time, then decide.

**Files:**
- Modify: `.claude/skills/target-debug/SKILL.md` — new subsection inside the PC-sampling section (after the OpenOCD variant paragraph)

**Interfaces:**
- Consumes: `$JB`, `$JB_UID`, `$JLINK_DEVICE` from Task 2.
- Produces: verified-or-tagged status consumed by Task 7's table row for SWO.

- [x] **Step 1: Probe for SWO output (gate experiment)**

```bash
python3 test/hil/board_lock.py hold $JB --reason "skill-enhance verify: SWO"
# arm DWT sources while the fw runs (background mem write, no halt):
printf 'w4 E0001000, 0x00011401\nqc\n' | JLinkExe -device $JLINK_DEVICE -SelectEmuBySN $JB_UID -if swd -speed 4000 -autoconnect 1 -nogui 1
# EXCTRCENA(16)|PCSAMPLENA(12)|SYNCTAP(10)|CYCCNTENA(0); tune POSTPRESET[4:1] if PC samples flood — then hand the probe to the viewer:
timeout 20s JLinkSWOViewerCLExe -device $JLINK_DEVICE -usb $JB_UID -swofreq 4000000 -itmmask 0xFFFFFFFF | head -40
```
Gate: ANY decoded output (stimulus, PC samples, exception packets) = SWO wired on `$JB` → land unmarked with the observed invocation. No output → try one more J-Link board, then land tagged. Either way `release $JB` after reflashing nothing (this experiment flashes nothing).

- [x] **Step 2: Insert the section (wording per gate outcome)**

Append to the `## PC-sampling` section:

```markdown
### SWO/ITM — hardware-timed trace on one pin (J-Link)

If the board routes SWO (TRACESWO), DWT emits packets with ZERO code change:
**exception trace** (`DWT_CTRL` bit16 EXCTRCENA) — every IRQ enter/exit,
timestamped, the ISR-ordering evidence the ring buffer needs code for — and
**hardware PC sampling** (bit12 PCSAMPLENA), better histograms than DWT_PCSR
polling. Arm the bits, then give the probe to the viewer (one client rule):

```bash
printf 'w4 E0001000, 0x00011401\nqc\n' | JLinkExe -device $JLINK_DEVICE -SelectEmuBySN <uid> ...
timeout 20s JLinkSWOViewerCLExe -device $JLINK_DEVICE -usb <uid> -swofreq 4000000 -itmmask 0xFFFFFFFF
```

SWO needs the pin physically wired to the probe — many rig boards route only
SWDIO/SWCLK. If the viewer shows nothing, that is the wiring, not the recipe.
```

If the gate FAILED on both boards, append ` (untested — SWO wiring unconfirmed on this rig)` to the subsection heading and keep the text.

- [x] **Step 3: Commit**

```bash
pre-commit run --files .claude/skills/target-debug/SKILL.md
git add .claude/skills/target-debug/SKILL.md
git commit -m "docs(target-debug): SWO exception-trace / hw PC-sampling recipe

Gate result on <JB>: <captured packet types | no SWO output — tagged untested>."
```

---

### Task 6: Flash verification, FreeRTOS thread awareness, semihosting & monitor-mode notes

**Files:**
- Modify: `.claude/skills/target-debug/SKILL.md` — `## Warnings` section + GDB section tail

**Interfaces:**
- Consumes: `$JB`, `$JB_UID`, `$JLINK_DEVICE` from Task 2.
- Produces: warning-list entries cited in Task 7's retrieval test scenarios.

- [x] **Step 1: Add flash-content verification to Warnings**

In `## Warnings`, after the "A marginal link can fake a deterministic firmware bug" bullet, add:

```markdown
- **"Flash OK" can lie** (silent no-op: old firmware keeps running after a
  green flash). When behavior contradicts the code you think is flashed,
  verify flash against the build:
  `arm-none-eabi-objcopy -O binary fw.elf /tmp/fw.bin`, then J-Link
  `verifybin /tmp/fw.bin,<flash-base>` (Commander) or OpenOCD
  `verify_image /tmp/fw.bin <flash-base>` — a mismatch means reflash with
  verification before debugging another minute.
```

- [x] **Step 2: Add FreeRTOS + semihosting + monitor-mode notes to the GDB section**

Append to the end of the `## GDB — state autopsy and watchpoints` section (after the Task-2 additions):

```markdown
FreeRTOS examples (`*_freertos`): add `-rtos GDBServer/RTOSPlugin_FreeRTOS`
to JLinkGDBServer (OpenOCD: `-rtos FreeRTOS` on the target) and `info
threads` / `thread <n>` shows every task's stack — a USB task blocked on a
queue vs. spinning is one `bt` away. Semihosting is never the answer here:
each call traps and halts the core — RTT does the same job without stopping.
**Monitor-mode debugging** (J-Link, M3+) can keep the USB ISR serviced while
you sit at a breakpoint — needs SEGGER's `JLINK_MONITOR.c`/ISR files compiled
in + `SetMonModeDebug=1`; not set up in this repo, reach for it when a bug
truly needs live breakpoints without killing the bus:
<https://kb.segger.com/Monitor_Mode_Debugging> (untested).
```

- [x] **Step 3: Hardware-verify verifybin + FreeRTOS awareness on $JB**

```bash
python3 test/hil/board_lock.py hold $JB --reason "skill-enhance verify: verifybin+rtos"
# (a) verifybin positive path against whatever is flashed — first reflash a known build:
#     flash examples/cmake-build-$JB/device/cdc_msc, then:
arm-none-eabi-objcopy -O binary examples/cmake-build-$JB/device/cdc_msc/cdc_msc.elf /tmp/fw.bin
printf 'verifybin /tmp/fw.bin,<flash-base from board .ld>\nqc\n' | \
  JLinkExe -device $JLINK_DEVICE -SelectEmuBySN $JB_UID -if swd -speed 4000 -autoconnect 1 -nogui 1
# (b) rtos plugin: flash cdc_msc_freertos for $JB (build if missing), start
JLinkGDBServer -device $JLINK_DEVICE -select usb=$JB_UID -if swd -speed 4000 -port 2331 -nogui -rtos GDBServer/RTOSPlugin_FreeRTOS &
timeout 60 arm-none-eabi-gdb -batch -ex 'target remote :2331' -ex 'monitor halt' -ex 'info threads' \
  <path to cdc_msc_freertos.elf>
python3 test/hil/board_lock.py release $JB   # after pristine reflash
```
Expected: (a) `Verify successful.` (b) `info threads` lists FreeRTOS tasks (`usbd`, `IDLE`, ...). If the plugin errors ("Could not load RTOS plugin"), drop the JLinkGDBServer variant from the Step-2 text and keep only the OpenOCD `-rtos FreeRTOS` form tagged `(untested)`.

- [x] **Step 4: Commit**

```bash
pre-commit run --files .claude/skills/target-debug/SKILL.md
git add .claude/skills/target-debug/SKILL.md
git commit -m "docs(target-debug): flash verifybin, FreeRTOS thread awareness, monitor-mode pointer

Verified on <JB>: verifybin 'Verify successful.'; info threads listed <n> tasks."
```

---

### Task 7: Intrusiveness table integration, agent update, retrieval test

**Files:**
- Modify: `.claude/skills/target-debug/SKILL.md` — the technique/intrusiveness table
- Modify: `.claude/agents/target-debugger.md` — primary-playbook bullet

**Interfaces:**
- Consumes: verified/untested status of every technique from Tasks 2-6.

- [x] **Step 1: Extend the intrusiveness table**

The table under `## Pick the least intrusive technique that can answer the question` currently has 5 rows (PC-sampling → GDB halt). Replace it with (keep the header row and any wording the earlier tasks did not contradict):

```markdown
| Technique | Intrusiveness | Reach for it when |
|---|---|---|
| PC-sampling | none — no halt, no code change | core wedged/spinning somewhere unknown (rusb2 FRDY) |
| SWO exception trace / hw PC-sample | none — needs SWO pin wired | ISR ordering/timing with zero code change |
| Vector catch | none until a fault fires | crash-shaped wedges — autopsy AT the faulting pc |
| RAM ring-buffer | ~tens of cycles per event | ISR ordering/timing bugs (musb babble) |
| TU_LOG (RTT) | µs per line | logic bugs that survive logging (J-Link or OpenOCD rtt) |
| TU_LOG (UART) | ms per line — blocking write | same, when no debug-probe RTT path |
| dprintf / conditional breakpoint | halt+resume per hit (~ms) | low-rate probes post-wedge; never ISR-rate events |
| GDB halt / breakpoints | stops USB service entirely | post-mortem state autopsy once wedged |
```

If Task 5's gate failed, keep the SWO row but append ` (untested)` in its "Reach for it" cell.

- [x] **Step 2: Update the agent's playbook bullet**

In `.claude/agents/target-debugger.md`, replace:

```markdown
- `.claude/skills/target-debug/SKILL.md` — your primary playbook: technique
  choice by intrusiveness, channel choice by link topology, capture recipes,
  GDB autopsy, all rig warnings.
```

with:

```markdown
- `.claude/skills/target-debug/SKILL.md` — your primary playbook: technique
  choice by intrusiveness, channel choice by link topology, capture recipes,
  breakpoint/watchpoint budget and cost model, vector catch + fault autopsy,
  GDB autopsy, all rig warnings.
```

- [x] **Step 3: Word-count and stale-reference check**

Run: `wc -w .claude/skills/target-debug/SKILL.md` — expected ≤ 2 700. If over, trim prose (not recipes) until under.
Run: `grep -n 'J-Link only' .claude/skills/target-debug/SKILL.md` — expected: no output (Task 3 removed the claim).

- [x] **Step 4: Retrieval test (skill-TDD GREEN gate)**

Dispatch a fresh read-only subagent (Explore) that reads ONLY the updated `.claude/skills/target-debug/SKILL.md` and answers:

1. "A CH32 board's firmware wedges; you suspect a HardFault loop. Least-intrusive next step?" — expected: vector catch (with the RISC-V caveat noted: CH32 is RISC-V → breakpoint on trap handler).
2. "You need RTT logs on an ST-Link-only board." — expected: OpenOCD `rtt setup/start/server`, NOT "impossible/J-Link only".
3. "Who is writing 0 into a busy flag, under live traffic?" — expected: OpenOCD value-match watchpoint `wp <addr> 4 w 0`, NOT a GDB conditional watch (halt-per-hit cost).
4. "Flash reported OK but behavior matches last week's build." — expected: verifybin/verify_image.
5. "You set `watch xfer_status[2][1]` and GDB answered `Watchpoint 2:` (no 'Hardware'). Proceed?" — expected: NO — software-watchpoint fallback single-steps the program; delete and narrow the expression.

All five must route correctly; a miss = fix the text (usually the table row or a heading), re-test.

- [x] **Step 5: Final commit**

```bash
pre-commit run --files .claude/skills/target-debug/SKILL.md .claude/agents/target-debugger.md
git add .claude/skills/target-debug/SKILL.md .claude/agents/target-debugger.md
git commit -m "docs(target-debug): integrate new techniques into intrusiveness table; agent playbook bullet

Retrieval test: 4/4 scenarios routed correctly."
```

---

## Deferred / out of scope (deliberate)

- **ETM / J-Trace instruction trace** — no J-Trace hardware on the rig; UM08001 "Trace" chapter is linked for the day one arrives.
- **Monitor-mode debugging as a working recipe** — needs SEGGER monitor files compiled into firmware (a firmware feature, not a doc change); landed as a pointer + `(untested)` in Task 6.
- **ITM stimulus-port logging backend for TU_LOG** — would be a `lib/` + `LOGGER=itm` firmware feature; out of scope for a skill-doc plan.
- **GDB tracepoints (`trace`/`tfind`)** — need a tracing-capable stub; neither JLinkGDBServer nor OpenOCD implements them for Cortex-M.
