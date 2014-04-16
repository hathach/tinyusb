# Host Demos #

<!-- START doctoc generated TOC please keep comment here to allow auto update -->
<!-- DON'T EDIT THIS SECTION, INSTEAD RE-RUN doctoc TO UPDATE -->
**Table of Contents**

- [Human Interface Device (HID)](#human-interface-device-hid)
	- [Keyboard](#keyboard)
	- [Mouse](#mouse)
- [Mass Storage Class Device (MSC)](#mass-storage-class-device-msc)
- [Communication Class Device (CDC)](#communication-class-device-cdc)
	- [Serial](#serial)

<!-- END doctoc generated TOC please keep comment here to allow auto update -->

device application code is store at *demos/device/src* containing

File  | Description
----- | -------------
main.c | Initialization (board, stack) and a RTOS task scheduler call or just simple a indefinite loop for non OS to invoke class-specific tasks.
tusb_config.h | tinyusb stack configuration.
app_os_prio.h | RTOS task priority definitions
Class-specific | Application files for supported classes.

The demo will start with the greeting of enabled classes and chosen RTOS then start to blink an LED at 1 Hz.

## Prerequisites ##

In order to run application demo, you would need

- A [supported development board](../../boards/readme.md) with at least a button for mouse, keyboard demo.
- A supported toolchain: LPCXpresso, Keil, IAR.
- A [ANSI escape](http://en.wikipedia.org/wiki/ANSI_escape_code) supported terminal such as [Tera Term](http://en.sourceforge.jp/projects/ttssh2/) to demonstrate properly.

## Human Interface Device (HID)

### Keyboard



### Mouse

## Mass Storage Class Device (MSC)

## Communication Class Device (CDC)

### Serial
