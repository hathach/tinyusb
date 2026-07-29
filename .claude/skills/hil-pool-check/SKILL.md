---
name: hil-pool-check
description: Use when asked for a pool check or board/probe health scan on a TinyUSB HIL rig, when probes or boards are offline or fail to flash, after rig maintenance, reboot, or re-cabling, or before starting a HIL test campaign.
---

# HIL Pool Check (board/probe health)

Health-scan the HIL board pool with `test/hil/hil_pool_check.py`. Per board it verifies the flash
probe is on the USB bus, flashes a light example (`device/dfu_runtime` preferred; host-only boards
get `host/device_info`, verified by serial output instead of device enumeration), waits for the
board's uid to re-enumerate, applies safe per-device recovery (probe authorized-toggle, board
reset), re-parks with `board_test`, and prints a markdown summary table plus a USB topology report
(controller PCI address/vendor → bus → root-port subtree device counts — spots dead or thinned hub
legs at a glance).

**REQUIRED BACKGROUND:** the `hil` skill owns config-by-hostname selection (run `hostname` first;
`ci` → tinyusb.json, `tusb` → hfp.json, any other host is a dev PC → local.json) and the board-lock
protocol. Locked boards are reported 🔒 locked and skipped immediately — never waited on, never
bypassed (a needed build peeks the lock first, so no work is spent on a CI-held board). A CI worker
reaching a board the pool check holds fails it as "board locked" — prefer running between CI runs.

## A "pool check" means the full check

A request for a "pool check" means the DEFAULT full check below. Use `--scan-only` only when the
user explicitly asks for a quick look, or when you have VERIFIED a CI sweep is mid-run right now
(`python3 test/hil/hil_lock.py status` shows `hil_test.py` holders) — "CI might be running" is not
that predicate: the full check is already lock-safe (CI-held boards report 🔒 locked and are never
touched), so an unconfirmed suspicion is no reason to downgrade. In either scan case say which
mode ran and why; never silently substitute the scan for the full check.

```bash
python3 test/hil/hil_pool_check.py              # full check: ~10 s + ~1-2 s/board with firmware built;
                                                #   first run on an unbuilt tree takes minutes (it builds)
python3 test/hil/hil_pool_check.py --scan-only  # USB presence only, <1 s, no locks/flashing/building
python3 test/hil/hil_pool_check.py -b BOARD [-b …]  # subset; may name boards-skip (parked) entries

# from a dev PC, against the ci rig (bash -lc: flashers like STM32_Programmer_CLI live in ~/bin):
ssh ci.lan 'bash -lc "cd ~/code/tinyusb && python3 test/hil/hil_pool_check.py"'
```

## Notes

Firmware is searched in `examples/cmake-build-<board>` and `cmake-build/cmake-build-<board>` by
default (an explicit `-B` names ONE tree and is searched exclusively for *existing* firmware); a
board with nothing built gets its light example **built on the spot** (tools/build.py with a
one-shot `get_deps -b` + cache-drop retry; espressif via `idf.py`, which needs the ESP-IDF env
`get-idf`) — never skipped; builds always land in `cmake-build/` and are recorded with a
`built <example>` note, even under `-B`. `--no-build` opts out; a board with nothing built then
reports `flash-failed`. Family SDK env vars must be set for those builds (rig exports them in
`~/.profile`/`~/.bashrc`): `PICO_SDK_PATH` for rp2040/rp2350 boards (on the rig:
`~/code/pico/pico-sdk`). Espressif boards additionally need `esptool` on PATH — on the rig it is
pip's `~/.local/bin/esptool` (a non-login shell may lack that dir: run via `bash -lc` or prefix
`PATH="$HOME/.local/bin:$PATH"`). Boards are re-parked with `board_test` afterwards (`--no-park`
to skip; espressif parks too when the ESP-IDF env is available, otherwise the park is skipped with
a note). A `⚠ pid … source says …` note means stale firmware on disk or a silent flash no-op
(probe reset the MCU without writing — see J-Link silent-no-op lore). A missing probe is reported
with its last-seen bus location (cached in `~/.cache/tinyusb-hil/pool_seen.json`); recovering a
device that is off the bus entirely needs the usb-kernel-recover skill or a physical replug.

Row statuses: `ok` (flashed and verified; in `--scan-only` it only means the probe is present —
the scan proves nothing else), `flash-failed` (firmware delivery failed — probe missing, build
failed, flasher error, silent no-op, park not verified), `failed` (the check ran but did not
verify — flashed with no enumeration / no serial, or the check itself errored), `locked` (board
flock held; untouched). Exit code = `flash-failed` + `failed` count (clamped at 125) — `locked`
and scan-only rows are *unverified*, not healthy, so an all-locked or scan-only run can exit 0
while having proven little; read the footer, not just `$?`.

## Reporting

The user-facing answer to a pool check IS the tool's summary table: paste the complete per-board
table (and footer counts) verbatim — never truncate rows or reduce it to a prose digest like
"27/27 healthy"; at most one line of commentary below it.
