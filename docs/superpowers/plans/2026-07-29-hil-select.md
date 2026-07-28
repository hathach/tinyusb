# PR-Scoped HIL Selection Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** A diff→(boards, tests) selector (`test/hil/hil_select.py`) that scopes CI's HIL build+test jobs on pull requests and is reusable locally, per `docs/superpowers/specs/2026-07-29-hil-pr-scoped-selection-design.md`.

**Architecture:** Pure-stdlib classification engine (changed files → per-board test selection, fail-open to full) + thin CLI emitting JSON with per-rig `hil_test.py` arg strings; consumed by `hil_ci_set_matrix.py --select` (prunes hil-build) and shell steps in the three HIL jobs (prunes rig runs). Test lists shared via new `hil_examples.py`.

**Tech Stack:** Python 3.11 stdlib only (`re`, `json`, `glob`, `subprocess` for git), `unittest` for tests, GitHub Actions YAML.

## Global Constraints

- Work in worktree `.claude/worktrees/hil-select` (branch `claude/hil-select`); never touch the primary checkout.
- `hil_select.py`, `hil_examples.py`, `test_hil_select.py` import NOTHING outside the stdlib and each other — in particular never `hil_test`/`hil_flash`/`hil_lock` (GitHub's bare runner has no pyserial/pymtp).
- Fail-open: any changed file matching no classification rule ⇒ `full: true`. Scoping applies to `pull_request` events only; push/scheduled runs stay full.
- Behavior-preserving for existing tools: `hil_test.py` runtime behavior unchanged (only its test-list constants move to `hil_examples.py`); `hil_ci_set_matrix.py` without `--select` emits byte-identical output to today.
- The selector only ever emits board names present in the given roster (`config['boards']`); `boards-skip` is invisible to it.
- Commit messages: imperative, scoped, NO Co-Authored-By/Claude-Session trailers.
- Every commit: `python3 -m py_compile` clean on touched python files, `python3 test/hil/test_hil_select.py` green (once it exists), `pre-commit run --files <touched>` clean.

---

### Task 1: hil_examples.py + selection engine with unit tests

**Files:**
- Create: `test/hil/hil_examples.py`
- Create: `test/hil/hil_select.py` (engine only; CLI comes in Task 2)
- Create: `test/hil/test_hil_select.py`
- Modify: `test/hil/hil_test.py` (import test lists from hil_examples)
- Modify: `test/hil/hil_ci.sh` (scp list gains `hil_examples.py`)

**Interfaces:**
- Produces `hil_examples.py`: `device_tests: list[str]`, `dual_tests: list[str]`, `host_test: list[str]` — the three lists moved VERBATIM (incl. comments) from `hil_test.py`.
- Produces `hil_select.py` engine API used by Task 2:
  - `classify(changed_files: list[str], repo_root: str, rosters: list[tuple[str, list[dict]]]) -> dict`
    returning `{'full': bool, 'boards': {board_name: 'all' | sorted list[str]}, 'reasons': list[str]}`
    where `rosters` = `[(config_path, config['boards']), ...]`.
  - `board_roles(board: dict) -> set[str]` — subset of `{'device', 'host'}` from the roster
    entry's `tests` flags (`device`/`host`/`dual` booleans; an `only` list contributes the
    roles of its entries' path prefixes; `dual` implies both roles).
  - `board_family(board_name: str, repo_root: str) -> str | None` — the `<family>` for which
    `hw/bsp/<family>/boards/<board_name>` exists.
  - `port_families(port_dir: str, repo_root: str) -> set[str]` — directories of
    `hw/bsp/*/family.cmake` and `hw/bsp/*/family.mk` whose text contains `port_dir`
    (e.g. `raspberrypi/rp2040`).
  - `class_examples(class_dir: str, role: str, repo_root: str) -> set[str]` — tests from
    `hil_examples` lists whose example `tusb_config.h` enables the class for that role (regex
    `#define\s+CFG_TUD_<C>\s+\(?\s*0*[1-9]` / `CFG_TUH_<C>`; exceptions per spec:
    `dfu_rt_device.*`→`CFG_TUD_DFU_RUNTIME`, `dfu_device.*`→`CFG_TUD_DFU`, class dir `net`
    → `CFG_TUD_ECM_RNDIS|CFG_TUD_NCM`). Test path `device/x` ⇒ config at
    `examples/device/x/src/tusb_config.h`; same pattern for `host/` and `dual/`.

- [ ] **Step 1: Move the test lists into `hil_examples.py`**

Create `test/hil/hil_examples.py`:

```python
#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# HIL example test lists, shared by hil_test.py (runner) and hil_select.py
# (PR-diff selector). Stdlib-only: hil_select runs on bare CI runners.
```

then MOVE the `device_tests`, `dual_tests`, `host_test` list definitions (and their preceding
comment block "The per-board run order is shuffled...") VERBATIM from `hil_test.py` into it.
In `hil_test.py`, add `from hil_examples import device_tests, dual_tests, host_test` where the
lists were (a `from`-import of data constants is fine here — they are read-only lists used by
name throughout `test_board`). Add `"$ROOT_DIR/test/hil/hil_examples.py" \` to the
`hil_ci.sh` scp list after the `hil_lock.py` line.

- [ ] **Step 2: Verify the move broke nothing**

Run: `cd /home/hathach/code/tinyusb/.claude/worktrees/hil-select && python3 -m py_compile test/hil/hil_examples.py test/hil/hil_test.py && python3 test/hil/hil_test.py --help >/dev/null && echo ok`
Expected: `ok`

- [ ] **Step 3: Write the failing unit tests (spec acceptance cases)**

Create `test/hil/test_hil_select.py`. ROSTER is a trimmed but real-shaped fixture; tests call
the engine API directly (no git, no CLI):

```python
#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Unit tests for hil_select.py — pure logic, no hardware, no git. Run directly:
#   python3 test/hil/test_hil_select.py
import os
import sys
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import hil_select
from hil_examples import device_tests, dual_tests, host_test

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

ROSTER = [
    # device-only, rp2040 family
    {'name': 'raspberry_pi_pico', 'uid': 'u1',
     'tests': {'device': True, 'host': True, 'dual': True}},
    # device-only, stm32f4 family
    {'name': 'stm32f407disco', 'uid': 'u2',
     'tests': {'device': True, 'host': False, 'dual': False}},
    # host-only board
    {'name': 'raspberry_pi_pico2', 'uid': 'u3',
     'tests': {'device': False, 'host': True, 'dual': False}},
    # only-list board (espressif-style)
    {'name': 'espressif_s3_devkitm', 'uid': 'u4',
     'tests': {'only': ['device/cdc_msc_freertos', 'host/device_info']}},
]
ROSTERS = [('test/hil/tinyusb.json', ROSTER)]


def sel(files):
    return hil_select.classify(files, REPO, ROSTERS)


class TestPortRule(unittest.TestCase):
    def test_dcd_rp2040_selects_pico_family_only(self):
        s = sel(['src/portable/raspberrypi/rp2040/dcd_rp2040.c'])
        self.assertFalse(s['full'])
        self.assertIn('raspberry_pi_pico', s['boards'])
        self.assertNotIn('stm32f407disco', s['boards'])
        self.assertNotIn('espressif_s3_devkitm', s['boards'])
        # device role: no host tests in pico's list
        self.assertTrue(all(not t.startswith('host/') for t in s['boards']['raspberry_pi_pico']))
        # host-only boards drop out entirely on a device-role change
        self.assertNotIn('raspberry_pi_pico2', s['boards'])

    def test_shared_port_file_is_both_roles(self):
        s = sel(['src/portable/synopsys/dwc2/dwc2_common.c'])
        self.assertFalse(s['full'])
        self.assertNotIn('raspberry_pi_pico', s['boards'])  # rp2040 is not a dwc2 family
        self.assertIn('stm32f407disco', s['boards'])        # stm32f4 is


class TestCoreRoleRule(unittest.TestCase):
    def test_usbd_selects_all_device_tests_everywhere(self):
        s = sel(['src/device/usbd.c'])
        self.assertFalse(s['full'])
        self.assertNotIn('raspberry_pi_pico2', s['boards'])  # host-only board dropped
        pico = s['boards']['raspberry_pi_pico']
        self.assertTrue(set(device_tests).issubset(set(pico)))
        self.assertTrue(set(dual_tests).issubset(set(pico)))   # dual survives device role
        self.assertTrue(all(not t.startswith('host/') for t in pico))
        # only-list board: selection intersects its only-list
        esp = s['boards']['espressif_s3_devkitm']
        self.assertEqual(esp, ['device/cdc_msc_freertos'])

    def test_host_change_drops_device(self):
        s = sel(['src/host/usbh.c'])
        self.assertFalse(s['full'])
        self.assertIn('raspberry_pi_pico2', s['boards'])
        self.assertNotIn('stm32f407disco', s['boards'])  # device-only board dropped


class TestClassRule(unittest.TestCase):
    def test_cdc_device_selects_cdc_examples_only(self):
        s = sel(['src/class/cdc/cdc_device.c'])
        self.assertFalse(s['full'])
        pico = s['boards']['raspberry_pi_pico']
        self.assertIn('device/cdc_msc', pico)
        self.assertIn('device/cdc_dual_ports', pico)
        self.assertNotIn('device/msc_dual_lun', pico)   # CFG_TUD_CDC 0 there
        self.assertNotIn('device/usbtest', pico)        # CFG_TUD_CDC 0 there
        self.assertTrue(all(not t.startswith('host/') for t in pico))

    def test_msc_host_selects_host_side(self):
        s = sel(['src/class/msc/msc_host.c'])
        self.assertFalse(s['full'])
        self.assertNotIn('stm32f407disco', s['boards'])  # device-only board
        pico2 = s['boards']['raspberry_pi_pico2']
        self.assertIn('host/msc_file_explorer', pico2)
        self.assertTrue(all(not t.startswith('device/') for t in pico2))


class TestFallbackRules(unittest.TestCase):
    def test_unknown_tool_is_full(self):
        s = sel(['tools/random_new_script.py'])
        self.assertTrue(s['full'])

    def test_docs_only_is_empty_not_full(self):
        s = sel(['docs/info/contributing.rst', 'README.rst'])
        self.assertFalse(s['full'])
        self.assertEqual(s['boards'], {})

    def test_bsp_family_selects_family_boards(self):
        s = sel(['hw/bsp/rp2040/family.cmake'])
        self.assertFalse(s['full'])
        self.assertIn('raspberry_pi_pico', s['boards'])
        self.assertEqual(s['boards']['raspberry_pi_pico'], 'all')
        self.assertNotIn('stm32f407disco', s['boards'])

    def test_bsp_board_narrows_to_board(self):
        s = sel(['hw/bsp/rp2040/boards/raspberry_pi_pico/board.h'])
        self.assertFalse(s['full'])
        self.assertEqual(list(s['boards'].keys()), ['raspberry_pi_pico'])

    def test_example_change_selects_that_example(self):
        s = sel(['examples/device/cdc_msc/src/main.c'])
        self.assertFalse(s['full'])
        self.assertEqual(s['boards']['raspberry_pi_pico'], ['device/cdc_msc'])

    def test_core_common_is_full(self):
        for f in ['src/tusb.c', 'src/common/tusb_fifo.c', 'src/osal/osal_freertos.h']:
            self.assertTrue(sel([f])['full'], f)

    def test_harness_is_full(self):
        for f in ['test/hil/hil_test.py', '.github/workflows/build.yml', 'hw/mcu/nxp/x.c', 'lib/foo/x.c']:
            self.assertTrue(sel([f])['full'], f)

    def test_mixed_roles_no_pruning(self):
        s = sel(['src/device/usbd.c', 'src/host/usbh.c'])
        self.assertFalse(s['full'])
        self.assertIn('raspberry_pi_pico2', s['boards'])
        self.assertIn('stm32f407disco', s['boards'])


if __name__ == '__main__':
    unittest.main(verbosity=1)
```

- [ ] **Step 4: Run tests to verify they fail**

Run: `python3 test/hil/test_hil_select.py 2>&1 | tail -2`
Expected: `ModuleNotFoundError: No module named 'hil_select'` (or import error).

- [ ] **Step 5: Implement the engine**

Create `test/hil/hil_select.py`:

```python
#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""PR-diff -> HIL selection: which rig boards and which tests a change can affect.

Stdlib-only (runs on bare CI runners; never imports hil_test/hil_flash/hil_lock).
Fail-open: any file no rule classifies forces the full matrix. See
docs/superpowers/specs/2026-07-29-hil-pr-scoped-selection-design.md.
"""
import argparse
import glob
import json
import os
import re
import subprocess
import sys

from hil_examples import device_tests, dual_tests, host_test

ALL_TESTS = {'device': device_tests, 'dual': dual_tests, 'host': host_test}

# class dir -> config macro suffix exceptions (rule 3); dfu is per-file, handled inline
NET_MACROS = ('ECM_RNDIS', 'NCM')

_NONCODE_RE = re.compile(
    r'^(docs/|\.claude/|.*\.(md|rst|txt)$|LICENSE)')
_FULL_RE = re.compile(
    r'^(src/common/|src/osal/|src/tusb\.c$|src/tusb\.h$|src/tusb_option\.h$|'
    r'test/hil/|\.github/workflows/build.*\.yml$|\.github/actions/|'
    r'tools/build\.py$|tools/get_deps\.py$|tools/cmake/|hw/mcu/|lib/|'
    r'hw/bsp/(family_support\.cmake|board_api\.h|board\.c|ansi_escape\.h)$|'
    r'examples/build_system/|examples/CMakeLists\.txt$)')


def test_role(test: str) -> str:
    return test.split('/', 1)[0]           # 'device' | 'dual' | 'host'


def board_roles(board: dict) -> set:
    t = board.get('tests', {})
    roles = set()
    if t.get('device'):
        roles.add('device')
    if t.get('host'):
        roles.add('host')
    if t.get('dual'):
        roles.update(('device', 'host'))
    for only in t.get('only', []):
        r = test_role(only)
        roles.update(('device', 'host') if r == 'dual' else (r,))
    return roles


def board_tests(board: dict) -> list:
    """Every test this board would run today (mirrors hil_test.test_board's default)."""
    t = board.get('tests', {})
    if 'only' in t:
        run = list(t['only'])
    else:
        run = []
        if t.get('device'):
            run += device_tests
        if t.get('dual'):
            run += dual_tests
        if t.get('host'):
            run += host_test
    return [x for x in run if x not in t.get('skip', [])]


def board_family(board_name: str, repo_root: str):
    hits = glob.glob(os.path.join(repo_root, 'hw/bsp/*/boards', board_name))
    return os.path.basename(os.path.dirname(os.path.dirname(hits[0]))) if hits else None


def port_families(port_dir: str, repo_root: str) -> set:
    fams = set()
    for f in glob.glob(os.path.join(repo_root, 'hw/bsp/*/family.cmake')) + \
             glob.glob(os.path.join(repo_root, 'hw/bsp/*/family.mk')):
        try:
            if port_dir in open(f).read():
                fams.add(os.path.basename(os.path.dirname(f)))
        except OSError:
            pass
    return fams


def _config_enables(cfg_path: str, macros) -> bool:
    try:
        text = open(cfg_path).read()
    except OSError:
        return False
    return any(re.search(rf'#define\s+{m}\s+\(?\s*0*[1-9]', text) for m in macros)


def class_examples(macros, role: str, repo_root: str) -> set:
    """Tests (from role's + dual lists) whose example config enables any macro."""
    pools = {'device': device_tests + dual_tests, 'host': host_test + dual_tests}
    out = set()
    for test in pools[role]:
        cfg = os.path.join(repo_root, 'examples', test, 'src', 'tusb_config.h')
        if _config_enables(cfg, macros):
            out.add(test)
    return out


class _Sel:
    """Accumulates contributions. board->set(tests) plus 'all-board' markers."""
    def __init__(self):
        self.full = False
        self.by_board = {}      # name -> set of tests, or 'all'
        self.roles = set()      # roles touched by any contribution
        self.reasons = []

    def add(self, boards, tests, reason):
        """tests: 'all' or iterable of test paths."""
        self.reasons.append(reason)
        for b in boards:
            cur = self.by_board.get(b)
            if tests == 'all' or cur == 'all':
                self.by_board[b] = 'all'
            else:
                self.by_board[b] = (cur or set()) | set(tests)

    def force_full(self, reason):
        self.full = True
        self.reasons.append(reason)


def _classify_one(path, repo_root, roster_boards, s: _Sel):
    base = os.path.basename(path)
    if _NONCODE_RE.match(path):
        s.reasons.append(f'{path}: non-code, no contribution')
        return
    if _FULL_RE.match(path):
        s.force_full(f'{path}: core/infra -> full matrix')
        return

    m = re.match(r'src/portable/((?:[^/]+/)?[^/]+)/', path)
    if m:
        port = m.group(1)
        if re.match(r'(dcd_|.*_device)', base):
            roles = {'device'}
        elif re.match(r'(hcd_|.*_host)', base):
            roles = {'host'}
        else:
            roles = {'device', 'host'}
        fams = port_families(port, repo_root)
        boards = [b['name'] for b in roster_boards
                  if board_family(b['name'], repo_root) in fams and (board_roles(b) & roles)]
        tests = [t for r in roles for t in ALL_TESTS[r]] + dual_tests
        s.roles.update(roles)
        s.add(boards, tests, f'{path}: port {port} -> families {sorted(fams)} -> boards {boards} ({"/".join(sorted(roles))})')
        return

    m = re.match(r'src/class/([^/]+)/', path)
    if m:
        cls = m.group(1)
        if re.search(r'_device\.[ch]$', base):
            roles = {'device'}
        elif re.search(r'_host\.[ch]$', base):
            roles = {'host'}
        else:
            roles = {'device', 'host'}
        # macro names per role
        def macros(prefix):
            if cls == 'net':
                return [f'CFG_{prefix}_{m2}' for m2 in NET_MACROS]
            if cls == 'dfu':
                if base.startswith('dfu_rt'):
                    return [f'CFG_{prefix}_DFU_RUNTIME']
                if base.startswith('dfu_device') or base.startswith('dfu_host'):
                    return [f'CFG_{prefix}_DFU']
                return [f'CFG_{prefix}_DFU', f'CFG_{prefix}_DFU_RUNTIME']
            return [f'CFG_{prefix}_{cls.upper()}']
        tests = set()
        if 'device' in roles:
            tests |= class_examples(macros('TUD'), 'device', repo_root)
        if 'host' in roles:
            tests |= class_examples(macros('TUH'), 'host', repo_root)
        boards = [b['name'] for b in roster_boards if board_roles(b) & roles]
        s.roles.update(roles)
        s.add(boards, tests, f'{path}: class {cls} -> {sorted(tests)} ({"/".join(sorted(roles))})')
        return

    m = re.match(r'src/(device|host)/', path)
    if m:
        role = m.group(1)
        boards = [b['name'] for b in roster_boards if role in board_roles(b)]
        s.roles.add(role)
        s.add(boards, ALL_TESTS[role] + dual_tests, f'{path}: core {role} stack -> all {role} tests')
        return

    m = re.match(r'hw/bsp/([^/]+)/(?:boards/([^/]+)/)?', path)
    if m:
        fam, brd = m.group(1), m.group(2)
        if brd:
            boards = [b['name'] for b in roster_boards if b['name'] == brd]
            why = f'{path}: bsp board {brd}'
        else:
            boards = [b['name'] for b in roster_boards
                      if board_family(b['name'], repo_root) == fam]
            why = f'{path}: bsp family {fam}'
        s.roles.update(('device', 'host'))
        s.add(boards, 'all', f'{why} -> boards {boards}')
        return

    m = re.match(r'examples/(device|host|dual)/([^/]+)/', path)
    if m:
        test = f'{m.group(1)}/{m.group(2)}'
        known = any(test in pool for pool in ALL_TESTS.values())
        if known:
            boards = [b['name'] for b in roster_boards]
            role = test_role(test)
            s.roles.update(('device', 'host') if role == 'dual' else (role,))
            s.add(boards, [test], f'{path}: example -> {test} on all boards')
        else:
            s.reasons.append(f'{path}: example not in HIL lists, no contribution')
        return

    s.force_full(f'{path}: unclassified -> full matrix')


def classify(changed_files, repo_root, rosters):
    all_boards = []
    seen = set()
    for _, boards in rosters:
        for b in boards:
            if b['name'] not in seen:
                seen.add(b['name'])
                all_boards.append(b)

    s = _Sel()
    for path in changed_files:
        _classify_one(path, repo_root, all_boards, s)
        if s.full:
            break

    if s.full:
        return {'full': True, 'boards': {b['name']: 'all' for b in all_boards},
                'reasons': s.reasons}

    # role pruning: single-role selections drop the other role's tests and boards
    by_name = {b['name']: b for b in all_boards}
    out = {}
    for name, tests in s.by_board.items():
        allowed = board_tests(by_name[name])
        if tests == 'all':
            kept = list(allowed)
        else:
            kept = [t for t in allowed if t in tests]
        if s.roles and s.roles != {'device', 'host'}:
            role = next(iter(s.roles))
            kept = [t for t in kept if test_role(t) in (role, 'dual')]
        if kept:
            out[name] = 'all' if set(kept) == set(allowed) else sorted(kept)
    return {'full': False, 'boards': out, 'reasons': s.reasons}
```

- [ ] **Step 6: Run tests to verify they pass**

Run: `python3 test/hil/test_hil_select.py`
Expected: all tests PASS (OK line). Iterate on the engine (not the tests) until green; if a
test premise contradicts the repo (e.g. a family name), verify against the tree and fix the
test only with evidence noted in your report.

- [ ] **Step 7: Commit**

```bash
git add test/hil/hil_examples.py test/hil/hil_select.py test/hil/test_hil_select.py test/hil/hil_test.py test/hil/hil_ci.sh
git commit -m "hil: add PR-diff selection engine (hil_select) with shared example lists"
```

---

### Task 2: CLI + args emission

**Files:**
- Modify: `test/hil/hil_select.py` (add `selection_args`, `main`)
- Modify: `test/hil/test_hil_select.py` (add CLI/args tests)

**Interfaces:**
- Consumes: Task 1's `classify` and roster shapes.
- Produces:
  - `selection_args(sel: dict, rosters) -> dict` mapping each config path's basename to the
    `hil_test.py` argument string for that rig: for each selected board ON that roster,
    `-b <name>`, plus `-bt <name>:<t1>,<t2>` when the board's entry is a list (not 'all').
    Empty string when no selected board is on that roster. When `sel['full']`, every roster
    board gets bare `-b`? NO — full means "today's behavior": `selection_args` returns `''`
    for every config (no filtering args at all).
  - CLI: `python3 test/hil/hil_select.py [--base REF | --diff-file PATH] CONFIG...` printing
    the JSON `{'full', 'boards', 'args', 'reasons'}` to stdout, reasons also to stderr
    (one line each, prefixed `hil_select: `). Non-zero exit only on operational errors
    (bad ref, unreadable config) — never on an empty selection.

- [ ] **Step 1: Add failing CLI/args tests to `test_hil_select.py`**

```python
class TestArgsEmission(unittest.TestCase):
    def test_args_for_scoped_selection(self):
        s = sel(['src/portable/raspberrypi/rp2040/dcd_rp2040.c'])
        args = hil_select.selection_args(s, ROSTERS)
        a = args['tinyusb.json']
        self.assertIn('-b raspberry_pi_pico', a)
        self.assertNotIn('stm32f407disco', a)
        self.assertIn('-bt raspberry_pi_pico:', a)   # device-only subset of a device+host board

    def test_args_full_is_empty(self):
        s = sel(['tools/random_new_script.py'])
        self.assertEqual(hil_select.selection_args(s, ROSTERS), {'tinyusb.json': ''})

    def test_args_all_board_gets_bare_b(self):
        s = sel(['hw/bsp/rp2040/boards/raspberry_pi_pico/board.h'])
        a = hil_select.selection_args(s, ROSTERS)['tinyusb.json']
        self.assertIn('-b raspberry_pi_pico', a)
        self.assertNotIn('-bt', a)

    def test_cli_diff_file(self):
        import subprocess, tempfile, json as j
        with tempfile.NamedTemporaryFile('w', suffix='.txt', delete=False) as f:
            f.write('src/class/cdc/cdc_device.c\n')
            path = f.name
        r = subprocess.run([sys.executable, os.path.join(REPO, 'test/hil/hil_select.py'),
                            '--diff-file', path, os.path.join(REPO, 'test/hil/tinyusb.json')],
                           capture_output=True, text=True)
        self.assertEqual(r.returncode, 0, r.stderr)
        out = j.loads(r.stdout)
        self.assertFalse(out['full'])
        self.assertIn('tinyusb.json', out['args'])
        self.assertTrue(any('cdc_device' in line for line in out['reasons']))
        os.unlink(path)
```

- [ ] **Step 2: Run to verify the new tests fail**

Run: `python3 test/hil/test_hil_select.py 2>&1 | tail -3`
Expected: failures/errors mentioning `selection_args`.

- [ ] **Step 3: Implement `selection_args` and `main`**

Append to `hil_select.py`:

```python
def selection_args(sel, rosters):
    args = {}
    for cfg_path, boards in rosters:
        key = os.path.basename(cfg_path)
        if sel['full']:
            args[key] = ''
            continue
        parts = []
        for b in boards:
            chosen = sel['boards'].get(b['name'])
            if chosen is None:
                continue
            parts.append(f'-b {b["name"]}')
            if chosen != 'all':
                parts.append(f'-bt {b["name"]}:{",".join(chosen)}')
        args[key] = ' '.join(parts)
    return args


def changed_files_from_git(base, repo_root):
    mb = subprocess.run(['git', 'merge-base', 'HEAD', base], cwd=repo_root,
                        capture_output=True, text=True, check=True).stdout.strip()
    diff = subprocess.run(['git', 'diff', '--name-only', f'{mb}..HEAD'], cwd=repo_root,
                          capture_output=True, text=True, check=True).stdout
    return [l for l in diff.splitlines() if l.strip()]


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    g = ap.add_mutually_exclusive_group(required=True)
    g.add_argument('--base', help='git ref to diff against (merge-base..HEAD)')
    g.add_argument('--diff-file', help='newline-separated changed-file list')
    ap.add_argument('configs', nargs='+', help='rig roster JSON file(s)')
    a = ap.parse_args()

    repo_root = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    rosters = []
    for c in a.configs:
        with open(c) as f:
            rosters.append((c, json.load(f)['boards']))

    files = (open(a.diff_file).read().splitlines() if a.diff_file
             else changed_files_from_git(a.base, repo_root))
    files = [f for f in files if f.strip()]

    s = classify(files, repo_root, rosters)
    s['args'] = selection_args(s, rosters)
    for r in s['reasons']:
        print(f'hil_select: {r}', file=sys.stderr)
    print(json.dumps(s))


if __name__ == '__main__':
    main()
```

(The `parts.append f'...'` line above is pseudo-highlighted; write valid Python:
`parts.append(f'-bt {b["name"]}:{",".join(chosen)}')`.)

- [ ] **Step 4: Run the full suite**

Run: `python3 test/hil/test_hil_select.py && chmod +x test/hil/hil_select.py`
Expected: OK.

- [ ] **Step 5: Smoke against the real repo state**

Run: `python3 test/hil/hil_select.py --base HEAD test/hil/tinyusb.json test/hil/hfp.json`
Expected: empty diff ⇒ `{"full": false, "boards": {}, "args": {"tinyusb.json": "", "hfp.json": ""}, ...}` exit 0.
Then: `printf 'src/portable/wch/dcd_ch32_usbfs.c\n' > /tmp/d.txt && python3 test/hil/hil_select.py --diff-file /tmp/d.txt test/hil/tinyusb.json | python3 -m json.tool | head -20`
Expected: only WCH-family boards (nanoch32v203, ch32v103r_r1_1v0, ch32v307v_r1_1v0 — whichever reference that port) with device tests.

- [ ] **Step 6: Commit**

```bash
git add test/hil/hil_select.py test/hil/test_hil_select.py
git commit -m "hil: hil_select CLI with per-rig hil_test argument emission"
```

---

### Task 3: hil_ci_set_matrix --select + build.yml wiring

**Files:**
- Modify: `test/hil/hil_ci_set_matrix.py`
- Modify: `.github/workflows/build.yml` (set-matrix job; hil-build consumers unchanged; hil-tinyusb + hil-tinyusb-esp steps)

**Interfaces:**
- Consumes: Task 2's CLI JSON (`full`, `boards`, `args`).
- Produces:
  - `hil_ci_set_matrix.py [--select JSON_STRING] CONFIG...`: with `--select` and
    `full == false`, boards not in `select['boards']` are skipped when building the toolchain
    buckets; otherwise identical behavior. Buckets stay present (possibly `[]`) so
    `fromJSON(...)[toolchain]` keeps resolving.
  - set-matrix outputs: `hil_select_json` (compact selection), `hil_args_tinyusb`,
    `hil_args_hfp`, `hil_run_tinyusb`, `hil_run_hfp` (string 'true'/'false').

- [ ] **Step 1: Add `--select` to `hil_ci_set_matrix.py`**

In `main()` add:

```python
    parser.add_argument('--select', help='hil_select.py JSON; scopes boards when full=false')
```

and after parsing:

```python
    selected = None
    sel = json.loads(args.select) if args.select else None
    if sel and not sel.get('full'):
        selected = set(sel.get('boards', {}))
```

then inside the per-board loop, first line:

```python
            if selected is not None and board['name'] not in selected:
                continue
```

- [ ] **Step 2: Verify byte-identical without --select and scoped with it**

Run: `python3 test/hil/hil_ci_set_matrix.py test/hil/tinyusb.json test/hil/hfp.json > /tmp/m1.json && git stash -q && python3 test/hil/hil_ci_set_matrix.py test/hil/tinyusb.json test/hil/hfp.json > /tmp/m0.json && git stash pop -q && diff /tmp/m0.json /tmp/m1.json && echo identical`
Expected: `identical`.
Then: `python3 test/hil/hil_ci_set_matrix.py --select '{"full": false, "boards": {"raspberry_pi_pico": "all"}}' test/hil/tinyusb.json test/hil/hfp.json`
Expected: JSON whose `arm-gcc` list contains only the raspberry_pi_pico entry, `riscv-gcc`/`esp-idf` = [].

- [ ] **Step 3: Wire set-matrix in `.github/workflows/build.yml`**

In the `set-matrix` job: give the checkout full history and add the selection step between
checkout and matrix generation; make the HIL matrix use it:

```yaml
      - name: Checkout TinyUSB
        uses: actions/checkout@v6
        with:
          fetch-depth: 0

      - name: HIL selection (PR only)
        id: hil-select
        if: github.event_name == 'pull_request'
        run: |
          python3 test/hil/test_hil_select.py
          SELECT_JSON=$(python3 test/hil/hil_select.py --base "origin/${{ github.base_ref }}" test/hil/tinyusb.json test/hil/hfp.json)
          echo "select=$SELECT_JSON" >> $GITHUB_OUTPUT
          python3 - "$SELECT_JSON" >> $GITHUB_OUTPUT <<'EOF'
          import json, sys
          s = json.loads(sys.argv[1])
          args = s.get('args', {})
          for cfg, key in (('tinyusb.json', 'tinyusb'), ('hfp.json', 'hfp')):
              a = args.get(cfg, '')
              run = 'true' if (s['full'] or a) else 'false'
              print(f'args_{key}={a}')
              print(f'run_{key}={run}')
          EOF
```

and in the existing "Generate matrix json" step, change the HIL line to:

```yaml
          # HIL matrix (merged from tinyusb + hifiphile configs), scoped on PRs
          SELECT='${{ steps.hil-select.outputs.select }}'
          HIL_MATRIX_JSON=$(python test/hil/hil_ci_set_matrix.py ${SELECT:+--select "$SELECT"} test/hil/tinyusb.json test/hil/hfp.json)
```

Add to the job's `outputs:` block:

```yaml
      hil_args_tinyusb: ${{ steps.hil-select.outputs.args_tinyusb }}
      hil_args_hfp: ${{ steps.hil-select.outputs.args_hfp }}
      hil_run_tinyusb: ${{ steps.hil-select.outputs.run_tinyusb }}
      hil_run_hfp: ${{ steps.hil-select.outputs.run_hfp }}
```

(On non-PR events the step is skipped: outputs are empty strings — the consumers below treat
empty `run_*` as 'true' and empty args as no filtering, i.e. today's behavior.)

- [ ] **Step 4: Wire the rig jobs**

In the `hil-tinyusb` job (the matrixed one covering both rigs), find the step that runs
`hil_test.py --retry 1 ${{ matrix.test_args }} ${{ env.HIL_JSON }} $RERUN_ARGS` (~line 360)
and change the step's `run:` to select per-rig args and honor the skip flag:

```yaml
        run: |
          case "$HIL_JSON" in
            *tinyusb.json) SEL_ARGS='${{ needs.set-matrix.outputs.hil_args_tinyusb }}'; SEL_RUN='${{ needs.set-matrix.outputs.hil_run_tinyusb }}' ;;
            *hfp.json)     SEL_ARGS='${{ needs.set-matrix.outputs.hil_args_hfp }}';     SEL_RUN='${{ needs.set-matrix.outputs.hil_run_hfp }}' ;;
          esac
          if [ "$SEL_RUN" = "false" ]; then echo "HIL skipped by PR selection (no affected boards on this rig)"; exit 0; fi
          python3 test/hil/hil_test.py --retry 1 ${{ matrix.test_args }} $SEL_ARGS ${{ env.HIL_JSON }} $RERUN_ARGS
```

Apply the same pattern to the second `hil_test.py` invocation at ~line 423 (`hil-tinyusb-esp`,
which is tinyusb-rig only: use the `hil_args_tinyusb`/`hil_run_tinyusb` outputs directly, no
case needed) and to the hfp job's direct `python3 test/hil/hil_test.py hfp.json` call at
~line 487 (use `hil_args_hfp`/`hil_run_hfp`). Preserve each step's existing surrounding lines
(report-dir env, RERUN_ARGS logic) — only inject the SEL_ARGS/SEL_RUN mechanics.

- [ ] **Step 5: Validate the YAML and the exact shell locally**

Run: `pre-commit run check-yaml --files .github/workflows/build.yml && python3 -c "import yaml; yaml.safe_load(open('.github/workflows/build.yml')); print('yaml ok')"`
Expected: `yaml ok` (pyyaml is available; if not, `pip install --user pyyaml` first).
Also simulate the selection step's python inline script:
`SELECT_JSON=$(python3 test/hil/hil_select.py --diff-file /tmp/d.txt test/hil/tinyusb.json test/hil/hfp.json) && python3 -c "import json,sys; s=json.loads(sys.argv[1]); print(s['args'])" "$SELECT_JSON"`
Expected: the args dict prints.

- [ ] **Step 6: Commit**

```bash
git add test/hil/hil_ci_set_matrix.py .github/workflows/build.yml
git commit -m "ci: scope HIL build+test matrix by PR diff via hil_select"
```

---

### Task 4: pre-pr + hil skill docs, final validation

**Files:**
- Modify: `.claude/skills/pre-pr/SKILL.md` (mapping section delegates to the selector)
- Modify: `.claude/skills/hil/SKILL.md` (document the selector for manual runs)

**Interfaces:**
- Consumes: Task 2's CLI.

- [ ] **Step 1: Rewrite pre-pr's "2. Map changes to boards" section**

Replace the section's grep heuristics (keep its numbered-section structure and the roster/cap
policy) with:

```markdown
## 2. Map changes to boards

- `python3 test/hil/hil_select.py --base $BASE test/hil/tinyusb.json` → JSON with the affected
  rig boards (`boards`) and per-file `reasons`. `full: true` means a broad/infra change.
- Build-board sampling: from the selection's boards (or, when `full`, the representative set
  `stm32f407disco` + `raspberry_pi_pico`), pick ONE board per family, preferring rig-roster
  boards; cap at 4 and tell the user which families the cap dropped. The boards list must
  NEVER end up empty — final fallback is `[stm32f407disco]`.
- A `full: true` selection or an empty one (docs-only) keeps today's behavior: minimal
  software-only gate for docs-only, representative set otherwise.
```

- [ ] **Step 2: Add a short "PR-scoped selection" note to the hil skill**

Append to `.claude/skills/hil/SKILL.md` after the pool-check section:

```markdown
## PR-scoped selection

`test/hil/hil_select.py` maps a diff to affected boards/tests (used by CI on PRs; fail-open
to the full matrix). Manual use:

```bash
ARGS=$(python3 test/hil/hil_select.py --base master test/hil/tinyusb.json | python3 -c "import json,sys; print(json.load(sys.stdin)['args']['tinyusb.json'])")
python3 test/hil/hil_test.py -B examples $ARGS test/hil/tinyusb.json
```

Unit suite: `python3 test/hil/test_hil_select.py` (no hardware).
```

- [ ] **Step 3: Full validation sweep**

Run: `python3 test/hil/test_hil_select.py && python3 -m py_compile test/hil/hil_select.py test/hil/hil_examples.py test/hil/hil_ci_set_matrix.py test/hil/hil_test.py && python3 test/hil/hil_test.py --help >/dev/null && pre-commit run --files $(git diff --name-only claude/hil-pool-check..HEAD) && echo ALL-GREEN`
Expected: `ALL-GREEN`.

- [ ] **Step 4: Real-diff spot checks (acceptance)**

Run each and eyeball the JSON (record outputs in your report):
```bash
for f in 'src/portable/raspberrypi/rp2040/dcd_rp2040.c' 'src/device/usbd.c' 'src/class/cdc/cdc_device.c' 'src/host/usbh.c'; do
  printf '%s\n' "$f" > /tmp/d.txt
  echo "=== $f"; python3 test/hil/hil_select.py --diff-file /tmp/d.txt test/hil/tinyusb.json test/hil/hfp.json 2>/dev/null | python3 -m json.tool | sed -n '1,25p'
done
```
Expected: matches the spec's acceptance examples (pico-family only / all-device / CDC examples
only / host side only, with hfp.json args populated only where hfp boards qualify).

- [ ] **Step 5: Commit**

```bash
git add .claude/skills/pre-pr/SKILL.md .claude/skills/hil/SKILL.md
git commit -m "docs: pre-pr and hil skill use hil_select for PR-scoped boards"
```
