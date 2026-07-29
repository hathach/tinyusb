---
name: hil
description: Use when running TinyUSB Hardware-in-the-Loop (HIL) tests on physical boards, checking pool health (which probes/boards are online), debugging HIL failures, or copying firmware to the ci.lan test rig. Covers per-host config selection (infra rigs ci/tusb use tinyusb.json/hfp.json, any dev PC uses local.json), local and remote execution, the test/hil/hil_pool_check.py health scan, and debugging tips.
---

# Hardware-in-the-Loop (HIL) Testing

Run TinyUSB HIL tests on real boards. **Run `hostname` first** — it tells you which host you are on, which determines the default config and whether remote mode is possible. Rule of thumb: only `ci` and `tusb` are infra rigs; **any other hostname is a dev PC** and uses `local.json`.

| Host                              | Local config                         | Remote (SSH → ci.lan)?                                 |
|-----------------------------------|--------------------------------------|--------------------------------------------------------|
| `ci` (the rig)                    | `test/hil/tinyusb.json` (large pool) | no — boards are already local                          |
| `tusb` (hifiphile's external rig) | `test/hil/hfp.json`                  | no outbound SSH to dev PCs/ci; SSH-reachable FROM both |
| anything else (a dev PC)          | `test/hil/local.json`                | yes (large pool, `test/hil/tinyusb.json`)              |

Default to **local**. Use **remote** only when on a dev PC and the user says `remote`/`ci.lan`. Never attempt remote on `ci`.

`tusb` (ssh alias `hifiphile`) is an external rig (hosted by maintainer hifiphile), exercised by the
GitHub CI `hil-tinyusb (hfp.json)` matrix job — **never run HIL against it unless the user explicitly asks.**

## Board locks — the CI runner keeps running

The `ci` rig also hosts a GitHub Actions runner that flashes boards and runs HIL as part of CI. Hardware access is arbitrated **per board** with kernel flocks in `/tmp/tinyusb-hil-locks/` — do NOT stop the runner service.

- `hil_test.py` self-locks each board for its flash+test (holder reason `hil_test.py`). A locked board fails immediately (`<board>  Failed: board locked: {holder info}`) without flashing — in CI, re-run the failed job later; if your `hold` is refused with reason `hil_test.py`, a CI job is mid-test — wait a few minutes and retry rather than forcing.
- For hardware work outside `hil_test.py` (JLink/GDB, manual flashing, `usbtest.py`, serial poking), hold the lock first:

```bash
python3 test/hil/hil_lock.py hold BOARD [BOARD...] --reason "why"
# ... hardware work ...
python3 test/hil/hil_lock.py release BOARD [BOARD...]
```

- Never pre-hold boards you are about to run `hil_test.py` on — it self-locks and would treat your own hold as a conflict.
- Rig-wide operations (uhubctl power cycling, pci-rebind — bus renumbering) affect every board: `hil_lock.py hold --all --reason "..."` first.
- `hil_lock.py status` lists holders. Locks auto-release when the holder process dies (kernel flock); `/tmp` clears on reboot.
- Forcing past a lock: `HIL_NO_BOARD_LOCK=1 python3 test/hil/hil_test.py ...` bypasses the guard without killing the holder. Only with the user's explicit go-ahead — they accept the risk of colliding with whatever holds the board.

## Pool check (quick health scan)

Before a HIL campaign, after rig maintenance/reboot, or when boards fail to flash, run
`test/hil/hil_pool_check.py`. It picks the config by hostname (table above), and per
board: verifies the flash probe is on the USB bus, flashes a light example (`device/dfu_runtime`
preferred; host-only boards get `host/device_info` and are checked for serial output instead of
device enumeration), waits for the board's uid to (re-)enumerate, applies safe per-device recovery
(probe authorized-toggle after repeated flash failure, board reset when the uid stays down), then
prints a markdown summary table plus a USB topology report (controller PCI address/vendor → bus →
root-port subtree device counts — spots dead or thinned hub legs at a glance). Each board is
flock'd (hil_lock.py protocol): locked boards are reported 🔒 locked and skipped immediately —
never waited on, never bypassed (a needed build peeks the lock first, so no work is spent on a
CI-held board). The locks prevent per-board *collisions* with a concurrent CI run, but note the
converse: a CI worker reaching a board the pool check holds fails it as "board locked" (a red cell
in that CI run), and the flashes take no cross-process per-controller budget (default `-j 4`
bounds concurrent flashes AND on-the-spot builds; a first run on an unbuilt tree is build-heavy —
minutes, all cores) — prefer running between CI runs.

A request for a "pool check" means the DEFAULT full check below. Use `--scan-only` only when the
user explicitly asks for a quick look, or when a CI sweep is mid-run — and in either case say
which mode ran and why; never silently substitute the scan for the full check.

```bash
python3 test/hil/hil_pool_check.py              # full check: ~10 s + ~1-2 s/board with firmware built;
                                                #   first run on an unbuilt tree takes minutes (it builds)
python3 test/hil/hil_pool_check.py --scan-only  # USB presence only, <1 s, no locks/flashing/building
python3 test/hil/hil_pool_check.py -b BOARD [-b …]  # subset; may name boards-skip (parked) entries

# from a dev PC, against the ci rig (bash -lc: flashers like STM32_Programmer_CLI live in ~/bin):
ssh ci.lan 'bash -lc "cd ~/code/tinyusb && python3 test/hil/hil_pool_check.py"'
```

Notes: firmware is searched in `examples/cmake-build-<board>` and
`cmake-build/cmake-build-<board>` by default (an explicit `-B` names ONE tree and is searched
exclusively for *existing* firmware); a board with nothing built gets its light example **built on
the spot** (tools/build.py with a one-shot `get_deps -b` + cache-drop retry; espressif via
`idf.py`, which needs the ESP-IDF env `get-idf`) — never skipped; builds always land in
`cmake-build/` and are recorded with a `built <example>` note, even under `-B`. `--no-build` opts
out; a board with nothing built then reports `flash-failed`. Family SDK env vars must be set for
those builds (rig exports them in `~/.profile`/`~/.bashrc`): `PICO_SDK_PATH` for rp2040/rp2350
boards (on the rig: `~/code/pico/pico-sdk`). Espressif boards additionally need
`esptool` on PATH — on the rig it is pip's `~/.local/bin/esptool` (a non-login shell may lack that
dir: run via `bash -lc` or prefix `PATH="$HOME/.local/bin:$PATH"`). Boards are re-parked with
`board_test` afterwards (`--no-park` to skip; espressif parks too when the ESP-IDF env is
available, otherwise the park is skipped with a note). A `⚠ pid … source says …` note means stale
firmware on disk or a silent flash no-op (probe reset the MCU without writing — see J-Link
silent-no-op lore). A missing probe is reported with its last-seen bus location (cached in
`~/.cache/tinyusb-hil/pool_seen.json`); recovering a device that is off the bus entirely needs the
usb-kernel-recover skill or a physical replug. Row statuses: `ok` (flashed and verified; in
`--scan-only` it only means the probe is present — the scan proves nothing else), `flash-failed`
(firmware delivery failed — probe missing, build failed, flasher error, silent no-op, park not
verified), `failed` (the check ran but did not verify — flashed with no enumeration / no serial,
or the check itself errored), `locked` (board flock held; untouched). Exit code = `flash-failed` +
`failed` count (clamped at 125) — `locked` and scan-only rows are *unverified*, not healthy, so an
all-locked or scan-only run can exit 0 while having proven little; read the footer, not just `$?`.

## Prerequisites

Examples must be built for the target board(s) — see CLAUDE.md "Build" → "All examples for a board" (produces `examples/cmake-build-<board>/`). `-B examples` points `hil_test.py` at that parent folder. (This applies to `hil_test.py`; `hil_pool_check.py` builds its own missing firmware.)

## Arguments

- **Board:** `-b BOARD_NAME` for one board; omit to run all boards in the config.
- **Pass-through:** `-v`, `-r N`, etc. forwarded unchanged.

If `local.json` is missing on a dev PC, ask the user to supply one (only fall back to `tinyusb.json` if told to).

## Local execution

Set `CONFIG` from `hostname` first (`test/hil/local.json` on a dev PC, `test/hil/tinyusb.json` on ci, `test/hil/hfp.json` on tusb):

```bash
CONFIG=test/hil/local.json      # on ci use: CONFIG=test/hil/tinyusb.json

# All boards in the config:
python3 test/hil/hil_test.py -B examples "$CONFIG"

# A single board (replace stm32f723disco):
python3 test/hil/hil_test.py -b stm32f723disco -B examples "$CONFIG"
```

## Remote execution (dev PC → ci.lan only)

`test/hil/hil_ci.sh` handles dir setup, scp of test scripts, rsync of firmware (`.elf`/`.bin`/`.hex`), and runs `hil_test.py` on `ci.lan` with `tinyusb.json`:

```bash
# All boards:
bash test/hil/hil_ci.sh

# A single board, with pass-through flags:
bash test/hil/hil_ci.sh -b raspberry_pi_pico2 -t host/cdc_msc_hid -r 1
```

Env overrides: `REMOTE`, `REMOTE_DIR`, `CONFIG`. Fails fast if the build dir/repo layout is missing.

## Timing

Runs take 2-5 min. Use a timeout ≥ 20 min (1200000 ms). NEVER cancel early.

## Reporting

The user-facing answer to a pool check or HIL run IS the tool's summary table: paste the
complete per-board table (and footer counts) verbatim — never truncate rows or reduce it to a
prose digest like "27/27 healthy"; at most one line of commentary below it. On failure, retry
with `-v`; if that's not enough, add temporary debug prints to `hil_test.py`.
