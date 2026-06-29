# Host: HID Controller

A USB host example that reads a USB game controller / gamepad (a HID device) and prints its inputs over the debug UART.

## What it does

- Enumerates HID devices (`CFG_TUH_HID`) and, on mount, prints the device's VID/PID.
- Decodes reports from explicitly supported controllers — Sony DualShock 4 and a few compatible PS4 pads (Hori FC4, Hori PS4 Mini, ASW GG xrd) — printing the joystick axes (x, y, z, rz), D-pad direction, and pressed buttons (Square/Cross/Circle/Triangle, L1/R1/L2/R2, Share/Option/L3/R3, PS, touchpad click). Output is only printed when the report changes meaningfully.
- Sends a periodic rumble output report back to the DualShock 4, with the motor intensities driven by the L2/R2 analog triggers.
- Blinks the board LED once per second.

Note: events are only shown for the explicitly supported controllers above; other HID devices enumerate but their reports are not decoded.

## Requirements

The board must support USB host mode (provide VBUS to the connected device); some boards need an external USB-A port / host adapter.

## Configuration

Notable `tusb_config.h` settings:

```c
#define CFG_TUH_ENABLED             1
#define CFG_TUH_HUB                 0
#define CFG_TUH_DEVICE_MAX          (3*CFG_TUH_HUB + 1)
#define CFG_TUH_HID                 (3*CFG_TUH_DEVICE_MAX)
#define CFG_TUH_HID_EP_BUFSIZE      64
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

## How to use

Plug a supported USB game controller (e.g. a Sony DualShock 4) into the board's USB host port. Move the sticks and press buttons — the changes are printed on the debug UART, and squeezing the L2/R2 triggers makes the controller rumble.
