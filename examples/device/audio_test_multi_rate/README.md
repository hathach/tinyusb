# Audio Multi-Rate Microphone

A single-channel USB microphone test example that supports multiple sample rates and adapts its descriptors to the negotiated USB bus speed — UAC1 at Full-Speed and UAC2 at High-Speed.

## What it does

- Enumerates as a single-channel microphone. The configuration descriptor returned depends on link speed: a UAC1 (Audio 1.0) configuration at Full-Speed and a UAC2 (Audio 2.0) configuration at High-Speed. A device-qualifier and other-speed-configuration descriptor are provided so it works at either speed.
- Supports discrete sample rates of 32 kHz, 48 kHz and 96 kHz. At Full-Speed the rate is selected through the UAC1 endpoint sampling-frequency control; at High-Speed through the UAC2 clock-source frequency range.
- Offers a 16-bit format; the High-Speed UAC2 configuration adds a second format with 24-bit samples carried in 32-bit slots, selected via the streaming interface's alternate setting.
- An audio task generates an incrementing ramp signal sized to the current sample rate and sample width, and writes it to the IN endpoint every 1 ms. The ramp resets when the streaming interface is closed.
- Handles both UAC1 and UAC2 control requests (mute, volume, sample frequency) dispatched on the active audio version.
- Blinks the on-board LED to indicate USB state (not mounted / mounted / suspended).

## USB Descriptors

| Interface | Class driver |
|-----------|--------------|
| 0–1 | Audio (control + streaming), 1-channel microphone input — UAC1 at Full-Speed, UAC2 at High-Speed |

## Configuration

Notable `tusb_config.h` settings:

```c
#define CFG_TUD_AUDIO                                        1
#define CFG_TUD_AUDIO_FUNC_1_MAX_SAMPLE_RATE                96000
#define CFG_TUD_AUDIO_FUNC_1_N_FORMATS                      2
#define CFG_TUD_AUDIO_ENABLE_EP_IN                          1
#define CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_TX                  1
#define CFG_TUD_AUDIO_FUNC_1_FORMAT_1_N_BYTES_PER_SAMPLE_TX 2   // 16-bit in 16-bit slots
#define CFG_TUD_AUDIO_FUNC_1_FORMAT_1_RESOLUTION_RX         16
#define CFG_TUD_AUDIO_FUNC_1_FORMAT_2_N_BYTES_PER_SAMPLE_TX 4   // 24-bit in 32-bit slots (UAC2 only)
#define CFG_TUD_AUDIO_FUNC_1_FORMAT_2_RESOLUTION_RX         24
#define CFG_TUD_AUDIO_FUNC_1_EP_IN_SZ_MAX                   TU_MAX(CFG_TUD_AUDIO10_FUNC_1_FORMAT_1_EP_SZ_IN, TU_MAX(CFG_TUD_AUDIO20_FUNC_1_FORMAT_1_EP_SZ_IN, CFG_TUD_AUDIO20_FUNC_1_FORMAT_2_EP_SZ_IN))
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

The device appears as a single-channel USB microphone. On Linux, list it with `arecord -l` and record at a chosen rate with `arecord`, e.g. `arecord -D hw:CARD=MicNode -c 1 -f S16_LE -r 96000 test.wav`, trying 32000/48000/96000 to exercise rate switching. On a High-Speed host you can also select the 24-bit format (`-f S24_3LE` / `S32_LE` depending on the host). The included `src/plot_audio_samples.py` records and plots the signal.
