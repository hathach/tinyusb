***************
Troubleshooting
***************

This guide helps you diagnose and fix common issues when developing with TinyUSB.

Build Issues
============

Toolchain Problems
------------------

**"arm-none-eabi-gcc: command not found"**

The ARM GCC toolchain is not installed or not in PATH.

*Solution*:
.. code-block:: bash

   # Ubuntu/Debian
   sudo apt-get update && sudo apt-get install gcc-arm-none-eabi

   # macOS with Homebrew
   brew install --cask gcc-arm-embedded

   # Windows: Download from ARM website and add to PATH

**"make: command not found" or CMake errors**

Build tools are missing.

*Solution*:
.. code-block:: bash

   # Ubuntu/Debian
   sudo apt-get install build-essential cmake

   # macOS
   xcode-select --install
   brew install cmake

Dependency Issues
-----------------

**"No rule to make target" or missing header files**

Dependencies for your MCU family are not downloaded.

*Solution*:
.. code-block:: bash

   # Download dependencies for specific family
   python tools/get_deps.py stm32f4  # Replace with your family

   # Or from example directory
   cd examples/device/cdc_msc
   make BOARD=your_board get-deps

**Board Not Found**

Invalid board name in build command.

*Diagnosis*:
.. code-block:: bash

   # List available boards for a family
   ls hw/bsp/stm32f4/boards/

   # List all supported boards
   python tools/build.py -l

*Solution*: Use exact board name from the listing.

Runtime Issues
==============

Device Mode Problems
--------------------

**Device not recognized by host**

The most common issue - host doesn't see your USB device.

*Diagnosis steps*:
1. Check USB cable (must support data, not just power)
2. Enable logging: build with ``LOG=2``
3. Use different USB ports/hosts
4. Check device manager (Windows) or ``dmesg`` (Linux)

*Common causes and solutions*:

- **Invalid descriptors**: Review ``usb_descriptors.c`` carefully
- **``tud_task()`` not called**: Ensure regular calls in main loop (< 1ms interval)
- **Wrong USB configuration**: Check ``tusb_config.h`` settings
- **Hardware issues**: Verify USB pins, crystal/clock configuration

**Enumeration starts but fails**

Device is detected but configuration fails.

*Diagnosis*:
.. code-block:: bash

   # Build with logging enabled
   make BOARD=your_board LOG=2 all

*Look for*:
- Setup request handling errors
- Endpoint configuration problems
- String descriptor issues

*Solutions*:
- Implement all required descriptors
- Check endpoint sizes match descriptors
- Ensure control endpoint (EP0) handling is correct

**Data transfer issues**

Device enumerates but data doesn't transfer correctly.

*Common causes*:
- Buffer overruns in class callbacks
- Incorrect endpoint usage (IN vs OUT)
- Flow control issues in CDC class

*Solutions*:
- Check buffer sizes in callbacks
- Verify endpoint directions in descriptors
- Implement proper flow control

Host Mode Problems
------------------

**No devices detected**

Host application doesn't see connected devices.

*Hardware checks*:
- Power supply adequate for host mode
- USB-A connector for host (not micro-USB)
- Board supports host mode on selected port

*Software checks*:
- ``tuh_task()`` called regularly
- Host stack enabled in ``tusb_config.h``
- Correct root hub port configuration

**Device enumeration fails**

Devices connect but enumeration fails.

*Diagnosis*:
.. code-block:: bash

   # Enable host logging
   make BOARD=your_board LOG=2 RHPORT_HOST=1 all

*Common issues*:
- Power supply insufficient during enumeration
- Timing issues with slow devices
- USB hub compatibility problems

**Class driver issues**

Device enumerates but class-specific communication fails.

*Troubleshooting*:
- Check device descriptors match expected class
- Verify interface/endpoint assignments
- Some devices need device-specific handling

Performance Issues
==================

Slow Transfer Speeds
--------------------

**Symptoms**: Lower than expected USB transfer rates

*Causes and solutions*:
- **Task scheduling**: Call ``tud_task()``/``tuh_task()`` more frequently
- **Endpoint buffer sizes**: Increase buffer sizes for bulk transfers
- **DMA usage**: Enable DMA for USB transfers if supported
- **USB speed**: Use High Speed (480 Mbps) instead of Full Speed (12 Mbps)

High CPU Usage
--------------

**Symptoms**: MCU spending too much time in USB handling

*Solutions*:
- Use efficient logging (RTT/SWO instead of UART)
- Reduce log level in production builds
- Optimize descriptor parsing
- Use DMA for data transfers

Memory Issues
=============

Stack Overflow
--------------

**Symptoms**: Hard faults, random crashes, especially during enumeration

*Diagnosis*:
- Build with ``DEBUG=1`` and use debugger
- Check stack pointer before/after USB operations
- Monitor stack usage with RTOS tools

*Solutions*:
- Increase stack size in linker script
- Reduce local variable usage in callbacks
- Use static buffers instead of large stack arrays

Heap Issues
-----------

**Note**: TinyUSB doesn't use dynamic allocation, but your application might.

*Check*:
- Application code using malloc/free
- RTOS heap usage
- Third-party library allocations

Hardware-Specific Issues
========================

STM32 Issues
------------

**Clock configuration problems**:
- USB requires precise 48MHz clock
- HSE crystal must be configured correctly
- PLL settings affect USB timing

**Pin configuration**:
- USB pins need specific alternate function settings
- VBUS sensing configuration
- ID pin for OTG applications

RP2040 Issues
-------------

**PIO-USB for host mode**:
- Requires specific pin assignments
- CPU overclocking may be needed for reliable operation
- Timing-sensitive - avoid long interrupt disable periods

**Flash/RAM constraints**:
- Large USB applications may exceed RP2040 limits
- Use code optimization and remove unused features

ESP32 Issues
------------

**USB peripheral differences**:
- ESP32-S2/S3 have different USB capabilities
- Some variants only support device mode
- DMA configuration varies between models

Advanced Debugging
==================

Using USB Analyzers
-------------------

For complex issues, hardware USB analyzers provide detailed protocol traces:

- **Wireshark** with USBPcap (Windows) or usbmon (Linux)
- **Hardware analyzers**: Total Phase Beagle, LeCroy USB analyzers
- **Logic analyzers**: For timing analysis of USB signals

Debugging with GDB
------------------

.. code-block:: bash

   # Build with debug info
   make BOARD=your_board DEBUG=1 all

   # Use with debugger
   arm-none-eabi-gdb build/your_app.elf

*Useful breakpoints*:
- ``dcd_int_handler()`` - USB interrupt entry
- ``tud_task()`` - Main device task
- Class-specific callbacks

Custom Logging
--------------

For production debugging, implement custom logging:

.. code-block:: c

   // In tusb_config.h
   #define CFG_TUSB_DEBUG_PRINTF   my_printf

   // Your implementation
   void my_printf(const char* format, ...) {
     // Send to RTT, SWO, or custom interface
   }

Getting Help
============

When reporting issues:

1. **Minimal reproducible example**: Simplify to bare minimum
2. **Build information**: Board, toolchain version, build flags
3. **Logs**: Include output with ``LOG=2`` enabled
4. **Hardware details**: Board revision, USB connections, power supply
5. **Host environment**: OS version, USB port type

**Resources**:
- GitHub Discussions: https://github.com/hathach/tinyusb/discussions
- Issue Tracker: https://github.com/hathach/tinyusb/issues
- Documentation: https://docs.tinyusb.org