# Deferred follow-ups — PR #3779 (WCH SuperSpeed, CH569 + CH32H417)

Everything deferred from PR #3779 and no other PR. Its remediation is described in
`docs/superpowers/specs/2026-08-14-review-findings-remediation-design.md`. Nothing here is a
won't-fix; each item is deferred for a stated reason. Delete entries as they land.

## Blocked on the USB2 sniffer tap

The CH32H417 high-speed path does not enumerate at all with the board plugged directly into the
host — PB8/PB9 are simultaneously the USB2 pair and the only SWD/SDI pins, so the tap has to be
inline for any HS measurement. Details in `docs/superpowers/notes/h417-ep0-diff.md`.

| Item                                                                                       | Why it is blocked                                        |
| ------------------------------------------------------------------------------------------ | -------------------------------------------------------- |
| Re-measure the H417 SETUP drop rate with the corrected parser                                | Every figure recorded before the F5 fix is invalid        |
| Re-examine F4 (OUT `DONE` acknowledge ordering)                                               | Attempted and reverted, see below; a real fix needs the tap |
| Confirm the F16 runtime quirk fires on the SS-with-fallback build at 480 Mbps                 | Implemented and built; only observable on the fallback link |

Re-taking the drop rate is the one that matters most: the Phase A campaign compared six candidate
changes against a 34.5 % baseline produced by the miscounting parser, so those comparisons cannot
currently be trusted in either direction.

## Fallback demotion — one half fixed on hardware, one half still open

**FIXED: the CH569's unconditional demotion on `LINK_IF_DISABLE`.** Reproduced with the hydra
powered from its WCH-Link so the cable move could not reset it: unplug a board that had trained
SuperSpeed, replug into the *same* SuperSpeed port, and it came back at 480 Mbps — `5-3 speed=480`
— returning to `6-3 speed=5000` only after a reflash, i.e. `dcd_init`. The branch demoted with no
`_fb_state` or tick-budget test, and `FB_USB2_ACTIVE` has no exit. It now only demotes while still
`FB_USB3_TRAINING`; reaching Disabled from `FB_USB3_UP` is a pulled cable, so it reports the
disconnect and rebuilds the link. Verified: the same move now returns 5000 directly. This also
closes a disagreement with the CH32H417 twin, which always gated the transition on
`fb_state == FB_USB3_TRAINING && ++fb_fail_count >= FB_FAIL_LIMIT`.

**STILL OPEN: F1, replug into a genuinely USB2-only host — now with a hardware reproducer.**
Hydra powered from its WCH-Link so the cable move cannot reset it: unplug from SuperSpeed, plug
into a USB2-only port, and **nothing enumerates** (0 enumerations on any 480 root hub across a
~30 s excursion); plug back into SuperSpeed and it returns at 5000 with CDC and MSC bound. So
`FB_USB3_UP` is terminal for the USB2-only case, but a failed excursion does NOT poison the state
— recovery is intact, which is the half the demotion fix bought. The blocker is unchanged and is a real design
question: the ladder cannot distinguish "USB2-only host attached" from "nothing attached" without a
VBUS signal, and re-arming training on disconnect — tried and reverted — makes the ladder advance
on its timer while unplugged, landing in `FB_USB2_ACTIVE` within ~2.75 s and demoting the board on
*any* unplug. A correct fix needs an attach signal, not a timer.

**STILL OPEN: the CH32H417 twin's terminal `FB_USB3_UP`** (`dcd_ch32h417_usb30.c:360`), same shape
as F1. Mitigated by `CFG_TUD_WCH_USB30_FALLBACK` defaulting to 0 there.

## F9 — sudoers grants: dropped by decision, not a defect

The review flagged that `test/hil/tinyusb-sudoer` grants no `setpci`, which `usbtest.py` needs on
the Renesas uPD720201/2 controllers, and later that a wildcarded `tee` grant in the same file was
an arbitrary root file-write primitive.

Both were dropped from this PR at the maintainer's direction: the rig account is expected to have
passwordless sudo (`NOPASSWD: ALL`), so no grant in this file gates anything there, and the
"escalation" conferred nothing the account did not already hold. The file remains as the base
commit added it.

That leaves the argument-restriction the file's own header advertises weaker than it claims, which
matters only for a rig provisioned *without* blanket sudo. If this template is ever pointed at such
a host, two things need doing: add a `setpci` grant, and replace
`tee /sys/bus/usb/devices/*/driver/unbind` with exact paths — sudoers fnmatch's the joined argv
without `FNM_PATHNAME`, so `*` spans spaces and `tee` writes stdin to every operand it receives.

## F1 — fallback re-arm on disconnect: attempted, reverted, still open

`FB_USB3_UP` is terminal, so a CH569 that trained SuperSpeed once stays dead after a replug into a
USB2-only host. Re-arming the ladder from the partner-gone branch (in `781e58000`, removed in
`519f8ceb8`) fixes that but introduces a worse regression, because the ladder advances on a timer
whether or not anything is attached: unplugged, nothing sets `_fb_saw_terms`, so the 4-tick budget
expires ~2.2 s later, and the 5th tick at ~2.75 s runs `ch56x_usb2_init()` and latches
`FB_USB2_ACTIVE` — itself terminal. Any unplug longer than about three seconds then demotes the
board to 480 Mbps permanently, including the ordinary replug into the same SuperSpeed host, and
this is the shipping default for `hydrausb3_v1`.

Fixing it properly means gating the ladder on an attached partner rather than on a timer — for
example only counting ticks while the far end shows terminations — so the board can distinguish
"training against a host that has no SuperSpeed" from "nothing plugged in at all".

The rest of that work stands: `fallback_enter()` is the only writer of `_fb_state`,
`fallback_timer_start()` re-enables `TMR0_IRQn` (F10), and the deferred re-init re-checks the state
after its settle (F7).

## F4 — OUT completion ordering: attempted, reverted, refuted

Clearing `RB_UEP_R_DONE` after `update_out()` (in `a24cf8569`, reverted in `dcdc457b6`) is wrong,
and the premise behind it was wrong too. `update_out()` → `queue_out_packet()` calls
`set_rx_res(ep_num, USBHS_UEP_R_RES_ACK)`, which deliberately writes `DONE` back as **1**: the bit
is RW0, so writing 1 cannot set it while writing 0 acknowledges a completion. Clearing `DONE`
afterwards performs exactly that write-0 on an endpoint just armed to ACK, so a packet arriving in
the window is swallowed and its transfer hangs — the failure mode the helper's own comment exists
to prevent. The original order clears `DONE` while the endpoint is still in its completed state,
which is not the same hazard, and the endpoint is not ACK-armed at the stale address as F4 assumed.

## F3 — clear-stall re-arm: attempted, refuted by hardware, still open

The review found that both WCH USB3 drivers re-arm the in-flight transfer in
`dcd_edpt_clear_stall()`, while `usbd_edpt_clear_stall()` clears `BUSY` **and** `CLAIMED` on a
was-stalled clear, and `vendord_abort_ep()` is `stall(); clear_stall();` used to abort on every
`SET_INTERFACE`. The reasoning is correct as far as it goes; the fix built on it is not.

Removing the re-arm was implemented and reverted (`2d7fe1bc9`, reverted in `25962b5e7`).
Measured on the CH569 hydra, case 29 run alone on a freshly flashed board so it is not
contaminated by case 13:

| Build                     | usbtest case 29 (clear toggle between bulk writes) |
| ------------------------- | --------------------------------------------------- |
| re-arm present (baseline) | **PASS** 0.26 s                                      |
| re-arm removed            | **FAIL** errno 22, "toggle sync failed, iterations left 63" |

The conclusion that the class always resubmits does not hold for the halt/clear-halt path case 29
exercises: the host clears the halt mid-transfer and expects the endpoint to resume, and the
re-arm is what makes that work.

The conflict the review identified is real and unresolved: a DCD cannot distinguish "clear-halt,
resume the transfer" from "clear-halt as an abort" through the current interface, so whichever it
picks is wrong for the other caller. Fixing it properly means changing that contract — for example
having `usbd_edpt_clear_stall()` tell the DCD which it means, or giving the vendor class a real
abort entry point instead of stall+clear — not patching either driver. Note also that case 13 is
quirk-skipped on this silicon (0x20: a halted endpoint answers exactly one STALL), and forcing it
to run wrecks the endpoint for whatever runs next, so it cannot be used to adjudicate this.

## Deferred fixes with a written plan

Both were confirmed by the 2026-08-14 review and are **not regressions from this branch**, so they
were kept out of #3779 to leave it reviewable as a SuperSpeed PR. Each wants its own branch off
`master`; the full step-by-step is Phase D of
`docs/superpowers/plans/2026-08-14-review-findings-remediation.md`.

**F11 — `src/class/msc/msc_device.c:331` discards the `usbd_defer_func` result.** This branch
changed that function to return `bool` precisely so a dropped deferral is visible, but MSC ignores
it and returns unconditional `true`. `proc_async_io_done` is the only place `pending_io` is cleared
on the async path, so a full event queue leaves no CSW and no next block while the application has
been told the completion was accepted. Fix is `TU_VERIFY(usbd_defer_func(...))`. Not a regression:
it was equally dropped when the return type was `void`. The identical exposure at
`dcd_nrf5x.c:155` and `:190` is listed separately below.

**F15 — `hw/bsp/family_support.cmake:451` appends `-w` instead of assigning it.** `set_property(...
APPEND ...)` lets every family's `BOARD_TARGET` PUBLIC/PRIVATE options reach the board library's own
sources, changing BSP codegen for ~19 unrelated families: `broadcom_32bit/64bit`'s `-O0
-ffreestanding` now beats MinSizeRel's `-Os`, `ch583` gains `-flto -fsigned-char` (char signedness is
ABI-visible between BSP and SDK objects), and several lpc families pass link-only `-nostdlib` as a
compile option. The in-code comment calls the blast radius deliberately accepted, but no non-WCH
family was re-validated. Fix is to restore the overwriting `set_target_properties` and give the WCH
board libraries their `-w` a way that does not leak, validated by building `broadcom_32bit` and
`ch583` and diffing the generated ninja `FLAGS` line against master.

## Out of scope for PR #3779

| Item                                                                                          | Note                                                             |
| --------------------------------------------------------------------------------------------- | ---------------------------------------------------------------- |
| `dcd_nrf5x.c:155` and `:190` drop the `usbd_defer_func` result                                  | Same defect as F11; the dropped defer is the only thing that would write the EasyDMA START task |
| CH569 and CH32H417 fallback ladders have drifted apart                                          | Six `_fb_state` writes vs four; the H417 cannot self-recover once `FB_USB2_ACTIVE`. A shared table-driven FSM was considered and rejected as YAGNI |
| ~250 lines of rig-only UART-bootloader / IWDG machinery in `hw/bsp/ch32h417/family.c`           | Defaults ON for every user of that BSP; should be opt-in          |
| `LINK_IF_WARM_RESET` still runs its ~30 ms settle and re-init inside the ISR                    | F14 half-fixed. The missing `ep_state_reset()` / `dcd_event_bus_reset()` pair landed, but the work cannot simply be deferred: `usb30_bus_reset_from_isr()` deinits synchronously and only defers settle+init, so moving it would leave the address-0 write and the `TX_WARM_RST` handshake driving a torn-down controller, with the deferred re-init then wiping them. Doing this properly means reordering the handshake ahead of the teardown, which needs a host that actually issues a warm reset to validate |

## RESOLVED — the nano's "harness-only" usbtest failures were stale firmware

`hil_test.py` resolves firmware from `<repo>/<build-dir>/cmake-build-<board>/<example>/` and
defaults `--build-dir` to `cmake-build`, the layout `tools/build.py` produces. Building the way
CLAUDE.md documents puts binaries in `examples/cmake-build-<board>/` instead, and the runs in this
session omitted `-B examples` — which the hil skill already tells you to pass. A `cmake-build/`
tree left over from 17 July then won every lookup, so every `hil_test` run flashed month-old
firmware on both boards.

For the CH32H417 that binary predates the SuperSpeed iso max-packet fix (`USBTEST_ISO_EP_MPS_SS`
= 1024), so its iso endpoints were 512 while the host drove 1024-byte packets — exactly the
`512 errors out of 1024` every-other-packet signature — plus the ctrl_out and `msc_dual_lun`
failures. With `-B examples` both boards are clean:

| Board          | usbtest              | result |
| -------------- | -------------------- | ------ |
| nanoch32h417   | 29/30 (1 skipped)    | 12/12  |
| hydrausb3_v1   | 27/30 (3 skipped)    | 12/12  |

`hil_flash.find_firmware()` now warns on stderr when a newer build of the same example exists in
a root it did not pick, naming both paths and the age gap, so this fails audibly next time.

**Every HIL result reported earlier in this session came from that stale tree and should be
disregarded** — the driver work was never actually exercised on hardware until this run.

## Open from the 2026-08-17 review

Validated, not fixed, with the reason:

| Finding | Why it is still open |
| ------- | --------------------- |
| `test/unit-test/test/support/tusb_config.h:86` — the suite default is now a SuperSpeed build (EP0=512), leaving only `test_usbd_fshs.c` (2 tests) on the FS/HS shape | Real coverage regression, demonstrated live: mutating the FS/HS stub `usbd_ss_ep_companion_len()` to return 999 still leaves `ceedling test:all` at 76/76, even though printer, usbtmc, ecm_rndis and ncm call it ungated. Fixing means running the usbd suite twice under both shapes, which is a project.yml restructure rather than a patch |
| `dcd_ch56x_usbhs.c:457` — EP0 `RX_CTRL` read-modify-written from task context while the ISR writes the same register from four places | Confirmed that `usbd_edpt_xfer()` calls the dcd with interrupts enabled, so the race is real. The CH32H417 twin avoids it by keeping the toggle in software and doing one full-byte store; porting that shape to the CH56x driver is the right fix and wants hardware to sign off |
| `bth_device.c:192, :220` — the isochronous alternate-setting walk is not SuperSpeed-companion aware | The file already documents this as deferred. Fixing it properly also means correcting `iso_alt_itf_size`, and no SS BTH template or SS-isochronous DCD exists to test against |
| `examples/device/net_lwip_webserver/src/lwipopts.h:72` — inbound IP/UDP/TCP checksum verification disabled for the CH569 SuperSpeed build | The stated reasoning (USB CRC32 covers the payload) does not cover NCM/NTB reassembly or the RAMX bounce copies between the wire and lwIP, and the compile-time gate keeps verification off after the runtime USB2 fallback, where the link CRC is CRC-16. A judgement call for the maintainer, not a defect to patch silently |
| `usbd_pvt.h:113` — `usbd_open_edpt_pair()` gained a `desc_end` parameter, a source-breaking change for out-of-tree class drivers | Deliberate (the bound is what makes the descriptor walk safe), but it needs a changelog entry the way the `usbd_edpt_xfer` `is_isr` change got one in 0.21.0 |
| `hfp.json:35` — `device/usbtest` newly skipped on lpcxpresso43s67, the fleet's only ip3511-HS board | The skip's own comment concedes it may mask a regression in the shared EP0/control code this branch changes. Needs a master baseline from that rig to settle, which is external hardware |
| Three copies of the WCH USBHS transfer engine (`dcd_ch32_usbhs.c`, and the two new ones) | Already cost a fix: the clear-halt bulk-IN re-arm exists in both new copies and never reached the original, so CH32V307/CH32F20x still NAK after a clear-halt. Sharing them is a refactor of a shipping driver, not part of this PR |

## Review runners-up

Non-refuted findings from the 2026-08-14 maximum-effort review that fell outside the top 15.
Full text in `review_findings_max_2026-08-14.json`.

| Location                                     | Issue                                                                                  |
| -------------------------------------------- | --------------------------------------------------------------------------------------- |
| `dcd_ch32h417_usb30.c:27`                    | Stale "compile-verified; full hardware bring-up pending" note, contradicted by ten hardware-derived findings in the same file |
| `test_usbd_set_sel`, `test_usbd_set_isoch_delay` | Pass only because an earlier test leaves the speed at SUPER; proven to fail in isolation |
| Both USBHS drivers                            | Iso-OUT endpoints left ACK-armed with a stale DMA address after completion             |
| CH32H417 USBHS                                | `ep0_tx_seq` read-modify-write raced by `handle_setup()`                                |
| `hfp.json`                                   | The `lpcxpresso43s67` usbtest skip, whose own comment admits it may be masking a regression rather than a silicon quirk |
| `lwipopts.h`                                 | Disables inbound IP/TCP/UDP checksum verification — the only such place in the repo, and the fallback default runs that same binary at USB2 HS |

Fixed since this list was written: the non-volatile CH32H417 driver state, `can_recover()`'s stderr
matching, `ch32h417_usb2_deinit()` leaving `INT_EN`/`INT_FG` set, the stale `LINK_INT_FLAG` on the
inline CH569 re-inits, `.DMADATA`'s silent initialiser loss, and audio's `TUSB_SPEED_HIGH` tests.
