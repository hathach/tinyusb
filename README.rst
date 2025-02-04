|Build Status| |CircleCI Status| |Documentation Status| |Fuzzing Status| |License|

Sponsors
========

TinyUSB is funded by: Adafruit. Purchasing products from them helps to support this project.

.. figure:: docs/assets/adafruit_logo.svg
   :alt: Adafruit Logo
   :target: https://www.adafruit.com

TinyUSB Project
===============

.. figure:: docs/assets/logo.svg
   :alt: TinyUSB

TinyUSB is an open-source cross-platform USB Host/Device stack for
embedded system, designed to be memory-safe with no dynamic allocation
and thread-safe with all interrupt events are deferred then handled in
the non-ISR task function. Check out the online `documentation <https://docs.tinyusb.org/>`__ for more details.

.. figure:: docs/assets/stack.svg
   :width: 500px
   :alt: stackup

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


Getting started
===============

See the `online documentation <https://docs.tinyusb.org>`_ for information about using TinyUSB and how it is implemented.

Check out `Getting Started`_ guide for adding TinyUSB to your project or building the examples. If you are new to TinyUSB, we recommend starting with the `cdc_msc` example. There is a handful of `Supported Boards`_ that should work out of the box.

We use `GitHub Discussions <https://github.com/hathach/tinyusb/discussions>`_ as our forum. It is a great place to ask questions and advice from the community or to discuss your TinyUSB-based projects.

For bugs and feature requests, please `raise an issue <https://github.com/hathach/tinyusb/issues>`_ and follow the templates there.

See `Porting`_ guide for adding support for new MCUs and boards.

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
- Vendor serial over USB: FTDI, CP210x, CH34x
- Hub with multiple-level support

Similar to the Device Stack, if you have a special requirement, `usbh_app_driver_get_cb()` can be used to write your own class driver without modifying the stack.

Power Delivery Stack
====================

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

Supported CPUs
==============

+--------------+-----------------------------+--------+------+-----------+------------------------+-------------------+
| Manufacturer | Family                      | Device | Host | Highspeed | Driver                 | Note              |
+==============+=============================+========+======+===========+========================+===================+
| Allwinner    | F1C100s/F1C200s             | ✔      |      | ✔         | sunxi                  | musb variant      |
+--------------+-----------------------------+--------+------+-----------+------------------------+-------------------+
| Analog       | MAX3421E                    |        | ✔    | ✖         | max3421                | via SPI           |
|              +-----------------------------+--------+------+-----------+------------------------+-------------------+
|              | MAX32 650, 666, 690,        | ✔      |      | ✔         | musb                   | 1-dir ep          |
|              | MAX78002                    |        |      |           |                        |                   |
+--------------+-----------------------------+--------+------+-----------+------------------------+-------------------+
| Brigetek     | FT90x                       | ✔      |      | ✔         | ft9xx                  | 1-dir ep          |
+--------------+-----------------------------+--------+------+-----------+------------------------+-------------------+
| Broadcom     | BCM2711, BCM2837            | ✔      |      | ✔         | dwc2                   |                   |
+--------------+-----------------------------+--------+------+-----------+------------------------+-------------------+
| Dialog       | DA1469x                     | ✔      | ✖    | ✖         | da146xx                |                   |
+--------------+-----------------------------+--------+------+-----------+------------------------+-------------------+
| Espressif    | S2, S3                      | ✔      | ✔    | ✖         | dwc2 or esp32sx        |                   |
|   ESP32      +-----------------------------+--------+------+-----------+------------------------+-------------------+
|              | P4                          | ✔      | ✔    | ✔         | dwc2                   |                   |
+--------------+----+------------------------+--------+------+-----------+------------------------+-------------------+
| GigaDevice   | GD32VF103                   | ✔      |      | ✖         | dwc2                   |                   |
+--------------+-----------------------------+--------+------+-----------+------------------------+-------------------+
| Infineon     | XMC4500                     | ✔      | ✔    | ✖         | dwc2                   |                   |
+--------------+-----+-----------------------+--------+------+-----------+------------------------+-------------------+
| MicroChip    | SAM | D11, D21, L21, L22    | ✔      |      | ✖         | samd                   |                   |
|              |     +-----------------------+--------+------+-----------+------------------------+-------------------+
|              |     | D51, E5x              | ✔      |      | ✖         | samd                   |                   |
|              |     +-----------------------+--------+------+-----------+------------------------+-------------------+
|              |     | G55                   | ✔      |      | ✖         | samg                   | 1-dir ep          |
|              |     +-----------------------+--------+------+-----------+------------------------+-------------------+
|              |     | E70,S70,V70,V71       | ✔      |      | ✔         | samx7x                 | 1-dir ep          |
|              +-----+-----------------------+--------+------+-----------+------------------------+-------------------+
|              | PIC | 24                    | ✔      |      |           | pic                    | ci_fs variant     |
|              |     +-----------------------+--------+------+-----------+------------------------+-------------------+
|              |     | 32 mm, mk, mx         | ✔      |      |           | pic                    | ci_fs variant     |
|              |     +-----------------------+--------+------+-----------+------------------------+-------------------+
|              |     | dsPIC33               | ✔      |      |           | pic                    | ci_fs variant     |
|              |     +-----------------------+--------+------+-----------+------------------------+-------------------+
|              |     | 32mz                  | ✔      |      |           | pic32mz                | musb variant      |
+--------------+-----+-----------------------+--------+------+-----------+------------------------+-------------------+
| Mind Montion | mm32                        | ✔      |      | ✖         | mm32f327x_otg          | ci_fs variant     |
+--------------+-----+-----------------------+--------+------+-----------+------------------------+-------------------+
| NordicSemi   | nRF 52833, 52840, 5340      | ✔      | ✖    | ✖         | nrf5x                  | only ep8 is ISO   |
+--------------+-----------------------------+--------+------+-----------+------------------------+-------------------+
| Nuvoton      | NUC120                      | ✔      | ✖    | ✖         | nuc120                 |                   |
|              +-----------------------------+--------+------+-----------+------------------------+-------------------+
|              | NUC121/NUC125               | ✔      | ✖    | ✖         | nuc121                 |                   |
|              +-----------------------------+--------+------+-----------+------------------------+-------------------+
|              | NUC126                      | ✔      | ✖    | ✖         | nuc121                 |                   |
|              +-----------------------------+--------+------+-----------+------------------------+-------------------+
|              | NUC505                      | ✔      |      | ✔         | nuc505                 |                   |
+--------------+---------+-------------------+--------+------+-----------+------------------------+-------------------+
| NXP          | iMXRT   | RT 10xx, 11xx     | ✔      | ✔    | ✔         | ci_hs                  |                   |
|              +---------+-------------------+--------+------+-----------+------------------------+-------------------+
|              | Kinetis | KL                | ✔      | ⚠    | ✖         | ci_fs, khci            |                   |
|              |         +-------------------+--------+------+-----------+------------------------+-------------------+
|              |         | K32L2             | ✔      |      | ✖         | khci                   | ci_fs variant     |
|              +---------+-------------------+--------+------+-----------+------------------------+-------------------+
|              | LPC     | 11u, 13, 15       | ✔      | ✖    | ✖         | lpc_ip3511             |                   |
|              |         +-------------------+--------+------+-----------+------------------------+-------------------+
|              |         | 17, 40            | ✔      | ⚠    | ✖         | lpc17_40               |                   |
|              |         +-------------------+--------+------+-----------+------------------------+-------------------+
|              |         | 18, 43            | ✔      | ✔    | ✔         | ci_hs                  |                   |
|              |         +-------------------+--------+------+-----------+------------------------+-------------------+
|              |         | 51u               | ✔      | ✖    | ✖         | lpc_ip3511             |                   |
|              |         +-------------------+--------+------+-----------+------------------------+-------------------+
|              |         | 54, 55            | ✔      |      | ✔         | lpc_ip3511             |                   |
|              +---------+-------------------+--------+------+-----------+------------------------+-------------------+
|              | MCX     | N9, A15           | ✔      |      | ✔         | ci_fs, ci_hs           |                   |
+--------------+---------+-------------------+--------+------+-----------+------------------------+-------------------+
| Raspberry Pi | RP2040, RP2350              | ✔      | ✔    | ✖         | rp2040, pio_usb        |                   |
+--------------+-----+-----------------------+--------+------+-----------+------------------------+-------------------+
| Renesas      | RX  | 63N, 65N, 72N         | ✔      | ✔    | ✖         | rusb2                  |                   |
|              +-----+-----------------------+--------+------+-----------+------------------------+-------------------+
|              | RA  | 4M1, 4M3, 6M1         | ✔      | ✔    | ✖         | rusb2                  |                   |
|              |     +-----------------------+--------+------+-----------+------------------------+-------------------+
|              |     | 6M5                   | ✔      | ✔    | ✔         | rusb2                  |                   |
+--------------+-----+-----------------------+--------+------+-----------+------------------------+-------------------+
| Silabs       | EFM32GG12                   | ✔      |      | ✖         | dwc2                   |                   |
+--------------+-----------------------------+--------+------+-----------+------------------------+-------------------+
| Sony         | CXD56                       | ✔      | ✖    | ✔         | cxd56                  |                   |
+--------------+-----------------------------+--------+------+-----------+------------------------+-------------------+
| ST STM32     | F0                          | ✔      | ✖    | ✖         | stm32_fsdev            |                   |
|              +----+------------------------+--------+------+-----------+------------------------+-------------------+
|              | F1 | 102, 103               | ✔      | ✖    | ✖         | stm32_fsdev            |                   |
|              |    +------------------------+--------+------+-----------+------------------------+-------------------+
|              |    | 105, 107               | ✔      | ✔    | ✖         | dwc2                   |                   |
|              +----+------------------------+--------+------+-----------+------------------------+-------------------+
|              | F2, F4, F7, H7              | ✔      | ✔    | ✔         | dwc2                   |                   |
|              +-----------------------------+--------+------+-----------+------------------------+-------------------+
|              | F3                          | ✔      | ✖    | ✖         | stm32_fsdev            |                   |
|              +-----------------------------+--------+------+-----------+------------------------+-------------------+
|              | C0, G0, H5                  | ✔      |      | ✖         | stm32_fsdev            |                   |
|              +-----------------------------+--------+------+-----------+------------------------+-------------------+
|              | G4                          | ✔      | ✖    | ✖         | stm32_fsdev            |                   |
|              +-----------------------------+--------+------+-----------+------------------------+-------------------+
|              | L0, L1                      | ✔      | ✖    | ✖         | stm32_fsdev            |                   |
|              +----+------------------------+--------+------+-----------+------------------------+-------------------+
|              | L4 | 4x2, 4x3               | ✔      | ✖    | ✖         | stm32_fsdev            |                   |
|              |    +------------------------+--------+------+-----------+------------------------+-------------------+
|              |    | 4x5, 4x6               | ✔      | ✔    | ✖         | dwc2                   |                   |
|              +----+------------------------+--------+------+-----------+------------------------+-------------------+
|              | L4+                         | ✔      | ✔    | ✖         | dwc2                   |                   |
|              +-----------------------------+--------+------+-----------+------------------------+-------------------+
|              | L5                          | ✔      | ✖    | ✖         | stm32_fsdev            |                   |
|              +----+------------------------+--------+------+-----------+------------------------+-------------------+
|              | U5 | 535, 545               | ✔      |      | ✖         | stm32_fsdev            |                   |
|              |    +------------------------+--------+------+-----------+------------------------+-------------------+
|              |    | 575, 585               | ✔      | ✔    | ✖         | dwc2                   |                   |
|              |    +------------------------+--------+------+-----------+------------------------+-------------------+
|              |    | 59x,5Ax,5Fx,5Gx        | ✔      | ✔    | ✔         | dwc2                   |                   |
|              +----+------------------------+--------+------+-----------+------------------------+-------------------+
|              | WBx5                        | ✔      | ✖    | ✖         | stm32_fsdev            |                   |
+--------------+-----------------------------+--------+------+-----------+------------------------+-------------------+
| TI           | MSP430                      | ✔      | ✖    | ✖         | msp430x5xx             |                   |
|              +-----------------------------+--------+------+-----------+------------------------+-------------------+
|              | MSP432E4                    | ✔      |      | ✖         | musb                   |                   |
|              +-----------------------------+--------+------+-----------+------------------------+-------------------+
|              | TM4C123                     | ✔      |      | ✖         | musb                   |                   |
+--------------+-----------------------------+--------+------+-----------+------------------------+-------------------+
| ValentyUSB   | eptri                       | ✔      | ✖    | ✖         | eptri                  |                   |
+--------------+-----------------------------+--------+------+-----------+------------------------+-------------------+
| WCH          | CH32F20x                    | ✔      |      | ✔         | ch32_usbhs             |                   |
|              +-----------------------------+--------+------+-----------+------------------------+-------------------+
|              | CH32V20x                    | ✔      |      | ✖         | stm32_fsdev/ch32_usbfs |                   |
|              +-----------------------------+--------+------+-----------+------------------------+-------------------+
|              | CH32V307                    | ✔      |      | ✔         | ch32_usbfs/hs          |                   |
+--------------+-----------------------------+--------+------+-----------+------------------------+-------------------+

Table Legend
------------

========= =========================
✔         Supported
⚠         Partial support
✖         Not supported by hardware
\[empty\] Unknown
========= =========================


.. |Build Status| image:: https://github.com/hathach/tinyusb/actions/workflows/build.yml/badge.svg
   :target: https://github.com/hathach/tinyusb/actions
.. |CircleCI Status| image:: https://dl.circleci.com/status-badge/img/circleci/4AYHvUhFxdnY4rA7LEsdqW/QmrpoL2AjGqetvFQNqtWyq/tree/master.svg?style=svg
   :target: https://dl.circleci.com/status-badge/redirect/circleci/4AYHvUhFxdnY4rA7LEsdqW/QmrpoL2AjGqetvFQNqtWyq/tree/master
.. |Documentation Status| image:: https://readthedocs.org/projects/tinyusb/badge/?version=latest
   :target: https://docs.tinyusb.org/en/latest/?badge=latest
.. |Fuzzing Status| image:: https://oss-fuzz-build-logs.storage.googleapis.com/badges/tinyusb.svg
   :target: https://oss-fuzz-build-logs.storage.googleapis.com/index.html#tinyusb
.. |License| image:: https://img.shields.io/badge/license-MIT-brightgreen.svg
   :target: https://opensource.org/licenses/MIT


.. _Changelog: docs/info/changelog.rst
.. _Contributors: CONTRIBUTORS.rst
.. _Getting Started: docs/reference/getting_started.rst
.. _Supported Boards: docs/reference/boards.rst
.. _Dependencies: docs/reference/dependencies.rst
.. _Concurrency: docs/reference/concurrency.rst
.. _Contributing: docs/contributing/index.rst
.. _Code of Conduct: CODE_OF_CONDUCT.rst
.. _Porting: docs/contributing/porting.rst
