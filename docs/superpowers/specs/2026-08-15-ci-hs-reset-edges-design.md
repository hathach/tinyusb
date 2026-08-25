# Bus-reset edge events + review fix wave — design

Date: 2026-08-15
Branch: `fix-ci-hs` (unpushed, 6 commits over master `53fef2833`)

## Problem

A max-effort review of the branch produced 15 findings. Four are regressions the branch
itself introduced; the rest are pre-existing or cross-cutting. The load-bearing one:

`dcd_ci_hs.c` now runs the RM-prescribed reset cleanup at the URI (reset-start) interrupt
but does not tell usbd until the Port Change Detect that ends the reset. For the whole
reset window — a minimum of 3 ms, typically 10–50 ms — usbd still believes the device is
configured while the DCD's queue heads have been zeroed. A class driver writing in that
window (`tud_hid_n_report()`, `tud_cdc_write_flush()`) primes a disabled endpoint over a
zeroed dQH, *after* the cleanup's flush, so the stale prime survives re-enumeration over a
buffer usbd has already released. On a 600 MHz M7 that window is enormous. Master had no
gap: cleanup and event were adjacent statements.

The stack has no way to express "reset started" — `DCD_EVENT_BUS_RESET` carries the
negotiated speed, which does not exist until the reset ends. That missing vocabulary is
the actual defect; the driver-level workarounds considered (deferring the memclr, guarding
primes with a private flag) only shrink the window.

## Design

### 1. Stack: split the bus-reset event into two edges

`src/device/dcd.h`:

```c
DCD_EVENT_BUS_RESET_START, // reset signaling detected; bus unusable, speed unknown
DCD_EVENT_BUS_RESET_END,   // reset complete; .bus_reset.speed is final
...
#define DCD_EVENT_BUS_RESET DCD_EVENT_BUS_RESET_END  // backward compatibility
```

No new helper: `dcd_event_bus_reset(rhport, speed, in_isr)` keeps its name and emits
`_END`, so every other port is bit-identical to today; `_START` uses the existing
payload-free `dcd_event_bus_signal()`. The alias keeps unit-test/fuzz references
compiling.

**Contract (documented in `dcd.h`):** `_START` is optional. A DCD that cannot distinguish
the two edges emits only `_END`, which stays self-sufficient — it performs the full
teardown with or without a preceding `_START`.

`src/device/usbd.c`:
- `case DCD_EVENT_BUS_RESET_START:` → `usbd_reset(rhport)` only; speed untouched.
- `case DCD_EVENT_BUS_RESET_END:` → unchanged (`usbd_reset()` + latch speed).
- `_usbd_event_str[]` gains both names.
- `TODO:` note that a DCD signalling both edges should not pay for two teardowns — track
  a per-rhport "start seen" flag and skip the redundant `usbd_reset()` in `_END`, keeping
  the unconditional teardown for the legacy single-event path.

Cost, accepted deliberately: one extra queued event and one extra `usbd_reset()` per
enumeration on ci_hs only, bounded at one per reset against a default
`CFG_TUD_TASK_QUEUE_SZ` of 16 (queue pressure is the failure PR #3817 fixed, hence the
explicit note).

### 2. ci_hs: split `bus_reset()` along the register/software line

- **`bus_reset_begin()` — at URI, inside the reset window (UM10503 25.10.3):** ENDPTCTRL
  type-reset loop, `ENDPTNAK`/`ENDPTNAKEN`, `ENDPTSETUPSTAT` and `ENDPTCOMPLETE`
  write-back clears, bounded `ENDPTPRIME` drain, `ENDPTFLUSH` all. Emit `_START`.
  Registers only — nothing in `_dcd_data` is touched, so no software structure is pulled
  out from under a task mid-`dcd_edpt_xfer`.
- **`bus_reset_complete()` — at the PCI ending the reset:** re-flush, `tu_memclr(&_dcd_data)`,
  EP0 queue-head re-init, dcache clean. Emit `_END` with the final PSPD speed.

Two properties fall out: the re-flush kills any prime armed during the window without a
new state flag, and the memclr now happens at the same instant usbd is told, so the
"configured over zeroed queue heads" mismatch is eliminated rather than shrunk. Residual
exposure (a task priming exactly as the ISR memclrs) equals master's.

The reason-dispatch (`pci_reason`, suspend/URI ordering) is unchanged; only the reset
case's body moves.

### 3. ci_hs: one bounded-flush helper

Extract `flush_endpoints(dcd_reg, mask)` — writes `ENDPTFLUSH = mask`, spins bounded by
`CI_HS_BUSY_SPIN` until those bits clear, returns `true` if they cleared — and route all
five flush sites through it (`bus_reset_begin`, `bus_reset_complete`, `dcd_deinit`,
`dcd_edpt_iso_activate`, the setup-time EP0 flush). The unified part is the mechanism
(one bound, one spin idiom, one return convention); callers keep their existing reactions,
all of which currently proceed regardless, and that stays true here — no caller gains new
error handling in this wave. Without this, §2 adds a fifth site to a file that already
carried four hand-rolled variants.

### 4. Mechanical fixes

`dcd_ci_hs.c`
- Setup-time EP0 flush waits for completion (via §3's helper) before the SETUP event is
  queued, so the flush can no longer still be asserted when the task primes the response —
  which also dissolves its interaction with the post-prime verify. This adds a bounded
  spin in ISR context; the RM notes a flush waits out any packet already in progress, so
  the wait is one packet time (microseconds at HS) and the existing `CI_HS_BUSY_SPIN`
  bound caps the pathological case, consistent with the file's other flush sites.
- `dcd_set_address()` writes `DEVICEADDR` only if the status-ZLP prime took. A refused
  prime means a newer SETUP superseded the transfer; staging an address whose ACK will
  never arrive is wrong.
- Emit `DCD_EVENT_RESUME` only when `!(PORTSC1 & PORTSC1_SUSPEND)` (restores master's
  hardware guard, lost in the rework).

`dcd_lpc_ip3511.c`
- Deliver the setup copy only when known-good:
  `if (latch still set) { INTSETSTAT = TU_BIT(0); } else { dcd_event_setup_received(...); }`.
- `TODO:` token on the USB.13 deferral so backlog sweeps surface it.

`usbd.c`
- The DCD-refusal path in `usbd_edpt_xfer` stops routing through the breakpoint-carrying
  assert: a DCD declining a prime is documented and self-healing, not a programming error,
  and `TU_BREAKPOINT()` is not gated on `CFG_TUSB_DEBUG` — with a probe attached (always,
  on the rig) it halts the target. Log and return false instead.

BSP
- Delete the seven-line RHPORT block in `lpcxpresso55s28/board.cmake` (byte-identical to
  `family.cmake`'s own guards; `board.mk`'s `?=` stays as the idiomatic Make form).
- `lpc11u37.ld`: correct the stale comment (nothing lands in RamUsb2 in either build
  system now — the stack owns the whole bank) and keep the ASSERT, re-labelled as
  future-proofing.

## Findings improved for free (documented, no code)

A reset that starts and never completes — cable pulled mid-reset — now delivers `_START`
and tears usbd down, where before usbd stayed configured on a dead bus. This softens both
the adjudicated UNPLUGGED-removal finding and the deferred aborted-reset item: a stray
later PCI delivering `_END` becomes harmless (usbd already torn down, just latches a
speed) instead of deconfiguring a live device. True detach detection still requires OTGSC
B-session-valid VBUS sensing — board-dependent, still a follow-up.

## Explicitly deferred

- Prime verification generalized to all endpoints and all causes (RM 25.10.8.2); the
  EP0/SETUP-gated form stays, its flush interaction fixed by §4.
- usbd discards `usbd_control_xfer_cb`/`tud_control_xfer` returns — cross-DCD behavior
  change needing its own regression pass, despite `usbd.c` being open here.
- Timed-out flush still proceeds to the memclr (now confined to one helper).
- LPC55S2x USB.3 FORCE_FS workaround; iso-IN 1023 enforcement; 8-byte OUT-spill
  enforcement; USB.13 INTONNAK workaround.
- Gating `TU_BREAKPOINT()` on `CFG_TUSB_DEBUG` stack-wide.
- Unguarded `set()` RHPORT knobs in ~14 sibling `board.cmake` files.

## Verification

1. `pre-commit run --all-files`; builds for mimxrt1064_evk, lpcxpresso18s37,
   lpcxpresso11u37, lpcxpresso55s28, plus Make link checks for the two previously-broken
   targets (`host/cdc_msc_hid` on 55s28, `device/cdc_msc_throughput` on 11u37).
2. Cross-DCD build guard: one non-ci_hs, non-ip3511 board (e.g. `stm32f407disco`) to prove
   the `DCD_EVENT_BUS_RESET` alias keeps legacy ports compiling untouched.
3. HIL on byte-verified flash (`verifyfile` on every J-Link load — the 1064's silent
   flash no-op has struck twice): usbtest 30/30 on mimxrt1064_evk, lpcxpresso55s28,
   lpcxpresso11u37; 50× case-9/10 loops on the 1064; 10× case-11/12/24 unlink loops.
4. Reset-path specific: confirm HS enumeration (480) and, with `LOG=2`, that a single
   enumeration shows exactly one `_START`/`_END` pair and no spurious RESUME.
5. Suspend/resume exercise on the 1064 (host-side autosuspend on the port) confirming
   `SUSPEND`/`RESUME` pairing and no reset misclassification.

## Success criteria

All four regressions closed, no new findings in a scoped re-review of the wave diff, every
listed HIL result green on verified flash, and legacy DCDs provably untouched (alias build
check + unchanged `_END` semantics).
