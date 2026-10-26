# Audio Test Microphone

A minimal USB Audio Class 2.0 (UAC2) microphone that streams a generated test signal, useful for verifying the audio device stack end to end.

## What it does

- Enumerates as a UAC2 microphone with 1 input channel at 48 kHz, 16-bit.
- An audio task fills a buffer with a continuously incrementing 16-bit ramp counter and writes it to the IN endpoint every 1 ms, simulating data from an I2S source. The counter resets when the streaming interface is closed.
- Handles UAC2 control requests: feature-unit mute/volume, clock-source sample-rate and clock-validity, and input-terminal connector.
- Blinks the on-board LED to indicate USB state (not mounted / mounted / suspended).

## USB Descriptors

| Interface | Class driver |
|-----------|--------------|
| 0–1 | UAC2 audio (control + streaming), 1-channel microphone input |

## Configuration

Notable `tusb_config.h` settings:

```c
#define CFG_TUD_AUDIO                                 1
#define CFG_TUD_AUDIO_FUNC_1_SAMPLE_RATE             48000
#define CFG_TUD_AUDIO_ENABLE_EP_IN                    1
#define CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_TX            1
#define CFG_TUD_AUDIO_FUNC_1_N_BYTES_PER_SAMPLE_TX    2   // 16-bit
#define CFG_TUD_AUDIO_EP_SZ_IN                        TUD_AUDIO_EP_SIZE(TUD_OPT_HIGH_SPEED, CFG_TUD_AUDIO_FUNC_1_SAMPLE_RATE, CFG_TUD_AUDIO_FUNC_1_N_BYTES_PER_SAMPLE_TX, CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_TX)
#define CFG_TUD_AUDIO_FUNC_1_EP_IN_SZ_MAX            CFG_TUD_AUDIO_EP_SZ_IN
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

A FreeRTOS build is in `examples/device/audio_test_freertos` — identical single-channel UAC2 test microphone, with the device, audio, and LED-blink work split across FreeRTOS tasks.

## Try it

The device appears as a single-channel USB microphone. On Linux, list it with `arecord -l` and capture with `arecord` (for example `arecord -D hw:CARD=MicNode -c 1 -f S16_LE -r 48000 test.wav`); the recorded samples form a rising ramp. The included `src/plot_audio_samples.py` records and plots the signal (requires `sounddevice` and `matplotlib`).
