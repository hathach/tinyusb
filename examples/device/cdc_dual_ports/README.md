# CDC Dual Ports

Enumerates as a USB device with two independent CDC virtual serial ports.

## What it does

- Exposes two CDC virtual COM ports.
- Echoes back any data received on either port to **both** ports: the first port echoes as lower case, the second as upper case.
- Sends a UART state notification (toggles the DSR line) on each press of the on-board button.
- Resets the board into the bootloader when the first port is opened at 1200 bps and then disconnected (touch-1200 trigger).
- Blinks the on-board LED: 250 ms when unmounted, 1000 ms when mounted.

## USB Descriptors

| Interface | Class driver |
|-----------|--------------|
| 0–1 | CDC (virtual serial port 1) |
| 2–3 | CDC (virtual serial port 2) |

## Configuration

Notable `tusb_config.h` settings:

```c
#define CFG_TUD_CDC               2   // two CDC ports
#define CFG_TUD_CDC_RX_BUFSIZE   (TUD_OPT_HIGH_SPEED ? 512 : 64)
#define CFG_TUD_CDC_TX_BUFSIZE   (TUD_OPT_HIGH_SPEED ? 512 : 64)
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

After flashing, two serial ports appear on the host (e.g. `/dev/ttyACM0` and `/dev/ttyACM1` on Linux). Open either one in a terminal and type: characters come back lower-cased on the first port and upper-cased on the second.
