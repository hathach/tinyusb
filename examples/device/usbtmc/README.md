# USBTMC

A USB Test & Measurement Class (USBTMC) instrument with USB488/SCPI support, behaving like a simple programmable instrument.

## What it does

- Implements a USBTMC interface with USB488 capabilities and advertises SCPI support.
- Responds to `*IDN?` with an identification string (`TinyUSB,ModelNumber,SerialNumber,FirmwareVer123456`).
- Buffers other written messages and echoes them back as the query response, after a configurable delay.
- A `delay <ms>` command adjusts that simulated response delay (0–10000 ms).
- Maintains an IEEE-488.2 status byte (MAV/SRQ), and handles trigger, clear, and bulk abort requests.
- Supports the USBTMC indicator-pulse request, which briefly pulses the board LED.
- Uses an interrupt endpoint in addition to the bulk IN/OUT endpoints.

## USB Descriptors

| Interface | Class driver |
|-----------|--------------|
| 0 | USBTMC |

## Configuration

Notable `tusb_config.h` settings:

```c
#define CFG_TUD_USBTMC                1
#define CFG_TUD_USBTMC_ENABLE_INT_EP  1
#define CFG_TUD_USBTMC_ENABLE_488     1
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

The device appears as a USBTMC test-and-measurement instrument. Talk to it with a VISA/SCPI tool (for example PyVISA). The included `visaQuery.py` opens the instrument and sends `*IDN?`:

```bash
python visaQuery.py
```
