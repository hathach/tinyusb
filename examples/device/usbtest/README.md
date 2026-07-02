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
