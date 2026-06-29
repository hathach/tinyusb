# CDC + UAC2

This example provides a composite CDC + UAC2 device.

## Use Cases

- The CDC + UAC2 composite device happens to be important, especially in the
  amateur radio community.

  Modern radios (`rigs`) like Icom IC-7300 + IC-705 expose a sound card and a
  serial device (`composite device`) to the computer over a single USB cable.
  This allows for Audio I/O and CAT control over a single USB cable which is
  very convenient.

  By including and maintaining this example in TinyUSB repository, we enable
  the amateur radio community to build (`homebrew`) radios with similar
  functionality as the (expensive) commercial rigs.

- https://digirig.net/digirig-mobile-rev-1-9/ is a digital interface for
  interfacing radios (that lack an inbuilt digital interface) with computers.
  Digirig Mobile works brilliantly (is OSS!) and is a big improvement over
  traditional digital interfaces (like the SignaLink USB Interface). By using a
  CDC + UAC2 composite device, we can simplify the
  Digirig Mobile schematic, drastically reduce the manufacturing cost, and
  (again) enable the homebrewers community to homebrew a modern digital interface
  with ease themselves.


## Build Steps

```bash
mkdir build && cd build
cmake -DBOARD=raspberry_pi_pico ..
cmake --build .
```

Make:

```bash
make BOARD=raspberry_pi_pico all
```

## USB Descriptors

| Interface | Class driver |
|-----------|--------------|
| 0–2 | UAC2 audio (control, speaker streaming, mic streaming) |
| 3–4 | CDC (virtual serial) |

## Configuration

Notable `tusb_config.h` settings:

```c
#define CFG_TUD_CDC                            1
#define CFG_TUD_AUDIO                          1
#define CFG_TUD_AUDIO_FUNC_1_N_FORMATS         2       // two audio formats
#define CFG_TUD_AUDIO_FUNC_1_MAX_SAMPLE_RATE   96000   // 48000 on Renesas RX
#define CFG_TUD_CDC_RX_BUFSIZE   (TUD_OPT_HIGH_SPEED ? 512 : 64)
#define CFG_TUD_CDC_TX_BUFSIZE   (TUD_OPT_HIGH_SPEED ? 512 : 64)
```

## How to use

After flashing, the device enumerates as both a USB sound card and a serial port:

- **Audio:** appears as a standard UAC2 sound card — list it with `arecord -l` / `aplay -l` on Linux, or find it in the OS sound settings. Audio is sourced/sunk by the example's `audio_task`.
- **Serial:** the CDC port shows up as `/dev/ttyACMx` (Linux/macOS) or a COM port (Windows); open it with any terminal.

In the amateur-radio setup, digital-mode software (WSJT-X, fldigi, …) uses the sound card for audio and the serial port for CAT/PTT control — both over the one cable.
