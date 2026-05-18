# MIDI 2.0 Song Sender

USB MIDI 2.0 Device example that plays "Twinkle Twinkle Little Star" using
native UMP (Universal MIDI Packet) format with full MIDI 2.0 expression.

## MIDI 2.0 Features Demonstrated

- 16-bit Velocity (vs 7-bit MIDI 1.0)
- 32-bit Control Change values
- 32-bit Pitch Bend (vs 14-bit MIDI 1.0)
- 32-bit Channel Pressure (Aftertouch)
- 32-bit Poly Pressure (Per-Note Aftertouch)
- Per-Note Management (MIDI 2.0 exclusive)
- Program Change with Bank Select
- JR Timestamps

## USB Descriptor

The device exposes both USB-MIDI 1.0 (Alt Setting 0) and USB-MIDI 2.0 (Alt Setting 1)
as required by the USB-MIDI 2.0 specification. A MIDI 2.0 capable host (e.g. Windows
MIDI Services) will select Alt Setting 1 for native UMP transport. Legacy hosts use
Alt Setting 0 with automatic MIDI 1.0 fallback.

## Hardware

- Any RP2040 board with USB (e.g. Raspberry Pi Pico)
- LED on GPIO 25: steady = playing, slow blink = waiting for host

## Building

```bash
mkdir build && cd build
cmake -DBOARD=raspberry_pi_pico -DPICO_SDK_FETCH_FROM_GIT=on -G Ninja ..
cmake --build .
```

## Flashing

Hold BOOTSEL, connect USB, drag `midi2_device.uf2` to the RPI-RP2 drive.

## Testing

**Linux:**
```bash
aseqdump -p "MIDI 2.0 Device"
```

**Windows (MIDI 2.0 native):**
```powershell
midi endpoint list
midi endpoint monitor
```

## Song Data

Twinkle Twinkle Little Star in C major, 120 BPM. Six phrases with dynamic
shaping (pp to ff crescendo and back), pitch bend vibrato on sustained notes,
and channel/poly pressure for expression. All values use genuine MIDI 2.0
resolution with no 7-bit equivalent.
