---
name: hil
description: Use when running TinyUSB Hardware-in-the-Loop (HIL) tests on physical boards, when a HIL run fails, hangs, reports a board locked, or produces a report you need to interpret, or when copying firmware to a test rig (ci.lan, hifiphile/tusb, or a dev PC). For board/probe health scans ("pool check") use the hil-pool-check skill instead.
---

# Hardware-in-the-Loop (HIL) Testing

Run TinyUSB HIL tests on real boards. **Run `hostname` first** — it tells you which host you are on, which determines the default config and whether remote mode is possible. Rule of thumb: only `ci` and `tusb` are infra rigs; **any other hostname is a dev PC** and uses `local.json`.

| Host                              | Local config                         | Remote (SSH → ci.lan)?                                 |
|-----------------------------------|--------------------------------------|--------------------------------------------------------|
| `ci` (the rig)                    | `test/hil/tinyusb.json` (large pool) | no — boards are already local                          |
| `tusb` (hifiphile's external rig) | `test/hil/hfp.json`                  | no outbound SSH to dev PCs/ci; SSH-reachable FROM both |
| anything else (a dev PC)          | `test/hil/local.json`                | yes (large pool, `test/hil/tinyusb.json`)              |

Default to **local**. Use **remote** only when on a dev PC and the request or task scope names `remote`/`ci.lan`. Never attempt remote on `ci`.

`tusb` (ssh alias `hifiphile`) is an external rig (hosted by maintainer hifiphile), exercised by the
GitHub CI `hil-tinyusb (hfp.json)` matrix job — **never run HIL against it unless the user explicitly asks.**

## Board locks — the CI runner keeps running

The `ci` rig also hosts a GitHub Actions runner that flashes boards and runs HIL as part of CI. Hardware access is arbitrated **per board** with kernel flocks in `/tmp/tinyusb-hil-locks/` — do NOT stop the runner service.

- `hil_test.py` self-locks each board for its flash+test (holder reason `hil_test.py`). A locked board fails immediately (`<board>  Failed: board locked: {holder info}`) without flashing — in CI, re-run the failed job later; if your `hold` is refused with reason `hil_test.py`, a CI job is mid-test — wait a few minutes and retry rather than forcing.
- For hardware work outside `hil_test.py` (JLink/GDB, manual flashing, `usbtest.py`, serial poking), hold the lock first and release it when done — release is mandatory cleanup; the auto-release on holder death is a backstop, not the plan:

```bash
python3 test/hil/helper/hil_lock.py hold BOARD [BOARD...] --reason "why"
# ... hardware work ...
python3 test/hil/helper/hil_lock.py release BOARD [BOARD...]
```

- A hold refused by another holder: report holder and reason (`hil_lock.py status`), never kill the holder. A holder reason of `hil_test.py` is a CI job mid-test — wait a few minutes and retry once when the task allows, otherwise return the holder to the caller.

- A manual session on a dev-bench board with no entry in this host's HIL config locks it by an agreed board name, with no config: `hil_lock.py hold BOARD --reason "..."` only reserves that name, so verify the probe serial and board identity yourself. A named-board lock needs no config; config-driven tools (`hil_test.py`, `hil_pool_check.py` without an explicit config, `hil_lock.py hold --all`) still need this host's config.
- Never pre-hold boards you are about to run `hil_test.py` on — it self-locks and would treat your own hold as a conflict.
- Rig-wide operations (uhubctl power cycling, `usb_recover.sh root-cycle`, pci-rebind, controller resets — bus renumbering) affect every board: `hil_lock.py hold --all --config <this host's config> --reason "..."` first — `--all` defaults to `tinyusb.json`, so on `tusb` it would reserve 27 boards that do not exist there and none of the three that do. Even a single root-port bounce needs `--all`: nothing maps a sysfs busport to a board name, and `hil_lock.py hold` accepts any string, so a "just the siblings" hold reserves nothing while reporting success. If `--all` cannot be taken, wait: a partial hold is worse than none, because it reads as protection.
- `hil_lock.py status` lists holders. Locks auto-release when the holder process dies (kernel flock); `/tmp` clears on reboot.
- A board that finished a run with a confirmed wedge (usbtest still saw a D-state holder on its node after the confirmation window) is marked `<board>.wedged` beside its flock, and `hil_test.py` refuses it in seconds with a `board-wedged` cell until the marker is cleared. `hil_lock.py wedged status` lists markers; after recovery has been verified, `hil_lock.py wedged clear BOARD --evidence '{"board": "BOARD", "uid": "<marker uid>", "holders": [], "complete": true, "identity": "<serial>@<busport>"}'` clears one, refusing while the board is held or when the evidence does not verify that marker. Once its worker pool is down (on an abort too), `hil_test.py` tries to recover every board of the run that carries a marker, from this run's worker or an earlier run's, when the marker's uid is the roster's: with every board in the config reserved in-process (refused if any is held, then the markers stay), one board at a time it resets through the recovery flasher, shielding the DUT's leaf, hub and root hub (`usb_recover.sh shield`) around it only when that flasher is not convoy-safe, reflashes the artifact under test if a holder survives, and clears the marker only on a complete holder scan with no holder plus the DUT enumerated again; the row then shows `⚪ recovered post-run` (or `⚪ refused at admission; recovered post-run`) in `board-wedged`, the test verdict and `ran` unchanged. A board that needs the shield recovers only as a non-root user with passwordless sudo (root ignores the shield), otherwise it keeps its marker and the others proceed; the phase never runs under `--skip-flash` and runs within `HIL_RECOVERY_TIMEOUT` (600 s) for the whole phase plus one step's overrun. The phase is a forked supervisor in its own session: it holds the reservation, reports its verdicts, then keeps every board reserved until each step process it started has ended, so neither a killed `hil_test.py` nor a step that will not die (a privileged child in D state) leaves the fleet unprotected; such a survivor is named in the log with the supervisor's pid. The marker and its `<board>.wedge-dmesg.txt` share the lock dir's lifetime. It contains that board's reuse only: it does not shield other enumerators from the poisoned node, a worker killed before writing leaves none, and a reboot clears the marker and the kernel's stuck processes but not necessarily the DUT, probe or controller, so post-reboot health still needs `hil-pool-check`.
- Forcing past a lock: `HIL_NO_BOARD_LOCK=1 python3 test/hil/hil_test.py ...` bypasses the guard without killing the holder. Only when the request or task scope explicitly names forcing that board — it risks colliding with whatever holds it; a refused hold alone never adds that scope.

## Pool check (board/probe health)

Board/probe health scanning (`test/hil/helper/hil_pool_check.py`) has its own skill: **hil-pool-check**.
Use it before a HIL campaign, after rig maintenance/reboot, or when boards fail to flash.

## PR-scoped selection

`tools/ci_select.py` maps a diff to affected boards/tests (used by CI on PRs; fail-open
to the full matrix). Manual use:

```bash
SEL=$(python3 tools/ci_select.py --base master test/hil/tinyusb.json)
FULL=$(printf '%s' "$SEL" | python3 -c "import json,sys; print(json.load(sys.stdin)['full'])")
ARGS=$(printf '%s' "$SEL" | python3 -c "import json,sys; print(json.load(sys.stdin)['args']['tinyusb.json'])")
if [ "$FULL" = "True" ] || [ -n "$ARGS" ]; then
  python3 test/hil/hil_test.py $ARGS test/hil/tinyusb.json   # $ARGS empty when full: run everything
else
  echo "diff affects nothing on this rig - skip HIL"
fi
```

Read `full`, never `args` alone: `args` is empty for BOTH `full: true` (run the whole matrix — a broad or
unclassified change) and "nothing selected" (skip). Skip only when `full` is false AND `args` is empty.

Unit suites (no hardware) live in `test/hil/test/test_*.py`; the `hil-test` pre-commit hook
runs every `test_hil*.py`, `ci-select-test` the two `test_ci_*` suites plus
`test_hil_util.BottomLayer`. `test_ci_select.py` covers only selection, `test_ci_metrics.py`
only the code-size plumbing; the containment work --- bounded reads, the kill ladders, the
build and pool guards --- lives in `test_hil_bounded.py`, `test_hil_health.py` and
`test_hil_util.py`; `test_hil_report.py` covers the report document and `test_hil_rtt.py`
the RTT console. Run them all when changing `test/hil`:
`for f in test/hil/test/test_*.py; do python3 "$f"; done` (about a minute, half of it
`test_hil_bounded.py`'s deliberate hang/timeout simulation).

## Pre-flight rig health check

`hil_test.py` notes any process already in D state when the run starts, as one line above
the table. It never aborts, and it is a hint rather than a diagnosis. What bounds a stuck
run is `HIL_POOL_TIMEOUT` plus the job's `timeout-minutes`; what diagnoses a wedged rig is
the `hil-pool-check` skill.

See the `usb-kernel-recover` skill for what a real wedge looks like and how to clear it, and the `usb-kernel-debug` skill to explain WHY the kernel rejected a device (dmesg analysis).

## Prerequisites

Examples must be built for the target board(s) — see [Build and Validate](../../../CLAUDE.md#build-and-validate). For a **local** run the `build` skill's `--shared` produces `cmake-build/cmake-build-<board>/`, the folder `hil_test.py` flashes from by default. A **remote** run stages the same folder; see Remote execution below. (This applies to `hil_test.py`; `hil_pool_check.py` builds its own missing firmware.)

A board whose flasher probe has no VCOM (or whose BSP has no UART) uses RTT as its console — "No serial device found for /dev/serial/by-id/…" on every host test is the symptom. Config: `"logger": "rtt"` (jlink flashers only) plus a self-named variant carrying the define — `"variant": [{"name": "<board>", "defines": ["LOGGER=rtt"]}]` — and prebuilt example sets must carry the same `-DLOGGER=rtt`. Caveat: the cdc/msc-fixture host tests don't speak RTT yet, so such a board cannot carry `is_cdc`/`is_msc` fixtures (the config loader rejects it). Details: the `rtt` skill.

## Arguments

- **Board:** `-b BOARD_NAME`, repeatable for a subset (`-b a -b b`); omit to run all boards in the config. Give a whole set to ONE run rather than one run per board: it schedules the boards across host controllers and budgets concurrent flashes and usbtest batteries per controller (`hil_lock.py` `FLASH_PARALLEL`/`USBTEST_PARALLEL`). Those permits are in-process semaphores — a second `hil_test.py` running alongside does not share them, it multiplies the load on the same xHCI cards. (A dead uPD720201 card is not a width problem: every observed death traced to a marginal DUT port bouncing under concurrent batteries, and lowering the widths does not fix a bad port — fix the port or pull the board; the concurrency note above `FLASH_PARALLEL` in `hil_lock.py` keeps the record.)
- **Pass-through:** `-v`, `-r N`, etc. forwarded unchanged.

If `local.json` is missing on a dev PC, ask the user to supply one before a `hil_test.py` or `hil_pool_check.py` run. An agent that cannot ask (`hil-operator`) does not run `hil_test.py`: it returns one `ran: false` row per requested board whose `detail` names the missing `test/hil/local.json`. Fall back to `tinyusb.json` only when the request or task scope says so; a manual session locks by board name as Board locks says.

## Local execution

Set `CONFIG` from `hostname` first (`test/hil/local.json` on a dev PC, `test/hil/tinyusb.json` on ci, `test/hil/hfp.json` on tusb):

```bash
CONFIG=test/hil/local.json      # on ci use: CONFIG=test/hil/tinyusb.json

# All boards in the config:
python3 test/hil/hil_test.py "$CONFIG"

# A single board (replace stm32f723disco):
python3 test/hil/hil_test.py -b stm32f723disco "$CONFIG"
```

## Remote execution (dev PC → ci.lan only)

`scripts/hil_remote.py` takes `hil_test.py`'s own arguments, minus the config. It stages the harness, the config and the firmware the run will read under `-B` (default `cmake-build`, the `build` skill's `--shared` layout), runs `hil_test.py` on `ci.lan` with `tinyusb.json`, and copies the report pair and `<config>.failed` back to the checkout root:

```bash
R=.claude/skills/hil/scripts/hil_remote.py
# All boards built under cmake-build/:
python3 $R

# A subset — repeat -b, ONE invocation for the whole set:
python3 $R -b raspberry_pi_pico2 -b stm32f723disco -t host/cdc_msc_hid -r 1
```

One invocation per board is wrong here, not merely slow: each run `rm -rf`s `REMOTE_DIR`
and rewrites the report, so only the last board's rows survive. A second run sharing
`REMOTE_DIR` is refused, before the wipe, while the first holds `<REMOTE_DIR>.lock`. The rig
needs `flock`; setup fails without it.

Before touching the rig it refuses a board not in the config, then applies `--flasher` and
`--exclude-flasher` (`no board left after the flasher filter` when none survives), then refuses a
requested board the filter kept with none of its `<-B>/cmake-build-<variant>` dirs, naming the dirs
it looked for (a variant's build flags are in the config); a board the filter drops needs no build.
Without `-b` it refuses with `nothing to test` when no board the filter kept is built. It warns for
each variant with no build, whose cells would be skipped rather than tested. `--build` is refused:
the rig receives binaries only.

Exit 200 means the remote tree stopped being this run's after staging (another run sharing
`REMOTE_DIR` replaced it): `hil_test.py` did not run and nothing was copied back, so any local
`hil_report` pair or `<config>.failed` is from an earlier run. Re-run; never report from it.

Env overrides: `REMOTE`, `REMOTE_DIR`, `CONFIG`, `ROOT_DIR`.

## Timing

Runs take 2-5 min per board, but a stuck fleet runs to `HIL_POOL_TIMEOUT` — 60 min
unless the env pins it. The run logs its guard in the startup line; never declare a run
stuck before THAT value has elapsed.
The Bash tool caps a foreground timeout at 10 min, so **run it in the background** and
wait for the completion notification -- never a foreground timeout, which would kill
the run before its own guard can write a report. NEVER cancel early.

## Reporting

Two audiences, two shapes. Interactively, the answer to a HIL run IS the tool's summary table:
paste the complete per-board table (and footer counts) verbatim — never truncate rows or reduce
it to a prose digest. Commentary below it covers only what the table cannot show: a banner
verdict from the list below, a retry, a wedged board.

A delegated run (the `hil-operator` role) returns the machine output instead: exactly
`{ pass, results, banner, caveat, wedged }` and nothing else. From the directory the run wrote its
report to:

```bash
python3 test/hil/helper/hil_report.py <config> -b BOARD [-b BOARD...]
```

`pass`, `results`, `banner` and `caveat` are copied from its output verbatim — never retyped,
reworded or re-ordered: rows are named per variant, a variant name need not start with the board
name, and lock contention is a cell rather than a phrase, so any of it re-derived by hand has come
out wrong before. `results` has exactly one entry per requested board. `pass` is the verdict of
this report snapshot: every row can pass on an abandoned or no-boards run, so the run-level
`caveat` (abandoned, aborted, no boards; empty on every other run) gates it. `--accumulate`
clears an earlier attempt's caveat by design, so the verdict of a retry sequence is the caller's:
keep every attempt's result, a clean subset re-run never erases an earlier run-level failure, and
a re-run's own caveat fails the sequence. Each row's `wedged`
is the report's verified verdict (a `board-wedged` cell) and is copied with the row; the
top-level `wedged` — the boards the run left unresponsive, usually none — is the operator's
own observation and the only field it authors when a run happened; it names requested boards,
never a variant row name.
A run refused with `board(s) not in <config>` is re-run without the unknown names only while
a known name remains — an empty `-b` list runs every configured board — keeping the full
requested list on the `hil_report.py` call, which emits a `ran: false` row for each unknown
board. When no run started (a missing config, every name unknown, a refused hold with no
permitted retry, a scope gap, unbuilt firmware), it authors the rows instead: one per requested
board, `ran: false`, `pass: false`, `locked` as observed, the reason in `detail`, top-level
`pass: false`, `banner` and `caveat` empty — and reads no stale report. The caller treats a
missing or malformed reply as inconclusive, never as a pass or a fail.

The caller decides what follows a run. The whole board set goes to ONE run (above); the failure
retry below runs only on a report with no `locked` or `wedged` board and an empty `caveat`, so
any other outcome returns to the caller as it is. On `locked` the caller bypasses with `HIL_NO_BOARD_LOCK=1` only
when the task scope names bypassing those boards' locks, never releasing or killing the holder;
or waits and re-runs the locked boards with `--accumulate`; or accepts, reporting the boards not
covered. A `wedged` board is never re-run. A delegated run recovers it only through its own paths (the
in-run confirmation, the post-pool phase under Board locks); a marker still standing after them is
reported, and any further recovery is a separately dispatched recovery action (below).

A dispatched recovery action is its own operation, never part of a run: the prompt names one
board, its busport, the rung ceiling, the budget and the reservation. Follow `usb-kernel-recover`
from its triage up to that ceiling and never above it, holding `--all` for every rig-wide rung.
Rung 1 resets through the board's recovery flasher (`flasher_recover`, else `flasher`); one that
is not `convoy_safe`, JLinkExe included, runs only behind `usb_recover.sh shield` on the board's
busport, unshielded afterwards;
sysrq and the hypervisor rungs need the user and are returned as the blocker in a headless run.
Clear the marker only with `hil_lock.py wedged clear` and the evidence it verifies; a marker it
refuses as untrusted or invalid-name is reported for a human to inspect and remove by hand. Report
the cleanup explicitly: marker cleared, or kept with the reason, plus any shield record or hold
still standing. A HIL run on that board follows only a cleared marker.

**First check what sits above the table.** Six banners can appear there; match on a
PREFIX, since each carries trailing detail and two are blockquotes:

- `**HIL run abandoned: worker pool timed out after …s.**` and
  `**HIL run aborted: a worker raised …**` — the pool guard fired, or a worker crashed. The
  banner counts what happened: "N board(s) below finished and are this run's; K never
  reported and are NOT in the table: <names>". The N finished boards' rows are this run's:
  report them. The K named boards are not this run's whatever the table shows — on a fresh
  run they have no row, on an `--accumulate` retry a previous attempt's row survives under
  the banner and `hil_report.py` still folds it into `results` as ran — so name them as not
  run; the `<config>.failed` re-run spec covers them. Never `"pass": true`.
- `**HIL run abandoned: the worker pool would not shut down.**` — DIFFERENT: the table
  below IS this run's, but the pool could not be shut down afterwards (the job exits
  non-zero even if every board passed). Report the results AND the abandonment; never
  `"pass": true`.
- `**HIL run selected no boards.**` — the filters intersected to nothing. A fresh run shows
  no table; an `--accumulate` run keeps the previous attempt's rows under the notice, and
  they are not this run's. Report the empty selection (and the filter shown), never
  `"pass": true`.
- `> **Rig note.**` — a process was in D state when the run started. This is NOT a wedge:
  a healthy in-flight testusb is uninterruptible for most of every case, and the rig
  supports a dev run alongside CI. On its own it is never `wedged: true` and never turns a
  green table into `"pass": false`. Mention it only when a board below failed, as the first
  thing to check.
- `> **Rig dirty.**` — a process survived SIGKILL and still holds a probe or usbfs node
  into the NEXT job. The table below is this run's and can be reported, but say the rig is
  dirty: the next job starts degraded and nothing in the harness can clear it.

On a test failure, retry once with `-v` — only when the report has no `locked` or `wedged` board
and an empty `caveat`; otherwise return the snapshot without retrying, since the `.failed` spec
lists those boards too and an accumulated retry would clear a caveat the caller must keep. Retry
from the `<config>.failed` spec the run just wrote, which
already begins with `--accumulate` and restricts each board to its failed tests. A hand-scoped
`-b <board>` retry MUST pass `--accumulate` too: a fresh run unlinks the report, replacing the
whole-fleet table with a one-row table. A usbtest battery that produced per-case verdicts is not
auto-retried; its result already stands. If a board or fixture stops enumerating, or a tool of
yours hangs in D state, that is a wedge: save `dmesg | tail -50` as an artifact beside the
report (never into the report's rows) and name the board in `wedged`, without recovering it —
a `> **Rig note.**` banner about someone else's D-state process is not that. If a retry is still not enough, an interactive
session may add temporary debug prints to `hil_test.py`; a non-editing operator returns the
failure for diagnosis instead.
