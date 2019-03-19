# TinyUSB

[![Build Status](https://travis-ci.org/hathach/tinyusb.svg?branch=master)](https://travis-ci.org/hathach/tinyusb) [![License](https://img.shields.io/badge/License-BSD%203--Clause-brightgreen.svg)](https://opensource.org/licenses/BSD-3-Clause)

TinyUSB is an open-source cross-platform USB Host/Device stack for embedded system.

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
├── tests           # Unit tests for the stack
└── tools           # Files used internally
```

## Device Stack

- Human Interface Device (HID): Keyboard, Mouse, Generic
- Communication Class (CDC)
- Mass Storage Class (MSC)

## Host Stack

**Most active development is on the Device stack. The Host stack is under rework and largely untested.**

- Human Interface Device (HID): Keyboard, Mouse, Generic
- Mass Storage Class (MSC)
- Hub currently only support 1 level of hub (due to my laziness)

## OS Abtraction layer

Currently the following OS are supported with tinyusb out of the box with a simple change of **CFG_TUSB_OS** macro.

- **No OS**
- **FreeRTOS**
- **MyNewt** (work in progress)

## Supported MCUs

The stack supports the following MCUs

- **Nordic:** nRF52840
- **NXP:** LPC11Uxx, LPC13xx, LPC175x_6x, LPC177x_8x, LPC40xx, LPC43xx
- **MicroChip:** SAMD21, SAMD51 (device only)
- **ST:** STM32F4

[Here is the list of supported Boards](hw/bsp/readme.md)

## Compiler & IDE

The stack is developed with GCC compiler, and should be compilable with others. However, it requires C99 to build with. Folder `examples` provide Makefile and Segger Embedded Studio build support.

## Getting Started

[Here is the details for getting started](docs/getting_started.md) with the stack.

## Uses

TinyUSB is currently used by these other projects:

* [Adafruit nRF52 Arduino](https://github.com/adafruit/Adafruit_nRF52_Arduino)
* [Adafruit nRF52 Bootloader](https://github.com/adafruit/Adafruit_nRF52_Bootloader)
* [CircuitPython](https://github.com/adafruit/circuitpython)

## Porting

Want to help add TinyUSB support for a new MCU? Read [here](docs/porting.md) for an explanation on the low-level API needed by TinyUSB.

## License

BSD license for all tinyusb sources [Full license is here](tinyusb/license.md) and most of the code base. However each file/folder is individually licensed especially those in `lib` and `hw/mcu` folder. Please make sure you understand all the license term for files you use in your project.
