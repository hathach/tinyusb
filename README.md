# TinyUSB

![TinyUSB](https://user-images.githubusercontent.com/2847802/108847382-a0a6a580-75ad-11eb-96d9-280c79389281.png)

[![Build Status](https://github.com/hathach/tinyusb/workflows/Build/badge.svg)](https://github.com/hathach/tinyusb/actions) [![License](https://img.shields.io/badge/license-MIT-brightgreen.svg)](https://opensource.org/licenses/MIT)

TinyUSB is an open-source cross-platform USB Host/Device stack for embedded system, designed to be memory-safe with no dynamic allocation and thread-safe with all interrupt events are deferred then handled in the non-ISR task function.

![tinyusb](https://user-images.githubusercontent.com/249515/49858616-f60c9700-fe27-11e8-8627-e76936352ff7.png)

```
.
├── docs            # Documentation
├── examples        # Sample with Makefile build support
├── hw
│   ├── bsp         # Supported boards source files
│   └── mcu         # Low level mcu core & peripheral drivers
├── lib             # Sources from 3rd party such as freeRTOS, fatfs ...
├── src             # All sources files for TinyUSB stack itself.
├── test            # Unit tests for the stack
└── tools           # Files used internally
```

## Contributors

Special thanks to all the people who spent their precious time and effort to help this project so far. Check out the 
[CONTRIBUTORS.md](CONTRIBUTORS.md) file for the list of all contributors and their awesome work for the stack.

## Supported MCUs

The stack supports the following MCUs:

- **Dialog:** DA1469x
- **Espressif:** ESP32-S2, ESP32-S3
- **MicroChip:** SAMD11, SAMD21, SAMD51, SAME5x, SAMG55
- **NordicSemi:** nRF52833, nRF52840
- **Nuvoton:** NUC120, NUC121/NUC125, NUC126, NUC505
- **NXP:**
  - iMX RT Series: RT1011, RT1015, RT1021, RT1052, RT1062, RT1064
  - Kinetis: KL25
  - LPC Series: 11u, 13, 15, 17, 18, 40, 43, 51u, 54, 55
- **Raspberry Pi:** RP2040
- **Renesas:** RX63N
- **Silabs:** EFM32GG12
- **Sony:** CXD56
- **ST:** STM32 series: L0, F0, F1, F2, F3, F4, F7, H7 both FullSpeed and HighSpeed
- **TI:** MSP430
- **[ValentyUSB](https://github.com/im-tomu/valentyusb)** eptri

[Here is the list of supported Boards](docs/boards.md) that can be used with provided examples.

## Device Stack

Supports multiple device configurations by dynamically changing usb descriptors. Low power functions such like suspend, resume, and remote wakeup. Following device classes are supported:

- Audio Class 2.0 (UAC2) still work in progress
- Bluetooth Host Controller Interface (BTH HCI)
- Communication Class (CDC)
- Device Firmware Update (DFU): DFU mode (WIP) and Runtinme 
- Human Interface Device (HID): Generic (In & Out), Keyboard, Mouse, Gamepad etc ...
- Mass Storage Class (MSC): with multiple LUNs
- Musical Instrument Digital Interface (MIDI)
- Network with RNDIS, CDC-ECM (work in progress)
- USB Test and Measurement Class (USBTMC)
- Vendor-specific class support with generic In & Out endpoints. Can be used with MS OS 2.0 compatible descriptor to load winUSB driver without INF file.
- [WebUSB](https://github.com/WICG/webusb) with vendor-specific class

If you have special need, `usbd_app_driver_get_cb()` can be used to write your own class driver without modifying the stack. Here is how RPi team add their reset interface [raspberrypi/pico-sdk#197](https://github.com/raspberrypi/pico-sdk/pull/197)

## Host Stack

**Most active development is on the Device stack. The Host stack is under rework and largely untested.**

- Human Interface Device (HID): Keyboard, Mouse, Generic
- Mass Storage Class (MSC)
- Hub currently only supports 1 level of hub (due to my laziness)

## OS Abstraction layer

TinyUSB is completely thread-safe by pushing all ISR events into a central queue, then process it later in the non-ISR context task function. It also uses semaphore/mutex to access shared resources such as CDC FIFO. Therefore the stack needs to use some of OS's basic APIs. Following OSes are already supported out of the box.

- **No OS**
- **FreeRTOS**
- **Mynewt** Due to the newt package build system, Mynewt examples are better to be on its [own repo](https://github.com/hathach/mynewt-tinyusb-example) 

## Getting Started

[Here are the details for getting started](docs/getting_started.md) with the stack.

## Porting

Want to help add TinyUSB support for a new MCU? Read [here](docs/porting.md) for an explanation on the low-level API needed by TinyUSB.

## License

MIT license for all TinyUSB sources `src` folder, [Full license is here](LICENSE). However, each file is individually licensed especially those in `lib` and `hw/mcu` folder. Please make sure you understand all the license term for files you use in your project.

## Uses

TinyUSB is currently used by these other projects:

- [Adafruit nRF52 Arduino](https://github.com/adafruit/Adafruit_nRF52_Arduino)
- [Adafruit nRF52 Bootloader](https://github.com/adafruit/Adafruit_nRF52_Bootloader)
- [Adafruit SAMD Arduino](https://github.com/adafruit/ArduinoCore-samd)
- [CircuitPython](https://github.com/adafruit/circuitpython)
- [Espressif IDF](https://github.com/espressif/esp-idf)
- [MicroPython](https://github.com/micropython/micropython)
- [mynewt](https://mynewt.apache.org)
- [Raspberry Pi Pico SDK](https://github.com/raspberrypi/pico-sdk)
- [TinyUF2 Bootloader](https://github.com/adafruit/tinyuf2)
- [TinyUSB Arduino Library](https://github.com/adafruit/Adafruit_TinyUSB_Arduino)
