---
name: hil
description: Use when running TinyUSB Hardware-in-the-Loop (HIL) tests on physical boards, debugging HIL failures, or copying firmware to the ci.lan test rig. Covers local execution and remote execution over SSH, config selection, and debugging tips.
---

# Hardware-in-the-Loop (HIL) Testing

Run TinyUSB HIL tests against real boards. Two execution modes — **local** (boards attached to this machine) and **remote** (boards attached to `ci.lan`, reached over SSH). Default to **local** unless the user specifies `remote`. Do not auto-detect.

## Prerequisites

- Examples must already be built for the target board(s). See AGENTS.md "Build" → "All examples for a board", which produces `examples/cmake-build-<board>/`.
- `-B examples` tells `hil_test.py` that `examples/` is the parent folder containing the per-board build outputs.

## Choosing arguments

Infer from the user's request:

- **Mode:** `local` (default) or `remote`. Only switch to `remote` if the user explicitly says so or names `ci.lan`.
- **Board:** if the user names a specific board, pass `-b BOARD_NAME`. Otherwise omit `-b` to run all boards in the config.
- **Pass-through flags:** `-v` (verbose), `-r N` (retry count), etc. — pass through unchanged.

Config file follows from mode:
- **Local** → `test/hil/local.json` (user-supplied; not tracked in repo — describes boards attached locally)
- **Remote** → `test/hil/tinyusb.json` (tracked; describes the `ci.lan` test rig)

If `local.json` is missing, fall back to `tinyusb.json` only when explicitly told to; otherwise stop and ask the user to supply one.

## Local execution

Boards attached to this machine:

```bash
# Specific board:
python3 test/hil/hil_test.py -b BOARD_NAME -B examples test/hil/local.json $EXTRA_ARGS
# All boards in the config (no -b):
python3 test/hil/hil_test.py -B examples test/hil/local.json $EXTRA_ARGS
```

## Remote execution (ci.lan)

Use `test/hil/hil_ci.sh` — it handles dir setup, scp of test scripts, rsync of firmware artifacts (`.elf` / `.bin` / `.hex` only), and running `hil_test.py` on `ci.lan`:

```bash
# Specific board:
bash test/hil/hil_ci.sh -b raspberry_pi_pico2
# All boards in tinyusb.json:
bash test/hil/hil_ci.sh
# Pass-through extra args (any non -b flag is forwarded to hil_test.py):
bash test/hil/hil_ci.sh -b raspberry_pi_pico2 -t host/cdc_msc_hid -r 1
```

Overrides via env vars: `REMOTE=ci.lan`, `REMOTE_DIR=/tmp/tinyusb-hil`, `CONFIG=test/hil/tinyusb.json`.

The script fails fast if the build dir or repo layout is missing.

## Timing

HIL runs take 2-5 minutes. Use a timeout of at least 20 minutes (600000 ms). NEVER cancel early.

## Reporting results

After the test completes:
- Show the test output to the user.
- Summarize pass/fail per board.
- On failure, suggest re-running with `-v` for verbose output. If `-v` isn't enough, temporarily add debug prints to `test/hil/hil_test.py` to pinpoint the issue.
