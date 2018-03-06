# Host Demos #

<!-- START doctoc generated TOC please keep comment here to allow auto update -->
<!-- DON'T EDIT THIS SECTION, INSTEAD RE-RUN doctoc TO UPDATE -->
**Table of Contents**

- [Prerequisites](#prerequisites)
- [Hub](#hub)
- [Human Interface Device (HID)](#human-interface-device-hid)
	- [Keyboard](#keyboard)
	- [Mouse](#mouse)
- [Mass Storage Class Device (MSC)](#mass-storage-class-device-msc)
- [Communication Class Device (CDC)](#communication-class-device-cdc)
	- [Serial](#serial)

<!-- END doctoc generated TOC please keep comment here to allow auto update -->

host application code is store at *demos/host/src* containing

File  | Description
----- | -------------
main.c | Initialization (board, stack) and a RTOS task scheduler call or just simple a indefinite loop for non OS to invoke class-specific tasks.
tusb_config.h | tinyusb stack configuration.
app_os_prio.h | RTOS task priority definitions
Class-specific | Application files for supported classes.

Firstly connect your ansi-escaped supported terminal (see below) to the UART of the  evaluation board since all the user interaction happens there. The default UART configure is 
- Baudrate = 115200
- Data = 8 bits
- Parity = none
- Stop = 1 bit
- Flow control = none

The demo will start with the greeting of enabled classes and chosen RTOS then start to blink an LED at 1 Hz. When any of supported devices is plugged or un-plugged either directly or via a hub, demo application will print out a short message. Notes: if usb device is composed of supported and unsupported classes such as a keyboard and a camera, the keyboard interface is still mounted as usual without any issues.

**NOTE** Host demo is quite power hunger, especially when you plug a hub with several devices on it. Make sure you have enough power before filing any bugs. 

## Prerequisites ##

In order to run application demo, you would need

- A [supported development board](../../boards/readme.md).
- A supported toolchain: LPCXpresso, Keil, IAR.
- A [ANSI escape](http://en.wikipedia.org/wiki/ANSI_escape_code) supported terminal such as [Tera Term](http://en.sourceforge.jp/projects/ttssh2/) to demonstrate properly.

## Hub

Hub is internally handled by tinyusb stack, application code does not have to worry on how to get hub operated. However, application must be written to handle multiple devices simultaneously.

## Human Interface Device (HID)

### Keyboard

When a keyboard device is enumerated, any keys pressed on the device will be echoed to the UART terminal.

![Host Keyboard Demo](http://docs.tinyusb.org/images/demo_host_keyboard.png)

### Mouse

When a mouse device is enumerated, any movements or clicks on the device will be reflected on the terminal. Only make sure the terminal's windows is active on your host OS and it supports ANSI escape code.

![Host Mouse Demo](http://docs.tinyusb.org/images/demo_host_mouse.png)

## Mass Storage Class Device (MSC)

Mass storage demo application only supports device with FAT file system by the help of fatfs (source in *vendor/fatfs*). In addition, it also includes a minimal command line interface *msc_cli.c* to allow user to navigate and execute several basic file operations. When mass storage device is plugged, CLI will be activated, type "help" for the usage. Some are

Command  | Description
----- | -------------
cls | clear screen 
ls | List information of the FILEs, only supported current directory.
cd | change the current directory.
cat | display contents of a file.
cp | copy files to another location.
mkdir | create a directory, if it does not already exist.
mv | rename or move a directory or a file.
rm | remove (delete) an empty directory or file

Furthermore, the demo's CLI also supports multiple mass storage devices. You could *cd* between disk drives, and copy a file from one to another.

![Host MSC Demo](http://docs.tinyusb.org/images/demo_host_msc.png)

## Communication Class Device (CDC)

CDC has several subclass, currently tinyusb only supports the popular *Abstract Control Model (ACM)*

### Serial

A virtual serial of CDC-ACM is supported such as those built with tinyusb device stack. The host demo literally does 2 things
- Echo back any thing it received from the device to the terminal
- Receive data from terminal and send it to device.

Notes: FTDI is a vendor-specific class, which is not currently supported.

![Host Serial Demo](http://docs.tinyusb.org/images/demo_host_serial.png)
