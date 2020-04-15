# TinyUSB

![tinyUSB_240x100](https://user-images.githubusercontent.com/249515/62646655-f9393200-b978-11e9-9c53-484862f15503.png)

[![Build Status](https://github.com/hathach/tinyusb/workflows/Build/badge.svg)](https://github.com/hathach/tinyusb/actions) [![License](https://img.shields.io/badge/license-MIT-brightgreen.svg)](https://opensource.org/licenses/MIT) [![Coverity](https://img.shields.io/coverity/scan/458.svg)](https://scan.coverity.com/projects/tinyusb)

TinyUSB is an open-source cross-platform USB Host/Device stack for embedded system, designed to be memory-safe with no dynamic allocation and thread-safe with all interrupt events are deferred then handled in the non-ISR task function.

![tinyusb](https://user-images.githubusercontent.com/249515/49858616-f60c9700-fe27-11e8-8627-e76936352ff7.png)

```
.
├── docs            # Documentation
├── examples        # Sample with Makefile and Segger Embedded build support
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

- **Espressif:** ESP32-S2
- **MicroChip:** SAMD21, SAMD51 (device only)
- **NordicSemi:** nRF52840, nRF52833
- **Nuvoton:** NUC120, NUC121/NUC125, NUC126, NUC505
- **NXP:** 
  - LPC Series: 11Uxx, 13xx, 175x_6x, 177x_8x, 18xx, 40xx, 43xx, 51Uxx, 54xxx, 55xx
  - iMX RT Series: RT1011, RT1015, RT1021, RT1052, RT1062, RT1064
- **Sony:** CXD56
- **ST:** STM32 series: L0, F0, F1, F2, F3, F4, F7, H7 (device only)
- **TI:** MSP430
- **[ValentyUSB](https://github.com/im-tomu/valentyusb)** eptri

[Here is the list of supported Boards](docs/boards.md) that can be used with provided examples.

## Device Stack

Supports multiple device configurations by dynamically changing usb descriptors. Low power functions such like suspend, resume, and remote wakeup. Following device classes are supported:

- Communication Class (CDC)
- Human Interface Device (HID): Generic (In & Out), Keyboard, Mouse, Gamepad etc ...
- Mass Storage Class (MSC): with multiple LUNs
- Musical Instrument Digital Interface (MIDI)
- Network with RNDIS, CDC-ECM, CDC-EEM (work in progress)
- USB Test and Measurement Class (USBTMC)
- Vendor-specific class support with generic In & Out endpoints. Can be used with MS OS 2.0 compatible descriptor to load winUSB driver without INF file.
- [WebUSB](https://github.com/WICG/webusb) with vendor-specific class

## Host Stack

**Most active development is on the Device stack. The Host stack is under rework and largely untested.**

- Human Interface Device (HID): Keyboard, Mouse, Generic
- Mass Storage Class (MSC)
- Hub currently only supports 1 level of hub (due to my laziness)

## OS Abstraction layer

TinyUSB is completely thread-safe by pushing all ISR events into a central queue, then process it later in the non-ISR context task function. It also uses semaphore/mutex to access shared resources such as CDC FIFO. Therefore the stack needs to use some of OS's basic APIs. Following OSes are already supported out of the box.

- **No OS** : Disabling USB IRQ is used as way to provide mutex
- **FreeRTOS**
- **Mynewt** Due to the newt package build system, Mynewt examples are better to be on its [own repo](https://github.com/hathach/mynewt-tinyusb-example) 

## Compiler & IDE

The stack is developed with GCC compiler and should be compilable with others. The `examples` folder provides Makefile and Segger Embedded Studio build support. [Here are example build instructions](examples/readme.md).

## Getting Started

[Here are the details for getting started](docs/getting_started.md) with the stack.

## Porting

Want to help add TinyUSB support for a new MCU? Read [here](docs/porting.md) for an explanation on the low-level API needed by TinyUSB.

## License

MIT license for all TinyUSB sources `src` folder, [Full license is here](LICENSE). However, each file is individually licensed especially those in `lib` and `hw/mcu` folder. Please make sure you understand all the license term for files you use in your project.

## Uses

TinyUSB is currently used by these other projects:

* [Adafruit nRF52 Arduino](https://github.com/adafruit/Adafruit_nRF52_Arduino)
* [Adafruit nRF52 Bootloader](https://github.com/adafruit/Adafruit_nRF52_Bootloader)
* [Adafruit SAMD Arduino](https://github.com/adafruit/ArduinoCore-samd)
* [CircuitPython](https://github.com/adafruit/circuitpython)
* [MicroPython](https://github.com/micropython/micropython)
* [TinyUSB Arduino Library](https://github.com/adafruit/Adafruit_TinyUSB_Arduino)

Let me know if your project also uses TinyUSB and want to share.
