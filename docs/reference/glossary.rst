********
Glossary
********

.. glossary::

   BSP
      Board Support Package. A collection of board-specific code that provides hardware abstraction for a particular development board, including pin mappings, clock settings, linker scripts, and hardware initialization routines. Located in ``hw/bsp/FAMILY/boards/BOARD_NAME``.

   Bulk Transfer
      USB transfer type used for large amounts of data that doesn't require guaranteed timing. Used by mass storage devices and CDC class.

   CDC
      Communications Device Class. USB class for devices that communicate serial data, creating virtual serial ports.

   Control Transfer
      USB transfer type used for device configuration and control. All USB devices must support control transfers on endpoint 0.

   DCD
      Device Controller Driver. The hardware abstraction layer for USB device controllers in TinyUSB. See also HCD.

   Descriptor
      Data structures that describe USB device capabilities, configuration, and interfaces to the host.

   Device Class
      USB specification defining how devices of a particular type (e.g., storage, audio, HID) communicate with hosts.

   DFU
      Device Firmware Update. USB class that allows firmware updates over USB.

   Endpoint
      Communication channel between host and device. Each endpoint has a direction (IN/OUT) and transfer type.

   Enumeration
      Process where USB host discovers and configures a newly connected device.

   HCD
      Host Controller Driver. The hardware abstraction layer for USB host controllers in TinyUSB. See also DCD.

   HID
      Human Interface Device. USB class for input devices like keyboards, mice, and game controllers.

   High Speed
      USB 2.0 speed mode operating at 480 Mbps.

   Full Speed
      USB speed mode operating at 12 Mbps, supported by USB 1.1 and 2.0.

   Low Speed
      USB speed mode operating at 1.5 Mbps, typically used by simple input devices.

   Interrupt Transfer
      USB transfer type for small, time-sensitive data with guaranteed maximum latency.

   Isochronous Transfer
      USB transfer type for time-critical data like audio/video with guaranteed bandwidth but no error correction.

   MSC
      Mass Storage Class. USB class for storage devices like USB drives.

   OSAL
      Operating System Abstraction Layer. TinyUSB component that abstracts RTOS differences.

   OTG
      On-The-Go. USB specification allowing devices to act as both host and device.

   Pipe
      Host-side communication channel to a device endpoint.

   Root Hub
      The USB hub built into the host controller, where devices connect directly.

   Stall
      USB protocol mechanism where an endpoint responds with a STALL handshake to indicate an error condition or unsupported request. Used for error handling, not flow control.

   Super Speed
      USB 3.0 speed mode operating at 5 Gbps. Not supported by TinyUSB.

   tud
      TinyUSB Device. Function prefix for all device stack APIs (e.g., ``tud_task()``, ``tud_cdc_write()``).

   tuh
      TinyUSB Host. Function prefix for all host stack APIs (e.g., ``tuh_task()``, ``tuh_cdc_receive()``).

   UAC
      USB Audio Class. USB class for audio devices.

   UVC
      USB Video Class. USB class for video devices like cameras.

   VID
      Vendor Identifier. 16-bit number assigned by USB-IF to identify device manufacturers.

   PID
      Product Identifier. 16-bit number assigned by vendor to identify specific products.

   USB-IF
      USB Implementers Forum. Organization that maintains USB specifications and assigns VIDs.