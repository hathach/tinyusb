# HID Generic In/Out

A USB HID device that exchanges raw IN/OUT reports over a vendor-defined usage page, with no standard keyboard/mouse usage. It simply echoes back whatever the host sends.

## What it does

- Presents one HID interface built from the generic in/out report descriptor template (vendor usage page), with both an IN and an OUT endpoint.
- Whenever the host sends data on the OUT endpoint (or via SET_REPORT), the device echoes the same bytes back to the host via `tud_hid_report`.
- The LED blinks to indicate USB state (250 ms not mounted, 1000 ms mounted, 2500 ms suspended).
- Because the reports carry no standard HID usage, the device is meant to be driven by a host-side tool rather than recognized as a keyboard/mouse. The example ships with `hid_test.js` (node-hid) and `hid_test.py` (Python `hid`) scripts to send and receive data.

## USB Descriptors

| Interface | Class driver |
|-----------|--------------|
| 0 | HID (generic in/out, raw vendor reports) |

## Configuration

Notable `tusb_config.h` settings:

```c
#define CFG_TUD_HID            1
#define CFG_TUD_HID_EP_BUFSIZE 64
```

## Building

CMake:

```bash
mkdir build && cd build
cmake -DBOARD=raspberry_pi_pico ..
cmake --build .
```

Make:

```bash
make BOARD=raspberry_pi_pico all
```

## Try it

After flashing, run the bundled host tool — `node hid_test.js` or `python3 hid_test.py` — to send a buffer to the device and observe the same data echoed back.
