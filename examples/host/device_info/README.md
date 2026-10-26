# Host: Device Info

A USB host example that enumerates any attached device and prints its descriptor information over the debug UART. No class drivers are enabled — only the hub driver, so it works with any kind of device.

## What it does

- On mount of any device, fetches and prints the full device descriptor: VID/PID, USB version, device class/subclass/protocol, max packet size, bcdDevice, and number of configurations.
- Reads and prints the manufacturer, product, and serial-number string descriptors (UTF-16 to UTF-8), falling back to a placeholder serial when none is present.
- Blinks the board LED, with a faster pattern while no device is mounted.
- Builds on either the bare main loop or a FreeRTOS task, depending on `CFG_TUSB_OS`.

## Requirements

The board must support USB host mode (provide VBUS to the connected device); some boards need an external USB-A port / host adapter.

## Configuration

Notable `tusb_config.h` settings:

```c
#define CFG_TUH_ENABLED             1
#define CFG_TUH_HUB                 1
#define CFG_TUH_DEVICE_MAX          (3*CFG_TUH_HUB + 1)
#define CFG_TUH_ENUMERATION_BUFSIZE 256
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

Plug any USB device into the board's USB host port. The debug UART prints the device's `ID vvvv:pppp`, serial number, and the decoded device descriptor (a hub lets you attach several devices, each printed in turn). Unplugging the device prints a removal message.
