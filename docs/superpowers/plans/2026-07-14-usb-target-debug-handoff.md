# Hand-off: `usb-target-debug` skill + `target-debugger` agent

**Status: agreed but NOT started.** Design discussion happened 2026-07-13 in session
`c31a4617-43b1-491d-9865-3e35f393996b` (post-merge of the agents/workflows harness,
PR #3762 / `ac595bc5c`). This document is the implementation brief for a fresh session.

**Agreed sequencing: skill first → dogfood on 1-2 real HIL failures → then the agent
as its own small PR.** Do not build both at once — the agent charter's hard parts are
exactly what dogfooding the skill answers.

## The gap being filled

When HIL fails today, *what failed* is covered (hil-validate workflow, hil-operator
agent) but the deep *why* loop — instrument the target, capture on both sides,
correlate — has no skill and no agent. Every hard case so far (musb babble, rusb2
FRDY wedge, ch32v307 Heisenbug) fell back to interactive main-session work.

Why no existing agent can do it:

- **hil-operator** (sonnet) is deliberately mechanical: lock → flash → `hil_test.py`
  → recover. It never edits source, so it cannot inject instrumentation.
- **port-dev** can edit source but its charter is scoped changes verified by a
  *build*; it has no hardware mandate.
- The host-side capture knowledge lives in skills (`usbmon`, `usb-debug`); the
  device-side half exists only as CLAUDE.md recipes plus session memory.

The skill completes the debugging trio:

| Skill | Answers | Status |
|---|---|---|
| `usbmon` | what the host actually exchanged (URBs) | on master |
| `usb-debug` | why the host acted (dmesg / dynamic debug) | ships in PR #3758 (untracked copy in tree) |
| `usb-target-debug` | what the device did | **this hand-off** |

## Part 1 — `usb-target-debug` skill (do this first)

Create `.claude/skills/usb-target-debug/SKILL.md`. Match the style of
`.claude/skills/usbmon/SKILL.md` and `usb-debug/SKILL.md`: frontmatter `name` +
`description` where the description states concretely *when* to reach for it
(HIL test fails and host-side capture can't explain it; device silently NAKs,
wedges, or misbehaves; need TU_LOG/device-state evidence from real hardware).

Playbook to codify — all techniques already proven on this rig:

1. **TU_LOG capture** — build with `LOG=2` (add `LOGGER=rtt` for RTT); UART capture
   from the board's debug serial; RTT via `JLinkGDBServer -RTTTelnetPort 19021` +
   `JLinkRTTClient` (non-interactive: `timeout 20s JLinkRTTClient > rtt.log`).
   Note which log level perturbs timing (see warning #6).
2. **GDB recipes per probe family** — J-Link, OpenOCD (ST-Link / CMSIS-DAP /
   WCH-Link). Base connect/load recipes already exist in CLAUDE.md "GDB Debugging";
   the skill adds the debug-loop specifics: breakpoints in ISR context, dumping
   endpoint/FIFO registers, watchpoints on driver state variables.
3. **RAM ring-buffer trace pattern** (used to crack the musb babble): instrument
   the dcd/hcd with a small RAM ring of event records instead of TU_LOG when
   printing perturbs timing; let the failure happen; halt and dump the ring via
   GDB. Include a minimal C snippet (fixed-size struct ring, no allocation,
   ISR-safe single-writer).
4. **J-Link PC-sampling** (nailed the rusb2 FRDY wedge): statistically sample PC
   without halting to find where the core spins — the non-intrusive option when
   halting or logging masks the bug.
5. **Dual-side capture**: usbmon on the host + RTT/ring-buffer on the target,
   simultaneously; correlate host URBs against device events on one timeline.
   This is the default posture for enumeration/transfer bugs, not an escalation.
6. **Warnings**: observation can mask the bug (the ch32v307 case changed behavior
   under logging/debug — prefer ring-buffer over TU_LOG, PC-sampling over halting,
   and say so explicitly); a J-Link core reset does NOT drop a DWC2 soft-connect
   pullup, so a wedged DUT stays wedged on the host side (cross-ref
   `usb-recover/SKILL.md`).
7. **Rig discipline**: hold the board lock for the whole manual session —
   `python3 test/hil/board_lock.py hold <board> --reason "target debug: <bug>"`
   … work … `release <board>`. Never stop the actions-runner. Board → probe
   mapping via `test/hil/tinyusb.json`; `JLINK_DEVICE`/`OPENOCD_OPTION` via
   `hw/bsp/*/boards/*/board.cmake` or `board.mk`.

**Where to ship**: its own small PR (usb-recover/usb-debug already belong to
PR #3758 — don't grow that one), or fold into #3758 if it is still open and being
rebased anyway. User's call at the time.

## Part 2 — `target-debugger` agent (later, after dogfooding)

Create `.claude/agents/target-debugger.md` as its own PR once the skill has been
through at least one real debug session.

Agreed charter outline:

- **Frontmatter**: `model: opus`; omit `tools:` (= all tools — it must edit source
  AND drive hardware). Note the registry supports no `effort` field — the agreed
  opus/**xhigh** tier is requested per `agent()` call by whichever workflow or
  session spawns it.
- **Loop**: instrument → build → flash under one held board lock → dual-side
  capture (host usbmon + target RTT/ring-buffer/GDB) → correlate → refine
  hypothesis → repeat. Deliberately serial: no fan-out win; the value is
  backgrounding a long debug session and the codified playbook.
- **Strictly one instance**, holds the board lock for the entire session — its work
  is exactly the "hardware work outside hil_test.py" case in the lock protocol.
- **Skills are its source of truth** (mirror hil-operator's pattern): read
  `usb-target-debug`, `usbmon`, `usb-debug`, `usb-recover`, `hil` SKILL.md files
  before acting.
- **Hard rule — instrumentation is temporary**: the instrumentation diff must be
  reverted (or explicitly listed in the hand-back report) at session end; the *fix*
  itself goes to port-dev. Keeps charters clean: this agent produces a diagnosis
  and evidence, not a merged patch.

Questions dogfooding must answer before the charter is written (do NOT guess these
now — that was the whole reason for skill-first):

1. When to stop instrumenting and report a partial diagnosis vs keep digging.
2. Maximum board-lock hold time / check-in cadence for a backgrounded session.
3. What "revert instrumentation" means when a partial fix emerged mid-debug
   (revert + attach diff? keep on a branch?).

## Conventions and references for the implementing session

- Skill style exemplars: `.claude/skills/usbmon/SKILL.md`, `usb-debug/SKILL.md`,
  `usb-recover/SKILL.md` (the latter two are #3758's copies, present untracked).
- Agent style exemplars: `.claude/agents/hil-operator.md` (lock discipline,
  skills-as-source-of-truth), `port-dev.md` (source-edit + verify charter).
- When the agent lands, update the harness spec's agent roster:
  `docs/superpowers/specs/2026-07-09-claude-agents-workflows-design.md`
  (convention: spec evolves in-repo; plans like this file are per-effort records).
- Agents register from `.claude/agents/*.md` at session start — a new agent file
  is only visible to sessions launched after it exists.
- Past cases to mine for the skill's examples: musb babble (ring-buffer trace),
  rusb2 FRDY wedge (J-Link PC-sampling), ch32v307 Heisenbug (observation
  sensitivity) — details in session memory and the referenced session transcript.
