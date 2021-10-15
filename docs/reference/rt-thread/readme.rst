TinyUSB for RT-Thread Port
==========================

`中文 <./readme_zh.rst>`__ \| English

TinyUSB is an open source cross-platform USB stack for embedded system.

1、 Getting start
-----------------

The specific path in RT-Thread package manager is as follows:

.. code:: text

    -> RT-Thread online packages
        -> system packages
            --- TinyUSB (offical): an open source cross-platform USB stack for embedded system
                (2048) TinyUSB thread stack size
                [*]   Using USB device  ---->
                    [*]   Using Communication Device Class (CDC)
                    [*]   Using Mass Storage Class (MSC)
                    ()      The name of the device used by MSC
                      Version (latest)  --->

The configuration instructions for each function are as follows:

-  TinyUSB thread stack size
-  Whether to use a USB device
-  Using CDC
-  Using MSC
-  Name of the block device used for MSC read/write

Then let the RT-Thread package manager automatically update, or use the
``pkgs --update`` command to update the package to the BSP.

2、Support
----------

2.1、MCU
~~~~~~~~

Currently only the STM32 family of MCUs is supported.

2.2、Device class
~~~~~~~~~~~~~~~~~

-  Communication Device Class (CDC)
-  Mass Storage Class (MSC)

3、Feedback
-----------

issue: `tfx2001/tinyusb <https://github.com/tfx2001/tinyusb/issues>`__
