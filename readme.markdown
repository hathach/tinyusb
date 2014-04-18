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

Currently the following OS are supported with tinyusb out of the box with a simple change of TUSB_CFG_OS macro.

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

## How Can I Help ##

If you find my little USB stack is useful and want to give something back

### Donate Time ###

You can contribute your time by helping with programming, testing and filing bug reports, improving documentation. Or simply by using tinyusb, giving me some feedback on how to improve it and telling others about it. 

### Donate Money ###

If you don't have time but still want to help, then please consider making a financial donation. This will help to pay the (mostly coffee) bills and motivate me to continue working on tinyusb. You can do so using the donation button, or contact me for other payment methods.

[//]: # (\htmlonly)
<a href="https://pledgie.com/campaigns/24694"><img alt="Click here to lend your support to tinyusb donation and make a donation at pledgie.com" src="https://pledgie.com/campaigns/24694.png?skin_name=chrome" border="0"></a>
[//]: # (\endhtmlonly)


