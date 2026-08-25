# USB Audio Host Example

This example demonstrates how to use TinyUSB's USB Audio Host driver (TUH_AUDIO) to capture audio from a UAC 1.0 compatible USB microphone and echo it back to the speaker, using a WASAPI/ALSA-like high-level API. The application never touches USB interfaces, alternate settings, or endpoint addresses — it only selects supported `{format, sample_rate, channels}` configurations by stream index.

## Features

- Enumerates and mounts USB Audio Class 1.0 devices
- Discovers the device's logical streams (capture/playback) and their supported configurations (discrete tuples only)
- Configures and starts an S16_LE capture stream (48 kHz preferred, 44.1 kHz fallback; stereo preferred, mono accepted)
- Echoes captured audio to an S16_LE playback stream at the same sample rate (same channel count preferred, mono/stereo conversion otherwise)
- Frame-based FIFO API: `tuh_audio_read()` / `tuh_audio_write()` queue frames; the driver schedules transfers at the endpoint's polling interval
- Cycles the streams through three phases (5 s each): mic-only (capture, data dropped), spk-only (sine test tone), and echo (capture looped back to playback)

## Supported Devices

This example supports UAC 1.0 devices whose Type I Format descriptor lists discrete sampling frequencies (`bSamFreqType > 0`), such as:

- USB microphones
- USB headsets (mono microphone + speaker)
- USB audio interfaces

The echo needs a matching S16_LE playback stream at the capture sample rate; devices without one run capture-only. The sample rate and channel preferences are configured by the `SAMPLE_RATES` / `AUDIO_MAX_CHANNELS` macros in `src/audio_app.c` (48 kHz stereo by default). Continuous sampling-frequency ranges are exposed as a single configuration at the range's highest frequency (e.g. a 8000–48000 Hz speaker appears as 48000 Hz); non-PCM formats are rejected by the driver.

## Limitations and trade-offs

- The Feature Unit API exposes one directly connected Feature Unit per logical capture/playback stream. If several Feature Units map to the same logical stream, the first mapping is retained.
- The UAC1 `MaxPacketsOnly` endpoint attribute is not supported. OUT transfers are not padded to `wMaxPacketSize`, and padding in IN transfers is not removed from the reported audio data.

## Building

### Using CMake (recommended)

```bash
cd examples/host/audio_host
mkdir -p build && cd build
cmake -DBOARD=<your_board> -G Ninja ..
cmake --build .
```

Replace `<your_board>` with your target board name (e.g., `raspberry_pi_pico`, `stm32f407disco`, etc.)

### Using Make

```bash
cd examples/host/audio_host
make BOARD=<your_board> all
```

## Flashing

```bash
# Using CMake
ninja flash

# Using Make
make BOARD=<your_board> flash
```

## Usage

1. Build and flash the example to your board
2. Connect a USB Audio device (UAC 1.0) to the USB host port
3. Open a serial terminal to view output
4. The example will:
   - Print each stream's supported configurations when mounted
   - Look for an S16_LE capture configuration at a preferred sample rate (48 kHz first, 44.1 kHz fallback; stereo preferred, mono accepted) and configure it
   - Echo captured audio to an S16_LE playback configuration at the same sample rate (same channel count preferred, converted otherwise)
   - Drain the capture FIFO in `audio_app_task_read()` and queue the frames into the playback FIFO; a sine test tone plays on the playback stream when no capture stream is echoing
   - Cycle through the three phases (mic-only / spk-only / echo, 5 s each) with `tuh_audio_start()` / `tuh_audio_stop()`; a failed stream is restarted automatically 100 ms after the error callback

## Serial Output Example

```
TinyUSB Host USB Audio Example
Connect a USB Audio Device (UAC 1.0) to test
Audio device mounted: idx=0 addr=1
  capture stream 1 configurations: 2
    [0] format=1 rate=44100 channels=2
    [1] format=1 rate=48000 channels=2
  playback stream 0 configurations: 2
    [0] format=1 rate=44100 channels=2
    [1] format=1 rate=48000 channels=2
  Configuring 48 kHz S16_LE capture (2 channels)
  Microphone configured, starting capture
  Configuring 48 kHz S16_LE playback (2 channels)
  Speaker configured, starting playback
```

## Configuration

Edit `src/tusb_config.h` to modify:
- `CFG_TUH_AUDIO_MAX`: Maximum number of audio devices supported
- `CFG_TUH_AUDIO_EPIN_BUFSIZE`: Maximum size of one capture transfer the driver submits (configurations needing a larger per-poll-interval packet are rejected)
- `CFG_TUH_AUDIO_EPOUT_BUFSIZE`: Maximum size of one playback transfer the driver submits
- `CFG_TUH_AUDIO_STREAM_BUFSIZE`: Per-stream FIFO depth in bytes (default 1024, i.e. four 256 B packets)

## Notes

- While a stream is running, the driver keeps one isochronous transfer in flight and re-submits on completion, so transfers follow the endpoint's `bInterval`. `tuh_audio_capture_cb()` / `tuh_audio_playback_cb()` report each completed transfer; `tuh_audio_err_cb()` reports failures. The example restarts the failed stream automatically 100 ms after the error callback.
- `tuh_audio_read()` / `tuh_audio_write()` are non-blocking FIFO operations: they return the number of whole frames actually queued/read (0 when the FIFO is empty/full or the stream is not running), and `tuh_audio_read_available()` / `tuh_audio_write_available()` report the FIFO occupancy in frames.
- Isochronous transfers require the host to poll `tuh_task()` continuously; the capture FIFO absorbs short scheduling gaps, but frames are dropped when it overflows.
