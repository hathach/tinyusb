# CDC + MSC

A composite USB device that exposes **two interfaces at once**:

- **CDC** — a virtual serial port (`/dev/ttyACMx`, or a COM port on Windows) that echoes back everything it receives.
- **MSC** — a small mass-storage disk that mounts as a removable drive.

This is the canonical TinyUSB example for a multi-interface (composite) device.

## What it does

- **CDC:** reads incoming data and echoes it straight back. Pressing the on-board button sends a serial-state (UART) notification to the host.
- **MSC:** presents an 8 KB FAT12 RAM disk (16 × 512-byte blocks — the smallest size Windows will mount) labelled `TinyUSB MSC`, containing a single `README.TXT`. The disk lives in RAM, so changes are not persistent.
- **LED** shows bus state: 250 ms blink = not mounted, 1000 ms = mounted, 2500 ms = suspended.

## USB Descriptors

| Interface | Class driver |
|-----------|--------------|
| 0–1 | CDC (virtual serial) |
| 2 | MSC (mass storage) |

## Configuration

Notable `tusb_config.h` settings:

```c
#define CFG_TUD_CDC              1
#define CFG_TUD_MSC              1
#define CFG_TUD_CDC_RX_BUFSIZE   (TUD_OPT_HIGH_SPEED ? 512 : 64)
#define CFG_TUD_CDC_TX_BUFSIZE   (TUD_OPT_HIGH_SPEED ? 512 : 64)
#define CFG_TUD_MSC_EP_BUFSIZE   512
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

A FreeRTOS build is in `examples/device/cdc_msc_freertos` — identical CDC + MSC behavior, with the device, CDC, and LED-blink work split across FreeRTOS tasks.

## Try it

- **Serial:** open the CDC port (`screen /dev/ttyACM0`, PuTTY, …) and type — characters are echoed back.
- **Storage:** a small `TinyUSB MSC` drive appears; open `README.TXT` to confirm it mounted.
