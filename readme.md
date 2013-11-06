# tinyusb #

## What Is tinyusb ##

tinyusb is an open-source (BSD-licensed) USB Host/Device/OTG stack for embedded micro-controller. It is developed using [Test-Driven Development (TDD)](tests/readme.md) approach to eliminate bugs as soon as possible.

![tinyusb diagram](/docs/images/what_is_tinyusb.png)

## Features ##

designed to be simple and run out-of-the-box provided the configuration is correct.

### Host ###

- HID Mouse
- HID Keyboard
- Communication Class (CDC)
- Mass-Storage (MSC)
- Hub
    - Only support 1 level of hub (due to my laziness)
- Multiple host controllers
- Android Accessory Open (comming soon)
- FTDI (coming soon)
- Audio (coming soon)


### Device ###

- HID Mouse
- HID Keyboard
- Mass-Storage (MSC)
- Communication Class (CDC) ... coming soon

### RTOS ###

tinyusb is designed to be OS-ware and run across OS vendors, thanks to its OS Abstraction Layer (OSAL). However, it can also run without an OS (OSAL will be expanded to be a state machine in this case). Currently the following OS can be run with tinyusb (out of the box).

- **None OS**
- **FreeRTOS**
- **CMSIS RTX**

## Is It Ready ##

Although tinyusb is still in early stage of developing, but most of the code can run out of the box with supported boards.

## Getting Started ##

coming soon ...

## Supported MCUs ##

- NXP LPC43xx family

## Supported Toolchains ##

The following toolchain is supported

- *lpcxpresso/redsuite*
- *Keil MDK*
- *IAR Workbench*

## Supported Boards ##

this code base can run out of the box with the following boards

### NXP LPC43xx ###

- [Embedded Artists LPC4357 OEM & Base board](http://www.embeddedartists.com/products/kits/lpc4357_kit.php)
- [NGX Technologies LPC4330 Explorer](http://shop.ngxtechnologies.com/product_info.php?products_id=104)
- [Keil MCB4357 Evaluation Board](http://www.keil.com/mcb4300)

## Coding Standards ##

C is a dangerous language by itself, plus tinyusb make use of goodies features of C99, which saves a tons of code lines (also means save a tons of bugs). However, those features can be misused and pave the way for bugs sneaking into. Therefore, to minimize bugs, the author try to comply with published Coding Standards like:

- [MISRA-C](http://www.misra-c.com/Activities/MISRAC/tabid/160/Default.aspx)
- [Power of 10](http://spinroot.com/p10/)
- [Jet Propulsion Laboratory (JPL) for C](http://lars-lab.jpl.nasa.gov)

Where is possible, standards are followed but it is almost impossible to follow all of these without making some exceptions. I am pretty sure this code base violates more than what are described below, if you can find any, please report it to me or file an issue on github.

### MISRA-C 2004 Exceptions ###

MISRA-C is well respected & a bar for industrial coding standard. 

- **Rule 2.2: use only /*** It has long passed the day that C99 comment style // will cause any issues, especially compiler's C99 mode is required to build tinyusb. I think they will eventually drop this rule in upcoming MISRA-C 2012.
- **Rule 8.5: No definitions of objects or function in a header file**  function definitions in header files are used to allow 'inlining'
- **Rule 14.7: A function shall have a single point of exit at the end of the function** Unfortunately, following this rule will have a lot of nesting if-else, I prefer to exit as soon as possible with assert style and flatten if-else.
- **Rule 18.4: Unions shall not be used** sorry MISRA, union is required to effectively mapped to MCU's registers
- expect to have more & more exceptions.

### Power of 10 ###

is a small & easy to remember but yet powerful coding guideline. Most (if not all) of the rules here are included in JPL. Because it is very small, all the rules will be listed here, those with *italic* are compliant, **bold** are violated. 

1. *`Restrict to simple control flow constructs`* 
yes, I hate goto statement, therefore there is none of those here

2. *`Give all loops a fixed upper-bound`* 
one of my favorite rule

3. *`Do not use dynamic memory allocation after initialization`* 
the tinyusb uses the static memory for all of its data.

4. **`Limit functions to no more than 60 lines of text`** 
60 is a little bit too strict, I will update the relaxing number later

5. *`Use minimally two assertions per function on average`* 
not sure the exact number, but I use a tons of those assert

6. *`Declare data objects at the smallest possible level of scope`* 
one of the best & easiest rule to follow

7. *`Check the return value of non-void functions, and check the validity of function parameters`* 
I did check all of the public application API's parameters. For internal API, calling function needs to trust their caller to reduce duplicated check.

8. **`Limit the use of the preprocessor to file inclusion and simple macros`** 
Although I prefer inline function, however C macros are far powerful than that. I simply cannot hold myself to use, for example X-Macro technique to simplify code.

9. *`Limit the use of pointers. Use no more than two levels of dereferencing per expression`* 
never intend to get in trouble with complex pointer dereferencing.

10. *`Compile with all warnings enabled, and use one or more source code analyzers`* 
I try to use all the defensive options of gnu, let me know if I miss some.
>-pedantic -Wextra -Wswitch-default -Wunsafe-loop-optimizations -Wcast-align -Wlogical-op -Wpacked-bitfield-compat -Wpadded -Wnested-externs -Wredundant-decls -Winline -Wpacked

### JPL ###

coming soon ...

## How Can I Help ##

If you find my little USB stack is useful, please take some time to file any issues that you encountered. It is not necessary to be a software bug, it can be a question, request, suggestion etc. We can consider each github's issue as a forum's topic. Alternatively, you can buy me a cup of coffee if you happen to be in Hochiminh city.

## License ##

BSD license for most of the code base, but each file is individually licensed especially those in /vendor folder. Please make sure you understand all the license term for files you use in your project.
