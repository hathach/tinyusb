# CDC + MSC Throughput

A deliberately minimal CDC + MSC composite device for measuring **pure USB bulk
throughput** — the ceiling set by the USB link and the TinyUSB driver, with no
backing storage or per-byte work in the way.

## How it measures the ceiling, not storage

- **MSC** advertises a 1 GiB logical disk (2 Mi × 512-byte blocks) but has no real
  backing store. Writes are discarded; reads zero-fill only the low LBAs the host
  scans during enumeration (partition table / GPT header) and otherwise return
  whatever is already in the transfer buffer — so no `memset`/copy cost skews the result.
- **CDC** drains its RX in `tud_cdc_rx_cb` and sources TX from a static zero filler,
  so `dd` can push data in either direction over `/dev/ttyACMx`.

The 1 GiB capacity lets `dd` run long enough for the rate to stabilise; high-speed peripherals show the most headroom.

## USB Descriptors

| Interface | Class driver |
|-----------|--------------|
| 0–1 | CDC (virtual serial) |
| 2 | MSC (mass storage) |

## Configuration

Notable `tusb_config.h` settings (tuned for throughput):

```c
#define CFG_TUD_CDC              1
#define CFG_TUD_MSC              1
#define CFG_TUD_MSC_EP_BUFSIZE   (TUD_OPT_HIGH_SPEED  ? 4096 : 1024)  // large MSC bulk buffer
#define CFG_TUD_CDC_RX_EPSIZE    (TUD_OPT_HIGH_SPEED ? 2*512 : 2*64)
#define CFG_TUD_CDC_TX_EPSIZE    CFG_TUD_CDC_RX_EPSIZE
#define CFG_TUD_CDC_RX_BUFSIZE   (TUD_OPT_HIGH_SPEED ? 2*512 : 2*64)
#define CFG_TUD_CDC_TX_BUFSIZE   CFG_TUD_CDC_RX_BUFSIZE
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

## Measuring throughput (Linux)

The MSC disk appears as a raw block device (e.g. `/dev/sdX`); the CDC port as `/dev/ttyACMx`.

```bash
# MSC read (device → host)
sudo dd if=/dev/sdX of=/dev/null bs=1M count=256 iflag=direct

# MSC write (host → device, discarded)
sudo dd if=/dev/zero of=/dev/sdX bs=1M count=256 oflag=direct

# CDC read (device → host)
dd if=/dev/ttyACM0 of=/dev/null bs=64k count=4096
```

> Pick the right `/dev/sdX` carefully — writing to the wrong block device destroys data.
