# Network Web Server (lwIP)

A USB virtual network adapter that runs a small lwIP stack on the device, serving DHCP, DNS, and a web page over USB.

## What it does

- Brings up a USB network interface. Depending on the target MCU the build is either CDC-NCM (the default for most MCUs) or a dual RNDIS + CDC-ECM device that offers two configurations and lets the host pick its preferred one (Windows uses RNDIS, macOS uses CDC-ECM, Linux works with either). The NCM build also ships a BOS / Microsoft OS 2.0 descriptor so Windows auto-loads its NCM driver.
- The device takes IP address 192.168.7.1 and runs a DHCP server (handing out 192.168.7.2–192.168.7.4), a DNS server (resolves `tiny.usb`), and an HTTP server.
- Pressing the board button toggles the network link state (up/down) to simulate an Ethernet cable being unplugged/plugged.
- On higher-RAM MCUs it also starts an iperf TCP server for throughput testing.
- Blinks the board LED to indicate USB state (not mounted / mounted / suspended).

## Requirements

- Depends on the lwIP TCP/IP stack, fetched as a dependency (e.g. `python3 tools/get_deps.py`).

## USB Descriptors

| Interface | Class driver |
|-----------|--------------|
| 0–1 | Network (CDC-NCM, or CDC-ECM/RNDIS) |

## Configuration

Notable `tusb_config.h` settings:

```c
// Network driver selection: NCM is the default; USE_ECM defaults to 0 but is
// forced to 1 on some MCUs (LPC15xx/40xx/51uxx/54, SAMD21/SAML2x, STM32F0/F1).
#define CFG_TUD_ECM_RNDIS     USE_ECM            // 0 by default -> ECM/RNDIS off
#define CFG_TUD_NCM           (1 - CFG_TUD_ECM_RNDIS)  // 1 by default -> NCM on

// NCM tuning (see ncm.h for performance notes)
#define CFG_TUD_NCM_IN_NTB_MAX_SIZE  2048
#define CFG_TUD_NCM_OUT_NTB_MAX_SIZE 4096        // 2048 on low-RAM MCUs
#define CFG_TUD_NCM_OUT_NTB_N        1
#define CFG_TUD_NCM_IN_NTB_N         1
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

A new USB network interface appears on the host. It is normally assigned an address in 192.168.7.x by the device's DHCP server (otherwise give the host NIC a static 192.168.7.x address), then browse to http://192.168.7.1 to load the served web page.
