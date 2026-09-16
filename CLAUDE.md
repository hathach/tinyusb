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
- From [agentrc](https://github.com/hathach/agentrc): skills `read-doc`, `simplify-gate`, `usb-sniffer`, `usb-kernel-debug`, `rtt`, `etm-trace`, `target-debug`, `esp-target-debug`; agents `chief`, `pvs-studio`, `code-writer`, `code-verifier`, `finding-verifier`, `pr-ci-watcher`, `pr-review-validator`; workflows `code-audit`, `pr-babysit`. When one is unavailable, skip the step that needs it.
- Driver audit: the `code-audit` workflow with `dirs` (e.g. `src/portable/<vendor>/<driver>`) and `dimensions`: `correctness: transfer state machines, endpoint bookkeeping, completion and error paths`; `ISR safety: work deferred to task context, shared-state races, register access ordering`; `register use vs datasheet and MCU errata: cross-check the reference manual AND errata sheets via the read-doc skill; if the skill is unavailable treat the document as absent (low confidence, never a web/filesystem substitute); a missing erratum workaround is a finding`; `style: repo conventions (TU_ASSERT, no dynamic allocation, include order, naming)`.
- Static analysis: the `pvs-studio` agent with rules `.PVS-Studio/.pvsconfig` (never add suppressions) on a board's examples build; `raspberry_pi_pico` mirrors CI, `stm32f407disco` is fastest.

## Claude and Codex Collaboration

- Keep project workflows in `.claude/workflows/`, nested one level at most.

## Build and Validate

- Build contract: `.claude/skills/build/SKILL.md`. Its script resolves a change to boards and builds them; `--shared` writes `cmake-build/cmake-build-<board>`, the dir HIL flashes from, so preserve it. Flash with `ninja -C cmake-build/cmake-build-<board> <example>-jlink` or `-openocd`.
- ESP-IDF: `. "$IDF_PATH/export.sh"` before anything Espressif; verification still goes through the build contract, with `idf.py -DBOARD=<board> flash monitor` in the example reserved for interactive flash and monitor.
- Before submitting: `pre-commit run --all-files` (includes unit tests).
- For code changes: build the full example set for boards that exercise the changed modules. Add fuzz/HIL coverage for parsers or protocol state machines.
- After board/dependency changes, regenerate docs with `build-doc`.
- Before committing code changes, verify size impact with `code-size`.

## PRs and Follow-ups

- Before opening or updating a PR, follow Build and Validate; use `pre-pr` when workflows are available.
- After opening a PR, use `chief` to drive reviews and CI to green. TinyUSB `pr-babysit` args:
  `{"pr": <num>, "reviewers": ["codex","copilot","coderabbit"], "autoRun": ["codex","copilot","coderabbit"], "protected": "^test/hil/[^/]+\\.json$"}`
  (`protected` excludes the HIL rig rosters from automated fixes).
- The follow-up label is `followup`.
