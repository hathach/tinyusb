# Host: Bare API

A USB host example that drives the TinyUSB host stack through its low-level API directly, without the higher-level class-driver application layer.

## What it does

- Enumerates any attached device and, on mount, fetches and prints the full device descriptor (VID/PID, USB version, class, max packet size, etc.) over the debug UART.
- Reads and prints the manufacturer, product, and serial-number string descriptors (UTF-16 to UTF-8).
- Fetches and walks the configuration descriptor with a small hand-written parser.
- For any HID interface found, opens its interrupt IN endpoint with the raw endpoint API (`tuh_edpt_open` / `tuh_edpt_xfer`) and continuously prints the incoming HID reports as raw hex bytes.
- Blinks the board LED once per second.

## Requirements

The board must support USB host mode (provide VBUS to the connected device); some boards need an external USB-A port / host adapter.

## Configuration

Notable `tusb_config.h` settings:

```c
#define CFG_TUH_ENABLED             1
#define CFG_TUH_HUB                 1
#define CFG_TUH_DEVICE_MAX          (3*CFG_TUH_HUB + 1)
#define CFG_TUH_ENUMERATION_BUFSIZE 256
#define CFG_TUH_API_EDPT_XFER       1
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

Plug any USB device (a HID keyboard/mouse works well) into the board's USB host port. On attach you should see `Device attached` followed by the full device/string/configuration descriptor dump on the debug UART. If the device exposes a HID interface, its interrupt-IN reports are printed as hex as they arrive (e.g. when you press a key or move the mouse).
