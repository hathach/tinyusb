# DFU

A USB Device Firmware Upgrade (DFU mode) device with two firmware partitions, demonstrating download, upload and manifestation.

## What it does

- Exposes a DFU interface with two alternate settings, one per simulated partition: alt 0 `FLASH`, alt 1 `EEPROM`.
- Download (host to device): prints each received byte to stdout and immediately reports flashing complete.
- Upload (device to host): returns a fixed string per partition (`Hello world from TinyUSB DFU! - Partition 0/1`), single block only.
- Reports per-partition poll timeouts during download: 1 ms for FLASH (alt 0), 100 ms for EEPROM (alt 1).
- Logs manifestation, abort and detach events.
- Provides a BOS / Microsoft OS 2.0 descriptor so Windows binds the WinUSB driver.
- LED blink rate indicates bus state (not mounted / mounted / suspended).

## USB Descriptors

| Interface | Class driver |
|-----------|--------------|
| 0 | DFU (two alternate settings) |

## Configuration

Notable `tusb_config.h` settings:

```c
#define CFG_TUD_DFU               1
#define CFG_TUD_DFU_XFER_BUFSIZE  (TUD_OPT_HIGH_SPEED ? 512 : 64)
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

List the device in DFU mode:

```bash
dfu-util -l
```

Download firmware to a partition (a text file works well for this demo):

```bash
dfu-util -d cafe -a 0 -D firmware.bin    # partition 0 (FLASH)
dfu-util -d cafe -a 1 -D firmware.bin    # partition 1 (EEPROM)
```

Upload from a partition back to the host:

```bash
dfu-util -d cafe -a 0 -U readback.bin
dfu-util -d cafe -a 1 -U readback.bin
```

Downloaded bytes are echoed on the device's stdout; uploads return the partition's fixed string.
