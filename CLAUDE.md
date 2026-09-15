# TinyUSB Agent Instructions

## Working Rules

- Worktrees — symlink `deps_all` paths from `tools/get_deps.py` to the primary checkout; replace a symlink and rerun `get_deps.py` only for a different revision.
- Code references — boards: `hw/bsp/`, `docs/reference/boards.rst`; classes: `src/class/`; core/config: `src/tusb.h`, `src/tusb_option.h`, each example's `src/tusb_config.h`; build/deps: `tools/build.py`, `tools/get_deps.py`; unit tests: `test/unit-test/project.yml`.

## Code Rules

- C99, 2-space indent, no tabs; `snake_case` helpers, `UPPER_CASE` macros, `tud_`/`tuh_` public APIs, `TU_` macros.
- No dynamic allocation. Defer ISR work to task context. Use `TU_ASSERT()` for error checks; check return values.
- Keep headers self-contained with `#if CFG_TUSB_MCU` guards. Include order: C stdlib → tusb common → drivers → classes.

## Skills

- Skill script tests live in `.claude/test/test_*.py`.
- From [agentrc](https://github.com/hathach/agentrc): skills `read-doc`, `simplify-gate`, `usb-sniffer`, `usb-kernel-debug`, `rtt`, `etm-trace`, `target-debug`; agents `pvs-studio`, `code-writer`, `code-verifier`, `finding-verifier`, `pr-ci-watcher`, `pr-review-validator`. When one is unavailable, skip the step that needs it.
- Driver audit: agentrc's `code-audit` workflow with `dirs` (e.g. `src/portable/<vendor>/<driver>`) and `dimensions`: `correctness: transfer state machines, endpoint bookkeeping, completion and error paths`; `ISR safety: work deferred to task context, shared-state races, register access ordering`; `register use vs datasheet and MCU errata: cross-check the reference manual AND errata sheets via the read-doc skill; if the skill is unavailable treat the document as absent (low confidence, never a web/filesystem substitute); a missing erratum workaround is a finding`; `style: repo conventions (TU_ASSERT, no dynamic allocation, include order, naming)`. Clean only when `confirmed`, `dropped` and `unverified` are all empty.
- Static analysis: the `pvs-studio` agent with rules `.PVS-Studio/.pvsconfig` (never add suppressions) on a board's examples build; `raspberry_pi_pico` mirrors CI, `stm32f407disco` is fastest.

## Claude and Codex Collaboration

- Keep orchestration in `.claude/workflows/`, nested one level at most. Workflows verify with the `code-verifier` and `finding-verifier` agents; an independent Codex opinion comes from the `chief` session's coworker lanes, not from a workflow.

## Build and Validate

- Build contract: `.claude/skills/build/SKILL.md`. Its script resolves a change to boards and builds them; `--shared` writes `cmake-build/cmake-build-<board>`, the dir HIL flashes from, so preserve it. Flash with `ninja -C cmake-build/cmake-build-<board> <example>-jlink` or `-openocd`.
- ESP-IDF: `. "$IDF_PATH/export.sh"` before anything Espressif; verification still goes through the build contract, with `idf.py -DBOARD=<board> flash monitor` in the example reserved for interactive flash and monitor.
- Before submitting: `pre-commit run --all-files` (includes unit tests).
- For code changes: build the full example set for boards that exercise the changed modules. Add fuzz/HIL coverage for parsers or protocol state machines.
- After board/dependency changes, regenerate docs with `build-doc`.
- Before committing code changes, verify size impact with `code-size`.

## PRs and Follow-ups

- Before opening or updating a PR, follow Build and Validate; use `pre-pr` when workflows are available.
- After opening a PR, drive reviews and CI to green with the user-level `pr-babysit`
  workflow, from a **clean** checkout of the PR branch with no other writer in it: an
  edit to a path the run already owns cannot be told from its own and would be pushed.
  ```
  Workflow /pr-babysit {"pr": <num>, "reviewers": ["codex","copilot","coderabbit","claude"],
    "autoRun": ["codex","coderabbit","claude"],
    "protected": "^test/hil/[^/]+\\.json$", "ciWait": 30}
  ```
  `reviewers` is required — `[]` runs the CI lane and harvests no review — and
  `autoRun` is the subset of it that runs on every push and gates `done` (default: all
  of `reviewers`), so Copilot is harvested but not waited on. `protected` is a regex
  matched against repo-relative paths. `maxCycles` bounds the
  review/fix/CI rounds (default 3); `checkoutDir` points it at the PR checkout when you
  are not running from inside one.
  It dry-runs by default. `"autoPush": true` is what tells it to push and to post PR
  comments, and is the user's to give — it does not stop an agent from doing either,
  it decides whether the workflow asks. A dry run still reruns a CI job classified as
  an infra flake, the one GitHub action dry mode does not suppress; fixes stay
  uncommitted and nothing is posted. `protected` keeps the HIL rig rosters out of
  every fix scope, since a failure needing hardware changed stays red for a human.
  Omit `build` and it resolves this repo's build contract. It refuses before touching
  anything on `dirty-start`, `wrong-branch`, `wrong-head`, `wrong-remote` (github.com
  over https or ssh only, and every push URL must be the PR's head repository),
  and `preflight-died`; `push-failed` with a `commit failed audit:` detail
  means the commit was made and deliberately not pushed.
- If workflows are unavailable, use `gh pr checks <num>` and `gh pr view <num> --comments`; fix failures, push, and resolve review threads.
- The follow-up label is `followup`.
