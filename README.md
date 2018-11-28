# tinyusb #

<!-- START doctoc generated TOC please keep comment here to allow auto update -->
<!-- DON'T EDIT THIS SECTION, INSTEAD RE-RUN doctoc TO UPDATE -->
**Table of Contents**

- [Features](#features)
	- [Host](#host)
	- [Device](#device)
	- [RTOS](#rtos)
	- [Supported MCUs](#supported-mcus)
	- [Toolchains](#toolchains)
- [Getting Started](#getting-started)
- [Uses](#uses)
- [Porting](#porting)
- [License](#license)

<!-- END doctoc generated TOC please keep comment here to allow auto update -->

tinyusb is an open-source (BSD-licensed) USB Host/Device/OTG stack for embedded micro-controllers, especially ARM MCUs. It is designed to be user-friendly in term of configuration and out-of-the-box running experience.

In addition to running without an RTOS, tinyusb can run across multiple RTOS vendors. More documents and API reference can be found at http://docs.tinyusb.org

![tinyusb diagram](http://docs.tinyusb.org/images/tinyusb_overview.png)

## Features ##

### Device ###

- HID Mouse
- HID Keyboard
- HID Generic
- Communication Class (CDC)
- Mass Storage Class (MSC)

### Host ###

** Most active development is on the Device stack. The Host stack is largely untested.**

- HID Mouse
- HID Keyboard
- HID Generic (coming soon)
- Communication Device Class (CDC)
- Mass Storage Class (MSC)
- Hub currently only support 1 level of hub (due to my laziness)

### RTOS ###

Currently the following OS are supported with tinyusb out of the box with a simple change of CFG_TUSB_OS macro.

- **None OS**
- **FreeRTOS**
- **MyNewt**

### Toolchains ###

You can compile with any of following toolchains, however, the stack requires C99 to build with

- GCC
- lpcxpresso
- Keil MDK
- IAR Workbench

### Supported MCUs ###

The stack supports the following MCUs

  - LPC11uxx
  - LPC13uxx (12 bit ADC)
  - LPC175x_6x
  - LPC43xx
  - MicroChip SAMD21
  - MicroChip SAMD51
  - Nordic nRF52840

[Here is the list of supported Boards](boards/readme.md) in the code base

## Getting Started ##

[Here is the details for getting started](doxygen/getting_started.md) with the stack.

## Uses ##

TinyUSB is currently used by these other projects:

* [nRF52840 UF2 Bootloader](https://github.com/adafruit/Adafruit_nRF52_Bootloader)
* [CircuitPython](https://github.com/adafruit/circuitpython)

## Porting ##

Want to help add TinyUSB support for a new MCU? Read [here](doxygen/porting.md) for an explanation on the low-level API needed by TinyUSB.

## License ##

BSD license for most of the code base, but each file is individually licensed especially those in *vendor* folder. Please make sure you understand all the license term for files you use in your project. [Full license is here](tinyusb/license.md)
