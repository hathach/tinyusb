# HID Composite

A single USB HID interface that combines several HID functions using report IDs: keyboard, mouse, stylus pen, consumer control, and gamepad.

## What it does

- Presents one HID interface carrying five report types, distinguished by report ID: keyboard, mouse, stylus pen, consumer control, and gamepad.
- Polls the board button every 10 ms and, on each cycle, sends a chain of reports (the next report is queued from `tud_hid_report_complete_cb`):
  - Keyboard: while the button is held, sends the `A` keycode; releasing sends an empty report.
  - Mouse: moves diagonally (+5, +5) each cycle.
  - Stylus pen: while the button is held, reports tip-switch + in-range at position (100, 100).
  - Consumer control: while the button is held, sends Volume Decrement; releasing sends a release report.
  - Gamepad: while the button is held, sets the hat to Up and presses button A; releasing recenters.
- If the device is suspended, pressing the button issues a USB remote wakeup.
- The LED blinks to indicate USB state (250 ms not mounted, 1000 ms mounted, 2500 ms suspended). When the host turns on Caps Lock, the LED is driven solid on via the keyboard's OUTPUT report.

## USB Descriptors

| Interface | Class driver |
|-----------|--------------|
| 0 | HID (keyboard + mouse + stylus pen + consumer control + gamepad, composite report IDs) |

## Configuration

Notable `tusb_config.h` settings:

```c
#define CFG_TUD_HID            1
#define CFG_TUD_HID_EP_BUFSIZE 16
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

## FreeRTOS variant

A FreeRTOS build is in `examples/device/hid_composite_freertos` — the USB device and HID logic run as FreeRTOS tasks (LED via a software timer). It exposes four report types (keyboard, mouse, consumer control, gamepad) and, unlike this example, omits the stylus-pen report.

## Try it

After flashing, the board enumerates as a single HID device that the host recognizes as a keyboard, mouse, consumer-control, gamepad, and stylus. Press and hold the button to type `A`, decrease the volume, press gamepad button A, and report stylus contact; the mouse pointer drifts toward the bottom-right continuously.
