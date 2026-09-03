# Rework Code Metrics: membrowse as first-class size analytics

**Date:** 2026-08-25
**Branch:** rework-metrics
**Status:** Approved design

## Goal

Remove the linkermap-based metrics pipeline from CI and make membrowse the
single size-analytics system. Stop uploading every built target to membrowse;
upload a curated ("pinned") set of boards that together cover every dcd/hcd
driver. Keep linkermap as a local-only tool for the code-size skill.

## Decisions (settled during brainstorming)

1. **Membrowse replaces both** the sticky PR code-size comment and the
   release-asset `metrics.json`/compare files. The `code-metrics` job and its
   comment are deleted, not replaced in-repo.
2. **Coverage unit:** one pinned board per driver source file, plus extra
   boards where one driver diverges strongly per vendor (dwc2: STM32 + ESP32
   + riscv/aarch64 variants; ci_hs: RT + kinetis-class, etc.). ~35-40 boards.
3. **All examples** are uploaded per pinned board (today's upload shape,
   restricted to pinned boards).
4. **Pinned boards drive the cmake build matrix** — they replace
   `--one-first`'s alphabetical pick; families needing several boards for
   driver coverage get several builds.
5. **linkermap local-only:** the POST_BUILD hook is removed everywhere; the
   explicit `<target>-linkermap` target stays and local tooling invokes it.
6. **Espressif included:** membrowse upload is wired into the esp-idf docker
   lane; an espressif board is pinned for the dwc2-ESP32 variant.
7. **Config approach:** curated `.github/ci-pinned-boards.json` + a coverage
   checker that fails when a driver has no pinned board (Approach A).
8. **`membrowse-onboard.yml` is deleted** — history backfill is done locally
   via the membrowse skill (`membrowse onboard` CLI), not a GH workflow.
9. **membrowse agent skill + local diff engine:** a `membrowse` skill is
   added, and the code-size skill's base-vs-branch diff switches its default
   engine to membrowse local reports; linkermap/metrics.py stay as fallback
   until the membrowse diff is proven equivalent, then drop in a follow-up.

## 1. Pinned-target config

`.github/ci-pinned-boards.json` (revived; new schema, single source of truth):

```json
{
  "targets": [
    { "board": "stm32f407disco", "family": "stm32f4", "toolchain": "arm-gcc",
      "drivers": ["dcd_dwc2", "hcd_dwc2"], "note": "dwc2, STM32 variant" },
    { "board": "espressif_s3_devkitm", "family": "espressif", "toolchain": "esp-idf",
      "drivers": ["dcd_dwc2"], "note": "dwc2, ESP32 variant" }
  ],
  "uncovered": {
    "dcd_pic": "XC32/pic32mx toolchain not available in CI"
  }
}
```

- One entry per pinned board. `drivers` values are driver source basenames:
  `dcd_*`/`hcd_*` from `src/portable/**`, plus `ehci` and `ohci` (which do
  not follow the prefix naming). `template` is excluded.
- Membrowse target names stay `<board>/<example>` for history continuity.
- Board preference: HIL-rig boards first (size tracks tested hardware), then
  boards without external SDKs.

**Coverage checker** `.github/scripts/membrowse_targets_check.py`, wired as a
pre-commit hook (runs locally and in the pre-commit CI lane). Fails when:

- a driver file exists that is neither in any pinned entry's `drivers` nor in
  `uncovered` (with a non-empty reason);
- a `drivers` name matches no real file (catches renames);
- a board/family does not exist under `hw/bsp`;
- a driver appears in both `targets` and `uncovered`.

## 2. CI integration

**Board resolution in `build.py`.** New flag
`--ci-pinned-boards .github/ci-pinned-boards.json`, used by the cmake job in place
of `--one-first`:

- family has pinned boards → build exactly those boards (all examples, EX_ARGS
  example scoping still applies);
- family has none → current one-first alphabetical pick (compile smoke-check
  is preserved for every family in the matrix).

`ci_set_matrix.py` keeps emitting family names per toolchain; ci_select
scoping and the CircleCI full build are untouched. Families newly required by
pinned boards (e.g. samx7x) are added to `family_list`; drivers whose
toolchain/SDK cannot run in CI go to `uncovered` instead.

**Upload gating.** The Membrowse Upload step still invokes
`--target examples-membrowse-upload`, but under `--ci-pinned-boards` build.py visits
only pinned boards' build dirs for that target. Non-pinned fallback boards are
built but never uploaded. The `code-changed == false` (`--identical`) path is
kept, restricted to pinned boards. Scoped PRs upload only the families they
build; master pushes are unscoped, so membrowse history is complete per
master commit.

**esp-idf lane.** Re-enable the `esp-idf` toolchain in the cmake job matrix.
Build and upload run inside the same `docker run` (the upload needs the IDF
environment); `pip install membrowse` is added to the container invocation,
`MEMBROWSE_API_KEY` is already passed through.

**Onboard workflow** (`membrowse-onboard.yml`, currently broken — it reads
the deleted json): deleted. History backfill is a manual, occasional task and
runs locally through the membrowse skill (`membrowse onboard`), with target
names `<board>/<example>` matching uploads. Ongoing history accrues from the
per-commit uploads on master pushes.

## 3. linkermap becomes local-only

- `family_add_linkermap` keeps the `<target>-linkermap` custom target and
  drops the POST_BUILD hook — linkermap no longer runs on every example
  build anywhere.
- `family_add_linkermap` becomes a no-op when `tools/linkermap/linkermap.py`
  does not exist (`get_deps.py` not run for it), so configure works on a
  checkout without the dep.
- New aggregate `examples-linkermap` target (mirroring
  `examples-membrowse-upload`) generates all `*.map.json` in one invocation.
  With linkermap absent no per-target subtargets exist, so the aggregate is
  also skipped and building it fails with "unknown target".
- `tools/metrics_compare_base.py` builds `examples-linkermap` after the
  example build, then globs `*.map.json` as today; if the target is missing
  it errors with a clear "run tools/get_deps.py" message.
- `tools/metrics.py`, the `tools/linkermap` entry in `get_deps.py`, and the
  code-size skill remain.

## 3b. membrowse skill + local size diff

- New agent skill `.claude/skills/membrowse/SKILL.md` covering: local report
  (`membrowse report <elf> <ld> [--json] [--all-symbols]`, no API key),
  base-vs-branch size diff, pinned-target upload conventions
  (`<board>/<example>` naming, API key handling), and onboarding.
- `tools/metrics_compare_base.py` gains a membrowse engine (default;
  `--engine linkermap` keeps the current path): build base worktree + branch
  as today, run `membrowse report --json --all-symbols` per elf, and diff the
  two reports on our side — the CLI has no compare subcommand. Per-file
  deltas come from membrowse's DWARF line info; the JSON schema and
  per-source-file attribution must be verified during implementation. The
  code-size skill doc is updated for the engine switch.
- Linkermap (`-linkermap` targets, `metrics.py`, the `get_deps.py` entry)
  stays as the fallback engine; removing it is an explicit follow-up once
  membrowse diff output is proven equivalent (a
  `docs/superpowers/followup/` handoff, written when this lands).

## 4. CI linkermap pipeline removal

- `build.yml`: delete the `code-metrics` job, `upload-metrics: true`, the
  `build_families_regex` plumbing (its only consumer is code-metrics), and the
  release-asset metrics upload.
- `build_util.yml`: delete the `upload-metrics` input, the
  `--target tinyusb_metrics` branch, and the metrics artifact upload step.
- `pr_comment.yml`: delete the `metrics-comment` job (`membrowse-comment.yml`
  is the size comment).
- Delete `.github/scripts/metrics_pair_compare.py` and the `tinyusb_metrics`
  target in `examples/CMakeLists.txt`.
- `tools/ci_select.py` + `build.yml` path filters: drop rules referencing the
  removed metrics files; `ci-pinned-boards.json` becomes a CI-relevant path
  (changing it must trigger builds, not be skipped as meta).
- Check the `make-release` skill for references to release `metrics.json`
  assets; update if present.

## 5. Draft pinned-board list

Starting point; the final list is produced during implementation and enforced
by the checker. "candidate" marks entries whose exact board needs a build
check first.

| Driver(s)                        | Board                                   | Family         | Note                               |
| -------------------------------- | --------------------------------------- | -------------- | ---------------------------------- |
| dcd_dwc2, hcd_dwc2               | stm32f407disco                          | stm32f4        | dwc2 STM32, HIL                    |
| dcd_dwc2                         | espressif_s3_devkitm                    | espressif      | dwc2 ESP32, esp-idf lane           |
| dcd_dwc2                         | gd32vf103 board (candidate)             | gd32vf103      | dwc2 riscv variant                 |
| dcd_dwc2                         | raspberrypi_zero2w (candidate)          | broadcom_64bit | dwc2 bcm, keeps aarch64 lane       |
| dcd_stm32_fsdev, hcd_stm32_fsdev | stm32l412nucleo                         | stm32l4        | HIL                                |
| dcd_ci_hs, ehci                  | mimxrt1064_evk                          | imxrt          | HIL                                |
| hcd_ci_hs                        | mcx board (candidate)                   | mcx            |                                    |
| dcd_ci_fs, hcd_ci_fs             | frdm_kl25z                              | kinetis_kl     | HIL                                |
| dcd_lpc17_40, ohci               | lpcxpresso1769 (candidate)              | lpc17          |                                    |
| dcd_lpc_ip3511                   | lpcxpresso1549                          | lpc15          | ip3511 FS, HIL                     |
| dcd_lpc_ip3511, hcd_lpc_ip3516   | lpcxpresso55s69                         | lpc55          | ip3511 HS + ip3516 host            |
| dcd_musb, hcd_musb               | ek_tm4c123gxl                           | tm4c           | HIL                                |
| dcd_musb                         | msp_exp432e401y                         | msp432e4       | musb MSP432 variant                |
| dcd_mm32f327x_otg                | mm32 board (candidate)                  | mm32           |                                    |
| dcd_nrf5x                        | pca10056                                | nrf            | HIL                                |
| dcd_samd, hcd_samd               | same54_xplained                         | samd5x_e5x     |                                    |
| dcd_samd                         | samd21/samd11 board (candidate)         | samd2x_l2x     | FS variant (optional)              |
| dcd_samg                         | samg55_xplained (candidate)             | samg           |                                    |
| dcd_samx7x                       | same70_xplained (candidate)             | samx7x         | family added to CI                 |
| dcd_rp2040, hcd_rp2040           | raspberry_pi_pico                       | rp2040         | HIL                                |
| dcd_pio_usb, hcd_pio_usb         | raspberry_pi_pico + PIO_USB (candidate) | rp2040         | build-option variant               |
| hcd_max3421                      | board + MAX3421=1 (candidate)           |                | SPI host, build-option             |
| dcd_rusb2, hcd_rusb2             | ra board (candidate, HIL preferred)     | ra             |                                    |
| dcd_cxd56                        | spresense (candidate)                   | spresense      | family added to CI, else uncovered |
| dcd_da146xx                      | da1469x_dk_pro (candidate)              | da1469x        |                                    |
| dcd_eptri                        | fomu                                    | fomu           |                                    |
| dcd_ch32_usbfs, hcd_ch32_usbfs   | ch32v203/v307 board (candidate)         | ch32v20x/30x   |                                    |
| dcd_ch32_usbhs                   | ch32v307v_r1 (candidate)                | ch32v30x       |                                    |
| dcd_msp430x5xx                   | msp_exp430f5529lp                       | msp430         |                                    |
| dcd_nuc120                       | nutiny_nuc120 (candidate)               | nuc100_120     |                                    |
| dcd_nuc121                       | nutiny_nuc121s (candidate)              | nuc121_125     |                                    |
| dcd_nuc505                       | nutiny_nuc505 (candidate)               | nuc505         |                                    |
| dcd_sunxi_musb                   | f1c100s (candidate)                     | f1c100s        | family added to CI, else uncovered |
| dcd_ft9xx                        | mm900evxb (candidate)                   | ft9xx          | ft9xx-gcc lane                     |
| dcd_pic, dcd_pic32mz             | —                                       | —              | likely `uncovered` (XC toolchain)  |

## 6. Validation

1. Checker self-test: passes against the final json; fails when a driver
   entry is removed.
2. Local: `build.py --ci-pinned-boards` on one multi-pin family (lpc55 + lpc15) and
   one unpinned family; `metrics_compare_base.py` end-to-end on
   stm32f407disco (proves the explicit `examples-linkermap` path).
3. `<example>-membrowse` (no-upload) target still works post-refactor.
3b. Diff-engine equivalence: run `metrics_compare_base.py` with both engines
   on the same change (stm32f407disco) and compare per-file deltas; membrowse
   engine must attribute sizes to `src/` files comparably to metrics.py.
4. CI on the PR branch: build.yml runs with upload gating (no API key on the
   PR context → skips/`--identical` paths exercised), esp-idf lane compiles.
5. `pre-commit run --all-files`.

## Out of scope

- Backfilling membrowse history for newly pinned targets (run manually via
  the membrowse skill's onboard flow when wanted).
- IAR/CircleCI builds — untouched.
- Removing `tools/linkermap` from `get_deps.py` or the code-size skill.
