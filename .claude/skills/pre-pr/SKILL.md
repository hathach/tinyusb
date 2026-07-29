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

- For each changed `src/portable/<vendor>/<ip>/` (or `src/portable/<name>/` for single-level ports): families = the `hw/bsp/<family>` directories whose build files reference it — `grep -rl "<vendor>/<ip>" hw/bsp/*/family.cmake hw/bsp/*/family.mk`, then take each matching file's directory name.
- For `src/class/*`, `src/common/*`, `src/device/*`, `src/host/*`, or `src/tusb.c`: broad change — use `stm32f407disco` + `raspberry_pi_pico` PLUS any families from portable changes.
- For `hw/bsp/<family>/...` changes: that family directly.
- Catch-all: any other C/CMake source change (`examples/*`, `test/*`, anything unmatched above) → the representative set `stm32f407disco` + `raspberry_pi_pico`. The boards list must NEVER end up empty — final fallback is `[stm32f407disco]` (full-check throws on an empty list).
- Rig roster: `python3 -c "import json;print([b['name'] for b in json.load(open('test/hil/tinyusb.json'))['boards']])"`
- Pick ONE board per affected family, preferring boards on the rig roster; otherwise the first entry in `hw/bsp/<family>/boards/`. Cap at 4 boards and tell the user which families the cap dropped.

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
