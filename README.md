# tinyusb

tinyusb is an cross-platform open-source USB Host/Device stack for embedded system.

![tinyusb](https://user-images.githubusercontent.com/249515/49858616-f60c9700-fe27-11e8-8627-e76936352ff7.png)

Folder  | Description
-----   | -------------
docs    | Documentation
examples| Sample applications are kept with Makefile and Segger Embedded build support
hw/bsp  | Source files of supported boards
hw/mcu  | Low level mcu core & peripheral drivers (e.g CMSIS )
lib     | Source files from 3rd party such as freeRTOS, fatfs etc ...
src     | All sources files for tinyusb stack itself.
tests   | Unit tests for the stack
tools   | Files used internally

## Device Stack

- Human Interface Device (HID): Keyboard, Mouse, Generic
- Communication Class (CDC)
- Mass Storage Class (MSC)

## Host Stack

** Most active development is on the Device stack. The Host stack is under rework and largely untested.**

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

  - **NXP:** LPC11Uxx, LPC13xx, LPC175x_6x, LPC177x_8x, LPC40xx, LPC43xx
  - **MicroChip:** SAMD21, SAMD51 (device only)
  - **Nordic:** nRF52840

[Here is the list of supported Boards](hw/bsp/readme.md)

## Compiler & IDE

The stack is developed with GCC compiler, and should be compilable with others. However, it requires C99 to build with. Folder `examples` provide Makefile and Segger Embedded Studio build support.

## Getting Started

[Here is the details for getting started](docs/getting_started.md) with the stack.

## Uses

TinyUSB is currently used by these other projects:

* [Adafruit nRF52 Arduino](https://github.com/adafruit/Adafruit_nRF52_Arduino)
* [CircuitPython](https://github.com/adafruit/circuitpython)
* [nRF52840 UF2 Bootloader](https://github.com/adafruit/Adafruit_nRF52_Bootloader)

## Porting

Want to help add TinyUSB support for a new MCU? Read [here](docs/porting.md) for an explanation on the low-level API needed by TinyUSB.

## License

BSD license for all tinyusb sources [Full license is here](tinyusb/license.md) and most of the code base. However each file/folder is individually licensed especially those in `lib` and `hw/mcu` folder. Please make sure you understand all the license term for files you use in your project.
