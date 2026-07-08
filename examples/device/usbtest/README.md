# usbtest

Device-side peer of the Linux kernel USB test pair:

- `usbtest.ko` — host kernel module (`drivers/usb/misc/usbtest.c`) containing ~30 numbered
  test cases over bulk/control/interrupt/isochronous transfers.
- `testusb` — userspace dispatcher (`tools/usb/testusb.c`) that tells the module which case
  to run via usbfs ioctl.

This example implements the Gadget-Zero style *source/sink* protocol on a vendor-specific
interface so the whole battery can exercise TinyUSB device controller drivers:

- bulk IN = infinite source (usbtest pattern 0: all zeros)
- bulk OUT = infinite sink (data discarded)

## Tiers

The firmware advertises its capability tier in `bcdDevice` (`0x01TT`); the host script picks
the matching test battery automatically.

| Tier | Capability | usbtest cases |
|------|------------|---------------|
| 1 | bulk source/sink | 0, 9, 10, 1–8, 11, 12, 24, 13, 29, 17–20, 27, 28 |
| 2 | + vendor control `0x5b`/`0x5c` (ctrl_out) | + 14, 21 |
| 3 | + interrupt source/sink | + 25, 26 |
| 4 | + isochronous source/sink | + 15, 16, 22, 23 |

This example implements all four tiers using the vendor class with the interrupt
(`CFG_TUD_VENDOR_EP_INT_OUT/IN`) and isochronous (`CFG_TUD_VENDOR_EP_ISO_OUT/IN`)
endpoint pairs and altsetting support (`CFG_TUD_VENDOR_ALT_SETTINGS`): alt 0
carries no endpoints, alt 1 the full source/sink set, per USB 2.0 5.6.3 (the host
usbtest driver selects alt 1 itself).

## Test cases

Directions are from the host's point of view: *write* = host→device (OUT endpoint, device
sinks and discards), *read* = device→host (IN endpoint, device sources zeros and the host
verifies every byte). All checking happens host-side in `usbtest.ko`; a case fails on a data
mismatch, an unexpected short packet/STALL, or a timeout. What each case stresses on the
device/DCD side:

| # | Name | What it does / what it exercises |
|---|------|----------------------------------|
| 0 | NOP | ioctl round-trip sanity, no USB traffic — proves the interface bound with the right capability profile |
| 9 | ch9 subset | chapter-9 standard control requests (GET_DESCRIPTOR, GET_STATUS, SET/CLEAR_FEATURE, SET_INTERFACE, …) — the EP0 state machine, incl. status stages and ZLPs |
| 10 | queued control | many control URBs in flight at once — EP0 under sustained back-to-back SETUPs |
| 1 / 2 | bulk write / read | plain OUT sink / IN source streams of whole max-size packets — FIFO handling, multi-packet transfers |
| 3 / 4 | bulk write / read vary | same with transfer sizes varying per URB — short packets and packet-boundary edge cases |
| 5–8 | bulk sg write/read (+vary) | scatter-gather queued URBs — continuous packet pressure with no inter-URB gap; classic overflow/babble catcher |
| 11 / 12 | unlink reads / writes | URBs submitted then cancelled mid-flight — the device keeps streaming while the host aborts; DCD abort/cleanup paths |
| 24 | unlink queued writes | unlink from a deep OUT queue — same, under queue pressure |
| 13 | ep halt set/clear | SET_FEATURE(ENDPOINT_HALT), verify the endpoint really STALLs, then CLEAR_FEATURE and verify traffic resumes at DATA0 — stall must abort an armed transfer (and flush any loaded FIFO) |
| 29 | toggle clear | CLEAR_FEATURE(HALT) on a **non-halted** endpoint mid-traffic, purely to reset the data toggle — the DCD must reset DATA0 *without* disarming the queued transfer (historically the most common per-DCD bug in this battery) |
| 17 / 18 | bulk write / read unaligned | bulk streams from oddly-offset host buffers — host DMA-alignment path; the device sees normal traffic |
| 19 / 20 | bulk write / read premapped | bulk streams using host pre-mapped DMA buffers — another host memory path |
| 27 / 28 | bulk write / read perf | sustained maximum-throughput streams, reported in MB/s — real-time FIFO servicing under load |
| 14 | ctrl_out write/read | vendor EP0 request `0x5b` stores wLength bytes, `0x5c` reads them back, sizes varying — multi-packet control-OUT data stages and buffer persistence across requests |
| 21 | ctrl_out unaligned | same from odd host buffer offsets |
| 25 / 26 | int write / read | interrupt OUT sink / IN source at the descriptor's polling interval — interrupt endpoint arming and completion |
| 15 / 16 | iso write / read | isochronous OUT sink / IN source, one packet per (micro)frame with per-packet status — no handshake/retry, DATA0-only; the IN source must re-arm fast enough to make every frame deadline |
| 22 / 23 | iso write / read unaligned | same from odd host buffer offsets |

Per-case iteration counts and sizes are chosen by `test/hil/usbtest.py` for the negotiated
speed (see its `PARAMS` table); the authoritative case implementations live in the kernel's
`drivers/usb/misc/usbtest.c`.

## Running

Use the host script (handles driver binding, per-case parameters, result parsing):

```bash
python3 test/hil/usbtest.py --serial <board-uid>
```

Requirements on the host: `usbtest` kernel module (`CONFIG_USB_TEST`, `modprobe usbtest`),
the `testusb` binary built from kernel `tools/usb/testusb.c`, and sudo (usbfs ioctls +
driver bind/unbind).

Manual runs are possible but beware `testusb` defaults: always pass explicit `-s`/`-v`
values that are multiples of 512 — the device streams whole max-size packets, so a
non-packet-aligned read length overflows (`-EOVERFLOW`), and never run bare `testusb -a`
(the default parameter set includes cases with invalid parameters and hour-long runtimes
at full speed).

```bash
# bind: MUST use the 5-field form referencing Gadget Zero (0525:a4a0) so the
# dynamic id inherits its capability profile. A plain "cafe 4010" id leaves
# driver_info NULL, which usbtest_probe() dereferences -> kernel oops.
sudo modprobe usbtest
echo "cafe 4010 0 0525 a4a0" | sudo tee /sys/bus/usb/drivers/usbtest/new_id
# example: bulk write/read
sudo testusb -D /dev/bus/usb/<BBB>/<DDD> -t 1 -c 128 -s 1024 -v 512
sudo testusb -D /dev/bus/usb/<BBB>/<DDD> -t 2 -c 128 -s 1024 -v 512
```
