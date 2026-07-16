# USB Audio Host Example

This example demonstrates how to use TinyUSB's USB Audio Host driver (TUH_AUDIO) to communicate with a UAC 1.0 compatible USB Audio Device.

## Features

- Enumerates and mounts USB Audio Class 1.0 devices
- Receives audio data from IN endpoint (e.g., microphone)
- Sends audio data to OUT endpoint (e.g., speaker)
- Sets sampling frequency via control requests
- Demonstrates isochronous transfer handling

## Supported Devices

This example supports any UAC 1.0 compliant USB Audio device, such as:
- USB microphones
- USB speakers/headphones
- USB audio interfaces

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
   - Print device information when mounted
   - Set sampling frequency to 48kHz
   - Receive audio samples from the device (IN endpoint)
   - Send test sine wave audio to the device (OUT endpoint)

## Serial Output Example

```
TinyUSB Host USB Audio Example
Connect a USB Audio Device (UAC 1.0) to test
Audio device mounted: idx=0, daddr=1
  --- Microphone ---
    IN EP: 0x81 (max size: 192)
    Input Terminal: ID=1, Type=0x0201, Channels=1
    Format Type: 1, Channels: 1, SubFrameSize: 2, BitResolution: 16
    Sampling Freq: Discrete, count=4
      Freq[0]: 44100 Hz
      Freq[1]: 48000 Hz
      Freq[2]: 96000 Hz
      Freq[3]: 192000 Hz
  --- Speaker ---
    OUT EP: 0x02 (max size: 192)
    Output Terminal: ID=2, Type=0x0301
    Format Type: 1, Channels: 2, SubFrameSize: 2, BitResolution: 16
    Sampling Freq: Continuous range 8000 Hz - 48000 Hz
  Feature Unit: ID=3, SourceID=1
  Setting IN sampling frequency to 48000 Hz
  Setting OUT sampling frequency to 48000 Hz
  Sampling frequency set OK, ready for isochronous transfer
```

## Configuration

Edit `src/tusb_config.h` to modify:
- `CFG_TUH_AUDIO_MAX`: Maximum number of audio devices supported
- `CFG_TUH_AUDIO_EPIN_BUFSIZE`: IN endpoint buffer size
- `CFG_TUH_AUDIO_EPOUT_BUFSIZE`: OUT endpoint buffer size

## Notes

- This example uses isochronous transfers which require precise timing
- For production applications, synchronize audio transfers with the device's audio clock
- The example sends a simple sine wave for testing; replace with actual audio data in real applications
