# hil_test.py Split Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Split `test/hil/hil_test.py` (2370 ln) into a test-focused core plus `hil_lock.py` (board locks + controller permits + operator CLI, superseding `board_lock.py`) and `hil_flash.py` (run_cmd + flash backends + firmware/serial lookup), with no behavior change.

**Architecture:** Pure code motion per `docs/superpowers/specs/2026-07-28-hil-test-refactor-design.md`. Import graph: `hil_test` → {`hil_lock`, `hil_flash`}; helpers import nothing local. Call sites use module-qualified names (`hil_lock.flash_permit(...)`), never wildcard mirroring.

**Tech Stack:** Python 3.11+ (existing `TypedDict`/`NotRequired` usage), stdlib only in the helpers (fcntl, json, glob, multiprocessing objects passed in).

## Global Constraints

- Work in worktree `.claude/worktrees/hil-test-split` (branch `claude/hil-test-split`); never touch the primary checkout.
- Behavior-preserving: `hil_test.py` CLI args, log lines, report format, lock/permit semantics, flash behavior all byte-identical. The ONLY user-visible change is the CLI filename `board_lock.py` → `hil_lock.py`.
- Moved functions are moved **verbatim** — no reformatting, no comment editing, no "improvements". A diff of a moved function's body against its old self must be empty.
- Commit messages: imperative, scoped, no Co-Authored-By/Claude-Session trailers.
- Every commit leaves the tree working: `python3 -m py_compile` clean on all touched modules, and `python3 .claude/skills/hil/pool_check.py --scan-only` exits 0 (safe on the rig: scan-only takes no locks, flashes nothing).
- Hardware steps (Task 4) run on the `ci` rig only, from this worktree, and rely on the tools' own board flocks — never pre-hold boards you are about to run `hil_test.py`/`pool_check.py` on.

---

### Task 1: Create hil_flash.py; repoint hil_test + pool_check flash call sites

**Files:**
- Create: `test/hil/hil_flash.py`
- Modify: `test/hil/hil_test.py` (delete moved code; add import; qualify call sites)
- Modify: `.claude/skills/hil/pool_check.py` (flash-related imports)
- Modify: `test/hil/hil_ci.sh` (scp list)

**Interfaces:**
- Produces (used by Tasks 2-4): module `hil_flash` with `CMD_TIMEOUT`, `run_cmd(cmd, cwd=None, timeout=CMD_TIMEOUT)`, `cmd_stdout_text(out)`, `OPENCOD_ADI_PATH`, `TINYUSB_ROOT`, `flash_jlink/reset_jlink`, `flash_stlink/reset_stlink`, `flash_stflash/reset_stflash`, `flash_openocd/reset_openocd`, `flash_openocd_wch/reset_openocd_wch`, `flash_openocd_adi/reset_openocd_adi`, `flash_wlink_rs/reset_wlink_rs`, `flash_esptool/reset_esptool`, `flash_uniflash/reset_uniflash`, `flash_lm4flash/reset_lm4flash`, `find_firmware(variant, example)`, `get_serial_dev(id, vendor_str, product_str, ifnum)`, module globals `build_dir = 'cmake-build'`, `verbose = False`.

- [ ] **Step 1: Create `test/hil/hil_flash.py`**

Header (new code), then the moved blocks verbatim:

```python
#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Firmware flashing for the TinyUSB HIL rig: run_cmd, one flash_*/reset_* pair per
# flasher type (dispatched by config name via getattr), find_firmware, and the
# fixture serial-port resolver get_serial_dev (here, not hil_test: flash_esptool
# needs it and helpers must not import hil_test).
# Callers set module globals `build_dir` and `verbose` (hil_test.main from argparse,
# pool_check directly) exactly as they set hil_test's globals today.

import glob
import json
import os
import signal
import subprocess
import sys
from pathlib import Path

verbose = False
build_dir = 'cmake-build'
```

Then MOVE (cut from `hil_test.py`, paste unchanged, in this order):
1. `CMD_TIMEOUT = int(os.getenv('HIL_CMD_TIMEOUT', '180'))` (from the constants block; leave `POOL_TIMEOUT`/`SERIAL_*_TIMEOUT` in hil_test)
2. `def cmd_stdout_text(out)`
3. `OPENCOD_ADI_PATH = Path.home() / 'app' / 'openocd_adi'` and `TINYUSB_ROOT = Path(__file__).resolve().parents[2]`
4. `def get_serial_dev(id, vendor_str, product_str, ifnum)`
5. `def run_cmd(cmd, cwd=None, timeout=CMD_TIMEOUT)`
6. All ten `flash_*`/`reset_*` pairs listed in Interfaces, in current file order
7. `def find_firmware(variant, example)`

- [ ] **Step 2: Delete the moved code from `hil_test.py` and qualify call sites**

In `hil_test.py`: add `import hil_flash` under the existing imports; delete the moved definitions and the `build_dir = 'cmake-build'` global (line ~165) plus `global build_dir` in `main`. Repoint every use, all module-qualified:
- `globals()[f'flash_{...}']` → `getattr(hil_flash, f'flash_{...}')` (1 site, in `test_example`)
- `globals()[f'reset_{...}']` → `getattr(hil_flash, f'reset_{...}')` (3 sites: `test_host_device_info`, `test_host_cdc_msc_hid`, `test_host_msc_file_explorer`)
- bare `run_cmd(` → `hil_flash.run_cmd(` ; `cmd_stdout_text(` → `hil_flash.cmd_stdout_text(` ; `find_firmware(` → `hil_flash.find_firmware(` ; `get_serial_dev(` → `hil_flash.get_serial_dev(` ; `TINYUSB_ROOT` → `hil_flash.TINYUSB_ROOT` (in `build_board`, `CONTROLLER_CACHE` stays hil_test-local)
- In `main()`: `build_dir = args.build_dir` → `hil_flash.build_dir = args.build_dir`; where `verbose` is set, add `hil_flash.verbose = args.verbose` (hil_test keeps its own `verbose` for test-side prints)
- `run_cmd`'s `elif verbose:` branch now reads `hil_flash.verbose` (it moved with the function — verify it references the module-local name, not hil_test's)

Find every remaining call site mechanically:

Run: `grep -nE 'run_cmd|cmd_stdout_text|find_firmware|get_serial_dev|flash_[a-z]|reset_[a-z]|TINYUSB_ROOT|OPENCOD' test/hil/hil_test.py | grep -v hil_flash`
Expected: only hits inside comments/strings and the `reset_{flasher}` dispatch f-strings already qualified.

- [ ] **Step 3: Repoint pool_check's flash imports**

In `.claude/skills/hil/pool_check.py`: add `import hil_flash` next to `import hil_test`; replace `hil_test.find_firmware` → `hil_flash.find_firmware` (3 sites), `hil_test.cmd_stdout_text` → `hil_flash.cmd_stdout_text`, `hil_test.get_serial_dev` → `hil_flash.get_serial_dev`, `hil_test.TINYUSB_ROOT` → `hil_flash.TINYUSB_ROOT`, `hil_test.build_dir` → `hil_flash.build_dir` (2 sites incl. `main`'s assignment), `hil_test.verbose = args.verbose` → `hil_flash.verbose = args.verbose`, `getattr(hil_test, f'flash_...')`/`getattr(hil_test, f'reset_...')` → `getattr(hil_flash, ...)` (4 sites). Keep `import hil_test` and the pymtp shim for now (locks still live there; removed in Task 2).

- [ ] **Step 4: Add hil_flash.py to the hil_ci.sh scp list**

```bash
scp -q "$ROOT_DIR/test/hil/hil_test.py" \
       "$ROOT_DIR/test/hil/hil_flash.py" \
       "$ROOT_DIR/test/hil/pymtp.py" \
       "$CONFIG" \
       "$REMOTE:$REMOTE_DIR/test/hil/"
```

- [ ] **Step 5: Verify**

Run: `python3 -m py_compile test/hil/hil_flash.py test/hil/hil_test.py .claude/skills/hil/pool_check.py && python3 test/hil/hil_test.py --help >/dev/null && python3 .claude/skills/hil/pool_check.py --scan-only`
Expected: compiles; help prints nothing to stderr; scan-only prints the table and exits 0.

- [ ] **Step 6: Commit**

```bash
git add test/hil/hil_flash.py test/hil/hil_test.py test/hil/hil_ci.sh .claude/skills/hil/pool_check.py
git commit -m "hil: extract flashing into hil_flash.py"
```

---

### Task 2: Create hil_lock.py core (flock protocol + controller permits); repoint hil_test + pool_check

**Files:**
- Create: `test/hil/hil_lock.py`
- Modify: `test/hil/hil_test.py`
- Modify: `.claude/skills/hil/pool_check.py`
- Modify: `test/hil/hil_ci.sh`

**Interfaces:**
- Produces: module `hil_lock` with `BOARD_LOCK_DIR`, `CI_REASON = 'hil_test.py'`, `lock_path(board)`, `flock_nb(board)`, `write_record(fh, reason)`, `clear_record(fh)`, `read_record(board)`, `acquire_board_lock(board, reason=CI_REASON)`, `FLASH_PARALLEL`, `USBTEST_PARALLEL`, `CONTROLLER_SLOTS`, `controller_of(uid)`, `controller_slot(pci)`, `controller_permit`, `flash_permit(uid)`, `usbtest_permit(uid)`, `init_scheduling(b_sems, f_sems, cmap, cmeta, hints, log_fn=None)`.

- [ ] **Step 1: Create `test/hil/hil_lock.py` with the flock core**

New code (the protocol, factored from today's three copies — `board_lock.py` `cmd_hold`/`read_info`, `hil_test.acquire_board_lock`, pool_check `lock_board`; behavior identical to `hil_test.acquire_board_lock` for the acquire path):

```python
#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Board locks + controller permits for the TinyUSB HIL rig.

Board locks are kernel flocks in BOARD_LOCK_DIR arbitrating hardware access
between dev sessions and CI's hil_test.py (never stop the actions-runner).
Controller permits are in-process semaphores budgeting flashes and usbtest
batteries per host controller; they have no CLI meaning. The CLI below
(hold/release/status) manages board locks only; it supersedes board_lock.py.
"""
import argparse
import fcntl
import glob
import json
import os
import re
import select
import signal
import sys
import time

BOARD_LOCK_DIR = '/tmp/tinyusb-hil-locks'
CI_REASON = 'hil_test.py'   # release-protected holder tag (release refuses to kill it)
PROFILE = os.environ.get('HIL_PROFILE') == '1'


def lock_path(board: str) -> str:
    return os.path.join(BOARD_LOCK_DIR, f'{board}.lock')


def flock_nb(board: str):
    """Open-or-create the lock file WITHOUT truncating (a losing racer must not
    wipe the winner's record) and take LOCK_EX|LOCK_NB. Returns the open handle;
    raises OSError when the flock is held elsewhere (handle already closed)."""
    fd = os.open(lock_path(board), os.O_RDWR | os.O_CREAT, 0o666)
    fh = os.fdopen(fd, 'r+')
    try:
        fcntl.flock(fh, fcntl.LOCK_EX | fcntl.LOCK_NB)
    except OSError:
        fh.close()
        raise
    return fh


def write_record(fh, reason: str) -> None:
    """Best-effort holder record; the flock itself is already held."""
    try:
        fh.truncate(0)
        fh.seek(0)
        json.dump({'pid': os.getpid(), 'reason': reason,
                   'since': time.strftime('%Y-%m-%dT%H:%M:%S%z')}, fh)
        fh.flush()
    except OSError:
        pass


def clear_record(fh) -> None:
    """Clear our record before dropping the flock so records stay truthful."""
    try:
        fh.truncate(0)
    except OSError:
        pass


def read_record(board: str):
    try:
        with open(lock_path(board)) as f:
            return json.load(f)
    except (OSError, ValueError):
        return None
```

Then MOVE `acquire_board_lock` from `hil_test.py` verbatim, with exactly two mechanical edits: signature becomes `def acquire_board_lock(board_name, reason=CI_REASON):` and the record-write dict's `'reason': 'hil_test.py'` becomes `'reason': reason`. Do NOT rewrite its body in terms of `flock_nb` — on conflict it reads holder info from the still-open handle before closing, which `flock_nb` (closes on conflict) cannot provide; the fail-open warning text and RuntimeError message must survive character-for-character.

- [ ] **Step 2: Move the controller-permit block into `hil_lock.py`**

MOVE verbatim from `hil_test.py`: the scheduling comment block + `FLASH_PARALLEL`, `USBTEST_PARALLEL`, `CONTROLLER_SLOTS`, the five module globals (`usbtest_sems`, `flash_sems`, `controller_map`, `controller_meta`, `controller_hints`), `controller_of`, `controller_slot`, `controller_permit`, `flash_permit`, `usbtest_permit`. Two mechanical adaptations:
- add at module scope `log = print` and a setter, replacing the two `log_line(...)` calls inside `controller_of`/`controller_permit` with `log(...)`:

```python
log = print  # hil_test.init_worker points this at log_line via init_scheduling


def init_scheduling(b_sems, f_sems, cmap, cmeta, hints, log_fn=None):
    """Install per-worker scheduling state (called from hil_test.init_worker)."""
    global usbtest_sems, flash_sems, controller_map, controller_meta, controller_hints, log
    usbtest_sems, flash_sems = b_sems, f_sems
    controller_map, controller_meta, controller_hints = cmap, cmeta, hints
    if log_fn is not None:
        log = log_fn
```

- `PROFILE` inside `controller_permit` now resolves to hil_lock's own module constant (defined in Step 1).

- [ ] **Step 3: Repoint `hil_test.py`**

Add `import hil_lock`. Delete the moved lock + permit code and the five globals. `init_worker` keeps its exact signature and initargs; its body sets the hil_test globals it still owns (`print_lock`, `shuffle_seed`) and forwards the rest:

```python
def init_worker(lock, seed, b_mutexes, f_sems, cmap, cmeta, hints_by_uid):
    global print_lock, shuffle_seed
    print_lock = lock
    shuffle_seed = seed
    hil_lock.init_scheduling(b_mutexes, f_sems, cmap, cmeta, hints_by_uid, log_fn=log_line)
```

Qualify remaining uses: `acquire_board_lock(name)` → `hil_lock.acquire_board_lock(name)` (in `test_board`), `flash_permit(` → `hil_lock.flash_permit(`, `usbtest_permit(` → `hil_lock.usbtest_permit(`, and `main()`'s startup log line + Semaphore construction read `hil_lock.FLASH_PARALLEL`/`hil_lock.USBTEST_PARALLEL`/`hil_lock.CONTROLLER_SLOTS`. `controller_map` reads in the hint-persistence block of `main` use the Manager dict it already holds locally (`cmap`) — no hil_lock global access there; verify.

- [ ] **Step 4: Repoint pool_check to hil_lock and drop its private copies + hil_test import**

In `pool_check.py`: replace `lock_board`/`unlock_board` bodies with the shared core —

```python
import hil_lock

def lock_board(name: str):
    try:
        fh = hil_lock.flock_nb(name)
    except OSError:
        info = hil_lock.read_record(name)
        return json.dumps(info) if info else 'unknown holder'
    hil_lock.write_record(fh, 'pool_check')
    return fh


def unlock_board(fh) -> None:
    hil_lock.clear_record(fh)
    fh.close()
```

(Behavior note: `lock_board` currently returns the raw record text; JSON-dumping the parsed record is equivalent for display. `hil_lock.BOARD_LOCK_DIR` replaces `hil_test.BOARD_LOCK_DIR`; `os.makedirs(...)` call stays, now on `hil_lock.BOARD_LOCK_DIR`.) Then delete `import hil_test` and the pymtp stub block (`try: import pymtp ... sys.modules['pymtp'] = ...`) — pool_check now imports only `hil_lock` + `hil_flash`.

Run: `grep -n 'hil_test' .claude/skills/hil/pool_check.py`
Expected: only the docstring mention of the protocol/history, no code references (update the docstring's "imports test/hil/hil_test.py" line to name hil_lock/hil_flash).

- [ ] **Step 5: Add hil_lock.py to the hil_ci.sh scp list** (same block as Task 1 Step 4, one more line: `"$ROOT_DIR/test/hil/hil_lock.py" \`)

- [ ] **Step 6: Verify**

Run: `python3 -m py_compile test/hil/hil_lock.py test/hil/hil_test.py .claude/skills/hil/pool_check.py && python3 test/hil/hil_test.py --help >/dev/null && python3 .claude/skills/hil/pool_check.py --scan-only`
Expected: clean compile, working scan table, exit 0.

- [ ] **Step 7: Commit**

```bash
git add test/hil/hil_lock.py test/hil/hil_test.py test/hil/hil_ci.sh .claude/skills/hil/pool_check.py
git commit -m "hil: extract board locks and controller permits into hil_lock.py"
```

---

### Task 3: Absorb board_lock.py CLI into hil_lock.py; delete board_lock.py; rename in docs

**Files:**
- Modify: `test/hil/hil_lock.py` (append CLI)
- Delete: `test/hil/board_lock.py`
- Modify: `.claude/skills/hil/SKILL.md`, `.claude/agents/hil-operator.md`, `.claude/agents/target-debugger.md`, `.claude/skills/etm-trace/SKILL.md`, `.claude/skills/usb-kernel-recover/SKILL.md`, `.claude/skills/target-debug/SKILL.md`

**Interfaces:**
- Produces: `python3 test/hil/hil_lock.py hold|release|status` — identical subcommands, flags, output, and exit codes to today's `board_lock.py`.

- [ ] **Step 1: Move the CLI from `board_lock.py` into `hil_lock.py`**

MOVE verbatim to the end of `hil_lock.py`: `boards_from_config`, `is_locked`, `cmd_hold`, `cmd_release`, `cmd_status`, `main()`, and the `if __name__ == '__main__':` guard. Mechanical adaptations only:
- `LOCK_DIR` → `BOARD_LOCK_DIR` (all sites), `lock_path` already exists (delete the duplicate), `read_info` → `read_record` (all sites; delete the duplicate definition)
- `cmd_hold`'s holder loop body (the open/flock/json.dump block) becomes `fh = flock_nb(b)` + `write_record(fh, reason)` inside the existing try/except OSError
- `_bow_out`'s per-handle truncate loop becomes `clear_record(h)` per handle
- `cmd_release`'s probe uses `flock_nb(b)` in a try/except OSError (held → existing record/victim logic, with the literal `'hil_test.py'` comparison becoming `CI_REASON`); the free-path truncate becomes `clear_record(fh)`
- `main()`'s module docstring reference for `--help` text: keep the usage lines, updating the tool name to `hil_lock.py`

Then delete `test/hil/board_lock.py` (`git rm test/hil/board_lock.py`).

- [ ] **Step 2: Rename `board_lock.py` → `hil_lock.py` in the six live docs**

Run: `cd <worktree> && sed -i 's/board_lock\.py/hil_lock.py/g' .claude/skills/hil/SKILL.md .claude/agents/hil-operator.md .claude/agents/target-debugger.md .claude/skills/etm-trace/SKILL.md .claude/skills/usb-kernel-recover/SKILL.md .claude/skills/target-debug/SKILL.md`
Then: `grep -rn 'board_lock' .claude/ test/ --include='*.md' --include='*.py' --include='*.sh'`
Expected: zero hits outside `docs/superpowers/` history (which stays untouched).

- [ ] **Step 3: Verify CLI behavior end-to-end**

```bash
python3 test/hil/hil_lock.py status                                        # expect: no locks (or current holders)
python3 test/hil/hil_lock.py hold stm32f072disco --reason "split test" &
sleep 1
python3 test/hil/hil_lock.py status                                        # expect: stm32f072disco: {... 'reason': 'split test' ...}
python3 test/hil/hil_lock.py hold stm32f072disco --reason "rival" || echo "conflict OK"   # expect: ERROR ... locked + conflict OK
python3 test/hil/hil_lock.py release stm32f072disco                        # expect: released holder pid NNN
python3 test/hil/hil_lock.py status                                        # expect: no locks
```

Also verify CI-holder protection: create a fake record `echo '{"pid": 1, "reason": "hil_test.py"}' > /tmp/tinyusb-hil-locks/faketest.lock` — since pid 1 holds no flock, `release faketest` must clear the stale record without printing the mid-test error; then `rm -f /tmp/tinyusb-hil-locks/faketest.lock`.

- [ ] **Step 4: Commit**

```bash
git add -A test/hil .claude
git commit -m "hil: fold board_lock CLI into hil_lock.py, retire board_lock.py"
```

---

### Task 4: Rig verification + pre-commit

**Files:** none new (fixes only if verification fails)

- [ ] **Step 1: pool_check flash path on one board**

Run: `python3 .claude/skills/hil/pool_check.py -b stm32f407disco`
Expected: `✅ dfu_runtime  ✅ cafe:...`, exit 0.

- [ ] **Step 2: Capture a pre-refactor baseline report**

Run: `cd /home/hathach/code/tinyusb && python3 test/hil/hil_test.py -b stm32f407disco -B examples test/hil/tinyusb.json && cp hil_report.md /tmp/claude-1000/-home-hathach-code-tinyusb/*/scratchpad/hil_report_master.md`
(Primary checkout = pre-refactor code but same rig/config; its working tree already carries the new probe uids.)

- [ ] **Step 3: Run the same board from the worktree and diff the report shape**

Run: `cd .claude/worktrees/hil-test-split && python3 test/hil/hil_test.py -b stm32f407disco -B /home/hathach/code/tinyusb/examples test/hil/tinyusb.json && diff <(sed 's/[0-9.]*s//g;s/[0-9.]* [kMG]B\/s//g' hil_report.md) <(sed 's/[0-9.]*s//g;s/[0-9.]* [kMG]B\/s//g' /tmp/claude-1000/-home-hathach-code-tinyusb/*/scratchpad/hil_report_master.md)`
Expected: empty diff after stripping timings/speeds. Note: `-B` accepts the absolute path so the worktree run reuses the primary checkout's built firmware; `find_firmware` resolves `TINYUSB_ROOT/<build_dir>` and an absolute `-B` overrides relative rooting — if it does not (Path join semantics), instead symlink `ln -s /home/hathach/code/tinyusb/examples/cmake-build-stm32f407disco examples/cmake-build-stm32f407disco` in the worktree and use `-B examples`.

- [ ] **Step 4: pre-commit + final grep hygiene**

Run: `pre-commit run --files test/hil/hil_test.py test/hil/hil_lock.py test/hil/hil_flash.py test/hil/hil_ci.sh .claude/skills/hil/pool_check.py $(git diff --name-only HEAD~3 -- '*.md')`
Expected: all hooks pass.

- [ ] **Step 5: Commit any verification fixes**

```bash
git add -A && git commit -m "hil: post-split verification fixes"   # only if Steps 1-4 required changes
```
