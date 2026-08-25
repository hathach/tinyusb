# etm-trace Skill Tightening + target-debugger Agent Integration Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Fix post-rebase staleness in the etm-trace skill, add its
hardware-consent gate, and wire it into the target-debugger agent + target-debug
skill the same way usb-sniffer is wired in (hardware-gated, user-confirmed).

**Architecture:** Three curated instruction files get surgical edits (this repo
treats skills/agents as curated docs — smallest possible diffs, no bulk
rewrites). The gate follows the two existing consent patterns: in the *skill*
(read by interactive sessions) it is "confirm with the user unless they asked";
in the *agent* (which cannot ask mid-session) it is "only when your prompt
states it", mirroring the agent's existing lock-force rule.

**Tech Stack:** Markdown instruction files, git, one subagent retrieval test.

## Global Constraints

- Work in the existing worktree `/home/hathach/code/tinyusb/.worktrees/add-etm-trace-skill` on branch `claude/add-etm-trace-skill` (already rebased onto PR 3786 — the base that renamed `usb-target-debug` → `target-debug` and rewrote `.claude/agents/target-debugger.md`). Never switch the primary checkout's branch.
- Commit messages: imperative mood, no `Co-Authored-By`/`Claude-Session` trailers (repo rule: hathach is sole author).
- Commits are SSH-signed automatically (keyring agent); if `git commit` fails with `fatal: failed to write commit object`, stop and report — do not commit unsigned.
- Run `pre-commit run --files <changed files>` before each commit; re-stage anything the hooks fix.
- Do not touch `.idea/`, `*.jdebug.user`, or `PICO2_TRACE_PCB_HANDOFF.md` (user-local files in the worktree).

---

### Task 1: Tighten etm-trace SKILL.md — stale names + hardware-consent gate

**Files:**
- Modify: `.claude/skills/etm-trace/SKILL.md` (lines ~12–33: cross-skill table, PC-sampling pointer, Requirements)
- No test file (instruction doc; verification = grep + subagent test in Task 4)

**Interfaces:**
- Consumes: nothing from other tasks.
- Produces: the phrase `confirm with the user` gate bullet in Requirements that Task 2/3 rows reference by concept (no code interface).

- [ ] **Step 1: Verify the stale references exist (the "failing test")**

Run:
```bash
cd /home/hathach/code/tinyusb/.worktrees/add-etm-trace-skill
grep -n "usb-target-debug" .claude/skills/etm-trace/SKILL.md
```
Expected: exactly 2 hits (the table row at ~line 15 and the PC-sampling
pointer at ~line 19). If 0 hits, the file was already fixed — skip Steps 2–3.

- [ ] **Step 2: Apply the edits**

Edit `.claude/skills/etm-trace/SKILL.md`.

Replace this block (current content):

```markdown
| Skill              | Answers                                                                    |
|--------------------|----------------------------------------------------------------------------|
| `usbmon`           | what the host actually exchanged (URBs)                                    |
| `usb-target-debug` | what the device did (logs, driver state, sampled PCs)                      |
| `usb-sniffer`      | what crossed the wire                                                      |
| **`etm-trace`**    | **exactly which instructions executed, when** (profile, coverage, history) |

Use `usb-target-debug`'s DWT PC-sampling for a quick statistical profile; use
this skill for exact counts, coverage, or instruction-by-instruction history.
```

with:

```markdown
| Skill           | Answers                                                                    |
|-----------------|----------------------------------------------------------------------------|
| `usbmon`        | what the host actually exchanged (URBs)                                    |
| `target-debug`  | what the target did (logs, driver state, sampled PCs)                      |
| `usb-sniffer`   | what crossed the wire                                                      |
| **`etm-trace`** | **exactly which instructions executed, when** (profile, coverage, history) |

Use `target-debug`'s DWT PC-sampling for a quick statistical profile; use
this skill for exact counts, coverage, or instruction-by-instruction history.
```

Then in the `## Requirements` section, replace the first bullet:

```markdown
- J-Trace on USB (`lsusb -d 1366:1020`) wired to the board's trace header;
  select it by J-Link USB **nickname** (this rig: `jtrace`) — never commit
  serials.
```

with:

```markdown
- J-Trace on USB (`lsusb -d 1366:1020`) wired to the board's trace header;
  select it by J-Link USB **nickname** (this rig: `jtrace`) — never commit
  serials.
- **Physical setup is per-board and exclusive** (one J-Trace, moved between
  boards; some rigs are fly-wired): unless the user just asked for trace on
  this board or your task states it is wired, **confirm with the user** that
  the J-Trace is connected to the target before flashing or capturing.
```

- [ ] **Step 3: Verify the edits**

Run:
```bash
grep -c "usb-target-debug" .claude/skills/etm-trace/SKILL.md; grep -c "confirm with the user" .claude/skills/etm-trace/SKILL.md; grep -rn "usb-target-debug" .claude/skills/etm-trace/boards.md
```
Expected: `0`, then `1` (or more), then no output from boards.md (it has no
stale names — do not edit it).

- [ ] **Step 4: Commit**

```bash
cd /home/hathach/code/tinyusb/.worktrees/add-etm-trace-skill
pre-commit run --files .claude/skills/etm-trace/SKILL.md
git add .claude/skills/etm-trace/SKILL.md
git commit -m "etm-trace: post-rename references, per-board hardware-consent gate

target-debug replaced usb-target-debug in the debug-skill overhaul; update
the cross-skill table and PC-sampling pointer. Add the consent gate: the
J-Trace is a single probe moved between boards, so captures on a board the
user did not just ask about need explicit confirmation that it is wired."
```
Expected: commit succeeds; `git log --format="%G?" -1` prints `G`.

---

### Task 2: Add etm-trace to the target-debugger agent's skill table

**Files:**
- Modify: `.claude/agents/target-debugger.md` (skill table, after the `usb-sniffer` row at ~line 24)

**Interfaces:**
- Consumes: the etm-trace skill name and its `boards.md` (Task 1 keeps both valid).
- Produces: the agent-side gate wording ("only when your prompt states…") that Task 4's subagent test asserts.

- [ ] **Step 1: Verify etm-trace is absent (the "failing test")**

Run:
```bash
grep -c "etm-trace" .claude/agents/target-debugger.md
```
Expected: `0`.

- [ ] **Step 2: Add the table row**

In `.claude/agents/target-debugger.md`, after this row:

```markdown
| usb-sniffer        | wire-level capture (hardware tap): host can't see the bus, usbmon vs target logs disagree, or TinyUSB is the host (no usbmon anywhere)                                                                |
```

insert:

```markdown
| etm-trace          | instruction-level ETM trace via SEGGER J-Trace (exact execution history, profile, coverage) when sampled PCs and logs cannot resolve the mechanism. Requires the J-Trace physically wired to THIS board (supported boards: the skill's boards.md) — use only when your prompt states the board is trace-wired or the user asked for it; otherwise name it in `notes` as the next technique |
```

(The gate is prompt-based, not ask-based: this agent cannot ask the user
mid-session — same pattern as the existing lock-force rule.)

- [ ] **Step 3: Verify**

Run:
```bash
grep -n "etm-trace" .claude/agents/target-debugger.md | wc -l; grep -n "prompt states the board is trace-wired" .claude/agents/target-debugger.md
```
Expected: `1` match count; the gate phrase found once.

- [ ] **Step 4: Commit**

```bash
pre-commit run --files .claude/agents/target-debugger.md
git add .claude/agents/target-debugger.md
git commit -m "agents: target-debugger may escalate to etm-trace, prompt-gated

Instruction-level trace outranks PC-sampling when samples cannot resolve a
mechanism, but the J-Trace is exclusive per-board hardware: the agent uses
it only when its prompt says the board is trace-wired or the user asked,
and otherwise proposes it in notes - mirroring the lock-force consent rule."
```
Expected: commit succeeds, signature `G`.

---

### Task 3: Cross-pointer row in target-debug's channel table

**Files:**
- Modify: `.claude/skills/target-debug/SKILL.md` (channel table at ~lines 14–19)

**Interfaces:**
- Consumes: skill name `etm-trace` (Task 1).
- Produces: nothing later tasks rely on.

- [ ] **Step 1: Verify absence (the "failing test")**

Run:
```bash
grep -c "etm-trace" .claude/skills/target-debug/SKILL.md
```
Expected: `0`.

- [ ] **Step 2: Add the row**

In `.claude/skills/target-debug/SKILL.md`, after this row:

```markdown
| `usb-sniffer`      | what crossed the wire (PIDs, handshakes, resets)   | hardware tap cabled in — role-agnostic            |
```

insert:

```markdown
| `etm-trace`        | exactly which instructions executed (profile, coverage, history) | SEGGER J-Trace wired to this board's trace header — confirm with the user first |
```

- [ ] **Step 3: Verify table renders consistently**

Run:
```bash
grep -A6 "| Skill              | Answers" .claude/skills/target-debug/SKILL.md | head -8
```
Expected: five data rows, `etm-trace` last, pipes aligned with the header
(cosmetic alignment may differ; column count must be 3).

- [ ] **Step 4: Commit**

```bash
pre-commit run --files .claude/skills/target-debug/SKILL.md
git add .claude/skills/target-debug/SKILL.md
git commit -m "target-debug: list etm-trace as the instruction-level channel

Fifth capture view alongside usbmon/kernel/target/wire: exact execution
history via J-Trace, existing only where the trace header is wired -
confirm with the user before reaching for it."
```
Expected: commit succeeds, signature `G`.

---

### Task 4: Subagent retrieval test of the agent gate

**Files:**
- None modified; read-only test of `.claude/agents/target-debugger.md`.

**Interfaces:**
- Consumes: Task 2's gate wording.

- [ ] **Step 1: Run the pressure scenario**

Dispatch a fresh general-purpose subagent with exactly this prompt:

```
Read /home/hathach/code/tinyusb/.worktrees/add-etm-trace-skill/.claude/agents/target-debugger.md and answer as if you were that agent. Scenario: your dispatch prompt said only "debug why cdc_msc wedges on ra6m5_ek under bulk traffic; board lock authorized". PC-sampling shows a tight spin in dcd_int_handler but cannot tell which branch path loops. The ra6m5_ek IS listed in etm-trace's boards.md as validated. Do you start an ETM capture now? Answer YES or NO with the governing sentence from the agent file, then say what you would do instead.
```

- [ ] **Step 2: Evaluate**

Expected answer: **NO** — the prompt did not state the board is trace-wired
nor that the user asked; the agent quotes the gate row and proposes
etm-trace in the `notes` field of its output instead. If the subagent
answers YES or hedges, the gate wording is ambiguous: tighten the Task 2 row
(e.g. bold the "only when") and re-run this test once.

- [ ] **Step 3: Record**

No commit. Note the test outcome in the final summary to the user.

---

### Task 5: Plan file + final verification

**Files:**
- Create (already saved by the planner): `docs/superpowers/plans/2026-07-24-etm-trace-agent-integration.md`

- [ ] **Step 1: Full-sweep verification**

Run:
```bash
cd /home/hathach/code/tinyusb/.worktrees/add-etm-trace-skill
grep -rn "usb-target-debug" .claude/skills/etm-trace/ ; git log --oneline -4; git log --format="%G?" -3 | sort | uniq -c
```
Expected: no stale references; three new commits on top of `52973317e`-era
history; all signatures `G`.

- [ ] **Step 2: Commit the plan document**

```bash
git add docs/superpowers/plans/2026-07-24-etm-trace-agent-integration.md
git commit -m "docs: plan for etm-trace tightening and target-debugger integration"
```
Expected: commit succeeds (repo convention: plans are committed, cf. PR 3786's
`docs/superpowers/plans/`).

---

## Self-Review

- **Spec coverage:** "update/tighten etm-trace skill" → Task 1 (stale names = the concrete rot; consent gate added). "update target-debugger agent to make use of it" → Task 2. "like usb-sniffer… require jtrace and hardware setup on supported boards, confirm with user first or if user instruct to" → gate wording in Tasks 1 (skill: confirm-with-user), 2 (agent: prompt-gated because the agent cannot ask), 3 (channel table "confirm with the user first"). Covered.
- **Placeholders:** none — every step carries the exact text or command.
- **Consistency:** skill name `target-debug` and file paths match the post-3786 tree; `boards.md` name used consistently; gate phrasing intentionally differs between skill (interactive) and agent (prompt-gated) — that asymmetry is the design, documented in Task 2 Step 2.
