***************
Class Drivers
***************

USB Class Drivers implement specific USB device classes (CDC, HID, MSC, MIDI, Audio, etc.) and are the main interface between the USB core and application code.

MIDI 2.0 Device Driver
=======================

Overview
--------

The MIDI 2.0 Device driver enables TinyUSB to act as a USB MIDI 2.0 device. It implements both Alt Setting 0 (MIDI 1.0 fallback) and Alt Setting 1 (native UMP) as required by the USB-MIDI 2.0 specification.

**Key Features:**

- **Dual Alt Settings**: Alt 0 (MIDI 1.0) and Alt 1 (UMP native) per USB-MIDI 2.0 spec
- **Protocol Negotiation**: Endpoint Discovery, Config Request/Notify, Function Block Discovery
- **Group Terminal Block**: Served via GET_DESCRIPTOR automatically
- **Atomic UMP Framing**: Read/write with correct message boundaries
- **Memory Safe**: No dynamic allocation, static instances

Configuration
-------------

Enable MIDI 2.0 Device support in ``tusb_config.h``:

.. code-block:: c

   #define CFG_TUD_ENABLED       1
   #define CFG_TUD_MIDI2         1

Optional configuration:

.. code-block:: c

   #define CFG_TUD_MIDI2_TX_BUFSIZE          256
   #define CFG_TUD_MIDI2_RX_BUFSIZE          256
   #define CFG_TUD_MIDI2_TX_EPSIZE           64
   #define CFG_TUD_MIDI2_RX_EPSIZE           64
   #define CFG_TUD_MIDI2_NUM_GROUPS          1     // 1..16
   #define CFG_TUD_MIDI2_NUM_FUNCTION_BLOCKS 1     // 1..32
   #define CFG_TUD_MIDI2_EP_NAME             "TinyUSB MIDI 2.0"
   #define CFG_TUD_MIDI2_PRODUCT_ID          "TinyUSB-MIDI2"

Public API
----------

Query Functions
^^^^^^^^^^^^^^^

.. code-block:: c

   bool     tud_midi2_mounted(void);
   uint32_t tud_midi2_available(void);
   uint8_t  tud_midi2_alt_setting(void);
   bool     tud_midi2_negotiated(void);
   uint8_t  tud_midi2_protocol(void);

I/O Functions
^^^^^^^^^^^^^

.. code-block:: c

   uint32_t tud_midi2_ump_read(uint32_t* words, uint32_t max_words);
   uint32_t tud_midi2_ump_write(const uint32_t* words, uint32_t count);
   bool     tud_midi2_packet_read(uint8_t packet[4]);
   bool     tud_midi2_packet_write(const uint8_t packet[4]);

Callbacks
^^^^^^^^^

.. code-block:: c

   void tud_midi2_rx_cb(uint8_t itf);
   void tud_midi2_set_itf_cb(uint8_t itf, uint8_t alt);
   bool tud_midi2_get_req_itf_cb(uint8_t rhport, const tusb_control_request_t* request);

MIDI 2.0 Host Driver
=====================

Overview
--------

The MIDI 2.0 Host driver enables TinyUSB to enumerate and communicate with USB MIDI 2.0 devices. It implements the USB MIDI 2.0 specification, supporting both MIDI 1.0 legacy devices and modern MIDI 2.0 devices with UMP (Universal MIDI Packet) protocol.

**Key Features:**

- **Reactive Architecture**: Auto-detects Alt Setting 1 (MIDI 2.0) capability during enumeration
- **Auto-Selection**: Automatically selects the highest available protocol and issues SET_INTERFACE to activate Alt Setting 1 when MIDI 2.0 is detected
- **Transparent Stream Messages**: All data (UMP packets + Stream Messages) flow through callbacks
- **Memory Safe**: No dynamic allocation, fixed-size instances per device

Configuration
-------------

Enable MIDI 2.0 Host support in ``tusb_config.h``:

.. code-block:: c

   #define CFG_TUH_ENABLED       1
   #define CFG_TUH_MIDI2         4    // Number of MIDI 2.0 devices to support

Optional buffer configuration:

.. code-block:: c

   #define CFG_TUH_MIDI2_RX_BUFSIZE  (4 * TUH_EPSIZE_BULK_MAX)
   #define CFG_TUH_MIDI2_TX_BUFSIZE  (4 * TUH_EPSIZE_BULK_MAX)

Enumeration Lifecycle
---------------------

When a MIDI 2.0 device is connected, the host stack invokes callbacks in this order:

.. code-block:: none

   Device Connected
        |
   [Host detects Alt 0 and Alt 1 descriptors]
        |
   tuh_midi2_descriptor_cb()  <- Device detected, NOT yet ready
        |
   [Auto-select highest protocol]
        |
   tuh_midi2_mount_cb()       <- Device ready to use
        |
   [Application can read/write data]
        |
   tuh_midi2_rx_cb()          <- Data arrived
   tuh_midi2_tx_cb()          <- TX buffer space available
        |
   [Device disconnects]
        |
   tuh_midi2_umount_cb()      <- Device removed

Public API
----------

Query Functions
^^^^^^^^^^^^^^^

.. code-block:: c

   bool     tuh_midi2_mounted(uint8_t idx);
   uint8_t  tuh_midi2_get_protocol_version(uint8_t idx);    // 0=MIDI 1.0, 1=MIDI 2.0
   uint8_t  tuh_midi2_get_alt_setting_active(uint8_t idx);  // 0 or 1
   uint8_t  tuh_midi2_get_cable_count(uint8_t idx);

I/O Functions
^^^^^^^^^^^^^

Read and write UMP (Universal MIDI Packet) data:

.. code-block:: c

   uint32_t tuh_midi2_ump_read(uint8_t idx, uint32_t* words, uint32_t max_words);
   uint32_t tuh_midi2_ump_write(uint8_t idx, const uint32_t* words, uint32_t count);
   uint32_t tuh_midi2_write_flush(uint8_t idx);

Callbacks
---------

Application can define weak callback implementations to respond to device events.

Descriptor Callback
^^^^^^^^^^^^^^^^^^^

Invoked when device is detected but not yet ready for I/O:

.. code-block:: c

   void tuh_midi2_descriptor_cb(uint8_t idx, const tuh_midi2_descriptor_cb_t *desc_cb_data) {
     printf("MIDI %s device detected\r\n",
            desc_cb_data->protocol_version == 0 ? "1.0" : "2.0");
   }

Mount Callback
^^^^^^^^^^^^^^

Invoked when device is ready for I/O:

.. code-block:: c

   void tuh_midi2_mount_cb(uint8_t idx, const tuh_midi2_mount_cb_t *mount_cb_data) {
     printf("Device mounted at idx=%u, protocol=%u, alt_setting=%u\r\n",
            idx, mount_cb_data->protocol_version, mount_cb_data->alt_setting_active);
   }

RX Callback
^^^^^^^^^^^

Invoked when data arrives from device (both UMP packets and Stream Messages):

.. code-block:: c

   void tuh_midi2_rx_cb(uint8_t idx, uint32_t xferred_bytes) {
     uint32_t words[4];
     uint32_t n = tuh_midi2_ump_read(idx, words, 4);

     for (uint32_t i = 0; i < n; i++) {
       uint8_t mt = (words[i] >> 28) & 0x0F;
       if (mt == 0x0F) {
         // Stream Message - app handles discovery, negotiation, etc.
       } else {
         // Regular MIDI UMP packet
       }
     }
   }

TX Callback
^^^^^^^^^^^

Invoked when TX buffer space becomes available:

.. code-block:: c

   void tuh_midi2_tx_cb(uint8_t idx, uint32_t xferred_bytes) {
     // Buffer space available for writing
   }

Unmount Callback
^^^^^^^^^^^^^^^^

Invoked when device is disconnected:

.. code-block:: c

   void tuh_midi2_umount_cb(uint8_t idx) {
     printf("Device at idx=%u disconnected\r\n", idx);
   }

Complete Example
----------------

.. code-block:: c

   #include "tusb.h"

   void tuh_midi2_mount_cb(uint8_t idx, const tuh_midi2_mount_cb_t *mount_cb_data) {
     printf("MIDI 2.0 device mounted\r\n");
   }

   void tuh_midi2_rx_cb(uint8_t idx, uint32_t xferred_bytes) {
     uint32_t words[4];
     uint32_t n = tuh_midi2_ump_read(idx, words, 4);

     for (uint32_t i = 0; i < n; i++) {
       printf("RX: 0x%08lx\r\n", words[i]);
     }
   }

   void tuh_midi2_umount_cb(uint8_t idx) {
     printf("MIDI 2.0 device disconnected\r\n");
   }

   int main(void) {
     board_init();

     tusb_rhport_init_t host_init = {.role = TUSB_ROLE_HOST, .speed = TUSB_SPEED_AUTO};
     tusb_init(BOARD_TUH_RHPORT, &host_init);

     while (1) {
       tuh_task();
     }
   }

Architecture
------------

The MIDI 2.0 Host driver uses a **reactive, callback-driven architecture** that mirrors the proven patterns in TinyUSB's existing device drivers (CDC, HID, etc.):

- **Auto-Detection**: Host automatically detects Alt Setting 1 capability
- **Auto-Selection**: Selects highest protocol available and issues SET_INTERFACE
- **Transparent I/O**: Stream Messages and UMP packets flow through callbacks
- **Callback-Driven**: App receives events via callbacks (descriptor, mount, rx, tx, unmount)

Differences from MIDI 1.0 Host
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. list-table::
   :header-rows: 1

   * - Aspect
     - MIDI 1.0 Host
     - MIDI 2.0 Host
   * - Alt Settings
     - Parses only Alt 0
     - Parses Alt 0 + Alt 1
   * - Data Format
     - 4-byte MIDI packets
     - UMP (32/64/128-bit)
   * - Version Detection
     - None
     - bcdMSC from descriptor
   * - GTB
     - N/A
     - Presence detection
   * - Stream Messages
     - N/A
     - Transparent passthrough
   * - Callbacks
     - descriptor_cb, mount_cb, rx_cb, umount_cb
     - descriptor_cb, mount_cb, rx_cb, tx_cb, umount_cb
   * - Public API
     - tuh_midi_*
     - tuh_midi2_*

Implementation Notes
--------------------

- All internal state is statically allocated (no dynamic allocation)
- Endpoint streams use TinyUSB's tu_edpt_stream_t for buffered I/O
- Protocol version detection via bcdMSC field
- Alt Setting is automatically selected during mount
- Compatible with all TinyUSB-supported MCU families
