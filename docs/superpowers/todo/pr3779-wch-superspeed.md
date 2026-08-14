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
| Confirm F4 (OUT `DONE` now cleared after `update_out()`) on hardware                          | Implemented and built; never executed                     |
| Confirm the F16 runtime quirk fires on the SS-with-fallback build at 480 Mbps                 | Implemented and built; only observable on the fallback link |

Re-taking the drop rate is the one that matters most: the Phase A campaign compared six candidate
changes against a 34.5 % baseline produced by the miscounting parser, so those comparisons cannot
currently be trusted in either direction.

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

## Review runners-up

Non-refuted findings from the 2026-08-14 maximum-effort review that fell outside the top 15.
Full text in `review_findings_max_2026-08-14.json`.

| Location                                     | Issue                                                                                  |
| -------------------------------------------- | --------------------------------------------------------------------------------------- |
| `dcd_ch32h417_usb30.c:27`                    | Stale "compile-verified; full hardware bring-up pending" note, contradicted by ten hardware-derived findings in the same file |
| `test_usbd_set_sel`, `test_usbd_set_isoch_delay` | Pass only because an earlier test leaves the speed at SUPER; proven to fail in isolation |
| Both USBHS drivers                            | Iso-OUT endpoints left ACK-armed with a stale DMA address after completion             |
| `dcd_ch32h417_usb30.c`                       | `fb_state`, `pending_addr`, `tim12_clocked` are non-volatile; the CH569 twin marks all three volatile |
| CH32H417 USBHS                                | `ep0_tx_seq` read-modify-write raced by `handle_setup()`                                |
| `hfp.json`                                   | The `lpcxpresso43s67` usbtest skip, whose own comment admits it may be masking a regression rather than a silicon quirk |
| `lwipopts.h`                                 | Disables inbound IP/TCP/UDP checksum verification — the only such place in the repo, and the fallback default runs that same binary at USB2 HS |
| `can_recover()`                              | Decides sudo policy by matching a stderr prefix                                          |
