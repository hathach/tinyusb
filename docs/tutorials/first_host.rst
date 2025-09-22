******************
Your First USB Host
******************

This tutorial guides you through creating a simple USB host application that can connect to and communicate with USB devices.

Prerequisites
=============

* Completed :doc:`getting_started` and :doc:`first_device` tutorials
* Development board with USB host capability (e.g., STM32F4 Discovery with USB-A connector)
* USB device to test with (USB drive, mouse, keyboard, or CDC device)

Understanding USB Host Basics
=============================

A USB host application needs:

1. **Device Enumeration**: Detect and configure connected devices
2. **Class Drivers**: Handle communication with specific device types
3. **Application Logic**: Process data from/to the connected devices

Step 1: Start with an Example
=============================

Use the ``cdc_msc_hid`` host example:

.. code-block:: bash

   cd examples/host/cdc_msc_hid

This example can communicate with CDC (serial), MSC (storage), and HID (keyboard/mouse) devices.

Step 2: Understand the Code Structure
=====================================

Key components:

* ``main.c`` - Main loop and device event handling
* Host callbacks - Functions called when devices connect/disconnect
* Class-specific handlers - Process data from different device types

**Main Loop Pattern**:

.. code-block:: c

   int main(void) {
     board_init();
     tusb_init();

     while (1) {
       tuh_task(); // TinyUSB host task
       // Handle connected devices
     }
   }

**Connection Events**: TinyUSB calls your callbacks when devices connect:

.. code-block:: c

   void tuh_mount_cb(uint8_t dev_addr) {
     printf("Device connected, address = %d\\n", dev_addr);
   }

   void tuh_umount_cb(uint8_t dev_addr) {
     printf("Device disconnected, address = %d\\n", dev_addr);
   }

Step 3: Build and Test
======================

.. code-block:: bash

   # Fetch dependencies
   python ../../../tools/get_deps.py stm32f4

   # Build
   make BOARD=stm32f407disco all

   # Flash
   make BOARD=stm32f407disco flash

**Testing**: Connect different USB devices and observe the output via serial console.

Step 4: Handle Specific Device Types
====================================

**Mass Storage (USB Drive)**:

.. code-block:: c

   void tuh_msc_mount_cb(uint8_t dev_addr) {
     printf("USB Drive mounted\\n");
     // Read/write files
   }

**HID Devices (Keyboard/Mouse)**:

.. code-block:: c

   void tuh_hid_mount_cb(uint8_t dev_addr, uint8_t instance,
                         uint8_t const* desc_report, uint16_t desc_len) {
     uint8_t const itf_protocol = tuh_hid_interface_protocol(dev_addr, instance);
     if (itf_protocol == HID_ITF_PROTOCOL_KEYBOARD) {
       printf("Keyboard connected\\n");
     }
   }

**CDC Devices (Serial)**:

.. code-block:: c

   void tuh_cdc_mount_cb(uint8_t idx) {
     printf("CDC device mounted\\n");
     // Configure serial settings
     tuh_cdc_set_baudrate(idx, 115200, NULL, 0);
   }

Common Issues and Solutions
===========================

**No Device Detection**:

* Check power supply - host mode requires more power than device mode
* Verify USB connector wiring and type (USB-A for host vs USB micro/C for device)
* Enable logging with ``LOG=2`` to see enumeration process

**Enumeration Failures**:

* Some devices need more time - increase timeouts
* Check USB hub support if using a hub
* Verify device is USB 2.0 compatible (USB 3.0 devices should work in USB 2.0 mode)

**Class Driver Issues**:

* Not all devices follow standards perfectly - may need custom handling
* Check device descriptors with USB analyzer tools
* Some composite devices may not be fully supported

Hardware Considerations
=======================

**Power Requirements**:

* Host mode typically requires external power or powered USB hub
* Check board documentation for power limitations
* Some boards need jumper changes to enable host power

**Pin Configuration**:

* Host and device modes often use different USB connectors/pins
* Verify board supports host mode on your chosen port
* Check if OTG (On-The-Go) configuration is needed

Next Steps
==========

* Learn about supported USB classes in :doc:`../reference/usb_classes`
* Understand advanced integration in :doc:`../guides/integration`
* Explore TinyUSB architecture in :doc:`../explanation/architecture`