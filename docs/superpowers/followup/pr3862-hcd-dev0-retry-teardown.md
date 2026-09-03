# HCD drivers whose dev0 teardown is a no-op wedge the new enum retry

> Split out of PR #3862 (`claude/usbh-enum-timeout`, control transfer watchdog +
> enumeration retry): a driver-level gap the retry makes reachable, not a bug in
> the retry logic itself, and not fixable without HIL access to the affected
> host controllers.

**Goal:** on every HCD, a control-transfer watchdog timeout followed by an
enumeration retry must leave dev0's EP0 hardware state ready to accept a new
SETUP — not permanently busy with the transfer that just timed out.

---

## What is already established

### The retry's safety argument assumes dev0 gets closed at HCD level

`enum_full_complete()` (`src/host/usbh.c:2296-2321`) calls
`usbh_device_close(_usbh_data.dev0_bus.rhport, 0)` on failure, then — under
`CFG_TUH_ENUM_ATTEMPT_MAX > 1` (default 3) — re-arms the same debounce/reset
ladder (`ENUM_AFTER_DEBOUNCING_DELAY`) to retry as dev0. The watchdog's own
comment (`usbh.c:2260-2262`) leans on this: "a completion the hcd still
delivers for the expired transfer is dropped ... unless the same device
already has a new transfer in flight, and **enumeration always closes the
device at hcd level before retrying**."

### Two HCDs make that close a no-op for dev_addr 0, one of them wedges

- `src/portable/raspberrypi/rp2040/hcd_rp2040.c:463-466` `hcd_device_close()`:
  `if (dev_addr == 0) { return; }` — skips the loop at `:472-490` that would
  otherwise reset a stuck endpoint's `ep->state` back to `EPSTATE_IDLE`
  (`:476`).
- `src/portable/ehci/ehci.c:272-276` `hcd_device_close()`: same early return
  for `daddr == 0`.
- rp2040 additionally has no working abort or reset to fall back on:
  `hcd_edpt_abort_xfer()` (`hcd_rp2040.c:583-589`) is `// TODO not implemented
  yet`, always returns `false`; `hcd_port_reset()` (`:437-440`) is `// TODO:
  Nothing to do here yet`, a pure no-op — so nothing in the retry path ever
  touches the shared `epx` endpoint's hardware state.

### Traced failure mechanism on rp2040 native host

1. A control transfer NAKs forever; the watchdog fires, calls the no-op
   `hcd_edpt_abort_xfer()`, then `control_xfer_complete(daddr, FAILED)`. The
   SIE hardware is still mid-transaction: `epx->state` stays `EPSTATE_ACTIVE`
   (`hcd_rp2040.c:176`, set by `epx_switch_ep()`) because nothing ever clears
   it — `EPSTATE_ACTIVE` is only cleared by `xfer_complete_isr()`
   (`:211-219`), which never runs since the transfer never completes.
2. `enum_full_complete(false)` retries: `usbh_device_close(rhport, 0)` is the
   no-op above, `hcd_port_reset()` is the no-op above.
3. The retry ladder reaches `usbh_edpt_control_open(0, 8)` →
   `hcd_edpt_open(dev_addr=0)`, which reuses `ep_pool[0]` without touching
   `state` (`:518-535`), then issues the new SETUP via `hcd_setup_send()`
   (`:660-670`): `if (epx->state == EPSTATE_ACTIVE) { ep->state =
   EPSTATE_PENDING_SETUP; ... }` — parked, not sent.
4. Promotion out of `EPSTATE_PENDING_SETUP` only happens inside
   `xfer_complete_isr()` (`:211-219`, via `epx_next_pending()` +
   `epx_switch_ep()`), which requires the *original* stuck transfer to
   complete. It never will — that is the exact condition the watchdog exists
   to route around. This is not silent, though: `tuh_control_xfer()` arms
   `ctrl_info->timeout_at_ms` (`usbh.c:942`) before calling `hcd_setup_send()`,
   so the retried, parked SETUP gets its own watchdog deadline and the
   watchdog fires again `CFG_TUH_CONTROL_TIMEOUT_MS` later, aborts (also a
   no-op) and completes it FAILED (`usbh.c:2265-2287`), driving another
   `enum_full_complete(false)`. Since the underlying `epx` hardware state is
   never cleared, every retried SETUP parks the same way, so this repeats
   until `CFG_TUH_ENUM_ATTEMPT_MAX` (default 3) is exhausted — a bounded,
   visible enumeration failure after `~ATTEMPT_MAX * CFG_TUH_CONTROL_TIMEOUT_MS`
   (default ~15s), not an unbounded silent hang.

EHCI is not shown to fully wedge the same way (`hcd_port_reset()` there is a
real port reset, `hcd_edpt_abort_xfer()` there actually disables the async/
periodic schedule to kill an active qtd) — only the dev0 `hcd_device_close()`
skip is confirmed there, not a proven deadlock.

### Not reachable through PIO-USB, the driver this PR actually bench-validates

`src/portable/raspberrypi/pio_usb/hcd_pio_usb.c` implements all three for real:
`hcd_port_reset()` calls `pio_usb_host_port_reset_start()`,
`hcd_device_close()` calls `pio_usb_host_close_device()` unconditionally (no
dev_addr-0 skip), and `hcd_edpt_abort_xfer()` calls
`pio_usb_host_endpoint_abort_transfer()`. The retry+watchdog bench soak
recorded in `pico_pio_usb_bench_state.md` (10/10 rounds) went through this
driver, not `hcd_rp2040.c`.

## What remains

1. `hcd_rp2040.c`: give `hcd_device_close(dev_addr == 0)` the same "reset any
   stuck ep's `state` back to `EPSTATE_IDLE`" treatment the non-zero path
   already does at `:476` — scoped to `ep_pool[0]` (`epx`). Needs an answer,
   grounded in the RP2040 datasheet SIE section, for whether flipping
   `state` alone is safe or whether the SIE must first be told to stop
   (register-level abort, the same gap `hcd_edpt_abort_xfer()` leaves as TODO)
   — otherwise the fix can race a still-transacting SIE and corrupt the next
   transfer instead of just hanging it.
2. `ehci.c`: confirm whether the real `hcd_port_reset()` there is sufficient
   to clear a dev0 qhd/qtd left by an aborted transfer, or needs the same
   dev_addr-0 carve-out removed from `hcd_device_close()`.
3. HIL: a NAK-forever dev0 fixture (or a way to simulate one) on a board using
   native `hcd_rp2040.c`, and again on an EHCI host, to confirm the retry
   actually recovers instead of parking silently.

## Why it was split out

Requires SIE/register-level changes to two HCD drivers neither exercised by
this PR's own bench validation (PIO-USB), each needing HIL confirmation on
that specific host-mode hardware to avoid trading a hang for wire corruption;
#3862's own scope was kept to `usbh.c` + `tusb_option.h`.
