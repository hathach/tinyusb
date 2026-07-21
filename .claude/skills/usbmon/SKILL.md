---
name: usbmon
description: Use when capturing, analyzing, or debugging USB bus traffic for TinyUSB device development on Linux — enumeration failures, STALLed control transfers, missing/short bulk or interrupt transfers, isochronous/audio dropouts, or descriptor problems. Captures host-side URBs with usbmon + tshark into a Wireshark pcapng and decodes them. Use whenever you need to see what the host actually exchanged with a device on real hardware, even if the user just says "sniff USB", "capture the enumeration", or "why won't my device enumerate".
---

# usbmon — capture & debug USB traffic

`usbmon` records host-side **URBs** — control / bulk / interrupt / isochronous transfers, descriptors, class requests, STALLs, short packets — i.e. exactly what the host exchanged with a device. Use it to debug a TinyUSB device on real hardware. (It's host/URB-level, not wire-level; for SOF/ACK/electrical use a hardware analyzer.)

**Setup (assumed in place):** `usbmon` loaded and a udev rule `SUBSYSTEM=="usbmon", GROUP="wireshark", MODE="0640"` with your user in the `wireshark` group — so `tshark` captures with no `sudo`. Freshly added to the group? The running shell doesn't have it yet (group adds need a new login) — wrap captures in `sg wireshark -c 'tshark -i usbmon3 -s 128 -a duration:30 -w /tmp/cap.pcapng'`; reading a finished `.pcapng` (`tshark -r`) needs no group. `-s 128` (snaplen) keeps only URB headers/status, not payloads — use it for long/high-throughput captures.

## Capture

```bash
.claude/skills/usbmon/scripts/usbcap.sh <bus|VID:PID|auto> [seconds] [outfile]
# examples
.claude/skills/usbmon/scripts/usbcap.sh cafe: 10              # auto-find a TinyUSB (VID 0xcafe) device
.claude/skills/usbmon/scripts/usbcap.sh 3 8 /tmp/enum.pcapng  # bus 3, 8 s
```
`lsusb` shows `Bus 00N` → interface `usbmonN`; `usbmon0` = all buses. Capture the device's own bus. To catch enumeration, start the capture, then replug the device.

## Analyze

```bash
tshark -r cap.pcapng                               # one line per URB
tshark -r cap.pcapng -V | less                     # full dissection (descriptors decoded)
tshark -r cap.pcapng -Y 'usb.device_address==26'   # filter to one device
```

## Filter (`-Y '<expr>'`)

| Goal | Expression |
|---|---|
| One device / endpoint | `usb.device_address==26` / `usb.endpoint_address==0x81` |
| IN (to host) / OUT (from host) | `usb.endpoint_address.direction==1` / `==0` |
| Submit / Complete event | `usb.urb_type=='S'` / `=='C'` (char literal: single quotes) |
| Control / bulk / interrupt / iso | `usb.transfer_type==2` / `3` / `1` / `0` |
| Only transfers carrying data | `usb.data_len>0` |
| GET_DESCRIPTOR / SET_ADDRESS / SET_CONFIGURATION | `usb.setup.bRequest==6` / `5` / `9` |
| SET_INTERFACE / CLEAR_FEATURE (clear-halt) | `usb.setup.bRequest==11` / `1` |
| Descriptor type DEVICE/CONFIG/STRING/HID-report | `usb.bDescriptorType==1` / `2` / `3` / `0x22` |
| Class / vendor requests | `usb.bmRequestType.type!=0` |
| STALLs / errors | `usb.urb_status!=0 && usb.urb_status!=-115` |

Combine with `&&` — e.g. one endpoint's data: `usb.endpoint_address==0x02 && usb.data_len>0`.

**`usb.urb_status` codes** (errno): `0` success · `-115` `-EINPROGRESS` (every Submit) · `-71` `-EPROTO` (transaction error — device answered wrong or didn't service in time, after the HC's retries) · `-32` `-EPIPE` (STALL) · `-110` `-ETIMEDOUT` · `-75` `-EOVERFLOW` (babble). A **control** transfer completing `-71` ≠ a STALL: the device mis-/under-served it (e.g. EP0 starved under load), not a `tud_*_control_xfer_cb` returning false.

**Decoding class requests:** the plain `-e usb.setup.bRequest` field is often blank for class requests (CDC `SET_LINE_CODING` 0x20 / `SET_CONTROL_LINE_STATE` 0x22, `bmRequestType==0x21`) — Wireshark routes them to class fields. Use `-V` on the Submit frame to get the request name + `wValue` (for `SET_CONTROL_LINE_STATE`, `wValue` bit0=DTR, bit1=RTS).

**Hard limit — usbmon is URB-level, not wire-level.** It cannot show data toggle (DATA0/DATA1) or NAKs. A device-side stall and a toggle desync both look identical: Submits on an endpoint with no Completes. To tell them apart, pair usbmon with **on-device GDB** (e.g. `openocd` + `gdb`): read the EP control register (response/toggle bits) and the DCD/USBD/class structs (`data.xfer[ep][dir]`, `_usbd_dev.ep_status`, `_cdcd_itf[].line_state`) at the moment of the hang. Build `MinSizeRel` still ships DWARF, so `p`/struct access works.

## Host-side kernel logs (dynamic debug)

usbmon shows the URBs; the kernel's **dynamic debug** shows the host driver's *reasoning* that usbmon can't — port resets, enumeration retries, address (re)assignment, EP halts, xHCI ring/command errors. Turn it on, reproduce, read `dmesg`:

```bash
# pick the module from `lsusb -t` Driver= : xhci_hcd (USB3 / most modern HCs), ehci_hcd (USB2),
# plus usbcore for enumeration / hub / descriptor logic (controller-independent).
echo 'module usbcore +p'  | sudo tee /sys/kernel/debug/dynamic_debug/control
echo 'module xhci_hcd +p' | sudo tee /sys/kernel/debug/dynamic_debug/control
dmesg -w                                                       # follow live; replug to catch enumeration
echo 'module xhci_hcd -p' | sudo tee /sys/kernel/debug/dynamic_debug/control   # OFF when done — very noisy
```

`+p` enables the print flag at every call-site in that module (`+pfl` also tags func+line); `grep <module> /sys/kernel/debug/dynamic_debug/control` lists active sites. The control file is debugfs (`/sys/kernel/debug`), root-only → use `sudo tee` (a plain `sudo echo >` redirects as your user, not root). Needs `CONFIG_DYNAMIC_DEBUG` (standard on distro kernels). Best for **re-enumerates / enumeration stalls / port-reset storms**, where usbmon shows the resets but not the host's reason.

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
