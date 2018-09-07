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
- [License](#license)
- [How Can I Help](#how-can-i-help)
	- [Donate Time](#donate-time)
	- [Donate Money](#donate-money)

<!-- END doctoc generated TOC please keep comment here to allow auto update -->

tinyusb is an open-source (BSD-licensed) USB Host/Device/OTG stack for embedded micro-controllers, especially ARM MCUs. It is designed to be user-friendly in term of configuration and out-of-the-box running experience.

In addition to running without an RTOS, tinyusb is an OS-awared stack that can run across RTOS vendors. For the purpose of eliminating bugs as soon as possible, the stack is developed using [Test-Driven Development (TDD)](tests/readme.md) approach. More documents and API reference can be found at http://docs.tinyusb.org

![tinyusb diagram](http://docs.tinyusb.org/images/tinyusb_overview.png)

## Features ##

### Host ###

- HID Mouse
- HID Keyboard
- HID Generic (comming soon)
- Communication Device Class (CDC)
- Mass Storage Class (MSC)
- Hub currnetly only support 1 level of hub (due to my laziness)

### Device ###

- HID Mouse
- HID Keyboard
- HID Generic (comming soon)
- Communication Class (CDC)
- Mass Storage Class (MSC)

### RTOS ###

Currently the following OS are supported with tinyusb out of the box with a simple change of CFG_TUSB_OS macro.

- **None OS**
- **FreeRTOS**
- **CMSIS RTX**

### Toolchains ###

You can compile with any of following toolchains, however, the stack requires C99 to build with

- lpcxpresso
- Keil MDK
- IAR Workbench

### Supported MCUs ###

The stack supports the following MCUs

  - LPC11uxx
  - LPC13uxx (12 bit ADC)
  - LPC175x_6x
  - LPC43xx

[Here is the list of supported Boards](boards/readme.md) in the code base

## Getting Started ##

[Here is the details for getting started](doxygen/getting_started.md) with the stack.

## License ##

BSD license for most of the code base, but each file is individually licensed especially those in *vendor* folder. Please make sure you understand all the license term for files you use in your project. [Full license is here](tinyusb/license.md)
