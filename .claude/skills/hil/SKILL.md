---
name: hil
description: Use when running TinyUSB Hardware-in-the-Loop (HIL) tests on physical boards, debugging HIL failures, or copying firmware to the ci.lan test rig. Covers per-host config selection (infra rigs ci/tusb use tinyusb.json/hfp.json, any dev PC uses local.json), local and remote execution, the board-lock protocol, and debugging tips. For board/probe health scans ("pool check") use the hil-pool-check skill.
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

## Pool check (board/probe health)

Board/probe health scanning (`test/hil/hil_pool_check.py`) has its own skill: **hil-pool-check**.
Use it before a HIL campaign, after rig maintenance/reboot, or when boards fail to flash.

## PR-scoped selection

`test/hil/hil_select.py` maps a diff to affected boards/tests (used by CI on PRs; fail-open
to the full matrix). Manual use:

```bash
SEL=$(python3 test/hil/hil_select.py --base master test/hil/tinyusb.json)
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

Unit suite: `python3 test/hil/test_hil_select.py` (no hardware).

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

The user-facing answer to a HIL run IS the tool's summary table: paste the complete per-board
table (and footer counts) verbatim — never truncate rows or reduce it to a prose digest; at most
one line of commentary below it. On failure, retry with `-v`; if that's not enough, add temporary
debug prints to `hil_test.py`.
