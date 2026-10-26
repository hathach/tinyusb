# Dual: USB Host Device-Info → CDC Device

A dual-role bridge: the board acts as a USB host on one root-hub port and a USB device on another at the same time, printing information about any device plugged into the host port out to the PC over a virtual serial port.

## What it does

- Host side: enumerates any USB device attached to the host port (including devices behind a hub) and captures its device descriptor during enumeration.
- Device side: presents a single CDC (virtual serial) interface to the PC.
- Data flow: when a device is mounted on the host port, the board reads its descriptor fields (VID/PID, bcdUSB, class/subclass/protocol, max packet size, configuration count) plus the manufacturer, product, and serial-number string descriptors (UTF-16 converted to UTF-8) and prints them as formatted text to the CDC serial port. Mount and unmount events are also reported. Optionally runs the host/device/main work as FreeRTOS tasks.

## USB Descriptors

(DEVICE-side interfaces only)

| Interface | Class driver |
|-----------|--------------|
| 0–1 | CDC (virtual serial) |

## Configuration

Notable `tusb_config.h` settings:

```c
#define CFG_TUD_CDC              1
#define CFG_TUD_CDC_RX_BUFSIZE   (TUD_OPT_HIGH_SPEED ? 512 : 64)
#define CFG_TUD_CDC_TX_BUFSIZE   (TUD_OPT_HIGH_SPEED ? 512 : 256)
#define CFG_TUH_HUB              1
#define CFG_TUH_DEVICE_MAX       (CFG_TUH_HUB ? 4 : 1)
#define CFG_TUH_ENUMERATION_BUFSIZE 256
```

## Requirements

The board needs two usable USB ports: one acting as host (for the device to inspect) and one acting as device (to the PC).

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

1. Connect the board's device port to the PC and open the CDC serial port it enumerates (e.g. `/dev/ttyACMx` on Linux, a COM port on Windows).
2. Plug any USB device into the board's host port.
3. The serial terminal shows a `mounted device N` line followed by a full device-descriptor dump (IDs, strings, and field values). Removing the device prints an `unmounted device N` line.
