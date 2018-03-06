# Device Demos #

<!-- START doctoc generated TOC please keep comment here to allow auto update -->
<!-- DON'T EDIT THIS SECTION, INSTEAD RE-RUN doctoc TO UPDATE -->
**Table of Contents**

- [Prerequisites](#prerequisites)
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
tusb_descriptors(c,h) | contains all the required usb descriptors for all combination of supported classes. And definition of stack-required variable *tusbd_descriptor_pointers*.
app_os_prio.h | RTOS task priority definitions
Class-specific | Application files for supported classes.

The demo will start with the greeting of enabled classes and chosen RTOS then start to blink an LED at 1 Hz.

## Prerequisites ##

In order to run application demo, you would need

- A [supported development board](../../boards/readme.md) with at least a button for mouse, keyboard demo.
- A supported toolchain: LPCXpresso, Keil, IAR.
- A decent terminal such as [Tera Term](http://en.sourceforge.jp/projects/ttssh2/) for CDC Serial.

## Human Interface Device (HID)

### Keyboard

After the board get enumerated successfully, you can try to press some buttons while opening notepad. It should get some characters out accordingly. In addition, when you press Capslock or Numlock key on your own pc's keyboard, the LED will blink faster twice. This demonstrates that keyboard application can receive Set Report via control pipe. 

Notes: The very same buttons may also used by Mouse application. You can get the mouse moving and character, should you enable both.

### Mouse

After the board get enumerated successfully, you can try to press some buttons. The mouse's cursor should move accordingly.

Notes: The very same buttons may also used by Keyboard application. You can get the mouse moving and character, should you enable both.

## Mass Storage Class Device (MSC)

This class is very simple, as soon as the demo work, you could open the demo drive. Inside, you should find a *README.TXT* file which contains a few lines of descriptions. The demo drive's format is FAT12 and only has 8KB, which is the smallest possible to work with most host OS (Windows/Linux).

Notes: The entire disk contents ( 8KB ) is located on MCU's SRAM if possible (such as lpc43xx, lpc175x_6x). For  MCU (lpc11u, lpc13u) that cannot afford that, the contents is instead on internal Flash which make the demo drive is read-only. 

## Communication Class Device (CDC)

CDC has several subclass, currently tinyusb only supports the popular *Abstract Control Model (ACM)*

### Serial

The virtual COM is also as easy as MSC. Except that we need to "install" driver for the first plug if your host OS is Windows, Linux should be able to work right away.

**Install Driver for Windows**

Actually Windows already has the needed driver to operate with virtual serial of CDC-ACM, the *usbser*. However, since it also use Abstract Control Model for other purposes, it requires us to tell exactly which VendorID/ProductID comibination should be used as a virtual serial. The demo's src folder includes *WinCDCdriver.inf* to do just that.

Firstly open *Device Manager*, we should find our board under "Other devices", right click on it and choose "Update Driver Software..."

![Serial Install Driver](http://docs.tinyusb.org/images/demo_serial_driver.png)

Then choose "Browse my computer for driver software" then navigate to the device demo's *src* and click next. Since I am nowhere near a known publisher to Microsoft, it will warn you with a scary dialog. With all your trust in me, click next and hope that nothing harmful will ever happen and we are done with the driver.

![Serial Install Driver](http://docs.tinyusb.org/images/demo_serial_driver2.png)

**Testing Demo**

Connect to the "tinyusb Serial Port" with your terminal application, and start to type. You should get echo back as the CDC serial application demo is written to transmit what it received. Notes: if Windows/terminal don't realize serial port (especially if you unplugged device without disconnect previously), simply unplug and plug device again. This is a "known feature" of Windows's usbser. 

![Serial Connect](http://docs.tinyusb.org/images/demo_cdc_connect.png)