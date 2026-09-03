# HCDs without a real abort can hand an expired control transfer's completion to the next one

> Split out of PR #3862 (`claude/usbh-enum-timeout`, control transfer watchdog +
> enumeration retry): per-HCD driver work the watchdog makes worth doing, not a
> bug in the watchdog logic itself.

**Goal:** after `control_xfer_timeout_expired()` gives up on a control transfer,
no HCD keeps the hardware transaction alive such that its eventual completion
can be misread as the completion of a later control transfer to the *same*
device address.

---

## What is already established

### The slot is global and reused as soon as the watchdog completes it

`control_xfer_timeout_expired()` (`src/host/usbh.c`) calls `hcd_edpt_abort_xfer()`
then `control_xfer_complete(daddr, XFER_RESULT_FAILED)`, which sets the single
shared `_usbh_data.ctrl_xfer_info.stage` back to `CONTROL_STAGE_IDLE` before
invoking `complete_cb`. The very next `tuh_control_xfer()` claim or
`control_xfer_dispatch_pending()` dispatch - for any device, including the one
that just timed out - reuses the slot immediately; a driver that reissues the
request from its `XFER_RESULT_FAILED` callback does so synchronously.

### `usbh_control_xfer_cb()` already drops most late completions

```c
if (ctrl_info->stage == CONTROL_STAGE_IDLE || ctrl_info->daddr != daddr) {
  return true;
}
```

A late completion is dropped when the slot is idle or owns a *different*
device's transfer. It cannot be dropped when the slot was reclaimed for the
*same* `daddr` before the stale completion drained: `hcd_event_t` carries only
`dev_addr` / `ep_addr` / `result` / `len`, no per-transfer token. During
enumeration this never matters - `enum_full_complete()` closes the device at
hcd level (`hcd_device_close()`) before any retry, which quiesces EP0 on every
HCD that implements close for that address (see
`pr3862-hcd-dev0-retry-teardown.md` for the two that skip dev0).

### Whether the hardware transaction stays alive depends on the HCD

`src/host/hcd.h` still documents `hcd_edpt_abort_xfer()` as "can only abort
transfer that has not been started", but most ports abort an active transfer:

| aborts an active transfer                                                            | no-op (`return false`, TODO)    |
| ------------------------------------------------------------------------------------ | ------------------------------- |
| pio_usb (`pio_usb_host_endpoint_abort_transfer`, waits out the in-frame transaction) | rp2040 native (`hcd_rp2040.c`)  |
| dwc2 (channel disable; HALTED handler deallocates without emitting a completion)     | musb (`hcd_musb.c`)             |
| ehci / ci_hs (`ehci.c:551`, removes the active qTD)                                  | rusb2 (`hcd_rusb2.c`)           |
| max3421 (`EP_STATE_ABORTING`)                                                        | ci_fs (`hcd_ci_fs.c`)           |
| stm32_fsdev (channel abort)                                                          | ch32_usbfs (`hcd_ch32_usbfs.c`) |
| lpc_ip3516 (closes the PTD)                                                          | ohci (`ohci.c`)                 |
| samd (pipe abort)                                                                    |                                 |

Left column: the watchdog is complete - the expired transfer produces no
completion. Right column: the transaction keeps running on EP0 of `daddr`; if
the device later answers it, the completion lands on whatever control transfer
to the same `daddr` is in flight at that moment. On rp2040 native the next
SETUP is additionally parked behind the busy EPX (`hcd_setup_send`,
`EPSTATE_PENDING_SETUP`), so the stale completion is what releases it.

## What remains

1. Implement `hcd_edpt_abort_xfer()` for an active transfer on the right-column
   HCDs, starting with rp2040 native (register-level EPX stop, needs the RP2040
   datasheet SIE section - same gap as `pr3862-hcd-dev0-retry-teardown.md`).
   Each needs HIL on that host-mode hardware with a NAK-forever fixture.
2. Update the `hcd_edpt_abort_xfer()` comment in `src/host/hcd.h` to the
   contract the ports actually implement (abort an active transfer, no
   completion afterwards), so new ports don't copy the no-op.
3. A usbh-level per-transfer token in `hcd_event_t` is not needed once every
   HCD aborts for real; only consider it if a port cannot.

## Why it was split out

Per-HCD register-level work on six ports, none of which is on the bench this PR
validated on (PIO-USB); #3862's scope was kept to `usbh.c` + `tusb_option.h`.
