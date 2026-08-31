# Audit of the `.claude/` instruction surface — design

**Date:** 2026-08-18
**Branch:** `claude/hil-doc-audit`

## Why

`hil-operator.md` told an operator two incompatible things at once: one rule forbade
pre-holding a board lock because `hil_test.py` self-locks, while a rule added in the same
revision made the lock the thing that keeps concurrent operators off each other's hardware —
so an operator following the second would take a hold that made its own run fail fast against
it. Both statements were fixed before this branch was folded, so neither survives in history;
what survives is the lesson that nothing checks these files against the code they describe.

That is not an isolated slip. A scan of the 36 repo paths cited across `.claude/` flags 8
that do not resolve. Five are legitimate — placeholders (`docs/changelog/X.Y.Z.md`,
`src/portable/x/dcd_x.c`, a `test_*.py` glob), a generated file
(`examples/cmake-build-pvs/compile_commands.json`), and a per-host gitignored config
(`test/hil/local.json`, whose absence the skill already handles). Three are drift:
`usbtest/SKILL.md:24,50` cites `src/usb_descriptors.h` and `src/tusb_config.h`, which are
example-relative but read as repo paths, and `:101` cites `tools/usb/testusb.c`, a Linux
kernel path presented like a repo file.

Cross-references are in better shape: every `agentType` in a workflow resolves to an agent
in `.claude/agents/`, every `.claude/skills/<name>` referenced by an agent or workflow
exists, and the workflow scripts call only harness functions that exist. The drift is in
**prose claims about behavior** — the class that made `hil-validate` parallelize at the
wrong layer, on top of a `hil_test.py` that already schedules boards across host
controllers under per-controller permits (`hil_lock.py:7,133-134`; `hil_test.py:2249,2419`).

## Scope

**In:** `.claude/agents/*.md` (7), `.claude/workflows/*.js` + `check.sh` (7),
`.claude/skills/*/SKILL.md` (16) and their 8 helper scripts, and the repo `CLAUDE.md`.
~4,700 lines (2,874 of prose, the rest helper scripts and `etm-trace/boards.md`).

**Out:** `docs/superpowers/**` (historical records — correcting them rewrites history
rather than fixing what a future session executes), `.claude/settings*.json` and hooks, the
memory index, and any behavior change to the scripts themselves.

## Claim taxonomy

Only falsifiable classes get a verdict. Guidance ("bias toward caution") is checked solely
for contradiction with the classes below.

| Class | Settled by | Example |
|---|---|---|
| Path | `ls`/`find`, with the base dir made explicit | `src/tusb_config.h` — example-relative, reads as repo-relative |
| Interface | argparse/grep in the named source | `-b` is `action='append'` (`hil_test.py:2249`) |
| Behavior | reading the implementing code, cited `file:line` | "permits are in-process semaphores" (`hil_lock.py:7`) |
| Number | the constant's definition | `FLASH_PARALLEL=4` (`hil_lock.py:133`) |
| Rig state | read-only `ssh ci.lan` probe | bus map, probe uids, sudoers entries, installed tools |
| Cross-doc | diffing the same rule's two statements | `hil-operator.md:18` vs `:37` |

### Verdicts

- **CONFIRMED** — current source says so. Cite `file:line`. Leave alone.
- **REFUTED** — current source says otherwise. Cite, correct the doc.
- **EARNED** — no source in scope settles it, and it is hard-earned rig knowledge. Stays in
  the docs untouched; see the rule below.
- **UNVERIFIABLE** — no source in scope settles it and it is not earned knowledge either
  (a placeholder, a generated file, a claim about something outside the repo).

### Hard-earned evidence is source of truth

A claim with no code backing is **not** a cut candidate when it is earned rig knowledge:
an observed hardware quirk, a failure mode paid for in rig downtime, a workaround whose
rationale lives only in the incident that produced it. Code is authoritative about code;
experience is authoritative about hardware, and the hardware does not document itself.

Consequences:

- Only a claim the **current source actively refutes** gets corrected. "I could not find
  backing" is never grounds for deletion.
- Rig-state claims that have gone stale (a bus map, a probe uid) are **re-derived and
  updated**, or converted into a derivation recipe ("buses renumber every boot — re-derive
  with X"), never dropped.
- Where earned knowledge and current code disagree, that is a **finding to report**, not an
  edit to make: one of them is a bug, and deciding which is out of this audit's scope.

## Passes

1. **Extraction (fan-out, 9 agents, no verdicts).** One agent per cluster, each writing a
   ledger to the scratchpad and returning only a count and the ledger path. Per claim:
   `file:line`, verbatim claim, class, what source would settle it, and a flag for
   suspected hard-earned evidence. Agents return no judgments, so nothing arrives as a
   verdict that would have to be unwound.
2. **Verification (mine).** Every claim checked against source myself: scripted checks for
   paths/interfaces/numbers, code reading for behavior, read-only `ssh ci.lan` for rig
   state (`ls`, `--help`, `which`, `lspci`, `lsusb`, `hil_lock.py status`, `sudo -l`,
   `uname -r` — no locks, no flashing, no `uhubctl`, no recovery). Nothing acted on is
   taken on an extractor's word.
3. **Cross-doc consistency (mine).** Build a rule inventory — board locks, timeouts,
   output contracts, retry policy, config selection, forcing — and diff every place each
   rule is stated. No per-file agent can do this pass; it is where the `hil-operator`
   failure lived.
4. **Edits.** Delete only what is refuted by source, restates the command it precedes, or
   duplicates a rule that has a canonical home elsewhere (keep one, reference it). Keep
   every claim source confirms that changes behavior, every hard-earned observation, and
   the "why" behind non-obvious rules. Structure stays as is.
5. **Gate.** Re-run the path and interface scans; `check.sh` on every workflow; `bash -n`
   and `py_compile` on all 8 helper scripts; the four `test/hil` suites;
   `pre-commit run --all-files`.

## Extraction clusters

| # | Cluster | Lines |
|---|---|---|
| 1 | `.claude/agents/*.md` (7 files) | 313 |
| 2 | `.claude/workflows/*.js` + `check.sh` | 659 |
| 3 | `hil`, `hil-pool-check` | 223 |
| 4 | `usb-kernel-recover`, `usb-kernel-debug` + 2 scripts | 253 + scripts |
| 5 | `target-debug`, `esp-target-debug` | 496 |
| 6 | `usbtest`, `usbmon`, `usb-sniffer` + `usbcap.sh` | 382 + script |
| 7 | `etm-trace` + `boards.md` + 2 scripts | 203 + files |
| 8 | `build-doc`, `code-size`, `pvs`, `make-release`, `read-doc`, `pre-pr` + 2 scripts | 345 + scripts |
| 9 | `CLAUDE.md` | 139 |

## Deliverables

Commits split by surface (agents / workflows / skills / CLAUDE.md) so review stays
tractable, on `claude/claude-doc-audit`. A findings report covering every REFUTED claim
with its citation, and every earned-knowledge-vs-code disagreement found in pass 2.

A refuted claim whose *code* is the wrong half does not get a silent code edit: it becomes
a handoff doc under `docs/superpowers/followup/`, per the repo's deferred-work rule.

## Success criteria

- Every falsifiable claim in scope carries a verdict with a citation.
- No claim that current source refutes survives in the tree.
- No hard-earned observation is deleted; stale rig state is re-derived or turned into a
  derivation recipe.
- No rule is stated in two places with two different meanings.
- The gate in pass 5 passes.
