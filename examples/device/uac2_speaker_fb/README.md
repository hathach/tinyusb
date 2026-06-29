# USB Speaker with Feedback

A stereo USB speaker (output only) that uses an explicit isochronous feedback endpoint to keep the host's sample rate in sync, adapting its descriptors to the negotiated bus speed — UAC1 at Full-Speed and UAC2 at High-Speed.

## What it does

- Enumerates as a stereo speaker (host → device), 16-bit. The configuration returned depends on link speed: a UAC1 configuration at Full-Speed and a UAC2 configuration at High-Speed, with a device-qualifier and other-speed-configuration descriptor so it works at either speed.
- Declares an explicit asynchronous feedback endpoint and reports rate via the FIFO-count feedback method (`tud_audio_feedback_params_cb`), letting the host adjust how many samples it sends so the device buffer neither under- nor over-runs.
- The audio task drains the speaker FIFO every 1 ms into a dummy buffer (the data is discarded), reading roughly one millisecond of audio per cycle with a small periodic adjustment for the 44.1 kHz and 88.2 kHz rates.
- Supports 44.1 kHz and 48 kHz at Full-Speed, plus 88.2 kHz and 96 kHz at High-Speed (UAC2). Handles UAC1 and UAC2 control requests (mute, volume, sample frequency).
- When `CFG_AUDIO_DEBUG` is enabled (default), adds a HID interface that reports debug telemetry — current sample rate, alternate setting, mute/volume, and FIFO size/count/average — every 1 ms.
- Blinks the on-board LED to indicate USB state, with a fast pattern while streaming.

## USB Descriptors

| Interface | Class driver |
|-----------|--------------|
| 0–1 | UAC2 audio (control + streaming), stereo speaker output with feedback endpoint |
| 2 | HID (vendor report) — audio debug telemetry, present only when `CFG_AUDIO_DEBUG` is enabled |

(UAC1 at Full-Speed, UAC2 at High-Speed.)

## Configuration

Notable `tusb_config.h` settings:

```c
#define CFG_TUD_AUDIO                                 1
#define CFG_AUDIO_DEBUG                               1   // adds HID debug interface
#define CFG_TUD_HID                                   1   // present when CFG_AUDIO_DEBUG
#define CFG_TUD_AUDIO_ENABLE_EP_OUT                   1   // speaker
#define CFG_TUD_AUDIO_ENABLE_FEEDBACK_EP             1
#define CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_RX            2   // stereo
#define CFG_TUD_AUDIO_FUNC_1_RESOLUTION_RX            16  // 16-bit
#define CFG_TUD_AUDIO_FUNC_1_MAX_SAMPLE_RATE_FS       48000
#define CFG_TUD_AUDIO_FUNC_1_MAX_SAMPLE_RATE_HS       96000
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

The device appears as a stereo USB speaker; select it as the system output or play to it with `aplay`/`speaker-test` (the audio is consumed and discarded). The feedback endpoint keeps the host's send rate matched to the device. With `CFG_AUDIO_DEBUG` enabled, `src/audio_debug.py` reads the HID telemetry stream and plots the FIFO level and sample rate.
