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
- From [agentrc](https://github.com/hathach/agentrc): skills `read-doc`, `simplify-gate`, `usb-sniffer`, `usb-kernel-debug`; agents `pvs-studio`, `code-writer`, `code-verifier`, `finding-verifier`, `pr-ci-watcher`, `pr-review-validator`. When one is unavailable, skip the step that needs it.
- Static analysis: the `pvs-studio` agent with rules `.PVS-Studio/.pvsconfig` (never add suppressions) on a board's examples build; `raspberry_pi_pico` mirrors CI, `stm32f407disco` is fastest.

## Claude and Codex Collaboration

- Keep orchestration in `.claude/workflows/`. Use `code-verify` with `provider: 'codex'` (default; reviews with Claude when Codex cannot run, but not after a Codex timeout), `'claude'`, or `'all'`; `validate`/`full-check` use `reviewProvider`; `pr-babysit` has Codex challenge every dismissal before it is posted (Claude when Codex cannot run, so the challenge is then not independent). Keep workflow nesting to one level.
- Delegate Codex workflow jobs through `.claude/agents/codex-agent.md`; `.claude/codex-agent.py` runs `codex exec --sandbox read-only` (`codex exec review` for `validate`'s diff review), keeps each job under `/tmp/tinyusb-codex/<job>/` and returns `{job, thread, status, result}` (`status` is `ok` or `timeout`): `tail -f` the job's `events.jsonl` to watch, `kill` the `codex exec` pid to stop, `codex exec resume <thread>` to ask a follow-up. Its read-only sandbox has no network: jobs must use local evidence, since failed network reads can produce false empty results. Write-capable roles need worktree isolation first.

## Build and Validate

Build all examples for a board from the repo root; preserve `cmake-build-<board>` for HIL:

```bash
cmake -S examples -B examples/cmake-build-adafruit_metro_rp2350 -DBOARD=adafruit_metro_rp2350 -G Ninja -DCMAKE_BUILD_TYPE=MinSizeRel
cmake --build examples/cmake-build-adafruit_metro_rp2350
ninja -C examples/cmake-build-adafruit_metro_rp2350 cdc_msc-jlink    # Flash with J-Link
ninja -C examples/cmake-build-adafruit_metro_rp2350 cdc_msc-openocd  # Or OpenOCD
```

- Single example after configuring above: `cmake --build examples/cmake-build-adafruit_metro_rp2350 --target cdc_msc`.
- ESP-IDF: `. "$IDF_PATH/export.sh"` before build/flash/monitor; run `idf.py -DBOARD=<board> build` in the ESP-IDF example.
- Debug/logging: `-DCMAKE_BUILD_TYPE=Debug -DLOG=2 -DLOGGER=rtt`.
- Before submitting: `pre-commit run --all-files` (includes unit tests).
- For code changes: build the full example set for boards that exercise the changed modules. Add fuzz/HIL coverage for parsers or protocol state machines.
- After board/dependency changes, regenerate docs with `build-doc`.
- Before committing code changes, verify size impact with `code-size`.

## PRs and Follow-ups

- Before opening or updating a PR, follow Build and Validate; use `pre-pr` when workflows are available.
- After opening a PR, use `pr-babysit` (`.claude/workflows/pr-babysit.js`) to drive reviews and CI to green. If workflows are unavailable, use `gh pr checks <num>` and `gh pr view <num> --comments`; fix failures, push, and resolve review threads.
- The follow-up label is `followup`.
