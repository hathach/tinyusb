**************************
Frequently Asked Questions
**************************

General Questions
=================

**Q: What microcontrollers does TinyUSB support?**

TinyUSB supports 30+ MCU families including STM32, RP2040, NXP (iMXRT, Kinetis, LPC), Microchip SAM, Nordic nRF5x, ESP32, and many others. See :doc:`reference/boards` for the complete list.

**Q: Can I use TinyUSB in commercial projects?**

Yes, TinyUSB is released under the MIT license, allowing commercial use with minimal restrictions.

**Q: Does TinyUSB require an RTOS?**

No, TinyUSB works in bare metal environments. It also supports FreeRTOS, RT-Thread, and Mynewt.

**Q: How much memory does TinyUSB use?**

Typical usage: 8-20KB flash, 1-4KB RAM depending on enabled classes and configuration. The stack uses static allocation only.

Build and Setup
================

**Q: Why do I get "arm-none-eabi-gcc: command not found"?**

Install the ARM GCC toolchain: ``sudo apt-get install gcc-arm-none-eabi`` on Ubuntu/Debian, or download from ARM's website for other platforms.

**Q: Build fails with "Board 'X' not found"**

Check available boards: ``ls hw/bsp/FAMILY/boards/`` or run ``python tools/build.py -l`` to list all supported boards.

**Q: What are the dependencies and how do I get them?**

Run ``python tools/get_deps.py FAMILY`` where FAMILY is your MCU family (e.g., stm32f4, rp2040). This downloads MCU-specific drivers and libraries.

**Q: Can I use my own build system instead of Make/CMake?**

Yes, just add all ``.c`` files from ``src/`` to your project and configure include paths. See :doc:`getting_started` for details.

**Q: Error: "tusb_config.h: No such file or directory"**

This is a very common issue. You need to create ``tusb_config.h`` in your project and ensure it's in your include path. The file must define ``CFG_TUSB_MCU`` and ``CFG_TUSB_OS`` at minimum. Copy from ``examples/device/*/tusb_config.h`` as a starting point.

**Q: RP2040 + pico-sdk ignores my tusb_config.h settings**

The pico-sdk build system can override ``tusb_config.h`` settings. The ``CFG_TUSB_OS`` setting is often ignored because pico-sdk sets it to ``OPT_OS_PICO`` internally. Use pico-sdk specific configuration methods or modify the CMake configuration.

**Q: "multiple definition of dcd_..." errors with STM32**

This happens when multiple USB drivers are included. Ensure you're only including the correct portable driver for your STM32 family. Check that ``CFG_TUSB_MCU`` is set correctly and you don't have conflicting source files.

Device Development
==================

**Q: My USB device isn't recognized by the host**

Common causes:
- Invalid USB descriptors - validate with ``LOG=2`` build
- ``tud_task()`` not called regularly in main loop
- Incorrect ``tusb_config.h`` settings
- USB cable doesn't support data (charging-only cable)

**Q: Windows shows "Device Descriptor Request Failed"**

This typically indicates:
- Malformed device descriptor
- USB timing issues (check crystal/clock configuration)
- Power supply problems during enumeration
- Conflicting devices on the same USB hub

**Q: How do I implement a custom USB class?**

Use the vendor class interface (``CFG_TUD_VENDOR``) or implement a custom class driver. See ``src/class/vendor/`` for examples.

**Q: Can I have multiple configurations or interfaces?**

Yes, TinyUSB supports multiple configurations and composite devices. Modify the descriptors in ``usb_descriptors.c`` accordingly.

**Q: How do I change Vendor ID/Product ID?**

Edit the device descriptor in ``usb_descriptors.c``. For production, obtain your own VID from USB-IF or use one from your silicon vendor.

**Q: Device works alone but fails when connected through USB hub**

This is a known issue where some devices interfere with each other when connected to the same hub. Try:
- Using different USB hubs
- Connecting devices to separate USB ports
- Checking for power supply issues with the hub

Host Development
================

**Q: Why doesn't my host application detect any devices?**

Check:
- Power supply - host mode requires more power than device mode
- USB connector type - use USB-A for host applications
- Board supports host mode on the selected port
- Enable logging with ``LOG=2`` to see enumeration details

**Q: Can I connect multiple devices simultaneously?**

Yes, through a USB hub. TinyUSB supports multi-level hubs and multiple device connections.

**Q: Does TinyUSB support USB 3.0?**

No, TinyUSB currently supports USB 2.0 and earlier. USB 3.0 devices typically work in USB 2.0 compatibility mode.

Configuration and Features
==========================

**Q: How do I enable/disable specific USB classes?**

Edit ``tusb_config.h`` and set the corresponding ``CFG_TUD_*`` or ``CFG_TUH_*`` macros to 1 (enable) or 0 (disable).

**Q: Can I use both device and host modes simultaneously?**

Yes, with dual-role/OTG capable hardware. See ``examples/dual/`` for implementation examples.

**Q: How do I optimize for code size?**

- Disable unused classes in ``tusb_config.h``
- Use ``CFG_TUSB_DEBUG = 0`` for release builds
- Compile with ``-Os`` optimization
- Consider using only required endpoints/interfaces

**Q: Does TinyUSB support low power/suspend modes?**

Yes, TinyUSB handles USB suspend/resume. Implement ``tud_suspend_cb()`` and ``tud_resume_cb()`` for custom power management.

**Q: What CFG_TUSB_MCU should I use for x86/PC platforms?**

For PC/motherboard applications, there's no standard MCU option. You may need to use a generic option or modify TinyUSB for your specific use case. Consider using libusb or other PC-specific USB libraries instead.

**Q: RP2040 FreeRTOS configuration issues**

The RP2040 pico-sdk has specific requirements for FreeRTOS integration. The ``CFG_TUSB_OS`` setting may be overridden by the SDK. Use pico-sdk specific configuration methods and ensure proper task stack sizes for the USB task.

Debugging and Troubleshooting
=============================

**Q: How do I debug USB communication issues?**

1. Enable logging: build with ``LOG=2``
2. Use ``LOGGER=rtt`` or ``LOGGER=swo`` for high-speed logging
3. Use USB protocol analyzers for detailed traffic analysis
4. Check with different host systems (Windows/Linux/macOS)

**Q: My application crashes or hard faults**

Common causes:
- Stack overflow - increase stack size in linker script
- Incorrect interrupt configuration
- Buffer overruns in USB callbacks
- Build with ``DEBUG=1`` and use a debugger

**Q: Performance is poor or USB transfers are slow**

- Ensure ``tud_task()``/``tuh_task()`` called frequently (< 1ms intervals)
- Use DMA for USB transfers if supported by your MCU
- Optimize endpoint buffer sizes
- Consider using high-speed USB if available

**Q: Some USB devices don't work with my host application**

- Not all devices follow USB standards perfectly
- Some may need device-specific handling
- Composite devices may have partial support
- Check device descriptors and implement custom drivers if needed

**Q: ESP32-S3 USB host/device issues**

ESP32-S3 has specific USB implementation challenges:
- Ensure proper USB pin configuration
- Check power supply requirements for host mode
- Some features may be limited compared to other MCUs
- Use ESP32-S3 specific examples and documentation

STM32CubeIDE Integration
========================

**Q: How do I integrate TinyUSB with STM32CubeIDE?**

1. In STM32CubeMX, enable USB_OTG_FS/HS under Connectivity, set to "Device_Only" mode
2. Enable the USB global interrupt in NVIC Settings
3. Add ``tusb.h`` include and call ``tusb_init()`` in main.c
4. Call ``tud_task()`` in your main loop
5. In the generated ``stm32xxx_it.c``, modify the USB IRQ handler to call ``tud_int_handler(0)``
6. Create ``tusb_config.h`` and ``usb_descriptors.c`` files

**Q: STM32CubeIDE generated code conflicts with TinyUSB**

Don't use STM32's built-in USB middleware (USB Device Library) when using TinyUSB. Disable USB code generation in STM32CubeMX and let TinyUSB handle all USB functionality.

**Q: STM32 USB interrupt handler setup**

Replace the generated USB interrupt handler with a call to TinyUSB:

.. code-block:: c

   void OTG_FS_IRQHandler(void) {
     tud_int_handler(0);
   }

**Q: Which STM32 families work best with TinyUSB?**

STM32F4, F7, and H7 families have the most mature TinyUSB support. STM32F0, F1, F3, L4 families are also supported but may have more limitations. Check the supported boards list for your specific variant.