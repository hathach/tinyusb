# `rtt` skill — design & decision record

Date: 2026-08-24. Branch: `rttconsole-skill`. Author sessions: lpc4088 handoff
(measurements), sysview handoff (mechanics + probe matrix), this session
(verification + decision). User approved promotion and the name `rtt` on
2026-08-24.

## Decision

Promote SEGGER RTT from an inline technique in `.claude/skills/target-debug/`
to a standalone skill `.claude/skills/rtt/`, scoped as **transport core +
console layer**: getting bytes on/off RTT channels over any debug probe, plus
the bidirectional console tooling the HIL harness ships. Consumer-specific
layers (SystemView encode/decode/licensing, TU_LOG conventions, debugging
methodology) stay in their skills and cross-reference.

## Scoring against the promotion criteria

Criteria: `docs/superpowers/specs/2026-07-09-claude-agents-workflows-design.md`
§"Skill vs technique — promotion criteria" (exists only on branch
`claude/add-systemview-debug`; read via `git show`). Two or more of four
required. Score: **3/4**.

1. **Ships tooling — yes.** `hil_util.JlinkRtt` (commit d98e77bac: probe
   selection by serial, dynamic port allocation, non-blocking bidirectional
   socket, process-group teardown) plus a thin CLI added by this plan.
   Precedent: `hil` and `code-size` are skills wrapping repo-versioned tools;
   "recipes over already-installed tools" is what RTT was *before* this code
   existed (why SWO stayed a technique at 1.5/4 — see `SWO_SKILL_HANDOFF.md`).
2. **Answers its own routed question — yes.** "Give this board a console /
   printf I/O with no UART and no VCOM" is asked from harness and bring-up
   contexts that never load target-debug (whose trigger is *misbehaving
   firmware*). Measured cost of the missing route: the lpc4088 session burned
   an hour rediscovering a gotcha already written at target-debug
   SKILL.md:249-253.
3. **Carries validation state — yes.** Measured tool matrix (below), 13-board
   OpenOCD read-path campaign from the sysview cycle, WCH SDI A/B proof,
   SAMD5x DSU gotcha, lock-porting example, per-probe constraints.
4. **Long but conditionally relevant — yes.** The transport knowledge is a
   page+ that most target-debug sessions don't need and harness sessions
   can't find there.

## Measured evidence the skill must carry

From the lpc4088 session (LPC4088 + LPC-Link2 J-Link fw 611000000, SWD 4 MHz;
single board — re-verify on more hardware during validation):

- `JLinkExe -RTTTelnetPort <port> -AutoConnect 1`: 6/6 reliable; delivers the
  buffered boot burst; accepted an 8550-byte write in one call. **The proven
  standalone path.**
- Drain rate 24.6 KiB/s (253,127 B / 10.0 s) against a saturating printf
  firmware that produced 689,896 lines — 0.6 % delivered. RTT console is
  **drain-limited and lossy under saturation; drops happen at the target**
  (NO_BLOCK_SKIP, 1 KB default buffer).
- `JLinkRTTLogger`: 0/6 — "RTT Control Block not found" even given
  `-RTTAddress`, block plainly readable over SWD. Searches once at attach,
  never retries. **Never build on it.**
- `JLinkGDBServer -RTTTelnetPort` with **no GDB client attached**: served the
  port, never located the control block (this board). target-debug's
  GDBServer+JLinkRTTClient recipe was proven in flows where GDB attaches, and
  CLAUDE.md's recipe worked on other parts — treat as per-part variance,
  document both; do not "correct" either into a flat contradiction.
- OpenOCD (jaylink) driving this J-Link-firmware probe: transport failure
  (`LIBUSB_ERROR_TIMEOUT`, `jaylink_swd_io() failed`), probe drops off USB,
  **physical replug needed** — twice, reproducible. Standing rule: never
  point OpenOCD at that class of probe (J-Link OB firmware on a debug-probe
  board like the LPC-Link2). Genuine SEGGER J-Links work under jaylink —
  routine in the sysview campaigns (metro_m4_express).

From the sysview cycle (branch `claude/add-systemview-debug`, 13-board
campaign 2026-08-12):

- OpenOCD `rtt setup <exact CB addr> … ; rtt start; rtt server start <port>
  <ch>` **read path validated** on ST-Link, CMSIS-DAP and J-Link probes
  (`test/hil/sysview_ci.py`). Exact CB address from
  `arm-none-eabi-nm <elf> | grep _SEGGER_RTT` beats a full-RAM scan (slower,
  can mis-hit stale RAM after soft reset).
- The real transport requirement is **autonomous memory access while the core
  runs**: ARM memory-AP (zero intrusion), RISC-V SBA where implemented.
  **WCH QingKe SDI has neither** — Debug Module abstract commands perturb the
  running core; A/B-proven kill ~1.9 s into USB traffic. Per-transport rule:
  SDI = halt→read→resume / post-mortem dump only, never live streaming.
- SAMD5x + OpenOCD: in-session `reset run` via the DSU CPU Reset Extension
  leaves the core held — attach without reset when the flash step already
  reset the board (general preference: attach-only capture).
- Lock porting example: `hw/bsp/ch583/sysview_rtt_lock_wch.h` (QingKe CSR
  0x800 brace-scoped save/restore; generic RISC-V lock traps mcause=2).
- Drain hierarchy: J-Link native > OpenOCD polling; matters only at
  SystemView bandwidths (workable buffers 2048–8192); console logs never
  overflow the drain in practice.
- RTT mechanics for the concepts section: control block `_SEGGER_RTT` (magic
  "SEGGER RTT") + ring buffers {sName, pBuffer, SizeOfBuffer, WrOff, RdOff,
  Flags}; the HOST must write RdOff back to drain; modes NO_BLOCK_SKIP (log
  default) / NO_BLOCK_TRIM / BLOCK_IF_FIFO_FULL (target spins — dangerous in
  ISRs); post-mortem mode = `SEGGER_RTT_WriteWithOverwriteNoLock` (target
  drags RdOff, ring holds last N bytes, no live host needed); channel 0 =
  "Terminal" console, SystemView claims its own "SysView" up-buffer —
  coexist on one control block.

## Gotchas the skill centralises

Control block exists only after the target's first printf (early reader sees
nothing; Logger gives up). The console owns the probe: flash and reset before
opening it; never reset while attached. An undrained NO_BLOCK_SKIP ring holds
the FIRST KB after boot, not the wedge tail. Always select probes by serial
(`-USB <sn>` / `adapter serial`) — rigs run several. Two probes wired to one
SWD header wedge the target.

## v1 backend matrix

| Backend                                               | Read (capture)               | Write (console input)                      |
| ----------------------------------------------------- | ---------------------------- | ------------------------------------------ |
| J-Link native (`JLinkExe -RTTTelnetPort`)             | validated                    | validated (8.5 KB writes)                  |
| OpenOCD on native probes (ST-Link/CMSIS-DAP/WCH-Link) | validated (sysview campaign) | unvalidated — validate in the ci-rig phase |
| OpenOCD on the LPC-Link2 (J-Link OB fw, measured)     | forbidden (USB drop)         | forbidden                                  |
| WCH SDI (any tool)                                    | halt→dump only               | n/a                                        |

`JlinkRtt`/CLI are J-Link-only in v1; OpenOCD console-write support is
added only if the ci-rig phase validates it.

## Tooling home

Single implementation in `tools/rtt.py`: a stdlib-only importable module
(shared socket-console base + `JlinkRtt` + `OpenocdRtt`) that doubles as
the CLI. `hil_util` imports and re-exports the classes (the harness keeps
addressing `hil_util.JlinkRtt`), so the dependency points harness → tools,
never tools → harness. Because `hil_util` loads it at import time, the file
is harness-critical: it is classified with `test/hil/` in `ci_select`'s full
rule and covered by the pre-commit `hil-test` hook (test_hil_rtt.py).
Precedent: `code-size` wrapping `tools/metrics_compare_base.py` — the skill
is md-only and points at the tool. `open_board_console()` stays in
`hil_test.py` for now; pool-check adoption is a follow-up doc, not this PR.

## Doc edits (curated-skills rule: smallest possible diffs)

- `target-debug/SKILL.md`: capture-channel rows and the drain-model warning
  stay; the two capture recipe blocks and the RTTLogger/GDBServer paragraph
  shrink to one-liners pointing at `rtt`; the manual ring-read recipe
  (`nm`/`mem32`/`savebin`) moves into `rtt` §post-mortem.
- `CLAUDE.md` GDB section RTT line becomes build flag + pointer.
- `hil/SKILL.md` gains one routing line (the fix that would have prevented
  the lost hour).
- `sysview/SKILL.md` pointer is **deferred** until that branch merges, and
  proposed to the user first. No edits to `sysview_ci.py` or the sysview
  skill now.

## Validation strategy (user-directed)

1. **Dogfood on the local htpc bench first**: ea4088_quickstart via LPC-Link2
   (replugged; OpenOCD attempts on it are skipped outright) and
   raspberry_pi_pico2 via the J-Trace (nickname `jtrace`, serial private; now wired to pico2; RP2350 =
   `rp2350_m33_0`, never a custom JLinkScript). Follow only the SKILL.md
   text (dogfood = REFACTOR input).
2. **Then all boards on the ci.lan rig**, per-transport smoke capture, rows
   recorded in `.claude/skills/rtt/boards.md`. Exclusions recorded honestly
   (esptool boards: no SEGGER-RTT path in our builds — USB-Serial-JTAG
   console instead; tm4c: no probe path configured on the rig).

## Non-goals

Timing/profiling (etm-trace, sysview, parked swo-trace), SystemView
encode/decode/licensing, TU_LOG conventions, debugging decision flows
(target-debug), Espressif USB-Serial-JTAG console (esp-target-debug), WCH SDI
live streaming (impossible — see matrix).
