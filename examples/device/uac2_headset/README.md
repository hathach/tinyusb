# USB Audio Headset

A USB audio headset that combines a stereo speaker output and a mono microphone input, adapting its descriptors to the negotiated bus speed — UAC1 at Full-Speed and UAC2 at High-Speed.

## What it does

- Enumerates as a headset with a stereo speaker (host → device) and a mono microphone (device → host). The configuration returned depends on link speed: a UAC1 configuration at Full-Speed and a UAC2 configuration at High-Speed, with a device-qualifier and other-speed-configuration descriptor so it works at either speed.
- Loops audio back: the audio task reads the speaker stream, mixes the two channels down to one, and writes it to the microphone IN endpoint, so whatever is played to the headset is heard back on its microphone.
- Supports 44.1 kHz and 48 kHz, with a 16-bit format and (UAC2) a 24-bit-in-32-bit format selected by the streaming interface's alternate setting.
- Handles UAC1 and UAC2 control requests (mute, volume, sample frequency) dispatched on the active audio version.
- Reads the on-board button to toggle the speaker volume (0 dB / -30 dB) and notifies the host through the audio interrupt endpoint.
- Blinks the on-board LED to indicate USB state, with a fast pattern while the speaker is streaming.

## USB Descriptors

| Interface | Class driver |
|-----------|--------------|
| 0 | Audio control |
| 1 | Audio streaming — speaker output (OUT) |
| 2 | Audio streaming — microphone input (IN) |

(UAC1 at Full-Speed, UAC2 at High-Speed.)

## Configuration

Notable `tusb_config.h` settings:

```c
#define CFG_TUD_AUDIO                                 1
#define CFG_TUD_AUDIO_ENABLE_INTERRUPT_EP             1
#define CFG_TUD_AUDIO_ENABLE_EP_IN                    1   // microphone
#define CFG_TUD_AUDIO_ENABLE_EP_OUT                   1   // speaker
#define CFG_TUD_AUDIO_FUNC_1_N_FORMATS                2
#define CFG_TUD_AUDIO_FUNC_1_MAX_SAMPLE_RATE          48000
#define CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_TX            1   // mic (mono)
#define CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_RX            2   // speaker (stereo)
#define CFG_TUD_AUDIO_FUNC_1_FORMAT_1_RESOLUTION_TX   16  // 16-bit
#define CFG_TUD_AUDIO_FUNC_1_FORMAT_2_RESOLUTION_TX   24  // 24-bit in 32-bit slots (UAC2)
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

The device appears as both a USB playback (speaker) and capture (microphone) device. Play audio to the speaker (e.g. `aplay -D ...` or select it as the system output) and record from the microphone with `arecord` — you should hear the played audio looped back. Press the on-board button to toggle the volume between full scale and -30 dB and watch the host volume control follow.
