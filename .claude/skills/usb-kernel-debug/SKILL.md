---
name: usb-kernel-debug
description: Use when the Linux end of a USB link has to explain itself on real hardware — enumeration failures, STALLed control transfers, missing/short bulk or interrupt transfers, isochronous dropouts, descriptor problems, port reset storms, repeated re-enumeration, address errors, xHCI ring/command errors, "device descriptor read error", babble. On a Linux PC host (TinyUSB in device role) it captures the host-side URBs with usbmon + tshark into a Wireshark pcapng and turns on the kernel's dynamic debug for the host's reasoning; on a Linux gadget peer (e.g. Raspberry Pi, TinyUSB is the host) only dynamic debug applies, since no URBs traverse a gadget — pair it with usb-sniffer for the wire and target-debug for the MCU.
---

# usb-kernel-debug — what the Linux side of the link saw

Two kernel facilities, by the role Linux plays in the link:

| Linux is the … | usbmon URB capture (`usbcap.py`) | dynamic debug (`usb_dyndbg.sh`)          |
|----------------|----------------------------------|------------------------------------------|
| PC host        | yes — what the host exchanged    | yes — why the host driver acted          |
| gadget peer    | no — a UDC has no usbmon         | yes — `dwc2`/`dwc3` + `udc_core` + `libcomposite` |

Neither sees inside the TinyUSB MCU (`target-debug` skill) or the wire itself
(`usb-sniffer` skill). For a wedged device or bus on the rig PC use
`usb-kernel-recover`.

## usbmon — capture (host role)

`usbmon` records host-side **URBs** — control / bulk / interrupt / isochronous transfers, descriptors, class requests, STALLs, short packets — i.e. exactly what the host exchanged with a device.

Requires `usbmon` loaded and `/dev/usbmon*` readable by your user (`wireshark` group); the script says so when `tshark` cannot open the interface — a fresh group membership needs a new login or `sg wireshark -c '...'`. Reading a finished `.pcapng` needs no group.

```bash
.claude/skills/usb-kernel-debug/scripts/usbcap.py cafe: 10   # the bus of the plugged-in TinyUSB device, 10 s; --help for selectors and defaults
```
Capture the device's own bus. A selector matching devices on several buses (`cafe:` on a rig) is refused with the matches listed: pass the bus, or `auto` when you really want every bus. Full payloads by default; `--snaplen 128` keeps URB headers and status only, for long or high-throughput captures. To catch enumeration, start the capture, then replug the device.

## usbmon — analyze

```bash
tshark -r cap.pcapng                               # one line per URB
tshark -r cap.pcapng -V | less                     # full dissection (descriptors decoded)
tshark -r cap.pcapng -Y 'usb.device_address==26'   # filter to one device
```

### Filter (`-Y '<expr>'`)

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

## Dynamic debug (either role)

Kernel **dynamic debug** shows the Linux side's *reasoning* that capture can't: port resets and their causes, enumeration retries, address (re)assignment, EP halts, xHCI ring/command errors. `scripts/usb_dyndbg.sh` (`--help` lists the allowlisted modules; run with `sudo`) flips the print flag for those USB modules only:

```bash
sudo .claude/skills/usb-kernel-debug/scripts/usb_dyndbg.sh on usbcore xhci_hcd    # then reproduce while following `sudo dmesg -w`
sudo .claude/skills/usb-kernel-debug/scripts/usb_dyndbg.sh off usbcore xhci_hcd   # ALWAYS: left on, it floods the log and skews timing
```

Module choice by role: on a host, `usbcore` for enumeration/hub logic plus the controller driver `lsusb -t` shows for the bus; on a gadget peer, `dwc2` (or `dwc3`) + `udc_core` + `libcomposite`, run on the peer itself (the script is self-contained, copy it over). Best for **re-enumerates / enumeration stalls / port-reset storms**, where usbmon shows the resets but not the host's reason.

## Troubleshoot — symptom → what to check

| Symptom | Look for |
|---|---|
| Not recognized / re-enumerates | Is `GET DESCRIPTOR (DEVICE)` answered, `bMaxPacketSize0` sane? Repeated SET_ADDRESS / resets = device too slow to respond. Enable `usbcore`/`xhci_hcd` dynamic debug (above) for the host's reset reason. |
| Enumeration stalls | Find the last good control transfer; the next request (often CONFIG, a string, or the first class request) is what your `tud_descriptor_*` / control callback mishandled. |
| Control STALL | A control URB completing `-32` (`-EPIPE`) is the STALL: usually a `tud_*_control_xfer_cb` returning `false` or an unhandled `bRequest` (decode it with `-V`). `-71` is not a STALL, see the errno list. |
| Bulk / interrupt missing or short | On the endpoint, do Submits get Completes? An unexpected short (`usb.data_len < wMaxPacketSize`) suggests a FIFO/length bug; no completions can be the class never writing, a stalled endpoint or a toggle desync, which usbmon cannot separate — check the target side (GDB, `target-debug`). |
| CDC read (`dd`/cat) hangs, IN endpoint idle | Don't assume a bulk-IN bug. Check EP0: did `SET_CONTROL_LINE_STATE` (`bmRequestType==0x21`) complete `0` or fail `-71`? If it failed, the device never saw DTR → `tud_cdc_connected()` is false → the app stops sourcing TX. A device that streams fine in isolation but stalls right after a bulk **write** phase points here (control transfer starved by concurrent bulk). Confirm device-side with GDB: `_cdcd_itf[0].line_state` bit0 (DTR). |
| ISO / audio dropouts | ISO URB cadence (~1/ms at full-speed) and payload lengths; zero-length frames = device starved the endpoint. |
| Wrong descriptors | `-V` decodes them; check `bLength` / `wTotalLength` against your `tud_descriptor_configuration_cb`. |
