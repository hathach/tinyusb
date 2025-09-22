************
USB Concepts
************

This document provides a brief introduction to USB protocol fundamentals that are essential for understanding TinyUSB development.

USB Protocol Basics
====================

Universal Serial Bus (USB) is a standardized communication protocol designed for connecting devices to hosts (typically computers). Understanding these core concepts is essential for effective TinyUSB development.

Host and Device Roles
----------------------

**USB Host**: The controlling side of a USB connection (typically a computer). The host:
- Initiates all communication
- Provides power to devices
- Manages the USB bus
- Enumerates and configures devices

**TinyUSB Host Stack**: Enable with ``CFG_TUH_ENABLED=1`` in ``tusb_config.h``. Call ``tuh_task()`` regularly in your main loop. See :doc:`../tutorials/first_host` for implementation details.

**USB Device**: The peripheral side (keyboard, mouse, storage device, etc.). Devices:
- Respond to host requests
- Cannot initiate communication
- Receive power from the host
- Must be enumerated by the host before use

**TinyUSB Device Stack**: Enable with ``CFG_TUD_ENABLED=1`` in ``tusb_config.h``. Call ``tud_task()`` regularly in your main loop. See :doc:`../tutorials/first_device` for implementation details.

**OTG (On-The-Go)**: Some devices can switch between host and device roles dynamically. **TinyUSB Support**: Both stacks can be enabled simultaneously on OTG-capable hardware. See ``examples/dual/`` for dual-role implementations.

USB Transfers
=============

Every USB transfer consists of the host issuing a request, and the device replying to that request. The host is the bus master and initiates all communication.
Devices cannot initiate sending data; for unsolicited incoming data, polling is used by the host.

USB defines four transfer types, each intended for different use cases:

Control Transfers
-----------------

Used for device configuration and control commands.

**Characteristics**:
- Bidirectional (uses both IN and OUT)
- Guaranteed delivery with error detection
- Limited data size (8-64 bytes per packet)
- All devices must support control transfers on endpoint 0

**Usage**: Device enumeration, configuration changes, class-specific commands

**TinyUSB Context**: Handled automatically by the core stack for standard requests; class drivers handle class-specific requests. Endpoint 0 is managed by ``src/device/usbd.c`` and ``src/host/usbh.c``. Configure buffer size with ``CFG_TUD_ENDPOINT0_SIZE`` (typically 64 bytes).

Bulk Transfers
--------------

Used for large amounts of data that don't require guaranteed timing.

**Characteristics**:
- Unidirectional (separate IN and OUT endpoints)
- Guaranteed delivery with error detection
- Large packet sizes (up to 512 bytes for High Speed)
- Uses available bandwidth when no other transfers are active

**Usage**: File transfers, large data communication, CDC serial data

**TinyUSB Context**: Used by MSC (mass storage) and CDC classes for data transfer. Configure endpoint buffer sizes with ``CFG_TUD_MSC_EP_BUFSIZE`` and ``CFG_TUD_CDC_EP_BUFSIZE``. See ``src/class/msc/`` and ``src/class/cdc/`` for implementation details.

Interrupt Transfers
-------------------

Used for small, time-sensitive data with guaranteed maximum latency.

**Characteristics**:
- Unidirectional (separate IN and OUT endpoints)
- Guaranteed delivery with error detection
- Small packet sizes (up to 64 bytes for Full Speed)
- Regular polling interval (1ms to 255ms)

**Usage**: Keyboard/mouse input, sensor data, status updates

**TinyUSB Context**: Used by HID class for input reports. Configure with ``CFG_TUD_HID`` and ``CFG_TUD_HID_EP_BUFSIZE``. Send reports using ``tud_hid_report()`` or ``tud_hid_keyboard_report()``. See ``src/class/hid/`` and HID examples in ``examples/device/hid_*/``.

Isochronous Transfers
---------------------

Used for time-critical streaming data.

**Characteristics**:
- Unidirectional (separate IN and OUT endpoints)
- No error correction (speed over reliability)
- Guaranteed bandwidth
- Real-time delivery

**Usage**: Audio, video streaming

**TinyUSB Context**: Used by Audio class for streaming audio data. Configure with ``CFG_TUD_AUDIO`` and related audio configuration macros. See ``src/class/audio/`` and audio examples in ``examples/device/audio_*/`` for UAC2 implementation.

Endpoints and Addressing
=========================

Endpoint Basics
---------------

**Endpoint**: A communication channel between host and device.

- Each endpoint has a number (0-15) and direction
- Endpoint 0 is reserved for control transfers
- Other endpoints are assigned by device class requirements

**TinyUSB Endpoint Management**: Configure maximum endpoints with ``CFG_TUD_ENDPOINT_MAX``. Endpoints are automatically allocated by enabled classes. See your board's ``usb_descriptors.c`` for endpoint assignments.

**Direction**:
- **OUT**: Host to device (host sends data out)
- **IN**: Device to host (host reads data in)
- Note that in TinyUSB code, for ``tx``/``rx``, the device perspective is used typically: E.g., ``tud_cdc_tx_complete_cb()`` designates the callback executed once the device has completed sending data to the host (in device mode).

**Addressing**: Endpoints are addressed as EPx IN/OUT (e.g., EP1 IN, EP2 OUT)

Endpoint Configuration
----------------------

Each endpoint is configured with:
- **Transfer type**: Control, bulk, interrupt, or isochronous
- **Direction**: IN, OUT, or bidirectional (control only)
- **Maximum packet size**: Depends on USB speed and transfer type
- **Interval**: For interrupt and isochronous endpoints

**TinyUSB Configuration**: Endpoint characteristics are defined in descriptors (``usb_descriptors.c``) and automatically configured by the stack. Buffer sizes are set via ``CFG_TUD_*_EP_BUFSIZE`` macros.

Error Handling and Flow Control
-------------------------------

**Transfer Results**: USB transfers can complete with different results:
- **ACK**: Successful transfer
- **NAK**: Device not ready (used for flow control)
- **STALL**: Error condition or unsupported request
- **Timeout**: Transfer failed to complete in time

**Flow Control in USB**: Unlike network protocols, USB doesn't use congestion control. Instead:
- Devices use NAK responses when not ready to receive data
- Applications implement buffering and proper timing
- Some classes (like CDC) support hardware flow control (RTS/CTS)

**TinyUSB Handling**: Transfer results are represented as ``xfer_result_t`` enum values. The stack automatically handles NAK responses and timing. STALL conditions indicate application-level errors that should be addressed in class drivers.

USB Device States
=================

A USB device progresses through several states:

1. **Attached**: Device is physically connected
2. **Powered**: Device receives power from host
3. **Default**: Device responds to address 0
4. **Address**: Device has been assigned a unique address
5. **Configured**: Device is ready for normal operation
6. **Suspended**: Device is in low-power state

**TinyUSB State Management**: State transitions are handled automatically by ``src/device/usbd.c``. You can implement ``tud_mount_cb()`` and ``tud_umount_cb()`` to respond to configuration changes, and ``tud_suspend_cb()``/``tud_resume_cb()`` for power management.

Device Enumeration Process
==========================

When a device is connected, the host follows this process:

1. **Detection**: Host detects device connection
2. **Reset**: Host resets the device
3. **Descriptor Requests**: Host requests device descriptors
4. **Address Assignment**: Host assigns unique address to device
5. **Configuration**: Host selects and configures device
6. **Class Loading**: Host loads appropriate drivers
7. **Normal Operation**: Device is ready for use

**TinyUSB Role**: The device stack handles steps 1-6 automatically; your application handles step 7.

USB Descriptors
===============

Descriptors are data structures that describe device capabilities:

Device Descriptor
-----------------
Describes the device (VID, PID, USB version, etc.)

Configuration Descriptor
------------------------
Describes device configuration (power requirements, interfaces, etc.)

Interface Descriptor
--------------------
Describes a functional interface (class, endpoints, etc.)

Endpoint Descriptor
-------------------
Describes endpoint characteristics (type, direction, size, etc.)

String Descriptors
------------------
Human-readable strings (manufacturer, product name, etc.)

**TinyUSB Implementation**: You provide descriptors in ``usb_descriptors.c`` via callback functions:
- ``tud_descriptor_device_cb()`` - Device descriptor
- ``tud_descriptor_configuration_cb()`` - Configuration descriptor
- ``tud_descriptor_string_cb()`` - String descriptors

The stack automatically handles descriptor requests during enumeration. See examples in ``examples/device/*/usb_descriptors.c`` for reference implementations.

USB Classes
===========

USB classes define standardized protocols for device types:

**Class Code**: Identifies the device type in descriptors
**Class Driver**: Software that implements the class protocol
**Class Requests**: Standardized commands for the class

**Common TinyUSB-Supported Classes**:
- **CDC (02h)**: Communication devices (virtual serial ports) - Enable with ``CFG_TUD_CDC``
- **HID (03h)**: Human interface devices (keyboards, mice) - Enable with ``CFG_TUD_HID``
- **MSC (08h)**: Mass storage devices (USB drives) - Enable with ``CFG_TUD_MSC``
- **Audio (01h)**: Audio devices (speakers, microphones) - Enable with ``CFG_TUD_AUDIO``
- **MIDI**: MIDI devices - Enable with ``CFG_TUD_MIDI``
- **DFU**: Device Firmware Update - Enable with ``CFG_TUD_DFU``
- **Vendor**: Custom vendor classes - Enable with ``CFG_TUD_VENDOR``

See :doc:`../reference/usb_classes` for detailed class information and :doc:`../reference/configuration` for configuration options.

USB Speeds
==========

USB supports multiple speed modes:

**Low Speed (1.5 Mbps)**:
- Simple devices (mice, keyboards)
- Limited endpoint types and sizes

**Full Speed (12 Mbps)**:
- Most common for embedded devices
- All transfer types supported
- Maximum packet sizes: Control (64), Bulk (64), Interrupt (64)

**High Speed (480 Mbps)**:
- High-performance devices
- Larger packet sizes: Control (64), Bulk (512), Interrupt (1024)
- Requires more complex hardware

**Super Speed (5 Gbps)**:
- USB 3.0 and later
- Not supported by TinyUSB

**TinyUSB Speed Support**: Most TinyUSB ports support Full Speed and High Speed. Speed is typically auto-detected by hardware. Configure speed requirements in board configuration (``hw/bsp/FAMILY/boards/BOARD/board.mk``) and ensure your MCU supports the desired speed.

USB Device Controllers
======================

USB device controllers are hardware peripherals that handle the low-level USB protocol implementation. Understanding how they work helps explain TinyUSB's architecture and portability.

Controller Fundamentals
-----------------------

**What Controllers Do**:
- Handle USB signaling and protocol timing
- Manage endpoint buffers and data transfers
- Generate interrupts for USB events
- Implement USB electrical specifications

**Key Components**:
- **Physical Layer**: USB signal drivers and receivers
- **Protocol Engine**: Handles USB packets, ACK/NAK responses
- **Endpoint Buffers**: Hardware FIFOs or RAM for data storage
- **Interrupt Controller**: Generates events for software processing

Controller Architecture Types
-----------------------------

Different MCU vendors implement USB controllers with varying architectures.
To list a few common patterns:

**FIFO-Based Controllers** (e.g., STM32 OTG, NXP LPC):
- Shared or dedicated FIFOs for endpoint data
- Software manages FIFO allocation and data flow
- Common in higher-end MCUs with flexible configurations

**Buffer-Based Controllers** (e.g., STM32 FSDEV, Microchip SAMD, RP2040):
- Fixed packet memory areas for each endpoint
- Hardware automatically handles packet placement
- Simpler programming model, common in smaller MCUs

**Descriptor-Based Controllers** (e.g., NXP EHCI-style):
- Use descriptor chains to describe transfers
- Hardware processes transfer descriptors independently
- More complex but can handle larger transfers autonomously

TinyUSB Controller Abstraction
------------------------------

TinyUSB abstracts controller differences through the TinyUSB **Device Controller Driver (DCD)** layer.
These internal details don't matter to users of TinyUSB typically; however, when debugging, knowledge about internal details helps sometimes.

**Portable Interface** (``src/device/usbd.h``):
- Standardized function signatures for all controllers
- Common endpoint and transfer management APIs
- Unified interrupt and event handling

**Controller-Specific Drivers** (``src/portable/VENDOR/FAMILY/``):
- Implement the DCD interface for specific hardware
- Handle vendor-specific register layouts and behaviors
- Manage controller-specific quirks and workarounds

**Common DCD Functions**:
- ``dcd_init()`` - Initialize controller hardware
- ``dcd_edpt_open()`` - Configure endpoint with type and size
- ``dcd_edpt_xfer()`` - Start data transfer on endpoint
- ``dcd_int_handler()`` - Process USB interrupts
- ``dcd_connect()/dcd_disconnect()`` - Control USB bus connection

Controller Event Flow
---------------------

**Typical USB Event Processing**:

1. **Hardware Event**: USB controller detects bus activity (setup packet, data transfer, etc.)
2. **Interrupt Generation**: Controller generates interrupt to CPU
3. **ISR Processing**: ``dcd_int_handler()`` reads controller status
4. **Event Queuing**: Events are queued for later processing (thread safety)
5. **Task Processing**: ``tud_task()`` processes queued events
6. **Class Notification**: Appropriate class drivers handle the event
7. **Application Callback**: User code responds to the event

Power Management
================

USB provides power to devices:

**Bus-Powered**: Device draws power from USB bus (up to 500mA)
**Self-Powered**: Device has its own power source
**Suspend/Resume**: Devices must enter low-power mode when bus is idle

**TinyUSB Power Management**:
- Implement ``tud_suspend_cb()`` and ``tud_resume_cb()`` for power management
- Configure power requirements in device descriptor (``bMaxPower`` field)
- Use ``tud_remote_wakeup()`` to wake the host from suspend (if supported)
- Enable remote wakeup with ``CFG_TUD_USBD_ENABLE_REMOTE_WAKEUP``

Next Steps
==========

- Start with :doc:`../tutorials/getting_started` for basic setup
- Review :doc:`../reference/configuration` for configuration options
- Check :doc:`../guides/integration` for advanced integration scenarios
