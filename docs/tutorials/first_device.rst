*********************
Your First USB Device
*********************

This tutorial walks you through creating a simple USB CDC (serial) device using TinyUSB. By the end, you'll have a working USB device that appears as a serial port on your computer.

Prerequisites
=============

* Completed :doc:`getting_started` tutorial
* Development board with USB device capability (e.g., STM32F4 Discovery, Raspberry Pi Pico)
* Basic understanding of C programming

Understanding USB Device Basics
===============================

A USB device needs three key components:

1. **USB Descriptors**: Tell the host what kind of device this is
2. **Class Implementation**: Handle USB class-specific requests (CDC, HID, etc.)
3. **Application Logic**: Your main application code

Step 1: Choose Your Starting Point
==================================

We'll start with the ``cdc_msc`` example as it's the most commonly used and well-tested.

.. code-block:: bash

   cd examples/device/cdc_msc

This example implements both CDC (virtual serial port) and MSC (mass storage) classes.

Step 2: Understand the Code Structure
=====================================

Key files in the example:

* ``main.c`` - Main application loop and board initialization
* ``usb_descriptors.c`` - USB device descriptors
* ``tusb_config.h`` - TinyUSB stack configuration

**Main Loop Pattern**:

.. code-block:: c

   int main(void) {
     board_init();
     tusb_init();

     while (1) {
       tud_task(); // TinyUSB device task
       cdc_task(); // Application-specific CDC handling
     }
   }

**Device Task**: ``tud_task()`` must be called regularly to handle USB events and maintain the connection.

Step 3: Build and Test
======================

.. code-block:: bash

   # Fetch dependencies for your board family
   python ../../../tools/get_deps.py stm32f4  # Replace with your family

   # Build for your board
   make BOARD=stm32f407disco all

   # Flash to device
   make BOARD=stm32f407disco flash

**Expected Result**: After flashing, connect the USB port to your computer. You should see:

* A new serial port device (e.g., ``/dev/ttyACM0`` on Linux, ``COMx`` on Windows)
* A small mass storage device

Step 4: Customize for Your Needs
=================================

**Simplify to CDC-only**:

1. In ``tusb_config.h``, disable MSC:

.. code-block:: c

   #define CFG_TUD_MSC               0  // Disable Mass Storage

2. Remove MSC-related code from ``main.c`` and ``usb_descriptors.c``

**Modify Device Information**:

In ``usb_descriptors.c``:

.. code-block:: c

   tusb_desc_device_t const desc_device = {
     .idVendor           = 0xCafe,  // Your vendor ID
     .idProduct          = 0x4000,  // Your product ID
     .bcdDevice          = 0x0100,  // Device version
     // ... other fields
   };

**Add Application Logic**:

In the CDC task function, add your serial communication logic:

.. code-block:: c

   void cdc_task(void) {
     if (tud_cdc_available()) {
       uint8_t buf[64];
       uint32_t count = tud_cdc_read(buf, sizeof(buf));

       // Echo back what was received
       tud_cdc_write(buf, count);
       tud_cdc_write_flush();
     }
   }

Common Issues and Solutions
===========================

**Device Not Recognized**:

* Check USB cable (must support data, not just power)
* Verify descriptors are valid using ``LOG=2`` build option
* Ensure ``tud_task()`` is called regularly in main loop

**Build Errors**:

* Missing dependencies: Run ``python tools/get_deps.py FAMILY``
* Wrong board name: Check ``hw/bsp/FAMILY/boards/`` for valid names
* Compiler issues: Install ``gcc-arm-none-eabi``

**Runtime Issues**:

* Hard faults: Check stack size in linker script
* USB not working: Verify clock configuration and USB pin setup
* Serial data corruption: Ensure proper flow control in CDC implementation

Next Steps
==========

* Learn about other device classes in :doc:`../reference/usb_classes`
* Understand advanced integration in :doc:`../guides/integration`
* Explore TinyUSB architecture in :doc:`../explanation/architecture`