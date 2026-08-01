*************
Class Drivers
*************

Class drivers implement USB protocols such as CDC serial, HID, mass storage,
and MIDI.  Enable only the classes your product uses: device APIs start with
``tud_`` and host APIs start with ``tuh_``.

Start with :doc:`device` or :doc:`host`, then use the class page for its
configuration, data flow, callbacks, and examples.  The API lists below focus
on the calls normally needed by an application; the public headers remain the
complete API reference.

Support matrix
==============

.. list-table::
   :header-rows: 1
   :widths: 24 18 18 40

   * - Class
     - Device
     - Host
     - Guide
   * - Audio 1.0/2.0
     - Yes
     - No
     - :doc:`audio`
   * - Bluetooth HCI
     - Yes
     - No
     - :doc:`bluetooth`
   * - CDC serial
     - Yes
     - Yes
     - :doc:`cdc`
   * - DFU 1.1
     - Yes
     - No
     - :doc:`dfu`
   * - HID 1.11
     - Yes
     - Yes
     - :doc:`hid`
   * - MIDI 1.0/2.0
     - Yes
     - Yes
     - :doc:`midi`
   * - Mass Storage (BOT)
     - Yes
     - Yes
     - :doc:`msc`
   * - Media Transfer (MTP)
     - Yes
     - No
     - :doc:`mtp`
   * - Network (ECM/RNDIS/NCM)
     - Yes
     - No
     - :doc:`network`
   * - Printer
     - Yes
     - No
     - :doc:`printer`
   * - Test and Measurement (USBTMC)
     - Yes
     - No
     - :doc:`usbtmc`
   * - Vendor-specific
     - Yes
     - Custom driver
     - :doc:`vendor`
   * - Video 1.5
     - Yes
     - No
     - :doc:`video`

.. toctree::
   :maxdepth: 1
   :hidden:

   device
   host
   audio
   bluetooth
   cdc
   dfu
   hid
   midi
   msc
   mtp
   network
   printer
   usbtmc
   vendor
   video
