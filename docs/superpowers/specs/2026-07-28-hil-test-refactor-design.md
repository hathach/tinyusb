# hil_test.py refactor: test core + infra helpers

**Date:** 2026-07-28
**Branch:** `claude/hil-test-split` (based on `claude/hil-pool-check`, which adds `pool_check.py`)

## Motivation

`test/hil/hil_test.py` is 2370 lines mixing five concerns: board-lock protocol, per-controller
scheduling permits, flash/reset backends, the actual per-example tests, and orchestration/report/CLI.
The lock protocol additionally exists in three copies (`hil_test.py`, `board_lock.py`,
`.claude/skills/hil/pool_check.py`), which has already produced drift (pool_check's copy lacks
hil_test's fail-open and error guards). Splitting the infrastructure out makes `hil_test.py`
test-focused and gives external tools (pool_check) one canonical import for locks, permits, and
flashing.

## Goal / non-goals

**Goal:** behavior-preserving code motion. `hil_test.py`'s CLI, arguments, output, report format,
and runtime behavior stay byte-identical. One deliberate user-visible change: the operator lock CLI
moves from `board_lock.py` to `hil_lock.py` (same subcommands, same behavior); `board_lock.py` is
deleted.

**Non-goals (explicit follow-ups, not this change):**
- The 15 pool_check findings from the 2026-07-28 code review (exception isolation, park-on-failure,
  espressif coverage, probe-recovery criterion, etc.).
- pool_check adopting `flash_permit` controller budgeting (enabled by this split).
- Any change to lock semantics, permit widths, flash behavior, or test logic.

## Resulting layout (`test/hil/`)

| File | ~Lines | Role |
|---|---|---|
| `hil_test.py` | 1600 | tests + orchestration + report + CLI (unchanged interface) |
| `hil_lock.py` (new) | 420 | board-lock protocol + controller permits + operator CLI |
| `hil_flash.py` (new) | 250 | `run_cmd` + flash/reset backends + `find_firmware` |
| `board_lock.py` | deleted | superseded by `hil_lock.py` |

Import graph: `hil_test` → {`hil_lock`, `hil_flash`}; the helpers import nothing local (no cycles).
`pool_check.py` imports all three.

## hil_lock.py

Docstring states the scope: board locks + controller flash/battery permits; the CLI manages board
locks only (permits are in-process semaphores with no CLI meaning).

**Flock core** (protocol defined once; moved from `board_lock.py`/`hil_test.py`):
- `BOARD_LOCK_DIR = '/tmp/tinyusb-hil-locks'`, `lock_path(board)`
- `CI_REASON = 'hil_test.py'` — the release-protected holder tag (release refuses to kill it)
- `flock_nb(board) -> fh` — `os.open(O_RDWR|O_CREAT, 0o666)` **without O_TRUNC** (a losing racer
  must not wipe the winner's record), `fdopen('r+')`, `LOCK_EX|LOCK_NB`; raises `OSError` when held
- `write_record(fh, reason)` — truncate+seek+`json.dump({pid, reason, since})`+flush
- `clear_record(fh)` — truncate(0), swallow OSError (records stay truthful on release)
- `read_record(board) -> dict | None` — today's `board_lock.read_info`
- `acquire_board_lock(board, reason=CI_REASON) -> fh | None` — today's `hil_test.acquire_board_lock`
  with a `reason` parameter: `HIL_NO_BOARD_LOCK=1` bypass, fail-open with warning on lock-dir
  OSError, `RuntimeError` carrying holder info on conflict

**Controller permits** (moved verbatim from `hil_test.py`):
- `FLASH_PARALLEL`, `USBTEST_PARALLEL`, `CONTROLLER_SLOTS` (env-overridable as today)
- `controller_of(uid)`, `controller_slot(pci)`, `controller_permit`, `flash_permit(uid)`,
  `usbtest_permit(uid)`
- Per-worker globals (`usbtest_sems`, `flash_sems`, `controller_map`, `controller_meta`,
  `controller_hints`) set by a new `init_scheduling(sems, fsems, cmap, cmeta, hints)` hook that
  `hil_test.init_worker` calls from the Pool initializer. `controller_permit`'s PROFILE logging
  calls back through a module-level `log = print`-style hook that `hil_test` points at `log_line`
  during `init_scheduling` (keeps helpers free of hil_test imports). The `PROFILE` env flag
  (`HIL_PROFILE=1`) is read independently in `hil_lock` at import, same derivation as today.

**Operator CLI** (moved verbatim from `board_lock.py`): `hold`/`release`/`status` subcommands with
the daemon-holder machinery (double-fork, setsid, stdio detach, success pipe, SIGTERM bow-out),
release policy (probe the flock; protect `CI_REASON` holders; SIGTERM other recorded pids),
`is_locked` pid-liveness, `--all`/`--config` roster handling. The hold/release/status internals
switch to the flock-core helpers above; observable behavior unchanged.

## hil_flash.py

Moved verbatim from `hil_test.py`:
- `CMD_TIMEOUT` (env-overridable), `run_cmd(cmd, cwd, timeout)`, `cmd_stdout_text(out)`
- `OPENCOD_ADI_PATH`, `TINYUSB_ROOT`
- All backends: `flash_jlink`/`reset_jlink`, `flash_stlink`/`reset_stlink`,
  `flash_stflash`/`reset_stflash`, `flash_openocd`/`reset_openocd`,
  `flash_openocd_wch`/`reset_openocd_wch`, `flash_openocd_adi`/`reset_openocd_adi`,
  `flash_wlink_rs`/`reset_wlink_rs`, `flash_esptool`/`reset_esptool`,
  `flash_uniflash`/`reset_uniflash`, `flash_lm4flash`/`reset_lm4flash`
- `find_firmware(variant, example)`
- `get_serial_dev(id, vendor_str, product_str, ifnum)` — moves here (not hil_test) because
  `flash_esptool` calls it; keeping it test-side would create a helper→hil_test import cycle.
  Tests call `hil_flash.get_serial_dev`.
- Module globals `build_dir = 'cmake-build'` and `verbose = False`, set by callers exactly as the
  `hil_test` globals are today (`hil_test.main` sets them from argparse; pool_check sets them
  directly). `run_cmd`'s verbose echo reads `hil_flash.verbose`.

Dispatch in callers stays string-based: `getattr(hil_flash, f'flash_{flasher["name"].lower()}')`.

## hil_test.py (what remains)

Config TypedDicts (`Board`, `FlasherCfg`, …), device-node lookup except `get_serial_dev`
(`get_disk_dev`, `get_hid_dev`, `get_alsa_capture_dev`, `open_serial_dev`, `serial_write_all`,
`read_disk_file`, `open_mtp_dev`, `get_printer_dev`/`open_printer_dev`), enum-timeout globals +
`wait_until`,
`log_line`/print-lock, `compact_output`, all `test_*` functions, test lists, `test_example`,
`build_board`, `test_board`, report rendering/accumulation, `main`. Call sites use explicit
module-qualified names (`hil_lock.flash_permit(...)`, `hil_flash.run_cmd(...)`) so provenance is
greppable; no `from … import *`-style mirroring.

`init_worker` keeps its signature (Pool initargs unchanged) and forwards the scheduling state to
`hil_lock.init_scheduling(...)`.

## Consumer updates (same commit)

- **`.claude/skills/hil/pool_check.py`** — drop its private `lock_board`/`unlock_board` in favor of
  `hil_lock.flock_nb` + `write_record(fh, 'pool_check')` (+ `clear_record` on release; deliberately NOT `acquire_board_lock`, whose HIL_NO_BOARD_LOCK bypass and fail-open behavior pool_check must not inherit); import
  flashers/`find_firmware`/`get_serial_dev`/`cmd_stdout_text`/`TINYUSB_ROOT`/`build_dir` from
  `hil_flash`; `BOARD_LOCK_DIR` references move to `hil_lock`. pool_check then imports **only**
  `hil_lock` + `hil_flash` (no `hil_test`), so its `pymtp` stub shim is deleted — that shim existed
  solely because importing `hil_test` pulls in libmtp.
- **`test/hil/hil_ci.sh`** — the scp list is currently `hil_test.py`, `pymtp.py`, `$CONFIG`; add
  `hil_lock.py` and `hil_flash.py` (hil_test cannot even import without them). `board_lock.py` was
  never in the list.
- **Docs rename `board_lock.py` → `hil_lock.py`** (live docs only): `.claude/skills/hil/SKILL.md`,
  `.claude/agents/hil-operator.md`, `.claude/agents/target-debugger.md`,
  `.claude/skills/etm-trace/SKILL.md`, `.claude/skills/usb-kernel-recover/SKILL.md`,
  `.claude/skills/target-debug/SKILL.md`. Historical `docs/superpowers/{plans,specs}` stay as
  records.
- **CI workflow** — untouched (invokes `hil_test.py` CLI only).

## Verification

1. `python3 -m py_compile` on all three modules + pool_check.
2. `hil_lock.py hold/status/release` interplay: hold, conflicting hold, status listing, release,
   protection of a `CI_REASON` record, stale-record cleanup.
3. `pool_check.py --scan-only`, then a single flash board (e.g. `-b stm32f407disco`).
4. Full `hil_test.py -b stm32f407disco -B examples tinyusb.json` on the rig; compare the report
   row and log shape against a pre-refactor run.
5. `pre-commit run` on all touched files.

## Sequencing

Lands on top of `claude/hil-pool-check`. After merge, fix the pool_check review findings as a
separate change on the new module boundaries, and update agent-memory references to
`board_lock.py`.
