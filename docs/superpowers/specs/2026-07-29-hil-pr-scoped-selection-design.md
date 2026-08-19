# PR-scoped HIL selection: helper/hil_select.py

**Date:** 2026-07-29
**Branch:** `claude/hil-select` (based on `claude/hil-pool-check`, which carries the
hil_lock/hil_flash split and the current rig rosters)

## Motivation

Every PR currently builds and runs the full HIL matrix (both rigs, every roster board, every
test). Most PRs touch one port or one class: a `dcd_rp2040` change cannot affect an STM32 board,
a `cdc_device.c` change cannot affect an MSC-only example, and a device-stack change cannot
affect host tests. Scoping HIL to the affected boards/tests cuts CI wall time and rig wear
without losing relevant coverage.

## Goal / non-goals

**Goal:** a shared selector that maps a PR diff to (boards, per-board test lists), wired into
CI's `set-matrix` on `pull_request` events (pruning both `hil-build` and the rig jobs) and
callable locally (pre-pr, manual runs). Scoping may only shrink coverage when the mapping is
confident; every uncertainty widens to the full matrix.

**Non-goals:**
- Variant-level selection (all variants of a selected board run).
- Scoping the non-HIL build jobs (cmake/CircleCI one-per-family builds are independent build
  coverage and stay untouched).
- Scoping push/master/scheduled runs (always full).
- Changing hil_test.py behavior (the selector only *composes* existing `-b`/`-bt` args).

## Component: `test/hil/helper/hil_select.py`

Stdlib-only, importable and CLI. Lives beside the harness so `hil_ci.sh` copies are unaffected
(it runs on the GitHub runner / dev PC, not on the rig). It must NOT import `hil_test.py`
(which drags pyserial/pymtp onto the bare GitHub runner): the three test lists
(`device_tests`, `dual_tests`, `host_test`) move verbatim into the stdlib-only
`test/hil/helper/hil_util.py` that both `hil_test.py` and `hil_select.py` import (behavior
preserving; `hil_ci.sh` copies the whole `helper/` directory).

```
python3 test/hil/helper/hil_select.py --base <ref> [--diff-file <path>] CONFIG.json [CONFIG.json...]
```

- `--base REF`: changed files = `git diff --name-only $(git merge-base HEAD REF)..HEAD`
  (mirrors pre-pr). `--diff-file`: newline-separated file list instead of git (unit tests, CI
  reuse of a precomputed diff).
- Output (stdout, JSON):

```json
{
  "full": false,
  "boards": {"raspberry_pi_pico": "all", "stm32f407disco": ["device/cdc_msc", "device/cdc_dual_ports"]},
  "args": {"tinyusb.json": "-b raspberry_pi_pico -b stm32f407disco -bt stm32f407disco:device/cdc_msc,device/cdc_dual_ports",
           "hfp.json": ""},
  "reasons": ["src/portable/raspberrypi/rp2040/dcd_rp2040.c: port rp2040 -> family rp2040 -> boards [raspberry_pi_pico, ...] (device role)"]
}
```

- `full: true` ⇒ `boards`/`args` cover the entire rosters (identical to today's behavior).
- `args` maps each input config file to the hil_test.py argument string for that rig: `-b` per
  selected board on that roster, plus `-bt BOARD:t1,t2` for boards with a restricted test list
  ("all" boards get bare `-b`). An empty string means: nothing on this rig is affected — the
  rig job is skipped for this PR.
- Per-file reasoning lines (`file → rule → contribution`) go in `reasons` and to stderr, so the
  CI log answers "why did/didn't HIL run X" without archaeology.

## Classification rules

Each changed file yields a contribution; the selection is the union. Any file matching no rule
sets `full: true` (fail-open). Rules, first match wins:

1. **Non-code:** `docs/**`, `.claude/**` (except the workflows below via rule 8), `*.md`,
   `*.rst`, `LICENSE*` → contributes nothing.
2. **Port:** `src/portable/<vendor>/<ip>/**` (or single-level `src/portable/<name>/**`).
   Role from basename: `dcd_*`/`*_device*` → device; `hcd_*`/`*_host*` → host; anything else
   (shared port files, e.g. `dwc2/dwc2_common.c`) → both. Families = directories of
   `hw/bsp/*/family.cmake|family.mk` whose text references `<vendor>/<ip>` (pre-pr's grep),
   boards = those families' entries on the input rosters. Tests = all tests of that role
   (device_tests / host_test from hil_test.py's lists; dual_tests count as both roles).
3. **Class:** `src/class/<c>/*_device.*` → all device-capable roster boards; tests = the
   device/dual examples in hil_test.py's lists whose `examples/<role>/<ex>/src/tusb_config.h`
   defines `CFG_TUD_<C>` with a nonzero value (derived at runtime; `<C>` = upper-cased class
   dir, with the map `musb→n/a`-style exceptions NOT needed — class dirs and config macros
   share names: cdc, msc, hid, midi, audio, video, vendor, usbtmc, mtp, printer. Two
   exceptions: in class dir `dfu`, `dfu_rt_device.*` maps to CFG_TUD_DFU_RUNTIME and
   `dfu_device.*` to CFG_TUD_DFU; class dir `net` maps to CFG_TUD_ECM_RNDIS|CFG_TUD_NCM.) `*_host.*` analogously via `CFG_TUH_<C>`. Shared class files (e.g. `cdc.h`) →
   both roles' matching examples. A class with zero matching examples contributes nothing
   (known path, does not force full).
4. **Core role:** `src/device/**` → all device-capable boards, all device tests (+dual);
   `src/host/**` → all host-capable boards, all host tests (+dual).
5. **Core common:** `src/common/**`, `src/osal/**`, `src/tusb.c`, `src/tusb.h`,
   `src/tusb_option.h` → full.
6. **BSP:** `hw/bsp/<family>/**` → that family's roster boards, all their tests;
   `hw/bsp/<family>/boards/<board>/**` narrows to that board if it is on a roster, and
   contributes nothing when it is not (an off-rig board cannot be HIL-tested; known path,
   does not force full).
   Family-agnostic BSP files (`hw/bsp/board_api.h`, `hw/bsp/board.c`, ansi_escape.h) → full.
7. **Example:** `examples/<role>/<ex>/**` → all roster boards, tests = that example if present
   in hil_test.py's lists, else contributes nothing. `examples/build_system/**`, top-level
   `examples/CMakeLists.txt` → full. `examples/device/board_test/**` → full: it is the park
   firmware hil_test.py flashes on every board (variant boundary + teardown), not a test.
8. **Harness/infra:** `test/hil/**`, `.github/workflows/build*.yml`,
   `.github/actions/**`, `tools/build.py`, `tools/get_deps.py`, `tools/cmake/**`,
   `hw/mcu/**`, `lib/**` → full.
9. **Everything else** (`test/unit-test/**`, `tools/**` not above, unknown paths) → full.
   (Unit-test-only changes could safely skip HIL, but per the fail-open stance anything not
   explicitly classified widens; narrowing rule 9 is a later refinement.)

**Role pruning:** after the union, if only device-role contributions exist, host-only boards
drop out and host tests are stripped from mixed boards (vice versa for host-only changes).
Dual tests survive either role. Board capability (device/host) comes from the roster entry's
`tests` flags/only-list, same logic hil_test.py uses.

**No-rig-coverage case:** a cleanly classified change whose boards intersect a roster to the
empty set yields an empty `args` string for that rig and a stderr line saying so — the rig job
is skipped, not widened (running unrelated boards would test nothing relevant).

**Roster source:** `config['boards']` only (boards-skip stays parked).

## CI wiring (`.github/workflows/build.yml`)

- `set-matrix` (PR events only): after generating today's matrices, run
  `helper/hil_select.py --base origin/${{ github.base_ref }} test/hil/tinyusb.json test/hil/hfp.json`
  (checkout with enough history to reach the merge base: `fetch-depth: 0` on this one job, or
  an explicit `git fetch origin $BASE_REF`). New job outputs: `hil_select_full`,
  `hil_args_tinyusb`, `hil_args_hfp`, plus the selected-board list consumed by the matrix
  generator. Non-PR events: skip the selector, outputs default to full/empty-args-means-all.
- `hil_ci_set_matrix.py` gains `--select '<json>'`: when given and `full` is false, it emits
  build entries only for selected boards (per config). Untouched otherwise.
- `hil-tinyusb` job (one matrixed job covering both rigs, selected by `matrix.hil_json`): a
  step picks the rig's selector args in shell (`case "$HIL_JSON" in ...`) from the set-matrix
  outputs and either appends them to the `hil_test.py` invocation or exits the step early with
  a "HIL skipped by selection" log line when that rig has nothing to run (`run` flag output
  false). The separate `hil-tinyusb-esp` job (esptool split) gets the same treatment with the
  tinyusb args. Non-PR events: outputs default to run=true with empty args (today's behavior).
- The `--flasher`/`--exclude-flasher` split in the existing matrix `test_args` composes fine
  with `-b` (hil_test.py applies both filters).

## Local use

- pre-pr's "Map changes to boards" step delegates to
  `python3 test/hil/helper/hil_select.py --base $BASE test/hil/tinyusb.json` and derives its
  one-board-per-family sample from the selector's board set (its capping/sampling policy is
  unchanged — the selector provides the affected set, pre-pr samples it).
- Manual: `python3 test/hil/hil_test.py -B examples $(python3 test/hil/helper/hil_select.py --base master test/hil/tinyusb.json | jq -r '.args["tinyusb.json"]') test/hil/tinyusb.json`
  — documented in the hil skill.

## Testing

`test/hil/test/test_hil_select.py` — stdlib `unittest`, no hardware, injected diffs via
`--diff-file`/API. Cases (the acceptance examples):
1. `src/portable/raspberrypi/rp2040/dcd_rp2040.c` → only rp2040-family roster boards, device
   tests only, host-only boards absent, `full` false.
2. `src/device/usbd.c` → every device-capable board on both rosters, all device tests + dual,
   no host-only board, no host tests.
3. `src/class/cdc/cdc_device.c` → only examples with CFG_TUD_CDC enabled (must include
   device/cdc_msc and device/cdc_dual_ports; must exclude device/msc_dual_lun and all
   host tests).
4. `src/class/msc/msc_host.c` → host-capable boards only, host examples with CFG_TUH_MSC.
5. `tools/random_new_script.py` → `full: true`.
6. `docs/foo.rst` alone → contributes nothing ⇒ empty selection, `full` false, all `args`
   empty (CI additionally has check-paths gating; the selector's answer is still honest).
7. `hw/bsp/rp2040/family.cmake` → rp2040-family boards, all their tests.
8. Mixed device+host diff → no pruning (both roles present).
The suite runs in `set-matrix` before the selector is used, and locally via
`python3 test/hil/test/test_hil_select.py`.

## Safety properties

- Fail-open: unknown/infra paths ⇒ full matrix; selector crash in CI ⇒ job fails visibly
  (never silently skips HIL).
- Only `pull_request` events are scoped.
- The selection JSON + per-file reasons are printed in the job log for audit.
- hil_test.py errors on `-b` names not in the config — the selector only emits roster names,
  and the unit suite locks that invariant.

## Sequencing

Lands on `claude/hil-select` on top of the pool-check/split stack. Follow-ups it does not
include: narrowing rule 9 for unit-test-only changes; variant-level selection; pre-pr skill
text update ships in the same change (its mapping section shrinks to a selector call).
