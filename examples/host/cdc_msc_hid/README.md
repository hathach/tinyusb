# Host: CDC/MSC/HID

A USB host example that enumerates CDC serial, Mass Storage, and HID devices and reports their activity over the debug UART.

## What it does

- CDC (`CFG_TUH_CDC`): bidirectionally bridges the debug console and the attached CDC serial device — bytes typed on the debug UART are written to the device, and bytes received from the device are echoed back to the UART. Also supports common USB-serial adapters (FTDI, CP210x, CH34x, PL2303). On mount it prints the interface info and line coding (set to 115200 8N1 on enumeration).
- MSC (`CFG_TUH_MSC`): on mount, issues a SCSI Inquiry and prints the drive's vendor/product/revision strings and its capacity (block count, block size, total MB).
- HID (`CFG_TUH_HID`): receives reports and prints keyboard keystrokes as ASCII, mouse button state and cursor movement, and any other (generic) report as raw hex.
- Blinks the board LED once per second.

## Requirements

The board must support USB host mode (provide VBUS to the connected device); some boards need an external USB-A port / host adapter.

## Configuration

Notable `tusb_config.h` settings:

```c
#define CFG_TUH_ENABLED             1
#define CFG_TUH_HUB                 1
#define CFG_TUH_CDC                 1
#define CFG_TUH_CDC_FTDI            1
#define CFG_TUH_CDC_CP210X          1
#define CFG_TUH_CDC_CH34X           1
#define CFG_TUH_CDC_PL2303          1
#define CFG_TUH_HID                 (3*CFG_TUH_DEVICE_MAX)
#define CFG_TUH_MSC                 1
#define CFG_TUH_DEVICE_MAX          (3*CFG_TUH_HUB + 1)
#define CFG_TUH_HID_EPIN_BUFSIZE    64
#define CFG_TUH_HID_EPOUT_BUFSIZE   64
#define CFG_TUH_CDC_LINE_CONTROL_ON_ENUM  (CDC_CONTROL_LINE_STATE_DTR | CDC_CONTROL_LINE_STATE_RTS)
#define CFG_TUH_CDC_LINE_CODING_ON_ENUM   { 115200, CDC_LINE_CODING_STOP_BITS_1, CDC_LINE_CODING_PARITY_NONE, 8 }
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

A FreeRTOS build is in `examples/host/cdc_msc_hid_freertos` — identical CDC/MSC/HID host behavior, running the host stack in a FreeRTOS task with a software-timer LED.

## How to use

Plug a USB keyboard, mouse, flash drive, or serial device into the board's USB host port (a USB hub also works, so several can be connected at once). Watch the debug UART: keystrokes and mouse movement appear as you use the input devices, an attached flash drive prints its inquiry/size info, and for a serial device you can type into the debug console to forward characters to it and see its output echoed back.
