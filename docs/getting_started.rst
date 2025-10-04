***************
Getting Started
***************

This guide will get you up and running with TinyUSB quickly. We'll start with working examples, then show you how to integrate TinyUSB into your own projects.

Quick Start Examples
====================

The fastest way to understand TinyUSB is to see it working. These examples demonstrate core functionality and can be built immediately. We'll assume you are using the stm32f407disco board.

Simple Device Example
---------------------

The `cdc_msc <https://github.com/hathach/tinyusb/tree/master/examples/device/cdc_msc>`_ example creates a USB device with both a virtual serial port (CDC) and mass storage (MSC).

**What it does:**
* Appears as a serial port that echoes back any text you send
* Appears as a small USB drive with a README.TXT file
* Blinks an LED to show activity

**Build and run:**

.. code-block:: bash

   $ git clone https://github.com/hathach/tinyusb tinyusb
   $ cd tinyusb
   $ python tools/get_deps.py stm32f4  # Download dependencies
   $ cd examples/device/cdc_msc
   $ make BOARD=stm32f407disco all flash

Connect to your computer and you'll see both a new serial port and a small USB drive appear.

Simple Host Example
-------------------

The `cdc_msc_hid <https://github.com/hathach/tinyusb/tree/master/examples/host/cdc_msc_hid>`_ example creates a USB host that can connect to USB devices with CDC, MSC, or HID interfaces.

**What it does:**
* Detects and enumerates connected USB devices
* Communicates with CDC devices (like USB-to-serial adapters)
* Reads from MSC devices (like USB drives)
* Receives input from HID devices (like keyboards and mice)

**Build and run:**

.. code-block:: bash

   $ python tools/get_deps.py stm32f4  # If not done already
   $ cd examples/host/cdc_msc_hid
   $ make BOARD=stm32f407disco all flash

Connect USB devices to see enumeration messages and device-specific interactions in the serial output.

Project Structure
-----------------

TinyUSB separates example applications from board-specific hardware configurations:

* **Example applications**: Located in `examples/device/ <https://github.com/hathach/tinyusb/tree/master/examples/device>`_, `examples/host/ <https://github.com/hathach/tinyusb/tree/master/examples/host>`_, and `examples/dual/ <https://github.com/hathach/tinyusb/tree/master/examples/dual>`_ directories
* **Board Support Packages (BSP)**: Located in ``hw/bsp/FAMILY/boards/BOARD_NAME/`` with hardware abstraction including pin mappings, clock settings, and linker scripts

For example, raspberry_pi_pico is located in `hw/bsp/rp2040/boards/raspberry_pi_pico <https://github.com/hathach/tinyusb/tree/master/hw/bsp/rp2040/boards/raspberry_pi_pico>`_ where ``FAMILY=rp2040`` and ``BOARD=raspberry_pi_pico``. When you build with ``BOARD=raspberry_pi_pico``, the build system automatically finds the corresponding BSP using the FAMILY.

Add TinyUSB to Your Project
============================

Once you've seen TinyUSB working, here's how to integrate it into your own project:

Integration Steps
-----------------

1. **Get TinyUSB**: Copy this repository or add it as a git submodule to your project at ``your_project/tinyusb``

2. **Add source files**: Add all ``.c`` files from ``tinyusb/src/`` to your project

3. **Configure include paths**: Add ``your_project/tinyusb/src`` to your include path. Ensure your include path contains ``tusb_config.h``

4. **Configure TinyUSB**: Create ``tusb_config.h`` with required macros like ``CFG_TUSB_MCU`` and ``CFG_TUSB_OS``. Copy from ``examples/device/*/tusb_config.h`` as a starting point

5. **Implement USB descriptors**: For device stack, implement all ``tud_descriptor_*_cb()`` callbacks

6. **Initialize TinyUSB**: Add ``tusb_init()`` to your initialization code

7. **Handle interrupts**: Call ``tusb_int_handler()`` from your USB IRQ handler

8. **Run USB tasks**: Call ``tud_task()`` (device) or ``tuh_task()`` (host) periodically in your main loop

9. **Implement class callbacks**: Implement callbacks for enabled USB classes

Simple Integration Example
--------------------------

.. code-block:: c

   #include "tusb.h"

   int main(void) {
     board_init();  // Your board initialization

     tusb_rhport_init_t dev_init = {
       .role = TUSB_ROLE_DEVICE,
       .speed = TUSB_SPEED_AUTO
     };
     // tud_descriptor_* callbacks omitted here
     tusb_init(0, &dev_init);

     while(1) {
       tud_task();           // TinyUSB device task
       your_application();   // Your application code
     }
   }

   void USB_IRQHandler(void) {
     tusb_int_handler(0, true);
   }

.. note::
   Unlike many libraries, TinyUSB callbacks don't need to be explicitly registered. The stack automatically calls functions with specific names (e.g., ``tud_cdc_rx_cb()``) when events occur. Simply implement the callbacks you need.

.. note::
   TinyUSB uses consistent naming prefixes: ``tud_`` for device stack functions and ``tuh_`` for host stack functions. See the :doc:`reference/glossary` for more details.

Development Tips
================

**Debug builds and logging:**

.. code-block:: bash

   $ make BOARD=stm32f407disco DEBUG=1 all        # Debug build
   $ make BOARD=stm32f407disco LOG=2 all          # Enable detailed logging

**CMake build system:**

.. code-block:: bash

   $ mkdir build && cd build
   $ cmake -DBOARD=stm32f407disco ..
   $ make

**Alternative flash methods:**

.. code-block:: bash

   $ make BOARD=stm32f407disco flash-jlink        # Use J-Link
   $ make BOARD=stm32f407disco flash-openocd      # Use OpenOCD
   $ make BOARD=stm32f407disco all uf2            # Generate UF2 for drag-and-drop

**IAR Embedded Workbench:**

For IAR users, project connection files are available. Import `tools/iar_template.ipcf <https://github.com/hathach/tinyusb/tree/master/tools/iar_template.ipcf>`_ or use native CMake support (IAR 9.50.1+). See `tools/iar_gen.py <https://github.com/hathach/tinyusb/tree/master/tools/iar_gen.py>`_ for automated project generation.

Common Issues and Solutions
===========================

**Build Errors**

* **"arm-none-eabi-gcc: command not found"**: Install ARM GCC toolchain: ``sudo apt-get install gcc-arm-none-eabi``
* **"Board 'X' not found"**: Check the available boards in ``hw/bsp/FAMILY/boards/`` or run ``python tools/build.py -l``
* **Missing dependencies**: Run ``python tools/get_deps.py FAMILY`` where FAMILY matches your board

**Runtime Issues**

* **Device not recognized**: Check USB descriptors implementation and ``tusb_config.h`` settings
* **Enumeration failure**: Enable logging with ``LOG=2`` and check for USB protocol errors
* **Hard faults/crashes**: Verify interrupt handler setup and stack size allocation

**Linux Permissions**

Some examples require udev permissions to access USB devices:

.. code-block:: bash

   $ cp `examples/device/99-tinyusb.rules <https://github.com/hathach/tinyusb/tree/master/examples/device/99-tinyusb.rules>`_ /etc/udev/rules.d/
   $ sudo udevadm control --reload-rules && sudo udevadm trigger

Next Steps
==========

* Check :doc:`reference/boards` for board-specific information
* Explore more examples in `examples/device/ <https://github.com/hathach/tinyusb/tree/master/examples/device>`_ and `examples/host/ <https://github.com/hathach/tinyusb/tree/master/examples/host>`_ directories
* Read :doc:`reference/usb_concepts` to understand USB fundamentals
