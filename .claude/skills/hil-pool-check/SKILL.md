---
name: hil-pool-check
description: Use when asked for a pool check or board/probe health scan on a TinyUSB HIL rig, when probes or boards are offline or fail to flash, after rig maintenance, reboot, or re-cabling, or before starting a HIL test campaign. Running or interpreting HIL tests is the hil skill's.
---

# HIL Pool Check (board/probe health)

`test/hil/helper/hil_pool_check.py` checks each board of the rig's roster in turn: the probe
is on the bus, a light example flashes (the first built of a candidate list and the roster's
`only` list; host-only boards are judged by serial or RTT output), the board comes back, and
it is re-parked with `board_test`. A host-only board with no light firmware is judged by the
output of whatever it already runs, without a flash or a park. It never recovers a probe or a
board: a wedge is reported for `usb-kernel-recover`. Flags: `--help`.

The `hil` skill owns config selection by hostname and the board-lock protocol. The tool takes
each board's lock itself; a held board is reported 🔒 locked and skipped, never waited on or
bypassed. A CI job reaching a board the pool check holds fails it as "board locked", so prefer
running between CI runs.

## A "pool check" means the full check

Run the default full check. Use `--scan-only` (probe presence, no locks, flashing or builds)
only when the user asks for a quick look, or when `python3 test/hil/helper/hil_lock.py status`
shows `hil_test.py` holders right now; a suspicion that CI might be running is no reason,
since the full check is lock-safe. Say which mode ran and why.

```bash
python3 test/hil/helper/hil_pool_check.py                 # full check of the roster's boards
python3 test/hil/helper/hil_pool_check.py --scan-only
python3 test/hil/helper/hil_pool_check.py -b BOARD [-b …]  # subset; may name boards-skip entries
python3 test/hil/helper/hil_pool_check.py --json          # delegated callers: one JSON document on stdout

# from a dev PC; this runs the rig's own checkout, not yours
ssh ci.lan 'cd ~/code/tinyusb && python3 test/hil/helper/hil_pool_check.py'
```

Missing firmware is built through the build contract (`check_build.py --variants`, every
roster variant of the board) before the board is locked; `--no-build` opts out and those
boards report `flash-failed`, host-only boards excepted as above. A named `boards-skip` board
is never built: the build contract refuses it, so its firmware must already exist. ESP boards
need `idf.py` on PATH or `IDF_PATH` exported; without either the row notes `ESP-IDF env
missing`. A run that has to build takes minutes: run it in the background and never cancel it,
since a killed run can leave a build or a flasher running with no board lock behind it.

## Reading the result

Statuses: `ok`, `flash-failed` (firmware not delivered: probe missing, build failed, flasher
error, silent no-op, park unverified), `failed` (the check ran but did not verify), `locked`.
The exit code counts `flash-failed` + `failed`; `locked` rows and every `--scan-only` row are
unverified, not healthy, so read the footer or the JSON `coverage` (`probe-only`,
`skipped-locked`, `full-attempted`), never `$?` alone. A `⚠ pid … source says …` note is a
stale build or a silent flash no-op. A probe that enumerates but will not flash shows
`flash-failed` with the flasher's error; confirm it with the flasher's own probe list
(`STM32_Programmer_CLI -l st-link`, `ShowEmuList` in a `JLinkExe` script), then follow
`usb-kernel-recover` from its triage.

When a recovery is needed, let the pool check finish first: a root-port bounce re-enumerates
every board under that port. Release any hold before re-checking, since the pool check reports
a held board locked, and re-check every board under the bounced port.

## Reporting

The answer to a pool check is the tool's summary table, pasted whole with its footer, never a
digest like "27/27 healthy". Below it, only what the table cannot show: the mode when it was
not the full check, and for a recovery, which boards needed it, what was done (or that a
physical replug is needed) and both passes' results. The first pass is the signal that
predicts a recurrence.
