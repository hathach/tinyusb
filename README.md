# tinyusb #

## What Is tinyusb ##

tinyusb is an open-source (BSD-licensed) USB host/device/otg stack for embedded micro-controller. It is developed using **Test-Driven Development (TDD)** approach to eliminate bugs as soon as possible. TDD indeed works with C & embedded with the help of Ceedling, Unity & CMock as a testing framework. 

More detail on TDD can be found at [James W. Grenning's book "Test Driven Development for Embedded C"](http://www.amazon.com/Driven-Development-Embedded-Pragmatic-Programmers/dp/193435662X) and [throwtheswitch's Ceedling, CMock & Unity](http://throwtheswitch.org/)

## Features ##

### RTOS ###

tinyusb is designed to be OS-ware and can run across RTOS vendor thanks to its OS Abstraction Layer (OSAL). However, it also run without OS by choosing the right value for macro **TUSB_CFG_OS**. Currently the following RTOS can be run with tinyusb (out of the box).

- **None OS**
- **FreeRTOS**

### Host ###

- HID Mouse
- HID Keyboard

### Device ###

coming soon ...

## Coding Standards ##

tinyusb make use of goodie features of C99, which saves a tons of code lines (also means save a tons of bugs). However, those feature can be misused and pave the way for bugs sneaking into. Therefore, to minimize bugs, the author try to comply with published Coding Standards like:

- [MISRA-C](http://www.misra-c.com/Activities/MISRAC/tabid/160/Default.aspx)
- [Power of 10](http://spinroot.com/p10/)
- ...

### MISRA-C 2004 Exceptions ###

MISRA-C is well respected & a bar for industrial coding standard. Where is possible, MISRA-C is followed but it is almost impossible to follow the standard without the following exceptions:  

- **Rule 2.2: use only /*** It has long passed the day that C99 comment style // will cause any issues especially to build tinyusb you need to enable your compiler's C99 mode. I think they will eventually drop this rule in upcoming MISRA-C 2012.
- **Rule 8.5: No definitions of objects or function in a header file**  function definitions in header files are used to allow 'inlining'
- **Rule 14.7: A function shall have a single point of exit at the end of the function** Unfortunately, following this rule will have a lot of nesting if else, I prefer to exit as soon as possible with assert style and flatten if else.
- **Rule 18.4: Unions shall not be used** sorry MISRA, union is required to effectively mapped to MCU's registers
- expect to have more & more

### Power of 10 Exception ###

- coming soon

## Is It Ready ##

It is still under developing, but most of the code can run out of the box with supported boards

## Getting Started ##

coming soon ...

## Supported MCUs ##

- NXP LPC18xx/LPC43xx family

## Supported Toolchains ##

currently only lpcxpresso/redsuite is supported. However Keil & IAR are always on top of the list in the near future 

## Supported Boards ##

this codebase can run out of the box with the following boards

### NXP LPC18xx/LPC43xx ###

- [Embedded Artists LPC4357 OEM & Base board](http://www.embeddedartists.com/products/kits/lpc4357_kit.php)
- [NGX Technologies LPC4330 Explorer](http://shop.ngxtechnologies.com/product_info.php?products_id=104)

## How Can I Help ##

If you find my little USB stack is useful, please take some time off to file any issues that you encountered. It is not necessary to be a software bug, it can be a question, request, suggestion etc. We can consider each github's issue as a forum's topic.

## License ##

BSD license for most of the codebase, but each file is individually licensed especially those in /vendor folder. Please make sure you understand all the license term for files you use in your project.
