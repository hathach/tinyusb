# MSC Dual LUN

A USB Mass Storage device that presents two logical units (LUNs), each a small in-RAM FAT12 disk.

## What it does

- Exposes two LUNs, each a 8 KB FAT12 RAM disk containing a `README.TXT` file (`TinyUSB 0` and `TinyUSB 1` volumes).
- Both disks are read/write by default, so host changes are written back to RAM (unless built read-only).
- Pressing the board button marks LUN1 as "not ready" to simulate removable media being absent (e.g. SD card removed).
- Reports a custom SCSI inquiry: vendor `TinyUSB`, product `Mass Storage`, revision `1.0`.
- Runs under no-OS, FreeRTOS or ThreadX (USB device and LED blink as separate tasks under an RTOS).
- LED blink rate indicates bus state (not mounted / mounted / suspended).

## USB Descriptors

| Interface | Class driver |
|-----------|--------------|
| 0 | MSC (mass storage) |

## Configuration

Notable `tusb_config.h` settings:

```c
#define CFG_TUD_MSC               1
#define CFG_TUD_MSC_EP_BUFSIZE    512
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

After plugging in, two removable drives appear on the host, each containing a `README.TXT`. Open or modify the files to exercise reads and writes. Press the board button to make the second drive report "not ready" (medium not present).
