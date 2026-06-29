# MIDI Test

A USB MIDI device that continuously plays a fixed melody, useful for verifying USB MIDI enumeration and streaming.

## What it does

- Sends a repeating sequence of note-on / note-off messages (cable 0, channel 1) about every 286 ms.
- Drains any incoming MIDI packets (read and discarded) so the host sender never blocks on a full input port.
- Blinks the board LED to indicate USB state (not mounted / mounted / suspended).

## USB Descriptors

| Interface | Class driver |
|-----------|--------------|
| 0–1 | MIDI (Audio Control + MIDI Streaming) |

## Configuration

Notable `tusb_config.h` settings:

```c
#define CFG_TUD_MIDI              1
#define CFG_TUD_MIDI_RX_BUFSIZE   (TUD_OPT_HIGH_SPEED ? 512 : 64)
#define CFG_TUD_MIDI_TX_BUFSIZE   (TUD_OPT_HIGH_SPEED ? 512 : 64)
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

A FreeRTOS build is in `examples/device/midi_test_freertos` — identical MIDI melody playback, with the device, MIDI, and LED-blink work split across FreeRTOS tasks.

## Try it

The device shows up as a MIDI port. On Linux, list ports with `aseqdump -l`, then watch the incoming stream with `aseqdump -p <client:port>` (you should see a steady run of note on/off events), or route the port into a synth such as FluidSynth/qsynth to hear the melody.
