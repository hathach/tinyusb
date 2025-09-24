***********
USB Classes
***********

TinyUSB supports multiple USB device and host classes. This reference describes the features, capabilities, and requirements for each class.

Device Classes
==============

CDC (Communication Device Class)
--------------------------------

Implements USB CDC specification for serial communication.

**Supported Features:**
- CDC-ACM (Abstract Control Model) for virtual serial ports
- Data terminal ready (DTR) and request to send (RTS) control lines
- Line coding configuration (baud rate, parity, stop bits)
- Break signal support

**Configuration:**
- ``CFG_TUD_CDC``: Number of CDC interfaces (1-4)
- ``CFG_TUD_CDC_EP_BUFSIZE``: Endpoint buffer size (typically 512)
- ``CFG_TUD_CDC_RX_BUFSIZE``: Receive FIFO size
- ``CFG_TUD_CDC_TX_BUFSIZE``: Transmit FIFO size

**Key Functions:**
- ``tud_cdc_available()``: Check bytes available to read
- ``tud_cdc_read()``: Read data from host
- ``tud_cdc_write()``: Write data to host
- ``tud_cdc_write_flush()``: Flush transmit buffer

**Callbacks:**
- ``tud_cdc_line_coding_cb()``: Line coding changed
- ``tud_cdc_line_state_cb()``: DTR/RTS state changed

HID (Human Interface Device)
----------------------------

Implements USB HID specification for input devices.

**Supported Features:**
- Boot protocol (keyboard/mouse)
- Report protocol with custom descriptors
- Input, output, and feature reports
- Multiple HID interfaces

**Configuration:**
- ``CFG_TUD_HID``: Number of HID interfaces
- ``CFG_TUD_HID_EP_BUFSIZE``: Endpoint buffer size

**Key Functions:**
- ``tud_hid_ready()``: Check if ready to send report
- ``tud_hid_report()``: Send HID report
- ``tud_hid_keyboard_report()``: Send keyboard report
- ``tud_hid_mouse_report()``: Send mouse report

**Callbacks:**
- ``tud_hid_descriptor_report_cb()``: Provide report descriptor
- ``tud_hid_get_report_cb()``: Handle get report request
- ``tud_hid_set_report_cb()``: Handle set report request

MSC (Mass Storage Class)
------------------------

Implements USB mass storage for file systems.

**Supported Features:**
- SCSI transparent command set
- Multiple logical units (LUNs)
- Read/write operations
- Inquiry and capacity commands

**Configuration:**
- ``CFG_TUD_MSC``: Number of MSC interfaces
- ``CFG_TUD_MSC_EP_BUFSIZE``: Endpoint buffer size

**Key Functions:**
- Storage operations handled via callbacks

**Required Callbacks:**
- ``tud_msc_inquiry_cb()``: Device inquiry information
- ``tud_msc_test_unit_ready_cb()``: Test if LUN is ready
- ``tud_msc_capacity_cb()``: Get LUN capacity
- ``tud_msc_start_stop_cb()``: Start/stop LUN
- ``tud_msc_read10_cb()``: Read data from LUN
- ``tud_msc_write10_cb()``: Write data to LUN

Audio Class
-----------

Implements USB Audio Class 2.0 specification.

**Supported Features:**
- Audio streaming (input/output)
- Multiple sampling rates
- Volume and mute controls
- Feedback endpoints for asynchronous mode

**Configuration:**
- ``CFG_TUD_AUDIO``: Number of audio functions
- Multiple configuration options for channels, sample rates, bit depth

**Key Functions:**
- ``tud_audio_read()``: Read audio data
- ``tud_audio_write()``: Write audio data
- ``tud_audio_clear_ep_out_ff()``: Clear output FIFO

MIDI
----

Implements USB MIDI specification.

**Supported Features:**
- MIDI 1.0 message format
- Multiple virtual MIDI cables
- Standard MIDI messages

**Configuration:**
- ``CFG_TUD_MIDI``: Number of MIDI interfaces
- ``CFG_TUD_MIDI_RX_BUFSIZE``: Receive buffer size
- ``CFG_TUD_MIDI_TX_BUFSIZE``: Transmit buffer size

**Key Functions:**
- ``tud_midi_available()``: Check available MIDI messages
- ``tud_midi_read()``: Read MIDI packet
- ``tud_midi_write()``: Send MIDI packet

DFU (Device Firmware Update)
----------------------------

Implements USB DFU specification for firmware updates.

**Supported Modes:**
- DFU Mode: Device enters DFU for firmware update
- DFU Runtime: Request transition to DFU mode

**Configuration:**
- ``CFG_TUD_DFU``: Enable DFU mode
- ``CFG_TUD_DFU_RUNTIME``: Enable DFU runtime

**Key Functions:**
- Firmware update operations handled via callbacks

**Required Callbacks:**
- ``tud_dfu_download_cb()``: Receive firmware data
- ``tud_dfu_manifest_cb()``: Complete firmware update

Vendor Class
------------

Custom vendor-specific USB class implementation.

**Features:**
- Configurable endpoints
- Custom protocol implementation
- WebUSB support
- Microsoft OS descriptors

**Configuration:**
- ``CFG_TUD_VENDOR``: Number of vendor interfaces
- ``CFG_TUD_VENDOR_EPSIZE``: Endpoint size

**Key Functions:**
- ``tud_vendor_available()``: Check available data
- ``tud_vendor_read()``: Read vendor data
- ``tud_vendor_write()``: Write vendor data

Host Classes
============

CDC Host
--------

Connect to CDC devices (virtual serial ports).

**Supported Devices:**
- CDC-ACM devices
- FTDI USB-to-serial converters
- CP210x USB-to-serial converters
- CH34x USB-to-serial converters

**Configuration:**
- ``CFG_TUH_CDC``: Number of CDC host instances
- ``CFG_TUH_CDC_FTDI``: Enable FTDI support
- ``CFG_TUH_CDC_CP210X``: Enable CP210x support

**Key Functions:**
- ``tuh_cdc_available()``: Check available data
- ``tuh_cdc_read()``: Read from CDC device
- ``tuh_cdc_write()``: Write to CDC device
- ``tuh_cdc_set_baudrate()``: Configure serial settings

HID Host
--------

Connect to HID devices (keyboards, mice, etc.).

**Supported Devices:**
- Boot keyboards and mice
- Generic HID devices with report descriptors
- Composite HID devices

**Configuration:**
- ``CFG_TUH_HID``: Number of HID host instances
- ``CFG_TUH_HID_EPIN_BUFSIZE``: Input endpoint buffer size

**Key Functions:**
- ``tuh_hid_receive_report()``: Start receiving reports
- ``tuh_hid_send_report()``: Send report to device
- ``tuh_hid_parse_report_descriptor()``: Parse HID descriptors

MSC Host
--------

Connect to mass storage devices (USB drives).

**Supported Features:**
- SCSI transparent command set
- FAT file system support (with FatFS integration)
- Multiple LUNs per device

**Configuration:**
- ``CFG_TUH_MSC``: Number of MSC host instances
- ``CFG_TUH_MSC_MAXLUN``: Maximum LUNs per device

**Key Functions:**
- ``tuh_msc_ready()``: Check if device is ready
- ``tuh_msc_read10()``: Read sectors from device
- ``tuh_msc_write10()``: Write sectors to device

Hub
---

Support for USB hubs to connect multiple devices.

**Features:**
- Multi-level hub support
- Port power management
- Device connect/disconnect detection

**Configuration:**
- ``CFG_TUH_HUB``: Number of hub instances
- ``CFG_TUH_DEVICE_MAX``: Total connected devices

Class Implementation Guidelines
===============================

Descriptor Requirements
-----------------------

Each USB class requires specific descriptors:

1. **Interface Descriptor**: Defines the class type
2. **Endpoint Descriptors**: Define communication endpoints
3. **Class-Specific Descriptors**: Additional class requirements
4. **String Descriptors**: Human-readable device information

Callback Implementation
-----------------------

Most classes require callback functions:

- **Mandatory callbacks**: Must be implemented for class to function
- **Optional callbacks**: Provide additional functionality
- **Event callbacks**: Called when specific events occur

Performance Considerations
--------------------------

When implementing USB classes, match **buffer sizes** to expected data rates to avoid bottlenecks. Choose appropriate **transfer types** based on your application's requirements. Keep **callback processing** lightweight for optimal performance. Avoid **memory allocations in critical paths** where possible to maintain consistent performance.

Testing and Validation
----------------------

- **USB-IF Compliance**: Ensure descriptors meet USB standards
- **Host Compatibility**: Test with multiple operating systems
- **Performance Testing**: Verify transfer rates and latency
- **Error Handling**: Test disconnect/reconnect scenarios

Class-Specific Resources
========================

- **USB-IF Specifications**: Official USB class specifications
- **Example Code**: Reference implementations in ``examples/`` directory
- **Test Applications**: Host-side test applications for validation
- **Debugging Tools**: USB protocol analyzers and debugging utilities