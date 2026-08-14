# CH32H417 HS campaign — measurements and EP0 differential

Working notes for `docs/superpowers/plans/2026-08-14-usbtest-30-30-h417-hs.md`.

## Phase 0a — CH569 HS oracle: CONFIRMED (2026-08-14)

The plan's premise was a memory claim from a previous session. Re-measured today:

```
cmake -B /tmp/ch569-hs/cmake-build-hydrausb3_v1 -DBOARD=hydrausb3_v1 -DSPEED=high \
      -DCMAKE_BUILD_TYPE=MinSizeRel -G Ninja .
python3 test/hil/hil_test.py -B /tmp/ch569-hs -b hydrausb3_v1 test/hil/local.json
```

| Board          | usbtest   | cdc_msc_throughput          | suite  |
| -------------- | --------- | --------------------------- | ------ |
| `hydrausb3_v1` | **30/30** | C 5.4/7.6 MB/s M 28.4/16.2 MB/s | 12/12 |

All 30 cases run at high speed (the quirk gates are SuperSpeed-only) and all pass,
**including 13 ep-halt, 14 and 21 ctrl_out** — the exact cases the H417 fails.

This is the load-bearing result for Phase A. Same usbd, same deferred status-stage
arming, same uPD720201 host, same rig. So:

- usbd's deferred model is **not** inherently fatal to this class of controller;
- the H417 failures are **H417-specific**, in its driver or its silicon;
- `dcd_ch56x_usbhs.c` is a legitimate oracle to diff against.

Decision: the oracle holds → proceed to Task 3, then the Phase A differential
(Tasks 4 and 5). Phase B is not needed unless Phase A falls short.

## Phase 0b — H417 baseline (2026-08-14)

Build: `-DBOARD=nanoch32h417 -DSPEED=high -DCMAKE_BUILD_TYPE=MinSizeRel`, driver at
commit `e2ebfb9a9`.

**Failing set, three fresh flashes — stable:**

| Run | Score | Failing                  |
| --- | ----- | ------------------------ |
| 1   | 25/30 | 9, 10, 13, 14, 21        |
| 2   | 24/30 | 9, 10, 13, 14, 21, 29    |
| 3   | 24/30 | 9, 10, 13, 14, 21, 29    |

9/10/13/14/21 is the real set; 29 flaps. Matches the spec.

**Pooled baseline drop rate: 34.5%** (109 dropped of 316 SETUPs, 5 fresh-flash runs,
per-run 33.8 / 30.6 / 33.9 / 39.1 / 34.9%).

Harness note: the first attempt started the capture *after* the flash and pooled only
42 SETUPs across 4 runs, and the harness correctly refused the number (`rc=2`). `lsusb -v`
answers mostly from cached descriptors and issues few live control transfers. Starting the
capture *before* the flash includes enumeration -- the densest control burst the device
sees, on a genuinely cold controller. That also tightened the per-run spread from a
27-point band (44-71%) to 8.5 points (30.6-39.1%), which is what makes the metric usable
for A/B comparison at all.

**This 34.5% is the reference every Phase A candidate is measured against.**

## Phase A — differential inventory (2026-08-14)

Oracle: `dcd_ch56x_usbhs.c` (30/30 at HS). Subject: `dcd_ch32h417_usbhs.c` (25/30).

### The four axes

**1. SETUP detection.** CH569 has a *dedicated* SETUP interrupt (`RB_USB_IE_SETUPACT`,
`R8_USB_INT_EN` line 238) and an unambiguous branch that clears its own flag. H417 has no
such interrupt (`R8_USB_INT_EN` has no SETUP bit) and must infer SETUP from the sticky
`RB_UEP_R_SETUP_IS` inside the TRANSFER branch. This is the deepest architectural
difference and is **not portable** -- the H417 silicon simply lacks the interrupt.

**2. Status-stage arming.** CH569 does **NOT** pre-arm the status OUT. Its `update_in`
EP0-completion branch is just `EP_TX_CTRL(0) = UEP_T_RES_NAK | tog` and EP0 RX is left at
NAK until usbd submits the status stage -- the same deferred model, and it still reaches
30/30. H417 currently pre-arms in the ISR. **The oracle contradicts our pre-arm.**

**3. Toggle at SETUP.** CH569 writes `EP_RX_CTRL(0) = ACK/NAK` and `EP_TX_CTRL(0) =
UEP_T_RES_NAK` with the toggle bits **zero (DATA0)**. H417 writes `| USBHS_UEP_x_TOG_DATA1`
on both. Both then drive the real toggle from software (`ep0_tog` / `ep0_rx_tog`) when
arming, so the register value at SETUP time is mostly vestigial -- but it is a difference.

**4. Interrupt-flag bookkeeping.** CH569 explicitly clears `R8_USB_INT_FG` in every branch.
H417's TRANSFER branch clears only the per-endpoint DONE bits, which is correct for this
part (`RB_UDIF_RTX_ACT` is RO, RM 25.2.1.8) and **not** a portable difference.

### Candidates, highest expected impact first

### C1: remove the ISR status pre-arm
- CH569 does: nothing -- EP0 RX stays NAK until usbd submits the status stage; 30/30.
- H417 does: `ep0_rx_tog = true; arm_ep0_out();` in `update_in`'s EP0 completion.
- Change: delete the pre-arm and the `ep0_status_out_early` latch it requires.
- Prediction: if the pre-arm is not what makes H417 work, the drop rate is unchanged and
  the code loses four separate defects at once (review findings 3, 9, 10, 14 all exist
  only because of it). If the drop rate worsens materially, the pre-arm is load-bearing
  and the findings must be fixed individually instead.
- Portable? Yes -- removal, toward the oracle.
- Note: its original justification (63% -> 37%) came from single ~90-SETUP captures, the
  method later shown to span a 27-point band on one unchanged build. It is unproven.

### C2: SETUP-branch toggle to DATA0
- CH569 does: `EP_TX_CTRL(0) = UEP_T_RES_NAK` (toggle bits 0).
- H417 does: `EP_TX_CTRL(0) = USBHS_UEP_T_RES_NAK | USBHS_UEP_T_TOG_DATA1`.
- Change: drop the `| TOG_DATA1` from the SETUP branch's TX write.
- Prediction: probably neutral -- `queue_in_packet` sets the real toggle from `ep0_tog`.
- Portable? Yes.

### C3 (review #4): SETUP must not destroy a co-pending EP0 IN completion
- H417 does: `xfer_status[0][TUSB_DIR_IN].valid = false` plus a full-byte TX write that
  clears a pending `RB_UEP_T_DONE`, so the previous transfer's status-IN completion is
  never delivered and `CONTROL_STAGE_ACK` never fires (drops HID SET_REPORT payloads).
- Change: service a pending TX completion before resetting the pipe, or reset only the
  OUT side.
- Portable? Yes. Independent of C1.

### C4 (review #12): reset the new module-level flags in `ch32h417_usb2_init`
- H417 does: never assigns `sof_enabled`, `bus_suspended`, `ep0_status_out_early`; the
  bus-reset branch misses `sof_enabled`. A stale latch emits SOF events usbd never asked
  for, which `usbd.c` turns into a fabricated RESUME that cancels a real suspend.
- Change: zero all three in init; add `sof_enabled` to the bus-reset reset.
- Portable? Yes. Independent of C1. Cheap and clearly correct.

### C5 (review #13): forward the caller's ISR context
- H417 does: `dcd_event_xfer_complete(..., false)` hardcoded in the early-status path, and
  the USB3 dispatcher drops `is_isr` before reaching the USB2 back end.
- Change: thread `is_isr` through, as `dcd_ch32h417_usb30.c` already does.
- Portable? Yes, but moot if C1 removes the early-status path entirely.

### Cross-check against spec Appendix A

None of C1-C5 duplicates a refuted row. Appendix A rules out gating SETUP detection on
`!valid` or on `EP_RX_LEN(0) == 8`, making `arm_ep0_out()` preserve DONE, dropping the
forced DONE write-back, `USBHS_UDIE_SOF_ACT`, re-latching `UEP0_DMA`/`UEP0_MAX_LEN`, and
clearing `RB_UEP_x_NAK_ACT`. C1 *removes* the construct that two of those rows tried to
patch, which is a different move from patching it again.

Review findings 5, 6, 7, 8 and 11 are USB3-driver defects unrelated to the HS SETUP drops
and are out of scope here; finding 15 is a unit-test ordering issue. All are recorded for
a separate pass.

### Phase A results (2026-08-14)

Reference baseline: **34.5%** (316 SETUPs). All measurements pooled over 5 fresh-flash runs.

| Candidate                          | Pooled drop rate | usbtest (3 fresh flashes) | Kept |
| ---------------------------------- | ---------------- | ------------------------- | ---- |
| baseline                           | 34.5% (316)      | 25/30, 24/30, 24/30       | --   |
| C1 remove status pre-arm           | 32.8% (308)      | 25/30 x3                  | yes  |
| C3+C4 correctness fixes            | 37.7% (329)      | 25/30 x3                  | yes  |

**No candidate moved the drop rate.** 32.8 / 34.5 / 37.7% all sit inside the per-run band
(25.0-46.5% observed across the three configurations), so none of them is the mechanism.

C1 was kept anyway: it is a *removal* that matches the oracle, is neutral on the metric,
makes usbtest more consistent (case 29 stops flapping), and deletes five review findings
whose only cause was the pre-arm. C3+C4 were kept as correctness fixes, explicitly not as
drop-rate candidates.

**C2 (SETUP-branch toggle to DATA0) was not implemented: it is provably inert.**
`queue_in_packet` rewrites `UEP0_TX_CTRL`'s toggle from `ep0_tog` on every arm, so the
value written at SETUP time never survives to affect a transaction. Testing it would spend
rig time to confirm a no-op that can be read off the code.

**Phase A verdict: exhausted without reaching 30/30.** Every portable difference between
the oracle and the subject has been tried or shown inert. The one structural difference
that could plausibly explain a SIE-level SETUP drop -- CH569's dedicated `RB_USB_IE_SETUPACT`
interrupt versus H417 inferring SETUP from a sticky flag inside the TRANSFER branch -- is
**not portable**: the H417 silicon has no SETUP interrupt to enable.

Proceed to Phase B (Task 6): the unmodified vendor SDK demo as the silicon-vs-driver oracle.

## Phase B — ABORTED: the vendor demo bricked the board (2026-08-14)

The unmodified `SimulateCDC-HID` demo built cleanly out-of-tree (53 KB text, `.highcode`
at 0x20080000, nothing under `hw/mcu/` edited). Because the demo has no reflash backdoor
of its own, it was flashed with a shim (`/tmp/vendor-oracle/loader_shim.c`) grafting the
BSP's USART1 magic matcher and UART flash loader onto it, keeping the vendor USB sources
bit-for-bit unmodified.

**The shim did not work.** The image flashed (57 KB) but never enumerated, produced no
UART output, and did not answer the park magic. The board hung before the USART1 RX
interrupt could be serviced.

Most likely cause: the vendor's `Hardware()` and `ch32h417_usbhs_device.c` call `printf()`,
and the shim deliberately skipped the vendor's `USART_Printf_Init(2000000)` to keep USART1
at 115200 for the loader. A blocking `printf` on an unconfigured port hangs before USB init.

Recovery required physically unplugging the Type-A cable (SDI is unusable while a host's
termination sits on PB8/PB9, `wlink` returns protocol error 0x55), then
`wlink -d 1 erase --method power-off --chip CH32H41X` and a reflash. The nano was restored
and the UART loader verified working again.

**Do it this way next time:** flash a shim that runs ONLY the loader window and returns --
prove the escape hatch round-trips -- and add `Hardware()` only after that. Also call the
vendor's `USART_Printf_Init()` and instead teach `wch_uart_flash.py` the vendor's baud, so
the vendor's own init sequence is left intact.

**Also learned:** `wlink` has no default probe and will attach to whichever WCH-Link it
finds first. Always pass `-d <index>` (resolve with `wlink list`): probe #0 is the hydra
(D3008F0657DB, CH56X), probe #1 is the nano (E8E68F066EEE, CH32H41X). An erase issued
without `-d` targeted the hydra's probe; only the chip-type guard stopped it, and the hydra
was re-verified afterwards at 12/12 + usbtest 30/30.

**Status: silicon-vs-driver remains undecided.** Phase A is exhausted and Phase B is unrun.

## Task 7 — verification (2026-08-14)

| Check                                   | Result                                            |
| --------------------------------------- | -------------------------------------------------- |
| usbtest, 3 fresh flashes                | 25/30 each, failing 9/10/13/14/21 (acceptance NOT met) |
| HIL device suite, high speed            | 11/12, CDC 5.8/6.5 MB/s, MSC 25.4/10.3 MB/s        |
| CH569 HS (oracle, unaffected)           | 12/12 + usbtest 30/30, verified twice today        |
| ceedling unit tests                     | 72/72                                              |
| pre-commit, all files                   | clean                                              |
| SuperSpeed regression                   | **BLOCKED** -- see below                           |

**SuperSpeed could not be regression-tested and this is cabling, not code.** The run reported
0/12, but the device enumerated normally on bus 5 (USB2) as `5-2` while bus 6 held nothing and
all four SS ports read `Rx.Detect`. The usb_sniffer tap now sits between the nano and the
uPD720201; it passes D+/D- through but not the SuperSpeed pairs, so SS cannot train through it.
Independently, the HS driver contributes 0 symbols to the `SPEED=super` build (verified with
`nm`), so nothing changed in this campaign can reach the SuperSpeed path.

To clear it: remove the sniffer from the cable path and re-run
`python3 test/hil/hil_test.py -B examples -b nanoch32h417 test/hil/local.json`,
expecting 12/12 with usbtest 29/29.

## Phase B retry (2026-08-14) — oracle still unrun, but recovery is now free

**The IWDG guard works and removes the bricking cost.** Arming an ~8 s IWDG kicked from a 1 ms
SysTick0, immediately before calling the vendor `Hardware()`, means a wedge with dead interrupts
resets the board straight back into the unconditional loader window. Verified: the hanging vendor
image was flashed, hung, self-reset, and was reflashed with no physical access. Any further Phase
B attempt is now safe to iterate on. The earlier boot-window-only design failed because nothing
can *trigger* a boot once the firmware is wedged.

**Step 1 passed, step 2 still hangs.** A loader-only shim (no `Hardware()` call) booted and
reflashed itself, so the scaffolding -- linker script, startup, system file, USART1, loader -- is
sound. Adding the single `Hardware()` call kills it. The fault is in the vendor USB init, and the
unbounded `while (!(RCC->CTLR & RCC_USBHS_PLLRDY));` at ch32h417_usbhs_device.c:178 remains the
prime suspect; our driver bounds the same wait with a timeout.

### C6: disable the SWJ pin function before USB init — NO EFFECT, and the test was invalid

Reading the vendor `Hardware()` produced a candidate Phase A's four EP0 axes could not have
found, because it is pin-mux rather than registers:

```c
GPIO_PinRemapConfig(GPIO_Remap_SWJ_Disable, ENABLE);   // vendor, before USBHS_Device_Init
```

Measured: **35.6%** (120 of 337 SETUPs) against the 34.5% baseline -- no effect, usbtest still
25/30. Reverted, since it costs in-system SWD for no benefit.

**But the experiment did not test the hypothesis.** `SWJ_CFG` does not appear anywhere in
CH32H417RM-EN; debug on this part is SDI two-wire on PB8/PB9, while `GPIO_Remap_SWJ_Disable`
(0x00300400) is the legacy AFIO bit covering PA13/PA14/PA15/PB3/PB4. The write was almost
certainly a no-op for the pins that matter, which is exactly consistent with the flat result. So
"the debug function on PB8/PB9 interferes with USB2" is **untested, not refuted** -- and earlier
project work already concluded there is no software off-switch for SDI on this part (no option
byte, no DBGMCU pin control), so there may be no way to test it at all from firmware.

## Campaign closed (2026-08-14)

Phase B produced **no measurement** across three attempts. Each one found a real defect in the
harness rather than in the device under test:

1. Vendor `Hardware()` wedges — the loader-only shim boots and reflashes itself, so the
   scaffolding is sound. Unbounded `while (!(RCC->CTLR & RCC_USBHS_PLLRDY));` at
   `ch32h417_usbhs_device.c:178` is still the prime suspect (our driver bounds the same wait).
2. The boot-window safety net was unreachable — nothing can trigger a boot once wedged. Fixed by
   the IWDG guard, which was then **verified working**.
3. The IWDG was armed *after* the first progress marker, so a fault in that marker left it
   unarmed. Fixed by arming it first, with SysTick kicking. Never re-tested.

Also established: `printf` is unusable in the out-of-tree vendor build (newlib wants a heap this
link never provides), which retroactively explains the silence in all three attempts -- including
the two originally attributed to `Hardware()`. Progress output must use `loader_putc()`.

**Final state: usbtest 25/30, HIL 11/12, silicon-vs-driver undecided.** The CH569 oracle
(12/12 + usbtest 30/30 under identical usbd, host and rig) remains the strongest result: the
defect is H417-specific and usbd's deferred model is exonerated. Every portable difference from
that oracle has been measured and none moves the drop rate.

The shim is at `/tmp/vendor-oracle/` (outside the repo) with the watchdog ordering fixed and
markers working, should anyone resume. Note the nano was left bricked by attempt 3 and needs one
Type-A unplug plus `wlink -d 1 erase --method power-off --chip CH32H41X` to restore.

## Manual re-read: interrupt-flag handling (2026-08-14)

CH32H417RM-EN chapter 25 has only a feature list (25.1) and register tables (25.2) -- no
transfer-flow narrative -- so the re-read was done as register coverage. Registers our driver
never touches all sit at reset defaults that match the vendor's usage: `UEP_TX/RX_TOG_AUTO`
(manual toggling, as we do), `UEP_TX/RX_BURST`, `UEP_RX_RES_MODE` (ACK, not NYET), `UEP_AF_MODE`,
`TEST_MODE`, `LPM_DATA`, `UEPn_RX_SIZE`, `UEPn_RX/TX_FIFO`. The vendor writes none of them either.

**One real divergence found: interrupt-flag retirement.** The vendor ISR has *no* `RX_SOF` branch;
SOF falls through to a catch-all that clears the whole `INT_FG` word. Ours tests `RX_SOF` second
and writes back only that bit, and since the hardware sets `RB_UDIF_RX_SOF` every frame, that
branch short-circuits the chain on nearly every interrupt -- leaving `RB_UDIF_FIFO_OV`, `LPM_ACT`,
`BUS_SLEEP` and `LINK_RDY` latched, and delaying `BUS_RST`/`SUSPEND` by one entry.

Measured, and it does not help:

| Variant                                            | Pooled drop rate      | Verdict            |
| -------------------------------------------------- | --------------------- | ------------------ |
| baseline (SOF branch first, clears only RX_SOF)     | 34.5% (316), 30.6-39.1% | reference        |
| C7c: also clear `RB_UDIF_FIFO_OV`                   | 40.4% (344), 31.7-47.4% | no benefit       |
| C7: full vendor branch order + catch-all            | 43.1% (350), 41.7-45.3% | **measurably worse** |

C7's band (41.7-45.3%) barely overlaps the baseline's, so the vendor's ordering is genuinely worse
here, not noise -- our SOF-first arrangement is the better one. Both reverted; usbtest back to
25/30. The sticky-`FIFO_OV` mechanism is refuted.

**Retraction:** an earlier pass recorded C7/C7b/C7c as "breaking enumeration". That was wrong --
the Type-A had been unplugged for the SDI recovery and never replugged, so the board had no host.
A control run of the known-good build failed identically, which is what exposed it. All three
variants enumerate normally. Always run the known-good control before believing a regression.

## ROOT-CAUSE HYPOTHESIS: the H417 USBHS has no auto-busy, its sibling does (2026-08-14)

The CH569 driver's init writes:

```c
R8_USB_CTRL = UCST_HS | RB_DEV_PU_EN | RB_USB_INT_BUSY | RB_USB_DMA_EN;
```

and WCH's own header defines that bit as (CH56xSFR.h:1489):

```c
#define RB_USB_INT_BUSY  0x08  // enable automatic responding busy for device mode or automatic
                               // pause for host mode during interrupt flag UIF_TRANSFER valid
```

**The CH32H417 USBHS control register has no such bit.** RM 25.2.1.1 lists only RB_UD_LPM_EN (7),
RB_UD_DEV_EN (5), RB_UD_DMA_EN (4), RB_UD_PHY_SUSPENDM (3), RB_UD_CLR_ALL (2), RB_UD_RST_SIE (1),
RB_UD_RST_LINK (0), with bit 6 reserved. The part's *USBFS* block does have `USBFS_UC_INT_BUSY`
(0x08), so the feature exists on the chip -- just not on the high-speed controller.

That is a complete mechanism for everything observed:

- **CH569**: while a transfer completion is unserviced, the SIE *automatically answers busy/NAK*.
  The host retries and the transfer succeeds; a SETUP is never lost. Hence 30/30.
- **CH32H417**: no such fallback. A token arriving while a completion is still pending gets **no
  handshake at all** -- exactly the wire signature measured (device completely silent, no
  malformed packet, host retries three times, -EPROTO).

It also explains the three things that made this so hard to chase:

1. Why **no EP0 register choreography moved the metric**: the drop happens in the window between
   a completion and the ISR clearing DONE, regardless of what software writes afterwards.
2. Why the rate **scales with control-transfer density** (63% under a back-to-back `lsusb -v`
   hammer vs ~34% during usbtest) -- denser traffic means more windows.
3. Why the failing set is exactly the **control-heavy** cases (9, 10, 13, 14, 21) while every
   bulk, interrupt and isochronous case passes: those endpoints tolerate a retry, control SETUPs
   do not.

**Status: strong inference, not a direct measurement.** It rests on WCH's own description of a
bit present on the passing sibling and absent here, plus a symptom that matches exactly. It has
not been confirmed against WCH.

**Consequence if it holds:** software can only *shrink* the window (ISR latency), never close it,
so 30/30 is not reachable on this controller and the honest outcome is a documented quirk in the
same family as the existing SuperSpeed ones. Note the ISR is already near-optimal for this: EP0 is
`ep_num == 0`, the first iteration of the drain loop, so its DONE is cleared almost immediately.

## Direct-plug re-test with the sniffer removed (2026-08-14) — HS does not enumerate at all

With the tap taken out and the nano's Type-A plugged straight into the uPD720201:

| Build            | Result                                                                 |
| ---------------- | ---------------------------------------------------------------------- |
| `SPEED=super`    | **12/12, usbtest 29/29**, CDC 6.3/10.8 MB/s, MSC 191/60.7 MB/s          |
| `SPEED=high`     | **0/12** -- no USB attach whatsoever                                    |

High speed produces no enumeration: `dmesg` shows `device descriptor read/64, error -71`, then
`attempt power cycle`, then `new low-speed USB device` (the host reading the idle SDI levels on
PB8/PB9 as a phantom), then `unable to enumerate USB device`, after which the port stays silent.
The chip is fine -- the HS `board_test` build, which never initialises USB, boots and prints its
1 Hz banner normally, and the UART loader keeps accepting firmware throughout. It is specifically
bringing up USB2 that fails.

**This is the hardware block recorded for this board in July**: PB8/PB9 are simultaneously the
USB2 D+/D- pair and the chip's only SWD/SDI debug pins, and a host's termination and bus-reset
signalling on those pins stops the device attaching.

**Consequence for everything above: every H417 high-speed measurement in this document was taken
with the ataradov USB2 tap in the cable path.** The tap evidently isolates the pair enough for the
controller to come up; removing it, high speed does not work at all on this board. The driver
fixes and quirk 0x40 are still real and were measured on real hardware, but the configuration in
which they can be observed requires that tap (or equivalent buffering) inline. SuperSpeed -- the
board's supported mode, and the default -- is unaffected and healthy either way.
