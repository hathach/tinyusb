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

Default to **local**. Use **remote** only when on `htpc` and the user says `remote`/`ci.lan`. Never attempt remote on `ci`.

## Prerequisites

Examples must be built for the target board(s) — see AGENTS.md "Build" → "All examples for a board" (produces `examples/cmake-build-<board>/`). `-B examples` points `hil_test.py` at that parent folder.

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

Append pass-through flags (`-v`, `-r 1`, …) to either command as needed.

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
