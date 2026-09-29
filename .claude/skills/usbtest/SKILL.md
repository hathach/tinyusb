---
name: usbtest
description: Use when bringing up examples/device/usbtest (cafe:4010) on a new MCU or DCD, or when a usbtest case fails — a red usbtest cell in a HIL report, testusb errno 110/32/5/71, NOTRUN or HUNG cases, device "did not bind", SET_CONFIGURATION fails, halt/toggle-clear/unlink/interrupt/isochronous failures. Load it before reproducing or debugging the case. A Linux PC hosts the link and drives TinyUSB in device role: it exercises the DCD, not the TinyUSB host stack.
---

# usbtest — bring-up and failed cases of the Linux kernel USB battery

`examples/device/usbtest` is the device-side peer of the kernel's `usbtest.ko`/`testusb`
(Gadget Zero source/sink): 30 cases over EP0, bulk, interrupt and isochronous, including halt,
data-toggle and unlink storms. The example README describes every case and tier; the host runner
is `test/hil/usbtest.py`.

**The battery tests the DCD, not the firmware.** A failed case points at the DCD path it
exercises (map below). Reproduce that one case, root-cause it on hardware before changing
anything (agentrc's `hw-debugger`), one variable at a time. A fix is proven when the case passes
and the full battery still passes across reflash cycles.

## Choose the path

| Situation                                        | Do                                                                                  |
|--------------------------------------------------|-------------------------------------------------------------------------------------|
| Routine full battery on rig boards               | The HIL contract (`hil` skill): `hil_test.py -b <board> -t device/usbtest <config>` |
| Chosen cases on a board you have not taken       | `scripts/run_case.py` (below)                                                       |
| Chosen cases inside a lock you hold, firmware on | `python3 test/hil/usbtest.py --serial <uid> --tests 13,29 --json`                   |
| What a case does, or whether its hang can clear  | `scripts/kernel_src.py` (below), then read the functions the case calls             |
| New MCU or DCD                                   | Bring-up ladder                                                                     |

Build first, through the build contract; `--variants` gives each roster variant its own
`cmake-build-<variant>` dir, the one HIL and `run_case.py` flash from. A board not yet in the
roster builds without `--variants` and is flashed by hand until it is added:

```bash
python3 .claude/skills/build/scripts/check_build.py --board <board> -e device/usbtest -e device/board_test --shared --variants <this host's config>
```

```bash
# lock, flash usbtest from the roster, wait for cafe:4010, run, park on board_test, release
python3 .claude/skills/usbtest/scripts/run_case.py --config <this host's config> --board <board> --tests 13,29 --after park
```

`--after leave` keeps usbtest running for a debug session. `--variant` is required when the board
has several. It refuses with exit 2 and the reason before touching hardware, including while any
battery runs on the host; pass `--allow-concurrent` only after checking no battery shares the
board's host controller. A wedged device is never parked. The last stdout line is its JSON
verdict.

## Rig hazards

- **Batteries are budgeted per host controller.** `hil_test.py` runs at most 2 per controller
  (`HIL_USBTEST_PARALLEL`); those permits live inside its process, so a battery started outside it
  is not counted. Unbudgeted batteries on one controller have frozen the rig, and a marginal DUT
  port bouncing under concurrent batteries has killed a uPD720201 (the note above
  `FLASH_PARALLEL` in `hil_lock.py`). Never start a battery outside `hil_test.py` on a controller
  another battery is using.
- **A wedged peer stalls every testusb on the host.** `testusb` opens every usbfs node while
  scanning, even with `-D` (`tools/usb/testusb.c` find_testdev), and opening a node takes its
  device lock. Once any device holds its lock for good, each new case blocks there in D state,
  and `usbtest.py` blames the device under test. When testusb runs without `sudo -n`, a hold that
  ends within the 30 s watch is a FAIL `timeout after Ns (the kill landed Ns late; a peer held the
  node)` that stops the battery; one that outlasts the watch becomes HUNG. Check for other D-state
  `testusb` processes on the host before trusting a HUNG verdict on a healthy board.
- **The id and binding stay.** `usbtest.py` registers `cafe 4010` once and never unbinds or
  removes it: those writes take the uninterruptible device lock. Recovery of a HUNG case is one
  step: a probe reset, or a reflash where the flasher has none (esptool); never a root-port
  cycle (`usb-kernel-recover`). Only the killed testusb reaping afterwards clears `wedged`, so
  under `sudo -n` (node not writable, the child is the wrapper) it stays set. A manual run
  without `--recover-board` leaves a HUNG device wedged. `wedged` is also set with no HUNG case
  when the serial matches two devices or cannot be read after a case; stderr names which
  (`reported wedged: ...`).

## Failed case

**Step 0: read the case in the rig's kernel.** The flags do not mean what they look like, and
whether a hang can clear depends on the wait the case reaches, not on the rig:

```bash
python3 .claude/skills/usbtest/scripts/kernel_src.py --release "$(ssh <rig> uname -r)" --case 24
```

It prints the `case N:` block of `usbtest_do_ioctl()` and every completion wait with its function.
A Debian release maps only to an upstream candidate, which the distro may have patched.
`testusb -c` is iterations, `-s` length, `-g` sglen, `-v` vary; the runner's per-speed values are
its `PARAMS` table. In v6.12, `test_ctrl_queue` (case 10), `unlink1` (11, 12), `unlink_queued` (24)
and `test_queue` (15, 16, 22, 23, 27, 28) wait with no timeout while holding the device lock, so a
device that stops answering there leaves `testusb` in D state for good; scatter-gather (5-8) runs
under a timer.

| Verdict or errno   | Meaning                                                                                    |
|--------------------|--------------------------------------------------------------------------------------------|
| 110                | Timeout: an endpoint NAKs forever or the device wedged                                     |
| 32                 | EPIPE: unexpected STALL                                                                    |
| 5                  | EIO: isochronous packet errors; dmesg says "N errors out of M"                             |
| 71                 | EPROTO: the device answered wrong or too slowly after host retries                         |
| NOTRUN             | testusb opened the device and the kernel skipped the case: profile or parameter gate       |
| FAIL "did not run" | testusb never reached the ioctl (open or usage error): read its captured stderr            |
| HUNG               | testusb not reaped 35 s after SIGKILL; under `sudo -n` 5 s and unconfirmed (wrapper only)  |
| BUDGET             | never dispatched: the battery stopped first; its detail names why                          |

| Failing case(s)    | Exercises                                  | First suspect                                                              |
|--------------------|--------------------------------------------|----------------------------------------------------------------------------|
| 9, 10              | EP0 control, queued control                | EP0 state machine, ZLP and status stage, control starvation under load     |
| 1-8, 17-20, 27, 28 | bulk source/sink, sg, perf                 | FIFO handling, multi-packet transfers, ZLP tolerance                       |
| 11, 12, 24         | URB unlink mid-transfer                    | abort and close paths that leave an endpoint half-armed                    |
| 13                 | set/clear halt                             | a stall must kill the armed transfer and flush a loaded IN FIFO            |
| 29                 | clear-halt on an armed, un-halted bulk OUT | toggle reset that also disarms the queued receive: NAKs forever, errno 110 |
| 14, 21             | vendor EP0 write and readback              | multi-packet control-OUT data stages                                       |
| 25, 26             | interrupt source/sink                      | usually clean once bulk works                                              |
| 15, 16, 22, 23     | isochronous                                | the isochronous rules below                                                |

Case 29 (`test_toggle_sync`: clear halt, write, clear halt, write) is the most common DCD bug here:
reset the toggle to DATA0 **and** keep the pending transfer armed (fixed that way in rp2040
ad7acc849, fsdev 046463687, ch32_usbhs d63a45509).

Escalate in order:
1. `usbtest.py`'s per-case detail and its captured dmesg (`TEST n` lines bracket each case).
2. usbmon (`usb-kernel-debug`): URB-level truth. It cannot show data toggles or NAKs, so a toggle
   desync and a dead endpoint look the same (Submits without Completes); tell them apart on the
   target.
3. Target-side evidence: `target-debug` (`esp-target-debug` on Espressif) on the failing case.
4. The reference manual and errata (`read-doc`) before changing any register-level code: DCD
   comments have been wrong about what the hardware can do.

## Isochronous rules

From USB 2.0 §8.5.5 (Calibre book 775, p. 229-230):
- **No handshake, no retry.** An isochronous endpoint never NAKs or STALLs; parts with a response
  field use their "no response" encoding.
- **No toggle sequencing.** A full-speed device sends only DATA0 and should accept DATA0 or DATA1:
  skip bulk-style toggle logic on isochronous endpoints in both directions (no toggle flip on IN,
  no toggle-mismatch drop on OUT). Symptom of breaking it: every other packet lost.
- **Full-speed sizes:** isochronous up to 1023 bytes per endpoint (§5.6.3), interrupt up to 64
  (§5.7.3). `TUD_OPT_HIGH_SPEED` is a build capability, not the live speed, so the full-speed and
  other-speed descriptors need full-speed sizes even on a high-speed build (`_FS`/`_HS` macros in
  the example's `src/usb_descriptors.h`).
- On ports that define `TUP_DCD_EDPT_ISO_ALLOC`, `dcd_edpt_iso_alloc`/`dcd_edpt_iso_activate` must
  work: a stub returning false fails the vendor interface open, SET_CONFIGURATION is refused and
  the runner reports "did not bind". Before accepting "the hardware has no isochronous", check the
  reference manual: two such claims in this tree were false.
- A multi-packet isochronous IN submit is legal: the DCD sends one packet per frame and refills in
  its ISR. Slow cores may need double buffering to meet the frame deadline.

## Bring-up ladder

1. **Tier 1, bulk and EP0** (`USBTEST_TIER 1`): enumeration, then cases 0, 9, 10 first (everything
   else reports through EP0), then 1-8, 11-13, 17-20, 24, 27-29 (`TIER_CASES` in `usbtest.py`).
2. **Tier 2** control-OUT (14, 21), **tier 3** interrupt (25, 26), **tier 4** isochronous
   (15, 16, 22, 23): raise the tier only when the layer below is clean, and run the full battery
   after each.
3. **Fit the endpoints.** Tier 4 needs six endpoints plus EP0. Small parts take per-MCU sizes in
   the example's `src/usb_descriptors.h` (`USBTEST_INT_EP_MPS_FS`, `USBTEST_ISO_EP_MPS_FS`) and
   `src/tusb_config.h` (`CFG_TUD_VENDOR_TX_EPSIZE`), as the CH32 and LPC11 entries do. A part whose
   DCD cannot serve a tier lowers its default `USBTEST_TIER` there, with the reason (RA2A1 is
   tier 3); one that cannot fit at all goes in `skip.txt`.
4. **Sign-off is every case of the justified tier, across 3-10 flash-and-run cycles.** The device
   advertises its tier in `bcdDevice`, so the battery is 30 cases only at tier 4. One pass proves
   nothing on a flaky bring-up; a deterministic partial count (exactly 1 in 8 lost) is a signature
   to chase, not noise.
5. Add the board to the HIL roster (`test/hil/*.json`) when roster edits are in the task's scope;
   otherwise hand it to the owner. Automated PR repair never touches the rosters.

## Wrong profile

`/sys/bus/usb/drivers/usbtest/new_id` lists only `cafe 4010`, never the profile behind it, and a
bound interface keeps the profile it was probed with. The right one probes as `Linux gadget zero`
in dmesg; the user-mode profile (`0525 a4a4`) passes bulk but reports 14, 21, 25, 26 and 15, 16,
22, 23 as NOTRUN. Repair is a module reload on a reserved, idle rig:

1. Reserve the whole fleet as the `hil` skill's rig-wide rule says, and confirm no `testusb` or
   `usbtest.py` is running: `modprobe -r` detaches every bound interface and blocks, unkillably,
   behind any case still in flight. It is never refused for an interface in use.
2. `sudo modprobe -r usbtest && sudo modprobe usbtest`.
3. The next `usbtest.py` or `hil_test.py` run registers the id again.
4. Confirm a `Linux gadget zero` probe for the next board, then release.

## Traps that pass gcc and desk review

Carried over from earlier ports, not re-verified:
- clang `-Wunused-function` and IAR `Pe177` reject an unused `static inline` that gcc accepts:
  `TU_ATTR_UNUSED`.
- A symbol referenced only from naked asm is dropped by `-flto` make builds: keep a
  `TU_ATTR_USED` C reference.
- Nested USB IRQs on cores with a hardware context stack (QingKe HWSTK): a plain
  `__attribute__((interrupt))` corrupts the return; use naked handlers.
- Dedicated USB RAM (PMA, USB-RAM) budgets differ per part and per build system's section
  placement: check the link map, not only that it builds.

## Red flags

- "One pass means done": run reflash cycles.
- "The DCD comment says the hardware can't": open the reference manual.
- "usbmon shows no toggle problem": usbmon cannot see toggles.
- "Fixed isochronous IN": apply the same exemption to OUT.
- "It works on gcc": clang, IAR, LTO and make builds are still pending.
- "A clean single-board run": fleet runs put two batteries per controller plus concurrent flashes
  on shared hub uplinks.
- Reasoning about a case from its name or a table row: run step 0.
