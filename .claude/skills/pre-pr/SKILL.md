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

- `python3 tools/ci_select.py --base $BASE test/hil/tinyusb.json` → JSON with the affected
  bsp `families`, the affected rig `boards`, and per-file `reasons`. `full: true` means a
  broad/infra change.
- Affected families = `families` ∪ the family of every name in `boards`. Neither half is
  enough alone: only the port and bsp rules fill `families` (a class/core/example change
  reports boards but no families), and `boards` only ever names rig boards (an off-rig driver
  change — `dcd_samx7x.c` → `same7x`, `boards: {}` — would never be compiled).
  - A board's family is the `hw/bsp/<family>/boards/<board>/` directory holding it.
  - When `full: true`, `boards` names every rig board and carries no signal — use `families`
    alone there, plus the representative set below.
- Sample ONE board per affected family: prefer a rig-roster board of that family, else the
  first entry in `hw/bsp/<family>/boards/`.
  - Rig roster: `python3 -c "import json;print([b['name'] for b in json.load(open('test/hil/tinyusb.json'))['boards']])"`
- Add the representative set `stm32f407disco` + `raspberry_pi_pico` when `full: true` (broad
  change — class/core/common/infra).
- Cap at 4 boards and tell the user which families the cap dropped. A broad change affects ~20
  families, so the order matters: keep `stm32f407disco` and `raspberry_pi_pico` first whenever
  their families are affected, then fill from the remaining families (spread across vendors —
  don't let one vendor's family names take every slot). The list must NEVER end up empty —
  final fallback is `[stm32f407disco]` (full-check throws on an empty list).
- Docs-only (`full: false`, no families, no boards) keeps §1's minimal software-only gate.

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
