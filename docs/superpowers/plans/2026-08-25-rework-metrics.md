# Rework Code Metrics Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Membrowse becomes the single CI size-analytics system: the linkermap CI pipeline is removed, membrowse uploads are restricted to a pinned board set covering every dcd/hcd driver, and linkermap survives as a local-only fallback engine.

**Architecture:** A curated `.github/ci-pinned-boards.json` is the single source of truth: a pre-commit checker enforces driver coverage, `tools/build.py --ci-pinned-boards` resolves it into CI builds and uploads, and the linkermap-based `tinyusb_metrics`/`code-metrics`/sticky-comment pipeline is deleted. Locally, `tools/metrics_compare_base.py` gains a membrowse diff engine (default) with linkermap kept as `--engine linkermap` fallback.

**Tech Stack:** Python 3 (tools/, .github/scripts/), CMake (hw/bsp/family_support.cmake), GitHub Actions YAML, membrowse CLI (pip), pre-commit.

**Spec:** `docs/superpowers/specs/2026-08-25-rework-metrics-design.md`

## Global Constraints

- Python: repo style — stdlib only for tools/ and .github/scripts/ (no new pip deps besides `membrowse` itself, which is already a CI dep).
- Membrowse target names are `<board>/<example>` (e.g. `stm32f407disco/cdc_msc`) — never change them; history is keyed on them.
- Driver source basenames used in the json: `dcd_*`/`hcd_*` file stems under `src/portable/**`, plus `ehci` and `ohci`; `src/portable/template/` is excluded.
- CMake: 2-space indent, match existing `family_support.cmake` style.
- Commit messages: imperative mood, no Claude trailers/footers (repo rule).
- Never modify vendor SDK submodules (`hw/mcu/*`, `lib/*`).
- Run all commands from the repo root (`git rev-parse --show-toplevel`).
- All tests referenced live under `test/hil/test/` and run with plain `python3` (unittest), matching `test_ci_select.py` conventions.

---

### Task 1: Pinned-target config + coverage checker

**Files:**
- Create: `.github/ci-pinned-boards.json`
- Create: `.github/scripts/membrowse_targets_check.py`
- Create: `test/hil/test/test_membrowse_targets.py`
- Modify: `.pre-commit-config.yaml` (new hook after `ci-select-test`)

**Interfaces:**
- Produces: `.github/ci-pinned-boards.json` schema consumed by Task 2 (`build.py`) and Task 7 (skill):
  `{"targets": [{"board": str, "family": str, "toolchain": str, "drivers": [str], "note": str}], "uncovered": {driver: reason}}`
- Produces: `membrowse_targets_check.py` exit 0 on valid file, exit 1 with per-error lines on stderr.
- Produces: `load_targets(path) -> dict` and `list_drivers(portable_dir) -> set[str]` in `membrowse_targets_check.py` (imported by the test and by Task 2's test for a shared fixture).

- [ ] **Step 1: Write the failing test**

`test/hil/test/test_membrowse_targets.py`:

```python
#!/usr/bin/env python3
"""Tests for .github/scripts/membrowse_targets_check.py and the real targets file."""
import json
import os
import subprocess
import sys
import tempfile
import unittest

REPO = subprocess.run(['git', 'rev-parse', '--show-toplevel'],
                      capture_output=True, text=True, check=True).stdout.strip()
CHECKER = os.path.join(REPO, '.github', 'scripts', 'membrowse_targets_check.py')
TARGETS_JSON = os.path.join(REPO, '.github', 'ci-pinned-boards.json')

sys.path.insert(0, os.path.dirname(CHECKER))
import membrowse_targets_check as mtc  # noqa: E402


def run_checker(json_path):
    return subprocess.run([sys.executable, CHECKER, json_path],
                         capture_output=True, text=True)


class DriverScan(unittest.TestCase):
    def test_finds_known_drivers(self):
        drivers = mtc.list_drivers(os.path.join(REPO, 'src', 'portable'))
        for d in ('dcd_dwc2', 'hcd_dwc2', 'dcd_rp2040', 'ehci', 'ohci',
                  'dcd_stm32_fsdev', 'hcd_max3421'):
            self.assertIn(d, drivers)

    def test_excludes_template(self):
        drivers = mtc.list_drivers(os.path.join(REPO, 'src', 'portable'))
        self.assertNotIn('dcd_template', drivers)
        self.assertNotIn('hcd_template', drivers)


class CheckerVerdicts(unittest.TestCase):
    def test_real_file_passes(self):
        r = run_checker(TARGETS_JSON)
        self.assertEqual(r.returncode, 0, r.stderr)

    def _mutated(self, mutate):
        with open(TARGETS_JSON) as f:
            data = json.load(f)
        mutate(data)
        tmp = tempfile.NamedTemporaryFile('w', suffix='.json', delete=False)
        json.dump(data, tmp)
        tmp.close()
        self.addCleanup(os.unlink, tmp.name)
        return run_checker(tmp.name)

    def test_missing_driver_fails(self):
        # drop every entry covering dcd_rp2040 and don't add it to uncovered
        def mutate(d):
            for t in d['targets']:
                t['drivers'] = [x for x in t['drivers'] if x != 'dcd_rp2040']
        r = self._mutated(mutate)
        self.assertEqual(r.returncode, 1)
        self.assertIn('dcd_rp2040', r.stderr)

    def test_unknown_driver_name_fails(self):
        r = self._mutated(lambda d: d['targets'][0]['drivers'].append('dcd_nonexistent'))
        self.assertEqual(r.returncode, 1)
        self.assertIn('dcd_nonexistent', r.stderr)

    def test_unknown_board_fails(self):
        r = self._mutated(lambda d: d['targets'][0].update(board='no_such_board'))
        self.assertEqual(r.returncode, 1)
        self.assertIn('no_such_board', r.stderr)

    def test_driver_in_both_lists_fails(self):
        def mutate(d):
            drv = d['targets'][0]['drivers'][0]
            d['uncovered'][drv] = 'also uncovered'
        r = self._mutated(mutate)
        self.assertEqual(r.returncode, 1)

    def test_empty_uncovered_reason_fails(self):
        def mutate(d):
            for t in d['targets']:
                t['drivers'] = [x for x in t['drivers'] if x != 'dcd_rp2040']
            d['uncovered']['dcd_rp2040'] = ''
        r = self._mutated(mutate)
        self.assertEqual(r.returncode, 1)


if __name__ == '__main__':
    unittest.main()
```

- [ ] **Step 2: Run test to verify it fails**

Run: `python3 test/hil/test/test_membrowse_targets.py`
Expected: FAIL — `ModuleNotFoundError: membrowse_targets_check` (checker doesn't exist yet).

- [ ] **Step 3: Write the checker**

`.github/scripts/membrowse_targets_check.py`:

```python
#!/usr/bin/env python3
"""Validate .github/ci-pinned-boards.json against the driver and board tree.

Every dcd_*/hcd_* driver under src/portable (plus ehci/ohci, minus template/)
must be covered by a pinned board's `drivers` list or listed in `uncovered`
with a non-empty reason. Boards/families must exist under hw/bsp. Exit 0 on
success; print one line per error to stderr and exit 1 otherwise.
"""
import json
import os
import sys

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))


def list_drivers(portable_dir):
    """Driver source basenames: dcd_*/hcd_* stems plus ehci/ohci, template excluded."""
    drivers = set()
    for root, _dirs, files in os.walk(portable_dir):
        if os.path.basename(root) == 'template':
            continue
        for f in files:
            stem, ext = os.path.splitext(f)
            if ext != '.c':
                continue
            if stem.startswith(('dcd_', 'hcd_')) or stem in ('ehci', 'ohci'):
                drivers.add(stem)
    return drivers


def load_targets(path):
    with open(path) as f:
        return json.load(f)


def check(path):
    errors = []
    data = load_targets(path)
    targets = data.get('targets')
    uncovered = data.get('uncovered', {})
    if not isinstance(targets, list) or not targets:
        return [f'{path}: "targets" must be a non-empty list']
    if not isinstance(uncovered, dict):
        return [f'{path}: "uncovered" must be an object of driver: reason']

    drivers = list_drivers(os.path.join(REPO, 'src', 'portable'))
    covered = set()
    for i, t in enumerate(targets):
        where = f'targets[{i}]'
        for key in ('board', 'family', 'toolchain', 'drivers'):
            if key not in t:
                errors.append(f'{where}: missing "{key}"')
        board, family = t.get('board', ''), t.get('family', '')
        if family and not os.path.isdir(os.path.join(REPO, 'hw', 'bsp', family)):
            errors.append(f'{where}: unknown family "{family}"')
        elif board and not os.path.isdir(
                os.path.join(REPO, 'hw', 'bsp', family, 'boards', board)):
            errors.append(f'{where}: unknown board "{board}" in family "{family}"')
        for d in t.get('drivers', []):
            if d not in drivers:
                errors.append(f'{where} ({board}): "{d}" matches no driver source file')
            covered.add(d)

    for d, reason in uncovered.items():
        if d not in drivers:
            errors.append(f'uncovered: "{d}" matches no driver source file')
        if d in covered:
            errors.append(f'"{d}" is both pinned and uncovered')
        if not (isinstance(reason, str) and reason.strip()):
            errors.append(f'uncovered "{d}": reason must be a non-empty string')

    for d in sorted(drivers - covered - set(uncovered)):
        errors.append(f'driver "{d}" has no pinned board and is not in "uncovered"')
    return errors


def main():
    path = sys.argv[1] if len(sys.argv) > 1 else os.path.join(
        REPO, '.github', 'ci-pinned-boards.json')
    errors = check(path)
    for e in errors:
        print(e, file=sys.stderr)
    return 1 if errors else 0


if __name__ == '__main__':
    sys.exit(main())
```

- [ ] **Step 4: Write the initial `.github/ci-pinned-boards.json`**

Use this content, then fix every board/family name the checker rejects (the
checker validates against `hw/bsp`, so a wrong name fails loudly — check the
actual directory with `ls hw/bsp/<family>/boards/` and substitute):

```json
{
  "targets": [
    { "board": "stm32f407disco",          "family": "stm32f4",       "toolchain": "arm-gcc",    "drivers": ["dcd_dwc2", "hcd_dwc2"],               "note": "dwc2 STM32 variant, HIL" },
    { "board": "espressif_s3_devkitm",    "family": "espressif",     "toolchain": "esp-idf",    "drivers": ["dcd_dwc2"],                           "note": "dwc2 ESP32 variant" },
    { "board": "sipeed_longan_nano",      "family": "gd32vf103",     "toolchain": "riscv-gcc",  "drivers": ["dcd_dwc2"],                           "note": "dwc2 GD32 riscv variant" },
    { "board": "raspberrypi_zero2",       "family": "broadcom_64bit","toolchain": "aarch64-gcc","drivers": ["dcd_dwc2"],                           "note": "dwc2 BCM aarch64 variant" },
    { "board": "stm32l412nucleo",         "family": "stm32l4",       "toolchain": "arm-gcc",    "drivers": ["dcd_stm32_fsdev", "hcd_stm32_fsdev"], "note": "HIL" },
    { "board": "mimxrt1064_evk",          "family": "imxrt",         "toolchain": "arm-gcc",    "drivers": ["dcd_ci_hs", "ehci"],                  "note": "HIL" },
    { "board": "frdm_mcxn947",            "family": "mcx",           "toolchain": "arm-gcc",    "drivers": ["hcd_ci_hs"],                          "note": "" },
    { "board": "frdm_kl25z",              "family": "kinetis_kl",    "toolchain": "arm-gcc",    "drivers": ["dcd_ci_fs", "hcd_ci_fs"],             "note": "HIL (htpc)" },
    { "board": "lpcxpresso1769",          "family": "lpc17",         "toolchain": "arm-gcc",    "drivers": ["dcd_lpc17_40", "ohci"],               "note": "" },
    { "board": "lpcxpresso1549",          "family": "lpc15",         "toolchain": "arm-gcc",    "drivers": ["dcd_lpc_ip3511"],                     "note": "ip3511 FS, HIL (htpc)" },
    { "board": "lpcxpresso55s69",         "family": "lpc55",         "toolchain": "arm-gcc",    "drivers": ["dcd_lpc_ip3511", "hcd_lpc_ip3516"],   "note": "ip3511 HS + ip3516 host" },
    { "board": "ek_tm4c123gxl",           "family": "tm4c",          "toolchain": "arm-gcc",    "drivers": ["dcd_musb"],                           "note": "musb TM4C variant, HIL" },
    { "board": "msp_exp432e401y",         "family": "msp432e4",      "toolchain": "arm-gcc",    "drivers": ["hcd_musb"],                           "note": "musb MSP432E variant" },
    { "board": "mm32f327x_mb39",          "family": "mm32",          "toolchain": "arm-gcc",    "drivers": ["dcd_mm32f327x_otg"],                  "note": "" },
    { "board": "pca10056",                "family": "nrf",           "toolchain": "arm-gcc",    "drivers": ["dcd_nrf5x"],                          "note": "HIL" },
    { "board": "same54_xplained",         "family": "samd5x_e5x",    "toolchain": "arm-gcc",    "drivers": ["dcd_samd", "hcd_samd"],               "note": "" },
    { "board": "samg55_xplained",         "family": "samg",          "toolchain": "arm-gcc",    "drivers": ["dcd_samg"],                           "note": "" },
    { "board": "same70_xplained",         "family": "same7x",        "toolchain": "arm-gcc",    "drivers": ["dcd_samx7x"],                         "note": "family newly added to CI" },
    { "board": "raspberry_pi_pico",       "family": "rp2040",        "toolchain": "arm-gcc",    "drivers": ["dcd_rp2040", "hcd_rp2040"],           "note": "HIL" },
    { "board": "feather_rp2040_max3421",  "family": "rp2040",        "toolchain": "arm-gcc",    "drivers": ["hcd_max3421"],                        "note": "MAX3421_HOST=1 via board.cmake" },
    { "board": "ra6m5_ek",                "family": "ra",            "toolchain": "arm-gcc",    "drivers": ["dcd_rusb2", "hcd_rusb2"],             "note": "HIL" },
    { "board": "spresense",               "family": "cxd56",         "toolchain": "arm-gcc",    "drivers": ["dcd_cxd56"],                          "note": "family newly added to CI" },
    { "board": "da1469x_dk_pro",          "family": "da1469x",       "toolchain": "arm-gcc",    "drivers": ["dcd_da146xx"],                        "note": "" },
    { "board": "fomu",                    "family": "fomu",          "toolchain": "riscv-gcc",  "drivers": ["dcd_eptri"],                          "note": "" },
    { "board": "nanoch32v203",            "family": "ch32v20x",      "toolchain": "riscv-gcc",  "drivers": ["dcd_ch32_usbfs"],                     "note": "" },
    { "board": "ch32v307v_r1_1v0",        "family": "ch32v30x",      "toolchain": "riscv-gcc",  "drivers": ["dcd_ch32_usbhs", "hcd_ch32_usbfs"],   "note": "" },
    { "board": "msp_exp430f5529lp",       "family": "msp430",        "toolchain": "msp430-gcc", "drivers": ["dcd_msp430x5xx"],                     "note": "" },
    { "board": "nutiny_sdk_nuc120",       "family": "nuc100_120",    "toolchain": "arm-gcc",    "drivers": ["dcd_nuc120"],                         "note": "" },
    { "board": "nutiny_sdk_nuc121",       "family": "nuc121_125",    "toolchain": "arm-gcc",    "drivers": ["dcd_nuc121"],                         "note": "" },
    { "board": "nutiny_sdk_nuc505",       "family": "nuc505",        "toolchain": "arm-gcc",    "drivers": ["dcd_nuc505"],                         "note": "" },
    { "board": "f1c100s",                 "family": "f1c100s",       "toolchain": "arm-gcc",    "drivers": ["dcd_sunxi_musb"],                     "note": "family newly added to CI" },
    { "board": "mm900evxb",               "family": "ft9xx",         "toolchain": "ft9xx-gcc",  "drivers": ["dcd_ft9xx"],                          "note": "" }
  ],
  "uncovered": {
    "dcd_pic":      "pic32mx has no BSP family in hw/bsp; XC toolchain not in CI",
    "dcd_pic32mz":  "hw/bsp/pic32mz is make-only (no family.cmake); CI membrowse lane is cmake",
    "dcd_pio_usb":  "PIO-USB device role needs Pico-PIO-USB lib config no CI board enables by default",
    "hcd_pio_usb":  "PIO-USB host: revisit with adafruit_feather_rp2040_usb_host once its CI build is proven",
    "dcd_musb":     "PLACEHOLDER-REMOVE: dcd_musb is covered by ek_tm4c123gxl; this key exists only so you remember to delete it after the checker run confirms coverage detection works"
  }
}
```

Then delete the `dcd_musb` uncovered entry (the checker must fail on it first —
that's your live negative test), and re-run until clean. Where a family listed
here later fails to build (Task 5 verification), move its driver(s) to
`uncovered` with the build error as the reason and drop the target entry.

- [ ] **Step 5: Run tests to verify they pass**

Run: `python3 test/hil/test/test_membrowse_targets.py -v`
Expected: all tests PASS (after the `dcd_musb` duplicate is removed).

- [ ] **Step 6: Add the pre-commit hook**

In `.pre-commit-config.yaml`, after the `ci-select-test` hook block, add:

```yaml
  - id: membrowse-targets
    name: membrowse-targets
    files: ^(\.github/membrowse-targets\.json$|\.github/scripts/membrowse_targets_check\.py$|src/portable/|test/hil/test/test_membrowse_targets\.py$)
    entry: sh -c "python3 .github/scripts/membrowse_targets_check.py && python3 test/hil/test/test_membrowse_targets.py"
    pass_filenames: false
    language: system
```

- [ ] **Step 7: Run the hook and commit**

Run: `pre-commit run membrowse-targets --all-files`
Expected: Passed.

```bash
git add .github/ci-pinned-boards.json .github/scripts/membrowse_targets_check.py \
        test/hil/test/test_membrowse_targets.py .pre-commit-config.yaml
git commit -m "ci: add membrowse pinned-target config with driver-coverage checker"
```

---

### Task 2: `build.py --ci-pinned-boards` / `--ci-pinned-boards-only`

**Files:**
- Modify: `tools/build.py` (argparse block ~line 380, board collection in `main()` ~line 455)
- Create: `test/hil/test/test_board_pins.py`

**Interfaces:**
- Consumes: `.github/ci-pinned-boards.json` (Task 1 schema).
- Produces: `resolve_pinned_boards(pins_path, family, pins_only, examples=None, build_system='cmake', extra_defines=(), ci=None) -> list[str]` in `tools/build.py`. CLI: `--ci-pinned-boards <path>` (per family: pinned boards if any, else the `--one-first` pick) and `--ci-pinned-boards-only` (families without pins contribute no boards; requires `--ci-pinned-boards`). Task 5's workflows call:
  - build step: `tools/build.py --ci-pinned-boards .github/ci-pinned-boards.json --target all <families>`
  - upload step: `tools/build.py --ci-pinned-boards .github/ci-pinned-boards.json --ci-pinned-boards-only --target examples-membrowse-upload -j 1 <families>`

- [ ] **Step 1: Write the failing test**

`test/hil/test/test_board_pins.py`:

```python
#!/usr/bin/env python3
"""Tests for build.py --ci-pinned-boards resolution against the real targets file."""
import os
import subprocess
import sys
import unittest

REPO = subprocess.run(['git', 'rev-parse', '--show-toplevel'],
                      capture_output=True, text=True, check=True).stdout.strip()
sys.path.insert(0, os.path.join(REPO, 'tools'))
os.chdir(REPO)  # build.py resolves hw/bsp relative to cwd
import build  # noqa: E402

PINS = os.path.join(REPO, '.github', 'ci-pinned-boards.json')


class BoardPins(unittest.TestCase):
    def test_pinned_family_returns_pins(self):
        boards = build.resolve_pinned_boards(PINS, 'rp2040', pins_only=False, ci=True)
        self.assertIn('raspberry_pi_pico', boards)
        self.assertIn('feather_rp2040_max3421', boards)  # pin overrides ci_skip_boards

    def test_unpinned_family_falls_back_to_one_first(self):
        # stm32f0 has no pinned board; expect exactly the one-first pick
        expected = build.get_family_boards('stm32f0', one_random=False,
                                           one_first=True, ci=True)
        boards = build.resolve_pinned_boards(PINS, 'stm32f0', pins_only=False, ci=True)
        self.assertEqual(boards, expected)

    def test_pins_only_skips_unpinned_family(self):
        boards = build.resolve_pinned_boards(PINS, 'stm32f0', pins_only=True, ci=True)
        self.assertEqual(boards, [])

    def test_pins_only_keeps_pinned_family(self):
        boards = build.resolve_pinned_boards(PINS, 'stm32f4', pins_only=True, ci=True)
        self.assertEqual(boards, ['stm32f407disco'])


if __name__ == '__main__':
    unittest.main()
```

- [ ] **Step 2: Run test to verify it fails**

Run: `python3 test/hil/test/test_board_pins.py`
Expected: FAIL — `AttributeError: module 'build' has no attribute 'resolve_pinned_boards'`.

- [ ] **Step 3: Implement in `tools/build.py`**

Add after `get_family_boards()`:

```python
def resolve_pinned_boards(pins_path, family, pins_only, examples=None,
                          build_system='cmake', extra_defines=(), ci=None):
    """Resolve a family to its membrowse-pinned boards.

    Pinned boards are returned as-is (an explicit pin outrules ci_skip_boards -
    that is how feather_rp2040_max3421 gets built for hcd_max3421). A family
    with no pins falls back to the one-first pick, or to nothing under
    pins_only (the upload step must not touch build dirs of boards that were
    built only as compile smoke-checks)."""
    with open(pins_path) as f:
        data = json.load(f)
    pinned = [t['board'] for t in data['targets'] if t['family'] == family]
    if pinned:
        return pinned
    if pins_only:
        return []
    return get_family_boards(family, one_random=False, one_first=True,
                             examples=examples, build_system=build_system,
                             extra_defines=extra_defines, ci=ci)
```

Add `import json` to the imports at the top of `build.py` (it is not imported today).

Add the CLI arguments next to `--one-first` in `main()`'s argparse block:

```python
    parser.add_argument('--ci-pinned-boards', default=None, metavar='JSON',
                        help='Path to ci-pinned-boards.json: build the pinned boards '
                             'of each family (fallback: first board alphabetically)')
    parser.add_argument('--ci-pinned-boards-only', action='store_true', default=False,
                        help='With --ci-pinned-boards: skip families that have no pinned board')
```

In `main()`, validate and use them. After `one_first = args.one_first` add:

```python
    board_pins = args.board_pins
    pins_only = args.pins_only
    if pins_only and not board_pins:
        parser.error('--ci-pinned-boards-only requires --ci-pinned-boards')
    if board_pins and (one_first or one_random):
        parser.error('--ci-pinned-boards replaces --one-first/--one-random')
```

Replace the family→boards loop body:

```python
    for f in all_families:
        if board_pins:
            all_boards.extend(resolve_pinned_boards(board_pins, f, pins_only, examples,
                                                    build_system, tuple(build_defines)))
        else:
            all_boards.extend(get_family_boards(f, one_random, one_first, examples,
                                                build_system, tuple(build_defines)))
```

- [ ] **Step 4: Run tests to verify they pass**

Run: `python3 test/hil/test/test_board_pins.py -v`
Expected: PASS (4 tests).

- [ ] **Step 5: Smoke-check the CLI end to end**

```bash
python3 tools/build.py --ci-pinned-boards .github/ci-pinned-boards.json --ci-pinned-boards-only \
        -e device/cdc_msc stm32f4
```
Expected: builds `device/cdc_msc` for exactly `stm32f407disco`, summary `1 OK`.

- [ ] **Step 6: Extend the pre-commit hook file filter and commit**

`test_board_pins.py` reads `hw/bsp` and the targets json — add it to the
`membrowse-targets` hook entry in `.pre-commit-config.yaml`:

```yaml
    entry: sh -c "python3 .github/scripts/membrowse_targets_check.py && python3 test/hil/test/test_membrowse_targets.py && python3 test/hil/test/test_board_pins.py"
```
and extend its `files:` regex with `|tools/build\.py$|test/hil/test/test_board_pins\.py$`.

```bash
pre-commit run membrowse-targets --all-files
git add tools/build.py test/hil/test/test_board_pins.py .pre-commit-config.yaml
git commit -m "build.py: resolve membrowse board pins with --ci-pinned-boards/--ci-pinned-boards-only"
```

---

### Task 3: linkermap becomes explicit-target-only in CMake

**Files:**
- Modify: `hw/bsp/family_support.cmake:255-274` (`family_add_linkermap`)
- Modify: `examples/CMakeLists.txt:20-26` (delete `tinyusb_metrics` target)

**Interfaces:**
- Produces: per-example `<target>-linkermap` custom target (unchanged name) and a new aggregate `examples-linkermap` target; no POST_BUILD linkermap anywhere. `family_add_linkermap` silently no-ops when `tools/linkermap/linkermap.py` is absent. Task 4 builds `examples-linkermap`.

- [ ] **Step 1: Rewrite `family_add_linkermap`**

Replace the whole function body (`hw/bsp/family_support.cmake:256-274`) with:

```cmake
function(family_add_linkermap TARGET)
  # local-only tool: skip silently when get_deps.py has not fetched it
  if (NOT EXISTS ${LINKERMAP_PY})
    return()
  endif ()

  set(OPTION "-j")
  if (DEFINED LINKERMAP_OPTION)
    string(APPEND OPTION " ${LINKERMAP_OPTION}")
  endif ()
  separate_arguments(OPTION_LIST UNIX_COMMAND ${OPTION})

  add_custom_target(${TARGET}-linkermap
    DEPENDS ${TARGET}
    COMMAND python ${LINKERMAP_PY} ${OPTION_LIST} $<TARGET_FILE:${TARGET}>.map
    VERBATIM
    )

  if (NOT TARGET examples-linkermap)
    add_custom_target(examples-linkermap)
  endif ()
  add_dependencies(examples-linkermap ${TARGET}-linkermap)
endfunction()
```

Notes on the diff vs today: the POST_BUILD `add_custom_command` block is
deleted; `DEPENDS ${TARGET}` is added to the custom target (previously the
POST_BUILD hook guaranteed the elf existed — the explicit target must now
build it first); the aggregate mirrors the `examples-membrowse-upload`
pattern at `family_support.cmake:357-360`.

- [ ] **Step 2: Delete the `tinyusb_metrics` target**

In `examples/CMakeLists.txt`, delete lines 20-26 (the comment plus the
`add_custom_target(tinyusb_metrics ...)` block). Nothing else in that file
references it.

- [ ] **Step 3: Verify — normal build no longer runs linkermap**

```bash
cd examples/device/cdc_msc && rm -rf build && mkdir build && cd build
cmake -DBOARD=stm32f407disco -G Ninja -DCMAKE_BUILD_TYPE=MinSizeRel .. >/dev/null && cmake --build . 2>&1 | grep -i linkermap
```
Expected: no output from the grep (exit 1) — linkermap did not run during the build.

- [ ] **Step 4: Verify — explicit target produces map.json**

```bash
cmake --build . --target cdc_msc-linkermap
ls cdc_msc.map.json
```
Expected: file exists. Then verify the aggregate from the all-examples build dir:

```bash
cd ../../../..   # repo root
cmake -B /tmp/claude-lm-check -DBOARD=stm32f407disco -G Ninja -DCMAKE_BUILD_TYPE=MinSizeRel examples >/dev/null
cmake --build /tmp/claude-lm-check --target examples-linkermap 2>&1 | tail -3
ls /tmp/claude-lm-check/device/cdc_msc/cdc_msc.map.json
rm -rf /tmp/claude-lm-check
```
Expected: aggregate builds every example and its map.json exists.

- [ ] **Step 5: Verify — configure works without linkermap**

```bash
mv tools/linkermap tools/linkermap.away
cd examples/device/cdc_msc && rm -rf build && mkdir build && cd build
cmake -DBOARD=stm32f407disco -G Ninja -DCMAKE_BUILD_TYPE=MinSizeRel .. >/dev/null && echo CONFIGURE_OK
cd ../../../.. && mv tools/linkermap.away tools/linkermap
```
Expected: `CONFIGURE_OK` (and no `-linkermap` targets exist in that build dir).

- [ ] **Step 6: Commit**

```bash
git add hw/bsp/family_support.cmake examples/CMakeLists.txt
git commit -m "cmake: make linkermap an explicit local target, drop POST_BUILD hook"
```

---

### Task 4: membrowse diff engine in `metrics_compare_base.py`

**Files:**
- Create: `tools/membrowse_compare.py`
- Modify: `tools/metrics_compare_base.py` (`generate_metrics()` ~line 130, `build_board()` ~line 115, argparse in `main()`)
- Create: `test/hil/test/test_membrowse_compare.py`

**Interfaces:**
- Consumes: `examples-linkermap` aggregate target (Task 3) for the linkermap engine path.
- Produces: `tools/membrowse_compare.py` with:
  - `report_for_elf(elf_path, map_path=None) -> dict` — runs `membrowse report <elf> --json --all-symbols` (plus `--map-file` when the .map exists) and returns the parsed JSON.
  - `per_file_sizes(report, filters) -> dict[str, dict]` — `{source_file: {'flash': int, 'ram': int}}`, keeping symbols whose `source_file` contains any filter substring.
  - `compare_reports(base_by_file, cur_by_file) -> str` — markdown table of per-file flash/ram deltas (files present on either side; delta columns signed; totals row).
- Produces: `metrics_compare_base.py --engine {membrowse,linkermap}` (default `membrowse`).

- [ ] **Step 1: Write the failing test**

`test/hil/test/test_membrowse_compare.py`:

```python
#!/usr/bin/env python3
"""Unit tests for tools/membrowse_compare.py (pure functions, no build needed)."""
import os
import subprocess
import sys
import unittest

REPO = subprocess.run(['git', 'rev-parse', '--show-toplevel'],
                      capture_output=True, text=True, check=True).stdout.strip()
sys.path.insert(0, os.path.join(REPO, 'tools'))
import membrowse_compare as mc  # noqa: E402


def fake_report(symbols):
    return {'symbols': symbols}


SYMS_BASE = [
    {'name': 'dcd_init', 'size': 100, 'section': '.text',
     'source_file': '/co/src/portable/synopsys/dwc2/dcd_dwc2.c'},
    {'name': 'dcd_buf', 'size': 64, 'section': '.bss',
     'source_file': '/co/src/portable/synopsys/dwc2/dcd_dwc2.c'},
    {'name': 'vendor_thing', 'size': 999, 'section': '.text',
     'source_file': '/co/hw/mcu/st/whatever.c'},
]
SYMS_CUR = [
    {'name': 'dcd_init', 'size': 120, 'section': '.text',
     'source_file': '/co2/src/portable/synopsys/dwc2/dcd_dwc2.c'},
    {'name': 'dcd_buf', 'size': 64, 'section': '.bss',
     'source_file': '/co2/src/portable/synopsys/dwc2/dcd_dwc2.c'},
]


class PerFileSizes(unittest.TestCase):
    def test_filters_and_buckets(self):
        by_file = mc.per_file_sizes(fake_report(SYMS_BASE), ['/co/src/'])
        self.assertEqual(len(by_file), 1)  # vendor_thing filtered out
        (path, sizes), = by_file.items()
        self.assertIn('dcd_dwc2.c', path)
        self.assertEqual(sizes['flash'], 100)   # .text
        self.assertEqual(sizes['ram'], 64)      # .bss

    def test_data_counts_both(self):
        syms = [{'name': 'd', 'size': 8, 'section': '.data',
                 'source_file': '/co/src/x.c'}]
        by_file = mc.per_file_sizes(fake_report(syms), ['/co/src/'])
        sizes = by_file[next(iter(by_file))]
        self.assertEqual(sizes['flash'], 8)
        self.assertEqual(sizes['ram'], 8)


class CompareReports(unittest.TestCase):
    def test_delta_table(self):
        base = mc.per_file_sizes(fake_report(SYMS_BASE), ['/co/src/'])
        cur = mc.per_file_sizes(fake_report(SYMS_CUR), ['/co2/src/'])
        md = mc.compare_reports(base, cur)
        self.assertIn('dcd_dwc2.c', md)
        self.assertIn('+20', md)          # flash grew 100 -> 120
        self.assertIn('TOTAL', md)


if __name__ == '__main__':
    unittest.main()
```

- [ ] **Step 2: Run test to verify it fails**

Run: `python3 test/hil/test/test_membrowse_compare.py`
Expected: FAIL — `ModuleNotFoundError: membrowse_compare`.

- [ ] **Step 3: Implement `tools/membrowse_compare.py`**

```python
#!/usr/bin/env python3
"""Diff two membrowse local reports per source file.

The membrowse CLI has report generation but no compare subcommand; this module
runs `membrowse report --json --all-symbols` per elf and diffs the results.
Key normalization: paths are keyed RELATIVE to the first matched filter
substring, so base and current checkouts (different absolute prefixes)
compare under the same key.
"""
import json
import os
import subprocess
import sys

# section-name -> which budgets a symbol counts against
FLASH_SECTIONS = ('.text', '.rodata', '.isr_vector', '.vector', '.init', '.fini')
RAM_SECTIONS = ('.bss', '.noinit', '.stack', '.heap')
BOTH_SECTIONS = ('.data', '.ramfunc', '.fastrun', '.itcm', '.dtcm')


def report_for_elf(elf_path, map_path=None):
    """Run membrowse local report on one elf, return parsed JSON dict."""
    cmd = ['membrowse', 'report', elf_path, '--json', '--all-symbols']
    if map_path and os.path.isfile(map_path):
        cmd += ['--map-file', map_path]
    r = subprocess.run(cmd, capture_output=True, text=True)
    if r.returncode != 0:
        raise RuntimeError(f'membrowse report failed for {elf_path}: {r.stderr}')
    return json.loads(r.stdout)


def _bucket(section):
    s = section or ''
    if any(s.startswith(p) for p in BOTH_SECTIONS):
        return ('flash', 'ram')
    if any(s.startswith(p) for p in RAM_SECTIONS):
        return ('ram',)
    if any(s.startswith(p) for p in FLASH_SECTIONS):
        return ('flash',)
    return ('flash',)  # unknown allocated section: count as flash, never drop


def per_file_sizes(report, filters):
    """{relative source path: {'flash': n, 'ram': n}} for symbols matching filters."""
    by_file = {}
    for sym in report.get('symbols', []):
        src = sym.get('source_file') or ''
        if not src or not sym.get('size'):
            continue
        key = None
        for f in filters:
            idx = src.find(f)
            if idx >= 0:
                key = src[idx + len(f):]
                break
        if key is None:
            continue
        entry = by_file.setdefault(key, {'flash': 0, 'ram': 0})
        for b in _bucket(sym.get('section')):
            entry[b] += sym['size']
    return by_file


def _fmt(delta):
    return f'+{delta}' if delta > 0 else str(delta)


def compare_reports(base_by_file, cur_by_file):
    """Markdown per-file delta table; files sorted by |flash delta| desc."""
    rows = []
    for path in sorted(set(base_by_file) | set(cur_by_file)):
        b = base_by_file.get(path, {'flash': 0, 'ram': 0})
        c = cur_by_file.get(path, {'flash': 0, 'ram': 0})
        df, dr = c['flash'] - b['flash'], c['ram'] - b['ram']
        rows.append((path, b, c, df, dr))
    rows.sort(key=lambda r: abs(r[3]), reverse=True)

    lines = ['| File | Flash base | Flash new | Flash Δ | RAM base | RAM new | RAM Δ |',
             '|------|-----------:|----------:|--------:|---------:|--------:|------:|']
    tb = {'flash': 0, 'ram': 0}
    tc = {'flash': 0, 'ram': 0}
    for path, b, c, df, dr in rows:
        # totals run over ALL rows; the table prints only changed ones
        tb['flash'] += b['flash']; tb['ram'] += b['ram']
        tc['flash'] += c['flash']; tc['ram'] += c['ram']
        if df == 0 and dr == 0:
            continue
        lines.append(f'| {path} | {b["flash"]} | {c["flash"]} | {_fmt(df)} '
                     f'| {b["ram"]} | {c["ram"]} | {_fmt(dr)} |')
    lines.append(f'| **TOTAL** | {tb["flash"]} | {tc["flash"]} | '
                 f'{_fmt(tc["flash"] - tb["flash"])} | {tb["ram"]} | {tc["ram"]} | '
                 f'{_fmt(tc["ram"] - tb["ram"])} |')
    if len(lines) == 3 and rows:
        lines.insert(2, '| _no per-file changes_ | | | | | | |')
    return '\n'.join(lines) + '\n'
```

- [ ] **Step 4: Run tests to verify they pass**

Run: `python3 test/hil/test/test_membrowse_compare.py -v`
Expected: PASS.

- [ ] **Step 5: Wire the engine into `metrics_compare_base.py`**

Three edits:

1. Argparse (in `main()`):

```python
    parser.add_argument('--engine', choices=['membrowse', 'linkermap'],
                        default='membrowse',
                        help='Size-diff engine (default: membrowse local reports; '
                             'linkermap is the legacy map.json path)')
```

2. `generate_metrics()` currently assumes map.json exists because the old
POST_BUILD hook produced it. For the linkermap engine, build the aggregate
first. Add to `build_board()` (after the existing `cmake --build` call, gated
on a new `linkermap=False` keyword argument threaded from `main()` when
`args.engine == 'linkermap'`):

```python
    if linkermap:
        ret = run(['cmake', '--build', build_dir, '--target',
                   os.path.basename(example) + '-linkermap' if example
                   else 'examples-linkermap'], timeout=600)
        if ret.returncode != 0:
            print(f'  Error: linkermap target failed for {board} - '
                  f'run `python3 tools/get_deps.py` to fetch tools/linkermap')
            return False
```

3. Add the membrowse path alongside `generate_metrics()`:

```python
def generate_membrowse_sizes(build_dir, filters, example=None):
    """Per-file sizes from membrowse local reports over every elf in build_dir."""
    import membrowse_compare
    pattern = f'{build_dir}/{example}/*.elf' if example \
        else f'{build_dir}/**/*.elf'
    elfs = glob.glob(pattern, recursive=True)
    if not elfs:
        print(f'  Error: no .elf files in {build_dir}')
        return None
    combined = {}
    for elf in sorted(elfs):
        report = membrowse_compare.report_for_elf(elf, elf + '.map')
        for path, sizes in membrowse_compare.per_file_sizes(report, filters).items():
            entry = combined.setdefault(path, {'flash': 0, 'ram': 0})
            entry['flash'] += sizes['flash']
            entry['ram'] += sizes['ram']
    return combined
```

In the per-board comparison section of `main()` (where `generate_metrics` is
called for base and current builds today), branch on the engine: for
`membrowse`, call `generate_membrowse_sizes(base_build, base_filters, example)`
and `...(cur_build, cur_filters, example)`, then write
`membrowse_compare.compare_reports(base_sizes, cur_sizes)` to the same
`metrics_compare.md` path the linkermap engine writes. IMPORTANT: the default
filters in `metrics_compare_base.py` are each checkout's absolute
`<checkout>/src/` — exactly what `per_file_sizes` needs to relativize keys, so
pass them through unchanged. Do not touch the `--bloaty` path.

- [ ] **Step 6: End-to-end verification (both engines)**

Make a deliberate size change, run both engines, compare:

```bash
# any trivial change that adds flash, e.g. add to src/tusb.c a
# volatile const uint8_t _size_probe[32] = {1};  (revert afterwards)
python3 tools/metrics_compare_base.py -b stm32f407disco -e device/cdc_msc --engine linkermap
python3 tools/metrics_compare_base.py -b stm32f407disco -e device/cdc_msc --engine membrowse
```

Expected: both `cmake-metrics/stm32f407disco/metrics_compare*.md` outputs show
tusb.c growing by ~32 bytes flash. If membrowse attributes to a different
file or misses the delta, STOP — the engine is not equivalent; keep default
`linkermap` and record the discrepancy in the commit message and in
`docs/superpowers/followup/` per the spec. Revert the probe change.

- [ ] **Step 7: Commit**

```bash
git add tools/membrowse_compare.py tools/metrics_compare_base.py \
        test/hil/test/test_membrowse_compare.py
git commit -m "metrics: add membrowse local-report diff engine, default for code-size compare"
```

---

### Task 5: CI selector + matrix plumbing

**Files:**
- Modify: `.github/scripts/ci_set_matrix.py` (family_list)
- Modify: `tools/ci_select.py` (~lines 28, 117-132, 648, 1150-1152)
- Modify: `test/hil/test/test_ci_select.py`, `test/hil/test/test_ci_metrics.py` (whatever asserts the old classifications — find with `grep -n "metrics\|membrowse" test/hil/test/test_ci_select.py test/hil/test/test_ci_metrics.py`)

**Interfaces:**
- Consumes: nothing new; prepares the family/rule ground Task 6's workflows stand on.
- Produces: `family_list` entries `"espressif": ["esp-idf"]`, `"same7x": ["arm-gcc"]`, `"cxd56": ["arm-gcc"]`, `"f1c100s": ["arm-gcc"]`; ci_select classifications: metrics tooling = meta (no build), `ci-pinned-boards.json` = full-build path, `membrowse-onboard` regex entry removed.

- [ ] **Step 1: Add the new families to `ci_set_matrix.py`**

In `family_list` (alphabetical position):

```python
    "cxd56": ["arm-gcc"],
    "espressif": ["esp-idf"],
    "f1c100s": ["arm-gcc"],
    "same7x": ["arm-gcc"],
```

Remove the now-stale trailing comment block (`# S3, P4 will be built by hil test`
and the two commented `-bespressif_*` lines) — espressif is a real matrix
family again.

- [ ] **Step 2: Verify each new family actually builds before it ships**

```bash
python3 tools/get_deps.py same7x cxd56 f1c100s
python3 tools/build.py --ci-pinned-boards .github/ci-pinned-boards.json --ci-pinned-boards-only \
        -e device/cdc_msc same7x cxd56 f1c100s
```
Expected: `3 OK`. For any family that FAILS and resists a quick fix: remove it
from `family_list` again, delete its entry from `.github/ci-pinned-boards.json`,
move its driver(s) to `uncovered` with the build error one-liner as reason,
and re-run `pre-commit run membrowse-targets --all-files` (must pass).
(espressif is verified in Task 6 — it needs the IDF environment.)

- [ ] **Step 3: Update `tools/ci_select.py`**

Three classification changes (find the exact lines with
`grep -n "metrics\|membrowse" tools/ci_select.py`):

1. The build-axis force-full for metrics tooling (~line 1150-1152,
   `s.force_full(f'{path}: metrics tooling runs in the build -> full build matrix')`):
   DELETE this branch — `tinyusb_metrics` no longer exists, so
   `tools/metrics.py` / `.github/scripts/metrics_*.py` no longer run in any CI
   build. Fold these paths into the meta/no-build classification the HIL axis
   already gives them (~line 648 keeps its reason line, now for both axes).
   Update the rule-table comment at ~line 28 (rule 2b) to say the paths are
   local-only tooling → no build, no HIL.
2. The meta workflow regex (~lines 117-119): remove `membrowse-onboard|`
   (the workflow is deleted in Task 6). `membrowse-comment` stays meta.
3. Ensure `.github/ci-pinned-boards.json` is NOT matched by any meta rule
   (it must classify as a full-build path — it changes which boards CI
   builds). Check with:
   `python3 - <<'EOF'` ... or simpler: temporarily `git diff --name-only`-style
   dry-run: `echo .github/ci-pinned-boards.json | python3 tools/ci_select.py --stdin`
   (use the actual invocation `test_ci_select.py` uses if `--stdin` does not
   exist — read the test file for the harness pattern).
   Expected classification: full build matrix, no HIL.

- [ ] **Step 4: Update the selector tests**

Run: `python3 test/hil/test/test_ci_select.py && python3 test/hil/test/test_ci_metrics.py`

Every failure names an assertion about the OLD classification — update those
assertions to the new expectations from Step 3 (metrics tooling → no build;
add a new case asserting `.github/ci-pinned-boards.json` → full build; drop
`membrowse-onboard.yml` cases). `test_ci_metrics.py` tests `tools/metrics.py`
itself — it stays passing untouched (metrics.py is unchanged); only its
selector-classification cases (if any) move.

- [ ] **Step 5: Run the full selector suite and commit**

Run: `pre-commit run ci-select-test --all-files && pre-commit run membrowse-targets --all-files`
Expected: Passed.

```bash
git add .github/scripts/ci_set_matrix.py tools/ci_select.py \
        test/hil/test/test_ci_select.py test/hil/test/test_ci_metrics.py \
        .github/ci-pinned-boards.json
git commit -m "ci_select: metrics tooling is local-only; add pinned membrowse families"
```

---

### Task 6: Workflow rewiring

**Files:**
- Modify: `.github/workflows/build.yml` (cmake job ~235-259, code-metrics job 261-389, set-matrix outputs ~30, check-paths filter ~40-48)
- Modify: `.github/workflows/build_util.yml` (inputs ~31-38, Build step ~106-121, Membrowse Upload step ~123-139, metrics artifact step ~142-148)
- Modify: `.github/workflows/pr_comment.yml` (metrics-comment job ~46-70)
- Delete: `.github/workflows/membrowse-onboard.yml`
- Delete: `.github/scripts/metrics_pair_compare.py`

**Interfaces:**
- Consumes: `--ci-pinned-boards`/`--ci-pinned-boards-only` (Task 2), families (Task 5).
- Produces: the final CI shape — no metrics artifacts, membrowse-only size analytics, esp-idf lane uploading.

- [ ] **Step 1: `build_util.yml` edits**

1. Delete the `upload-metrics` input block (lines ~31-35).
2. In the Build step: delete the
   `if [ "${{ inputs.upload-metrics }}" = "true" ]; then BUILD_PY_ARGS=...tinyusb_metrics; fi`
   branch, and replace `--one-first` in `inputs.build-options` usage — NOT
   here; `--one-first` arrives via `build-options` from build.yml (changed in
   Step 3). The esp-idf docker branch becomes:

```yaml
          if [ "${{ inputs.toolchain }}" == "esp-idf" ]; then
            docker run --rm -e MEMBROWSE_API_KEY="$MEMBROWSE_API_KEY" -e CI="$CI" -v $PWD:/project -w /project espressif/idf:tinyusb bash -c "pip install membrowse >/dev/null && python tools/build.py ${{ inputs.build-options }} --target all ${{ matrix.arg }} $EX_ARGS"
          else
```
   (single docker run; upload for esp-idf happens in the Membrowse Upload step
   below, same container image.)
3. Membrowse Upload step: drop the `inputs.toolchain != 'esp-idf'` condition;
   add `--ci-pinned-boards-only`:

```yaml
      - name: Membrowse Upload
        if: inputs.upload-membrowse == true
        continue-on-error: true
        env:
          MEMBROWSE_API_KEY: ${{ secrets.MEMBROWSE_API_KEY }}
        run: |
          # code-changed false -> no elf -> membrowse uploads with --identical.
          # --ci-pinned-boards-only: families without a pinned board were built as compile
          # smoke-checks only and must not upload.
          BUILD_PY_ARGS="-s ${{ inputs.build-system }} ${{ steps.setup-toolchain.outputs.build_option }} ${{ inputs.build-options }}"
          CMD="python tools/build.py $BUILD_PY_ARGS --ci-pinned-boards-only --target examples-membrowse-upload -j 1 ${{ matrix.arg }} $EX_ARGS"
          if [ "${{ inputs.toolchain }}" == "esp-idf" ]; then
            docker run --rm -e MEMBROWSE_API_KEY="$MEMBROWSE_API_KEY" -e CI="$CI" -v $PWD:/project -w /project espressif/idf:tinyusb bash -c "pip install membrowse >/dev/null && $CMD"
          else
            $CMD
          fi
        shell: bash
```
   Keep (rewrite) the existing comment about `$EX_ARGS`/aggregate semantics.
4. Delete the `Upload Artifacts for Metrics` step entirely.

- [ ] **Step 2: `build.yml` cmake job**

- Toolchain matrix: uncomment `- 'esp-idf'`.
- `build-options`: change `'--one-first'` to
  `'--ci-pinned-boards .github/ci-pinned-boards.json'`.
- Delete the `upload-metrics: true` line.
- Delete the whole `code-metrics` job (lines ~261-389).
- In `set-matrix`: delete the `build_families_regex` output (line ~30) and the
  `FAM_REGEX` computation block inside `Generate matrix json` (the
  `FAM_REGEX=...`/`case "$FAM_REGEX"` block and its
  `echo "build_families_regex=..." >> $GITHUB_OUTPUT` line ~213). Verify
  no other consumer: `grep -rn "build_families_regex" .github/ .circleci/`
  must return only lines you are deleting.
- check-paths `code:` filter: remove `- 'tools/metrics.py'`; add
  `- '.github/ci-pinned-boards.json'`.

- [ ] **Step 3: `pr_comment.yml`**

Delete the `metrics-comment` job (~lines 46-70). If the workflow's remaining
jobs reference it in `needs:`, fix those. `grep -n "metrics" .github/workflows/pr_comment.yml`
must come back empty afterwards.

- [ ] **Step 4: Deletions**

```bash
git rm .github/workflows/membrowse-onboard.yml .github/scripts/metrics_pair_compare.py
```
Then `grep -rn "metrics_pair_compare\|membrowse-onboard\|tinyusb_metrics\|upload-metrics" .github/ tools/ examples/ .circleci/ docs/`
— every remaining hit must be either this plan/spec or a line you still need
to fix. Also `grep -rn "metrics" .claude/skills/make-release/` — if the
release skill references release-asset `metrics.json`, remove that passage.

- [ ] **Step 5: Espressif membrowse targets**

`family_add_membrowse` is only called from `family_configure_common`
(`hw/bsp/family_support.cmake:524`), which `hw/bsp/espressif/family.cmake`
does NOT use. Locate espressif's per-example configure function
(`grep -n "^function(family_configure" hw/bsp/espressif/family.cmake`) and add
at its end:

```cmake
  family_add_membrowse(${TARGET})
```

Then verify locally (host has `IDF_PATH`):

```bash
. "$IDF_PATH/export.sh"
python3 tools/build.py --ci-pinned-boards .github/ci-pinned-boards.json --ci-pinned-boards-only \
        -e device/cdc_msc_freertos espressif
# then the no-upload dry-run target in the build dir it produced:
cmake --build cmake-build/cmake-build-espressif_s3_devkitm/device/cdc_msc_freertos --target cdc_msc_freertos-membrowse
```
Expected: membrowse prints a local report. If the ninja linker-script
extraction inside `family_add_membrowse` finds no `.ld` (IDF links through
response files), fall back per the spec: extend the espressif branch to pass
IDF's known scripts explicitly —
`$<TARGET_FILE_DIR>/../esp-idf/esp_system/ld/memory.ld` + `sections.ld` — by
setting a `MEMBROWSE_LD_OVERRIDE` variable before calling
`family_add_membrowse` and teaching that function to use it verbatim when set:

```cmake
  # in family_add_membrowse, before composing MEMBROWSE_LD_SCRIPTS_CMD:
  if (DEFINED MEMBROWSE_LD_OVERRIDE)
    set(MEMBROWSE_LD_SCRIPTS_CMD "ld_scripts=\"${MEMBROWSE_LD_OVERRIDE}\"")
  endif ()
```

- [ ] **Step 6: Validate YAML and commit**

```bash
pre-commit run --all-files
python3 -c "import yaml,glob; [yaml.safe_load(open(f)) for f in glob.glob('.github/workflows/*.yml')]; print('YAML OK')"
git add -A .github/workflows .github/scripts hw/bsp/espressif/family.cmake hw/bsp/family_support.cmake
git commit -m "ci: membrowse-only size analytics - drop linkermap metrics pipeline, gate uploads on pinned boards"
```

---

### Task 7: membrowse skill + code-size skill update

**Files:**
- Create: `.claude/skills/membrowse/SKILL.md`
- Modify: `.claude/skills/code-size/SKILL.md` (engine default + fallback note)

**Interfaces:**
- Consumes: `metrics_compare_base.py --engine`, `ci-pinned-boards.json`, `<target>-membrowse` / `examples-membrowse-upload` cmake targets, `membrowse report/onboard/summary` CLI.

- [ ] **Step 1: Write `.claude/skills/membrowse/SKILL.md`**

Frontmatter + body (fill the `membrowse onboard` flag list from
`membrowse onboard --help` output at write time — run it, do not guess):

```markdown
---
name: membrowse
description: Use when analyzing firmware memory footprint with membrowse — local
  size reports for an elf, base-vs-branch size diffs, uploading pinned targets
  to the membrowse dashboard, backfilling history (onboard), or editing
  .github/ci-pinned-boards.json (pinned boards covering all dcd/hcd drivers).
---

# Membrowse Size Analytics

Membrowse is TinyUSB's first-class size-analytics system. CI uploads every
example of the PINNED boards in `.github/ci-pinned-boards.json` on each
master push; the membrowse PR comment (membrowse-comment.yml) is the size
feedback on PRs. Target names are `<board>/<example>` — never change them.

## Local report (no API key)

    membrowse report <elf> [--json] [--all-symbols] [--map-file <elf>.map]

Or via cmake per example: `cmake --build <dir> --target <example>-membrowse`
(extracts linker scripts from ninja automatically).

## Base-vs-branch size diff (default engine of the code-size skill)

    python3 tools/metrics_compare_base.py -b <board> [-e device/cdc_msc] [--engine linkermap]

membrowse engine is the default; `--engine linkermap` is the legacy fallback
(needs `python3 tools/get_deps.py` for tools/linkermap).

## Pinned targets

`.github/ci-pinned-boards.json`: one entry per pinned board with the drivers
it covers; `uncovered` documents drivers with no CI-buildable board. The
pre-commit hook `membrowse-targets` enforces that every dcd/hcd driver (plus
ehci/ohci) is covered or documented. To pin a new board: add the entry, run
`pre-commit run membrowse-targets --all-files`, and check the board builds:
`python3 tools/build.py --ci-pinned-boards .github/ci-pinned-boards.json --ci-pinned-boards-only -e device/cdc_msc <family>`.

## Upload (needs MEMBROWSE_API_KEY)

CI-only under normal operation (build_util.yml). Manual:
`python3 tools/build.py --ci-pinned-boards .github/ci-pinned-boards.json --ci-pinned-boards-only --target examples-membrowse-upload -j 1 <family>`

## History backfill (onboard)

For a newly pinned target, history starts at its first master upload. To
backfill: `membrowse onboard ...` — [INSERT the real flags from
`membrowse onboard --help` here when writing this file]. Run per board and
example with target name `<board>/<example>`.
```

- [ ] **Step 2: Update the code-size skill**

In `.claude/skills/code-size/SKILL.md`: state that the default engine is now
membrowse local reports, `--engine linkermap` is the fallback (and the only
mode needing the `tools/linkermap` dep), and map.json generation now happens
via the explicit `examples-linkermap` target (the tool does this itself — no
user action changed). Smallest-possible diff: touch only sentences that are
now wrong (repo rule: no bulk harness-doc edits).

- [ ] **Step 3: Verify skill loads and commit**

Run: `head -12 .claude/skills/membrowse/SKILL.md` — frontmatter must have only
`name` and `description`, description under ~500 chars.

```bash
git add .claude/skills/membrowse/SKILL.md .claude/skills/code-size/SKILL.md
git commit -m "skills: add membrowse skill; code-size defaults to membrowse engine"
```

---

### Task 8: Final validation sweep

**Files:** none new — verification only, plus `CLAUDE.md` if stale references exist.

- [ ] **Step 1: Grep for leftovers**

```bash
grep -rn "tinyusb_metrics\|upload-metrics\|metrics_pair_compare\|membrowse-onboard\|build_families_regex" \
    --include="*.yml" --include="*.py" --include="*.cmake" --include="CMakeLists.txt" \
    .github .circleci tools examples hw test | grep -v cmake-build
```
Expected: empty. Also check `grep -n "metrics" CLAUDE.md .claude/skills/code-size/SKILL.md`
— update any sentence describing the removed CI pipeline.

- [ ] **Step 2: Full pre-commit**

Run: `pre-commit run --all-files`
Expected: all hooks pass (~55 s; HIL hooks exercise real timeouts).

- [ ] **Step 3: Full board build**

```bash
cd examples
cmake -B cmake-build-stm32f407disco -DBOARD=stm32f407disco -G Ninja -DCMAKE_BUILD_TYPE=MinSizeRel . \
  && cmake --build cmake-build-stm32f407disco
```
Expected: all examples build; no linkermap output in the log.

- [ ] **Step 4: Selector + pins test suites once more**

```bash
python3 test/hil/test/test_ci_select.py && python3 test/hil/test/test_ci_metrics.py \
  && python3 test/hil/test/test_membrowse_targets.py && python3 test/hil/test/test_board_pins.py \
  && python3 test/hil/test/test_membrowse_compare.py
```
Expected: all pass.

- [ ] **Step 5: Commit any doc fixes; report ready-to-push**

```bash
git add -A && git commit -m "docs: align remaining references with membrowse-only metrics" || true
git log --oneline master..HEAD
```
Do NOT push — report the branch as ready and wait for the maintainer
(repo rule: never push unless asked). The PR run itself is the remaining
validation: watch `build.yml` (upload gating + esp-idf lane) once the
maintainer opens the PR.
