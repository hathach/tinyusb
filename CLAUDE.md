# TinyUSB Agent Instructions

## Working Rules

Bias toward caution over speed. For trivial tasks, use judgment.

- Think first — state assumptions; ask if unclear; when a choice matters, name it and recommend one rather than picking silently or surveying every option.
- Simplicity — Follow YAGNI. Reuse existing code, standard-library, and native-platform features before adding dependencies or abstractions. Prefer the smallest clear solution, but never sacrifice correctness, safety, or necessary tests.
- Surgical changes — touch only what the task requires; match existing style; don't refactor working code; mention unrelated dead code rather than deleting it. Remove only orphans your changes created.
- Goal-driven — turn tasks into verifiable goals ("write failing test, make it pass"). For multi-step work, state a brief `step → verify` plan.
- Assume the dev machine is configured. Run commands directly; troubleshoot setup only when a command fails.
- **Worktrees** — For branch or multi-step work: `git worktree add .worktrees/<branch> -b <branch>`; never switch the primary checkout. Symlink `deps_all` paths from `tools/get_deps.py` to the primary checkout; replace a symlink and rerun `get_deps.py` only for a different revision.
- Hardware references — before register/bitfield/pinout/errata/timing claims or DCD/HCD changes, use `read-doc` to check Calibre first; report missing documents. Search its database, never the library tree.
- Open-source behavior — when investigating an issue or understanding behavior (e.g. kernel, libusb, OpenOCD; usbfs, usbtest, sysfs attributes, device locks, D state), read the source for the version in use rather than infer from symptoms.
- Code references — boards: `hw/bsp/`, `docs/reference/boards.rst`; classes: `src/class/`; core/config: `src/tusb.h`, `src/tusb_option.h`, each example's `src/tusb_config.h`; build/deps: `tools/build.py`, `tools/get_deps.py`; unit tests: `test/unit-test/project.yml`.

## Code Rules

- C99, 2-space indent, no tabs; `snake_case` helpers, `UPPER_CASE` macros, `tud_`/`tuh_` public APIs, `TU_` macros.
- No dynamic allocation. Defer ISR work to task context. Use `TU_ASSERT()` for error checks; check return values.
- Keep headers self-contained with `#if CFG_TUSB_MCU` guards. Include order: C stdlib → tusb common → drivers → classes.

## Claude and Codex Collaboration

- Keep `CLAUDE.md` and `.claude/{agents,skills,workflows}` canonical; preserve `AGENTS.md -> CLAUDE.md` and `.agents -> .claude`.
- Use `.codex/agents/<role>.toml` to load `.claude/agents/<role>.md`; keep adapters thin and never duplicate role bodies.
- Use `/codex:review` for independent read-only review, `/codex:adversarial-review` to challenge a design, and `/codex:rescue` for bounded implementation or diagnosis.
- Concurrent writers need separate worktrees; otherwise yield the worktree until delegated edits finish.
- Keep orchestration in `.claude/workflows/`. Use `code-verify` with `provider: 'codex'` (default), `'claude'`, or `'both'`; `validate`/`full-check` use `reviewProvider`. Keep workflow nesting to one level and the Codex subprocess in `.claude/agents/codex-code-verifier.md`.

## Build and Validate

Build all examples for a board from the repo root; preserve `cmake-build-<board>` for HIL:

```bash
cmake -S examples -B examples/cmake-build-stm32f407disco -DBOARD=stm32f407disco -G Ninja -DCMAKE_BUILD_TYPE=MinSizeRel
cmake --build examples/cmake-build-stm32f407disco
ninja -C examples/cmake-build-stm32f407disco cdc_msc-jlink    # Flash with J-Link
ninja -C examples/cmake-build-stm32f407disco cdc_msc-openocd  # Or OpenOCD
```

- Single example after configuring above: `cmake --build examples/cmake-build-stm32f407disco --target cdc_msc`.
- ESP-IDF: `. "$IDF_PATH/export.sh"` before build/flash/monitor; run `idf.py -DBOARD=<board> build` in the ESP-IDF example.
- Debug/logging: `-DCMAKE_BUILD_TYPE=Debug -DLOG=2 -DLOGGER=rtt`.
- Before submitting: `pre-commit run --all-files` (includes unit tests).
- For code changes: build a board's full example set. Add fuzz/HIL coverage for parsers or protocol state machines.
- Validate device runtime on hardware; a successful build alone does not establish runtime correctness.
- After board/dependency changes, regenerate docs with `build-doc`.
- Before committing code changes, verify size impact with `code-size`.

## PRs and Follow-ups

- Before opening or updating a PR, run `pre-pr`.
- Use imperative commit/PR subjects; keep scope focused, link relevant issues, and include test/build evidence.
- After opening a PR, use the `pr-babysit` workflow (`.claude/workflows/pr-babysit.js`) to drive reviews and CI to green.
- **Deferred work** — Separate scope gets a separate PR/session. Use `superpowers:writing-plans` for one handoff per topic at `docs/superpowers/followup/pr<NNN>-<topic>.md` (`NNN` = originating PR). Include evidence, remaining work, and why deferred; delete when its PR lands.
