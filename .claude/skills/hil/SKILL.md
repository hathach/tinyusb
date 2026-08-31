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

Default to **local**. Use **remote** only when on a dev PC and the user says `remote`/`ci.lan`. Never attempt remote on `ci`.

`tusb` (ssh alias `hifiphile`) is an external rig (hosted by maintainer hifiphile), exercised by the
GitHub CI `hil-tinyusb (hfp.json)` matrix job — **never run HIL against it unless the user explicitly asks.**

## Board locks — the CI runner keeps running

The `ci` rig also hosts a GitHub Actions runner that flashes boards and runs HIL as part of CI. Hardware access is arbitrated **per board** with kernel flocks in `/tmp/tinyusb-hil-locks/` — do NOT stop the runner service.

- `hil_test.py` self-locks each board for its flash+test (holder reason `hil_test.py`). A locked board fails immediately (`<board>  Failed: board locked: {holder info}`) without flashing — in CI, re-run the failed job later; if your `hold` is refused with reason `hil_test.py`, a CI job is mid-test — wait a few minutes and retry rather than forcing.
- For hardware work outside `hil_test.py` (JLink/GDB, manual flashing, `usbtest.py`, serial poking), hold the lock first:

```bash
python3 test/hil/helper/hil_lock.py hold BOARD [BOARD...] --reason "why"
# ... hardware work ...
python3 test/hil/helper/hil_lock.py release BOARD [BOARD...]
```

- Never pre-hold boards you are about to run `hil_test.py` on — it self-locks and would treat your own hold as a conflict.
- Rig-wide operations (uhubctl power cycling, `usb_recover.sh root-cycle`, pci-rebind, controller resets — bus renumbering) affect every board: `hil_lock.py hold --all --config <this host's config> --reason "..."` first — `--all` defaults to `tinyusb.json`, so on `tusb` it would reserve 27 boards that do not exist there and none of the three that do. Even a single root-port bounce needs `--all`: nothing maps a sysfs busport to a board name, and `hil_lock.py hold` accepts any string, so a "just the siblings" hold reserves nothing while reporting success.
- `hil_lock.py status` lists holders. Locks auto-release when the holder process dies (kernel flock); `/tmp` clears on reboot.
- Forcing past a lock: `HIL_NO_BOARD_LOCK=1 python3 test/hil/hil_test.py ...` bypasses the guard without killing the holder. Only with the user's explicit go-ahead — they accept the risk of colliding with whatever holds the board.

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
  python3 test/hil/hil_test.py -B examples $ARGS test/hil/tinyusb.json   # $ARGS empty when full: run everything
else
  echo "diff affects nothing on this rig - skip HIL"
fi
```

Read `full`, never `args` alone: `args` is empty for BOTH `full: true` (run the whole matrix — a broad or
unclassified change) and "nothing selected" (skip). Skip only when `full` is false AND `args` is empty.

Unit suites (no hardware), all five run by the `hil-test`/`ci-select-test` pre-commit
hooks: `test_ci_select.py` covers only selection, `test_ci_metrics.py` only the code-size
plumbing. The containment work --- bounded reads, the kill ladders, the build and pool
guards --- lives in `test_hil_bounded.py`, `test_hil_health.py` and `test_hil_util.py`, so
run all five when changing `test/hil`:
`for f in test/hil/test/test_*.py; do python3 "$f"; done` (~84s, of which
`test_hil_bounded.py` is ~76s of deliberate hang/timeout simulation; the two `test_ci_*`
suites are ~4s together).

## Pre-flight rig health check

`hil_test.py` notes any process already in D state when the run starts, as one line above
the table. It never aborts, and it is a hint rather than a diagnosis. What bounds a stuck
run is `HIL_POOL_TIMEOUT` plus the job's `timeout-minutes`; what diagnoses a wedged rig is
the `hil-pool-check` skill.

See the `usb-kernel-recover` skill for what a real wedge looks like and how to clear it.

## Prerequisites

Examples must be built for the target board(s) — see CLAUDE.md "Build" → "All examples for a board" (produces `examples/cmake-build-<board>/`). `-B examples` points `hil_test.py` at that parent folder. (This applies to `hil_test.py`; `hil_pool_check.py` builds its own missing firmware.)

## Arguments

- **Board:** `-b BOARD_NAME`, repeatable for a subset (`-b a -b b`); omit to run all boards in the config. Give a whole set to ONE run rather than one run per board: it schedules the boards across host controllers and budgets concurrent flashes and usbtest batteries per controller (`hil_lock.py` `FLASH_PARALLEL`/`USBTEST_PARALLEL`). Those permits are in-process semaphores — a second `hil_test.py` running alongside does not share them, it multiplies the load on the same xHCI cards.
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

# A subset — repeat -b, ONE invocation for the whole set:
bash test/hil/hil_ci.sh -b raspberry_pi_pico2 -b stm32f723disco -t host/cdc_msc_hid -r 1
```

One invocation per board is wrong here, not merely slow: each run `rm -rf`s `REMOTE_DIR`
and rewrites the report, so only the last board's rows survive.

Env overrides: `REMOTE`, `REMOTE_DIR`, `CONFIG`. Fails fast if the build dir/repo layout is missing.

## Timing

Runs take 2-5 min per board, but a stuck fleet runs to `HIL_POOL_TIMEOUT` — 60 min
unless the env pins it. The run logs its guard in the startup line; never declare a run
stuck before THAT value has elapsed.
The Bash tool caps a foreground timeout at 10 min, so **run it in the background** and
wait for the completion notification -- never a foreground timeout, which would kill
the run before its own guard can write a report. NEVER cancel early.

## Reporting

The user-facing answer to a HIL run IS the tool's summary table: paste the complete per-board
table (and footer counts) verbatim — never truncate rows or reduce it to a prose digest; at most
one line of commentary below it.

**First check what sits above the table.** Seven banners can appear there; match on a
PREFIX, since each carries trailing detail and one is a blockquote:

- `**HIL run abandoned: worker pool timed out after …s.**` — no results were collected this
  attempt, so any table below is a PREVIOUS attempt's. Report the abandonment, never those
  rows, and never `"pass": true`.
- `**HIL run aborted: a worker raised …**` — same rule: a worker crashed before results
  were collected; any table below is stale. Report the abort, never the rows.
- `**HIL run abandoned: the worker pool would not shut down.**` — DIFFERENT: the table
  below IS this run's, but the pool could not be shut down afterwards (the job exits
  non-zero even if every board passed). Report the results AND the abandonment; never
  `"pass": true`.
- `**HIL run selected no boards.**` — the filters intersected to nothing, so there is no
  table at all. Report that (and the filter shown), never `"pass": true`.
- `> **Rig note.**` — a process was in D state when the run started. This is NOT a wedge:
  a healthy in-flight testusb is uninterruptible for most of every case, and the rig
  supports a dev run alongside CI. On its own it is never `wedged: true` and never turns a
  green table into `"pass": false`. Mention it only when a board below failed, as the first
  thing to check.
- `> **Rig dirty.**` — a process survived SIGKILL and still holds a probe or usbfs node
  into the NEXT job. The table below is this run's and can be reported, but say the rig is
  dirty: the next job starts degraded and nothing in the harness can clear it.
- `> **Not all verdicts are evidence.**` — one or more workers went blind on sysfs, so
  "device not found" from the named boards means "could not tell". Do NOT report their red
  cells as broken boards.

On failure, retry once with `-v` — from the `<config>.failed` spec the run just wrote, which
already begins with `--accumulate` and restricts each board to its failed tests. A hand-scoped
`-b <board>` retry MUST pass `--accumulate` too: a fresh run unlinks the report, replacing the
whole-fleet table with a one-row table. If that is still not enough, add temporary debug prints
to `hil_test.py`.
