# Video Capture (UVC)

A USB Video Class (UVC) camera that streams generated color-bar frames.

## What it does

- Presents itself as a UVC camera and streams 128x96 video at 10 fps.
- By default generates YUY2 (uncompressed) color-bar frames into a RAM frame buffer; build options select alternatives: `CFG_EXAMPLE_VIDEO_READONLY` streams fixed MJPEG images from flash (or YUY2 with `CFG_EXAMPLE_VIDEO_DISABLE_MJPEG`), and `CFG_EXAMPLE_VIDEO_BUFFERLESS` fills the payload on the fly.
- Uses an isochronous streaming endpoint by default (bulk if `CFG_TUD_VIDEO_STREAMING_BULK` is set).
- Adopts the frame interval requested by the host at stream commit.
- Blinks the board LED to indicate USB state (not mounted / mounted / suspended).

## USB Descriptors

| Interface | Class driver |
|-----------|--------------|
| 0–1 | UVC video (control + streaming) |

## Configuration

Notable `tusb_config.h` settings:

```c
#define CFG_TUD_VIDEO                       1
#define CFG_TUD_VIDEO_STREAMING            1
#define CFG_TUD_VIDEO_STREAMING_EP_BUFSIZE 256
#define CFG_TUD_VIDEO_STREAMING_BULK       0   // 0 = isochronous, 1 = bulk streaming
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

The device appears as a webcam. On Linux, find it with `v4l2-ctl --list-devices` and view it with `ffplay /dev/videoN` or any camera app. You should see scrolling color bars.
