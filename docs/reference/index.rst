*********
Reference
*********

.. figure:: ../assets/stack.svg
   :width: 1600px
   :alt: TinyUSB

::

    .
    ├── docs            # Documentation
    ├── examples        # Examples with make and cmake build system
    ├── hw
    │   ├── bsp         # Supported boards source files
    │   └── mcu         # Low level mcu core & peripheral drivers
    ├── lib             # Sources from 3rd party such as freeRTOS, fatfs ...
    ├── src             # All sources files for TinyUSB stack itself.
    ├── test            # Tests: unit test, fuzzing, hardware test
    └── tools           # Files used internally


Device Stack
============

Supports multiple device configurations by dynamically changing USB descriptors, low power functions such like suspend, resume, and remote wakeup. The following device classes are supported:

-  Audio Class 2.0 (UAC2)
-  Bluetooth Host Controller Interface (BTH HCI)
-  Communication Device Class (CDC)
-  Device Firmware Update (DFU): DFU mode (WIP) and Runtime
-  Human Interface Device (HID): Generic (In & Out), Keyboard, Mouse, Gamepad etc ...
-  Mass Storage Class (MSC): with multiple LUNs
-  Musical Instrument Digital Interface (MIDI)
-  Network with RNDIS, Ethernet Control Model (ECM), Network Control Model (NCM)
-  Test and Measurement Class (USBTMC)
-  Video class 1.5 (UVC): work in progress
-  Vendor-specific class support with generic In & Out endpoints. Can be used with MS OS 2.0 compatible descriptor to load winUSB driver without INF file.
-  `WebUSB <https://github.com/WICG/webusb>`__ with vendor-specific class

If you have a special requirement, `usbd_app_driver_get_cb()` can be used to write your own class driver without modifying the stack. Here is how the RPi team added their reset interface `raspberrypi/pico-sdk#197 <https://github.com/raspberrypi/pico-sdk/pull/197>`_

Host Stack
==========

- Human Interface Device (HID): Keyboard, Mouse, Generic
- Mass Storage Class (MSC)
- Communication Device Class: CDC-ACM
- Vendor serial over USB: FTDI, CP210x
- Hub with multiple-level support

Similar to the Device Stack, if you have a special requirement, `usbh_app_driver_get_cb()` can be used to write your own class driver without modifying the stack.

TypeC PD Stack
==============

- Power Delivery 3.0 (PD3.0) with USB Type-C support (WIP)
- Super early stage, only for testing purpose
- Only support STM32 G4

OS Abstraction layer
====================

TinyUSB is completely thread-safe by pushing all Interrupt Service Request (ISR) events into a central queue, then processing them later in the non-ISR context task function. It also uses semaphore/mutex to access shared resources such as Communication Device Class (CDC) FIFO. Therefore the stack needs to use some of the OS's basic APIs. Following OSes are already supported out of the box.

- **No OS**
- **FreeRTOS**
- `RT-Thread <https://github.com/RT-Thread/rt-thread>`_: `repo <https://github.com/RT-Thread-packages/tinyusb>`_
- **Mynewt** Due to the newt package build system, Mynewt examples are better to be on its `own repo <https://github.com/hathach/mynewt-tinyusb-example>`_

License
=======

All TinyUSB sources in the `src` folder are licensed under MIT license. However, each file can be individually licensed especially those in `lib` and `hw/mcu` folder. Please make sure you understand all the license term for files you use in your project.

Index
=====

.. toctree::
   :maxdepth: 2

   supported
   getting_started
   dependencies
   concurrency
