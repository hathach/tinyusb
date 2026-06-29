# Dual-Channel Video Capture (UVC)

A USB Video Class (UVC) device that exposes two independent video streaming functions on a single device.

## What it does

- Presents two UVC camera functions, both 128x96 at 10 fps.
- Stream 0 sends YUY2 (uncompressed) color bars; stream 1 sends MJPEG color-bar frames.
- Uses bulk streaming endpoints (`CFG_TUD_VIDEO_STREAMING_BULK`).
- Adopts the frame interval requested by the host at stream commit, per stream.
- Blinks the board LED to indicate USB state (not mounted / mounted / suspended).

## USB Descriptors

| Interface | Class driver |
|-----------|--------------|
| 0–1 | UVC video (control + streaming) — YUY2 |
| 2–3 | UVC video (control + streaming) — MJPEG |

## Configuration

Notable `tusb_config.h` settings:

```c
#define CFG_TUD_VIDEO                       2   // 2 video control interfaces
#define CFG_TUD_VIDEO_STREAMING            2   // 2 video streaming interfaces
#define CFG_TUD_VIDEO_STREAMING_EP_BUFSIZE 256
#define CFG_TUD_VIDEO_STREAMING_BULK       1   // bulk streaming endpoints
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

Two webcam devices appear on the host. On Linux, list them with `v4l2-ctl --list-devices` and open each with `ffplay /dev/videoN` or a camera app — one shows YUY2 color bars, the other MJPEG color bars.
