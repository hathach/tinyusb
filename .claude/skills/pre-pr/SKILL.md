---
name: pre-pr
description: Use before opening or updating a TinyUSB PR — derives the affected boards from the branch diff and hands that selection to the `validate` workflow (unit tests, per-board builds, code size, PVS, diff review). Software only; HIL boards are reported as candidates for the caller, never run here.
---

# Pre-PR validation

Software gate for the current branch. `chief` is the launcher: a worker running this skill returns the selection below, chief launches `validate`. Selection and validation use the same base SHA.

## 1. Scout the diff (inline — no agents)

- `BASE`: the pinned task base chief supplied; otherwise the base the user named, defaulting to `master`, resolved to a SHA (`git rev-parse`) before selection.
- `git diff --name-only $(git merge-base HEAD $BASE)..HEAD`

## 2. Map changes to boards

- `python3 tools/ci_select.py --base $BASE test/hil/tinyusb.json` → JSON with independent
  top-level HIL and nested `build` selections.
- `buildBoards`: when `build.full` is false, one board per `build.families`; prefer a
  rig-roster board of that family, else the first entry in `hw/bsp/<family>/boards/`. When
  `build.full` is true, use `stm32f407disco` + `raspberry_pi_pico` as the broad representative
  set.
- A non-default build target selected by the changed path must run through its existing
  command (for example, `examples-membrowse-upload` for `tools/membrowse_report.py`). A
  default board sweep is not evidence for a target it does not execute; report the target
  unverified if the current workflow cannot express it.
- `hilCandidates`: derive only from the top-level HIL selection, never from `buildBoards`. When
  top-level `full` is false, one board per family from the keys of top-level `boards`; when it
  is true, the broad representative set above. Report them; do not launch HIL. Hardware runs are
  the caller's decision, through `hil-validate` under the HIL contract
  (`.claude/skills/hil/SKILL.md`), which also names the firmware layout they need.
  - A board's family is the `hw/bsp/<family>/boards/<board>/` directory holding it.
  - Rig roster: `python3 -c "import json;print([b['name'] for b in json.load(open('test/hil/tinyusb.json'))['boards']])"`
- Return the full selection. Trimming it is the caller's decision, and the caller reports what
  it dropped.
- Empty `buildBoards`: run `pre-commit run --all-files` plus the smallest targeted check for
  changed tooling; no board build. HIL candidates never stand in for build coverage.
- A non-empty selection yielding no sampled board is a mapping failure.

## 3. Launch

If `buildBoards` is non-empty, chief invokes the Workflow tool:
`{ name: 'validate', args: { boards: buildBoards, base: BASE, maxCycles: 1 } }` — repairs go
back through its writer path, never the workflow's internal fixer — adding `skip: ['review']`
only when a coworker lane reviews the diff.

## 4. Summarize

- Per-stage table: unit / build:<board> / size / pvs — pass/fail with the first error for each failure.
- HIL: not run by pre-pr; list `hilCandidates`.
- End with the software verdict — passed or failed, and what to fix first. If the caller dropped
  required build coverage, the omitted scope is unverified and the verdict is incomplete, even
  when every executed stage passed. It is not a ship verdict when the task requires hardware
  evidence.
