# TinyUSB Agent Instructions

## Working Rules

- Worktrees — symlink `deps_all` paths from `tools/get_deps.py` to the primary checkout; replace a symlink and rerun `get_deps.py` only for a different revision.
- Code references — boards: `hw/bsp/`, `docs/reference/boards.rst`; classes: `src/class/`; core/config: `src/tusb.h`, `src/tusb_option.h`, each example's `src/tusb_config.h`; build/deps: `tools/build.py`, `tools/get_deps.py`; unit tests: `test/unit-test/project.yml`.

## Code Rules

- C99, 2-space indent, no tabs; `snake_case` helpers, `UPPER_CASE` macros, `tud_`/`tuh_` public APIs, `TU_` macros.
- No dynamic allocation. Defer ISR work to task context. Use `TU_ASSERT()` for error checks; check return values.
- Keep headers self-contained with `#if CFG_TUSB_MCU` guards. Include order: C stdlib → tusb common → drivers → classes.

## Skills

- Agents, skills and workflows beyond this repo's `.claude/` come from [agentrc](https://github.com/hathach/agentrc); install it before agent work on TinyUSB.
- Skill script tests live in `.claude/test/test_*.py`.
- Hardware diagnosis and repair use agentrc's `hw-debugger`; independent hardware claims and committed-fix validation use `hw-validator`, orchestrated by `chief`.
- Driver audit: the `code-audit` workflow with `dirs` (e.g. `src/portable/<vendor>/<driver>`) and `dimensions`: `correctness: transfer state machines, endpoint bookkeeping, completion and error paths`; `ISR safety: work deferred to task context, shared-state races, register access ordering`; `register use vs datasheet and MCU errata: cross-check the reference manual AND errata sheets via the read-doc skill; if the skill is unavailable treat the document as absent (low confidence, never a web/filesystem substitute); a missing erratum workaround is a finding`; `style: repo conventions (TU_ASSERT, no dynamic allocation, include order, naming)`.
- Static analysis: the `pvs-studio` agent with rules `.PVS-Studio/.pvsconfig` (never add suppressions) on a board's examples build; `raspberry_pi_pico` mirrors CI, `stm32f407disco` is fastest.

## Build and Validate

- Build contract: `.claude/skills/build/SKILL.md`. Its script resolves a change to boards and builds them; `--shared` writes `cmake-build/cmake-build-<board>`, the dir HIL flashes from, so preserve it. Flash with `ninja -C cmake-build/cmake-build-<board> <example>-jlink` or `-openocd`.
- HIL contract: `.claude/skills/hil/SKILL.md`. It owns taking and releasing a rig board and names the HIL config json for the host you are on.
- ESP-IDF: `. "$IDF_PATH/export.sh"` before anything Espressif; verification still goes through the build contract, with `idf.py -DBOARD=<board> flash monitor` in the example reserved for interactive flash and monitor.
- Before submitting: `pre-commit run --all-files` (includes unit tests).
- For code changes: build the full example set for boards that exercise the changed modules. Add fuzz/HIL coverage for parsers or protocol state machines.
- After board/dependency changes, regenerate docs with `build-doc`.
- Before committing code changes, verify size impact with `code-size`.

## PRs and Follow-ups

- Before opening or updating a PR, follow Build and Validate; use `pre-pr` when workflows are available.
- After opening a PR, ask the human to authorize one headless `chief` invocation to drive reviews and CI to green under agentrc chief's Authorization exception. Name the PR URL, head repository and branch, and actions: push commits to that branch, post replies to the PR's review feedback, and resolve review threads through `pr-babysit` with `autoPush: true`. On an affirmative answer, launch `CLAUDE_CODE_PRINT_BG_WAIT_CEILING_MS=0 claude -p --agent chief "$task"` in the PR worktree. The task names the PR, head repository and branch, expected HEAD and scope, and includes the question and answer verbatim. Leave the checkout to chief until it exits; a new chief invocation requires a fresh exchange. TinyUSB `pr-babysit` args:
  `{"pr": <num>, "protected": "^test/hil/[^/]+\\.json$"}`; reviewers are agentrc's default.
  (`protected` excludes the HIL rig rosters from automated fixes).
- The follow-up label is `followup`.
