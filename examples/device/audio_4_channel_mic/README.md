# Audio 4-Channel Microphone

A USB Audio Class 2.0 (UAC2) microphone that streams four channels of locally generated test audio to the host.

## What it does

- Enumerates as a UAC2 microphone with 4 input channels at 48 kHz, 16-bit.
- On startup fills a 1 ms buffer with four distinct waveforms — channel 0 a sawtooth, channel 1 an inverted sawtooth, channel 2 a square wave, channel 3 a sine wave.
- An audio task writes that buffer to the IN endpoint every 1 ms, simulating data arriving from an I2S source.
- Handles UAC2 control requests: feature-unit mute/volume, clock-source sample-rate and clock-validity, and input-terminal connector. The endpoint uses IN flow control.
- Blinks the on-board LED to indicate USB state (not mounted / mounted / suspended).

## USB Descriptors

| Interface | Class driver |
|-----------|--------------|
| 0–1 | UAC2 audio (control + streaming), 4-channel microphone input |

## Configuration

Notable `tusb_config.h` settings:

```c
#define CFG_TUD_AUDIO                                 1
#define CFG_TUD_AUDIO_FUNC_1_SAMPLE_RATE             48000
#define CFG_TUD_AUDIO_ENABLE_EP_IN                    1
#define CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_TX            4
#define CFG_TUD_AUDIO_FUNC_1_N_BYTES_PER_SAMPLE_TX    2   // 16-bit
#define CFG_TUD_AUDIO_EP_IN_FLOW_CONTROL              1
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

A FreeRTOS build is in `examples/device/audio_4_channel_mic_freertos` — identical UAC2 4-channel microphone, with the device, audio (1 ms IN-endpoint writes), and LED-blink work split across FreeRTOS tasks.

## Try it

The device appears as a USB microphone with four channels. On Linux, list it with `arecord -l` and capture with `arecord` (for example `arecord -D hw:CARD=MicNode4Ch -c 4 -f S16_LE -r 48000 test.wav`). The included `src/plot_audio_samples.py` records and plots the generated waveforms (requires `sounddevice` and `matplotlib`).
