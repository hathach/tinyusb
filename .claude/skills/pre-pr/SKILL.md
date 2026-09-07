---
name: pre-pr
description: Use before opening or updating a TinyUSB PR — derives affected boards from the branch diff, runs the full-check workflow (software validation + optional HIL on the rig), and summarizes a ship/no-ship verdict.
---

# Pre-PR validation

Run the software + hardware gate for the current branch. The user invoking this skill is the opt-in for launching the workflows below.

## 1. Scout the diff (inline — no agents)

- `BASE` = `master` unless the user names another base.
- `git diff --name-only $(git merge-base HEAD $BASE)..HEAD`

## 2. Map changes to boards

- `python3 tools/ci_select.py --base $BASE test/hil/tinyusb.json` → JSON with independent
  top-level HIL and nested `build` selections.
- `buildBoards`: when `build.full` is false, sample one board per `build.families`; prefer a
  rig-roster board of that family, else the first entry in `hw/bsp/<family>/boards/`. When
  `build.full` is true, use `stm32f407disco` + `raspberry_pi_pico` as the broad representative
  set.
- A non-default build target selected by the changed path must run through its existing
  command (for example, `tinyusb_metrics` for `tools/metrics.py`). A default board sweep is
  not evidence for a target it does not execute; report the target unverified if the current
  workflow cannot express it.
- `hilBoards`: derive only from the top-level HIL selection, never from `buildBoards`. When
  top-level `full` is false, sample one board per family from the keys of top-level `boards`;
  when it is true, use the broad representative set above. Run HIL only if the rig is reachable
  per `.claude/skills/hil/SKILL.md`.
  - A board's family is the `hw/bsp/<family>/boards/<board>/` directory holding it.
  - Rig roster: `python3 -c "import json;print([b['name'] for b in json.load(open('test/hil/tinyusb.json'))['boards']])"`
- Cap the union of both lists at 4 boards and report what the cap dropped; spread the sample
  across vendors.
- Empty selections: run `pre-commit run --all-files` plus the smallest targeted check for changed tooling; skip board build/HIL.
- A non-empty selection yielding no sampled board is a mapping failure.

## 3. Launch

If the unique union of `buildBoards` and `hilBoards` is non-empty, invoke the Workflow tool:
`{ name: 'full-check', args: { boards: [...new Set([...buildBoards, ...hilBoards])], hilBoards, base: BASE } }`

## 4. Summarize

- Per-stage table: unit / build:<board> / size / pvs, then HIL per board — pass/fail with the first error for each failure.
- If the hardware result has non-empty `locked` (a CI job held those boards): ask the user to choose **Force now** (re-invoke `hil-validate` with `force: true` for those boards; user accepts the risk of colliding with a mid-test CI job), **Keep waiting** (re-invoke `hil-validate` for them after a few minutes; ask again if still locked), or **Accept** the partial verdict. Never force without the user's answer.
- Wedged boards: point at `.claude/skills/usb-kernel-recover/SKILL.md`.
- End with a clear ship / no-ship verdict and what to fix first.
