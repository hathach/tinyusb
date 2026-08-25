# WebUSB Serial

A WebUSB device that acts as a web serial bridge, reachable directly from a WebUSB-capable browser (e.g. Chrome). It also exposes a standard CDC virtual serial port, and bridges traffic between the two.

## What it does

- Exposes a Vendor (WebUSB) interface plus a CDC virtual serial port.
- Echoes any data received on either the WebUSB or the CDC interface back to **both** of them.
- Serves a WebUSB landing-page URL (`example.tinyusb.org/webusb-serial/index.html`) via a BOS descriptor, so a supporting browser can offer to open the page.
- Provides a Microsoft OS 2.0 descriptor so Windows auto-binds the WinUSB driver to the WebUSB interface.
- Treats a vendor control request as a connect/disconnect signal from the browser; lights the on-board LED solid while the web serial is connected.
- Blinks the on-board LED to reflect bus state: 250 ms unmounted, 1000 ms mounted, 2500 ms suspended.

## USB Descriptors

| Interface | Class driver |
|-----------|--------------|
| 0–1 | CDC (virtual serial) |
| 2 | Vendor (WebUSB) |

## Configuration

Notable `tusb_config.h` settings:

```c
#define CFG_TUD_CDC               1
#define CFG_TUD_VENDOR            1
#define CFG_TUD_CDC_RX_BUFSIZE    (TUD_OPT_HIGH_SPEED ? 512 : 64)
#define CFG_TUD_CDC_TX_BUFSIZE    (TUD_OPT_HIGH_SPEED ? 512 : 64)
#define CFG_TUD_VENDOR_RX_BUFSIZE (TUD_OPT_HIGH_SPEED ? 512 : 64)
#define CFG_TUD_VENDOR_TX_BUFSIZE (TUD_OPT_HIGH_SPEED ? 512 : 64)
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

After flashing, open the landing page (`https://example.tinyusb.org/webusb-serial/index.html`) in a WebUSB-capable browser such as Chrome, click **Connect**, and select the device — the on-board LED lights solid once connected. Characters typed in the web page are echoed back, and are also mirrored to the CDC serial port (e.g. `/dev/ttyACM0`) and vice versa.

On Linux/macOS you may need to install the udev rules from `examples/device/99-tinyusb-examples.rules` for the browser to access the device.
