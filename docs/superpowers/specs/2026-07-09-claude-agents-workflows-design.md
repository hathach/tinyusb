# Multi-Agent Dev/Test Setup for TinyUSB — Design

Date: 2026-07-09
Branch: worktree-claude-agents-workflows

## Goal

Give Claude Code sessions in this repo a reusable, efficient multi-agent harness
for developing and testing TinyUSB: custom worker agents that already know the
repo's build/test/rig discipline, and small deterministic workflows that fan
them out. The orchestrator (main session) authors arguments and reads verdicts;
workers do the volume.

## Context

- Existing process skills: `hil`, `code-size`, `pvs`, `build-doc`, `usbmon`,
  `usb-debug`, `usb-recover`, `make-release` (`.claude/skills/`).
- One prototype workflow exists in the master working tree (untracked):
  `.claude/workflows/port-audit.js`. This design supersedes it.
- No custom agent definitions exist yet (`.claude/agents/` absent).
- Test infra: `test/unit-test` (ceedling), `test/hil` (`hil_test.py`,
  `tinyusb.json`), `test/fuzz`; size metrics via
  `tools/metrics_compare_base.py`.

## Architecture

Layered: **agents** (who does the work, with baked-in domain knowledge) ×
**workflows** (deterministic fan-out/join) × **one skill** (human entry point).

### Worker agents — `.claude/agents/*.md`

Tiered models (owner revision 2026-07-09; originally all-opus): `port-dev`,
`driver-reviewer` and `target-debugger` on **opus** at **xhigh**;
`hil-operator`, `pr-monitor` and `static-analyzer` on **sonnet**; `builder`
on **haiku** (mechanical, log-heavy). The registry has no effort field —
xhigh is requested per `agent()` call by whichever workflow or session spawns
the agent.

| Agent | Effort | Role |
|---|---|---|
| `builder` | low | Build one board's example set with the canonical commands: `cmake -B cmake-build-<board> -DBOARD=<board> -G Ninja -DCMAKE_BUILD_TYPE=MinSizeRel` from `examples/` (exact dir name — HIL expects it), `python3 tools/get_deps.py` on missing deps, `. $HOME/code/esp-idf/export.sh` for Espressif boards, tolerate non-critical objcopy failures. Returns structured `{board, pass, failures: [{example, firstError}]}`. |
| `port-dev` | xhigh | Implement one well-scoped change in one port / file set. Follows repo rules: C99, 2-space indent, snake_case, `TU_ASSERT`, no dynamic allocation, ISR work deferred to task context. Runs `clang-format` (repo `.clang-format`) on touched files before finishing. Cross-checks the MCU datasheet in `$HOME/Documents/calibre-library` when changing dcd/hcd register logic. Verifies with a targeted build of one board using the port. Returns `{item, diffstat, buildOk, notes}`. |
| `driver-reviewer` | xhigh | Review one dcd/hcd directory against dimensions: correctness, ISR safety, register use vs. datasheet AND MCU errata (calibre library; missing erratum workarounds are findings), style. Returns structured findings `{file, line, snippet, why, severity, confidence}` — coverage-first (report everything; filtering happens downstream). |
| `hil-operator` | default | All rig interaction — the actions-runner service is NEVER stopped; per-board flock locks arbitrate with concurrent CI. `hil_test.py` runs rely on its per-board self-locking; manual hardware work (JLink/GDB, usbtest, serial) is wrapped in `test/hil/board_lock.py hold/release`; rig-wide ops (uhubctl, pci-rebind) require `hold --all`; on wedge `usb_recover.sh` + dmesg. Used strictly serially — never two instances concurrently. |
| `target-debugger` | xhigh | Root-cause one USB misbehavior on one board by instrumenting the device side (TU_LOG/RTT, RAM ring-buffer trace, GDB autopsy, J-Link PC-sampling) with dual-side host+target capture, per `.claude/skills/usb-target-debug/SKILL.md`, plus wire-level capture via the ataradov hardware tap (`.claude/skills/usb-sniffer/SKILL.md`) when the host side can't see or is disputed. Deliberately serial loop under one held board lock (released around `hil_test.py` runs, which self-lock); strictly one instance. Diagnosis standard: evidence shows the mechanism, or a fix flips the ORIGINAL failing case on hardware; stops after two evidence-free cycles with a partial report. Hard rule "fix stays, probe goes, re-verify clean": instrumentation reverted, candidate fix left uncommitted and re-verified on a clean build, pristine firmware reflashed before lock release. Returns `{board, bug, diagnosis, confirmed, ruledOut[], evidence[], fixDiffstat, fixVerified, instrumentationReverted, lockReleased, notes}`. |
| `pr-monitor` | default | Triage one GitHub PR via `gh`: check CI status (`gh pr checks`), read failing run logs and classify each failure infra/flake vs real; re-run infra failures (`gh run rerun --failed`); harvest automated review comments (Codex/Copilot/Claude bots — knows their signals: Codex posts a "Didn't find any major issues" issue comment when clean; Copilot drops out of `requested_reviewers` when done; bot logins differ across APIs); adversarially validate each finding against the actual code. Returns structured triage `{ci: {status, infraRerun[], realFailures[]}, findings: [{source, file, line, claim, verdict, fixHint}]}`. Read/triage/re-run/reply only — never edits code. |
| `static-analyzer` | low | Run PVS-Studio (SAST + MISRA C:2023/C++:2008) for one board: build with exported `compile_commands.json` (via `run_pvs.sh` solo, or a dedicated `cmake-build-pvs` dir when parallel builders run), analyze against `.PVS-Studio/.pvsconfig`, gate on diagnostics in files changed vs a base ref. Returns `{pass, ga1, ga2, changedFindings[], detail}`; `pass=false` only on GA:1 in changed files or tool failure. Read-only. |

### Workflows — `.claude/workflows/*.js`

| Workflow | Args | Shape |
|---|---|---|
| `validate.js` | `{boards[], examples?, base?, skip?: ('unit'\|'size'\|'pvs')[]}` | One parallel stage: unit tests (ceedling) + one `builder` per board + code-size compare (`tools/metrics_compare_base.py` vs `base`, default master) + PVS analyze (`static-analyzer` agent). Join → plain-JS verdict `{pass, failures[]}`. Barrier is correct here: the verdict needs all results. |
| `fanout-dev.js` | `{task, items[], board?, review?, worktree?}` | `pipeline(items)`: `port-dev` per item → `builder` verify → optional `driver-reviewer` pass. Workers share the tree by default (ports are disjoint directories); `worktree: true` switches on per-agent worktree isolation for collision-prone tasks. Returns per-item results. |
| `driver-review.js` | `{dirs[], dimensions?, question?}` | Supersedes `port-audit.js`. Scan stage per (dir × dimension) → adversarial verify per finding (verifier prompted to refute) → confirmed findings only. |
| `hil-validate.js` | `{boards[], force?}` | Strictly serial `for` loop of `hil-operator` calls; each board is protected by `hil_test.py`'s own per-board flock, so the actions-runner keeps running throughout. Boards found locked (a concurrent CI job mid-test) are retried once at the end of the loop; boards still locked are returned in `locked[]` for a user force/wait/accept decision. `force: true` (user-authorized only) bypasses locks via `HIL_NO_BOARD_LOCK=1`. Returns per-board `{board, pass, detail}` plus `wedged[]` and `locked[]`. |
| `full-check.js` | `{boards[], ...}` | Thin composer: `workflow('validate', ...)` → only if green → `workflow('hil-validate', ...)`. Single nesting level (children do not nest further). |
| `pr-babysit.js` | `{pr, maxCycles?, autoPush?}` | Cycle until CI green + review threads resolved, or `maxCycles` (default 3): `pr-monitor` triage (blocks on `gh pr checks --watch` while CI runs) → valid findings + real CI failures grouped by file/port → `port-dev` fix per group (pipeline) → `driver-reviewer` verifies each fix addresses its finding → one commit + push per cycle. Every actioned inline comment is both **replied to and marked resolved** (GraphQL `resolveReviewThread`): refuted findings get the refutation, fixed findings get a "fixed in <sha>" note. `autoPush` defaults **false** (dry run: fixes stay uncommitted, nothing posted); **passing `autoPush: true` is the explicit push authorization** for follow-up commits and PR comments on that branch (scoped exception to the hold-pushes-until-told rule). |

### Board lock protocol — `test/hil/` (repo code)

CI and dev sessions share the rig concurrently; the actions-runner service is
never stopped. Arbitration is per-board kernel flocks in
`/tmp/tinyusb-hil-locks/<board>.lock` — auto-released when the holder process
dies, with holders truncating their lock-file record on release so records
stay truthful (`/tmp` clears on reboot):

- **`test/hil/board_lock.py`** (new tool): `hold <boards|--all> --reason TEXT`
  spawns a background holder process flocking each board file (JSON
  `{pid, reason, since}` written inside for debuggability); the holder's own
  LOCK_NB flock is the sole authority — there is deliberately no pid-based
  pre-check (recorded pids can be stale or recycled). `release <boards|--all>`
  probes each board's flock: a free lock only gets its stale record cleared;
  a genuinely held one gets its recorded holder SIGTERMed — unless the holder
  reason is `hil_test.py` (a CI run mid-test), which release refuses to kill.
  `status` lists holders. `--all` is required before rig-wide operations
  (uhubctl power cycling, pci-rebind — bus renumbering affects every board).
- **`hil_test.py` guard** (small patch to the per-board worker): take the
  board's flock non-blocking before flashing and hold it for that board's
  flash+test; on acquire it writes its own holder info
  (`{pid, reason: "hil_test.py", since}`) so conflicts report truthfully in
  both directions, and truncates that record on release (the pool worker
  outlives the per-board flock). If already held, FAIL the board immediately —
  `Failed: board locked: <holder info>` — no flash, no waiting.
  The CI job fails visibly for exactly those boards and `re-run failed`
  passes once the lock is released (`build.yml` already retries the HIL step
  once, absorbing short dev sessions). Guard proceeds unlocked — with a
  printed warning — if the lock dir is unusable, and `-b` names absent from
  the config are a hard error rather than a silent zero-test green run.
- **Re-entrancy rule:** dev sessions do NOT pre-hold boards they are about to
  run `hil_test.py` on (it self-locks; pre-holding deadlocks it).
  `board_lock.py hold` is for hardware work outside `hil_test.py` only.
- **Symmetric conflicts (CI running while an agent tests):** CI mid-test on a
  board holds that board's flock, so the dev side hits it — `board_lock.py
  hold` refuses showing the holder, and a dev `hil_test.py` run fails that
  board fast. Two sessions can never double-flash a board.
  `hil-validate.js` retries locked boards once at the end of its loop (CI
  finishes a board in minutes); manual sessions wait and retry when the
  holder reason is `hil_test.py`. Concurrent activity on *different* boards
  is normal — CI's own `hil_test.py` already runs boards in parallel via
  `multiprocessing.Pool`.
- **User decision on persistent locks:** workers cannot prompt the user, so
  boards still locked after the retry are returned in `locked[]`; the main
  session then asks the user — **force** (re-invoke `hil-validate` with
  `force: true`, which runs `hil_test.py` with `HIL_NO_BOARD_LOCK=1`: a
  bypass, never killing the holder, at the user-accepted risk of colliding
  with a mid-test CI job), **continue waiting** (re-invoke later), or
  **accept** the partial result. Workers never force on their own; forcing
  requires the user's explicit authorization relayed in the prompt.
- **Propagation caveat:** CI enforces the guard only once the patch lands on
  master (CI runs `hil_test.py` from each PR's merge-with-master ref). This
  also widens blast radius beyond `.claude/` config into shared CI test
  infra — the change is a small isolated guard, but it needs its own CI pass
  and careful review on the eventual PR.

### Entry point — `.claude/skills/pre-pr/SKILL.md`

`/pre-pr` instructs the session to: scout the diff inline (cheap), map changed
`src/portable/<vendor>/<ip>` and `src/class/*` to affected families and pick
test boards from `hw/bsp` (fallback: `stm32f407disco`, `raspberry_pi_pico`),
launch `full-check` with that board list, and summarize the verdict. Markdown
carries the judgment; JS carries the orchestration.

## Model & effort policy

- Tiered worker models: `port-dev`/`driver-reviewer`/`target-debugger` **opus**
  `xhigh`; `hil-operator`/`pr-monitor` **sonnet**; `builder` **haiku**.
- Inline workflow stages: unit/size **haiku**; pvs **sonnet** (low effort);
  pr-babysit push/replies **sonnet**.
- Agent frontmatter `model:` is canonical for `agentType` calls; it is read
  at session-start registration, so tier changes apply from the next session.
- Deterministic control flow (loops, joins, filtering, verdict assembly) is
  plain JS in the workflow scripts — zero model tokens.
- The orchestrating session derives work lists inline (glob/grep) and passes
  them as `args`; worker prompts carry paths, not file contents.

## Error handling

- Workers always return schema-validated structured output; validation retries
  happen at the tool-call layer.
- Workflows `filter(Boolean)` for killed agents and `log()` every skipped or
  dropped item — no silent truncation.
- `validate.js` reports partial results (a failed stage becomes a failure entry,
  not a thrown error).
- `hil-validate.js`: board access arbitrated by `hil_test.py` self-locking; a
  locked board fails fast with the holder info (never forced); wedged boards
  reported, not retried blindly.

## Success criteria

Each piece smoke-tested on a minimal real scope before the branch is done:

1. `validate.js` on `stm32f407disco` + `raspberry_pi_pico` (real build, unit
   tests, size compare, PVS) returns a correct verdict object.
2. `fanout-dev.js` on a trivial 2-port task on this branch; diffs build clean
   and are `.clang-format`-clean.
3. `driver-review.js` on 2 driver dirs returns only verified findings.
4. `hil-validate.js` on 1 board against the real rig with the actions-runner
   ACTIVE throughout: while a `board_lock.py` hold is in place the run fails
   fast citing the holder and returns the board in `locked[]`; with the lock
   still held, `force: true` proceeds (user-authorized bypass); after
   release a normal run passes; a direct `hil_test.py` run under a held lock
   fails that board without flashing.
5. `/pre-pr` end-to-end on this branch's own diff.
6. `pr-babysit.js` with `autoPush: false` (dry cycle) on a real open PR:
   CI failures classified correctly, at least one bot finding correctly
   validated or rejected, proposed fixes produced but not pushed.

## Out of scope

- CI (GitHub Actions) integration — this harness is for interactive sessions.
- Fuzzing orchestration.
- usbtest/usbtest-host stress batteries (existing separate branch).
- Removing the untracked `port-audit.js` prototype from the master working
  tree happens when this branch merges (it is not tracked by git).
