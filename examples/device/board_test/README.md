# Board Test

A minimal bring-up test that exercises a board's basic I/O without using the USB stack. Both the device and host stacks are disabled (`CFG_TUD_ENABLED`/`CFG_TUH_ENABLED` are `0`), so this is the first thing to run when porting to new hardware.

## What it does

- Blinks the on-board LED. The interval changes with the button: 1000 ms when the button is not pressed, 250 ms while it is held.
- Prints `Hello from TinyUSB` over `stdout`/UART on each blink.
- Echoes any character received on UART back out.

(No USB class interfaces are present — this example does not enumerate as a USB device.)

## Configuration

Notable `tusb_config.h` settings:

```c
#define CFG_TUD_ENABLED   0   // device stack disabled
#define CFG_TUH_ENABLED   0   // host stack disabled
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

Flash the firmware and watch the on-board LED blink. Open the board's UART (serial console) to see `Hello from TinyUSB` printed repeatedly, and type characters to see them echoed back. Press and hold the button to speed up the blink rate.
