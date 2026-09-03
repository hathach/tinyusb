# Host class drivers: always finish set_config, even on a failed request

> Split out of PR #3862 (`claude/usbh-enum-timeout`, control transfer watchdog +
> enumeration retry): a per-driver contract gap the watchdog makes easier to hit,
> but which exists on master for STALL already and is not the watchdog's to paper over.

**Goal:** no class driver's set_config chain may return without eventually calling
`usbh_driver_set_config_complete()`; a failed request must either be tolerated
(mount without the optional data) or reported as a failed interface.

---

## What is already established

### `enumerating_daddr` is only released by `usbh_driver_set_config_complete()`

`src/host/usbh.c` hands the enumerating device to each class driver's
`set_config(daddr, itf_num)` in `ENUM_CONFIG_DRIVER`; the driver runs its own
control-request chain and is expected to call `usbh_driver_set_config_complete()`
when done, which advances to the next interface and finally `enum_full_complete(true)`.
Nothing else clears `_usbh_data.enumerating_daddr`. While it is set, every later
`HCD_EVENT_DEVICE_ATTACH` is deferred (`usbh.c:800`, `tuh_task_ext`), so a chain
that silently stops wedges all future enumeration on that host.

### Drivers that stop silently on a non-SUCCESS result

- `src/class/hid/hid_host.c:576-582` `process_set_config()`:
  `TU_ASSERT(xfer->result == XFER_RESULT_SUCCESS,)` for everything except
  SET_IDLE / SET_PROTOCOL — a STALLed (or timed-out) GET_DESCRIPTOR(HID report)
  returns without `config_driver_mount_complete()`.
- `src/host/hub.c:309,328` `TU_ASSERT(XFER_RESULT_SUCCESS == xfer->result, )`
  in the hub's own set_config chain.
- Audit the rest (`cdc_host.c` mostly routes failures through
  `set_config_complete(p_cdc, false)`; `msc_host.c:426` tolerates GET_MAX_LUN
  failure and continues on bulk — these are the model to follow).

### Why this surfaced now

The watchdog in PR #3862 completes a control transfer that never finishes as
`XFER_RESULT_FAILED` after `CFG_TUH_CONTROL_TIMEOUT_MS`. Before it, a device that
NAKed a set_config request forever wedged the whole host anyway (no control slot);
after it, the same device reaches the driver's failure path — and the drivers above
turn a recoverable timeout into the same permanent wedge, just one level up.

A usbh-level backstop was tried in #3862 (fail the enumeration when the timed-out
transfer belonged to the enumerating device and the callback issued no follow-up)
and rejected in review: it cannot see non-control follow-ups (msc continues with a
bulk TEST_UNIT_READY after a failed GET_MAX_LUN) and would kill a valid mount.
The driver is the only place that knows whether the chain is still alive.

## What remains

1. `hid_host.c`: on a failed GET_DESCRIPTOR(HID report), mount without the
   descriptor (`config_driver_mount_complete(daddr, idx, NULL, 0)`) instead of
   asserting — the too-large-descriptor branch already does exactly that.
2. `hub.c`: route the two asserts to a failure path that still calls
   `usbh_driver_set_config_complete()` (a hub whose descriptor read fails should
   not block the root port for every later device).
3. Sweep the remaining `*_host.c` set_config chains for the same pattern.
4. Test: HIL or unit — force a STALL on the HID report descriptor request and
   check the next attach on the same host still enumerates.

## Why it was split out

Per-driver behavior, multiple files, each with its own class-spec judgement about
what "tolerate" means; none of it is specific to the watchdog or the enumeration
retry, and #3862's scope was deliberately kept to `usbh.c` + `tusb_option.h`.
