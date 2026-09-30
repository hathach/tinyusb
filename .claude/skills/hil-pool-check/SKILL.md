---
name: hil-pool-check
description: Use when asked for a pool check or board/probe health scan on a TinyUSB HIL rig, when probes or boards are offline or fail to flash, after rig maintenance, reboot, or re-cabling, or before starting a HIL test campaign. Running or interpreting HIL tests is the hil skill's.
---

# HIL Pool Check (board/probe health)

`test/hil/helper/hil_pool_check.py` checks each board of the rig's roster in turn: the probe
is on the bus, a light example flashes (the first found of a candidate list and the roster's
`only` list; host-only boards are judged by UART output, and an RTT host-only board is
reported unsupported), the board comes back, and it is re-parked with `board_test` from the
same variant. It never builds firmware and never recovers a probe or a board: a wedge is
reported for `usb-kernel-recover`. Flags: `--help`.

The `hil` skill owns config selection by hostname and the board-lock protocol. The tool takes
each board's lock itself; a held board is reported 🔒 locked and skipped, never waited on or
bypassed. A CI job reaching a board the pool check holds fails it as "board locked", so prefer
running between CI runs.

## Firmware: the CI artifact cache

It checks basic USB function, not the current checkout, so it flashes older CI builds from a
per-host cache, `~/.cache/tinyusb-hil/firmware/cmake-build-<variant>/`, shared by the host's
worktrees. A board with no variant cached gets one downloaded once, before any board is locked.
The search covers up to 10 completed master push runs of `hathach/tinyusb`'s `build.yml`
(whatever the checkout's remote) within the 90-day artifact retention, newest first; within a
run, the board's variants are tried in roster order, and the first whose artifact holds a light
image and `board_test` is cached. PR runs are never used; master pushes build the whole HIL
matrix, so 10 runs have covered every variant so far. `.source` in each variant dir names its
run and commit. That needs `gh` logged in; the first fetch of ci.lan's roster downloads 28
variants, about 1.1 GB unpacked, in ~12 min. The cached variant may hold a less preferred light
example than another variant would. A board left without firmware is `flash-failed`, with the
fetch's reason.

A cached variant is never revalidated or replaced. To refresh one (roster change, damaged
files), delete its dir while no pool check is running; the next run fetches it again.
`-B DIR` flashes from another firmware root instead (e.g. `-B cmake-build` after a local
build contract run) and never fetches.

## A "pool check" means the full check

Run the default full check. Use `--scan-only` (probe presence, no locks, flashing or fetch)
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

Never cancel a run: a killed run can leave a download or a flasher running with no board lock
behind it.

## Reading the result

Statuses: `ok`, `flash-failed` (firmware not delivered: probe missing, not cached, flasher
error, silent no-op, park unverified), `failed` (the check ran but did not verify), `locked`.
The exit code counts `flash-failed` + `failed`; `locked` rows and every `--scan-only` row are
unverified, not healthy, so read the footer or the JSON `coverage` (`probe-only`, `skipped-
locked`, `full-attempted`), never `$?` alone. A probe that enumerates but will not flash shows
`flash-failed` with the flasher's error; confirm it with the flasher's own probe list
(`STM32_Programmer_CLI -l st-link`, `ShowEmuList` in a `JLinkExe` script), then follow `usb-
kernel-recover` from its triage.

When a recovery is needed, let the pool check finish first: a root-port bounce re-enumerates
every board under that port. Release any hold before re-checking, since the pool check reports
a held board locked, and re-check every board under the bounced port.

## Reporting

The answer to a pool check is the tool's summary table, pasted whole with its footer, never a
digest like "27/27 healthy". Below it, only what the table cannot show: the mode when it was
not the full check, and for a recovery, which boards needed it, what was done (or that a
physical replug is needed) and both passes' results. The first pass is the signal that
predicts a recurrence.
