# HID Multiple Interfaces

A composite USB device with two separate HID interfaces — one keyboard and one mouse — each with its own report descriptor, endpoint, and interface string.

## What it does

- Presents two independent HID interfaces: a keyboard (interface 0, "Keyboard Interface") and a mouse (interface 1, "Mouse Interface").
- Polls the board button every 10 ms:
  - Keyboard: while the button is held, sends the `A` keycode; releasing sends an empty report.
  - Mouse: while the button is held, moves diagonally (+5, +5).
- If the device is suspended, pressing the button issues a USB remote wakeup.
- The LED blinks to indicate USB state (250 ms not mounted, 1000 ms mounted, 2500 ms suspended).

## USB Descriptors

| Interface | Class driver |
|-----------|--------------|
| 0 | HID (keyboard) |
| 1 | HID (mouse) |

## Configuration

Notable `tusb_config.h` settings:

```c
#define CFG_TUD_HID            2 // keyboard + mouse
#define CFG_TUD_HID_EP_BUFSIZE 8
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

After flashing, the board enumerates as two distinct HID devices, a keyboard and a mouse. Press and hold the button: the host receives repeated `A` key presses and the pointer drifts toward the bottom-right.
