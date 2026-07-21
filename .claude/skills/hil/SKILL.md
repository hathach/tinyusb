---
name: hil
description: Use when running TinyUSB Hardware-in-the-Loop (HIL) tests on physical boards, debugging HIL failures, or copying firmware to the ci.lan test rig. Covers per-host config selection (htpc uses local.json, ci uses tinyusb.json), local execution on either htpc or ci, remote execution over SSH from htpc, and debugging tips.
---

# Hardware-in-the-Loop (HIL) Testing

Run TinyUSB HIL tests on real boards. **Run `hostname` first** — it tells you which host you are on, which determines the default config and whether remote mode is possible.

| Host | Local config | Remote (SSH → ci.lan)? |
|------|--------------|------------------------|
| `htpc` (dev PC) | `test/hil/local.json` | yes (large pool, `test/hil/tinyusb.json`) |
| `ci` (the rig) | `test/hil/tinyusb.json` (large pool) | no — can't SSH to htpc, and boards are already local |
| `hifiphile` (external rig) | `test/hil/hfp.json` | no outbound SSH to htpc/ci; SSH-reachable FROM both |

Default to **local**. Use **remote** only when on `htpc` and the user says `remote`/`ci.lan`. Never attempt remote on `ci`.

`hifiphile` is an external rig (hosted by maintainer hifiphile), exercised by the GitHub CI
`hil-tinyusb (hfp.json)` matrix job — **never run HIL against it unless the user explicitly asks.**

## Board locks — the CI runner keeps running

The `ci` rig also hosts a GitHub Actions runner that flashes boards and runs HIL as part of CI. Hardware access is arbitrated **per board** with kernel flocks in `/tmp/tinyusb-hil-locks/` — do NOT stop the runner service.

- `hil_test.py` self-locks each board for its flash+test (holder reason `hil_test.py`). A locked board fails immediately (`<board>  Failed: board locked: {holder info}`) without flashing — in CI, re-run the failed job later; if your `hold` is refused with reason `hil_test.py`, a CI job is mid-test — wait a few minutes and retry rather than forcing.
- For hardware work outside `hil_test.py` (JLink/GDB, manual flashing, `usbtest.py`, serial poking), hold the lock first:

```bash
python3 test/hil/board_lock.py hold BOARD [BOARD...] --reason "why"
# ... hardware work ...
python3 test/hil/board_lock.py release BOARD [BOARD...]
```

- Never pre-hold boards you are about to run `hil_test.py` on — it self-locks and would treat your own hold as a conflict.
- Rig-wide operations (uhubctl power cycling, pci-rebind — bus renumbering) affect every board: `board_lock.py hold --all --reason "..."` first.
- `board_lock.py status` lists holders. Locks auto-release when the holder process dies (kernel flock); `/tmp` clears on reboot.
- Forcing past a lock: `HIL_NO_BOARD_LOCK=1 python3 test/hil/hil_test.py ...` bypasses the guard without killing the holder. Only with the user's explicit go-ahead — they accept the risk of colliding with whatever holds the board.

## Prerequisites

Examples must be built for the target board(s) — see CLAUDE.md "Build" → "All examples for a board" (produces `examples/cmake-build-<board>/`). `-B examples` points `hil_test.py` at that parent folder.

## Arguments

- **Board:** `-b BOARD_NAME` for one board; omit to run all boards in the config.
- **Pass-through:** `-v`, `-r N`, etc. forwarded unchanged.

If `local.json` is missing on `htpc`, ask the user to supply one (only fall back to `tinyusb.json` if told to).

## Local execution

Set `CONFIG` from `hostname` first (`test/hil/local.json` on htpc, `test/hil/tinyusb.json` on ci):

```bash
CONFIG=test/hil/local.json      # on ci use: CONFIG=test/hil/tinyusb.json

# All boards in the config:
python3 test/hil/hil_test.py -B examples "$CONFIG"

# A single board (replace stm32f723disco):
python3 test/hil/hil_test.py -b stm32f723disco -B examples "$CONFIG"
```

## Remote execution (htpc → ci.lan only)

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

Show the output, summarize pass/fail per board. On failure, retry with `-v`; if that's not enough, add temporary debug prints to `hil_test.py`.
