# Host: MIDI 2.0

A minimal USB host example that receives MIDI from a connected USB-MIDI device and prints it over the debug UART, using the USB-MIDI 2.0 host driver (UMP).

## What it does

- Enumerates USB-MIDI devices (`CFG_TUH_MIDI2`) and, on mount, prints the negotiated protocol (MIDI 1.0 / MIDI 2.0) and the number of RX/TX cables.
- Receives Universal MIDI Packets (UMP) and prints each one, decoding common Channel Voice messages — Note On/Off, Control Change, Program Change, Channel Pressure, Pitch Bend — for both MIDI 2.0 and MIDI 1.0 message types, and falling back to a raw hex dump for anything else.

## Requirements

The board must support USB host mode (provide VBUS to the connected device); some boards need an external USB-A port / host adapter.

## Configuration

Notable `tusb_config.h` settings:

```c
#define CFG_TUH_ENABLED             1
#define CFG_TUH_HUB                 1
#define CFG_TUH_DEVICE_MAX          (3*CFG_TUH_HUB + 1)
#define CFG_TUH_MIDI2               CFG_TUH_DEVICE_MAX
#define CFG_TUH_MIDI2_RX_BUFSIZE    512
#define CFG_TUH_MIDI2_TX_BUFSIZE    512
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

Plug a USB-MIDI device (keyboard, controller, or interface) into the board's USB host port. On attach the debug UART prints the mount/descriptor info; then playing notes or moving controls on the device prints the decoded MIDI messages.
