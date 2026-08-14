# Remediating the 16 review findings on the WCH SuperSpeed branch — design

**Status:** approved 2026-08-14. Supersedes nothing; complements
`2026-08-14-usbtest-30-30-h417-hs-design.md`, whose outcome produced several of these findings.

## Goal

Resolve all 16 findings from the maximum-effort review of `claude/wch-ch569-ch32h417-usb3`
(recorded verbatim in `review_findings_max_2026-08-14.json`), so that PR #3779 ships without
known defects and — more importantly — without test instrumentation that reports success it has
not earned.

Every finding was independently re-validated against the code before this design was written.
All 16 accurately describe what is there.

## Why the review's ranking is not this document's ranking

The review ranked by bug severity. For this branch the more useful axis is what each finding
costs, which reorders them substantially. Three of the five cheapest findings are the very
instruments used to judge whether the expensive ones are fixed: a usbtest run that renders five
skipped cases as a green full pass, a drop-rate parser that miscounts, and a quirk that never
fires in the configuration it was written for. Those are fixed first because everything else is
measured with them.

| Class | Findings                  | Character                                                     |
| ----- | ------------------------- | ------------------------------------------------------------- |
| A     | F2, F5, F9, F13, F16      | Instrumentation reports unearned success; all new in branch    |
| B     | F1, F7, F10, F14          | One state machine, four defects; shipping default for hydra    |
| C     | F3, F4, F6, F8, F12       | Correctness newly exposed by SuperSpeed                        |
| D     | F11, F15                  | Not regressions from this branch                               |

## Scope decisions

- **All 16 are in scope.** Nothing is written off as won't-fix.
- **Packaging:** classes A, B and C land in PR #3779. Class D goes to two separate PRs off
  `master`, because neither is a regression from this branch and both reach far outside it —
  F11 also affects `dcd_nrf5x.c`, F15 changes codegen for ~19 non-WCH families. Keeping them out
  leaves #3779 reviewable as a SuperSpeed PR.
- **Validation:** unit tests and builds for everything; hardware sign-off on the CH569 hydra,
  including the replug test that is the only real proof of F1. The USB2 sniffer tap stays
  unplugged, which bounds what can be claimed — see *Limits* below.

## Approach

Point fixes throughout, except for class B, which gets a small state-transition helper.

Class B's four defects are one root cause wearing four hats: `_fb_state`, the TMR0 interrupt
mask, and the timer registers are three pieces of a single logical state, mutated independently
across six sites in `dcd_ch56x_usb30.c`. Patching them individually leaves the fifth instance
waiting for whoever edits the ladder next. A helper that owns all three makes two of the four
findings unreachable rather than merely fixed, for roughly thirty lines more than the point-fix
alternative.

A full table-driven rewrite shared between the CH569 and CH32H417 ladders was considered and
rejected as YAGNI. The twins have already drifted — `dcd_ch56x_usb30.c` has six `_fb_state`
writes to the CH32H417's four, and the H417 cannot self-recover once `FB_USB2_ACTIVE` — but that
is a redesign wearing a bugfix costume, and it would need both boards to sign off. The drift is
recorded as a follow-up instead.

## Section 1 — Class A: make the instruments honest

**F2 — a quirk-skipped run must not look like a full pass.** `test/hil/usbtest.py` already emits
a `skipped` count; `test/hil/hil_test.py:1338` never reads it, computing `total = passed + failed`
so that a CH32H417 high-speed run with five quirked cases renders `✅ 25/25`, indistinguishable
from a genuine 30/30. Read `skipped`, compute `total = passed + failed + skipped`, and render a
distinct cell (`⚠️ 25/30 (5 skipped)`). Change the failing-case list at `:1346` from
`status != 'PASS'` to `status not in ('PASS', 'SKIP')`, which today reports the five skipped cases
as failures the moment one real case breaks. Exit status stays 0 — a documented quirk is not a
test failure — it simply stops looking like a clean sweep.

**F5 — the drop-rate parser miscounts.** `test/hil/hs_drop_rate.py:37` accepts a device ACK only
when `usbll.src` starts with `'1.'`, hardcoding wire address 1, while `one_run()` deliberately
captures across enumeration where the device answers at address 0, and xHCI hosts routinely
assign an address other than 1. Test `r[2] != 'host'` instead. Add unit rows at addresses 0, 5
and 12 — the existing five tests use `'1.0'` throughout and structurally cannot catch this. Record
in the docstring that any drop rate measured before this fix is invalid.

**F16 — the quirk cannot see the fallback.** `USBTEST_QUIRKS` is selected by which dcd was
compiled in: an SS build has `CFG_TUD_WCH_USBIP_USBHS == 0` and so takes the `0x20` branch, even
though that same image drives the USBHS block at 480 Mbps when `CFG_TUD_WCH_USB30_FALLBACK=1`.
Have `tud_descriptor_device_cb` patch the `bcdDevice` quirk nibble from `tud_speed_get()` at call
time: CH32H417 reports `0x40` at high speed and `0x20` at SuperSpeed, CH569 `0x30` at SuperSpeed
and none at high speed. The compile-time macro remains the default for non-fallback builds.

**F13 — an interrupted flash strands the board.** `hw/bsp/ch32h417/family.c:161` is `noreturn` and
its first act is to disable the RXNE interrupt and `USART1_IRQn` — the only other way in or out.
Its single exit is `NVIC_SystemReset()` after a CRC-verified session, so an interrupted
`wch_uart_flash.py` leaves the board in the banner loop, off the USB bus, while
`hil_flash.py:246` reports `returncode=0` because the park *write* succeeded. Give the loop an
inactivity timeout: no valid session within ~30 s and it resets into the application. That makes
stranding structurally impossible rather than depending on the rig to rescue it, which is the
part that failed. `reset_wch_uart_loader()` keeps its park window but stops equating a successful
write with a successful reset.

**F9 — the sudoers drop-in is missing a grant its own tooling needs.** `test/hil/tinyusb-sudoer`
grants eight binaries, none of them `setpci`; `usbtest.py:228` needs it and hard-exits at `:235`
on any Renesas uPD720201/2 xHCI — the controllers the rig uses. Add argument-restricted grants for
`setpci -s <slot> 0x6c.l` and for `lsusb`, and add `-n` to `hs_drop_rate.py`'s lone `sudo`, the
only call in `test/hil` without it.

## Section 2 — Class B: one helper, four bugs

Introduce `fallback_enter(fb_state_t)` in `dcd_ch56x_usb30.c` as the only writer of `_fb_state`,
owning the timer and the TMR0 interrupt mask with it:

- `FB_USB3_TRAINING` — set state, start the timer, **enable `TMR0_IRQn`**
- `FB_USB3_UP` — set state, stop the timer
- `FB_USB2_ACTIVE` — set state, stop the timer
- `FB_USB3_OFF` — set state; the timer keeps running for the second expiry

Convert the six existing mutation sites to call it.

**F10 is then unreachable.** `fallback_timer_stop()` masks `TMR0_IRQn` and `fallback_timer_start()`
never re-enables it; the only `PFIC_EnableIRQ(TMR0_IRQn)` in the file is in `dcd_int_enable()`.
After a `tud_disconnect()` / `tud_connect()` cycle the timer therefore counts with its vector
masked. The helper makes arming a timer whose vector is masked impossible to express.

**F1 becomes one line.** `_fb_state = FB_USB3_UP` at `:553` is terminal: the TERM_PRESENT-lost
branch at `:600-614` tears the link down and returns without touching the state, and the only
writes back to `FB_USB3_TRAINING` are in `dcd_init` and `dcd_connect`, neither of which usbd calls
on link loss. Replugged into a USB2-only host the board is dead until a power cycle — and this is
the shipping default for `hydrausb3_v1`. The disconnect branch calls
`fallback_enter(FB_USB3_TRAINING)` before its early return.

**F7 needs an explicit fix**, as no helper covers it. `usb30_hw_reinit_task()` checks
`_fb_state` *before* `link_delay_us(30000)` and never re-checks, while the training-exhausted
branch that sets `FB_USB3_OFF` does not consult `_hw_reinit_deferred` and can land anywhere in
that window. Re-check `_fb_state` after the settle and bail before `usb30_hw_init()` if the ladder
claimed the port meanwhile.

**F14 overrules a comment written in this branch.** The `LINK_IF_WARM_RESET` branch runs
`usb30_bus_reset()` — deinit, a 30 ms busy-wait, re-init — plus a further million-iteration poll,
entirely in the ISR, defended by a comment arguing that a host-driven reset with a spec'd ~100 ms
window must not depend on application loop timing. But the same file already defers exactly this
work on the TERM_PRESENT path, and 100 ms comfortably covers one task-loop turn. Move the settle
and re-init to `usbd_defer_func`, and add the `ep_state_reset()` / `dcd_event_bus_reset()` that
the HOT_RESET branch performs and this one omits. That missing bus-reset event is a bug
independent of where the work runs. If the hydra shows a regression, revert to inline and keep
only the bus-reset fix.

## Section 3 — Class C: correctness

**F3 — `dcd_edpt_clear_stall()` must not resurrect a retired transfer.** It re-arms on the stated
assumption that the class driver still considers the transfer submitted, but `usbd.c:1846-1850`
clears `BUSY` *and* `CLAIMED` on a was-stalled clear, and `vendord_abort_ep()` is literally
`stall(); clear_stall();` used to abort on every `SET_INTERFACE`. Drop the re-arm; the class
resubmits, which `examples/device/usbtest` does on its next tick. Same fix in the CH32H417 twin,
where `dcd_edpt_stall` likewise never clears `xfer->valid`. The hydra adjudicates this via usbtest
cases 13 and 29 — the cases the original re-arm was added for.

**F4 — the OUT completion drops its only interlock too early.** `dcd_ch32h417_usbhs.c:553` clears
`RB_UEP_R_DONE` before `EP_RX_LEN` is sampled and before `queue_out_packet()` reprograms the DMA
address, leaving the endpoint ACK-armed at the finished packet's address. The CH56x twin is immune
by hardware — it sets `RB_USB_INT_BUSY` and clears `R8_USB_INT_FG` only after `update_out()` has
re-armed — and CH32H417 RM 25.2.1.1 lists no equivalent bit. Clear `DONE` after `update_out()`
returns, mirroring where the CH56x driver clears its flag.

**F6 — MTP's ZLP threshold assumes 512.** `mtp_device.c:426` falls back to a hardcoded 512 because
`ep_sz_fs` is only written when `tud_speed_get() == TUSB_SPEED_FULL`, while `TUD_MTP_SS_DESCRIPTOR`
arms 1024. A data phase ending in exactly 512 bytes then queues a ZLP the host reads where the
response container belongs. Store the real bulk max packet size at open for every speed and use it
as the threshold.

**F8 — `midi2_device` stalls a mandatory request.** This branch gives it a high-speed
configuration (master had none) without `tud_descriptor_device_qualifier_cb` or
`tud_descriptor_other_speed_configuration_cb`, so `usbd.c` stalls EP0 on
`GET_DESCRIPTOR(DEVICE_QUALIFIER)` — which USB 2.0 §9.6.2 forbids for a high-speed-capable device.
Add both callbacks, following every other example that carries an HS config.

**F12 — the new per-function remote-wake state is write-only.** `func_wakeup_bm` is written at
`usbd.c:1305/1307` and read only by the aggregate loop at `:1312`; interface `GET_STATUS` at
`:1288` still returns a hardcoded `0x0000`, so a wake-capable function reports itself
non-capable, contrary to USB 3.2 §9.4.5. Return D0/D1 at SuperSpeed, keeping the two reserved zero
bytes at USB 2.0 speeds. Separately, the state survives `SET_INTERFACE`, a same-value
`SET_CONFIGURATION` (which Linux's `usb_reset_configuration()` sends) and `SET_ADDRESS(0)`, all of
which USB 3.2 Table 9-10 marks as resetting FUNCTION REMOTE WAKEUP; clear it on each.

## Section 4 — Class D: two separate PRs off master

**F11 — MSC discards a result the branch made meaningful.** `usbd_defer_func()` now returns
`bool` precisely so a dropped deferral is visible, but `msc_device.c:331` ignores it and returns
unconditional `true`. `proc_async_io_done` is the only place `pending_io` is cleared on the async
path, so a full event queue hangs the transfer forever while the application is told the
completion was accepted. Propagate the result. Not a regression — it was equally dropped when the
return was `void` — hence its own PR. The identical exposure at `dcd_nrf5x.c:155` and `:190` is
noted as follow-up, not fixed here.

**F15 — the `-w` change leaks family flags into unrelated BSPs.** `family_support.cmake:451`
changed from a property assignment to `APPEND`, so every family's `BOARD_TARGET` compile options
now reach the BSP library's own sources. The in-code comment calls the blast radius deliberately
accepted, but no non-WCH family was re-validated. Revert to the overwriting
`set_target_properties` and give the WCH board library its `-w` a way that does not leak. Validate
by building two families the APPEND provably changed: `broadcom_32bit`, where `-O0` now beats
MinSizeRel's `-Os`, and `ch583`, where `-fsigned-char` is ABI-visible between BSP and SDK objects.

## Verification

- **Unit:** `ceedling test:all`; new tests for the drop-rate parser (addresses 0, 1, 5, 12) and
  the usbtest cell rendering with a non-zero `skipped`; usbd tests for the `func_wakeup_bm` resets
  and SuperSpeed interface `GET_STATUS`.
- **Build:** `hydrausb3_v1` and `nanoch32h417` at both speeds; `stm32f407disco` as the
  unrelated-port regression check; `broadcom_32bit` and `ch583` for F15 on its own branch.
- **Format/lint:** `pre-commit run --all-files`.
- **Hardware (CH569 hydra):** 12/12 HIL examples; usbtest 30/30 at SuperSpeed; cases 13 and 29
  specifically for F3; and SuperSpeed trained, then replugged into a USB2-only port, for F1.

## Limits

Two fixes cannot be measured in this round and must not be described as validated:

- **F4** is in the CH32H417 USBHS driver, and that path does not enumerate at all without the
  USB2 tap inline (`docs/superpowers/notes/h417-ep0-diff.md`).
- **F16**'s runtime quirk only fires on the fallback link, which needs the same tap.

Both ship built-and-reasoned. Re-measuring them, and re-taking the H417 drop-rate numbers with
the corrected parser, are recorded in `docs/superpowers/todo/2026-08-14-deferred-followups.md`.
