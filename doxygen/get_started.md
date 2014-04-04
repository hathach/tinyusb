# Getting Started #

<!-- START doctoc generated TOC please keep comment here to allow auto update -->
<!-- DON'T EDIT THIS SECTION, INSTEAD RE-RUN doctoc TO UPDATE -->
**Table of Contents**

- [Download](#download)
- [Import and Build](#import-and-build)
	- [Prerequisites](#prerequisites)
- [Configure demo](#configure-demo)

<!-- END doctoc generated TOC please keep comment here to allow auto update -->

## Download ##

tinyusb uses github as online repository https://github.com/hathach/tinyusb since it is the best place for open source project. 

If you are using Linux, you already know how to what to do. But If Windows is your OS, I would suggest to install [git](http://git-scm.com/) and front-end gui such as [tortoisegit](http://code.google.com/p/tortoisegit) to begin with.

After downloading/cloning, the code base is composed of

Folder  | Description
-----   | -------------
*boards*  | Source files of supported boards
*demos*   | Source & project files for demonstration application
*mcu*     | Low level mcu core & peripheral drivers (e.g CMSIS )
*tests*   | Unit tests for the stack
*tinyusb* | All sources files for tinyusb stack itself.
*vendor*  | Source files from 3rd party such as freeRTOS, fatfs etc ...

*repo/demos* is the folder where all the application & project files are located. There are demos for both device and hosts. For each, there are different projects for each of supported RTOS. 

## Prerequisites ##

In order to build and run application demo, you would need

- A [supported development board](../../boards/readme.md)
- A supported toolchain: LPCXpresso, Keil, IAR.

## Configure demo ##

Application demo is written to have the code excluded if its required option is not enabled in [tusb_config.h](). Some of combination may exceed the 32KB limit of IAR/Keil so you may want to re-configure to disable some class support, decrease TUSB_CFG_DEBUG or increase the compiler optimization level.

In addition, there are some configuration you can change such as

- CFG_UART_BAUDRATE in board.h
- CFG_PRINTF_TARGET in the specific board header (e.g board_ea4357.h) to either Semihost, Uart, or SWO.

\subpage md_boards_readme
\subpage md_doxygen_started_build