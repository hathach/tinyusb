# Dynamic Configuration

Demonstrates returning different device and configuration descriptors at enumeration time, selected by the on-board button. The button is sampled when the host requests the device descriptor, so the device enumerates as one of two completely different personalities depending on whether the button is held while plugging in.

## What it does

- Reads the on-board button when the host fetches the device descriptor to choose the active configuration.
- **Button not pressed** — enumerates as CDC + MIDI:
  - CDC echoes back received data (and appends a newline after each carriage return); prints a banner when the terminal connects.
  - MIDI continuously plays a fixed note sequence, and drains/discards any incoming MIDI.
- **Button pressed** — enumerates as MSC: presents a small read/write FAT12 RAM disk containing a `README.TXT` file.
- Blinks the on-board LED to reflect bus state: 250 ms unmounted, 1000 ms mounted, 2500 ms suspended.

## USB Descriptors

Two alternate configurations are served, chosen by the button at enumeration.

Button not pressed (CDC + MIDI):

| Interface | Class driver |
|-----------|--------------|
| 0–1 | CDC (virtual serial) |
| 2–3 | MIDI |

Button pressed (MSC):

| Interface | Class driver |
|-----------|--------------|
| 0 | MSC (mass storage) |

## Configuration

Notable `tusb_config.h` settings (both personalities share one config):

```c
#define CFG_TUD_CDC              1
#define CFG_TUD_MSC              1
#define CFG_TUD_MIDI             1
#define CFG_TUD_CDC_RX_BUFSIZE   (TUD_OPT_HIGH_SPEED ? 512 : 64)
#define CFG_TUD_CDC_TX_BUFSIZE   (TUD_OPT_HIGH_SPEED ? 512 : 64)
#define CFG_TUD_MIDI_RX_BUFSIZE  (TUD_OPT_HIGH_SPEED ? 512 : 64)
#define CFG_TUD_MIDI_TX_BUFSIZE  (TUD_OPT_HIGH_SPEED ? 512 : 64)
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

## Try it

Plug the board in normally: a serial port and a MIDI device appear on the host. Now hold the on-board button while plugging in (or while resetting): the device instead enumerates as a removable drive with a `README.TXT` file. The two personalities use different product IDs so the host treats them as distinct devices.
