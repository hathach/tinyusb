# Dual: USB HID Host → CDC Device

A dual-role bridge: the board acts as a USB host on one root-hub port and a USB device on another at the same time, forwarding input from a connected HID keyboard/mouse to the host PC as text over a virtual serial port.

## What it does

- Host side: enumerates a connected USB HID device (boot keyboard and/or mouse), including devices behind a hub, and requests its interrupt reports.
- Device side: presents a single CDC (virtual serial) interface to the PC.
- Data flow: incoming keyboard reports are converted from HID keycodes to ASCII (with shift handling) and written to the CDC serial port; mouse reports are formatted as `[addr] LMR x y wheel` text lines. HID mount/unmount events are also announced over the CDC port. Data received from the PC on the CDC port is read and discarded (LED control is a TODO).

## USB Descriptors

(DEVICE-side interfaces only)

| Interface | Class driver |
|-----------|--------------|
| 0–1 | CDC (virtual serial) |

## Configuration

Notable `tusb_config.h` settings:

```c
#define CFG_TUD_CDC              1
#define CFG_TUH_HUB              1
#define CFG_TUH_DEVICE_MAX       (CFG_TUH_HUB ? 4 : 1)
#define CFG_TUH_HID              (3*CFG_TUH_DEVICE_MAX)
#define CFG_TUH_ENUMERATION_BUFSIZE 256
```

## Requirements

The board needs two usable USB ports: one acting as host (for the HID device) and one acting as device (to the PC).

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

1. Plug a USB keyboard or mouse into the board's host port.
2. Connect the board's device port to the PC and open the CDC serial port it enumerates (e.g. `/dev/ttyACMx` on Linux, a COM port on Windows).
3. Type on the keyboard: the characters appear in the serial terminal. Move/click the mouse: lines like `[1] L-- 3 -2 0` appear. Connecting or removing a HID device prints a mount/unmount message.
