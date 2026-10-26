# Host: MIDI Receive

A USB host example that receives MIDI from a connected USB-MIDI device and prints the incoming bytes over the debug UART.

## What it does

- Enumerates USB-MIDI devices (`CFG_TUH_MIDI`) and, on mount, prints the interface index, device address, and the number of RX/TX cables.
- On each received MIDI packet, reads the stream and prints the cable number followed by the raw MIDI bytes as hex.
- Blinks the board LED once per second.

## Requirements

The board must support USB host mode (provide VBUS to the connected device); some boards need an external USB-A port / host adapter.

## Configuration

Notable `tusb_config.h` settings:

```c
#define CFG_TUH_ENABLED             1
#define CFG_TUH_HUB                 1
#define CFG_TUH_DEVICE_MAX          (3*CFG_TUH_HUB + 1)
#define CFG_TUH_MIDI                CFG_TUH_DEVICE_MAX
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

Plug a USB-MIDI device (keyboard, controller, or interface) into the board's USB host port. On attach the debug UART prints the mount info; then playing notes or moving controls prints lines like `Cable 0 rx: 90 3C 7F` with the raw MIDI bytes.
