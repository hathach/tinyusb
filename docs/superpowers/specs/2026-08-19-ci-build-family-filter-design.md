# PR-scoped CI selection: promoting hil_select to tools/ci_select.py

**Date:** 2026-08-19
**Branch:** `build-filter`

## Motivation

Every PR builds every example on one board per family, on both CI providers: **74 legs /
2494 example-builds** on the GitHub Actions `cmake` job, and 129 family-legs per build system
on CircleCI (which runs cmake *and* make, plus clang/IAR). Most PRs touch one port, one class,
or one example, and a `hid_host.c` change cannot break an MSC device example on msp430.

`hil-build` is worse in a different way: it builds **1702 example-builds** (37 board-builds ×
46 examples, `--target all`) to run a test suite that needs at most **515**. The HIL example
universe is only 21 of the 46 examples in tree, and the median board needs 15 of them.

`test/hil/helper/hil_select.py` already maps a PR diff to affected boards and per-board test
lists for HIL, and already owns both mappings the build matrix needs: port-to-family, and
class-macro-to-example
(`docs/superpowers/specs/2026-07-29-hil-pr-scoped-selection-design.md`). That design listed
"scoping the non-HIL build jobs" as an explicit non-goal; this is that follow-up.

## Goal / non-goals

**Goal:** promote the selector to a repo-wide `tools/ci_select.py` whose single classification
of a diff drives **all three** CI axes from one rule table — which families to build, which
example targets to build on each, and which rig boards run which tests — wired into
`ci_set_matrix.py` and `hil_ci_set_matrix.py` so both providers and the rig filter from one
source. Scoping applies to `pull_request` events only; push, release and `workflow_dispatch`
keep the full matrix.

**Non-goals:**
- Variant-level or board-level selection below one-board-per-family on the build axis (all
  variants of a selected HIL board still build and run).
- Changing `hil_test.py` behaviour. The selector only *composes* existing `-b` / `-bt` args.
- Changing which tests HIL decides to run. The HIL board/test decision is preserved except for
  the single rule-7 change called out below.

## The rule table

One classification, three outputs. Every rule yields build families, build examples, and HIL
boards/tests. Pairs are unioned **per family** (build) and **per board** (HIL), so a mixed diff
never inflates one axis with another's breadth.

`DEV` = 33 `examples/device/*`, `HOST` = 9, `DUAL` = 3, `TYPEC` = 1, `ALL` = 46.
`FAM` = the families whose `family.cmake` references the changed path (CMake only — see below).
"roster boards" = boards on `test/hil/{tinyusb,hfp}.json`.

| #   | Changed path                                                                                                                                                                                                                                                                                                          | Build families                                                            | Build examples                                                    | HIL boards → tests                                                                                                               |
| --- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------- | ----------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------- |
| 1   | `docs/`, `.claude/`, `*.md`, `*.rst`, `LICENSE`                                                                                                                                                                                                                                                                       | —                                                                         | —                                                                 | —                                                                                                                                |
| 1b  | `.gitignore`, `.clang-format`, `.idea/**`, `test/{fuzz,unit-test}/**`, `test/hil/test/**`, non-build `.github/**`, packaging manifests                                                                                                                                                                                                    | —                                                                         | —                                                                 | —                                                                                                                                |
| 2   | `test/hil/**` (not `test/hil/test/**`)                                                                                                                                                                                                                                                                                                         | —                                                                         | —                                                                 | all boards → all tests                                                                                                           |
| 2b  | `tools/metrics.py`, `.github/scripts/metrics_*.py`                                                                                                                                                                                                                                                                    | `ALL` (unchanged — `tinyusb_metrics` runs `metrics.py` as a build target) | `ALL`                                                             | — (nothing on the rig runs it)                                                                                                   |
| 3   | `src/portable/<port>/dcd_*`, `*_device.[ch]`                                                                                                                                                                                                                                                                          | `FAM`                                                                     | `DEV`+`DUAL`                                                      | `FAM`'s device-role boards → device+dual tests                                                                                   |
| 4   | `src/portable/<port>/hcd_*`, `*_host.[ch]`                                                                                                                                                                                                                                                                            | `FAM`                                                                     | `HOST`+`DUAL`                                                     | `FAM`'s host-role boards → host+dual tests                                                                                       |
| 5   | `src/portable/<port>/**` (anything else)                                                                                                                                                                                                                                                                              | `FAM`                                                                     | `ALL`                                                             | `FAM`'s boards → all their tests                                                                                                 |
| 5b  | `src/portable/<port>/**` where `FAM` is empty                                                                                                                                                                                                                                                                         | —                                                                         | —                                                                 | — (empty resolves to nothing on BOTH axes)                                                                                       |
| 6   | `hw/bsp/<family>/**`                                                                                                                                                                                                                                                                                                  | that family                                                               | `ALL`                                                             | that family's boards → all tests (a `boards/<board>/` path narrows to that board)                                                |
| 7   | `hw/mcu/<vendor>/**`                                                                                                                                                                                                                                                                                                  | `FAM` — empty resolves to nothing (maintainer ruling)                     | `ALL`                                                             | `FAM`'s boards → all tests; empty resolves to nothing (maintainer ruling)  ⚠ *see below*                                         |
| 8   | `src/class/<cls>/*_device.[ch]`                                                                                                                                                                                                                                                                                       | `ALL`                                                                     | examples enabling `CFG_TUD_<CLS>`                                 | device-role boards → HIL tests enabling `CFG_TUD_<CLS>`                                                                          |
| 9   | `src/class/<cls>/*_host.[ch]`                                                                                                                                                                                                                                                                                         | `ALL`                                                                     | examples enabling `CFG_TUH_<CLS>`                                 | host-role boards → HIL tests enabling `CFG_TUH_<CLS>`                                                                            |
| 10  | `src/class/<cls>/**` (shared header)                                                                                                                                                                                                                                                                                  | `ALL`                                                                     | either, **plus include-edge classes**                             | both roles → same, plus include-edge classes                                                                                     |
| 11  | `src/device/**`                                                                                                                                                                                                                                                                                                       | `ALL`                                                                     | `DEV`+`DUAL`                                                      | device-role boards → device+dual tests                                                                                           |
| 12  | `src/host/**`                                                                                                                                                                                                                                                                                                         | `ALL`                                                                     | `HOST`+`DUAL`                                                     | host-role boards → host+dual tests                                                                                               |
| 12b | `src/typec/**`                                                                                                                                                                                                                                                                                                        | `ALL`                                                                     | examples enabling `CFG_TUC_ENABLED`                               | — (no rig board runs a typec test)                                                                                               |
| 13  | `examples/<role>/<name>/**`                                                                                                                                                                                                                                                                                           | `ALL`                                                                     | just `<name>`                                                     | if `<name>` is a HIL test: all boards → that test; else nothing                                                                  |
| 14  | `examples/device/board_test/**`                                                                                                                                                                                                                                                                                       | `ALL`                                                                     | just `board_test`                                                 | all boards → all tests (HIL parking firmware)                                                                                    |
| 15  | `examples/build_system/**`, `examples/CMakeLists.txt`, `examples/<role>/CMakeLists.txt`                                                                                                                                                                                                                               | `ALL`                                                                     | `ALL`                                                             | all boards → all tests                                                                                                           |
| 16  | `src/common/`, `src/osal/`, `src/tusb.[ch]`, `src/tusb_option.h`, `tools/{build,build_utils,ci_select}.py`, `tools/cmake/**`, `src/CMakeLists.txt`, `src/tinyusb.mk`, `hw/bsp/{family_support.{cmake,mk},family_rules.mk,zephyr_board_aliases.cmake,board.c,board_api.h,ansi_escape.h}`, `.github/**`, `.circleci/**` | `ALL`                                                                     | `ALL`                                                             | all boards → all tests                                                                                                           |
| 16a | `lib/<name>/**`                                                                                                                                                                                                                                                                                                       | `ALL`                                                                     | examples whose own `CMakeLists.txt`/`Makefile` names `lib/<name>` | those examples that are HIL tests, on all boards; empty resolves to nothing                                                      |
| 16b | `tools/get_deps.py`                                                                                                                                                                                                                                                                                                   | families whose `deps_mandatory`/`deps_optional` entries changed           | `ALL`                                                             | those families' boards → all tests; a logic change, an `'all'` entry, no base content or a changed token naming no family → full |
| 17  | anything unclassified (no tracked file reaches this — TestNoTrackedFileIsUnclassified)                                                                                                                                                                                                                                | `ALL`                                                                     | `ALL`                                                             | all boards → all tests (fail-open)                                                                                               |

**Rule 2 is deliberately asymmetric.** A `test/hil/**` change is invisible to the family matrix
but is exactly what the rig exercises, so it builds nothing and runs everything.

`test/hil/test/**` is carved out to rule 1b: it holds the harness's own unit tests, which
nothing on the rig runs (pre-commit does, and `build.yml` runs `test_ci_select.py` as the
gate before trusting a selection). A bare `test/hil/` prefix was booking the full 27-board
rig for diffs that cannot reach it. The carve-out is a claim about that directory's
contents, so a test pins its file list: add anything the rig reads and it fails.

**Rule 7 is the one HIL-side behaviour change in this design.** Today `hw/mcu/` sits in
`hil_select`'s `_FULL_RE` and forces the full HIL matrix. Since the build axis now resolves
those paths to a family through the same scan, forcing full on the rig is inconsistent. The
path fires rarely — 4 commits in 3 years — so this is low-risk either way; if you would rather
keep the HIL view untouched, rule 7's HIL column becomes "all boards → all tests" and nothing
else in this design changes.

Rules 8–10 reuse machinery `hil_select` already has — `class_macros`, `_config_enables`,
`class_include_edges` — applied over all 46 examples' `src/tusb_config.h` for the build axis
and over the HIL test list for the HIL axis. The include edges are why an `audio.h` change also
selects the MIDI examples (`midi{,2}_{device,host}.h` include `class/audio/audio.h`) and a
`cdc.h` change the net one.

### Buildability post-filter (build axis)

After the pairs are unioned, every `(family, examples)` pair is pruned with
`build_utils.skip_example(example, <family's first board>)` — the same `skip.txt` / `only.txt`
data CMake's `family_filter` uses (40 `skip.txt`, 13 `only.txt` in tree). Examples the family
cannot build are dropped; a family left with none is dropped entirely.

This is where most of the host-side saving comes from: only 23 of 75 CI families can build
`host/bare_api` at all, and 2 can build `typec/power_delivery`.

### Measured effect

GHA `cmake` job, baseline **74 legs / 2494 example-builds**; `hil-build`, baseline **1702
example-builds** across 37 board-builds.

| PR shape                     | Build legs | Build ex-builds | HIL boards | hil-build ex-builds |
| ---------------------------- | ---------: | --------------: | ---------: | ------------------: |
| `dcd_rp2040.c`               |          1 |              35 |          2 |                  32 |
| `hcd_max3421.c`              |          1 |              10 |          7 |                  36 |
| `hw/bsp/stm32f4/**`          |          1 |              45 |          1 |                  15 |
| `dcd_dwc2.c`                 |         20 |             646 |         10 |                 184 |
| `hid_host.c`                 |         24 |              68 |          — |                   — |
| `msc_host.c`                 |          — |               — |          9 |                  40 |
| `usbh.c`                     |         25 |             217 |         10 |                  57 |
| `msc_device.c`               |         74 |             350 |          — |                   — |
| `cdc_device.c`               |         74 |             588 |         27 |                 192 |
| `examples/device/cdc_msc/**` |         73 |              73 |         25 |                  60 |
| `usbd.c`                     |         74 |            2297 |         27 |                 472 |
| `src/common/**` (full)       |         74 |            2494 |         30 |                 515 |
| `test/hil/**` only           |          0 |               0 |         30 |                 515 |

The full-matrix row is the headline for `hil-build`: even with **no** PR narrowing, per-board
example selection takes it from 1702 to 515.

### Why "empty means empty"

Both views answer an empty `FAM` the same way (rule 5b): nothing. The HIL view used to force
the full 30-board rig there, on the theory that an empty result might be a scan miss — but the
build view answered the identical condition with zero families for the same path, so the rig
ran every board to validate a file that nothing compiled. In the build view that theory costs
74 legs, and the
evidence does not support it: of the 28 `src/portable/*/*` directories, **26 resolve to at
least one family**. The two that do not are both real orphans as far as CI is concerned:
`microchip/pic` (only `dcd_pic.c` and a README, with no `hw/bsp/pic` family at all) and
`microchip/pic32mz` (`hw/bsp/pic32mz` has only a `family.mk`, and `pic32mz` is in neither
provider's family list, so no CI job builds it today). A file no CI job compiles cannot be
validated by building anything.

The safety this gives up is recovered structurally: a unit test asserts every
`src/portable/*/*` and every tracked `hw/mcu/<vendor>` resolves to ≥1 family, with an explicit
allowlist of known orphans (`microchip/pic`, `microchip/pic32mz`). Adding a port without
wiring a family then fails
pre-commit instead of silently building nothing on every later PR. Same enforcement style as
the existing `test_hil_util.BottomLayer` structural tests.

Fail-open survives where it belongs: an *unclassified* path or any exception widens to `ALL` on
every axis.

### A class no example enables selects nothing

`src/class/bth` is the live instance: no example's `tusb_config.h` sets `CFG_TUD_BTH`, so
rules 8-10 resolve to no examples and a bth-only PR builds nothing and runs nothing. That is
the empty-means-empty ruling applied to classes, and it is deliberate — nothing compiles the
file, so nothing can validate it, and the master-push build is the net.

Worth stating plainly because the exposure changed: GHA used to rebuild everything for such
a PR by accident, through the empty-`families` bug in `build.yml`. With that fixed, both
providers now correctly build nothing, so `tud_bt_*` can be broken by a green PR.
`TestClassesWithNoEnablingExample` pins the set to `{bth}` so a second class cannot enter
this state unnoticed.

### Why `hw/mcu/**` is rule 7 and not "full"

`hw/mcu` is overwhelmingly dependency territory — `tools/get_deps.py` has 87 entries under it,
and those paths are gitignored, so they can never appear in a diff. Only 51 files survive
in-tree, touched 4 times in 3 years, and they resolve through the same scan the ports use:

| Tracked directory             | In `get_deps`?                              | Resolves to |
| ----------------------------- | ------------------------------------------- | ----------- |
| `hw/mcu/dialog/` (`da1469x`)  | **no** — real in-repo MCU support, 21 files | `da1469x`   |
| `hw/mcu/nordic/` (`nrf5x`)    | beside the `nrfx` dep                       | `nrf`       |
| `hw/mcu/sony/` (`cxd56`)      | beside the `spresense-exported-sdk` dep     | `cxd56`     |
| `hw/mcu/bridgetek/` (`ft9xx`) | beside the `ft90x-sdk` dep                  | `ft9xx`     |

Rule 7 is therefore not a mechanism of its own — it is rules 3–5's scan pointed at a second
tree, because `src/portable/<port>` and `hw/mcu/<vendor>` ask the same question.

Unlike the port rule, an `hw/mcu` path that resolves to no family contributes *nothing* on
either axis (maintainer ruling): if no family's build references it, no build compiles it. The
table above is kept honest by `test_tracked_mcu_vendors_resolve`, which fails pre-commit if a
tracked vendor directory stops resolving.

### Why `lib/**` is rule 16a and scanned per example

Its tracked contents (`SEGGER_RTT`, `networking`, `rt-thread`, `embedded-cli`, 22 commits in
3 years) are wired in at `examples/build_system` and per-example `CMakeLists.txt`, not per
family — so the family scan the ports use is the wrong instrument here: it would *wrongly*
narrow `SEGGER_RTT` to the three families that name the path in their `family.cmake`, while
the path is not compiled by any of them by default (it is reached only through `LOGGER=rtt`).
That scan stays applied to `src/portable/` and `hw/mcu/` only.

Rule 16a asks the per-example question instead (maintainer ruling: only the examples that use
the lib need building): `lib_examples()` reads each example's own `CMakeLists.txt` and
`Makefile` and keeps the ones naming `lib/<name>` at a directory boundary. Every family stays
in play — any of them can build those examples — while the example list collapses:

| Tracked lib    | Examples that build it                                      | HIL tests among them |
| -------------- | ----------------------------------------------------------- | -------------------- |
| `embedded-cli` | `host/msc_file_explorer`, `host/msc_file_explorer_freertos` | both                 |
| `networking`   | `device/net_lwip_webserver`                                 | none (test disabled) |
| `SEGGER_RTT`   | —                                                           | —                    |
| `rt-thread`    | —                                                           | —                    |

`SEGGER_RTT` and `rt-thread` resolve to nothing, and "empty means empty" applies: no CI build
compiles them, so there is nothing to validate by building.

### Why `tools/get_deps.py` is rule 16b

`deps_mandatory` / `deps_optional` are data: `path -> [url, commit, 'fam1 fam2 ...']`. A commit
bump therefore affects exactly the families listed in that entry, and building the other 70+ is
pure waste. `get_deps_changed_families()` parses both sides of the file with `ast` (never
`exec` — this is PR content), diffs the two dict literals **separately**, and unions the family
tokens of every added, removed or edited entry, from **both** sides (a removed entry has only a
base side; an edited family list must cover the families that lose the dep as well as the ones
that gain it). Separately, because merging the dicts before diffing hides a *move* between
`deps_mandatory` and `deps_optional` — the value is untouched, but mandatory deps are fetched
for every family, so demoting one stops families fetching it.

It falls open to the full matrix whenever the entries are not the whole answer:

* anything outside the two dict assignments differs — a logic change to `get_deps` can change
  what every family fetches (compared as `ast.dump(..., annotate_fields=False)` of the module
  with those two assignments removed, so comments and reformatting alone are not a logic
  change);
* an `'all'` entry (every mandatory dep) changed;
* the file will not parse;
* there is no base content: `--diff-file` mode has no git, so no merge-base blob;
* a changed entry carries a family token that names no `hw/bsp/<dir>` and is not one of the
  known aliases. "Changed but unmappable" is not "nothing changed": reading it as the
  latter empties the whole build matrix for a dep bump.

The six known aliases (`sam3x`, `samd21`, `samd51`, `same5x`, `stm32l1`, `stm32l5`) are
pinned in `_DEPS_ALIAS_TOKENS` and select nothing. `get_deps` matches a token against a
requested family name verbatim (`f in entry[2].split()`), so these tokens match nothing
there either — four are pre-rename spellings listed beside the current name in the same
entry, and two name no family in the tree. (`fc100s` and `spresense` were on this list
until they were corrected in `get_deps.py`; those two were the only ones that left a
real dep unreachable for its own family.) A seventh appearing fails `TestOrphanInvariant`.

## Component: `tools/ci_select.py`

`git mv test/hil/helper/hil_select.py tools/ci_select.py` (history preserved). The HIL
classifier is unchanged apart from rule 7; a second, independent build classifier is added
beside it. One diff read, two classifiers, one unit suite.

```
python3 tools/ci_select.py --base <ref> [--diff-file <path>] [CONFIG.json ...]
```

`configs` becomes `nargs='*'`. With rosters it emits everything it emits today plus the new
keys; with none it emits only the build view, so CircleCI never needs to know HIL exists.

```json
{
  "full": false,
  "boards": {"raspberry_pi_pico": "all"},
  "families": ["rp2040"],
  "args": {"tinyusb.json": "-b raspberry_pi_pico"},
  "args_flasher": {"tinyusb.json": {"openocd": "-b raspberry_pi_pico"}},
  "hil_examples": {"raspberry_pi_pico": ["device/cdc_msc", "device/board_test"]},
  "build": {
    "full": false,
    "families": ["rp2040"],
    "family_examples": {
      "rp2040": ["device/cdc_msc", "device/hid_composite", "dual/dynamic_switch"]
    }
  },
  "reasons": ["src/portable/raspberrypi/rp2040/dcd_rp2040.c: port rp2040 -> ..."]
}
```

`build.families` is the build family axis. `build.family_examples` maps a family to its example
list; **a family absent from the map builds all its examples**, so the common "narrow families,
all examples" case carries no payload. `build.full` true means no build narrowing at all.

`hil_examples` is the new HIL build axis: per roster board, the examples `hil-build` must
produce. It is `board_tests(board)` — which the selector already computes — **plus
`device/board_test`**, which `hil_test.py` flashes to park every board at each variant boundary
and at end-of-board teardown (`hil_test.py:1798`, `:1866`). It is emitted even when
`full: true`, because the HIL example universe is 21 of 46 examples regardless of any diff.

All pre-existing keys keep their exact meaning, so `.github/scripts/hil_ci_set_matrix.py`, the
HIL legs in `build.yml` and `.claude/skills/pre-pr/SKILL.md` need only a path update.

### Shared helper change

`port_families(port_dir, repo_root)` generalizes to a path-to-families reference scan over a
second tree (`hw/mcu/`). Its existing **CMake-only** behaviour is kept unchanged and is now the
rule for every axis: it scans `hw/bsp/*/family.cmake` plus the espressif component
`CMakeLists.txt`, and never `family.mk`.

CMake is the first-class build system; Make follows whatever CMake decides. A family that
CMake does not wire up to a port is not a consumer of that port, and the Make legs on CircleCI
build the same families CMake does. Scanning `family.mk` as well would only ever *widen* the
selection to families CMake never builds, which is coverage nobody asked for — and it would
resolve `microchip/pic32mz` to a family that appears in no CI family list.

One consequence to keep in view: because HIL and build now share one scan, there is no
per-caller flag, no second cache key, and no way for the two axes to disagree about which
families own a port.

**Boundary matching.** A directory reference must match at a directory boundary — a trailing
`/` *or* end-of-token — not as a bare substring. Both traps are live: `hw/bsp/nrf/family.cmake`
writes `${TOP}/hw/mcu/nordic/nrf5x` with no trailing slash, while the existing port scan
requires a trailing `/` precisely to stop `microchip/pic` matching `microchip/pic32mz`.

### Move fallout

All mechanical, all one-line:

| File                               | Change                                                            |
| ---------------------------------- | ----------------------------------------------------------------- |
| `tools/ci_select.py`               | `sys.path` walk 4 levels → 2                                      |
| `test/hil/hil_ci.sh`               | drop from the scp list (nothing on the rig imports it)            |
| `test/hil/test/test_hil_select.py` | rename to `test_ci_select.py`, import path                        |
| `test/hil/test/test_hil_util.py`   | `BottomLayer` stdlib-closure allowlist + module list              |
| `.pre-commit-config.yaml`          | both hooks (`hil-select-test` → `ci-select-test`, `files:` globs) |
| `.github/workflows/build.yml`      | selector path, step name                                          |
| `.claude/skills/pre-pr/SKILL.md`   | selector path                                                     |

The test file stays in `test/hil/test/` — it still consumes the rig rosters and `hil_util`.

The selector gains one non-stdlib-but-local import: `tools/build_utils.skip_example` for the
buildability post-filter. `build_utils` imports only `subprocess`, `pathlib` and `re`, so the
stdlib closure the bare GitHub runner depends on is preserved; `BottomLayer` must be extended
to cover it.

Because a wrong parents-count already broke this module once (there is a comment in the source
recording it), the moved module gets a guard test asserting its derived repo root contains
`src/` and `hw/bsp/`.

## Component: `tools/build.py --example`

`build.py` has no example filter today. `-T/--target` exists and maps to
`cmake --build --target <name>`, but it hard-fails on a target that does not exist, and absent
targets are routine (40 `skip.txt`, 13 `only.txt`).

New repeatable `-e/--example <role>/<name>`:

- Default (none given) keeps today's behaviour exactly: `--target all`.
- Given, each board's list is intersected with `build_utils.skip_example(example, board)`, then
  passed as one `--target <name>` per example. Example target names are the directory names and
  are unique across all four roles (verified: 46 examples, zero collisions).
- A board whose intersection is empty is reported **skipped**, not failed.
- `--target tinyusb_metrics` must stay last so metrics run after the examples that feed them.
- The espressif path already builds per example via `get_examples` + `skip_example`; it takes
  the same filter.

Both the family matrix and `hil-build` use this one flag.

## CI wiring

### `.github/scripts/ci_set_matrix.py`

Two mutually exclusive optional flags. **Output shape is unchanged** — `{toolchain: [family]}`,
just fewer families. With no flags the output is byte-for-byte today's, so push, release and
`workflow_dispatch` are untouched.

| Flag            | Caller   | Behaviour                                                |
| --------------- | -------- | -------------------------------------------------------- |
| `--select JSON` | GHA      | consumes the selector JSON the workflow already computes |
| `--base REF`    | CircleCI | runs `tools/ci_select.py` itself                         |

`build.full` true, or any exception, prints the full matrix with a warning on stderr.

### `.github/scripts/hil_ci_set_matrix.py`

Already takes `--select` and already scopes boards. It additionally appends `-e <example>` per
board from `hil_examples`, so each `hil-build` entry builds only what its board will run plus
`board_test`. When `hil_examples` is absent (hand-runs), it falls back to today's `--target all`.

### The example map is a side channel, not a matrix entry

The build example list deliberately does **not** ride inside the family matrix entry string. On
CircleCI the `family` parameter is also passed to `python tools/get_deps.py
<< parameters.family >>` and tested with `if [ << parameters.family >> == "rp2040" ]` — a value
carrying `-e` flags breaks both — and CircleCI matrix parameters form a cartesian product, so a
parallel `example-args` parameter would multiply the jobs rather than zip with them.

So `build.family_examples` travels as one JSON blob and each build job resolves its own entry:

- **GHA:** `set-matrix` exposes it as an output; `build_util.yml` gains an optional
  `example-map` input (default `''`); a step resolves `-e` flags for `matrix.arg` with `jq`.
- **CircleCI:** `set-matrix` writes `example_map.json` and `persist_to_workspace`s it; the
  `build` job gains `attach_workspace` and resolves the same way.

Consequences of keeping the matrix shape: the metrics artifact name stays `metrics-<family>`,
and CircleCI's generated `config2.yml` does not inflate to one entry per family. `hil-build`
needs none of this — its matrix entries are already compound per-board strings from
`hil_ci_set_matrix.py`, so `-e` flags go straight in.

### `.github/workflows/build.yml`

The existing `HIL selection (PR only)` step in `set-matrix` is already gated on
`pull_request` — exactly the gate wanted. It is renamed, repointed at `tools/ci_select.py`, and
its `select` output is threaded into `ci_set_matrix.py --select`, so the filter costs zero extra
selector invocations.

`build_util.yml`'s `if: inputs.build-args != '[]'` already skips a toolchain leg whose list is
empty, and a partially-skipped matrix aggregating to success is the documented pattern
`hil-build` already relies on. When every leg is empty (a `test/hil`-only PR), the `cmake` job
has nothing to build. Accepted: GitHub treats a skipped job as satisfying a required status
check, and HIL is unaffected because `hil-build` is a separate matrix. `code-metrics` still
runs (`!cancelled()` plus `cmake` success-or-skipped) and posts a "built no families on this
push" marker, so the sticky size comment never shows a stale table from an earlier push.

### `.circleci/config.yml`

The `set-matrix` job passes `--base origin/master` when `CIRCLE_PULL_REQUEST` is set, after
`git fetch --no-tags origin master || true`; unfiltered otherwise. CircleCI does not expose the
PR base branch, so `master` is assumed — true for essentially every tinyusb PR, and any ref or
clone problem falls back to the full matrix.

Two fixes the GHA side does not need:

- `gen_build_entry` must **skip** a toolchain whose family list is `[]`. An empty matrix
  parameter is a hard CircleCI config error, not a skipped job.
- `BUILD_ALIASES` must collect only aliases that were actually generated, or `code-metrics`'
  `requires:` names a job that does not exist.

## Code metrics

`tools/metrics.py` averages per-file sizes across every build, and the per-family
`metrics-<family>` artifact stores only that average — over whichever examples were built. Both
build axes therefore break the comparison: a 3-family PR against master's 64-family average,
and an 11-example average against master's 46-example one.

The fix is to make the artifact carry per-example detail and compare the intersection.

1. **`metrics.py combine --by-example`** additionally writes `metrics_by_example.json`,
   `{example: {files: [...]}}`. The example name is the map.json's parent directory
   (`<build>/<role>/<example>/*.map.json`).
2. `examples/CMakeLists.txt`'s `tinyusb_metrics` target emits both files; `build_util.yml`
   uploads both under the existing `metrics-<family>` artifact name.
3. `combine` learns to expand a by-example JSON into one data entry per example and an
   `--only-examples` filter, so a subset can be averaged on demand.
4. `code-metrics` computes the **intersection of `(family, example)` pairs present on both
   sides**, averages each side over exactly those pairs, and compares. Dropped pairs are named
   in the PR comment. An empty intersection skips the compare with an explicit note.

`search_artifacts: true` is required on the base-side download: a docs-only master push
produces no per-family artifacts — which is why `metrics-carry-forward` exists for the
aggregate — so per-family baselines may come from different master runs. That is still a valid
per-family baseline.

Today's `metrics-tinyusb` aggregate keeps being produced for the unfiltered path, releases and
`metrics-carry-forward`. The filtered path never falls back to it — that is precisely the
mismatched compare this section exists to prevent. `hil-build` uploads no metrics, so its
narrowing does not touch any of this.

For narrow PRs this is sharper than today: a `dcd_rp2040` PR's size delta stops being diluted
by a 64-family, 46-example average.

**Size check the plan must run first:** the by-example JSON is ~46× the entries of today's
average. If it proves too large as an artifact, drop per-symbol detail from the by-example file
(sizes only) — symbols are only needed in the aggregate. The plan must also verify that
`dawidd6/action-download-artifact@v11` supports `name_is_regexp`; the fallback is a
`gh run download` loop.

## Testing

Extended in `test/hil/test/test_ci_select.py` (stdlib-only, ~0.1 s, already a pre-commit hook
and already gating CI's selector step):

- One case per rule 1–17, asserting all three outputs.
- Per-family union: a mixed diff (`dcd_rp2040.c` + `cdc_device.c`) gives `rp2040` the device
  list and every other family the CDC list — not the cross product of both.
- Include edges: an `audio.h` change selects the MIDI examples; a `cdc.h` change the net one.
- `hil_examples` always contains `device/board_test` for every selected board, including when
  `full: true`, and is otherwise exactly `board_tests(board)`.
- `hil_examples` never exceeds the 21-example HIL universe.
- The scan is CMake-only: a port referenced solely from a `family.mk` (`microchip/pic32mz`)
  resolves to no family, and no `family.mk` is ever read.
- Boundary matching: `microchip/pic` does not inherit `microchip/pic32mz`'s families, and
  `hw/mcu/nordic/nrf5x` resolves despite having no trailing slash at its reference site.
- Buildability post-filter: `typec/power_delivery` prunes to 2 families, `host/bare_api` to 23.
- Structural invariant: every `src/portable/*/*` and every tracked `hw/mcu/<vendor>` resolves
  to ≥1 family, allowlist `{microchip/pic, microchip/pic32mz}`.
- Every name in `build.families` is a real `hw/bsp/<dir>`; every example name on either axis is
  a real `examples/<role>/<name>` directory.
- Repo-root guard for the moved module.
- `ci_set_matrix.py`: no flags → byte-identical to today; `--select` with `build.full` →
  identical; `--select` narrow → a subset; malformed `--select` → full plus a warning.
- `hil_ci_set_matrix.py`: no `hil_examples` → today's args byte-for-byte; with it → `-e` flags
  appended per board, `board_test` always present.
- `build.py`: `-e` with an example the board skips builds nothing and reports skipped, not
  failed; no `-e` still passes `--target all`.

## Known gaps

- **CircleCI size comparison.** CircleCI stores only the combined `metrics.json`, so the
  intersection compare is unavailable there; when filtered it prints a note and copies
  `metrics.md`. Its `metrics_compare.md` is a stored artifact that nothing reads in review — the
  PR comment comes from GHA. Making CircleCI store per-example metrics is a follow-up.
- **HIL re-run attempts.** A re-run spec is a subset of the original selection, so the
  firmware `hil-build` produced already covers it. This holds only while re-run specs stay
  subsets; a future "re-run with extra tests" feature would need `hil-build` re-run too.
- **Membrowse** receives rows for fewer families and fewer examples on filtered PRs. If that
  service misbehaves, the escape hatch is keeping the membrowse upload leg unfiltered.
- **`typec/power_delivery`** is reached only through rules 5 and 13 (`src/portable/st/typec`
  has neither a `dcd_` nor an `hcd_` prefix, so it selects `ALL` examples on its 5 families,
  which the post-filter then prunes to 2). A dedicated typec rule is possible later; the
  post-filter already makes it cheap.
- **A `test/hil`-only PR reports `cmake` as skipped** rather than passing. Accepted;
  revertible with a one-family floor if branch protection turns out to disagree.
  `code-metrics` still runs in that case: with no `cmake-build/*/metrics.json` to
  aggregate it writes `_Code-size comparison skipped: PR selection built no families
  on this push._` and posts that as the sticky comment, so the size section reflects
  THIS push instead of keeping the previous one's table.
- **`microchip/pic32mz` builds nothing.** The scan is CMake-only and `hw/bsp/pic32mz` ships
  only a `family.mk`, so a change there selects no family. That matches reality — `pic32mz` is
  in neither provider's family list — but it means the port is unbuilt by CI whether or not
  this design lands. Giving it a `family.cmake` is the fix, and is out of scope here.
- **Seven bsp families are in no CI toolchain today** (`espressif`, `efm32`, `same7x`,
  `cxd56`, `f1c100s`, `pic32mz`, `py32f0`); the intersection drops them, matching current
  behaviour. This change does not alter that.
