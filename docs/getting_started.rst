***************
Getting Started
***************

This guide will get you up and running with TinyUSB quickly with working examples.

Project Structure
====================

TinyUSB separates example applications from board-specific hardware configurations:

* **Example applications**: Located in `examples/ <https://github.com/hathach/tinyusb/tree/master/examples/>`_ directories
* **Board Support Packages (BSP)**: Located in ``hw/bsp/FAMILY/boards/BOARD_NAME/`` with hardware abstraction including pin mappings, clock settings, and linker scripts
* **Build system**: Located in `examples/build_system/ <https://github.com/hathach/tinyusb/tree/master/examples/build_system>`_ which supports both Make and CMake. Though some MCU families such as espressif or rp2040 only support cmake

For example, stm32h743eval is located in `hw/bsp/stm32h7/boards/stm32h743eval <https://github.com/hathach/tinyusb/tree/master/hw/bsp/stm32h7/boards/stm32h743eval>`_ where ``FAMILY=stm32h7`` and ``BOARD=stm32h743eval``. When you build with ``BOARD=stm32h743eval``, the build system automatically finds the corresponding BSP using the FAMILY.

For guidance on integrating TinyUSB into your own firmware (configuration, descriptors, initialization, and callback workflow), see :doc:`integration`.

Quick Start Examples
====================

The fastest way to understand TinyUSB is to see it working. These examples demonstrate core functionality and can be built immediately.

We'll assume you are using the **STM32H743 Eval board** (BOARD=stm32h743eval) under the **stm32h7** family. For other boards, see ``Board Support Packages`` below.

Get the Code
------------

.. code-block:: bash

   $ git clone https://github.com/hathach/tinyusb tinyusb
   $ cd tinyusb
   $ python tools/get_deps.py -b stm32h743eval  # or python tools/get_deps.py stm32h7

.. note::
   For rp2040 `pico-sdk <https://github.com/raspberrypi/pico-sdk>`_ or `esp-idf <https://github.com/espressif/esp-idf>`_ for Espressif targets are required; install them per vendor instructions.

Simple Device Example
---------------------

The `cdc_msc <https://github.com/hathach/tinyusb/tree/master/examples/device/cdc_msc>`_ example creates a USB device with both a virtual serial port (CDC) and mass storage (MSC).

**What it does:**

* Appears as a serial port that echoes back any text you send
* Appears as a small USB drive with a README.TXT file
* Blinks an LED to show activity

**Build and run with CMake:**

.. code-block:: bash

   $ cd examples/device/cdc_msc
   $ cmake -DBOARD=stm32h743eval -B build # add "-G Ninja" to use Ninja build
   $ cmake --build build
   # cmake --build build --target cdc_msc-jlink

.. tip::
   Flashed/Debugger can be selected with --target ``-jlink``, ``-stlink`` or ``-openocd`` depending on your board. Use ``--target help`` to list all supported targets.

**Build and run with Make:**

.. code-block:: bash

   $ cd examples/device/cdc_msc
   $ make BOARD=stm32h743eval all
   $ make BOARD=stm32h743eval flash-jlink

.. tip::
   Flashed/Debugger can be selected with target ``flash-jlink``, ``flash-stlink`` or ``flash-openocd`` depending on your board.

Connect the device to your computer and you'll see both a new serial port and a small USB drive appear.

Simple Host Example
-------------------

The `cdc_msc_hid <https://github.com/hathach/tinyusb/tree/master/examples/host/cdc_msc_hid>`_ example creates a USB host that can connect to USB devices with CDC, MSC, or HID interfaces.

**What it does:**

* Detects and enumerates connected USB devices
* Communicates with CDC devices (like USB-to-serial adapters)
* Reads from MSC devices (like USB drives)
* Receives input from HID devices (like keyboards and mice)

**Build and run with CMake:**

.. code-block:: bash

   $ cd examples/host/cdc_msc_hid
   $ cmake -DBOARD=stm32h743eval -B build
   $ cmake --build build

**Build and run with Make:**

.. code-block:: bash

   $ cd examples/host/cdc_msc_hid
   $ make BOARD=stm32h743eval all
   $ make BOARD=stm32h743eval flash-jlink

Connect USB devices to see enumeration messages and device-specific interactions in the serial output.

Additional Build Options
------------------------

Debug and Logging
^^^^^^^^^^^^^^^^^

TinyUSB built-in logging can be enabled by setting `CFG_TUSB_DEBUG` which is done by passing ``LOG=level``. The higher the level, the more verbose the logging.

In addition to traditional hw uart as default, logging with debugger such as `Segger RTT <https://www.segger.com/products/debug-probes/j-link/technology/about-real-time-transfer/>`_ (10x faster) is also supported with `LOGGER=rtt` option.

.. code-block:: bash

   $ cmake -B build -DBOARD=stm32h743eval -DLOG=2               # logging level 2 with uart
   $ cmake -B build -DBOARD=stm32h743eval -DLOG=2 -DLOGGER=rtt  # logging level 2 with RTT

.. code-block:: bash

   $ make BOARD=stm32h743eval LOG=2 all              # logging level 2 with uart
   $ make BOARD=stm32h743eval LOG=2 LOGGER=rtt all   # logging level 2 with RTT

RootHub Port Selection
^^^^^^^^^^^^^^^^^^^^^^

Some boards support multiple usb controllers (roothub ports), by default one rh port is used as device, another as host in ``board.mk/board.cmake``. This can be overridden with option ``RHPORT_DEVICE=n`` or ``RHPORT_HOST=n`` To choose another port. For example to select the HS port of a STM32F746Disco board, use:

.. code-block:: bash

   $ cmake -B build -DBOARD=stm32h743eval -DRHPORT_DEVICE=1 # select roothub port 1 as device

.. code-block:: bash

   $ make BOARD=stm32h743eval RHPORT_DEVICE=1 all # select roothub port 1 as device

RootHub Port Speed
^^^^^^^^^^^^^^^^^^

A MCU can support multiple operational speed. By default, the example build system will use the fastest supported on the board. Use option ``RHPORT_DEVICE_SPEED=OPT_MODE_FULL/HIGH_SPEED/`` or ``RHPORT_HOST_SPEED=OPT_MODE_FULL/HIGH_SPEED/`` e.g To force operating speed

.. code-block:: bash

   $ cmake -B build -DBOARD=stm32h743eval -DRHPORT_DEVICE_SPEED=OPT_MODE_FULL_SPEED

.. code-block:: bash

   $ make BOARD=stm32h743eval RHPORT_DEVICE_SPEED=OPT_MODE_FULL_SPEED all


IAR Embedded Workbench
----------------------

For IAR users, project connection files are available. Import `tools/iar_template.ipcf <https://github.com/hathach/tinyusb/tree/master/tools/iar_template.ipcf>`_ or use native CMake support (IAR 9.50.1+). See `tools/iar_gen.py <https://github.com/hathach/tinyusb/tree/master/tools/iar_gen.py>`_ for automated project generation.


Common Issues and Solutions
---------------------------

**Build Errors**

* **"arm-none-eabi-gcc: command not found"**: Install ARM GCC toolchain: ``sudo apt-get install gcc-arm-none-eabi``
* **"Board 'X' not found"**: Check the available boards in ``hw/bsp/FAMILY/boards/`` or run ``python tools/build.py -l``
* **Missing dependencies**: Run ``python tools/get_deps.py FAMILY`` where FAMILY matches your board or ``python tools/get_deps.py -b BOARD``

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

* Check :doc:`integration` for integrating TinyUSB into your own firmware
* Check :doc:`reference/boards` for board-specific information
* Explore more examples in `examples/device/ <https://github.com/hathach/tinyusb/tree/master/examples/device>`_ and `examples/host/ <https://github.com/hathach/tinyusb/tree/master/examples/host>`_ directories
* Read :doc:`reference/usb_concepts` to understand USB fundamentals
