***************
Getting Started
***************

Add TinyUSB to your project
---------------------------

It is relatively simple to incorporate tinyusb to your (existing) project


* Copy or ``git submodule`` this repo into your project in a subfolder. Let's say it is *your_project/tinyusb*
* Add all the .c in the ``tinyusb/src`` folder to your project
* Add *your_project/tinyusb/src* to your include path. Also make sure your current include path also contains the configuration file tusb_config.h.
* Make sure all required macros are all defined properly in tusb_config.h (configure file in demo application is sufficient, but you need to add a few more such as CFG_TUSB_MCU, CFG_TUSB_OS since they are passed by IDE/compiler to maintain a unique configure for all boards).
* If you use the device stack, make sure you have created/modified usb descriptors for your own need. Ultimately you need to implement all **tud descriptor** callbacks for the stack to work.
* Add tusb_init() call to your reset initialization code.
* Call ``tud_int_handler()`` (device) and/or ``tuh_int_handler()`` (host) in your USB IRQ Handler
* Implement all enabled classes's callbacks.
* If you don't use any RTOSes at all, you need to continuously and/or periodically call tud_task()/tuh_task() function. All of the callbacks and functionality are handled and invoked within the call of that task runner.

.. code-block::

   int main(void)
   {
     your_init_code();
     tusb_init(); // initialize tinyusb stack

     while(1) // the mainloop
     {
       your_application_code();

       tud_task(); // device task
       tuh_task(); // host task
     }
   }

Examples
--------

For your convenience, TinyUSB contains a handful of examples for both host and device with/without RTOS to quickly test the functionality as well as demonstrate how API() should be used. Most examples will work on most of `the supported boards <supported.rst>`_. Firstly we need to ``git clone`` if not already

.. code-block::

   $ git clone https://github.com/hathach/tinyusb tinyusb
   $ cd tinyusb

Some TinyUSB examples also requires external submodule libraries in ``/lib`` such as FreeRTOS, Lightweight IP to build. Run following command to fetch them 

.. code-block::

   $ git submodule update --init lib

Some ports will also require a port-specific SDK (e.g. RP2040) or binary (e.g. Sony Spresense) to build examples. They are out of scope for tinyusb, you should download/install it first according to its manufacturer guide. 

Build
^^^^^

To build example, first change directory to an example folder. 

.. code-block::

   $ cd examples/device/cdc_msc

Before building, we need to download MCU driver submodule to provide low-level MCU peripheral's driver first. Run the ``get-deps`` target in one of the example folder as follow. You only need to do this once per mcu

.. code-block::

   $ make BOARD=feather_nrf52840_express get-deps


Some modules (e.g. RP2040 and ESP32s2) require the project makefiles to be customized using CMake. If necessary apply any setup steps for the platform's SDK.

Then compile with ``make BOARD=[board_name] all``\ , for example

.. code-block::

   $ make BOARD=feather_nrf52840_express all

Note: ``BOARD`` can be found as directory name in ``hw/bsp``\ , either in its family/boards or directly under bsp (no family).
Note: some examples especially those that uses Vendor class (e.g webUSB) may requires udev permission on Linux (and/or macOS) to access usb device. It depends on your OS distro, typically copy ``/examples/device/99-tinyusb.rules`` file to /etc/udev/rules.d/ then run ``sudo udevadm control --reload-rules && sudo udevadm trigger`` is good enough.

Port Selection
~~~~~~~~~~~~~~

If a board has several ports, one port is chosen by default in the individual board.mk file. Use option ``PORT=x`` To choose another port. For example to select the HS port of a STM32F746Disco board, use:

.. code-block::

   $ make BOARD=stm32f746disco PORT=1 all

Port Speed
~~~~~~~~~~

A MCU can support multiple operational speed. By default, the example build system will use the fastest supported on the board. Use option ``SPEED=full/high`` e.g To force F723 operate at full instead of default high speed

.. code-block::

   $ make BOARD=stm32f746disco SPEED=full all

Size Analysis
~~~~~~~~~~~~~

First install `linkermap tool <https://github.com/hathach/linkermap>`_ then ``linkermap`` target can be used to analyze code size. You may want to compile with ``NO_LTO=1`` since -flto merges code across .o files and make it difficult to analyze.

.. code-block::

   $ make BOARD=feather_nrf52840_express NO_LTO=1 all linkermap

Debug
^^^^^

To compile for debugging add ``DEBUG=1``\ , for example

.. code-block::

   $ make BOARD=feather_nrf52840_express DEBUG=1 all

Log
~~~

Should you have an issue running example and/or submitting an bug report. You could enable TinyUSB built-in debug logging with optional ``LOG=``. LOG=1 will only print out error message, LOG=2 print more information with on-going events. LOG=3 or higher is not used yet. 

.. code-block::

   $ make BOARD=feather_nrf52840_express LOG=2 all

Logger
~~~~~~

By default log message is printed via on-board UART which is slow and take lots of CPU time comparing to USB speed. If your board support on-board/external debugger, it would be more efficient to use it for logging. There are 2 protocols: 


* `LOGGER=rtt`: use `Segger RTT protocol <https://www.segger.com/products/debug-probes/j-link/technology/about-real-time-transfer/>`_

  * Cons: requires jlink as the debugger.
  * Pros: work with most if not all MCUs
  * Software viewer is JLink RTT Viewer/Client/Logger which is bundled with JLink driver package.

* ``LOGGER=swo``\ : Use dedicated SWO pin of ARM Cortex SWD debug header.

  * Cons: only work with ARM Cortex MCUs minus M0
  * Pros: should be compatible with more debugger that support SWO.
  * Software viewer should be provided along with your debugger driver.

.. code-block::

   $ make BOARD=feather_nrf52840_express LOG=2 LOGGER=rtt all
   $ make BOARD=feather_nrf52840_express LOG=2 LOGGER=swo all

Flash
^^^^^

``flash`` target will use the default on-board debugger (jlink/cmsisdap/stlink/dfu) to flash the binary, please install those support software in advance. Some board use bootloader/DFU via serial which is required to pass to make command

.. code-block::

   $ make BOARD=feather_nrf52840_express flash
   $ make SERIAL=/dev/ttyACM0 BOARD=feather_nrf52840_express flash

Since jlink can be used with most of the boards, there is also ``flash-jlink`` target for your convenience.

.. code-block::

   $ make BOARD=feather_nrf52840_express flash-jlink

Some board use uf2 bootloader for drag & drop in to mass storage device, uf2 can be generated with ``uf2`` target

.. code-block::

   $ make BOARD=feather_nrf52840_express all uf2

IAR Support
^^^^^^^^^^^

IAR Project Connection files are provided to import TinyUSB stack into your project.

* A buldable project of your MCU need to be created in advance.


  * Take example of STM32F0:
  
    -  You need `stm32l0xx.h`, `startup_stm32f0xx.s`, `system_stm32f0xx.c`.

    - `STM32L0xx_HAL_Driver` is only needed to run examples, TinyUSB stack itself doesn't rely on MCU's SDKs.

* Open `Tools -> Configure Custom Argument Variables` (Switch to `Global` tab if you want to do it for all your projects) 
   Click `New Group ...`, name it to `TUSB`, Click `Add Variable ...`, name it to `TUSB_DIR`, change it's value to the path of your TinyUSB stack,
   for example `C:\\tinyusb`

Import stack only
~~~~~~~~~~~~~~~~~

1. Open `Project -> Add project Connection ...`, click `OK`, choose `tinyusb\\tools\\iar_template.ipcf`.

Run examples
~~~~~~~~~~~~

1. (Python3 is needed) Run `iar_gen.py` to generate .ipcf files of examples:

   .. code-block::

     cd C:\tinyusb\tools
     python iar_gen.py

2. Open `Project -> Add project Connection ...`, click `OK`, choose `tinyusb\\examples\\(.ipcf of example)`.
   For example `C:\\tinyusb\\examples\\device\\cdc_msc\\iar_cdc_msc.ipcf`
