***************
Getting Started
***************

This tutorial will guide you through setting up TinyUSB for your first project. We'll cover the basic integration steps and build your first example application.

Add TinyUSB to your project
---------------------------

To incorporate TinyUSB into your project:

* Copy this repository or add it as a git submodule to a subfolder in your project. For example, place it at ``your_project/tinyusb``
* Add all the ``.c`` files in the ``tinyusb/src`` folder to your project
* Add ``your_project/tinyusb/src`` to your include path. Also ensure that your include path contains the configuration file ``tusb_config.h``.
* Ensure all required macros are properly defined in ``tusb_config.h``. The configuration file from the demo applications provides a good starting point, but you'll need to add additional macros such as ``CFG_TUSB_MCU`` and ``CFG_TUSB_OS``. These are typically passed by make/cmake to maintain unique configurations for different boards.
* If you're using the **device stack**, you need to implement all **tud descriptor** callbacks for the stack to work.
* Add a ``tusb_init(rhport, role)`` call to your reset initialization code.
* Call ``tusb_int_handler(rhport, in_isr)`` from your USB IRQ handler
* Implement all enabled classes' callbacks.
* If you're not using an RTOS, you must call the ``tud_task()``/``tuh_task()`` functions periodically. These task functions handle all callbacks and core functionality.

.. note::
   TinyUSB uses consistent naming prefixes: ``tud_`` for device stack functions and ``tuh_`` for host stack functions. See the :doc:`../reference/glossary` for more details.

.. code-block:: c

   int main(void) {
     tusb_rhport_init_t dev_init = {
        .role = TUSB_ROLE_DEVICE,
        .speed = TUSB_SPEED_AUTO
     };
     // tud descriptor omitted here
     tusb_init(0, &dev_init); // initialize device stack on roothub port 0

     tusb_rhport_init_t host_init = {
        .role = TUSB_ROLE_HOST,
        .speed = TUSB_SPEED_AUTO
     };
     tusb_init(1, &host_init); // initialize host stack on roothub port 1

     while(1) { // the mainloop
       your_application_code();
       tud_task(); // device task
       tuh_task(); // host task
     }
   }

   void USB0_IRQHandler(void) {
     tusb_int_handler(0, true);
   }

   void USB1_IRQHandler(void) {
     tusb_int_handler(1, true);
   }

Examples
--------

For your convenience, TinyUSB contains a handful of examples for both host and device with/without RTOS to quickly test the functionality as well as demonstrate how API should be used. Most examples will work on most of :doc:`the supported boards <boards>`. Firstly we need to ``git clone`` if not already

.. code-block:: bash

   $ git clone https://github.com/hathach/tinyusb tinyusb
   $ cd tinyusb

Some ports require additional port-specific SDKs (e.g., for RP2040) or binaries (e.g., for Sony Spresense) to build examples. These components are outside the scope of TinyUSB, so you should download and install them first according to the manufacturer's documentation.

Dependencies
^^^^^^^^^^^^

TinyUSB separates example applications from board-specific hardware configurations (Board Support Packages, BSP). Example applications live in ``examples/device``, ``examples/host``, and ``examples/dual`` directories, while BSP configurations are stored in ``hw/bsp/FAMILY/boards/BOARD_NAME``. The BSP provides hardware abstraction including pin mappings, clock settings, linker scripts, and hardware initialization routines. For example, raspberry_pi_pico is located in ``hw/bsp/rp2040/boards/raspberry_pi_pico`` where ``FAMILY=rp2040`` and ``BOARD=raspberry_pi_pico``. When you build an example with ``BOARD=raspberry_pi_pico``, the build system automatically finds and uses the corresponding BSP.

Before building, you must first download dependencies including MCU low-level peripheral drivers and external libraries such as FreeRTOS (required by some examples). You can do this in either of two ways:

1. Run the ``tools/get_deps.py {FAMILY}`` script to download all dependencies for a specific MCU family. To download dependencies for all families, use ``FAMILY=all``.

.. code-block:: bash

   $ python tools/get_deps.py rp2040

2. Or run the ``get-deps`` target in one of the example folders as follows.

.. code-block:: bash

   $ cd examples/device/cdc_msc
   $ make BOARD=feather_nrf52840_express get-deps

You only need to do this once per family. Check out :doc:`complete list of dependencies and their designated path here <dependencies>`

Build Examples
^^^^^^^^^^^^^^

Examples support both Make and CMake build systems for most MCUs. However, some MCU families (such as Espressif and RP2040) only support CMake. First change directory to an example folder.

.. code-block:: bash

   $ cd examples/device/cdc_msc

Then compile with make or cmake

.. code-block:: bash

   $ # make
   $ make BOARD=feather_nrf52840_express all

   $ # cmake
   $ mkdir build && cd build
   $ cmake -DBOARD=raspberry_pi_pico ..
   $ make

To list all available targets with cmake

.. code-block:: bash

   $ cmake --build . --target help

Note: Some examples, especially those that use Vendor class (e.g., webUSB), may require udev permissions on Linux (and/or macOS) to access USB devices. It depends on your OS distribution, but typically copying ``99-tinyusb.rules`` and reloading udev is sufficient

.. code-block:: bash

   $ cp examples/device/99-tinyusb.rules /etc/udev/rules.d/
   $ sudo udevadm control --reload-rules && sudo udevadm trigger

RootHub Port Selection
~~~~~~~~~~~~~~~~~~~~~~

If a board has several ports, one port is chosen by default in the individual board.mk file. Use option ``RHPORT_DEVICE=x`` or ``RHPORT_HOST=x`` To choose another port. For example to select the HS port of a STM32F746Disco board, use:

.. code-block:: bash

   $ make BOARD=stm32f746disco RHPORT_DEVICE=1 all

   $ cmake -DBOARD=stm32f746disco -DRHPORT_DEVICE=1 ..

Port Speed
~~~~~~~~~~

An MCU can support multiple operational speeds. By default, the example build system uses the fastest speed supported by the board. Use the option ``RHPORT_DEVICE_SPEED=OPT_MODE_FULL_SPEED/OPT_MODE_HIGH_SPEED`` or ``RHPORT_HOST_SPEED=OPT_MODE_FULL_SPEED/OPT_MODE_HIGH_SPEED``. For example, to force the F723 to operate at full speed instead of the default high speed:

.. code-block:: bash

   $ make BOARD=stm32f746disco RHPORT_DEVICE_SPEED=OPT_MODE_FULL_SPEED all

   $ cmake -DBOARD=stm32f746disco -DRHPORT_DEVICE_SPEED=OPT_MODE_FULL_SPEED ..

Size Analysis
~~~~~~~~~~~~~

First install `linkermap tool <https://github.com/hathach/linkermap>`_ then ``linkermap`` target can be used to analyze code size. You may want to compile with ``NO_LTO=1`` since ``-flto`` merges code across ``.o`` files and make it difficult to analyze.

.. code-block:: bash

   $ make BOARD=feather_nrf52840_express NO_LTO=1 all linkermap

Flashing the Device
^^^^^^^^^^^^^^^^^^^

The ``flash`` target uses the default on-board debugger (jlink/cmsisdap/stlink/dfu) to flash the binary. Please install the supporting software in advance. Some boards use bootloader/DFU via serial, which requires passing the serial port to the make command

.. code-block:: bash

   $ make BOARD=feather_nrf52840_express flash
   $ make SERIAL=/dev/ttyACM0 BOARD=feather_nrf52840_express flash

Since jlink/openocd can be used with most of the boards, there is also ``flash-jlink/openocd`` (make) and ``EXAMPLE-jlink/openocd`` target for your convenience. Note for stm32 board with stlink, you can use ``flash-stlink`` target as well.

.. code-block:: bash

   $ make BOARD=feather_nrf52840_express flash-jlink
   $ make BOARD=feather_nrf52840_express flash-openocd

   $ cmake --build . --target cdc_msc-jlink
   $ cmake --build . --target cdc_msc-openocd

Some boards use UF2 bootloader for drag-and-drop into a mass storage device. UF2 files can be generated with the ``uf2`` target

.. code-block:: bash

   $ make BOARD=feather_nrf52840_express all uf2

   $ cmake --build . --target cdc_msc-uf2

Debugging
^^^^^^^^^

To compile for debugging add ``DEBUG=1``\ , for example

.. code-block:: bash

   $ make BOARD=feather_nrf52840_express DEBUG=1 all

   $ cmake -DBOARD=feather_nrf52840_express -DCMAKE_BUILD_TYPE=Debug ..

Enable Logging
~~~~~~~~~~~~~~

If you encounter issues running examples or need to submit a bug report, you can enable TinyUSB's built-in debug logging with the optional ``LOG=`` parameter. ``LOG=1`` prints only error messages, while ``LOG=2`` prints more detailed information about ongoing events. ``LOG=3`` or higher is not used yet.

.. code-block:: bash

   $ make BOARD=feather_nrf52840_express LOG=2 all

   $ cmake -DBOARD=feather_nrf52840_express -DLOG=2 ..

Logging Performance Impact
~~~~~~~~~~~~~~~~~~~~~~~~~~

By default, log messages are printed via the on-board UART, which is slow and consumes significant CPU time compared to USB speeds. If your board supports an on-board or external debugger, it would be more efficient to use it for logging. There are 2 protocols:


* `LOGGER=rtt`: use `Segger RTT protocol <https://www.segger.com/products/debug-probes/j-link/technology/about-real-time-transfer/>`_

  * Cons: requires jlink as the debugger.
  * Pros: work with most if not all MCUs
  * Software viewer is JLink RTT Viewer/Client/Logger which is bundled with JLink driver package.

* ``LOGGER=swo``\ : Use dedicated SWO pin of ARM Cortex SWD debug header.

  * Cons: Only works with ARM Cortex MCUs except M0
  * Pros: should be compatible with more debugger that support SWO.
  * Software viewer should be provided along with your debugger driver.

.. code-block:: bash

   $ make BOARD=feather_nrf52840_express LOG=2 LOGGER=rtt all
   $ make BOARD=feather_nrf52840_express LOG=2 LOGGER=swo all

   $ cmake -DBOARD=feather_nrf52840_express -DLOG=2 -DLOGGER=rtt ..
   $ cmake -DBOARD=feather_nrf52840_express -DLOG=2 -DLOGGER=swo ..

IAR Support
^^^^^^^^^^^

IAR Embedded Workbench is a commercial IDE and toolchain for embedded development. TinyUSB provides integration support for IAR through project connection files and native CMake support.

Use project connection
~~~~~~~~~~~~~~~~~~~~~~

IAR Project Connection files are provided to import TinyUSB stack into your project.

* A buildable project for your MCU needs to be created in advance.

  * Take example of STM32F0:

    -  You need ``stm32f0xx.h``, ``startup_stm32f0xx.s``, and ``system_stm32f0xx.c``.

    - ``STM32F0xx_HAL_Driver`` is only needed to run examples, TinyUSB stack itself doesn't rely on MCU's SDKs.

* Open ``Tools -> Configure Custom Argument Variables`` (Switch to ``Global`` tab if you want to do it for all your projects)
   Click ``New Group ...``, name it to ``TUSB``, Click ``Add Variable ...``, name it to ``TUSB_DIR``, change it's value to the path of your TinyUSB stack,
   for example ``C:\\tinyusb``

**Import stack only**

Open ``Project -> Add project Connection ...``, click ``OK``, choose ``tinyusb\\tools\\iar_template.ipcf``.

**Run examples**

1. Run ``iar_gen.py`` to generate .ipcf files of examples:

   .. code-block::

      > cd C:\tinyusb\tools
      > python iar_gen.py

2. Open ``Project -> Add project Connection ...``, click ``OK``, choose ``tinyusb\\examples\\(.ipcf of example)``.
   For example ``C:\\tinyusb\\examples\\device\\cdc_msc\\iar_cdc_msc.ipcf``

Native CMake support
~~~~~~~~~~~~~~~~~~~~

With 9.50.1 release, IAR added experimental native CMake support (strangely not mentioned in public release note). Now it's possible to import CMakeLists.txt then build and debug as a normal project.

Following these steps:

1. Add IAR compiler binary path to system ``PATH`` environment variable, such as ``C:\Program Files\IAR Systems\Embedded Workbench 9.2\arm\bin``.
2. Create new project in IAR, in Tool chain dropdown menu, choose CMake for Arm then Import ``CMakeLists.txt`` from chosen example directory.
3. Set up board option in ``Option - CMake/CMSIS-TOOLBOX - CMake``, for example ``-DBOARD=stm32f439nucleo -DTOOLCHAIN=iar``, **Uncheck 'Override tools in env'**.
4. (For debug only) Choose correct CPU model in ``Option - General Options - Target``, to profit register and memory view.

Common Issues and Solutions
---------------------------

**Build Errors**

* **"arm-none-eabi-gcc: command not found"**: Install ARM GCC toolchain: ``sudo apt-get install gcc-arm-none-eabi``
* **"Board 'X' not found"**: Check the available boards in ``hw/bsp/FAMILY/boards/`` or run ``python tools/build.py -l``
* **Missing dependencies**: Run ``python tools/get_deps.py FAMILY`` where FAMILY matches your board

**Runtime Issues**

* **Device not recognized**: Check USB descriptors implementation and ``tusb_config.h`` settings
* **Enumeration failure**: Enable logging with ``LOG=2`` and check for USB protocol errors
* **Hard faults/crashes**: Verify interrupt handler setup and stack size allocation

Quick Start Examples
--------------------

Now that you have TinyUSB set up, you can try these examples to see it in action.

Simple Device Example
^^^^^^^^^^^^^^^^^^^^^

The ``cdc_msc`` example creates a USB device with both a virtual serial port (CDC) and mass storage (MSC). This is the most commonly used example and demonstrates core device functionality.

**What it does:**
* Appears as a serial port that echoes back any text you send
* Appears as a small USB drive with a README.TXT file
* Blinks an LED to show activity

**Build and run:**

.. code-block:: bash

   $ cd examples/device/cdc_msc
   $ make BOARD=stm32f407disco all
   $ make BOARD=stm32f407disco flash

**Key files:**
* ``src/main.c`` - Main application with ``tud_task()`` loop
* ``src/usb_descriptors.c`` - USB device descriptors
* ``src/msc_disk.c`` - Mass storage implementation

**Expected behavior:** Connect to your computer and you'll see both a new serial port and a small USB drive appear.

Simple Host Example
^^^^^^^^^^^^^^^^^^^

The ``cdc_msc_hid`` example creates a USB host that can connect to USB devices with CDC, MSC, or HID interfaces.

**What it does:**
* Detects and enumerates connected USB devices
* Communicates with CDC devices (like USB-to-serial adapters)
* Reads from MSC devices (like USB drives)
* Receives input from HID devices (like keyboards and mice)

**Build and run:**

.. code-block:: bash

   $ cd examples/host/cdc_msc_hid
   $ make BOARD=stm32f407disco all
   $ make BOARD=stm32f407disco flash

**Key files:**
* ``src/main.c`` - Main application with ``tuh_task()`` loop
* ``src/cdc_app.c`` - CDC host functionality
* ``src/msc_app.c`` - Mass storage host functionality
* ``src/hid_app.c`` - HID host functionality

**Expected behavior:** Connect USB devices to see enumeration messages and device-specific interactions in the serial output.

Next Steps
^^^^^^^^^^

* Check :doc:`reference/boards` for board-specific information
* Explore more examples in ``examples/device/`` and ``examples/host/`` directories
