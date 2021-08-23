.. figure:: docs/assets/logo.svg
   :alt: TinyUSB

|Build Status| |Documentation Status| |License|

TinyUSB is an open-source cross-platform USB Host/Device stack for
embedded system, designed to be memory-safe with no dynamic allocation
and thread-safe with all interrupt events are deferred then handled in
the non-ISR task function.

Please take a look at the online `documentation <https://docs.tinyusb.org/>`__.

.. figure:: docs/assets/stack.svg
   :width: 500px
   :alt: stackup

::

	.
	├── docs            # Documentation
	├── examples        # Sample with Makefile build support
	├── hw
	│   ├── bsp         # Supported boards source files
	│   └── mcu         # Low level mcu core & peripheral drivers
	├── lib             # Sources from 3rd party such as freeRTOS, fatfs ...
	├── src             # All sources files for TinyUSB stack itself.
	├── test            # Unit tests for the stack
	└── tools           # Files used internally

Supported MCUs
==============

The stack supports the following MCUs:

- **Dialog:** DA1469x
- **Espressif:** ESP32-S2, ESP32-S3
- **MicroChip:** SAMD11, SAMD21, SAMD51, SAME5x, SAMG55, SAML21, SAML22, SAME7x
- **NordicSemi:** nRF52833, nRF52840
- **Nuvoton:** NUC120, NUC121/NUC125, NUC126, NUC505
- **NXP:**

  - iMX RT Series: RT1011, RT1015, RT1021, RT1052, RT1062, RT1064
  - Kinetis: KL25
  - LPC Series: 11u, 13, 15, 17, 18, 40, 43, 51u, 54, 55

- **Raspberry Pi:** RP2040
- **Renesas:** RX63N, RX65N
- **Silabs:** EFM32GG12
- **Sony:** CXD56
- **ST:** STM32 series: L0, F0, F1, F2, F3, F4, F7, H7 both FullSpeed and HighSpeed
- **TI:** MSP430
- **ValentyUSB:** eptri

Here is the list of `Supported Devices`_ that can be used with provided examples.

Device Stack
============

Supports multiple device configurations by dynamically changing usb descriptors. Low power functions such like suspend, resume, and remote wakeup. Following device classes are supported:

-  Audio Class 2.0 (UAC2)
-  Bluetooth Host Controller Interface (BTH HCI)
-  Communication Class (CDC)
-  Device Firmware Update (DFU): DFU mode (WIP) and Runtinme
-  Human Interface Device (HID): Generic (In & Out), Keyboard, Mouse, Gamepad etc ...
-  Mass Storage Class (MSC): with multiple LUNs
-  Musical Instrument Digital Interface (MIDI)
-  Network with RNDIS, CDC-ECM (work in progress)
-  USB Test and Measurement Class (USBTMC)
-  Vendor-specific class support with generic In & Out endpoints. Can be used with MS OS 2.0 compatible descriptor to load winUSB driver without INF file.
-  `WebUSB <https://github.com/WICG/webusb>`__ with vendor-specific class

If you have special need, `usbd_app_driver_get_cb()` can be used to write your own class driver without modifying the stack. Here is how RPi team add their reset interface [raspberrypi/pico-sdk#197](https://github.com/raspberrypi/pico-sdk/pull/197)

Host Stack
==========

- Human Interface Device (HID): Keyboard, Mouse, Generic
- Mass Storage Class (MSC)
- Hub currently only supports 1 level of hub (due to my laziness)

OS Abstraction layer
====================

TinyUSB is completely thread-safe by pushing all ISR events into a central queue, then process it later in the non-ISR context task function. It also uses semaphore/mutex to access shared resources such as CDC FIFO. Therefore the stack needs to use some of OS's basic APIs. Following OSes are already supported out of the box.

- **No OS**
- **FreeRTOS**
- **Mynewt** Due to the newt package build system, Mynewt examples are better to be on its [own repo](https://github.com/hathach/mynewt-tinyusb-example)

Local Docs
==========

- Info

  - `Uses`_
  - `Changelog`_
  - `Contributors`_

- `Reference`_

  - `Supported Devices`_
  - `Gettin Started`_
  - `Concurrency`_

- `Contributing`_

  - `Code of Conduct`_
  - `Structure`_
  - `Porting`_

License
=======

All TinyUSB sources in the ``src`` folder are licensed under MIT
license, `Full license is here <LICENSE>`__. However, each file can be
individually licensed especially those in ``lib`` and ``hw/mcu`` folder.
Please make sure you understand all the license term for files you use
in your project.


.. |Build Status| image:: https://github.com/hathach/tinyusb/workflows/Build/badge.svg
   :target: https://github.com/hathach/tinyusb/actions
.. |Documentation Status| image:: https://readthedocs.org/projects/tinyusb/badge/?version=latest
   :target: https://docs.tinyusb.org/en/latest/?badge=latest
.. |License| image:: https://img.shields.io/badge/license-MIT-brightgreen.svg
   :target: https://opensource.org/licenses/MIT


.. _Uses: docs/info/uses.rst
.. _Changelog: docs/info/changelog.rst
.. _Contributors: CONTRIBUTORS.rst
.. _Reference: docs/reference/index.rst
.. _Supported Devices: docs/reference/supported.rst
.. _Gettin Started: docs/reference/getting_started.rst
.. _Concurrency: docs/reference/concurrency.rst
.. _Contributing: docs/contributing/index.rst
.. _Code of Conduct: CODE_OF_CONDUCT.rst
.. _Structure: docs/contributing/structure.rst
.. _Porting: docs/contributing/porting.rst
