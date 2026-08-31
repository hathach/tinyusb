# PR-Scoped CI Selection Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Promote `test/hil/helper/hil_select.py` to a repo-wide `tools/ci_select.py` whose one classification of a PR diff narrows three CI axes — build families, per-family example targets, and per-board HIL examples — wired into both GitHub Actions and CircleCI.

**Architecture:** The selector gains an independent build classifier beside the untouched HIL one (17-rule table in the spec). `ci_set_matrix.py` filters the family matrix from the selector JSON; the per-family example map travels as a side channel (GHA job output / CircleCI pipeline parameter), resolved to `-e` flags per build job by a new `tools/build.py --example` filter. `hil_ci_set_matrix.py` appends `-e` per rig board. Code metrics gain per-example artifacts and a (family, example)-intersection compare.

**Tech Stack:** Python 3 stdlib (selector must run on bare CI runners), GitHub Actions YAML, CircleCI dynamic config (continuation orb), jq, CMake/Ninja.

**Spec:** `docs/superpowers/specs/2026-08-19-ci-build-family-filter-design.md` — read it first; every rule number below refers to its rule table.

## Global Constraints

- Commit messages: imperative mood, **no** `Co-Authored-By:` or `Claude-Session:` trailers (hathach is sole author — this overrides harness defaults).
- Never stage or touch `.idea/`. Always `git add` explicit paths, never `-A`.
- Bare-runner Python modules (`tools/ci_select.py`, `tools/build.py`, `tools/build_utils.py`, everything under `test/hil/helper/`) stay stdlib-only at module level — `test_hil_util.BottomLayer` enforces this; extend its lists, never work around them.
- `ci_select.py` stdout is machine-read JSON; every diagnostic goes to stderr.
- The family reference scan is **CMake-only** (`family.cmake` + espressif component `CMakeLists.txt`, never `family.mk`): CMake is the first-class build system, Make follows it.
- Fail-open everywhere: a selector/matrix-script failure must yield the full matrix, never a red job or a silently-empty one.
- Python style: match the existing modules (4-space indent in tools/ and test/hil/, terse targeted comments explaining *why*).
- YAML: 2-space indent, match surrounding style in `.github/workflows/` and `.circleci/`.
- Run suites from the repo root. Selector suite: `python3 test/hil/test/test_ci_select.py` (after Task 1). Full HIL-side suite: `python3 -m unittest discover -s test/hil/test`.

---

### Task 1: Move the selector to `tools/ci_select.py` (mechanical, no behavior change)

**Files:**
- Move: `test/hil/helper/hil_select.py` → `tools/ci_select.py` (git mv)
- Move: `test/hil/test/test_hil_select.py` → `test/hil/test/test_ci_select.py` (git mv)
- Modify: `test/hil/test/test_hil_util.py` (BottomLayer lists), `test/hil/hil_ci.sh` (scp list), `.pre-commit-config.yaml` (both hooks), `.github/workflows/build.yml` (4 path refs), `.claude/skills/pre-pr/SKILL.md`, `test/hil/helper/hil_util.py:21` (comment), `test/hil/hil_flash.py:297` (comment)

**Interfaces:**
- Produces: module `tools/ci_select.py` importable as `ci_select` with `tools/` on `sys.path`; module attribute `_REPO_ROOT` (absolute repo root); CLI `python3 tools/ci_select.py --base REF|--diff-file F CONFIG.json...` — output JSON byte-compatible with today's `hil_select.py`.
- Consumes: `test/hil/helper/hil_util.py` rosters (unchanged).

- [ ] **Step 1: git mv both files**

```bash
git mv test/hil/helper/hil_select.py tools/ci_select.py
git mv test/hil/test/test_hil_select.py test/hil/test/test_ci_select.py
```

- [ ] **Step 2: Fix `tools/ci_select.py` imports and repo root**

Replace the current path setup (line 24, `sys.path.insert(0, os.path.dirname(os.path.dirname(...)))` and its comment) with:

```python
# tools/ -> repo root is ONE level up. Guarded by TestModuleMove.test_repo_root_guard:
# a wrong parent count here silently re-points every repo-relative glob (it happened
# at the helper/ move).
_REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(_REPO_ROOT, 'test', 'hil'))  # for `from helper...`
from helper.hil_util import device_tests, dual_tests, host_test
```

In `main()`, replace the 4-level `repo_root` derivation (lines 503-505) with `repo_root = _REPO_ROOT`. Change the stderr prefix at line 519 from `hil_select:` to `ci_select:`. Update the module docstring: it now lives in `tools/`, serves HIL and (from Task 3) build selection; keep the fail-open sentence and the spec pointer, adding this spec's path.

- [ ] **Step 3: Fix `test/hil/test/test_ci_select.py` imports**

Replace the header import block (`from helper import hil_select`) so `REPO` is computed first, then:

```python
REPO = os.path.dirname(os.path.dirname(os.path.dirname(
    os.path.dirname(os.path.abspath(__file__)))))
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))  # test/hil, for hil_flash/helper
sys.path.insert(0, os.path.join(REPO, 'tools'))
import hil_flash
import ci_select
from helper.hil_util import device_tests, dual_tests
```

Then `sed -i 's/\bhil_select\b/ci_select/g' test/hil/test/test_ci_select.py` and fix the header comment (file names, run command). Add the guard test:

```python
class TestModuleMove(unittest.TestCase):
    def test_repo_root_guard(self):
        # __file__-derived root: moving the module without re-deriving the parent
        # count re-points every scan at the wrong tree (it happened once already)
        self.assertTrue(os.path.isdir(os.path.join(ci_select._REPO_ROOT, 'src')))
        self.assertTrue(os.path.isdir(os.path.join(ci_select._REPO_ROOT, 'hw', 'bsp')))
        self.assertEqual(os.path.realpath(ci_select._REPO_ROOT), os.path.realpath(REPO))
```

- [ ] **Step 4: Update every reference**

- `test/hil/test/test_hil_util.py` BottomLayer: in `test_bare_runner_modules_stay_stdlib_only`, replace `'hil_select'` with `'ci_select'` in the `local` set and replace `'helper/hil_select'` with `'../../tools/ci_select'` in the module-path tuple (the loop builds `hil_dir / f'{mod}.py'`, so a relative path out of test/hil works). Update the docstring sentence naming hil_select.
- `test/hil/hil_ci.sh`: delete the `"$ROOT_DIR/test/hil/helper/hil_select.py" \` scp line (nothing on the rig imports it).
- `.pre-commit-config.yaml`: rename hook `hil-select-test` → `ci-select-test`; `entry: python3 test/hil/test/test_ci_select.py`; `files: ^(hw/bsp/|src/|examples/|tools/ci_select\.py$)`. In the `hil-test` hook comment, s/test_hil_select/test_ci_select/.
- `.github/workflows/build.yml`: four call sites — lines ~82/84 (set-matrix) and ~632/637 (hil-hfp-iar): `test/hil/test/test_hil_select.py` → `test/hil/test/test_ci_select.py`, `test/hil/helper/hil_select.py` → `tools/ci_select.py`; s/hil_select/ci_select/ in the adjacent `::warning::` strings and comments (keep `hil_select.json` file names as `ci_select.json` for consistency — update both writers and both readers in the hfp-iar job).
- `.claude/skills/pre-pr/SKILL.md`: `python3 test/hil/helper/hil_select.py` → `python3 tools/ci_select.py`.
- Comments only: `test/hil/helper/hil_util.py:21` (hil_select → ci_select), `test/hil/hil_flash.py:297` (test_hil_select → test_ci_select).

- [ ] **Step 5: Verify**

```bash
python3 test/hil/test/test_ci_select.py          # all pass
python3 -m unittest discover -s test/hil/test     # all pass (~55 s)
python3 tools/ci_select.py --diff-file /dev/null test/hil/tinyusb.json | python3 -m json.tool >/dev/null
grep -rn "hil_select" --include='*.py' --include='*.yml' --include='*.yaml' --include='*.sh' --include='*.md' . | grep -v docs/superpowers | grep -v '\.worktrees'
```

Expected: suites green; last grep returns nothing (historical spec docs are the only allowed hits).

- [ ] **Step 6: Commit**

```bash
git add tools/ci_select.py test/hil/test/test_ci_select.py test/hil/test/test_hil_util.py \
  test/hil/hil_ci.sh .pre-commit-config.yaml .github/workflows/build.yml \
  .claude/skills/pre-pr/SKILL.md test/hil/helper/hil_util.py test/hil/hil_flash.py
git commit -m "tools: promote hil_select.py to tools/ci_select.py"
```

---

### Task 2: Generalize the family scan and re-rule `hw/mcu/**` (HIL side)

**Files:**
- Modify: `tools/ci_select.py` (`port_families` → `path_families` + `mcu_families`, `_FULL_RE`, `_classify_one`)
- Test: `test/hil/test/test_ci_select.py`

**Interfaces:**
- Produces: `path_families(rel_dir: str, repo_root: str) -> set[str]` — families whose `family.cmake`/espressif component CMakeLists reference `rel_dir` at a directory boundary; `mcu_families(path: str, repo_root: str) -> set[str]` — longest-resolving-prefix lookup for a changed `hw/mcu/...` path; `port_families(port_dir, repo_root)` kept as a thin wrapper (existing callers/tests unchanged).
- HIL JSON change: `hw/mcu/**` no longer forces `full: true`; it selects the resolved families' boards, all their tests (spec rule 7).

- [ ] **Step 1: Write the failing tests** (append to `test_ci_select.py`)

```python
class TestPathFamilies(unittest.TestCase):
    def test_port_wrapper_unchanged(self):
        self.assertEqual(ci_select.port_families('raspberrypi/rp2040', REPO), {'rp2040'})
        self.assertIn('stm32f4', ci_select.port_families('synopsys/dwc2', REPO))

    def test_boundary_without_trailing_slash(self):
        # hw/bsp/nrf/family.cmake writes `${TOP}/hw/mcu/nordic/nrfx` — no trailing
        # slash; the match must accept a directory-boundary end-of-token
        self.assertEqual(ci_select.path_families('hw/mcu/nordic/nrfx', REPO), {'nrf'})

    def test_boundary_rejects_prefix_sibling(self):
        # 'microchip/pic' must not inherit pic32mz's references (and pic32mz itself
        # is family.mk-only, which the CMake-only scan never reads)
        self.assertEqual(ci_select.port_families('microchip/pic', REPO), set())
        self.assertEqual(ci_select.port_families('microchip/pic32mz', REPO), set())

    def test_mcu_families_prefix_walk(self):
        self.assertEqual(ci_select.mcu_families('hw/mcu/nordic/nrf5x/nrf_clock.h', REPO), {'nrf'})
        self.assertEqual(ci_select.mcu_families('hw/mcu/dialog/da1469x/x.h', REPO), {'da1469x'})
        self.assertEqual(ci_select.mcu_families('hw/mcu/no_such_vendor/x.c', REPO), set())


class TestMcuHilRule(unittest.TestCase):
    def test_mcu_no_longer_forces_full(self):
        s = ci_select.classify(['hw/mcu/nordic/nrf5x/nrf_clock.h'], REPO, ROSTERS)
        self.assertFalse(s['full'])
        self.assertIn('nrf', s['families'])   # recorded even with no nrf rig board

    def test_mcu_selects_family_boards(self):
        got = on_roster(self, 'feather_nrf52840_express', 'pca10056', 'pca10095')
        s = ci_select.classify(['hw/mcu/nordic/nrf5x/nrf_clock.h'], REPO, real_rosters())
        self.assertFalse(s['full'])
        for b in got:
            self.assertIn(b, s['boards'])


class TestOrphanInvariant(unittest.TestCase):
    ALLOW = {'microchip/pic', 'microchip/pic32mz'}   # spec: known orphans, CMake builds neither

    def test_every_port_resolves_to_a_family(self):
        for d in sorted(glob.glob(os.path.join(REPO, 'src/portable/*/*'))):
            if not os.path.isdir(d):
                continue
            port = os.path.relpath(d, os.path.join(REPO, 'src/portable')).replace(os.sep, '/')
            fams = ci_select.port_families(port, REPO)
            if port in self.ALLOW:
                self.assertEqual(fams, set(), f'{port}: no longer an orphan - drop it from ALLOW')
            else:
                self.assertTrue(fams, f'{port}: no family.cmake references it - wire it up or allowlist it')

    def test_tracked_mcu_vendors_resolve(self):
        import subprocess as sp
        r = sp.run(['git', 'ls-files', 'hw/mcu'], cwd=REPO, capture_output=True, text=True)
        if r.returncode != 0:
            self.skipTest('not a git checkout')
        vendors = sorted({'/'.join(p.split('/')[:3]) for p in r.stdout.split()})
        for v in vendors:
            self.assertTrue(ci_select.mcu_families(v + '/x.c', REPO), f'{v}: resolves to no family')
```

- [ ] **Step 2: Run to verify failure**

Run: `python3 test/hil/test/test_ci_select.py TestPathFamilies -v`
Expected: FAIL/ERROR — `path_families`/`mcu_families` not defined.

- [ ] **Step 3: Implement**

In `tools/ci_select.py`, replace `port_families` with:

```python
@functools.lru_cache(maxsize=None)
def path_families(rel_dir: str, repo_root: str) -> set:
    """Board families whose family.cmake (or espressif component CMakeLists)
    references rel_dir at a directory boundary. CMake only, on every axis: CMake
    is the first-class build system and Make follows it, so family.mk is never
    read - a port wired up in family.mk alone (microchip/pic32mz) is built by no
    CI job and resolves to nothing. Boundary = '/', whitespace, quote, paren,
    brace or end: `${TOP}/hw/mcu/nordic/nrfx` has no trailing slash, while bare
    'microchip/pic' must not match '.../microchip/pic32mz/...'."""
    fams = set()
    bsp_root = os.path.join(repo_root, 'hw/bsp')
    pat = re.compile(re.escape(rel_dir) + r'(?=[/\s"\')}]|$)', re.M)
    for f in glob.glob(os.path.join(bsp_root, '*/family.cmake')) + \
             glob.glob(os.path.join(bsp_root, '*/components/*/CMakeLists.txt')):
        try:
            if pat.search(open(f).read()):
                fams.add(os.path.relpath(f, bsp_root).split(os.sep, 1)[0])
        except OSError:
            pass
    return fams


def port_families(port_dir: str, repo_root: str) -> set:
    return path_families('src/portable/' + port_dir, repo_root)


def mcu_families(path: str, repo_root: str) -> set:
    """Families referencing a changed hw/mcu path: longest resolving dir prefix,
    hw/mcu/<vendor>/<sub>/... down to hw/mcu/<vendor>."""
    parts = path.split('/')
    for n in range(len(parts) - 1, 2, -1):
        fams = path_families('/'.join(parts[:n]), repo_root)
        if fams:
            return fams
    return set()
```

Keep the old docstring's CMake-only rationale for HIL (folded into the new one). Remove `hw/mcu/|` from `_FULL_RE`. In `_classify_one`, insert after the `hw/bsp/` block, before the `examples/` block:

```python
    if re.match(r'hw/mcu/', path):
        fams = mcu_families(path, repo_root)
        s.families.update(fams)
        boards = [b['name'] for b in roster_boards
                  if board_family(b['name'], repo_root) in fams]
        s.roles.update(('device', 'host'))
        s.add(boards, 'all', f'{path}: mcu dir -> families {sorted(fams)} -> boards {boards}')
        return
```

- [ ] **Step 4: Run tests**

Run: `python3 test/hil/test/test_ci_select.py -v 2>&1 | tail -5`
Expected: all pass (the pre-existing port tests exercise the wrapper).

- [ ] **Step 5: Commit**

```bash
git add tools/ci_select.py test/hil/test/test_ci_select.py
git commit -m "ci_select: generalize family scan to hw/mcu, drop hw/mcu from HIL full-matrix rule"
```

---

### Task 3: Build classifier — rules 1-17, raw two-axis selection

**Files:**
- Modify: `tools/ci_select.py`
- Test: `test/hil/test/test_ci_select.py`

**Interfaces:**
- Produces: `classify_build(changed_files, repo_root) -> dict` with keys `full: bool`, `families: [str]` (sorted bsp-dir names), `family_examples: {family: [example]}` (key absent ⇒ that family builds all examples; examples as `role/name`), `reasons: [str]`. Also `all_examples(repo_root) -> tuple[str]`, `role_examples(repo_root, roles) -> set[str]`, `all_bsp_families(repo_root) -> list[str]`. Buildability pruning is Task 4 — this task emits the raw rule output.
- Consumes: `path_families`, `mcu_families`, `class_macros`, `class_include_edges`, `_config_enables`, `_NONCODE_RE` (all existing).

- [ ] **Step 1: Write the failing tests**

```python
class TestBuildClassifier(unittest.TestCase):
    def b(self, files):
        return ci_select.classify_build(files, REPO)

    def test_noncode_and_test_hil_contribute_nothing(self):        # rules 1, 2
        s = self.b(['docs/info/index.rst', 'README.rst', 'test/hil/hil_test.py', '.claude/skills/hil/SKILL.md'])
        self.assertFalse(s['full'])
        self.assertEqual(s['families'], [])
        self.assertEqual(s['family_examples'], {})

    def test_port_device_rule(self):                               # rule 3
        s = self.b(['src/portable/raspberrypi/rp2040/dcd_rp2040.c'])
        self.assertFalse(s['full'])
        self.assertEqual(s['families'], ['rp2040'])
        exs = s['family_examples']['rp2040']
        self.assertIn('device/cdc_msc', exs)
        self.assertFalse(any(e.startswith(('host/', 'typec/')) for e in exs))
        # dual inclusion asserted on the pure role helper: whether a dual example
        # survives Task 4's buildability pruning depends on the environment-gated
        # CI board pick, so the classifier-output assertion must not rely on it
        self.assertIn('dual/host_info_to_device_cdc',
                      ci_select.role_examples(REPO, ('device', 'dual')))
        self.assertNotIn('host/bare_api', ci_select.role_examples(REPO, ('device', 'dual')))

    def test_port_host_rule(self):                                 # rule 4
        s = self.b(['src/portable/analog/max3421/hcd_max3421.c'])
        self.assertFalse(s['full'])
        # max3421 is referenced only by the espressif component CMakeLists — and
        # espressif is in no provider's family list, so this may prune to nothing
        self.assertLessEqual(set(s['families']), {'espressif'})
        for exs in s['family_examples'].values():
            self.assertFalse(any(e.startswith(('device/', 'typec/')) for e in exs))

    def test_port_shared_file_selects_all_examples(self):          # rule 5
        s = self.b(['src/portable/synopsys/dwc2/dwc2_common.c'])
        self.assertFalse(s['full'])
        self.assertIn('stm32f4', s['families'])
        self.assertNotIn('rp2040', s['families'])
        self.assertNotIn('stm32f4', s['family_examples'])   # 'all' => no map key

    def test_bsp_family_rule(self):                                # rule 6
        s = self.b(['hw/bsp/stm32f4/boards/stm32f407disco/board.h'])
        self.assertEqual(s['families'], ['stm32f4'])
        self.assertNotIn('stm32f4', s['family_examples'])

    def test_bsp_top_level_file_is_full(self):                     # rule 16
        self.assertTrue(self.b(['hw/bsp/board.c'])['full'])
        self.assertTrue(self.b(['hw/bsp/family_support.cmake'])['full'])

    def test_mcu_rule(self):                                       # rule 7
        s = self.b(['hw/mcu/nordic/nrf5x/nrf_clock.h'])
        self.assertEqual(s['families'], ['nrf'])
        s = self.b(['hw/mcu/no_such_vendor/x.c'])                  # empty means empty
        self.assertFalse(s['full'])
        self.assertEqual(s['families'], [])

    def test_class_device_rule(self):                              # rule 8
        s = self.b(['src/class/cdc/cdc_device.c'])
        self.assertFalse(s['full'])
        # near-all families (Task 4's pruning may drop a few); never equality
        # against all_bsp_families — that's a tuple, and pruning shrinks the list
        self.assertIn('stm32f4', s['families'])
        self.assertGreater(len(s['families']), 50)
        exs = s['family_examples']['stm32f4']
        self.assertIn('device/cdc_msc', exs)
        self.assertNotIn('device/hid_composite', exs)
        self.assertNotIn('host/cdc_msc_hid', exs)          # TUH_CDC examples are rule 9's

    def test_class_host_rule(self):                                # rule 9
        s = self.b(['src/class/msc/msc_host.c'])
        exs = s['family_examples']['stm32f4']
        self.assertIn('host/msc_file_explorer', exs)
        self.assertNotIn('device/cdc_msc', exs)

    def test_class_shared_header_and_include_edge(self):           # rule 10
        s = self.b(['src/class/audio/audio.h'])
        exs = s['family_examples']['stm32f4']
        self.assertIn('device/audio_test', exs)
        self.assertIn('device/midi_test', exs)              # midi headers include audio.h

    def test_core_device_rule(self):                               # rule 11
        s = self.b(['src/device/usbd.c'])
        exs = s['family_examples']['stm32f4']
        self.assertIn('device/cdc_msc', exs)
        # no dual In-assertion: dual examples are only.txt-gated to max3421/pio-usb
        # boards, so pruning legitimately drops them on a plain stm32f4 board
        self.assertFalse(any(e.startswith(('host/', 'typec/')) for e in exs))

    def test_core_host_rule(self):                                 # rule 12
        s = self.b(['src/host/usbh.c'])
        exs = s['family_examples']['stm32f4']
        self.assertFalse(any(e.startswith(('device/', 'typec/')) for e in exs))

    def test_example_rule(self):                                   # rules 13, 14
        s = self.b(['examples/device/cdc_msc/src/main.c'])
        self.assertEqual(s['family_examples']['stm32f4'], ['device/cdc_msc'])
        s = self.b(['examples/device/board_test/src/main.c'])
        self.assertEqual(s['family_examples']['stm32f4'], ['device/board_test'])
        s = self.b(['examples/device/no_such_example/src/main.c'])  # deleted example: nothing
        self.assertFalse(s['full'])
        self.assertEqual(s['families'], [])

    def test_full_paths(self):                                     # rules 15-17
        for p in ('src/common/tusb_fifo.c', 'src/osal/osal.h', 'src/tusb.c',
                  'src/tusb_option.h', 'lib/SEGGER_RTT/RTT/SEGGER_RTT.c',
                  'tools/build.py', 'tools/get_deps.py', 'tools/cmake/cpu/cortex-m4.cmake',
                  'examples/CMakeLists.txt', 'examples/device/CMakeLists.txt',
                  'examples/build_system/cmake/cpu.cmake', '.github/workflows/build.yml',
                  'sonar-project.properties', 'some/unknown/path.c'):
            self.assertTrue(self.b([p])['full'], p)

    def test_mixed_diff_unions_per_family(self):
        s = self.b(['src/portable/raspberrypi/rp2040/dcd_rp2040.c', 'src/class/cdc/cdc_device.c'])
        self.assertFalse(s['full'])
        self.assertIn('stm32f4', s['families'])
        self.assertGreater(len(s['families']), 50)
        self.assertIn('device/hid_composite', s['family_examples']['rp2040'])   # from the dcd rule
        self.assertNotIn('device/hid_composite', s['family_examples']['stm32f4'])  # cdc-only there

    def test_example_names_are_real_dirs(self):
        for ex in ci_select.all_examples(REPO):
            role, name = ex.split('/')
            self.assertTrue(os.path.isdir(os.path.join(REPO, 'examples', role, name)), ex)
            self.assertRegex(ex, r'^(device|dual|host|typec)/[A-Za-z0-9_]+$')
```

Note for `test_mixed_diff_unions_per_family`: it encodes the per-family union — rp2040 gets DEV+DUAL ∪ cdc-set, every other family only the cdc-set (spec §Two axes). Buildability pruning may later remove entries; these Task-3 tests use families/examples that survive pruning (stm32f4 and rp2040 build all the named examples), so they stay valid after Task 4.

- [ ] **Step 2: Run to verify failure**

Run: `python3 test/hil/test/test_ci_select.py TestBuildClassifier -v 2>&1 | tail -3`
Expected: ERROR — `classify_build` not defined.

- [ ] **Step 3: Implement** (append to `tools/ci_select.py`, after the HIL classifier)

```python
# -------------------------------------------------------------
# Build-axis classifier (spec rule table, docs/superpowers/specs/
# 2026-08-19-ci-build-family-filter-design.md). Independent of the HIL
# classifier: same diff, second walk, its own fail-open.
# -------------------------------------------------------------
_EX_ROLES = ('device', 'dual', 'host', 'typec')


@functools.lru_cache(maxsize=None)
def all_examples(repo_root: str) -> tuple:
    """Every examples/<role>/<name> with a CMakeLists.txt, as 'role/name'."""
    out = []
    for role in _EX_ROLES:
        for d in sorted(glob.glob(os.path.join(repo_root, 'examples', role, '*/'))):
            if os.path.isfile(os.path.join(d, 'CMakeLists.txt')):
                out.append(f'{role}/{os.path.basename(d.rstrip(os.sep))}')
    return tuple(out)


def role_examples(repo_root: str, roles) -> set:
    want = set(roles)
    return {e for e in all_examples(repo_root) if e.split('/', 1)[0] in want}


@functools.lru_cache(maxsize=None)
def all_bsp_families(repo_root: str) -> tuple:
    return tuple(sorted(d for d in os.listdir(os.path.join(repo_root, 'hw/bsp'))
                        if os.path.isdir(os.path.join(repo_root, 'hw/bsp', d))))


def _build_class_examples(cls: str, base: str, roles: set, repo_root: str) -> set:
    """Examples (all 46, not the HIL lists) whose tusb_config.h enables the class's
    macros for the given roles, plus classes that #include the changed header."""
    via = sorted(class_include_edges(repo_root).get(f'{cls}/{base}', ()))
    out = set()
    for prefix, role in (('TUD', 'device'), ('TUH', 'host')):
        if role not in roles:
            continue
        macros = class_macros(cls, base, prefix) + \
                 [m for c in via for m in class_macros(c, '', prefix)]
        for ex in all_examples(repo_root):
            cfg = os.path.join(repo_root, 'examples', ex, 'src', 'tusb_config.h')
            if _config_enables(cfg, macros):
                out.add(ex)
    return out


class _BSel:
    """family -> set(examples) | 'all', unioned per family."""
    def __init__(self):
        self.full = False
        self.fam_ex = {}
        self.reasons = []

    def add(self, fams, examples, reason):
        self.reasons.append(reason)
        for f in fams:
            cur = self.fam_ex.get(f)
            if examples == 'all' or cur == 'all':
                self.fam_ex[f] = 'all'
            else:
                self.fam_ex[f] = (cur or set()) | set(examples)

    def force_full(self, reason):
        self.full = True
        self.reasons.append(reason)


def _classify_build_one(path, repo_root, s: _BSel):
    base = os.path.basename(path)
    if _NONCODE_RE.match(path):                                   # rule 1
        return
    if re.match(r'test/hil/', path):                              # rule 2
        s.reasons.append(f'{path}: HIL harness, no build contribution')
        return
    m = re.match(r'src/portable/((?:[^/]+/)?[^/]+)/', path)
    if m:                                                         # rules 3-5
        port = m.group(1)
        fams = port_families(port, repo_root)
        if re.match(r'(dcd_|.*_device)', base):
            exs = role_examples(repo_root, ('device', 'dual'))
        elif re.match(r'(hcd_|.*_host)', base):
            exs = role_examples(repo_root, ('host', 'dual'))
        else:
            exs = 'all'
        s.add(fams, exs, f'{path}: port {port} -> families {sorted(fams)}')
        return
    if re.match(r'hw/bsp/[^/]+/', path):                          # rule 6
        fam = path.split('/')[2]
        s.add({fam}, 'all', f'{path}: bsp family {fam}')
        return
    if re.match(r'hw/mcu/', path):                                # rule 7
        fams = mcu_families(path, repo_root)
        s.add(fams, 'all', f'{path}: mcu -> families {sorted(fams)}')
        return
    m = re.match(r'src/class/([^/]+)/', path)
    if m:                                                         # rules 8-10
        cls = m.group(1)
        if re.search(r'_device\.[ch]$', base):
            roles = {'device'}
        elif re.search(r'_host\.[ch]$', base):
            roles = {'host'}
        else:
            roles = {'device', 'host'}
        exs = _build_class_examples(cls, base, roles, repo_root)
        s.add(all_bsp_families(repo_root), exs,
              f'{path}: class {cls} -> {sorted(exs)}')
        return
    m = re.match(r'src/(device|host)/', path)
    if m:                                                         # rules 11-12
        role = m.group(1)
        s.add(all_bsp_families(repo_root), role_examples(repo_root, (role, 'dual')),
              f'{path}: core {role} stack')
        return
    m = re.match(r'examples/(device|dual|host|typec)/([^/]+)/', path)
    if m:                                                         # rules 13-14
        ex = f'{m.group(1)}/{m.group(2)}'
        if ex in all_examples(repo_root):
            s.add(all_bsp_families(repo_root), {ex}, f'{path}: example {ex}')
        else:
            # a deleted example builds nothing; removing it from the role
            # CMakeLists (rule 15) is what forces the full matrix
            s.reasons.append(f'{path}: not an example dir, no build contribution')
        return
    s.force_full(f'{path}: unclassified -> full build matrix')    # rules 15-17


def classify_build(changed_files, repo_root):
    s = _BSel()
    for p in changed_files:
        _classify_build_one(p, repo_root, s)
    if s.full:
        return {'full': True, 'families': list(all_bsp_families(repo_root)),
                'family_examples': {}, 'reasons': s.reasons}
    fams, fam_ex = [], {}
    for fam, exs in sorted(s.fam_ex.items()):
        fams.append(fam)
        if exs != 'all':
            fam_ex[fam] = sorted(exs)
    return {'full': False, 'families': fams, 'family_examples': fam_ex,
            'reasons': s.reasons}
```

Note: `examples/<role>/CMakeLists.txt` has no trailing slash after the second component, so the example regex misses it and it correctly falls through to `force_full` (rule 15) — `test_full_paths` pins this.

- [ ] **Step 4: Run tests**

Run: `python3 test/hil/test/test_ci_select.py TestBuildClassifier -v`
Expected: all pass. Then the full file: `python3 test/hil/test/test_ci_select.py 2>&1 | tail -3` — all pass.

- [ ] **Step 5: Commit**

```bash
git add tools/ci_select.py test/hil/test/test_ci_select.py
git commit -m "ci_select: add build-axis classifier (families x example targets)"
```

---

### Task 4: Buildability post-filter, `build` + `hil_examples` output keys

**Files:**
- Modify: `tools/ci_select.py` (imports, post-filter, `main()`), `test/hil/test/test_hil_util.py` (BottomLayer lists)
- Test: `test/hil/test/test_ci_select.py`

**Interfaces:**
- Produces: `classify_build` result is now pruned: each family's list intersected with what that family's CI board can build (`build_utils.skip_example`); family dropped when nothing survives; map key omitted when the kept set equals everything the board can build. `hil_examples(sel, rosters) -> {board: [example]}` — the board's selected tests (`sel['boards'][name]` when narrowed, else `board_tests`) plus always `device/board_test`. CLI JSON gains top-level `"build": {...}` (always) and `"hil_examples": {...}` (when rosters given; emitted even when `full` is true).
- Consumes: `tools/build_utils.skip_example(example, board)`; `tools/build.py:get_family_boards(family, one_random, one_first)` (module import — no behavior change to build.py yet).

- [ ] **Step 1: Write the failing tests**

```python
class TestBuildPostFilter(unittest.TestCase):
    def test_kept_examples_are_buildable(self):
        import build_utils, build as build_py
        s = ci_select.classify_build(['src/class/msc/msc_host.c'], REPO)
        self.assertFalse(s['full'])
        # families that cannot build a single TUH_MSC example drop out entirely
        self.assertNotIn('msp430', s['families'])
        old = os.getcwd()
        os.chdir(REPO)
        try:
            for fam, exs in s['family_examples'].items():
                board = build_py.get_family_boards(fam, False, True)[0]
                for e in exs:
                    self.assertFalse(build_utils.skip_example(e, board), f'{fam}: {e}')
        finally:
            os.chdir(old)

    def test_unfiltered_family_has_no_map_key(self):
        s = ci_select.classify_build(['hw/bsp/stm32f4/family.c'], REPO)
        self.assertEqual(s['families'], ['stm32f4'])
        self.assertEqual(s['family_examples'], {})

    def test_no_stdout_pollution(self):
        # get_family_boards prints on odd families; the selector's stdout is JSON
        import io, contextlib
        buf = io.StringIO()
        with contextlib.redirect_stdout(buf):
            ci_select.classify_build(['src/class/msc/msc_host.c'], REPO)
        self.assertEqual(buf.getvalue(), '')


class TestHilExamples(unittest.TestCase):
    def test_board_test_always_present_and_full_emits(self):
        s = ci_select.classify(['src/common/tusb_fifo.c'], REPO, ROSTERS)  # full
        he = ci_select.hil_examples(s, ROSTERS)
        self.assertEqual(set(he), {b['name'] for b in ROSTER})
        for name, exs in he.items():
            self.assertIn('device/board_test', exs)

    def test_narrowed_board_gets_chosen_tests_only(self):
        s = ci_select.classify(['examples/device/cdc_msc/src/main.c'], REPO, ROSTERS)
        he = ci_select.hil_examples(s, ROSTERS)
        self.assertEqual(he['stm32f407disco'], ['device/board_test', 'device/cdc_msc'])

    def test_full_board_gets_its_whole_test_list(self):
        s = ci_select.classify(['hw/bsp/stm32f4/boards/stm32f407disco/board.h'], REPO, ROSTERS)
        he = ci_select.hil_examples(s, ROSTERS)
        want = set(ci_select.board_tests(ROSTER[1])) | {'device/board_test'}
        self.assertEqual(set(he['stm32f407disco']), want)
        self.assertNotIn('raspberry_pi_pico', he)   # deselected board: no firmware needed


class TestCliJson(unittest.TestCase):
    def test_build_key_without_rosters(self):
        r = subprocess.run([sys.executable, os.path.join(REPO, 'tools/ci_select.py'),
                            '--diff-file', '/dev/null'], capture_output=True, text=True)
        self.assertEqual(r.returncode, 0, r.stderr)
        j = json.loads(r.stdout)
        self.assertIn('build', j)
        self.assertNotIn('hil_examples', j)     # rosters not given

    def test_build_and_hil_keys_with_rosters(self):
        import tempfile
        with tempfile.NamedTemporaryFile('w', suffix='.txt', delete=False) as f:
            f.write('src/portable/raspberrypi/rp2040/dcd_rp2040.c\n')
            df = f.name
        r = subprocess.run([sys.executable, os.path.join(REPO, 'tools/ci_select.py'),
                            '--diff-file', df, os.path.join(REPO, 'test/hil/tinyusb.json')],
                           capture_output=True, text=True)
        os.unlink(df)
        j = json.loads(r.stdout)
        self.assertEqual(j['build']['families'], ['rp2040'])
        self.assertIn('hil_examples', j)
        for exs in j['hil_examples'].values():
            self.assertIn('device/board_test', exs)
```

(`subprocess`, `sys` are already imported in the test file.)

- [ ] **Step 2: Run to verify failure**

Run: `python3 test/hil/test/test_ci_select.py TestBuildPostFilter TestHilExamples TestCliJson -v 2>&1 | tail -3`
Expected: FAIL — no pruning, no `hil_examples`, no `build` key.

- [ ] **Step 3: Implement**

In `tools/ci_select.py` module header, after the existing `helper` import, add:

```python
import contextlib
import io

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))   # tools/, for build helpers
import build_utils
import build as build_py
```

(`contextlib`/`io` go into the stdlib import block at the top.) Add the pruning helpers and rewrite the tail of `classify_build`:

```python
@contextlib.contextmanager
def _in_repo(repo_root):
    """build_utils/build.py use repo-relative paths; scope a chdir around them.
    get_family_boards also prints on an empty family - swallow stdout so the
    selector's machine-read JSON stays clean (diagnostics belong on stderr)."""
    old = os.getcwd()
    os.chdir(repo_root)
    try:
        with contextlib.redirect_stdout(io.StringIO()):
            yield
    finally:
        os.chdir(old)


def _prune_buildable(fams, fam_ex, repo_root):
    """Intersect each family's selection with what its CI board can build
    (build_utils.skip_example - the same skip.txt/only.txt data CMake's
    family_filter reads). get_family_boards mirrors the build jobs' one-first
    pick, CI preferred/skip lists included."""
    out_fams, out_ex = [], {}
    allex = list(all_examples(repo_root))
    with _in_repo(repo_root):
        for fam in fams:
            boards = build_py.get_family_boards(fam, False, True)
            if not boards:
                out_fams.append(fam)         # unknown layout: keep unfiltered
                continue
            board = boards[0]
            buildable = [e for e in allex if not build_utils.skip_example(e, board)]
            want = fam_ex.get(fam)
            kept = buildable if want is None else [e for e in want if e in set(buildable)]
            if not kept:
                continue                     # this diff builds nothing for this family
            out_fams.append(fam)
            if set(kept) != set(buildable):
                out_ex[fam] = kept
    return out_fams, out_ex
```

Replace `classify_build`'s non-full return with:

```python
    fams = sorted(s.fam_ex)
    fam_ex = {f: sorted(e) for f, e in s.fam_ex.items() if e != 'all'}
    fams, fam_ex = _prune_buildable(fams, fam_ex, repo_root)
    return {'full': False, 'families': fams, 'family_examples': fam_ex,
            'reasons': s.reasons}
```

Add `hil_examples` beside `selection_args`:

```python
def hil_examples(sel, rosters):
    """{board: examples hil-build must produce}: the board's selected tests plus
    device/board_test, which hil_test.py flashes to park at every variant
    boundary and at end-of-board teardown. Emitted for full selections too - the
    HIL example universe is a fraction of the tree regardless of the diff."""
    by_name = {}
    for _, boards in rosters:
        for b in boards:
            by_name.setdefault(b['name'], b)
    if sel['full']:
        chosen = {n: 'all' for n in by_name}
    else:
        chosen = sel['boards']
    out = {}
    for name, tests in chosen.items():
        run = board_tests(by_name[name]) if tests == 'all' else list(tests)
        out[name] = sorted(set(run) | {'device/board_test'})
    return out
```

In `main()`: change the configs argument to optional — `ap.add_argument('configs', nargs='*', help='rig roster JSON file(s); omit for the build view alone')` — so CircleCI (which never touches HIL) can run without rosters; with no configs, `rosters` is `[]`, the HIL keys degrade to empty, and `hil_examples` is omitted. Then after the `args_flasher` line:

```python
    if rosters:
        s['hil_examples'] = hil_examples(s, rosters)
    s['build'] = classify_build(files, repo_root)
    for r in s['build']['reasons']:
        print(f'ci_select[build]: {r}', file=sys.stderr)
```

Update `test/hil/test/test_hil_util.py` BottomLayer: add `'build'`, `'build_utils'` to the `local` allowed set and `'../../tools/build'`, `'../../tools/build_utils'` to the module-path tuple (ci_select now imports both on the bare runner).

- [ ] **Step 4: Run tests + timing check**

```bash
python3 test/hil/test/test_ci_select.py 2>&1 | tail -3
python3 -m unittest discover -s test/hil/test 2>&1 | tail -3
time python3 tools/ci_select.py --diff-file <(echo src/class/cdc/cdc_device.c) test/hil/tinyusb.json >/dev/null
```

Expected: suites pass; the timed run stays under ~5 s (skip_example over 75 families × 46 examples re-reads small files — if it exceeds that, memoize `skip_example` results per (example, board) inside `_prune_buildable`).

- [ ] **Step 5: Commit**

```bash
git add tools/ci_select.py test/hil/test/test_ci_select.py test/hil/test/test_hil_util.py
git commit -m "ci_select: prune build selection by example buildability, emit build + hil_examples keys"
```

---

### Task 5: `ci_set_matrix.py --select / --base`

**Files:**
- Modify: `.github/scripts/ci_set_matrix.py`
- Test: `test/hil/test/test_ci_select.py`

**Interfaces:**
- Produces: CLI `python .github/scripts/ci_set_matrix.py [--select JSON | --base REF]`. No flags → byte-identical to today's output. `--select`: families intersected with `select.build.families` unless `build.full`; unusable JSON → full matrix + stderr warning. `--base REF`: runs `tools/ci_select.py --base REF` itself and proceeds as `--select`. Output shape `{toolchain: [family]}` unchanged.

- [ ] **Step 1: Write the failing tests**

```python
SET_MATRIX = os.path.join(REPO, '.github/scripts/ci_set_matrix.py')

class TestCiSetMatrix(unittest.TestCase):
    def run_matrix(self, *args):
        return subprocess.run([sys.executable, SET_MATRIX, *args],
                              capture_output=True, text=True)

    def test_no_flags_is_todays_output(self):
        r = self.run_matrix()
        self.assertEqual(r.returncode, 0, r.stderr)
        self.baseline = json.loads(r.stdout)
        self.assertIn('stm32f4', self.baseline['arm-gcc'])

    def test_select_full_is_identical(self):
        base = json.loads(self.run_matrix().stdout)
        sel = json.dumps({'build': {'full': True, 'families': [], 'family_examples': {}}})
        self.assertEqual(json.loads(self.run_matrix('--select', sel).stdout), base)

    def test_select_narrow_is_a_subset(self):
        sel = json.dumps({'build': {'full': False, 'families': ['rp2040', 'stm32f4'],
                                    'family_examples': {}}})
        m = json.loads(self.run_matrix('--select', sel).stdout)
        self.assertEqual(m['arm-gcc'], ['rp2040', 'stm32f4'])
        self.assertEqual(m['riscv-gcc'], [])
        self.assertEqual(set(m), set(json.loads(self.run_matrix().stdout)))  # all keys kept

    def test_malformed_select_falls_open(self):
        base = json.loads(self.run_matrix().stdout)
        r = self.run_matrix('--select', 'not json {')
        self.assertEqual(r.returncode, 0)
        self.assertEqual(json.loads(r.stdout), base)
        self.assertIn('full matrix', r.stderr)
```

- [ ] **Step 2: Run to verify failure**

Run: `python3 test/hil/test/test_ci_select.py TestCiSetMatrix -v 2>&1 | tail -3`
Expected: FAIL — argparse rejects `--select`.

- [ ] **Step 3: Implement**

In `.github/scripts/ci_set_matrix.py`, add imports `argparse, os, subprocess, sys` and replace `set_matrix_json` + the main guard:

```python
def set_matrix_json(select=None):
    sel_fams = None
    if select:
        b = select.get('build') or {}
        if b.get('full') is False:
            sel_fams = set(b.get('families') or [])
    matrix = {}
    for toolchain in toolchain_list:
        fams = [family for family, tc in family_list.items() if toolchain in tc]
        if sel_fams is not None:
            fams = [f for f in fams if f in sel_fams]
        matrix[toolchain] = fams
    print(json.dumps(matrix))


def main():
    parser = argparse.ArgumentParser()
    group = parser.add_mutually_exclusive_group()
    group.add_argument('--select', help='tools/ci_select.py JSON; scopes families when build.full is false')
    group.add_argument('--base', help='git ref: run tools/ci_select.py --base REF and scope from it')
    args = parser.parse_args()

    select = None
    try:
        if args.select:
            select = json.loads(args.select)
        elif args.base:
            root = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
            r = subprocess.run([sys.executable, os.path.join(root, 'tools', 'ci_select.py'),
                                '--base', args.base],
                               capture_output=True, text=True, cwd=root, check=True)
            select = json.loads(r.stdout)
    except Exception as e:  # fail-open: an unusable selection must never turn into a red job
        print(f'ci_set_matrix: selection unusable ({e}) - full matrix', file=sys.stderr)
        select = None
    set_matrix_json(select)


if __name__ == '__main__':
    main()
```

- [ ] **Step 4: Run tests**

Run: `python3 test/hil/test/test_ci_select.py TestCiSetMatrix -v` — all pass.
Also: `python3 .github/scripts/ci_set_matrix.py | diff - <(git show HEAD:.github/scripts/ci_set_matrix.py | python3 -)` → no diff (byte-identical default output).

- [ ] **Step 5: Extend the pre-commit hook scope and commit**

In `.pre-commit-config.yaml`, `ci-select-test` hook: `files: ^(hw/bsp/|src/|examples/|tools/(ci_select|build|build_utils)\.py$|\.github/scripts/)`.

```bash
git add .github/scripts/ci_set_matrix.py test/hil/test/test_ci_select.py .pre-commit-config.yaml
git commit -m "ci_set_matrix: scope the family matrix from a ci_select selection"
```

---

### Task 6: `hil_ci_set_matrix.py` emits `-e` per board

**Files:**
- Modify: `.github/scripts/hil_ci_set_matrix.py`
- Test: `test/hil/test/test_ci_select.py`

**Interfaces:**
- Produces: each build entry for board `B` gains ` -e <ex>` for every entry of `select.hil_examples[B]` (before variant expansion, so all of a board's variants carry the same list). No `hil_examples` key (hand runs, old selectors) → output byte-identical to today.
- Consumed by: `hil-build` / `hil-build-esp` (via `build_util.yml` → `tools/build.py`), `hil-hfp-iar`'s inline build loop — all funnel into `tools/build.py`, which learns `-e` in Task 7.

- [ ] **Step 1: Write the failing tests**

```python
HIL_SET_MATRIX = os.path.join(REPO, '.github/scripts/hil_ci_set_matrix.py')

class TestHilCiSetMatrixExamples(unittest.TestCase):
    def run_matrix(self, *args):
        r = subprocess.run([sys.executable, HIL_SET_MATRIX, *args,
                            os.path.join(REPO, 'test/hil/tinyusb.json')],
                           capture_output=True, text=True)
        self.assertEqual(r.returncode, 0, r.stderr)
        return r.stdout

    def test_no_hil_examples_is_byte_identical(self):
        plain = self.run_matrix()
        sel = json.dumps({'full': True, 'boards': {}})
        self.assertEqual(self.run_matrix('--select', sel), plain)

    def test_examples_appended_per_board(self):
        board = on_roster(self, 'stm32f407disco')[0]
        sel = json.dumps({'full': False, 'boards': {board: 'all'},
                          'hil_examples': {board: ['device/board_test', 'device/cdc_msc']}})
        m = json.loads(self.run_matrix('--select', sel))
        entries = [e for entries in m.values() for e in entries]
        self.assertTrue(entries)
        for e in entries:
            self.assertIn(f'-b {board}', e)
            self.assertIn('-e device/board_test', e)
            self.assertIn('-e device/cdc_msc', e)
```

- [ ] **Step 2: Run to verify failure**

Run: `python3 test/hil/test/test_ci_select.py TestHilCiSetMatrixExamples -v`
Expected: `test_examples_appended_per_board` FAILS (no `-e` in entries).

- [ ] **Step 3: Implement**

In `hil_ci_set_matrix.py` `main()`, after the `selected` computation add `ex_map = (sel or {}).get('hil_examples', {})`, and in the board loop, after the `build.args` append (line ~72):

```python
            # PR selection: build only the examples this board will run (its test
            # list plus device/board_test, the parking firmware) - tools/build.py -e.
            # Absent key (hand runs, full non-PR builds) keeps --target all.
            for ex in ex_map.get(name, []):
                build_board += f' -e {ex}'
```

- [ ] **Step 4: Run tests**

Run: `python3 test/hil/test/test_ci_select.py -v 2>&1 | tail -3` — all pass.

- [ ] **Step 5: Commit**

```bash
git add .github/scripts/hil_ci_set_matrix.py test/hil/test/test_ci_select.py
git commit -m "hil_ci_set_matrix: append per-board -e example filters from the selection"
```

---

### Task 7: `tools/build.py --example`

**Files:**
- Modify: `tools/build.py`
- Test: `test/hil/test/test_ci_select.py`

**Interfaces:**
- Produces: repeatable `-e/--example role/name`. Without it, behavior is exactly today's (`--target all`). With it: cmake builds one `--target <name>` per requested example the board can build (`build_utils.skip_example`), mapping `all` → example names and `examples-membrowse-upload` → `<name>-membrowse-upload` (the aggregate target `DEPENDS` every example — `hw/bsp/family_support.cmake:346-360` — and would rebuild the excluded ones); `tinyusb_metrics` and other targets pass through, order preserved. A board whose intersection is empty reports **skipped**. Make and espressif paths filter their example lists the same way. New helper `resolve_example_targets(build_targets, examples, board) -> list | None` (None = nothing buildable).

- [ ] **Step 1: Write the failing tests**

```python
class TestBuildPyExampleFilter(unittest.TestCase):
    def setUp(self):
        import build as build_py
        self.build = build_py
        self.old = os.getcwd()
        os.chdir(REPO)                      # skip_example uses repo-relative paths

    def tearDown(self):
        os.chdir(self.old)

    def test_all_maps_to_example_names(self):
        t = self.build.resolve_example_targets(['all'], ['device/cdc_msc', 'device/dfu'],
                                               'stm32f407disco')
        self.assertEqual(t, ['cdc_msc', 'dfu'])

    def test_membrowse_maps_per_example(self):
        t = self.build.resolve_example_targets(['all', 'examples-membrowse-upload'],
                                               ['device/cdc_msc'], 'stm32f407disco')
        self.assertEqual(t, ['cdc_msc', 'cdc_msc-membrowse-upload'])

    def test_other_targets_pass_through_in_order(self):
        t = self.build.resolve_example_targets(['all', 'tinyusb_metrics'],
                                               ['device/cdc_msc'], 'stm32f407disco')
        self.assertEqual(t, ['cdc_msc', 'tinyusb_metrics'])

    def test_unbuildable_examples_drop_and_empty_is_none(self):
        # typec/power_delivery only builds on stm32g4-class parts, never on f4
        t = self.build.resolve_example_targets(['all'],
                                               ['typec/power_delivery', 'device/cdc_msc'],
                                               'stm32f407disco')
        self.assertEqual(t, ['cdc_msc'])
        self.assertIsNone(self.build.resolve_example_targets(['all'],
                                                             ['typec/power_delivery'],
                                                             'stm32f407disco'))
```

- [ ] **Step 2: Run to verify failure**

Run: `python3 test/hil/test/test_ci_select.py TestBuildPyExampleFilter -v`
Expected: ERROR — `resolve_example_targets` not defined.

- [ ] **Step 3: Implement**

In `tools/build.py` add near `get_examples`:

```python
def resolve_example_targets(build_targets, examples, board):
    """Map generic targets onto per-example targets for a filtered build (-e).
    'all' -> the example executables; 'examples-membrowse-upload' -> per-example
    upload targets (the aggregate DEPENDS on every example and would rebuild the
    excluded ones); anything else (e.g. tinyusb_metrics) passes through.
    Returns None when no requested example is buildable on this board."""
    buildable = [e for e in examples if not build_utils.skip_example(e, board)]
    if not buildable:
        return None
    names = [e.split('/', 1)[1] for e in buildable]
    out = []
    for t in build_targets:
        if t == 'all':
            out += names
        elif t == 'examples-membrowse-upload':
            out += [f'{n}-membrowse-upload' for n in names]
        else:
            out.append(t)
    return list(dict.fromkeys(out))
```

Thread `examples` (a list or `None`) through `main()` → `build_boards_list` → `cmake_board`/`make_board`:

- `main()`: `parser.add_argument('-e', '--example', action='append', default=[], help='Only build these examples (role/name, repeatable). Default: all examples')`; pass `args.example or None` as a new final parameter of `build_boards_list`.
- `build_boards_list(..., examples=None)`: forward to both branches.
- `cmake_board(..., examples=None)`: in the espressif branch, after `all_examples = get_examples(family)` insert:

```python
        if examples is not None:
            all_examples = [e for e in all_examples if e in examples]
```

  In the generic branch, replace the target loop:

```python
        if rcmd.returncode == 0:
            targets = build_targets
            if examples is not None:
                targets = resolve_example_targets(build_targets, examples, board)
            if targets is None:
                print_build_result(board, 'examples (PR filter)', 2, '-')
                return [0, 0, 1]
            cmd = ["cmake", "--build", build_dir, '--parallel', str(parallel_jobs)]
            for target in targets:
                rcmd = run_cmd(cmd + ['--target', target])
                if rcmd.returncode != 0:
                    break
```

- `make_board(..., examples=None)`: after `all_examples = get_examples(family)`:

```python
    if examples is not None:
        all_examples = [e for e in all_examples if e in examples]
        if not all_examples:
            print_build_result(board, 'examples (PR filter)', 2, '-')
            return [0, 0, 1]
```

- [ ] **Step 4: Run tests + a real filtered build**

```bash
python3 test/hil/test/test_ci_select.py TestBuildPyExampleFilter -v
python3 tools/build.py -e device/cdc_msc -e device/cdc_dual_ports -b stm32f407disco
ls cmake-build/cmake-build-stm32f407disco/device/cdc_msc/cdc_msc.elf \
   cmake-build/cmake-build-stm32f407disco/device/cdc_dual_ports/cdc_dual_ports.elf
python3 tools/build.py -e typec/power_delivery -b stm32f407disco   # expect: Skipped row, exit 0
```

Expected: tests pass; both elfs exist; the typec run prints a Skipped result and exits 0.

- [ ] **Step 5: Commit**

```bash
git add tools/build.py test/hil/test/test_ci_select.py
git commit -m "build.py: add -e/--example filter with per-example target mapping"
```

---

### Task 8: `metrics.py --by-example` + by-example expansion + CMake wiring

**Files:**
- Modify: `tools/metrics.py`, `examples/CMakeLists.txt`, `.pre-commit-config.yaml`
- Create + Test: `test/hil/test/test_ci_metrics.py`

**Interfaces:**
- Produces: `metrics.py combine --by-example` additionally writes `<out>_by_example.json` = `{"<role>/<example>": {"files": [...]}}`, the example id taken from the map.json's two parent dirs (`<build>/<role>/<example>/*.map.json`). `combine` also accepts a by-example JSON as *input*, expanding each example to one data entry, with `--only-examples a,b` filtering which. `combine_files(input_files, filters=None, only_examples=None)`. Existing outputs byte-identical when the new flags are absent.
- Consumed by: `examples/CMakeLists.txt` `tinyusb_metrics` target (adds the flag), Task 9's pair-compare, Task 10's artifact upload.

- [ ] **Step 1: Write the failing tests** (new file `test/hil/test/test_ci_metrics.py`)

```python
#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Unit tests for the by-example half of tools/metrics.py and the (family, example)
# pair-compare script. Stdlib only; synthetic map.json fixtures, no builds.
#   python3 test/hil/test/test_ci_metrics.py
import json
import os
import subprocess
import sys
import tempfile
import unittest

REPO = os.path.dirname(os.path.dirname(os.path.dirname(
    os.path.dirname(os.path.abspath(__file__)))))
METRICS = os.path.join(REPO, 'tools', 'metrics.py')


def fake_map(path, files):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, 'w') as f:
        json.dump({'files': files}, f)


def entry(name, size, path_prefix='tinyusb/src'):
    return {'file': name, 'path': f'{path_prefix}/{name}', 'size': size,
            'symbols': [{'name': f'{name}_fn', 'size': size}], 'sections': {'.text': size}}


class TestByExample(unittest.TestCase):
    def build_tree(self, td):
        fake_map(os.path.join(td, 'device', 'cdc_msc', 'cdc_msc.map.json'),
                 [entry('usbd.c', 100), entry('cdc_device.c', 50)])
        fake_map(os.path.join(td, 'host', 'bare_api', 'bare_api.map.json'),
                 [entry('usbh.c', 200)])

    def test_by_example_output(self):
        with tempfile.TemporaryDirectory() as td:
            self.build_tree(td)
            out = os.path.join(td, 'metrics')
            r = subprocess.run([sys.executable, METRICS, 'combine', '-q', '-j',
                                '--by-example', '-o', out,
                                os.path.join(td, '*', '*', '*.map.json')],
                               capture_output=True, text=True)
            self.assertEqual(r.returncode, 0, r.stderr)
            by_ex = json.load(open(out + '_by_example.json'))
            self.assertEqual(set(by_ex), {'device/cdc_msc', 'host/bare_api'})
            self.assertEqual({f['file'] for f in by_ex['device/cdc_msc']['files']},
                             {'usbd.c', 'cdc_device.c'})
            # the plain averaged output is unchanged by the extra flag
            avg = json.load(open(out + '.json'))
            self.assertIn('files', avg)

    def test_by_example_json_roundtrips_as_combine_input(self):
        with tempfile.TemporaryDirectory() as td:
            self.build_tree(td)
            out = os.path.join(td, 'metrics')
            subprocess.run([sys.executable, METRICS, 'combine', '-q', '-j', '--by-example',
                            '-o', out, os.path.join(td, '*', '*', '*.map.json')], check=True)
            out2 = os.path.join(td, 'sub')
            r = subprocess.run([sys.executable, METRICS, 'combine', '-q', '-j',
                                '--only-examples', 'device/cdc_msc',
                                '-o', out2, out + '_by_example.json'],
                               capture_output=True, text=True)
            self.assertEqual(r.returncode, 0, r.stderr)
            sub = json.load(open(out2 + '.json'))
            names = {f['file'] for f in sub['files']}
            self.assertEqual(names, {'usbd.c', 'cdc_device.c'})   # bare_api filtered out


if __name__ == '__main__':
    unittest.main()
```

- [ ] **Step 2: Run to verify failure**

Run: `python3 test/hil/test/test_ci_metrics.py -v`
Expected: FAIL — argparse rejects `--by-example`.

- [ ] **Step 3: Implement in `tools/metrics.py`**

`combine_files` signature → `combine_files(input_files, filters=None, only_examples=None)`. Inside the `.json` branch, after `json.load`, insert the by-example expansion before the filter logic:

```python
                if 'files' not in json_data and json_data and \
                        all(isinstance(v, dict) and 'files' in v for v in json_data.values()):
                    # a metrics_by_example.json: one data entry per example
                    for ex in sorted(json_data):
                        if only_examples and ex not in only_examples:
                            continue
                        sub = {'files': list(json_data[ex]['files'])}
                        if filters:
                            sub['files'] = [f for f in sub['files']
                                            if f.get('path') and any(x in f['path'] for x in filters)]
                        all_json_data['file_list'].append(f'{fin}:{ex}')
                        all_json_data['data'].append(sub)
                    continue
```

Add a writer near `write_json_output`:

```python
def write_by_example(input_files, filters, path):
    """{<role>/<example>: {files: [...]}} from map.json inputs laid out as
    <build>/<role>/<example>/<name>.map.json (examples/CMakeLists.txt's pattern)."""
    out = {}
    for fin in input_files:
        d = os.path.dirname(os.path.abspath(fin))
        ex = f'{os.path.basename(os.path.dirname(d))}/{os.path.basename(d)}'
        data = combine_files([fin], filters)
        if data['data']:
            out.setdefault(ex, {'files': []})['files'] += data['data'][0].get('files', [])
    with open(path, 'w', encoding='utf-8') as f:
        json.dump(out, f)
```

`cmd_combine`: pass `only_examples=set(args.only_examples.split(',')) if args.only_examples else None` into `combine_files`, and after the existing outputs:

```python
    if args.by_example:
        write_by_example(input_files, args.filters, args.out + '_by_example.json')
```

Argparse additions on the combine subparser:

```python
    combine_parser.add_argument('--by-example', dest='by_example', action='store_true',
                                help='Also write <out>_by_example.json: per-example file lists keyed by role/example')
    combine_parser.add_argument('--only-examples', dest='only_examples', default='',
                                help='Comma-separated role/example ids to keep when reading by-example JSON inputs')
```

- [ ] **Step 4: Wire CMake + hooks**

`examples/CMakeLists.txt` `tinyusb_metrics` target: change the command to
`combine -f tinyusb/src -j --by-example -o ${CMAKE_BINARY_DIR}/metrics` (one added flag).
`.pre-commit-config.yaml` `hil-test` hook: `files: ^(test/hil/|examples/device/mtp/src/|tools/metrics\.py$|\.github/scripts/metrics_pair_compare\.py$)`.

- [ ] **Step 5: Run tests**

```bash
python3 test/hil/test/test_ci_metrics.py -v      # pass
python3 -m unittest discover -s test/hil/test 2>&1 | tail -3   # discovery picks the new file up
```

- [ ] **Step 6: Commit**

```bash
git add tools/metrics.py examples/CMakeLists.txt test/hil/test/test_ci_metrics.py .pre-commit-config.yaml
git commit -m "metrics: emit and consume per-example size data (--by-example, --only-examples)"
```

---

### Task 9: `(family, example)`-intersection compare script

**Files:**
- Create: `.github/scripts/metrics_pair_compare.py`
- Test: `test/hil/test/test_ci_metrics.py`

**Interfaces:**
- Produces: CLI `metrics_pair_compare.py --base-dir D1 --new-dir D2 [--out metrics_compare]`. Each dir is searched recursively for `cmake-build-<board>/metrics_by_example.json`; board → family via `hw/bsp/*/boards/<board>`. Writes `<out>.md`: the standard compare table over the intersection of `(family, example)` pairs, then a scope footer naming the compared families and any pairs missing on one side. Empty intersection → an explanatory one-line `.md`, exit 0.
- Consumes: `tools/metrics.py` internals `combine_files`/`compute_avg`-backed `compare_files` and `write_compare_markdown` (via `sys.path` import).

- [ ] **Step 1: Write the failing tests** (append to `test_ci_metrics.py`)

```python
PAIR_COMPARE = os.path.join(REPO, '.github/scripts/metrics_pair_compare.py')


def fake_by_example(root, board, data):
    d = os.path.join(root, f'cmake-build-{board}')
    os.makedirs(d, exist_ok=True)
    with open(os.path.join(d, 'metrics_by_example.json'), 'w') as f:
        json.dump(data, f)


class TestPairCompare(unittest.TestCase):
    def test_intersection_compare(self):
        with tempfile.TemporaryDirectory() as td:
            base, new = os.path.join(td, 'base'), os.path.join(td, 'new')
            # real board names so board->family resolution works against hw/bsp
            fake_by_example(base, 'raspberry_pi_pico',
                            {'device/cdc_msc': {'files': [entry('usbd.c', 100)]},
                             'device/dfu': {'files': [entry('dfu_device.c', 10)]}})
            fake_by_example(new, 'raspberry_pi_pico',
                            {'device/cdc_msc': {'files': [entry('usbd.c', 120)]}})
            out = os.path.join(td, 'cmp')
            r = subprocess.run([sys.executable, PAIR_COMPARE, '--base-dir', base,
                                '--new-dir', new, '--out', out],
                               capture_output=True, text=True)
            self.assertEqual(r.returncode, 0, r.stderr)
            md = open(out + '.md').read()
            self.assertIn('usbd.c', md)
            self.assertNotIn('dfu_device.c', md)          # not on both sides
            self.assertIn('rp2040', md)                    # scope footer
            self.assertIn('device/dfu', md)                # named as dropped

    def test_empty_intersection_writes_note(self):
        with tempfile.TemporaryDirectory() as td:
            base, new = os.path.join(td, 'base'), os.path.join(td, 'new')
            fake_by_example(base, 'raspberry_pi_pico', {'device/dfu': {'files': [entry('a.c', 1)]}})
            fake_by_example(new, 'stm32f407disco', {'device/cdc_msc': {'files': [entry('b.c', 1)]}})
            out = os.path.join(td, 'cmp')
            r = subprocess.run([sys.executable, PAIR_COMPARE, '--base-dir', base,
                                '--new-dir', new, '--out', out],
                               capture_output=True, text=True)
            self.assertEqual(r.returncode, 0, r.stderr)
            self.assertIn('skipped', open(out + '.md').read())
```

- [ ] **Step 2: Run to verify failure**

Run: `python3 test/hil/test/test_ci_metrics.py TestPairCompare -v`
Expected: FAIL — script does not exist.

- [ ] **Step 3: Implement `.github/scripts/metrics_pair_compare.py`**

```python
#!/usr/bin/env python3
"""Family+example-matched code-size compare for PR-scoped builds.

The averaged metrics baseline (metrics-tinyusb) spans every family and example;
a scoped PR builds a subset, so comparing against it is apples-to-oranges. This
compares the intersection of (family, example) pairs present on BOTH sides,
averaged over exactly those pairs, and names what was dropped. See
docs/superpowers/specs/2026-08-19-ci-build-family-filter-design.md #code-metrics.
"""
import argparse
import glob
import json
import os
import sys
import tempfile

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', 'tools'))
import metrics


def board_family(board, repo_root):
    hits = glob.glob(os.path.join(repo_root, 'hw/bsp/*/boards', board))
    return os.path.basename(os.path.dirname(os.path.dirname(hits[0]))) if hits else None


def collect(root, repo_root):
    """{(family, 'role/example'): [file entries]} from every
    **/cmake-build-<board>/metrics_by_example.json under root."""
    pairs = {}
    pat = os.path.join(root, '**', 'metrics_by_example.json')
    for f in sorted(glob.glob(pat, recursive=True)):
        board = os.path.basename(os.path.dirname(f))
        if not board.startswith('cmake-build-'):
            continue
        fam = board_family(board[len('cmake-build-'):], repo_root)
        if not fam:
            print(f'pair_compare: no family for {board}, skipping', file=sys.stderr)
            continue
        try:
            data = json.load(open(f))
        except (OSError, ValueError) as e:
            print(f'pair_compare: unreadable {f} ({e}), skipping', file=sys.stderr)
            continue
        for ex, ent in data.items():
            pairs.setdefault((fam, ex), []).extend(ent.get('files', []))
    return pairs


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--base-dir', required=True)
    ap.add_argument('--new-dir', required=True)
    ap.add_argument('--out', default='metrics_compare')
    a = ap.parse_args()
    repo_root = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

    base = collect(a.base_dir, repo_root)
    new = collect(a.new_dir, repo_root)
    common = sorted(set(base) & set(new))
    dropped = sorted(set(base) ^ set(new))

    if not common:
        with open(a.out + '.md', 'w') as f:
            f.write('_Code-size comparison skipped: no (family, example) pair was built '
                    'on both the base branch and this PR._\n')
        return

    def synth(pairs, path):
        with open(path, 'w') as f:
            json.dump({'files': [e for k in common for e in pairs[k]]}, f)

    with tempfile.TemporaryDirectory() as td:
        b, n = os.path.join(td, 'base.json'), os.path.join(td, 'new.json')
        synth(base, b)
        synth(new, n)
        comparison = metrics.compare_files(b, n, ['tinyusb/src'])
        if comparison is None:
            with open(a.out + '.md', 'w') as f:
                f.write('_Code-size comparison failed to produce data._\n')
            return
        metrics.write_compare_markdown(comparison, a.out + '.md', 'name+')

    with open(a.out + '.md', 'a') as f:
        fams = sorted({k[0] for k in common})
        f.write(f'\n_Scoped compare: {len(common)} (family, example) pairs across '
                f'{", ".join(fams)}._\n')
        if dropped:
            f.write('_Not compared (missing on one side): '
                    + ', '.join(f'{fam}:{ex}' for fam, ex in dropped) + '._\n')


if __name__ == '__main__':
    main()
```

- [ ] **Step 4: Run tests**

Run: `python3 test/hil/test/test_ci_metrics.py -v` — all pass.

- [ ] **Step 5: Commit**

```bash
git add .github/scripts/metrics_pair_compare.py test/hil/test/test_ci_metrics.py
git commit -m "ci: add (family, example)-intersection code-size compare for scoped PRs"
```

---

### Task 10: GitHub Actions wiring (`build.yml` + `build_util.yml`)

**Files:**
- Modify: `.github/workflows/build.yml`, `.github/workflows/build_util.yml`

**Interfaces:**
- `set-matrix` new outputs: `example_map` (JSON `{family: [example]}`), `build_filtered` (`'true'`/`'false'`), `build_families_regex` (`fam1|fam2`, only when filtered).
- `build_util.yml` new input `example-map` (string, default `''`); when set, each leg resolves `-e` flags for its `matrix.arg` family and appends them (via env `$EX_ARGS`) to the Build and Membrowse invocations; metrics upload also grabs `metrics_by_example.json`.
- `code-metrics` gains `needs: set-matrix` and a scoped-baseline path.

- [ ] **Step 1: Rename + thread the selection in `set-matrix`**

Rename the step `HIL selection (PR only)` → `CI selection (PR only)` (id stays `hil-select`; renaming the id would touch every `steps.hil-select` reference — leave it). In the **Generate matrix json** step, replace the first three lines of the script (`MATRIX_JSON=$(python .github/scripts/ci_set_matrix.py)` and the two echo lines) with:

```bash
          # Build matrix, scoped by the PR selection when one exists. Best-effort:
          # ci_set_matrix falls back to the full matrix itself on unusable JSON,
          # and an empty $SELECT (non-PR event, selector fallback) means no flags.
          if [ -n "$SELECT" ]; then
            MATRIX_JSON=$(python .github/scripts/ci_set_matrix.py --select "$SELECT") || MATRIX_JSON=''
          else
            MATRIX_JSON=''
          fi
          [ -z "$MATRIX_JSON" ] && MATRIX_JSON=$(python .github/scripts/ci_set_matrix.py)
          echo "matrix=$MATRIX_JSON"
          echo "matrix=$MATRIX_JSON" >> $GITHUB_OUTPUT

          # Build-axis extras: the per-family example map rides as a side channel
          # (a value inside matrix entries would break CircleCI's family parameter
          # and multiply GHA matrix legs). NOTE jq's // treats false like null, so
          # .build.full is compared explicitly.
          EXAMPLE_MAP=$(printf '%s' "${SELECT:-null}" | jq -c '.build.family_examples // {}') || EXAMPLE_MAP='{}'
          BUILD_FILTERED=$(printf '%s' "${SELECT:-null}" | jq -r 'if (.build? | type) == "object" and .build.full == false then "true" else "false" end') || BUILD_FILTERED='false'
          FAM_REGEX=''
          if [ "$BUILD_FILTERED" = "true" ]; then
            FAM_REGEX=$(printf '%s' "$SELECT" | jq -r '.build.families | join("|")') || FAM_REGEX=''
            [ -z "$FAM_REGEX" ] && BUILD_FILTERED='false'
          fi
          echo "example_map=$EXAMPLE_MAP" >> $GITHUB_OUTPUT
          echo "build_filtered=$BUILD_FILTERED" >> $GITHUB_OUTPUT
          echo "build_families_regex=$FAM_REGEX" >> $GITHUB_OUTPUT
```

Add to the `set-matrix` job `outputs:` block:

```yaml
      example_map: ${{ steps.set-matrix-json.outputs.example_map }}
      build_filtered: ${{ steps.set-matrix-json.outputs.build_filtered }}
      build_families_regex: ${{ steps.set-matrix-json.outputs.build_families_regex }}
```

- [ ] **Step 2: `build_util.yml` — example-map input**

Add the input:

```yaml
      example-map:
        required: false
        default: ''
        type: string
```

Insert between **Get Dependencies** and **Build**:

```yaml
      - name: Resolve PR example filter
        if: inputs.example-map != '' && inputs.example-map != '{}'
        env:
          # values are PR-derived - keep them out of ${{ }} script interpolation
          # (env expansion word-splits but never re-parses shell metacharacters)
          EXAMPLE_MAP: ${{ inputs.example-map }}
          FAMILY: ${{ matrix.arg }}
        run: |
          # -e flags for this family; a family absent from the map builds everything
          EX_ARGS=$(printf '%s' "$EXAMPLE_MAP" | jq -r --arg fam "$FAMILY" '(.[$fam] // []) | map("-e " + .) | join(" ")') || EX_ARGS=''
          echo "EX_ARGS=$EX_ARGS"
          echo "EX_ARGS=$EX_ARGS" >> $GITHUB_ENV
```

Append `$EX_ARGS` to all three `tools/build.py` invocations (the esp-idf docker line, the generic Build line, and the Membrowse line — build.py maps `examples-membrowse-upload` per example when `-e` is active, because the aggregate target rebuilds everything). Extend the metrics upload:

```yaml
          path: |
            cmake-build/cmake-build-*/metrics.json
            cmake-build/cmake-build-*/metrics_by_example.json
```

- [ ] **Step 3: `cmake` job passes the map**

In the `cmake` job's `with:` block add `example-map: ${{ needs.set-matrix.outputs.example_map }}`. Do **not** add it to `hil-build`/`hil-build-esp`/`build-os` — hil legs carry `-e` inside their matrix entries; build-os keeps the full example set.

- [ ] **Step 4: `code-metrics` scoped baseline**

Verify the download action supports regexp names:
`curl -fsSL https://raw.githubusercontent.com/dawidd6/action-download-artifact/v11/action.yml | grep -n name_is_regexp` — expect a hit. (Fallback if absent: replace the download step below with a `gh run download`-based loop over `build_families_regex` split on `|`, using `gh api` to find the newest master run per artifact; keep the same directory layout.)

Change `needs: [ check-paths, cmake ]` → `needs: [ check-paths, cmake, set-matrix ]`. Guard the two unscoped steps with the filtered flag: on **Download Base Branch Metrics** change the `if:` to

```yaml
        if: (github.event_name == 'pull_request' || github.event_name == 'workflow_dispatch') && needs.set-matrix.outputs.build_filtered != 'true'
```

and on **Compare with Base Branch** change `if: github.event_name != 'push'` to

```yaml
        if: github.event_name != 'push' && needs.set-matrix.outputs.build_filtered != 'true'
```

Insert after **Download Base Branch Metrics**:

```yaml
      - name: Download base per-family metrics (scoped PR)
        if: github.event_name == 'pull_request' && needs.set-matrix.outputs.build_filtered == 'true'
        uses: dawidd6/action-download-artifact@v11
        with:
          workflow: build.yml
          workflow_conclusion: ''
          search_artifacts: true      # a docs-only master push uploads no per-family artifacts
          branch: ${{ github.base_ref }}
          name: ^metrics-(${{ needs.set-matrix.outputs.build_families_regex }})$
          name_is_regexp: true
          path: base-family-metrics
        continue-on-error: true

      - name: Compare with Base Branch (scoped)
        if: github.event_name == 'pull_request' && needs.set-matrix.outputs.build_filtered == 'true'
        run: |
          # never fall back to the averaged metrics-tinyusb here: a scoped PR vs the
          # 64-family/46-example average is exactly the mismatch this path prevents
          python .github/scripts/metrics_pair_compare.py \
            --base-dir base-family-metrics --new-dir cmake-build --out metrics_compare
          cat metrics_compare.md
```

(The PR-side `cmake-build/` dir already holds this run's `metrics_by_example.json` files from the artifact download at the top of the job.)

- [ ] **Step 5: Validate and commit**

```bash
python3 -c "import yaml,sys; yaml.safe_load(open('.github/workflows/build.yml')); yaml.safe_load(open('.github/workflows/build_util.yml')); print('yaml ok')"
command -v actionlint >/dev/null && actionlint .github/workflows/build.yml .github/workflows/build_util.yml || true
git add .github/workflows/build.yml .github/workflows/build_util.yml
git commit -m "ci: scope the GHA build matrix and code-metrics baseline by PR selection"
```

---

### Task 11: CircleCI wiring

**Files:**
- Modify: `.circleci/config.yml`, `.circleci/config2.yml`

**Interfaces:**
- `config.yml` set-matrix: on PRs, runs the selector (gated on its own unit suite), scopes `MATRIX_JSON` via `--select`, skips empty toolchains, and forwards `example-map` + `build-filtered` to the continued workflow as pipeline parameters.
- `config2.yml`: declares those parameters; the `build` command resolves `-e` flags per family; `code-metrics` compare is bypassed with a note when filtered; a `no-op` job keeps the workflow valid when nothing is selected.

- [ ] **Step 1: Verify the continuation orb accepts parameters**

`curl -fsSL "https://circleci.com/developer/orbs/orb/circleci/continuation" | grep -io 'parameters' | head -1` — the `continuation/continue` command takes a `parameters` input (inline JSON or a file path). If the page is unreachable, proceed — the orb has carried this input since 0.2; the fallback is `parameters: '{"example-map": ...}'` inline via an env-composed string.

- [ ] **Step 2: `config.yml` — selector + scoping + parameters**

In the `Set matrix` run command, replace the first two lines (`MATRIX_JSON=$(python .github/scripts/ci_set_matrix.py)` and its echo) with:

```bash
            # PR-scoped selection (best-effort: any failure falls back to the full
            # matrix). CircleCI has no base-branch var; tinyusb PRs target master.
            SELECT_JSON=''
            if [ -n "${CIRCLE_PULL_REQUEST:-}" ]; then
              git fetch --no-tags origin master || true
              if python3 test/hil/test/test_ci_select.py >/dev/null 2>&1; then
                SELECT_JSON=$(python3 tools/ci_select.py --base origin/master) || SELECT_JSON=''
              else
                echo "ci_select unit suite failed - using the full matrix"
              fi
            fi
            MATRIX_JSON=''
            if [ -n "$SELECT_JSON" ]; then
              MATRIX_JSON=$(python .github/scripts/ci_set_matrix.py --select "$SELECT_JSON") || MATRIX_JSON=''
            fi
            [ -z "$MATRIX_JSON" ] && MATRIX_JSON=$(python .github/scripts/ci_set_matrix.py)
            echo "MATRIX_JSON=$MATRIX_JSON"

            EXAMPLE_MAP=$(printf '%s' "${SELECT_JSON:-null}" | jq -c '.build.family_examples // {}') || EXAMPLE_MAP='{}'
            BUILD_FILTERED=$(printf '%s' "${SELECT_JSON:-null}" | jq -r 'if (.build? | type) == "object" and .build.full == false then "true" else "false" end') || BUILD_FILTERED='false'
            jq -n --arg map "$EXAMPLE_MAP" --arg filt "$BUILD_FILTERED" \
              '{"example-map": $map, "build-filtered": $filt}' > /tmp/continue_params.json
```

In the toolchain loop, after `FAMILY=$(echo $MATRIX_JSON | jq -r ".\"$toolchain\"")` add:

```bash
                if [ "$(echo "$FAMILY" | jq 'length')" = "0" ]; then
                  # an empty matrix parameter is a hard CircleCI config error, not a skip
                  echo "skip build-${build_system}-${toolchain}: no families selected"
                  continue
                fi
```

(the `continue` also keeps the alias out of `BUILD_ALIASES`, so `code-metrics` never requires a job that was not generated). Guard the code-metrics emission and keep the workflow non-empty:

```bash
            if [ ${#BUILD_ALIASES[@]} -gt 0 ]; then
              echo "      - code-metrics:" >> .circleci/config2.yml
              echo "          requires:" >> .circleci/config2.yml
              for alias in "${BUILD_ALIASES[@]}"; do
                echo "            - $alias" >> .circleci/config2.yml
              done
            else
              # a workflow with zero jobs is invalid config
              echo "      - no-op" >> .circleci/config2.yml
            fi
```

(replacing the current unconditional code-metrics block). Change the continuation call to:

```yaml
      - continuation/continue:
          configuration_path: .circleci/config2.yml
          parameters: /tmp/continue_params.json
```

- [ ] **Step 3: `config2.yml` — parameters, `-e` resolution, scoped-compare note, no-op job**

At the top, after `version: 2.1`:

```yaml
parameters:
  example-map:
    type: string
    default: "{}"
  build-filtered:
    type: string
    default: "false"
```

In the `build` command's **Build** step, before the toolchain if/else, insert:

```bash
            # PR example filter for this family ('{}' or a missing key = build all).
            # The parameter is a JSON string composed by set-matrix from ci_select.
            EX_ARGS=$(printf '%s' '<< pipeline.parameters.example-map >>' | jq -r --arg fam "<< parameters.family >>" '(.[$fam] // []) | map("-e " + .) | join(" ")' 2>/dev/null) || EX_ARGS=''
```

and append `$EX_ARGS` to both `tools/build.py` invocations (docker esp-idf and the generic one). In `code-metrics`, wrap the existing compare `when:` condition with the filter guard and add the note branch:

```yaml
      - when:
          condition:
            and:
              - not:
                  equal: [ master, << pipeline.git.branch >> ]
              - equal: [ "false", << pipeline.parameters.build-filtered >> ]
          steps:
            # ... the existing Download Base Branch Metrics + Compare + store_artifacts steps, unchanged ...
      - when:
          condition:
            and:
              - not:
                  equal: [ master, << pipeline.git.branch >> ]
              - equal: [ "true", << pipeline.parameters.build-filtered >> ]
          steps:
            - run:
                name: Scoped build - comparison unavailable
                command: |
                  # CircleCI stores only the averaged metrics.json; the per-example
                  # baseline lives on GHA. See the GHA code-metrics PR comment.
                  echo "_Code-size comparison skipped on CircleCI: this PR built a scoped example set._" > metrics_compare.md
            - store_artifacts:
                path: metrics_compare.md
                destination: metrics_compare.md
```

Add the no-op job beside the other job definitions:

```yaml
  no-op:
    docker:
      - image: cimg/base:current
    resource_class: small
    steps:
      - run:
          name: No families selected
          command: echo "PR selection - no families to build on CircleCI"
```

- [ ] **Step 4: Validate and commit**

```bash
python3 -c "import yaml; yaml.safe_load(open('.circleci/config.yml')); yaml.safe_load(open('.circleci/config2.yml')); print('yaml ok')"
command -v circleci >/dev/null && circleci config validate .circleci/config.yml || true
git add .circleci/config.yml .circleci/config2.yml
git commit -m "ci: scope the CircleCI build matrix and example set by PR selection"
```

---

### Task 12: End-to-end validation, review, hand-off

**Files:** none new — verification only (fix-ups amend the relevant earlier area).

- [ ] **Step 1: Full hooks + suites**

```bash
pre-commit run --all-files          # ~55 s; HIL hooks exercise real timeouts deliberately
```

Expected: all hooks pass (`ci-select-test` and `hil-test` among them).

- [ ] **Step 2: Selector scenario table**

```bash
for f in src/portable/raspberrypi/rp2040/dcd_rp2040.c src/class/cdc/cdc_device.c \
         src/host/usbh.c examples/device/cdc_msc/src/main.c test/hil/hil_test.py \
         src/common/tusb_fifo.c hw/mcu/nordic/nrf5x/x.h; do
  echo "== $f"
  python3 tools/ci_select.py --diff-file <(echo "$f") test/hil/tinyusb.json 2>/dev/null | \
    python3 -c "import json,sys; s=json.load(sys.stdin); b=s['build']; print('hil_full:', s['full'], ' build_full:', b['full'], ' fams:', len(b['families']), ' mapped:', len(b['family_examples']))"
done
```

Expected (spot-check against the spec's measured table): rp2040 → 1 family; cdc_device → all families, mapped lists; usbh → ~25 families; example → all families, 1-example lists; test/hil → 0 families, hil_full true; common → build_full true; hw/mcu → 1 family (`nrf`).

- [ ] **Step 3: Matrix + build smoke**

```bash
SEL=$(python3 tools/ci_select.py --diff-file <(echo src/portable/raspberrypi/rp2040/dcd_rp2040.c) test/hil/tinyusb.json 2>/dev/null)
python3 .github/scripts/ci_set_matrix.py --select "$SEL" | python3 -m json.tool | head
python3 .github/scripts/hil_ci_set_matrix.py --select "$SEL" test/hil/tinyusb.json | python3 -m json.tool | head
python3 tools/build.py -e device/cdc_msc -b stm32f407disco --target all --target tinyusb_metrics
python3 -c "import json; d=json.load(open('cmake-build/cmake-build-stm32f407disco/metrics_by_example.json')); print(sorted(d))"
```

Expected: matrix shows only rp2040 under arm-gcc; hil matrix entries carry `-e ... -e device/board_test`; the by-example JSON lists exactly `['device/cdc_msc']`.

- [ ] **Step 4: Full example set for one board** (repo validation rule after tool changes)

```bash
cd examples && cmake -B cmake-build-stm32f407disco -DBOARD=stm32f407disco -G Ninja -DCMAKE_BUILD_TYPE=MinSizeRel . \
  && cmake --build cmake-build-stm32f407disco && cd ..
```

Expected: builds green (objcopy warnings non-critical per CLAUDE.md).

- [ ] **Step 5: Local review, then stop**

Run the `/code-review` skill on the branch diff (user policy: every push carrying local changes gets a local review pass first) and fix what holds up, amending into the appropriate task commits. Then **stop and hand back to the user** — pushing `build-filter` and opening the PR is their call; note for the PR description that the workflow changes only fully prove out on a real PR run (first PR after merge-to-branch should be watched with `gh pr checks --watch`, and the `hil-select` step's warnings checked for silent fallbacks).

---

## Self-Review Notes

- Spec coverage: rule table (Tasks 2-4), CMake-only scan (Task 2), orphan invariant (Task 2), build/hil_examples JSON contract (Task 4), `ci_set_matrix` flags (Task 5), `hil_ci_set_matrix -e` (Task 6), `build.py -e` incl. membrowse aggregate-dependency workaround (Task 7), metrics by-example + intersection compare + never-fall-back rule (Tasks 8-10), GHA side channel + injection-safe env passing (Task 10), CircleCI empty-toolchain/alias/no-op fixes + parameters (Task 11), move fallout table (Task 1).
- Known deviation from the spec text, both directions justified inline: `hil_examples` uses the *narrowed* chosen test list when a board is narrowed (the spec's JSON example implies this; its prose says `board_tests` — the narrowed form is a strict subset and matches what the rig runs, and re-run specs are subsets of it).
- Spec's measured "hcd_max3421.c → 1 leg" is really 1 *bsp* family (`espressif`) that neither provider's family list builds → 0 CI legs; Task 3's rule-4 test therefore asserts shape, not that specific count.
