---
name: hil-contract
description: The TinyUSB HIL contract, under agentrc's `hil` rules. Use when running TinyUSB HIL tests on physical boards, when a HIL run fails, hangs, reports a board locked, or produces a report you need to interpret, or when copying firmware to a test rig (ci.lan, hifiphile/tusb, or a dev PC). For board/probe health scans ("pool check") use the hil-pool-check skill instead.
---

# HIL contract

TinyUSB's HIL mechanics; agentrc's `hil` skill holds the rules every project's HIL shares (locks, running, reporting, verdicts, retries) and is read first.

## Hosts/rosters

**Run `hostname` first** — it tells you which host you are on, which determines the default config and whether remote mode is possible. Rule of thumb: only `ci` and `tusb` are infra rigs; **any other hostname is a dev PC** and uses `local.json`.

| Host                              | Local config                         | Remote (SSH → ci.lan)?                                 |
|-----------------------------------|--------------------------------------|--------------------------------------------------------|
| `ci` (the rig)                    | `test/hil/tinyusb.json` (large pool) | no — boards are already local                          |
| `tusb` (hifiphile's external rig) | `test/hil/hfp.json`                  | no outbound SSH to dev PCs/ci; SSH-reachable FROM both |
| anything else (a dev PC)          | `test/hil/local.json`                | yes (large pool, `test/hil/tinyusb.json`)              |

Default to **local**. Use **remote** only when on a dev PC and the request or task scope names `remote`/`ci.lan`. Never attempt remote on `ci`.

`tusb` (ssh alias `hifiphile`) is an external rig (hosted by maintainer hifiphile), exercised by the
GitHub CI `hil-tinyusb (hfp.json)` matrix job — **never run HIL against it unless the user explicitly asks.**

A roster lists `boards` and parked `boards-skip`. Common entry fields: `name` (also the board's lock name), `uid`, `tests`, `flasher {name, uid, args, vid_pid, verify}` (only `name` and `uid` required), and optional `flasher_recover`, `variant`, `logger`, `toolchain`, `uart_uid`, `note`. A review may add or enable a `local.json` entry on a dev PC: name, board uid, flasher and tests, and a flashable example to run on it.

If `local.json` is missing on a dev PC, ask the user to supply one before a `hil_test.py` or `hil_pool_check.py` run. An agent that cannot ask (`hil-operator`) does not run `hil_test.py`: it returns one `ran: false` row per requested board whose `detail` names the missing `test/hil/local.json`. Fall back to `tinyusb.json` only when the request or task scope says so; a manual session locks by board name as Reservation says.

Board/probe health scanning (`test/hil/helper/hil_pool_check.py`) has its own skill, **hil-pool-check**: use it before a HIL campaign, after rig maintenance/reboot, or when boards fail to flash.

## Selection

`tools/change_impact.py` maps a diff to affected boards/tests (used by CI on PRs; fail-open
to the full matrix). Manual use:

```bash
SEL=$(python3 tools/change_impact.py --base master test/hil/tinyusb.json)
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

A caller with a changed-file list passes it as `--diff-file <file>` instead of `--base`. In a checkout without `tools/change_impact.py`, use `tools/ci_select.py`, chosen by that file's absence alone, never after a failed run.

## Build

Examples must be built for the target board(s) — see [Build and Validate](../../../CLAUDE.md#build-and-validate). Build them with the `build` skill's `--shared --variants <config>`, naming the HIL config the run uses: it writes the `cmake-build/cmake-build-<variant>/` folders `hil_test.py` flashes from by default, each with its variant's `flags` and `defines`, and refuses a folder still configured with options neither the variant nor the command line supplies. A local `hil_test.py --build` runs that same build for the selected boards first. A **remote** run stages the same folders; see Execution. (This applies to `hil_test.py`; `hil_pool_check.py` never builds: it flashes CI artifacts from its own cache, see **hil-pool-check**.)

A bare `--shared` takes nothing from the roster and builds only `cmake-build-<board>/`, so a self-named variant is built without its flags (`raspberry_pi_pico`'s `CFG_TUH_RPI_PIO_USB=1`) and any other variant stays unbuilt. An unbuilt variant's tests report `Skip (no binary)` without failing the run (`stm32f723disco-DMA` goes untested). On a run of one test per variant (a single-test `-bt`, or `-t` tests the board's `only` or capabilities leave at one) without `--skip-flash`, a later unbuilt variant instead fails `same-PID boundary ... not cleared (no board_test binary)`, naming `board_test`, not the missing build.

A delegated remote run is bound to its build: the build writes a build receipt of the HEAD it built,
and the run passes it, refusing before it touches the rig when HEAD, the roster or a staged file
no longer match it, or a tracked file (the harness it stages, the roster) has changed since HEAD
(rebuild, which writes a new one). A retry of a subset of those boards reuses
it; a local `hil_test.py` run takes none.

```bash
python3 .claude/skills/build/scripts/check_build.py --board raspberry_pi_pico2 --board stm32f723disco --shared --variants test/hil/tinyusb.json --receipt .hil-remote/build-1790000000.json
python3 .claude/skills/hil-contract/scripts/hil_remote.py --run-id hil-1790000000 --receipt .hil-remote/build-1790000000.json -b raspberry_pi_pico2 -b stm32f723disco
```

The build refuses a receipt for a tree that is not clean before or after it (commit a
`hw/bsp/family.json` it rewrote, then build again). `.hil-remote/` is ignored.

A CI firmware artifact is the one delegated run without a receipt: download it straight into a
fresh `-B` dir under `cmake-build/`, never a symlink to it (the staging rsync would copy the link).
Run it from a checkout whose harness and roster match the artifact's head, since those are staged
from the checkout. Report the artifact's name, run, head and staged sha256s beside the results.

```bash
mkdir -p cmake-build && mkdir cmake-build/ci-36166669952
gh run download 36166669952 -n 'binaries-arm-gcc--b raspberry_pi_pico_w' -D cmake-build/ci-36166669952
python3 .claude/skills/hil-contract/scripts/hil_remote.py --run-id hil-1790000000 -B cmake-build/ci-36166669952 -b raspberry_pi_pico_w
```

## Execution

### Arguments

- **Board:** `-b BOARD_NAME`, repeatable for a subset (`-b a -b b`); omit to run all boards in the config. Give a whole set to ONE run rather than one run per board: it spreads the boards across host controllers in dispatch order, while a second `hil_test.py` running alongside multiplies the load on the same xHCI cards. (A marginal DUT port bouncing under concurrent batteries has killed a uPD720201: fix the port or pull the board.)
- **Pass-through:** `-v`, `-r N`, etc. forwarded unchanged.

### Local

Set `CONFIG` per Hosts/rosters:

```bash
CONFIG=test/hil/local.json      # on ci use: CONFIG=test/hil/tinyusb.json

# All boards in the config:
python3 test/hil/hil_test.py "$CONFIG"

# A single board (replace stm32f723disco):
python3 test/hil/hil_test.py -b stm32f723disco "$CONFIG"
```

### Remote (dev PC → ci.lan only)

`scripts/hil_remote.py` takes `hil_test.py`'s own arguments, minus the config. It stages the harness, the config and the firmware the run will read under `-B` (default `cmake-build`, the `build` skill's `--shared` layout), runs `hil_test.py` on `ci.lan` with `tinyusb.json`, and copies the report pair and `<config>.failed` back to the checkout root:

```bash
# All boards built under cmake-build/:
python3 .claude/skills/hil-contract/scripts/hil_remote.py
# A subset — repeat -b, ONE invocation for the whole set:
python3 .claude/skills/hil-contract/scripts/hil_remote.py -b raspberry_pi_pico2 -b stm32f723disco -t host/cdc_msc_hid -r 1
```

An agent launches it with `--run-id` and waits on it as Timing says.

One invocation per board is wrong here, not merely slow: each run `rm -rf`s `REMOTE_DIR`
and rewrites the report, so only the last board's rows survive. A second run sharing
`REMOTE_DIR` is refused, before the wipe, while the first holds `<REMOTE_DIR>.lock`. The rig
needs `flock`; setup fails without it.

Before touching the rig it refuses a board not in the config, then applies `--flasher` and
`--exclude-flasher` (`no board left after the flasher filter` when none survives), then refuses a
requested board the filter kept with none of its `<-B>/cmake-build-<variant>` dirs, naming the dirs
it looked for (a variant's build flags are in the config); a board the filter drops needs no build.
Without `-b` it refuses with `nothing to test` when no board the filter kept is built, and stages
what is built without a word about unbuilt variants. With `-b` it also warns for each variant of a
requested board with no build; Build says what `hil_test.py` then reports for it. `--build` is
refused: the rig receives binaries only.

Exit 200 means the remote tree stopped being this run's after staging (another run sharing
`REMOTE_DIR` replaced it): `hil_test.py` did not run and nothing was copied back, so any local
`hil_report` pair or `<config>.failed` is from an earlier run. Re-run; never report from it.

Env overrides: `REMOTE`, `REMOTE_DIR`, `CONFIG`, `ROOT_DIR`.

### Timing

Runs take 2-5 min per board, but a stuck fleet runs to `HIL_POOL_TIMEOUT` — 60 min
unless the env pins it. The run logs its guard in the startup line; never declare a run
stuck before THAT value has elapsed.
The Bash tool caps a foreground timeout at 10 min, so **run it in the background** --
never a foreground timeout, which would kill the run before its own guard can write a
report.

A remote run is launched once, in the background, with a new `--run-id` (a used one is
refused), then waited on in the foreground; write the id literally in both calls, since a shell
variable does not survive from one tool call to the next:

```bash
# Bash run_in_background: true; a delegated run adds its --receipt (Build)
python3 .claude/skills/hil-contract/scripts/hil_remote.py --run-id hil-1790000000 --receipt .hil-remote/build-1790000000.json -b raspberry_pi_pico2 -b stm32f723disco
# foreground, Bash timeout 600000
python3 .claude/skills/hil-contract/scripts/hil_remote.py wait hil-1790000000
```

`wait` blocks up to 9.5 min and prints one JSON line:

- `done` (exit 0): the run's `exit` and the `reports` it copied back. Read only those; an
  empty list means no report is from this run, whatever the checkout holds.
- `running` (exit 3): call `wait` again.
- `dead` (exit 4): the run ended without its done record (killed, or its session died); no
  local report is from it.

A local `hil_test.py` run has no done record; wait for its completion notification.

What bounds a stuck run is `HIL_POOL_TIMEOUT` plus the job's `timeout-minutes`: a worker
the kernel will not let die (D state) holds the job until that ceiling. The `hil-pool-check`
skill finds which boards a wedge left unusable.

## Reservation

The `ci` rig also hosts a GitHub Actions runner that flashes boards and runs HIL as part of CI; never stop the runner service. Each board's lock is taken through `test/hil/helper/hil_lock.py`, under the board's roster `name`.

- `hil_test.py` self-locks each board for its flash+test (holder reason `hil_test.py`). A locked board fails immediately (`<board>  Failed: board locked: {holder info}`) without flashing — in CI, re-run the failed job later.
- For usbtest bring-up or case diagnosis, load the `usbtest` skill before a manual case run; its `run_case.py` takes and releases the lock itself. This contract still supplies the lock helper, the lock names and rig selection; agentrc's `hil` holds the lock rules.
- For hardware work outside `hil_test.py` (JLink/GDB, manual flashing, `usbtest.py`, serial poking), hold the lock first and release it when done — release is mandatory cleanup; the auto-release on holder death is a backstop, not the plan:

```bash
python3 test/hil/helper/hil_lock.py hold BOARD [BOARD...] --reason "why"
# ... hardware work ...
python3 test/hil/helper/hil_lock.py release BOARD [BOARD...]
```

- A hold refused by another holder: report holder and reason (`hil_lock.py status`). A holder reason of `hil_test.py` is a battery run, on `ci` usually a CI job mid-test: wait within the lock-wait budget and retry once, otherwise return the holder to the caller. A `hil_test.py` holder whose record has `role: cleanup-holder` (a TinyUF2 board held after a failed cleanup) is a recovery blocker, never waited on.

- A manual session on a dev-bench board with no entry in this host's HIL config locks it by an agreed board name, with no config: `hil_lock.py hold BOARD --reason "..."` only reserves that name, so verify the probe serial and board identity yourself. A named-board lock needs no config; config-driven tools (`hil_test.py`, `hil_pool_check.py` without an explicit config, `hil_lock.py hold --all`) still need this host's config.
- Rig-wide operations (uhubctl power cycling, `usb_recover.sh root-cycle`, pci-rebind, controller resets — bus renumbering) affect every board: `hil_lock.py hold --all --config <this host's config> --reason "..."` first — `--all` defaults to `tinyusb.json`, so on `tusb` it would reserve `tinyusb.json`'s boards, which are not there, instead of the three that are. Even a single root-port bounce needs `--all`: nothing maps a sysfs busport to a board name, and `hil_lock.py hold` accepts any string, so a "just the siblings" hold reserves nothing while reporting success. If `--all` cannot be taken, wait. `--all` covers this config's names only and ignores board names given beside it: on a host another project's rig also uses (htpc: TinyUF2's `test/hil/local.json`), also take a separate `hil_lock.py hold <names> --reason "..."` for that project's lock names (its contract's Reservation names them) not already in this config. Proceed only when both holds succeed; if either is refused, `hil_lock.py release` a name of each hold you took, never `release --all`, and stop.
- `hil_lock.py status` lists holders. Locks auto-release when the holder process dies (kernel flock); `/tmp` clears on reboot.
- Forcing past a lock: `HIL_NO_BOARD_LOCK=1 python3 test/hil/hil_test.py ...` bypasses the guard without killing the holder. It risks colliding with whatever holds it.

## Results

Interactively, paste the complete per-board table and its footer counts; commentary covers a
notice from the list below, a retry, a wedged board.

A delegated run (`hil-operator`) builds its output, from the directory the run wrote its
report to, with:

```bash
python3 test/hil/helper/hil_report.py <config> -b BOARD [-b BOARD...]
```

Copy it verbatim: rows are named per variant, a variant name need not start with the board
name, and lock contention is a cell rather than a phrase, so any of it re-derived by hand has come
out wrong before. It cannot carry all that Verdicts needs: a skipped cell counts toward
`cell(s) ok`, and a red usbtest cell is `❌ passed/total` whichever cases made it red. So the
reply adds two objects beside `results`, each keyed by every requested board (`[]` when it has
none): `skipped`, each skipped test as `<row> <test>` (a `⚪` cell in `hil_report.md`,
`Skip (no binary)` or `Skipped` in the run's output); and `usbtest`, each red usbtest row as
`<row>: ` plus its `device/usbtest` failure line in the run's output from after
`usbtest passed/total`: the `cases failed: [...]` numbers and the `N case(s) never ran` count.
That line is the only per-case source a run leaves: `cases failed` lumps FAIL, HUNG and NOTRUN,
and the never-ran (BUDGET) cases come as a count. Two red usbtest rows have no
`usbtest passed/total` and are copied whole from their message: `usbtest did not run: …` (the
battery left no result, an outer-timeout kill under load included) and
`no cafe:4010 device with serial …` (the device never enumerated). `results` has exactly one
entry per requested board. `pass` is the verdict of this report snapshot: every row can pass on
an aborted or no-boards run, so the run-level `caveat` (aborted, no boards; empty on every other
run) gates it. `--accumulate` clears an earlier attempt's caveat by design,
so the verdict of a retry sequence is the caller's, and a re-run's own caveat fails the sequence.
Each row's `wedged`
is the report's own verdict (a `board-wedged` cell: a confirmed hang, or one the battery could
not rule out) and is copied with the row; the
top-level `wedged` names requested boards, never a variant row name.
A run refused with `board(s) not in <config>` is re-run without the unknown names only while
a known name remains — an empty `-b` list runs every configured board — keeping the full
requested list on the `hil_report.py` call, which emits a `ran: false` row for each unknown
board. When no run started (a missing config, every name unknown, a refused hold with no
permitted retry, a scope gap, unbuilt firmware), it authors the rows instead: one per requested
board, `ran: false`, `pass: false`, `locked` as observed, the reason in `detail`, top-level
`pass: false`, `caveat` empty, each board's `skipped` and `usbtest` entry `[]` — and reads no stale report.

The operator retries a test failure once (below), only on a report with no `locked` or `wedged`
board and an empty `caveat`; any other outcome returns to the caller as it is. The whole board
set goes to ONE run (Execution); the caller re-runs locked boards with `--accumulate` once their
holder releases. A delegated run recovers a `wedged` board only through usbtest's in-run
recovery (a probe reset, or a reflash where the flasher has no reset); a board still wedged
after that is reported for a separately dispatched recovery action (Recovery).

**First check what sits above the table.** Three notices can appear there; match on a
PREFIX, since each carries trailing detail:

- `**HIL run aborted: worker pool timed out after …s.**` and
  `**HIL run aborted: a worker raised …**` — the pool guard fired, or a worker crashed. The
  notice counts what happened: "N board(s) below finished and are this run's; K never
  reported and are NOT in the table: <names>. The re-run spec covers those", plus, when
  `check_build.py` refused a board under `--build`, ", and the M whose build check_build.py
  refused: <names>." The N finished boards' rows are this run's: report them. The K named
  boards are not this run's whatever the table shows — on a fresh run they have no row, on
  an `--accumulate` retry a previous attempt's row survives under the notice and
  `hil_report.py` still folds it into `results` as ran — so name them as not run. The M
  refused boards DO have a `build refused` row, reported ran=false/pass=false (not run,
  failed), but sit outside N; the `<config>.failed` re-run spec covers both K and M. Never
  `"pass": true`.
- `**HIL run selected no boards.**` — the filters intersected to nothing, or, under
  `--build`, `check_build.py` refused every board. On the filter case, a fresh run shows no
  table; an `--accumulate` run keeps the previous attempt's rows under the notice, and they
  are not this run's. On the all-refused-build case, a fresh run AND an `--accumulate` run
  alike instead write every board's `build refused` row as this run's — report them
  ran=false/pass=false; `--accumulate` also keeps an earlier LOCKED/WEDGED cell for that
  board, and the `<config>.failed` re-run spec is written or refreshed to name the boards.
  Either way the exit code is nonzero. Report what the message shows (the filter, or the
  refused boards), never `"pass": true`.

The operator's retry (above) runs with `-v` and never on any other report: the `.failed` spec
lists locked and wedged boards too, and an accumulated retry would clear a caveat the caller
must keep. Retry from the `<config>.failed` spec the run just wrote, which
already begins with `--accumulate` and restricts each board to its failed tests. A hand-scoped
`-b <board>` retry MUST pass `--accumulate` too: a fresh run unlinks the report, replacing the
whole-fleet table with a one-row table. Every red row is a test failure for this retry except a
usbtest row whose every case ran (a `cases failed` list and no never-ran count): that result is
final, never a reason to retry, its first attempt stands if the spec re-runs it, and its FAIL,
NOTRUN or HUNG cases go to the `usbtest` skill for diagnosis. A usbtest row with a never-ran
count, `usbtest did not run` or `no cafe:4010 device` does get the retry: `hil_test.py` leaves
it retryable (`parsed` false, which its own `-r` follows), as a reflash can recover a battery
that never ran or stopped early.
If a board or fixture stops enumerating, or a tool of
yours hangs in D state, that is a wedge: save `dmesg | tail -50` as an artifact beside the
report (never into the report's rows) and name the board in `wedged`, without recovering it.
If a retry is still not enough, an interactive
session may add temporary debug prints to `hil_test.py`; a non-editing operator returns the
failure for diagnosis instead.

### Verdicts

The caller maps each requested board onto one `hil` verdict from the copied output and its
`skipped` and `usbtest` entries (Results), the first below that applies. A board whose exercised
tests all passed while a requested test was skipped or a usbtest case never ran is therefore
not-run, naming those tests and cases:

- blocker: a `board-wedged` cell (a row's `wedged`) or the board in the operator's top-level
  `wedged`; an aborted run (an `HIL run aborted` notice), the boards it never reported
  included; a launched run that left no report.
- fail: a test that ran and failed (a FAIL cell; any red usbtest row but the one below, its
  NOTRUN cases included, since nothing the operator can supply separates them from FAIL).
  `usbtest did not run` and `no cafe:4010 device` count as ran: the test started and left no
  passing result. A HUNG case the in-run recovery cleared is not a blocker by itself; the
  row's `wedged` decides. A usbtest row whose line gives a never-ran count and no `cases failed` list, and that is not
  wedged, is not-run for those cases, not a fail.
- not-run: `locked`, `build refused`, a board not in the config, or a run that never started
  for a known refusal (missing config, scope gap, unbuilt firmware).
- pass: every requested test ran and passed. A folded `ran: true, pass: true` row can still
  hold skipped tests (`Skip (no binary)`): the copied output stays as it is, but the board is
  not a pass while a requested test is skipped: it is not-run (above).

## Recovery

A board whose usbtest battery reported the device wedged (a HUNG case the in-run recovery could not clear, or a device it could no longer identify afterwards) gets a `board-wedged` cell and the rest of its examples are skipped for that run. No admission marker blocks the next run. Before a board's first flash, a run without `--skip-flash` looks for a D-state `testusb` on that board's usbfs node, which a cancelled or killed run leaves behind, and resets the board through its convoy-safe recovery flasher (`flasher_recover`, else `flasher`); a board it cannot free is reported `board-wedged` without being flashed. A stray on a board outside the run is not swept. Interactively, follow `usb-kernel-recover` from its triage before running it again; a delegated run follows Results. `usb-kernel-recover` shows what a real wedge looks like and how to clear it; `usb-kernel-debug` explains WHY the kernel rejected a device (dmesg analysis).

A dispatched recovery action is its own operation, never part of a run: the prompt names one
board, its busport, the rung ceiling, the budget and the reservation. Follow `usb-kernel-recover`
from its triage up to that ceiling and never above it, holding `--all` for every rig-wide rung.
Rung 1 resets through the board's recovery flasher (`flasher_recover`, else `flasher`); one that
is not `convoy_safe`, JLinkExe included, runs only behind `usb_recover.sh shield` on the board's
busport, unshielded afterwards;
sysrq and the hypervisor rungs need the user and are returned as the blocker in a headless run.
Report the cleanup explicitly: the board's state, plus any shield record or hold still standing.

## Limits

A board whose flasher probe has no VCOM (or whose BSP has no UART) uses RTT as its console — "No serial device found for /dev/serial/by-id/…" on every host test is the symptom. Config: `"logger": "rtt"` (jlink flashers only) plus a self-named variant carrying the define — `"variant": [{"name": "<board>", "defines": ["LOGGER=rtt"]}]` — and prebuilt example sets must carry the same `-DLOGGER=rtt`. Caveat: the cdc/msc-fixture host tests don't speak RTT yet, so such a board cannot carry `is_cdc`/`is_msc` fixtures (the config loader rejects it). Details: the `rtt` skill.

## Harness tests

Unit suites (no hardware) live in `test/hil/test/test_*.py`; the `hil-test` pre-commit hook
runs every `test_hil*.py`, `change-impact-test` `test_change_impact.py`, `test_ci_metrics.py` and
`test_hil_util.BottomLayer`. `test_change_impact.py` covers only selection, `test_ci_metrics.py`
only the code-size plumbing; the bounded reads and the build and pool guards live in
`test_hil_bounded.py` and `test_hil_util.py`; `test_hil_report.py` covers the report document, `test_hil_rtt.py`
the RTT console and `test_hil_pool_check.py` the pool check's verdicts and output. Run them all when changing `test/hil`:
`for f in test/hil/test/test_*.py; do python3 "$f"; done` (about a minute, half of it
`test_hil_bounded.py`'s deliberate hang/timeout simulation).
