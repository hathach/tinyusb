# DFU Runtime

A minimal device that exposes a DFU runtime interface, advertising to the host that it can be switched into DFU (bootloader) mode.

## What it does

- Exposes a single DFU runtime interface alongside normal operation.
- Its descriptor clears `bitWillDetach`, so the host follows DFU_DETACH with a USB reset, and TinyUSB invokes `tud_dfu_runtime_reboot_to_dfu_cb()` on that reset. The callback does not actually reboot into a bootloader; instead it speeds up the LED blink as an indicator (this example is intentionally minimal).
- Provides a BOS / Microsoft OS 2.0 descriptor so Windows binds the WinUSB driver.
- LED blink rate indicates bus state (not mounted / mounted / suspended / reset after DFU detach).

## USB Descriptors

| Interface | Class driver |
|-----------|--------------|
| 0 | DFU Runtime |

## Configuration

Notable `tusb_config.h` settings:

```c
#define CFG_TUD_DFU_RUNTIME 1
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

List the device; it appears in Runtime mode:

```bash
dfu-util -l
```

Request a switch to DFU mode:

```bash
dfu-util -e
```

This sends a DETACH request and then resets the device. Since the example is minimal it does not enter a real bootloader: the reset switches the LED to a fast blink, and the host then re-enumerates the same runtime device, whose mount callback restores the mounted blink rate, so the fast blink shows only briefly.
