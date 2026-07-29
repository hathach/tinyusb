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
  bsp `families`, the affected rig `boards`, and per-file `reasons`. `full: true` means a
  broad/infra change.
- Sample ONE board per entry in `families`: prefer a rig-roster board of that family, else the
  first entry in `hw/bsp/<family>/boards/`. Build boards come from `families`, never from
  `boards` alone — most families have no rig board, so an off-rig driver change
  (`dcd_samx7x.c` → `same7x`, `boards: {}`) would otherwise never be compiled.
  - Rig roster: `python3 -c "import json;print([b['name'] for b in json.load(open('test/hil/tinyusb.json'))['boards']])"`
  - A board's family is the `hw/bsp/<family>/boards/<board>/` directory holding it.
- Add the representative set `stm32f407disco` + `raspberry_pi_pico` when `full: true` (broad
  change — class/core/common/infra); `families` alone doesn't say which board exercises it.
- Cap at 4 boards and tell the user which families the cap dropped. The list must NEVER end up
  empty — final fallback is `[stm32f407disco]` (full-check throws on an empty list).
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
