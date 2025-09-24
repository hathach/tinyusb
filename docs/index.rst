TinyUSB Documentation
=====================

TinyUSB is an open-source cross-platform USB Host/Device stack for embedded systems, designed to be memory-safe with no dynamic allocation and thread-safe with all interrupt events deferred to non-ISR task functions.

For Developers
--------------

TinyUSB provides a complete USB stack implementation supporting both device and host modes across a wide range of microcontrollers. The stack is designed for resource-constrained embedded systems with emphasis on code size, memory efficiency, and real-time performance.

**Key Features:**

* **Thread-safe design**: All USB interrupts are deferred to task context
* **Memory-safe**: No dynamic allocation, all buffers are statically allocated
* **Portable**: Supports 30+ MCU families from major vendors
* **Comprehensive**: Device classes (CDC, HID, MSC, Audio, etc.) and Host stack
* **RTOS support**: Works with bare metal, FreeRTOS, RT-Thread, and Mynewt

**Quick Navigation:**

* New to TinyUSB? Start with :doc:`tutorials/getting_started`
* Need to solve a specific problem? Check :doc:`guides/index`
* Looking for API details? See :doc:`reference/index`
* Want to understand the design? Read :doc:`explanation/architecture`
* Having issues? Check :doc:`faq` and :doc:`troubleshooting`

Documentation Structure
-----------------------

.. toctree::
   :maxdepth: 2
   :caption: Information

   explanation/index
   tutorials/index
   guides/index
   reference/index
   faq
   troubleshooting

.. toctree::
   :maxdepth: 1
   :caption: Project Info

   info/index
   contributing/index

.. toctree::
   :caption: External Links
   :hidden:

   Source Code <https://github.com/hathach/tinyusb>
   Issue Tracker <https://github.com/hathach/tinyusb/issues>
   Discussions <https://github.com/hathach/tinyusb/discussions>

GitHub Project Main README
==========================

.. include:: ../README_processed.rst
