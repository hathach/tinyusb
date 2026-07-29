---
name: pre-pr
description: Use before opening or updating a TinyUSB PR — derives affected boards from the branch diff, runs the full-check workflow (software validation + optional HIL on the rig), and summarizes a ship/no-ship verdict.
---

# /pre-pr — pre-PR validation

Run the software + hardware gate for the current branch. The user invoking this skill is the opt-in for launching the workflows below.

## 1. Scout the diff (inline — no agents)

- `BASE` = `master` unless the user names another base.
- `git diff --name-only $(git merge-base HEAD $BASE)..HEAD`
- If NO C sources changed (only docs / `.claude/` / tools): say so, and run a minimal software-only gate — `boards = [stm32f407disco]`, no HIL — unless the user asks for more.

## 2. Map changes to boards

- `python3 test/hil/hil_select.py --base $BASE test/hil/tinyusb.json` → JSON with the affected
  rig boards (`boards`) and per-file `reasons`. `full: true` means a broad/infra change.
- Build-board sampling: from the selection's boards (or, when `full`, the representative set
  `stm32f407disco` + `raspberry_pi_pico`), pick ONE board per family, preferring rig-roster
  boards; cap at 4 and tell the user which families the cap dropped. The boards list must
  NEVER end up empty — final fallback is `[stm32f407disco]`.
- A `full: true` selection or an empty one (docs-only) keeps today's behavior: minimal
  software-only gate for docs-only, representative set otherwise.

## 3. HIL boards

- `hilBoards` = chosen boards that are on the rig roster. This host must be able to reach the rig (per `.claude/skills/hil/SKILL.md`: host `ci`/`tusb` = local, any other host (dev PC) = remote). If none qualify, run software-only.

## 4. Launch

Invoke the Workflow tool:

```
{ name: 'full-check', args: { boards: [...], hilBoards: [...], base: BASE } }
```

## 5. Summarize

- Per-stage table: unit / build:<board> / size / pvs, then HIL per board — pass/fail with the first error for each failure.
- If the hardware result has non-empty `locked` (a CI job held those boards): ask the user with AskUserQuestion — **Force now** (re-invoke `hil-validate` with `force: true` for those boards; user accepts the risk of colliding with a mid-test CI job), **Keep waiting** (re-invoke `hil-validate` for them after a few minutes; ask again if still locked), or **Accept** the partial verdict. Never force without the user's answer.
- Wedged boards: point at `.claude/skills/usb-kernel-recover/SKILL.md`.
- End with a clear ship / no-ship verdict and what to fix first.
