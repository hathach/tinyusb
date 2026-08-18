---
name: hil-pool-check
description: Use when asked for a pool check or board/probe health scan on a TinyUSB HIL rig, when probes or boards are offline or fail to flash, after rig maintenance, reboot, or re-cabling, or before starting a HIL test campaign.
---

# HIL Pool Check (board/probe health)

Health-scan the HIL board pool with `test/hil/helper/hil_pool_check.py`: per board it checks the flash
probe is on the USB bus, flashes a light example (`device/dfu_runtime`; host-only boards get
`host/device_info`, verified by serial output), waits for the board's uid to re-enumerate,
applies safe per-device recovery (probe authorized-toggle, board reset), re-parks with
`board_test`, and prints a summary table plus a USB topology report. Flags and details: `--help`
and the module docstring.

**REQUIRED BACKGROUND:** the `hil` skill owns config-by-hostname selection (run `hostname`
first) and the board-lock protocol. Locked boards are reported 🔒 locked and skipped — never
waited on, never bypassed; a needed build peeks the lock first. A CI worker reaching a board the
pool check holds fails it as "board locked" — prefer running between CI runs.

## A "pool check" means the full check

A request for a "pool check" means the DEFAULT full check below. Use `--scan-only` only when the
user explicitly asks for a quick look, or when you have VERIFIED a CI sweep is mid-run right now
(`python3 test/hil/helper/hil_lock.py status` shows `hil_test.py` holders) — "CI might be running" is not
that predicate: the full check is already lock-safe (CI-held boards report 🔒 locked and are never
touched), so an unconfirmed suspicion is no reason to downgrade. In either scan case say which
mode ran and why; never silently substitute the scan for the full check.

```bash
python3 test/hil/helper/hil_pool_check.py              # full check: ~10 s + ~1-2 s/board with firmware built;
                                                #   first run on an unbuilt tree takes minutes (it builds)
python3 test/hil/helper/hil_pool_check.py --scan-only  # USB presence only, <1 s, no locks/flashing/building
python3 test/hil/helper/hil_pool_check.py -b BOARD [-b …]  # subset; may name boards-skip (parked) entries

# from a dev PC, against the ci rig (bash -lc: flashers like STM32_Programmer_CLI live in ~/bin):
ssh ci.lan 'bash -lc "cd ~/code/tinyusb && python3 test/hil/helper/hil_pool_check.py"'
```

## Notes

Missing firmware is **built on the spot** — never skipped (`--no-build` opts out; those boards
then report `flash-failed`). Builds need the family env, exported on the rig in
`~/.profile`/`~/.bashrc`: `PICO_SDK_PATH` for rp2040/rp2350 (`~/code/pico/pico-sdk`), the
ESP-IDF env (`get-idf`) for espressif — which also needs `esptool` on PATH (pip's
`~/.local/bin/esptool`; a non-login shell may lack it — run via `bash -lc`). An explicit `-B` is
searched exclusively for *existing* firmware; builds still land in `cmake-build/` and are noted
`built <example>`. Espressif boards park too when the IDF env is present. A first run on an
unbuilt tree builds for many minutes: the Bash tool caps a foreground timeout at 10 min, so run
it in the BACKGROUND and NEVER cancel early — a killed run leaves detached cmake/ninja children
still writing to `cmake-build/` with the board locks held under a protected reason.

Statuses: `ok` (flashed and verified; in `--scan-only` it only means the probe is present),
`flash-failed` (firmware delivery failed: probe missing, build failed, flasher error, silent
no-op, park unverified), `failed` (check ran but did not verify), `locked` (flock held;
untouched). Exit code = `flash-failed` + `failed` (clamped at 125); `locked` and scan-only rows
are *unverified*, not healthy — read the footer, not just `$?`. A `⚠ pid … source says …` note
means stale firmware or a silent flash no-op (J-Link lore); a device off the bus entirely needs
the usb-kernel-recover skill or a physical replug.

## Reporting

The user-facing answer to a pool check IS the tool's summary table: paste the complete per-board
table (and footer counts) verbatim — never truncate rows or reduce it to a prose digest like
"27/27 healthy"; at most one line of commentary below it.
