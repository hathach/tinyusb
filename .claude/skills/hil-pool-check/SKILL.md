---
name: hil-pool-check
description: Use when asked for a pool check or board/probe health scan on a TinyUSB HIL rig, when probes or boards are offline or fail to flash, after rig maintenance, reboot, or re-cabling, or before starting a HIL test campaign.
---

# HIL Pool Check (board/probe health)

Health-scan the HIL board pool with `test/hil/helper/hil_pool_check.py`: per board it checks the flash
probe is on the USB bus, flashes a light example (`device/dfu_runtime`; host-only boards get
`host/device_info`, verified by serial output), waits for the board's uid to re-enumerate,
applies safe per-device recovery (probe authorized-toggle, board reset), re-parks with
`board_test`, and prints a summary table plus a USB topology report. Flags and details: `--help`
and the module docstring.

**REQUIRED BACKGROUND:** the `hil` skill owns config-by-hostname selection (run `hostname`
first) and the board-lock protocol. Locked boards are reported 🔒 locked and skipped — never
waited on, never bypassed; a needed build peeks the lock first. A CI worker reaching a board the
pool check holds fails it as "board locked" — prefer running between CI runs.

## A "pool check" means the full check

A request for a "pool check" means the DEFAULT full check below. Use `--scan-only` only when the
user explicitly asks for a quick look, or when you have VERIFIED a CI sweep is mid-run right now
(`python3 test/hil/helper/hil_lock.py status` shows `hil_test.py` holders) — "CI might be running" is not
that predicate: the full check is already lock-safe (CI-held boards report 🔒 locked and are never
touched), so an unconfirmed suspicion is no reason to downgrade. In either scan case say which
mode ran and why; never silently substitute the scan for the full check.

```bash
python3 test/hil/helper/hil_pool_check.py              # full check: ~10 s + ~1-2 s/board with firmware built;
                                                #   first run on an unbuilt tree takes minutes (it builds)
python3 test/hil/helper/hil_pool_check.py --scan-only  # USB presence only, <1 s, no locks/flashing/building
python3 test/hil/helper/hil_pool_check.py -b BOARD [-b …]  # subset; may name boards-skip (parked) entries

# from a dev PC, against the ci rig (bash -lc: flashers like STM32_Programmer_CLI live in ~/bin):
ssh ci.lan 'bash -lc "cd ~/code/tinyusb && python3 test/hil/helper/hil_pool_check.py"'
```

## Notes

Missing firmware is **built on the spot** — never skipped (`--no-build` opts out; those boards
then report `flash-failed`). Builds need the family env, referenced by its OFFICIAL variable so the docs hold on any
rig: `PICO_SDK_PATH` for rp2040/rp2350, `IDF_PATH` for espressif — activated explicitly as
`. "$IDF_PATH/export.sh"`, never as `get-idf` (an interactive alias; aliases are not expanded
in non-interactive shells, so scripts get `get-idf: command not found` even under `bash -lc`).
Each host exports both vars in `~/.bashrc` ABOVE the interactive early-return, which is what
makes a plain non-interactive `ssh <rig> 'cmd'` see them (verified on ci; where the checkouts
live is that host's business, not this file's). It also
needs `esptool` on PATH (pip's
`~/.local/bin/esptool`; a non-login shell may lack it — run via `bash -lc`). An explicit `-B` is
searched exclusively for *existing* firmware; builds still land in `cmake-build/` and are noted
`built <example>`. Espressif boards park too when the IDF env is present. A first run on an
unbuilt tree builds for many minutes: the Bash tool caps a foreground timeout at 10 min, so run
it in the BACKGROUND and NEVER cancel early — a killed run leaves detached cmake/ninja children
still writing to `cmake-build/` with the board locks held under a protected reason.

Statuses: `ok` (flashed and verified; in `--scan-only` it only means the probe is present),
`flash-failed` (firmware delivery failed: probe missing, build failed, flasher error, silent
no-op, park unverified), `failed` (check ran but did not verify), `locked` (flock held;
untouched). Exit code = `flash-failed` + `failed` (clamped at 125); `locked` and scan-only rows
are *unverified*, not healthy — read the footer, not just `$?`. A `⚠ pid … source says …` note
means stale firmware or a silent flash no-op (J-Link lore); a device off the bus entirely needs
the usb-kernel-recover skill or a physical replug.

## When the tool's probe recovery fails

`flash-failed` with the probe ✅ present and a `probe toggle unconfirmed` note means the probe's
own firmware is wedged, not the board. The tool's recovery is an `authorized` toggle, which is a
USB re-enumeration and never removes power, so probes that keep their sysfs kobject across it
(ST-Link, WCH-Link, CP210x, picoprobe) survive the toggle still wedged. Confirm with the flasher's
own list — `STM32_Programmer_CLI -l st-link`, or `JLinkExe -CommandFile <script>` with
`ShowEmuList` in it: a probe that enumerates but reports a blank serial/firmware is answering the
kernel and not the tool, which is a host-to-probe fault. A dead target reports the opposite: the
probe identifies itself normally and then fails to connect.

The next rung is a root-port bounce, and what it buys depends on which card the probe hangs off
(`readlink -f /sys/bus/usb/devices/usb<bus>` gives the PCI address):

- **Renesas** (five cards here): `uhubctl` lists their root hubs as `ppps`-capable, but the cards
  do not implement it — VBUS never drops, only D+/D− (see usb-kernel-recover). A cycle is therefore
  a harder forced re-enumeration, **not** a power cycle: worth one attempt, but a probe that rode
  out the `authorized` toggle can ride this out too. Do not read `ppps` here as power control.
- **AMD `0000:02:00.0`** (where the WCH-Links live): no port-power switching at all — `uhubctl`
  does not list it. There is nothing to cycle; go straight to a physical replug.

The leaf hubs are ganged, so a bounce hits every device under that root port. Escalate by hand, in
this order:

1. **Let the full run finish first.** Never cycle mid-run: the bounce re-enumerates siblings and
   would corrupt the checks still in flight for other boards.
2. Identify the subtree and its blast radius, so the report can name what was disturbed:
   ```bash
   ls -d /sys/bus/usb/devices/<bus>-<rootport>.*        # siblings that will be bounced
   ```
3. **Hold `--all` for the cycle, and release before the re-check.** `hil_lock.py status` only
   observes; CI can take a board a second later and flash straight into the bounce. The bounce
   hits every board under the root port and nothing maps a sysfs busport to a board name, so
   `--all` is the only reservation that actually covers them:
   ```bash
   python3 test/hil/helper/hil_lock.py hold --all --config test/hil/tinyusb.json --reason "probe power cycle"
   ```
   It is all-or-nothing: a refusal naming `hil_test.py` means a CI job is mid-test — wait, do
   not force, and do not substitute a partial hold. Release before step 5: `hil_pool_check.py`
   self-locks every board it checks and reports 🔒 locked for any it cannot take, so a hold
   still in place makes the whole verification pass report locked against you and verify nothing.
4. Cycle the ROOT port through the recovery script — rung 2 of usb-kernel-recover, which owns this
   invocation:
   ```bash
   sudo .claude/skills/usb-kernel-recover/scripts/usb_recover.sh \
        root-cycle <the wedged probe's own busport> [expected-serial]
   #   e.g. 13-1.6, NOT the 13-1 hub path from step 2 — the script derives the root port
   #   itself. Give the full path: the script ships inside the checkout, is on no PATH, and
   #   sudo's secure_path excludes the repo, so a bare `usb_recover.sh` is command-not-found
   ```
   Give it the device, not the hub: the expected-serial guard and the success check both read the
   path you pass, so handing it the hub compares the hub's serial and watches the hub's inode,
   which always changes when its own root port is cycled — it prints success while the probe is
   still dead. **Never a bare `uhubctl -a cycle` here.** Without `-S` it writes sysfs `disable`,
   whose `disable_store` takes the root hub's lock uninterruptibly and then calls
   `usb_disconnect()` on the child — against the wedged probe you are trying to clear, that blocks
   while holding the root hub's lock and poisons the whole bus. The script passes `-S`.
5. Release the locks, then re-check the affected boards:
   `python3 test/hil/helper/hil_pool_check.py -b BOARD [-b …]`. Include the bounced siblings — a
   cycle that fixes one probe can leave another unenumerated.

If the second pass still fails, the probe needs a physical replug: no software rung on this rig
removes VBUS, so there is nothing further to try.

## Reporting

The user-facing answer to a pool check IS the tool's summary table: paste the complete per-board
table (and footer counts) verbatim — never truncate rows or reduce it to a prose digest like
"27/27 healthy"; at most one line of commentary below it.

When an escalation above was needed, add a short note under the table naming: which boards needed
it, which root port was cycled (or that a replug was needed instead), which siblings bounced, and
the second-pass result for each.
Report BOTH passes — a final table showing every board ok hides the fact that a probe had to be
power-cycled to get there, which is exactly the signal that predicts it recurring.
