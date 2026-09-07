# MSC host: bound the Test Unit Ready retry loop and act on sense data

> Split out of PR #3851 (`etmtrace-rp2350`, rp2350 ETM trace + stock clocks):
> a host-stack MSC bug with no relation to that branch's scope.

**Goal:** stop `msch_open`'s enumeration retry from spinning forever when a
device answers Test Unit Ready with CHECK CONDITION, and use the sense data the
driver already fetches to decide whether to keep waiting, give up, or report.

---

## What is already established

### The loop is unbounded, and the source says so

`src/class/msc/msc_host.c:445-472` is a two-function cycle with no counter:

```c
static bool config_test_unit_ready_complete(...) {
  if (csw->status == 0) {
    ... tuh_msc_read_capacity(...);            // ready -> proceed to mount
  } else {
    // Note: During enumeration, some device fails Test Unit Ready and require a few retries
    // with Request Sense to start working !!
    // TODO limit number of retries                        <-- :459, pre-existing
    TU_LOG_DRV("SCSI Request Sense\r\n");
    TU_ASSERT(tuh_msc_request_sense(dev_addr, cbw->lun, enum_buf,
                                    config_request_sense_complete, 0));
  }
  return true;
}

static bool config_request_sense_complete(...) {
  TU_ASSERT(csw->status == 0);
  TU_ASSERT(tuh_msc_test_unit_ready(dev_addr, cbw->lun,
                                    config_test_unit_ready_complete, 0));   // :472
  return true;
}
```

Two defects, independent of each other:

1. **No bound.** TUR fail -> Request Sense -> TUR -> ... forever. `tuh_msc_mount_cb()`
   is never called and the application is never told anything; the device sits
   enumerated-but-unmounted indefinitely.
2. **Sense data is fetched and discarded.** `config_request_sense_complete`
   checks only the CSW status. `enum_buf` holds a `scsi_sense_fixed_resp_t`
   whose `sense_key` / ASC / ASCQ distinguish "Not Ready — becoming ready"
   (retry is correct) from "Not Ready — medium not present" (a card reader with
   no card; retrying can never succeed) from a hard error. The driver cannot
   currently tell these apart because it never looks.

### Measured on hardware (2026-08-25)

Rig: `raspberry_pi_pico` (RP2040) + Pico-PIO-USB host on GP20/21, probe
`E6614103E719612F`, console over the probe's CDC. Build:
`-DCFG_TUH_RPI_PIO_USB=1 -DLOG=2`.

- `examples/host/msc_file_explorer` never mounts. Debug log over ~25 s:
  **1× `SCSI Test Unit Ready`, 350× `SCSI Request Sense`**, zero
  `SCSI Read Capacity`, zero mount callbacks. `dd` reports
  `no MSC device mounted`.
- **The transfers themselves all succeed** — every CBW/CSW pair logs `OK`
  (`Queue EP 02 with 31 bytes ... OK`, `Queue EP 81 with 13 bytes ... OK`), so
  this is a SCSI-state-machine problem, not a bulk-transfer or PIO-USB timing
  problem.
- Reproduced with **two different drives** (`24a9:1802` "STORAGE DEVICE" and the
  drive swapped in after it), so it is not one device's quirk.
- Control transfers on the same target are fine: `examples/host/device_info`
  reads full descriptors from the same drive on the same board
  (`bcdUSB 0210`, `bMaxPacketSize0 64`, i.e. full-speed).
- **The very same drive mounts and sustains I/O on RP2350**
  (`pico2_etm_trace` carrier): `msc_file_explorer` + `dd` returns
  `dd: 524288 bytes in 8448 ms = 62 KB/s`. Confirmed by the maintainer at the
  bench, so the device is healthy and the "not ready" answer is provoked by
  something specific to the RP2040 setup.
- **Bumping Pico-PIO-USB does not fix it.** Retested with upstream HEAD
  `5a37a66` (10 commits ahead of the pinned `675543b`, including
  `512d3a2` "Place calc_usb_crc16 in RAM like calc_usb_crc5 and the CRC
  tables", which looked like a promising RP2040 timing fix, and `cbf055d`
  transaction-length clamp) via `-DPICO_PIO_USB_PATH=<clone>`: identical
  failure, no mount.
- Clock is **not** a factor: identical failure at 120 MHz, 133 MHz and
  156 MHz on RP2040 (and on RP2350 all of 120/125/126/138/150/156/162/174/186/240 MHz
  behave identically).

### What is NOT established

- The actual sense key/ASC/ASCQ the failing drives return — the driver never
  logs it. **Task 1 below exists to capture it**, and its answer decides whether
  a bounded retry is sufficient or a "medium not present" path is also needed.
- **Why the RP2040 setup provokes the not-ready state.** Leading suspect is
  VBUS quality rather than firmware: the RP2350 carrier feeds J5 through a
  proper load switch, while the RP2040 rig is a bare Pico whose GP22 "VBUS
  enable" drives nothing (no load switch on a bare Pico), so the drive is fed
  directly off the VBUS pin through hookup wire. A bus-powered drive that
  cannot spin up answers exactly this "not ready" forever. Measure VBUS at the
  device under load, or retest with a powered hub / self-powered device,
  BEFORE attributing the stall to the host stack.
- The actual sense key (Task 1) — still the gate for any policy change.

---

## What remains

### Task 1: Log the sense response (diagnostic, ship-able on its own)

**Files:** `src/class/msc/msc_host.c` (`config_request_sense_complete`, ~:467)

Add a `TU_LOG_DRV` of `sense_key`, `add_sense_code`, `add_sense_qualifier` from
the fixed-format response in `usbh_get_enum_buf()`. `scsi_sense_fixed_resp_t` is
already declared in `src/class/msc/msc.h`.

Verify on the rig above: rebuild `msc_file_explorer` with `-DLOG=2`, flash, read
the probe CDC, and record the triple. Expected candidates:
`0x02/0x04/0x01` (becoming ready) or `0x02/0x3A/0x00` (medium not present).

### Task 2: Bound the retry

**Files:** `src/class/msc/msc_host.c`, `msch_interface_t` (add a retry counter),
`src/class/msc/msc_host.h` (a `CFG_TUH_MSC_TUR_RETRY_COUNT`-style knob with a
sane default; follow the existing `CFG_TUH_MSC_*` naming in
`src/tusb_option.h`).

On exhaustion, stop the cycle and surface the failure rather than silently
looping — the application currently has no way to learn the device is stuck.

### Task 3: Decide behaviour per sense key

Gated on Task 1's measurement. At minimum: keep retrying on "becoming ready",
stop immediately on "medium not present". Do not invent policy for sense keys
that were not observed.

### Task 4: Regression coverage

`test/unit-test/` has no MSC host suite today; adding one means mocking
`tuh_msc_*` completions. Confirm with the maintainer whether a unit test or a
HIL case on a known not-ready device (an empty card reader is the cheap
reproducer) is the wanted evidence before building either.

---

## Why it was split out

Found while sweeping PIO-USB clocks on the `etmtrace-rp2350` branch, which
touches only rp2040/rp2350 clock pinning and ETM trace config. This bug is in
the class-driver layer, affects every MCU running the MSC host, and predates
that branch (the `// TODO limit number of retries` is already in master). It
deserves its own PR and its own hardware evidence.
