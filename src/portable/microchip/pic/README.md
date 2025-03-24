# Microchip PIC Chipidea FS Driver

This driver adds support for Microchip PIC microcontrollers with full-speed Chipidea USB peripheral to the TinyUSB stack. It supports the following families:

- PIC32MX (untested)
- PIC32MM
- PIC32MK (untested)
- PIC24FJ
- PIC24EP (untested)
- dsPIC33EP (untested)

Currently only the device mode is supported.


## Important Notes

### Handling of shared VBUS & GPIO pin

Some PICs have the USB VBUS pin bonded with a GPIO pin in the chip package. This driver does **NOT** handle the potential conflict between the VBUS and GPIO functionalities.

Developers must ensure that the GPIO pin is tristated when the VBUS pin is managed by the USB peripheral in order to prevent damaging the chip.

This design choice allows developers the flexibility to use the GPIO functionality for controlling VBUS in device mode if desired.


## TODO

### Handle USB remote wakeup timing correctly

The Chipidea FS IP doesn't handle the RESUME signal automatically and it must be managed in software. It needs to be asserted for exactly 10ms, and this is impossible to do without per-device support due to BSP differences. For now, a simple for-based loop is used.

### 8-bit PIC support

The 8-bit PICs also uses the Chipidea FS IP. Technically it's possible to support them as well.

Possible difficulties:
- Memory size constraints (1KB/8KB ballpark)
- A third BDT layout (now we have two)
- Different compiler-specific directives
- Compiler bugs if you use SDCC


## Author
[ReimuNotMoe](https://github.com/ReimuNotMoe) at SudoMaker, Ltd.


## Credits

This driver is based on:
- Microchip's USB driver (usb_device.c)
- TinyUSB's NXP KHCI driver (dcd_khci.c)
