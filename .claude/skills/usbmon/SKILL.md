---
name: usbmon
description: Use when capturing, analyzing, or debugging USB bus traffic on a link where a Linux PC is the host (TinyUSB in device role) — enumeration failures, STALLed control transfers, missing/short bulk or interrupt transfers, isochronous/audio dropouts, or descriptor problems. Captures host-side URBs with usbmon + tshark into a Wireshark pcapng and decodes them. Not applicable when TinyUSB is the host — no URBs traverse the PC (use usb-sniffer / target-debug). Use it whenever you need to see what the Linux host actually exchanged with a device on real hardware.
---

# usbmon — capture & debug USB traffic

`usbmon` records host-side **URBs** — control / bulk / interrupt / isochronous transfers, descriptors, class requests, STALLs, short packets — i.e. exactly what the host exchanged with a device. Use it to debug a TinyUSB device on real hardware. (It's host/URB-level, not wire-level; for SOF/ACK/electrical use a hardware analyzer.) It exists only on the Linux host side of a link: when TinyUSB runs the *host* stack (peer = another TinyUSB board or a Linux gadget, e.g. a Raspberry Pi), neither end has usbmon — capture the wire (`usb-sniffer` skill) or instrument the target (`target-debug` skill).

**Setup (assumed in place):** `usbmon` loaded and a udev rule `SUBSYSTEM=="usbmon", GROUP="wireshark", MODE="0640"` with your user in the `wireshark` group — so `tshark` captures with no `sudo`. Freshly added to the group? The running shell doesn't have it yet (group adds need a new login) — wrap captures in `sg wireshark -c 'tshark -i usbmon3 -s 128 -a duration:30 -w /tmp/cap.pcapng'`; reading a finished `.pcapng` (`tshark -r`) needs no group. `-s 128` (snaplen) keeps only URB headers/status, not payloads — use it for long/high-throughput captures.

## Capture

```bash
.claude/skills/usbmon/scripts/usbcap.py <bus|VID:PID|VID:|auto> [seconds] [outfile] [--snaplen 128]
# examples
.claude/skills/usbmon/scripts/usbcap.py cafe: 10              # the bus of the plugged-in TinyUSB (VID 0xcafe) device
.claude/skills/usbmon/scripts/usbcap.py 3 8 /tmp/enum.pcapng  # bus 3, 8 s
```
`lsusb` shows `Bus 00N` → interface `usbmonN`; `usbmon0` = all buses. Capture the device's own bus. A selector that matches devices on several buses (`cafe:` on a rig) is refused with the matches listed: pick the bus, or `auto` when you really want every bus. To catch enumeration, start the capture, then replug the device.

## Analyze

```bash
tshark -r cap.pcapng                               # one line per URB
tshark -r cap.pcapng -V | less                     # full dissection (descriptors decoded)
tshark -r cap.pcapng -Y 'usb.device_address==26'   # filter to one device
```

## Filter (`-Y '<expr>'`)

| Goal                                             | Expression                                                  |
|--------------------------------------------------|-------------------------------------------------------------|
| One device / endpoint                            | `usb.device_address==26` / `usb.endpoint_address==0x81`     |
| IN (to host) / OUT (from host)                   | `usb.endpoint_address.direction==1` / `==0`                 |
| Submit / Complete event                          | `usb.urb_type=='S'` / `=='C'` (char literal: single quotes) |
| Control / bulk / interrupt / iso                 | `usb.transfer_type==2` / `3` / `1` / `0`                    |
| Only transfers carrying data                     | `usb.data_len>0`                                            |
| GET_DESCRIPTOR / SET_ADDRESS / SET_CONFIGURATION | `usb.setup.bRequest==6` / `5` / `9`                         |
| SET_INTERFACE / CLEAR_FEATURE (clear-halt)       | `usb.setup.bRequest==11` / `1`                              |
| Descriptor type DEVICE/CONFIG/STRING/HID-report  | `usb.bDescriptorType==1` / `2` / `3` / `0x22`               |
| Class / vendor requests                          | `usb.bmRequestType.type!=0`                                 |
| STALLs / errors                                  | `usb.urb_status!=0 && usb.urb_status!=-115`                 |

Combine with `&&` — e.g. one endpoint's data: `usb.endpoint_address==0x02 && usb.data_len>0`.

**`usb.urb_status` codes** (errno): `0` success · `-115` `-EINPROGRESS` (every Submit) · `-71` `-EPROTO` (transaction error — device answered wrong or didn't service in time, after the HC's retries) · `-32` `-EPIPE` (STALL) · `-110` `-ETIMEDOUT` · `-75` `-EOVERFLOW` (babble). A **control** transfer completing `-71` ≠ a STALL: the device mis-/under-served it (e.g. EP0 starved under load), not a `tud_*_control_xfer_cb` returning false.

**Decoding class requests:** the plain `-e usb.setup.bRequest` field is often blank for class requests (CDC `SET_LINE_CODING` 0x20 / `SET_CONTROL_LINE_STATE` 0x22, `bmRequestType==0x21`) — Wireshark routes them to class fields. Use `-V` on the Submit frame to get the request name + `wValue` (for `SET_CONTROL_LINE_STATE`, `wValue` bit0=DTR, bit1=RTS).

**Hard limit — usbmon is URB-level, not wire-level.** It cannot show data toggle (DATA0/DATA1) or NAKs. A device-side stall and a toggle desync both look identical: Submits on an endpoint with no Completes. To tell them apart, pair usbmon with **on-device GDB** (e.g. `openocd` + `gdb`): read the EP control register (response/toggle bits) and the DCD/USBD/class structs (`data.xfer[ep][dir]`, `_usbd_dev.ep_status`, `_cdcd_itf[].line_state`) at the moment of the hang. Build `MinSizeRel` still ships DWARF, so `p`/struct access works.

## Host-side kernel logs (dynamic debug)

usbmon shows the URBs; the kernel's **dynamic debug** shows the host driver's *reasoning* that usbmon can't — port resets, enumeration retries, address (re)assignment, EP halts, xHCI ring/command errors. The `usb-kernel-debug` skill owns it; its script flips the print flag on an allowlisted set of USB modules:

```bash
sudo .claude/skills/usb-kernel-debug/scripts/usb_dyndbg.sh on usbcore xhci_hcd   # host controller from `lsusb -t` Driver=; usbcore for enumeration/hub logic
dmesg -w                                                                        # follow live; replug to catch enumeration
sudo .claude/skills/usb-kernel-debug/scripts/usb_dyndbg.sh off usbcore xhci_hcd  # OFF when done — very noisy
```

Best for **re-enumerates / enumeration stalls / port-reset storms**, where usbmon shows the resets but not the host's reason.

## Troubleshoot — symptom → what to check

| Symptom | Look for |
|---|---|
| Not recognized / re-enumerates | Is `GET DESCRIPTOR (DEVICE)` answered, `bMaxPacketSize0` sane? Repeated SET_ADDRESS / resets = device too slow to respond. Enable `usbcore`/`xhci_hcd` dynamic debug (above) for the host's reset reason. |
| Enumeration stalls | Find the last good control transfer; the next request (often CONFIG, a string, or the first class request) is what your `tud_descriptor_*` / control callback mishandled. |
| Control STALL | `usb.urb_status!=0` on a control URB = a `tud_*_control_xfer_cb` returned `false` or didn't handle that `bRequest` (decode it with `-V`). |
| Bulk / interrupt missing or short | On the endpoint, do Submits get Completes? an unexpected short (`usb.data_len < wMaxPacketSize`) = FIFO/length bug; no completions = the class never wrote. |
| CDC read (`dd`/cat) hangs, IN endpoint idle | Don't assume a bulk-IN bug. Check EP0: did `SET_CONTROL_LINE_STATE` (`bmRequestType==0x21`) complete `0` or fail `-71`? If it failed, the device never saw DTR → `tud_cdc_connected()` is false → the app stops sourcing TX. A device that streams fine in isolation but stalls right after a bulk **write** phase points here (control transfer starved by concurrent bulk). Confirm device-side with GDB: `_cdcd_itf[0].line_state` bit0 (DTR). |
| ISO / audio dropouts | ISO URB cadence (~1/ms at full-speed) and payload lengths; zero-length frames = device starved the endpoint. |
| Wrong descriptors | `-V` decodes them; check `bLength` / `wTotalLength` against your `tud_descriptor_configuration_cb`. |
