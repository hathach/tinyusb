# TinyUSB

![tinyUSB_240x100](https://user-images.githubusercontent.com/249515/62646655-f9393200-b978-11e9-9c53-484862f15503.png)

[![Build Status](https://travis-ci.org/hathach/tinyusb.svg?branch=master)](https://travis-ci.org/hathach/tinyusb) [![License](https://img.shields.io/badge/license-MIT-brightgreen.svg)](https://opensource.org/licenses/MIT)

TinyUSB is an open-source cross-platform USB Host/Device stack for embedded system. It is designed to be memory-safe with no dynamic allocation and thread-safe with all interrupt events are deferred then handled in the stack's task function.

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

## Supported MCUs

The stack supports the following MCUs

- **Nordic:** nRF52840
- **NXP:** LPC11Uxx, LPC13xx, LPC175x_6x, LPC177x_8x, LPC18xx, LPC40xx, LPC43xx, LPC51Uxx
- **MicroChip:** SAMD21, SAMD51 (device only)
- **ST:** STM32 series: L0, F0, F2, F3, F4, F7, H7 (device only)

[Here is the list of supported Boards](docs/boards.md) that can be used with provided examples.

## Device Stack

Support multiple device configurations by dynamically changing usb descriptors. Low power functions such as suspend, resume and remote wakeup. Following device classes are supported:

- Communication Class (CDC)
- Human Interface Device (HID): Generic (In & Out), Keyboard, Mouse, Gamepad etc ...
- Mass Storage Class (MSC): with multiple LUNs
- Musical Instrument Digital Interface (MIDI)
- Vendor-specific class support with generic In & Out endpoints. Can be used with MS OS 2.0 compatible descriptor to load winUSB driver without INF file.
- [WebUSB](https://github.com/WICG/webusb) with vendor-specific class

## Host Stack

**Most active development is on the Device stack. The Host stack is under rework and largely untested.**

- Human Interface Device (HID): Keyboard, Mouse, Generic
- Mass Storage Class (MSC)
- Hub currently only support 1 level of hub (due to my laziness)

## OS Abtraction layer

TinyUSB is completely thread-safe by pushing all ISR events into a central queue, then process it later in the non-ISR context. It also uses semphore/mutex to access shared resource such as CDC FIFO. Therefore the stack needs to use some of OS's basic APIs. Following OSes are already supported out of the box.

- **No OS** : Disabling USB IRQ is used as way to provide mutex
- **FreeRTOS**
- **Mynewt** Due to the newt package build system, Mynewt examples are better to be on its [own repo](https://github.com/hathach/mynewt-tinyusb-example) 

## Compiler & IDE

The stack is developed with GCC compiler, and should be compilable with others. Folder `examples` provide Makefile and Segger Embedded Studio build support. [Here is instruction to build example](examples/readme.md).

## Getting Started

[Here is the details for getting started](docs/getting_started.md) with the stack.

## Porting

Want to help add TinyUSB support for a new MCU? Read [here](docs/porting.md) for an explanation on the low-level API needed by TinyUSB.

## License

MIT license for all TinyUSB sources `src` folder, [Full license is here](LICENSE). However each file is individually licensed especially those in `lib` and `hw/mcu` folder. Please make sure you understand all the license term for files you use in your project.

## Uses

TinyUSB is currently used by these other projects:

* [Adafruit nRF52 Arduino](https://github.com/adafruit/Adafruit_nRF52_Arduino)
* [Adafruit nRF52 Bootloader](https://github.com/adafruit/Adafruit_nRF52_Bootloader)
* [Adafruit SAMD Arduino](https://github.com/adafruit/ArduinoCore-samd)
* [CircuitPython](https://github.com/adafruit/circuitpython)
* [TinyUSB Arduino Library](https://github.com/adafruit/Adafruit_TinyUSB_Arduino)

Let's me know if your project also uses TinyUSB and want to share.
