---
name: hil-contract
description: The TinyUSB HIL contract, under agentrc's `hil` rules. Use when running TinyUSB HIL tests on physical boards, when a HIL run fails, hangs, reports a board locked, or produces a report you need to interpret, or when copying firmware to a test rig (ci.lan, hifiphile/tusb, or a dev PC). For a farm check (board/probe health scan, "pool check") use the `farm-check` skill, which runs the launcher this contract's Farm check names.
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

htpc, a dev PC, also carries TinyUF2's rig, `test/hil/local.json` in that checkout (its contract's Hosts/rosters): a rig-wide operation there holds that roster's lock names too (Reservation).

`tusb` (ssh alias `hifiphile`) is an external rig (hosted by maintainer hifiphile), exercised by the
GitHub CI `hil-tinyusb (hfp.json)` matrix job — **never run HIL against it unless the user explicitly asks.**

A roster lists `boards` and parked `boards-skip`. Common entry fields: `name` (also the board's lock name), `uid`, `tests`, `flasher {name, uid, args, vid_pid, verify}` (only `name` and `uid` required), and optional `flasher_recover`, `variant`, `logger`, `toolchain`, `uart_uid`, `note`; `test/hil/hil_test.py`'s `Board` type is the full shape. A review may add or enable a `local.json` entry on a dev PC: name, board uid, flasher and tests, and a flashable example to run on it.

If `local.json` is missing on a dev PC, ask the user to supply one before a `hil_test.py` run or a farm check. An agent that cannot ask (`hil-operator`) does not run `hil_test.py`: it returns the no-run rows Results defines, `detail` naming the missing `test/hil/local.json`. Fall back to `tinyusb.json` only when the request or task scope says so; a manual session locks by board name as Reservation says.

## Farm check

The `farm-check` skill's launcher here is `test/hil/helper/hil_farm_check.py` (`[CONFIG] [-b BOARD]... [-B DIR] [--scan-only] [--json] [--report-dir DIR] [-v]`): agentrc's engine from `lib/agentrc` at the pin, with this repo's profile `test/hil/helper/farm_profile.py`. Run it before a HIL campaign, after rig maintenance/reboot, or when boards fail to flash; from a dev PC, on the rig's own checkout: `ssh ci.lan 'cd ~/code/tinyusb && python3 test/hil/helper/hil_farm_check.py'`. Without CONFIG it takes `ci`'s or `tusb`'s roster above and refuses on any other host, so a dev PC names `test/hil/local.json`; `-b` may name `boards-skip` entries. `--no-park` is refused.

- Probe: the board's flasher `uid` among the bus serials, never a `cafe` device (an ESP USJ shares its DUT's serial); a J-Link's zero-padded serial matches when no exact one does.
- Light image: `dfu_runtime` for a device board, judged by its `cafe` device re-enumerating (the VID:PID it had before the flash leaves the new image unverified). A host-only board takes the first built of `host/device_info`, `host/cdc_msc_hid`, `host/msc_file_explorer_freertos` (its `only` list's host examples when it has one), judged by output on the flasher's UART after a reset; output that is only `board_test`'s is a silent flash no-op (`flash-failed`). A host-only board logging over RTT is unsupported (`failed`, untouched); `openocd_esp` boards are refused.
- Final state: `board_test` from the light image's variant, verified by the `cafe` device leaving the bus (only when it was on the bus before the park) or, on a host-only board, by `board_test`'s hello on the UART. Anything less leaves the board blocked for Recovery.
- Firmware is never built. By default it comes from the per-host CI artifact cache `~/.cache/tinyusb-hil/firmware/cmake-build-<variant>/`, shared by the host's worktrees. A board with no variant cached gets one downloaded before any board is locked (needs `gh` logged in): from up to 10 completed master push runs of `hathach/tinyusb`'s `build.yml` with firmware artifacts, within the 90-day retention, newest first; never a PR run; the first of the board's variants, in roster order, whose artifact holds a light image and `board_test` (and an ESP image's `config.env` and `flash_args`). `.source` in each variant dir names its run, commit, artifact and conclusion, the row's image provenance. A cached tree is never revalidated or replaced: delete its dir while no farm check runs to refetch it. `-B DIR` flashes from another firmware root (`-B cmake-build` after a build contract run), never fetches, and reports its provenance missing.
- CI built a silent `board_test` while it defined `CI_BUILD`: a cached variant whose `.source` commit still has that branch (`git grep -q CI_BUILD <sha> -- examples/device/board_test` exits 0) cannot prove a host-only board's park (`park unverified: no board_test output`, the board blocked). Delete those variant dirs once a master run provides updated artifacts, or use `-B` with a fresh local build.

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

An empty selection means the diff touched nothing `change_impact.py` maps to a board or test;
what it cannot map (an example absent from the HIL lists, a Type-C example) contributes nothing
and is named with its reason on stderr: report that as uncovered, never as covered. Read `full`,
never `args` alone: `args` is empty for BOTH `full: true` (run the whole matrix — a broad or
unclassified change) and "nothing selected" (skip). Skip only when `full` is false AND `args` is empty.

A caller with a changed-file list passes it as `--diff-file <file>` instead of `--base`. `-t` lists tests for every board, filtered through each board's capability flags or its
`only` list; `-bt <board>:<tests>` names one board's tests outright and overrides that filter for
it (`_tests_for` in `hil_test.py`). In a checkout without `tools/change_impact.py`, use `tools/ci_select.py`, chosen by that file's absence alone, never after a failed run.

## Build

Examples must be built for the target board(s) — see [Build and Validate](../../../CLAUDE.md#build-and-validate). Build them with the `build` skill's `--shared --variants <config>`, naming the HIL config the run uses: it writes the `cmake-build/cmake-build-<variant>/` folders `hil_test.py` flashes from by default, each with its variant's `flags` and `defines`, and refuses a folder still configured with options neither the variant nor the command line supplies. A local `hil_test.py --build` runs that same build for the selected boards first. A **remote** run stages the same folders; see Execution. (This applies to `hil_test.py`; the farm check never builds: see Farm check.)

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

`scripts/hil_remote.py` takes `hil_test.py`'s own arguments, minus the config. It stages the harness, the config and the firmware the run will read under `-B` (default `cmake-build`, the `build` skill's `--shared` layout), runs `hil_test.py` on `ci.lan` with `tinyusb.json`, and copies the report pair, the rig report's attempt records (`hil_report.attempts/`) and `<config>.failed` back to the checkout root in one fetch. agentrc's `hil_report_core.adopt` makes the rig's pair the checkout's report, the local one archived first, or withdraws the local one (an `HIL report unavailable` notice) when the rig's is not usable as this run's; its docstring lists when. Whenever the report did not come home readable, the run exits nonzero even if `hil_test.py` exited 0, and the rig's report and records are kept under `.hil-remote/collect-*/`. The transport is agentrc's `hil_remote_core`, loaded from `lib/agentrc` at the pin:

```bash
# All boards built under cmake-build/:
python3 .claude/skills/hil-contract/scripts/hil_remote.py
# A subset — repeat -b, ONE invocation for the whole set:
python3 .claude/skills/hil-contract/scripts/hil_remote.py -b raspberry_pi_pico2 -b stm32f723disco -t host/cdc_msc_hid -r 1
```

An agent launches it with `--run-id` and waits on it as Timing says.

It refuses, before touching the rig, when no `rsync` is on PATH, or when the local `hil_report.json` is of an unknown schema or version (it would be replaced unarchived). On a Windows dev PC that must be
an MSYS2 or Cygwin `rsync.exe` in an absolute PATH dir, and it must find its own distribution's
`ssh` first on PATH, since with Windows OpenSSH (`System32\OpenSSH`) the argument conversion and
the pipes rsync relies on break. There it also refuses a config or firmware dir on another drive
than the checkout, which rsync cannot name, and snapshots receipt-pinned firmware under
`.hil-remote/`. The Windows path is not yet proved end to end on a rig.

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
unless the env pins it — and in CI a worker the kernel will not let die (D state) holds the job
until its `timeout-minutes`. The run logs its guard in the startup line; never declare a run
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

## Reservation

The `ci` rig also hosts a GitHub Actions runner that flashes boards and runs HIL as part of CI, never stopped (`hil` Board locks). Each board's lock is taken through `test/hil/helper/hil_lock.py`, under the board's roster `name`. The harness's shared scripts (the lock mechanics, the bounded sysfs reads and bus scan of `test/hil/helper/usb_observe.py`, the RTT console, and the `hil` skill's Flashers: the bounded command runner behind `test/hil/helper/hil_util.py` and the per-flasher commands and pyOCD launcher behind `test/hil/hil_flash.py`) are loaded by `test/hil/helper/agentrc.py` from `lib/agentrc` at the commit `tools/get_deps.py` pins, fetched by `python3 tools/get_deps.py lib/agentrc`; missing, off that commit or lacking a file the flash provider loads or launches, every HIL entry point refuses before any board access.

The provider's `openocd_esp` flasher is supported by `hil_test.py` only (the farm check and `run_case.py` refuse it) and is unrostered on ci.lan; it is refused under a CI job (`RUNNER_TRACKING_ID` or `GITHUB_ACTIONS` set), whose end kills the job's processes, until the openocd server and the cleanup holder are shown to survive it. A session it leaves running keeps the board's lock with a cleanup holder, reported in a `cleanup-pending` cell naming the receipt and its `release --token`.

- `hil_test.py` self-locks each board for its flash+test (holder reason `hil_test.py`). A locked board fails immediately (`<board>  Failed: board locked: {holder info}`) without flashing — in CI, re-run the failed job later.
- For usbtest bring-up or case diagnosis, load the `usbtest` skill before a manual case run; its `run_case.py` takes and releases the lock itself. This contract still supplies the lock helper, the lock names and rig selection.
- For hardware work outside `hil_test.py` (JLink/GDB, manual flashing, `usbtest.py`, serial poking), hold the lock first and release it when done — release is mandatory cleanup:

```bash
python3 test/hil/helper/hil_lock.py hold BOARD [BOARD...] --reason "why"
# ... hardware work ...
python3 test/hil/helper/hil_lock.py release BOARD [BOARD...]
```

- A hold refused by another holder: `hil_lock.py status` gives the holder and reason to report. A holder reason of `hil_test.py` is a battery run, on `ci` usually a CI job mid-test; waiting and the budget follow `hil` Board locks; a `role: cleanup-holder` holder is reported as a recovery blocker, released only through the owning project's recovery procedure, and `hil_lock.py release` refuses to kill it.

- A manual session on a dev-bench board with no entry in this host's HIL config locks it by an agreed board name, with no config: `hil_lock.py hold BOARD --reason "..."` only reserves that name, so verify the probe serial and board identity yourself. A named-board lock needs no config; config-driven tools (`hil_test.py`, `hil_lock.py hold --all`) still need this host's config, and a farm check off `ci` and `tusb` refuses without one.
- Rig-wide operations affect every board, so they hold every board on the host first (`hil` Board locks). A root-port cycle (`usb_recover.sh root-cycle`, rung 2 of `usb-kernel-recover`) runs only through agentrc's `hil_reserve.py`, with every roster on this host: on `ci`, `python3 lib/agentrc/skills/hil/scripts/hil_reserve.py run root-cycle <busport> --roster test/hil/tinyusb.json --roster <tinyuf2 checkout>/test/hil/ci.json tinyuf2.lock --reason "..."`; on htpc, `test/hil/local.json` and TinyUF2's `test/hil/local.json` with the same selector. It refuses when a device under that root port is not explained by those rosters, holds every board of every roster in one process around the cycle, and keeps the locks as a cleanup holder (printing a token) unless the cycle ended clean with every device back; the recovery dispatch that establishes restoration releases them with `hil_reserve.py release --token <token>`. Other rig-wide operations (a leaf hub's `uhubctl` power cycle) take `hil_lock.py hold --all --config <this host's config> --reason "..."` (`--all` defaults to `tinyusb.json`, so on `tusb` it would reserve the wrong boards) plus a separate `hil_lock.py hold <names> --reason "..."` for the other project's lock names on a shared host (its contract's Reservation names them), and proceed only when every hold succeeded; on a refusal, `hil_lock.py release` a name of each hold you took, never `release --all`, then wait within the budget or return the holder (`hil` Board locks). A controller operation (`pci-rebind`, `pci-bind`) has no reservation procedure that covers it yet and is refused under this contract until one exists.
- `hil_lock.py status` lists holders. Locks auto-release when the holder process dies (kernel flock); `/tmp` clears on reboot.
- Forcing past a lock: `HIL_NO_BOARD_LOCK=1 python3 test/hil/hil_test.py ...` bypasses the guard without killing the holder. It risks colliding with whatever holds it.

## Results

Interactively, the summary table is shown with its footer counts; commentary covers a notice from the
list below, a retry, a wedged board.

A delegated run (`hil-operator`) builds its output, from the directory the run wrote its
report to, with:

```bash
python3 test/hil/helper/hil_report.py <config> -b BOARD [-b BOARD...]
```

Copy it verbatim (`hil` Reporting): rows are named per variant, a variant name need not start with the board
name, and lock contention is a cell rather than a phrase, so any of it re-derived by hand has come
out wrong before. What it cannot show is in Limits; where Verdicts needs it, it is read from
the run's output (`Skip (no binary)` and `Skipped` lines; each red usbtest row's last
`device/usbtest` failure line and, in full failure output, usbtest's per-case JSON), never from
the reply, which stays the JSON the operator returns: the launcher keeps the operator's transcript,
and, until #4226, the operator copies the first attempt's report directory aside (`mktemp -d`) before a retry,
saying where in that transcript. `testedHead` for a
review row is the build receipt's head (Build), or HEAD of the tree a local run built.
`hil_test.py` exits with its failure count (capped at 125); `hil_report.py` exits 0 whenever it
wrote its JSON, a failing one included: read `pass`, never its exit. It exits 1 without JSON when
the report is missing or of an unknown schema or version: the sidecar carries agentrc's
`hil_report_core` metadata (`_hil`: attempt id, predecessor, how the run ended) beside its own
fields, and every attempt a later one replaced is kept under `hil_report.attempts/`. `results` has exactly one
entry per requested board. `pass` is the verdict of
this report snapshot: every row can pass on an aborted or no-boards run, so the run-level
`caveat` (aborted, no boards; empty on every other run) gates it. `--accumulate`
clears an earlier attempt's caveat by design, so the verdict of a retry sequence is the caller's,
and a re-run's own caveat fails the sequence. `python3 test/hil/helper/hil_report.py sequence <config> [-b BOARD]...
[--report-dir DIR]` prints that sequence verdict from every linked attempt (agentrc's `sequence_verdict`:
`{current, sequence}`; every board an attempt selected decides whether the next is an eligible retry, and a board passes only with a passing test cell), with the same exits; a run that could not publish its report leaves a withdrawal
record (`HIL report unavailable` notice) that blocks the sequence. Each row's `wedged`
is the report's own verdict (a `board-wedged` cell: a confirmed hang, or one the battery could
not rule out) and is copied with the row; the
top-level `wedged` names requested boards, never a variant row name.
A run refused with `board(s) not in <config>` is re-run without the unknown names only while
a known name remains — an empty `-b` list runs every configured board — keeping the full
requested list on the `hil_report.py` call, which emits a `ran: false` row for each unknown
board. When no run started (a missing config, every name unknown, a refused hold with no
permitted retry, a scope gap, no build for the head), it authors the rows instead: one per requested
board, `ran: false`, `pass: false`, `locked` as observed, the reason in `detail`, top-level
`pass: false`, `caveat` empty — and reads no stale report.

The operator retries a test failure once, under `hil`'s Caller rules (how: below); any other
outcome returns to the caller as it is. The whole board set goes to ONE run (Execution); a
locked board is admitted again, once its holder releases and within the budget, by a hand-scoped
`--accumulate -b <board>` run (below) carrying the original `-t`/`-bt` selection, which is not the
retry. A board still wedged after usbtest's
in-run recovery (Recovery) is reported for a separately dispatched recovery action.

**First check what sits above the table.** These notices can appear there; match on a
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
- `**HIL run aborted: config error: …**`, `**HIL run aborted: usbtest id registration failed: …**`
  and `**HIL run aborted: No boards left: check_build.py refused every build (…)**` — boards
  were selected but none was tested: a roster error, a failed usbtest registration, or, under
  `--build`, `check_build.py` refusing every board. A fresh run shows no table on the first
  two; an `--accumulate` run keeps the previous attempt's rows under the notice, and they are
  not this run's. On the all-refused-build case, a fresh run AND an `--accumulate` run alike
  write every board's `build refused` row as this run's — report them ran=false/pass=false;
  `--accumulate` also keeps an earlier LOCKED/WEDGED cell for that board, and the
  `<config>.failed` re-run spec is written or refreshed to name the boards. The exit code is
  nonzero; never `"pass": true`.
- `**HIL run selected no boards.**` — the filters intersected to nothing. A fresh run shows
  no table; an `--accumulate` run keeps the previous attempt's rows under the notice, and
  they are not this run's. The exit code is nonzero. Report the filter the message shows,
  never `"pass": true`.

The operator's retry (above) runs with `-v`; `hil`'s gate on it matters here because the `.failed` spec
lists locked and wedged boards too, and an accumulated retry would clear a caveat the caller
must keep. Retry from the `<config>.failed` spec the run just wrote, which
already begins with `--accumulate` and restricts each board to its failed tests. A hand-scoped
`-b <board>` retry MUST pass `--accumulate` too: a fresh run unlinks the report, replacing the
whole-fleet table with a one-row table. A usbtest battery that produced per-case verdicts is not
auto-retried; its result already stands, and its FAIL, NOTRUN or HUNG cases go to the `usbtest`
skill for diagnosis. If a board or fixture stops enumerating, or a tool of
yours hangs in D state, that is a wedge: save `dmesg | tail -50` as an artifact beside the
report (never into the report's rows) and name the board in `wedged`, without recovering it.
If a retry is still not enough, an interactive
session may add temporary debug prints to `hil_test.py`; a non-editing operator returns the
failure for diagnosis instead.

### Verdicts

The caller maps each requested board onto the `hil` verdicts from the copied output:

- blocker: a `board-wedged` cell (a row's `wedged`) or the board in the operator's top-level
  `wedged`; a `cleanup-pending` cell, which the report folds as a FAIL and never as `wedged` (an
  `openocd_esp` session left running, recovered under Recovery, never retried); an `HIL run aborted`
  notice, the boards it never reported included; a launched run that left no report.
- fail: a FAIL cell, or a usbtest FAIL case. A HUNG case the in-run recovery cleared is not a
  blocker by itself; the row's `wedged` decides.
- not-run: `locked`, `build refused`, a `lock-unavailable` cell (the host's lock dir is unusable: rig-side, not
  re-admitted until it is repaired), a board not in the config, a run that never started (the
  cases Results lists); a FAIL cell whose row's output shows the flash failed before any test
  ran, and a usbtest row red only from NOTRUN or BUDGET cases, though the report folds both as
  failures (Limits); a skipped test (`Skip (no binary)`), which its row lists in `skipped` and folds
  `pass: false`, while the row's other cells keep their own verdicts.
- pass: every requested test ran and passed; a row with a skipped requested test is not a pass
  for that test, and the copied output stays as it is.

## Recovery

A board whose usbtest battery reported the device wedged (a HUNG case the in-run recovery could not clear, or a device it could no longer identify afterwards) gets a `board-wedged` cell and the rest of its examples are skipped for that run; that in-run recovery is a probe reset, or a reflash where the flasher has no reset. No admission marker blocks the next run. Before a board's first flash, a run without `--skip-flash` looks for a D-state `testusb` on that board's usbfs node, which a cancelled or killed run leaves behind, and resets the board through its convoy-safe recovery flasher (`flasher_recover`, else `flasher`); a board it cannot free is reported `board-wedged` without being flashed. A stray on a board outside the run is not swept. Interactively, follow `usb-kernel-recover` from its triage before running it again; a delegated run follows Results. `usb-kernel-recover` shows what a real wedge looks like and how to clear it; `usb-kernel-debug` explains WHY the kernel rejected a device (dmesg analysis).

A dispatched recovery action is its own operation, never part of a run: the prompt names one
board, its busport, the rung ceiling, the budget and the reservation. Follow `usb-kernel-recover`
from its triage up to that ceiling and never above it, holding the rig-wide reservation (Reservation) for every rig-wide rung.
The skill is agentrc's, read at the pin `tools/get_deps.py` records: `lib/agentrc/skills/usb-kernel-recover/SKILL.md`, its script
`lib/agentrc/skills/usb-kernel-recover/scripts/usb_recover.sh` run under `sudo` by that path (`tools/get_deps.py lib/agentrc` fetches
both); `tusb` has no such checkout and no shared recovery until it does.
Rung 1 resets through the board's recovery flasher (`flasher_recover`, else `flasher`); one that
is not `convoy_safe`, JLinkExe included, runs only behind `usb_recover.sh shield` on the board's
busport, unshielded afterwards;
sysrq and the hypervisor rungs need the user and are returned as the blocker in a headless run.
Report the cleanup explicitly: the board's state, plus any shield record or hold still standing.

A `cleanup-pending` board (an `openocd_esp` session left running) is recovered under a dispatch naming it: `python3 lib/agentrc/skills/hil/scripts/hil_openocd_esp.py end --receipt <receipt>`, and only on its exit 0 `python3 test/hil/helper/hil_lock.py release --token <token>`; on exit 1 the board stays held, and one whose init failed or is unproven needs a separately reviewed recovery.

## Limits

`hil_lock.py` fails closed: a lock dir, lock file or holder record it cannot use refuses the board before any
hardware access, as `<board>  Failed: board lock unavailable: <OS error>` with a `lock-unavailable` cell of
`❌ board lock unavailable: <OS error>`.

`hil_report.py` still folds two things: a FAIL cell from a flash that failed before any test ran is folded
`ran: true`, and a red usbtest cell is `❌ passed/total` whichever cases made it red; the `device/usbtest` failure
line lists those cases per status (`FAIL: [...]; HUNG: [...]; NOTRUN: [...]; BUDGET: [...]`). A skipped cell
(`Skip (no binary)`) is listed in the row's `skipped` and the row folds `pass: false`. `--accumulate` keeps each
changed row's earlier cells in the sidecar's `history`, oldest first, which `summarize()` exposes per board.
A pool-guard or abort report written over an unreadable sidecar keeps the earlier Markdown below its
table as labelled legacy text: those rows are in no JSON, and no verdict is read from them.

A board whose flasher probe has no VCOM (or whose BSP has no UART) uses RTT as its console — "No serial device found for /dev/serial/by-id/…" on every host test is the symptom. Config: `"logger": "rtt"` (jlink flashers only) plus a self-named variant carrying the define — `"variant": [{"name": "<board>", "defines": ["LOGGER=rtt"]}]` — and prebuilt example sets must carry the same `-DLOGGER=rtt`. Caveat: the cdc/msc-fixture host tests don't speak RTT yet, so such a board cannot carry `is_cdc`/`is_msc` fixtures (the config loader rejects it). Details: the `rtt` skill.

## Harness tests

Unit suites (no hardware) live in `test/hil/test/test_*.py`; the `hil-test` pre-commit hook
runs every `test_hil*.py`, `change-impact-test` `test_change_impact.py`, `test_ci_metrics.py` and
`test_hil_util.BottomLayer`. `test_change_impact.py` covers only selection, `test_ci_metrics.py`
only the code-size plumbing; the build and pool guards live in `test_hil_bounded.py` and
`test_hil_util.py` (the bounded sysfs reads' and the runner's own tests are agentrc's, with the modules); `test_hil_flash.py`
covers the flash adapters, the provider's loading and the rosters by its rules, `test_hil_report.py` the report document, `test_hil_rtt.py` and `test_hil_agentrc.py` how the harness loads agentrc's scripts and `test_hil_farm_check.py` the farm check's profile and its launcher's wiring to the
engine. Run them all when changing `test/hil`:
`for f in test/hil/test/test_*.py; do python3 "$f"; done` (about a minute, half of it
`test_hil_bounded.py`'s deliberate hang/timeout simulation).
