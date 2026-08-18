# Drop the EP0 post-prime verify — design

Date: 2026-08-16
Branch: `fix-ci-hs` (unpushed, 19 commits over merge-base `53fef2833`)

## Context

The branch grew while chasing a wedge on `mimxrt1064_evk`: the board would stop answering a
host transfer, the URB would never complete, `testusb` would block uninterruptibly and the
whole rig would follow it down. Eight occurrences over four days, across the Linux usbtest
battery's queued control and bulk tests.

The cause turned out to be silicon: **Errata i.MX RT1064_A / RT1060_A ERR050101**. While an
isochronous IN endpoint is active, an IN token addressed to that same endpoint number on
another device sharing the host silently unprimes one of this device's OUT endpoints —
control, bulk, interrupt or isochronous. NXP states it cannot be detected by software and
raises no interrupt. Moving the usbtest example's iso IN endpoint from 3 to 7 (commit
`42870b15b`) cleared it: 340 consecutive wedge-free runs, where the board previously
re-wedged within hours.

Before that was known, an earlier theory — a SETUP arriving mid-prime silently cancelling an
EP0 prime — produced a post-prime verification block in `qhd_start_xfer()`. That theory's
supporting capture (EP0's status ZLP armed but unprimed, the device a control transfer ahead
of the host) is explained by ERR050101 just as well, because the errata explicitly covers
*control* OUT endpoints and a control status stage **is** an OUT endpoint. The generalized
version of that verify was already reverted (`565bb0d99`) as both regression-prone and aimed
at a failure the vendor documents as undetectable in software. This spec removes what
remains of it.

## Change

Delete the post-prime block in `qhd_start_xfer()` (`src/portable/chipidea/ci_hs/dcd_ci_hs.c`):
the bounded `ENDPTPRIME` drain, the `ENDPTFLUSH`-on-timeout, and the
`ENDPTSTAT | ENDPTCOMPLETE` / `ENDPTSETUPSTAT` verdict. The tail becomes:

```c
  // start transfer
  dcd_reg->ENDPTPRIME = TU_BIT(epnum + (dir ? 16 : 0));
  return true;
```

This removes two register spins and four volatile reads from every EP0 transfer, and with
them the false-fail path a reviewer flagged: a transfer the interrupt handler has already
completed reads identically to a cancelled prime.

## Deliberately kept

- **The pre-prime setup-lockout guard** directly above it — UM10503 25.10.8.1.1 step 4
  verbatim ("Before priming for status/handshake phases ensure that ENDPTSETUPSTAT is '0'"),
  and older than the wedge theory. It also keeps `qhd_start_xfer()` returning `bool`, so
  `dcd_set_address()`'s gating and the usbd breakpoint removal stay meaningful — no cascade.
- **The setup-time EP0 flush and its completion wait** — the flush is the 25.10.8.1.1 step-3
  remark; the wait exists because an unfinished flush can retire a freshly primed response,
  an interaction independent of the verify.
- **The `BUS_RESET_START`/`END` split** and the rest of the review-driven hardening.
- Everything hardware-proven: the rf_tv fix, the lpc11u37 stack move, the lpc55s28
  onboarding, the lpc55 Make OHCI link, and the ERR050101 endpoint move itself.

The commit message records the corrected attribution of the handoff capture, so the next
reader does not re-derive the superseded theory from the same evidence.

## Validation

The "with it" arm is already banked from 2026-08-16: 10x 30/30 batteries plus 15x TEST 27,
15x tests 9/10 and 10x tests 11/12/24, all clean. This is the second half of an A/B.

1. **Rebase onto current master first** (master has moved: midi2/usbtmc/video), then rebuild —
   otherwise the validated tree is not the tree that merges.
2. **Software gates:** `pre-commit run --all-files`; full example builds for
   mimxrt1064_evk, lpcxpresso18s37, lpcxpresso11u37, lpcxpresso55s28; the two Make link
   canaries (`host/cdc_msc_hid` on lpcxpresso55s28, `device/cdc_msc_throughput` on
   lpcxpresso11u37); `ceedling test:all`.
3. **Hardware — mimxrt1064_evk only.** It is the only ci_hs board on the rig; the other two
   run ip3511, which this change does not touch. Preconditions: CI idle
   (`pgrep -f "hil_test.py [-]-retry"`), board lock held for the whole run. Flash with
   `loadfile` (its built-in Program & Verify — JLinkExe V9.66 has no `verifyfile`), then
   confirm re-enumeration as `cafe:4010` with serial `BAE96FB95AFA6DBB8F00005002001200`, and
   confirm `lsusb -v` still reports the iso IN endpoint as **0x87** so a stale image cannot
   masquerade as a pass.
4. **Runs:** 5x the full 30-case battery, then 15x `--tests 9,10,14,21` (queued control, ch9
   subset, both ctrl_out cases) — the control paths the verify actually protected, which a
   plain battery samples only once per run. Print a `testusb` D-state scan after every
   iteration.

**Acceptance:** 5/5 batteries at 30/30, 15/15 loops, and no `testusb` D-state outliving its
case runtime.

**Rollback trigger:** any control-case failure (errno 110 or 71 on cases 9, 10, 14, 21) or a
lingering D-state means the verify was load-bearing after all — restore it and record that
result in the commit message. A negative result is a finding, not a setback.
