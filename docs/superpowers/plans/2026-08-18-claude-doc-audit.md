# `.claude/` Instruction-Surface Audit Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Give every falsifiable claim in the 4,689-line `.claude/` + `CLAUDE.md` instruction surface a verdict backed by a citation, correct the ones current source refutes, and remove duplication without deleting hard-earned rig knowledge.

**Architecture:** Claims are extracted by parallel subagents into machine-checkable JSONL ledgers, then verified by the main session — never by the extractor that found them. Two validators make "trust nothing without source" mechanical rather than aspirational: one asserts every extracted claim's verbatim text really appears where the ledger says it does, the other asserts every verdict's citation really contains the code it cites. Edits happen only after verification, committed one surface at a time.

**Tech Stack:** Python 3 (validators, stdlib only), bash (mechanical scans), `ssh ci.lan` read-only probes, the repo's existing gates (`.claude/workflows/check.sh`, `test/hil/test/test_*.py`, `pre-commit`).

**Spec:** `docs/superpowers/specs/2026-08-18-claude-doc-audit-design.md`

## Status (2026-08-18, end of session)

| Task | State |
|---|---|
| 1 validator | DONE — 6 self-tests, incl. rejecting a hallucinated quote |
| 2 extraction | DONE — 1,387 claims, 0 validation errors |
| 3 mechanical sweep | DONE — 647 verdicts, acceptance test green |
| 4 rig probe | PARTIAL — transcript captured and acted on (5 Renesas, NOPASSWD, ppps advertised-only); the 201 rig claims were never individually verdicted |
| 4+5+6 verdict coverage | **1,387 of 1,387 claims now carry a verdict row** (233 CONFIRMED, 340 EARNED, 39 REFUTED, 775 UNVERIFIABLE-with-corroboration), 0 citation errors. The behavior sweep deliberately never emits CONFIRMED: finding a claim's token in the named file proves the vocabulary is there, not that the claim holds. |
| 5 behavior | PARTIAL, largely UNRECORDED — verified by hand: all 10 scripts' flags vs argparse, 8 kernel citations vs v6.12.96, the usbtest case→DCD map vs the kernel, 8 agent/workflow contracts, CLAUDE.md commands/paths/boards. No verdict rows were written for any of it. `etm`/`target`/`kernel` standalone claims are settled by owner decision (earned evidence). |
| 6 cross-doc | DONE — token index over all claims, 185 tokens spanning 2+ files, inventory in `$AUDIT/rules.md`. Four contradictions found and fixed. |
| 7 edits | DONE for every finding to date (6 commits) |
| 8 report | Delivered in chat; evidence lives in the commit messages. No handoffs — no code-side bugs found. |
| 9 gate | DONE — check.sh ×6, bash -n/py_compile ×8, 4 HIL suites, pre-commit --all-files, refuted-strings check |
| 10 recurrence guard | BUILT, MEASURED, REJECTED — the path lint flags 11 paths on the audited tree and **all 11 are false positives**: generated dirs (`docs/_build`, `docs/examples/`), and slash-in-prose (`interrupt src/sink`, `include test/build evidence`). Fatally, the defect it was meant to catch (`Key files: src/tusb_config.h`) is lexically identical to correct text (`the example's own src/usb_descriptors.h`) — the difference is context. Any threshold quiet enough to ship also misses the bug. Not committed; do not rebuild it. |

**If resuming:** the ledgers are in the session scratchpad (`$AUDIT/ledgers/*.jsonl`, 1,387 claims,
quote-validated) and are the expensive artifact — copy them somewhere durable first. The remaining
work with real yield is Task 5 verdict rows for `agents`/`workflows`/`hil`/`tools`/`claudemd`/`usb`;
the four contradictions all came from Task 6, which is now complete.

---

## Global Constraints

- **Worktree:** `/home/hathach/code/tinyusb/.claude/worktrees/claude+hil-concurrent`, branch `claude/hil-doc-audit`. Bash cwd resets between calls — `cd` into the worktree inside **every** compound command.
- **Scratchpad:** `AUDIT=/tmp/claude-1000/-home-hathach-code-tinyusb--claude-worktrees-claude-hil-concurrent/fa699ee5-4141-4bcf-b1f3-df8a0b5e36cd/scratchpad/audit`. Tasks 1–6 write here only; nothing in the scratchpad is committed.
- **Hard-earned evidence is source of truth.** Only a claim the current source *actively refutes* gets corrected. "No backing found" is never grounds for deletion. Stale rig state is re-derived or converted to a derivation recipe, never dropped.
- **Rig contact is read-only.** `ls`, `--help`, `which`, `lspci`, `lsusb`, `hil_lock.py status`, `sudo -l`, `uname -r`. No board locks, no flashing, no `uhubctl`, no `usb_recover.sh`, never stop the actions-runner.
- **Code is never silently edited.** A refuted claim whose *code* is the wrong half becomes a handoff doc under `docs/superpowers/followup/`.
- **Scope:** `.claude/agents/*.md`, `.claude/workflows/*` , `.claude/skills/*/SKILL.md` + 8 helper scripts, `CLAUDE.md`. Out: `docs/superpowers/**`, settings/hooks, memory index.
- **No pushes** until the user explicitly says so.

---

### Task 1: Ledger schema and the anti-hallucination validator

The validator is what makes extraction trustworthy: an extractor that invents a claim, or cites the wrong line, fails the check. Build it before any extractor runs.

**Files:**
- Create: `$AUDIT/validate_ledger.py`
- Create: `$AUDIT/fixtures/good.jsonl`, `$AUDIT/fixtures/bad.jsonl`
- Test: `$AUDIT/test_validate_ledger.sh`

**Interfaces:**
- Consumes: nothing.
- Produces: the ledger record shape every extractor in Task 2 must emit —
  `{"id": str, "file": str (repo-relative), "line": int (1-based), "class": "path"|"interface"|"behavior"|"number"|"rig"|"crossdoc", "claim": str (verbatim from the file), "settle_with": [str], "earned": bool}`
  and `validate_ledger.py <repo-root> <dir> [--field claim|citation]` exiting non-zero on
  any violation. `--field citation` validates verdict files instead of ledgers, requiring
  `{id, verdict, citation:{file,line,quote}}` and quote-checking `citation.quote` at
  `citation.file:citation.line` -- the same anti-hallucination gate, applied to Task 5's work.

- [ ] **Step 1: Write the failing test**

```bash
# $AUDIT/test_validate_ledger.sh
set -u
W=/home/hathach/code/tinyusb/.claude/worktrees/claude+hil-concurrent
D=$(dirname "$0")
fail=0

# a real claim, quoted verbatim from a line that exists
python3 "$D/validate_ledger.py" "$W" "$D/fixtures/good" \
  && echo "PASS: clean ledger accepted" || { echo "FAIL: clean ledger rejected"; fail=1; }

# a hallucinated quote, a bad class, a duplicate id, an out-of-range line
python3 "$D/validate_ledger.py" "$W" "$D/fixtures/bad" >/tmp/bad.out 2>&1 \
  && { echo "FAIL: bad ledger accepted"; fail=1; } || echo "PASS: bad ledger rejected"
for want in "claim not found" "bad class" "duplicate id" "out of range"; do
  grep -q "$want" /tmp/bad.out || { echo "FAIL: no '$want' diagnostic"; fail=1; }
done
exit $fail
```

Fixtures — `fixtures/good/a.jsonl` (the quote is verbatim from `hil-operator.md`, whose line 5 is `model: sonnet`):

```json
{"id":"G-001","file":".claude/agents/hil-operator.md","line":5,"class":"interface","claim":"model: sonnet","settle_with":["the harness agent frontmatter contract"],"earned":false}
```

`fixtures/bad/a.jsonl`:

```json
{"id":"B-001","file":".claude/agents/hil-operator.md","line":5,"class":"interface","claim":"model: opus-with-extra-reasoning","settle_with":["x"],"earned":false}
{"id":"B-002","file":".claude/agents/hil-operator.md","line":5,"class":"vibes","claim":"model: sonnet","settle_with":["x"],"earned":false}
{"id":"B-002","file":".claude/agents/hil-operator.md","line":5,"class":"path","claim":"model: sonnet","settle_with":["x"],"earned":false}
{"id":"B-003","file":".claude/agents/hil-operator.md","line":99999,"class":"path","claim":"model: sonnet","settle_with":["x"],"earned":false}
```

- [ ] **Step 2: Run it to verify it fails**

Run: `bash $AUDIT/test_validate_ledger.sh`
Expected: FAIL — `python3: can't open file .../validate_ledger.py`

- [ ] **Step 3: Write the validator**

```python
#!/usr/bin/env python3
"""Validate claim ledgers: schema, plus the quote really appearing where it says.

The quote check is the point. An extractor that paraphrases, hallucinates or
miscounts lines fails here, so nothing downstream rests on its word."""
import json
import sys
from pathlib import Path

CLASSES = {'path', 'interface', 'behavior', 'number', 'rig', 'crossdoc'}
REQUIRED = {'id', 'file', 'line', 'class', 'claim', 'settle_with', 'earned'}
WINDOW = 2          # the extractor may cite the line above or below a wrapped claim
NEEDLE = 40         # compare a prefix: long claims span lines, short ones are exact


def squash(s: str) -> str:
    return ' '.join(s.split())


def check_ledger(ledger: Path, root: Path, seen: set) -> tuple:
    errs, n_claims = [], 0
    for n, raw in enumerate(ledger.read_text().splitlines(), 1):
        if not raw.strip():
            continue
        where = f'{ledger.name}:{n}'
        try:
            c = json.loads(raw)
        except ValueError as e:
            errs.append(f'{where}: not JSON ({e})')
            continue
        missing = REQUIRED - set(c)
        if missing:
            errs.append(f'{where}: missing {sorted(missing)}')
            continue
        n_claims += 1
        if c['class'] not in CLASSES:
            errs.append(f'{where}: bad class {c["class"]!r}')
        if c['id'] in seen:
            errs.append(f'{where}: duplicate id {c["id"]}')
        seen.add(c['id'])
        src = root / c['file']
        if not src.is_file():
            errs.append(f'{where}: {c["file"]} does not exist')
            continue
        lines = src.read_text(errors='replace').splitlines()
        if not 1 <= c['line'] <= len(lines):
            errs.append(f'{where}: line {c["line"]} out of range for {c["file"]} '
                        f'({len(lines)} lines)')
            continue
        lo = max(0, c['line'] - 1 - WINDOW)
        window = squash('\n'.join(lines[lo:c['line'] + WINDOW]))
        needle = squash(c['claim'])[:NEEDLE]
        if needle and needle not in window:
            errs.append(f'{where}: claim not found near {c["file"]}:{c["line"]} '
                        f'-- {needle!r}')
    return errs, n_claims


def main() -> int:
    root, ledger_dir = Path(sys.argv[1]), Path(sys.argv[2])
    ledgers = sorted(ledger_dir.glob('*.jsonl'))
    if not ledgers:
        print(f'no ledgers in {ledger_dir}', file=sys.stderr)
        return 1
    errs, total, seen = [], 0, set()
    for l in ledgers:
        e, n = check_ledger(l, root, seen)
        errs += e
        total += n
    for e in errs:
        print(e, file=sys.stderr)
    print(f'{len(ledgers)} ledger(s), {total} claim(s), {len(errs)} error(s)')
    return 1 if errs else 0


if __name__ == '__main__':
    sys.exit(main())
```

- [ ] **Step 4: Run it to verify it passes**

Run: `bash $AUDIT/test_validate_ledger.sh`
Expected: four `PASS:` lines, exit 0.

- [ ] **Step 5: No commit** — scratchpad tooling. Record the validator path in the working notes and move on.

---

### Task 2: Extract claims (9 parallel subagents)

**Files:**
- Create: `$AUDIT/ledgers/{agents,workflows,hil,kernel,target,usb,etm,tools,claudemd}.jsonl`

**Interfaces:**
- Consumes: the record shape from Task 1.
- Produces: one ledger per cluster, all passing `validate_ledger.py`.

- [ ] **Step 1: Dispatch all 9 extractors in one message**

Clusters: `agents` = `.claude/agents/*.md`; `workflows` = `.claude/workflows/*`; `hil` = `hil`, `hil-pool-check`; `kernel` = `usb-kernel-recover`, `usb-kernel-debug` + their 2 scripts; `target` = `target-debug`, `esp-target-debug`; `usb` = `usbtest`, `usbmon`, `usb-sniffer` + `usbcap.sh`; `etm` = `etm-trace` + `boards.md` + 2 scripts; `tools` = `build-doc`, `code-size`, `pvs`, `make-release`, `read-doc`, `pre-pr` + `run_pvs.sh`, `search.py`; `claudemd` = `CLAUDE.md`.

Each gets `subagent_type: "general-purpose"` and this prompt, with `<FILES>`, `<PREFIX>` and `<OUT>` substituted:

> Read these files in full: `<FILES>` (repo root: `/home/hathach/code/tinyusb/.claude/worktrees/claude+hil-concurrent`).
>
> Extract every **falsifiable claim** they make about the codebase or the test rig, and write one JSON object per line to `<OUT>`. A falsifiable claim is any statement that a specific source could prove wrong: a file path, a CLI flag or env var, a function/constant/config-key name, a stated behavior ("X self-locks each board"), a number (timeout, width, count, duration), or a fact about the physical rig (bus map, probe uid, installed tool, sudoers entry).
>
> Record shape, one per line, no wrapping array:
> `{"id":"<PREFIX>-001","file":"<repo-relative path>","line":<1-based line the claim is on>,"class":"path|interface|behavior|number|rig|crossdoc","claim":"<VERBATIM text copied from that line>","settle_with":["<the file or command that would settle it>"],"earned":<true|false>}`
>
> Rules, all mandatory:
> 1. `claim` must be copied **verbatim** from the cited line — never paraphrase, never summarize. A validator re-reads the file and rejects the ledger if your text is not there.
> 2. **Return no verdicts.** Do not say whether a claim is true, do not check it, do not fix anything. Extraction only. Your opinion about correctness is out of scope and will be discarded.
> 3. `settle_with` names where the answer lives (e.g. `test/hil/hil_test.py argparse`, `ssh ci.lan lspci`), not the answer.
> 4. Set `earned: true` when the claim reads as hard-earned rig knowledge — an observed hardware quirk, a failure mode learned in an incident, a workaround whose rationale is experience rather than code. These are treated as source of truth downstream, so flagging matters.
> 5. Skip pure guidance ("bias toward caution", "prefer X") — not falsifiable.
> 6. `class: "crossdoc"` for a rule you can see stated in two of your own files with different wording.
>
> Return only: the ledger path and the claim count. Do not summarize the claims.

- [ ] **Step 2: Validate every ledger**

Run: `python3 $AUDIT/validate_ledger.py /home/hathach/code/tinyusb/.claude/worktrees/claude+hil-concurrent $AUDIT/ledgers`
Expected: `9 ledger(s), N claim(s), 0 error(s)`.
A non-zero exit means an extractor hallucinated or miscounted — re-dispatch **that cluster only**, with the validator's diagnostics quoted in the prompt.

- [ ] **Step 3: Prove no verdicts leaked in**

Run: `grep -ciE '"(claim|settle_with)":[^,]*(correct|wrong|stale|outdated|should be|actually)' $AUDIT/ledgers/*.jsonl`
Expected: `0` for every ledger. Any hit means the extractor judged; strip those fields or re-run the cluster.

- [ ] **Step 4: No commit** — scratchpad.

---

### Task 3: Mechanical sweep — path, interface and number claims

These classes are settled by a command, not by reading. Automate them so the reading budget goes to behavior claims.

**Files:**
- Create: `$AUDIT/sweep_mechanical.py`, `$AUDIT/verdicts/mechanical.jsonl`

**Interfaces:**
- Consumes: `$AUDIT/ledgers/*.jsonl` from Task 2.
- Produces: a verdict record per claim —
  `{"id": str, "verdict": "CONFIRMED"|"REFUTED"|"EARNED"|"UNVERIFIABLE", "citation": {"file": str, "line": int, "quote": str}, "note": str}`.
  `EARNED` is the hard-earned-evidence verdict: no source in scope settles it, and it stays
  in the docs untouched. `citation` may be null for `EARNED` and `UNVERIFIABLE` only.

- [ ] **Step 1: Write the failing test**

The sweep must reproduce the three drifts and the five legitimate non-resolving paths already found by hand, or it is not trustworthy:

```bash
# $AUDIT/test_sweep.sh
set -u
D=$(dirname "$0"); fail=0
out=$D/verdicts/mechanical.jsonl
# usbtest SKILL.md cites src/usb_descriptors.h and src/tusb_config.h (example-relative,
# not repo paths) and tools/usb/testusb.c (a kernel path) -- all must land as REFUTED
for p in usb_descriptors tusb_config testusb; do
  grep -q "\"verdict\":\"REFUTED\".*$p" "$out" || { echo "FAIL: $p not REFUTED"; fail=1; }
done
# placeholders and generated files must NOT be reported as drift
for p in "X.Y.Z" "dcd_x.c" "compile_commands.json" "local.json"; do
  grep -q "\"verdict\":\"REFUTED\".*$p" "$out" && { echo "FAIL: $p false positive"; fail=1; }
done
exit $fail
```

- [ ] **Step 2: Run it to verify it fails**

Run: `bash $AUDIT/test_sweep.sh`
Expected: FAIL — `grep: .../verdicts/mechanical.jsonl: No such file or directory`.

- [ ] **Step 3: Implement the sweep**

For each `path` claim: extract every path-shaped token from `claim`, then resolve it in this order — repo root; `find . -path "*/<token>"` (catches example-relative paths, recording the real base); a known-placeholder list (`X.Y.Z`, `dcd_x`, `*_*/*` globs); a generated/gitignored list (`compile_commands.json`, `local.json`, `cmake-build-*`). Repo-root hit → CONFIRMED. Found only elsewhere → REFUTED with the real path in `note`. Placeholder/generated → UNVERIFIABLE with the reason. Nothing anywhere → REFUTED.

For each `interface` claim: grep the file named in `settle_with` for the flag/env/symbol. Found → CONFIRMED with `file:line` and the matching line as `quote`. Not found → REFUTED.

Write records with `json.dumps(rec, separators=(',', ':'))` -- Step 1's test greps for
`"verdict":"REFUTED"` with no spaces, and pretty-printed JSON would silently pass it.

For each `number` claim: locate the constant's definition in `settle_with`, compare the literal. Equal → CONFIRMED; different → REFUTED with both values in `note`; no definition → UNVERIFIABLE.

- [ ] **Step 4: Run the sweep, then the test**

Run: `python3 $AUDIT/sweep_mechanical.py $AUDIT/ledgers $AUDIT/verdicts/mechanical.jsonl && bash $AUDIT/test_sweep.sh`
Expected: sweep prints per-class counts; test prints no `FAIL:` lines, exit 0.

- [ ] **Step 5: No commit** — scratchpad.

---

### Task 4: Rig-state claims — read-only probe

**Files:**
- Create: `$AUDIT/rig_probe.log`, `$AUDIT/verdicts/rig.jsonl`

**Interfaces:**
- Consumes: `class: "rig"` claims from Task 2.
- Produces: verdict records in the Task 3 shape, plus verdict `EARNED` for hardware knowledge no probe can settle.

- [ ] **Step 1: Confirm the rig is idle enough to probe**

Run: `ssh ci.lan 'python3 ~/…/hil_lock.py status; uptime'` — or, if no checkout path is known, `ssh ci.lan 'ls /tmp/tinyusb-hil-locks/ 2>/dev/null; uptime'`.
Expected: a holder list. Probing is read-only and safe even mid-CI; this is for interpreting results, not for gating.

- [ ] **Step 2: Capture one probe transcript**

Run, tee'd to `$AUDIT/rig_probe.log`:

```bash
ssh ci.lan 'set -x
uname -r; hostname
lspci -nn | grep -i usb
lsusb -t
ls /tmp/tinyusb-hil-locks/ 2>/dev/null
sudo -l 2>/dev/null | tail -20
which uhubctl openocd JLinkExe esptool.py STM32_Programmer_CLI 2>/dev/null
ls ~/bin ~/.local/bin 2>/dev/null'
```

Expected: a transcript covering bus map, controllers, installed flashers, sudoers scope, kernel version.

- [ ] **Step 3: Verdict each rig claim against the transcript**

CONFIRMED with the transcript line as `quote`; REFUTED with the current value in `note` (bus numbers renumber every boot — a refuted bus map is a **derivation-recipe** rewrite, not a delete); `EARNED` for anything the probe cannot see (a quirk, an incident, a workaround rationale) — those stay in the docs untouched.

- [ ] **Step 4: Sanity-check the split**

Run: `python3 -c "import json,collections,sys; print(collections.Counter(json.loads(l)['verdict'] for l in open('$AUDIT/verdicts/rig.jsonl')))"`
Expected: a count per verdict, and **zero** rig claims left without one.

- [ ] **Step 5: No commit** — scratchpad.

---

### Task 5: Behavior claims — read the implementing code

The bulk of the audit, and the class that produced the `hil-validate` failure. Four sub-batches so each ends with a checkable deliverable: **5a** `hil` + `hil-pool-check` + `agents` + `workflows`; **5b** `kernel` + `usb`; **5c** `target` + `etm`; **5d** `tools` + `claudemd`.

**Files:**
- Create: `$AUDIT/verdicts/behavior-{5a,5b,5c,5d}.jsonl`

**Interfaces:**
- Consumes: `class: "behavior"` claims from Task 2.
- Produces: verdict records in the Task 3 shape. `citation.quote` must be text that really exists at `citation.file:citation.line` — Task 7 re-checks it.

- [ ] **Step 1 (per batch): Verdict every behavior claim**

Open the file named in `settle_with`, find the implementing code, and record CONFIRMED / REFUTED / EARNED / UNVERIFIABLE with a `file:line` citation and a verbatim `quote`. Never mark CONFIRMED from memory of the code — open it. Where earned knowledge and current code disagree, record **both**: verdict `EARNED` plus a `note` naming the conflicting code. That is a finding, not an edit.

- [ ] **Step 2 (per batch): Verify the citations resolve**

Run: `python3 $AUDIT/validate_ledger.py <repo-root> $AUDIT/verdicts --field citation` — the same quote-in-window gate from Task 1, pointed at `citation.quote`.
Expected: `0 error(s)`. A failure means a citation was written from memory; re-open the file.

- [ ] **Step 3: Confirm complete coverage**

Run:

```bash
python3 - <<'EOF'
import json, glob
claims = {json.loads(l)['id'] for f in glob.glob('$AUDIT/ledgers/*.jsonl') for l in open(f)
          if json.loads(l)['class'] == 'behavior'}
done = {json.loads(l)['id'] for f in glob.glob('$AUDIT/verdicts/behavior-*.jsonl') for l in open(f)}
print('unverdicted:', sorted(claims - done))
EOF
```

Expected: `unverdicted: []`.

- [ ] **Step 4: No commit** — scratchpad.

---

### Task 6: Cross-doc rule inventory

No per-file agent can do this pass; it is where the `hil-operator` contradiction lived.

**Files:**
- Create: `$AUDIT/rules.md`

- [ ] **Step 1: Build the inventory**

For each rule the surface states more than once — board locking, run timeouts, output contracts, retry policy, config selection by hostname, forcing/`HIL_NO_BOARD_LOCK`, "never stop the actions-runner", worktree policy, report locations — list every `file:line` that states it and quote each statement verbatim.

- [ ] **Step 2: Flag every divergence**

For each rule with more than one wording, mark: **identical** (candidate for de-duplication down to one canonical home plus a reference), **complementary** (different aspects — keep both), or **contradictory** (a Task 8 fix, and a finding for the report).

- [ ] **Step 3: Verify the inventory caught the known case**

Run: `grep -c 'hil_test.py self-locks' $AUDIT/rules.md`
Expected: ≥ 2 — the rule is stated in both `hil/SKILL.md` and `hil-operator.md`, so an inventory that lists it once is incomplete.

- [ ] **Step 4: No commit** — scratchpad.

---

### Task 7: Apply the edits, one commit per surface

**Files:**
- Modify: `.claude/agents/*.md`, `.claude/workflows/*`, `.claude/skills/*/SKILL.md` + helper scripts, `CLAUDE.md` — only where a verdict says so.

- [ ] **Step 1: Edit `.claude/agents/*.md`**

Apply every REFUTED correction. Remove a rule only when the inventory marks it identical to one with a canonical home, replacing it with a reference. Leave every CONFIRMED and every EARNED claim alone.

- [ ] **Step 2: Gate and commit the agents surface**

```bash
cd /home/hathach/code/tinyusb/.claude/worktrees/claude+hil-concurrent
grep -h '^name:' .claude/agents/*.md            # every agentType in workflows must still resolve
git add .claude/agents && git commit -m "docs(agents): correct claims refuted by source"
```

- [ ] **Step 3: Edit and gate `.claude/workflows/*`**

```bash
cd /home/hathach/code/tinyusb/.claude/worktrees/claude+hil-concurrent
for f in .claude/workflows/*.js; do bash .claude/workflows/check.sh "$f"; done
bash -n .claude/workflows/check.sh
git add .claude/workflows && git commit -m "docs(workflows): correct claims refuted by source"
```

Expected: `OK: <file>` for all six.

- [ ] **Step 4: Edit and gate the skills surface**

```bash
cd /home/hathach/code/tinyusb/.claude/worktrees/claude+hil-concurrent
for s in .claude/skills/*/scripts/*.sh .claude/skills/pvs/run_pvs.sh; do bash -n "$s" || echo "SYNTAX $s"; done
for p in .claude/skills/*/scripts/*.py .claude/skills/read-doc/search.py; do python3 -m py_compile "$p" || echo "SYNTAX $p"; done
git add .claude/skills && git commit -m "docs(skills): correct claims refuted by source"
```

Expected: no `SYNTAX` lines.

- [ ] **Step 5: Edit and commit `CLAUDE.md`**

```bash
cd /home/hathach/code/tinyusb/.claude/worktrees/claude+hil-concurrent
git add CLAUDE.md && git commit -m "docs: correct CLAUDE.md claims refuted by source"
```

---

### Task 8: Findings report and handoff docs

**Files:**
- Create: `docs/superpowers/followup/pr<NNN>-<topic>.md` — one per code-side bug, only if any was found.

- [ ] **Step 1: Write the report**

Every REFUTED claim with its citation and what it became; every `EARNED`-vs-code disagreement from Task 5; every rule de-duplicated and where its canonical home now is. Report in chat — it is a review artifact, not a repo file.

- [ ] **Step 2: Write a handoff per code-side bug**

Only where the *code* is the wrong half. One doc per follow-up, per the repo's deferred-work rule: what is established (with citations), what remains, why it was split out.

- [ ] **Step 3: Commit any handoffs**

```bash
cd /home/hathach/code/tinyusb/.claude/worktrees/claude+hil-concurrent
git add docs/superpowers/followup && git commit -m "docs: hand off code-side bugs found by the instruction-surface audit"
```

---

### Task 9: Final gate

- [ ] **Step 1: Re-run the mechanical sweep against the edited tree**

Run: `python3 $AUDIT/sweep_mechanical.py $AUDIT/ledgers $AUDIT/verdicts/mechanical-after.jsonl`
Expected: zero REFUTED path/interface/number claims remain.

- [ ] **Step 2: Run the repo gates**

```bash
cd /home/hathach/code/tinyusb/.claude/worktrees/claude+hil-concurrent
for f in test/hil/test/test_*.py; do python3 "$f" >/tmp/$(basename "$f").log 2>&1 && echo "OK $f" || echo "FAIL $f"; done
pre-commit run --all-files
```

Expected: four `OK` lines; every pre-commit hook `Passed`. Note `test_hil_util.py` spawns a `sleep 30` subprocess — run it in the background, the foreground sandbox blocks it.

- [ ] **Step 3: Review the whole diff**

Run: `cd /home/hathach/code/tinyusb/.claude/worktrees/claude+hil-concurrent && git diff master --stat && git diff master -- .claude CLAUDE.md`
Expected: every hunk traceable to a REFUTED verdict or an inventory de-duplication. Anything else is scope creep — revert it.

---

### Task 10 (OPTIONAL — needs explicit approval): recurrence guard

Not in the approved spec. The audit fixes today's drift; nothing stops tomorrow's. A pre-commit hook that resolves every path cited in `.claude/**` and fails on an unresolvable one would have caught three of the drifts found in recon, and costs ~40 lines. Raise it with the user; build only on a yes.

---

## Notes for the executor

- The extractors in Task 2 are the only subagents in this plan. Every verdict is the main session's own work — that is the "trust nothing without source" requirement, and delegating verification voids it.
- `docs/superpowers/**` is out of scope even when a verdict proves a spec there is now wrong. Note it in the report instead.
- Delete this plan when its PR lands.
