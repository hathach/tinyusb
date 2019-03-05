This README describes the layout of system files and headers for
microcontrollers supported by tinyusb from the vendor [ST](www.st.com).

The CMSIS by ARM is an attempt to unify how system initialization and headers
for ARM microcontrollers are handled across vendors. A vendor is expected to
provide a number of files, including vendor-specific device headers,
vendor-specific system init code (in C), vendor-specific startup code (coded in
assembly _and_ meant to make writing C interrupt handlers easy), and ARM
architecture-specific headers. In practice, it's actually difficult to find all
these files in one place.

* stm32cube generates a self-contained application. All the relevant headers,
  system init, and startup code are under `Drivers/CMSIS`. The linker script is
  stored at the root of your project, _along_ with a copy of the startup code.
  Relative to `Drivers/CMSIS`:
  * Device header files are stored under `Device/ST/$family/Include`.
  * Arm-provided CMSIS headers are stored under `Include`.
  * The portable C portion of the startup code, called `system_$family.c`, is
    stored under `Device/ST/$family/Source/Templates`. This "system init" code
    is required by CMSIS.
  * The assembly portion of the startup code, called `startup_$device.c`, is
    stored under `Device\ST\STM32F4xx\Source\Templates\gcc`.

* Micropython maintains [a repo](https://github.com/micropython/stm32lib)
  with _only_ the device header files. We use that
  repo and provide the remaining files ourselves.
  * Micropython keeps the linker script and assembly portion of the startup
    code under the
    [boards](https://github.com/micropython/micropython/tree/master/ports/stm32/boards)
    directory in the stm32 port (but the code is board agnostic).
  * Micropython also embeds its own
    [copy](https://github.com/micropython/micropython/tree/master/lib/cmsis/inc)
    of CMSIS headers for all ARM targets into the repo.
  * Micropython indeed _does_ use their own custom C startup code (CMSIS system
    init) for
    [stm32](https://github.com/micropython/micropython/blob/master/ports/stm32/system_stm32.c)
    targets. It is
    [missing](https://github.com/micropython/micropython/commit/fdbc22e4d27b9d1c686647c9c67a32206f7bc83a)
    the `SystemCoreClockUpdate` function.

* TinyUSB uses Micropython's [stm32lib](stm32lib) as a submodule to provide
  device headers. The remaining system files can be found in the following
  directories:
  * C system init code is in the [system-init](system-init) directory.
  * Assembly startup code is in the [startup](startup) directory. Each family
    gets its own subdirectory under startup (e.g. [stm32f4](startup/stm32f4)).
  * ARM-provided headers are stored in the [cmsis](cmsis) directory.
  * The linker script for the STM32F4-DISCOVERY board demo is supplied as
    part of TinyUSB's
    [Board Support Packages](../../bsp/stm32f407g_disc1/STM32F407VGTx_FLASH.ld).
  The above files were extracted from a dummy stm32cube project in February
  2019.
