****
MIDI
****

Roles: device and host.  TinyUSB has separate drivers for USB-MIDI 1.0 event
packets and USB-MIDI 2.0 Universal MIDI Packets (UMP).  Enable the driver that
matches the data model used by the application.

MIDI 1.0 device
===============

Enable ``CFG_TUD_MIDI``, define ``CFG_TUD_MIDI_RX_BUFSIZE`` and
``CFG_TUD_MIDI_TX_BUFSIZE``, and add ``TUD_MIDI_DESCRIPTOR``.

.. list-table::
   :header-rows: 1
   :widths: 38 18 44

   * - Option
     - Default
     - What it controls
   * - ``CFG_TUD_MIDI_RX_BUFSIZE`` / ``CFG_TUD_MIDI_TX_BUFSIZE``
     - Required when enabled
     - Software FIFO bytes per interface.  Define both, normally at least as
       large as the matching endpoint buffer.
   * - ``CFG_TUD_MIDI_RX_EPSIZE`` / ``CFG_TUD_MIDI_TX_EPSIZE``
     - Device bulk maximum
     - Endpoint transfer buffer bytes.  Keep them at least the descriptor
       packet size, the ``_epsize`` argument of ``TUD_MIDI_DESCRIPTOR``.

Use ``tud_midi_stream_read()``/``tud_midi_stream_write()`` for MIDI byte
streams on the first interface (the write takes a cable number).  Use
``tud_midi_n_*`` to select an
interface, ``tud_midi_n_demux_stream_read()`` to separate received cables, and
the ``*_packet_*`` APIs when the application already works with 4-byte
USB-MIDI event packets.  Drain received data in ``tud_midi_rx_cb()``.

.. list-table::
   :header-rows: 1
   :widths: 42 58

   * - API or callback
     - What it does
   * - ``tud_midi_n_available()`` /
       ``tud_midi_n_stream_read()``
     - Report raw FIFO bytes (4-byte event packets) and read MIDI bytes for
       one interface.  Both ignore ``cable_num``: the read merges every cable.
   * - ``tud_midi_n_demux_stream_read()``
     - Reads bytes from one cable at a time, returns the byte count and writes
       the cable number to ``*p_cable_num``.  Do not mix it with the legacy
       stream reader on the same interface.
   * - ``tud_midi_n_stream_write()``
     - Packetizes a MIDI byte stream and returns the number of source bytes
       accepted.
   * - ``tud_midi_n_packet_read_n()`` /
       ``tud_midi_n_packet_write_n()``
     - Reads or writes complete 4-byte USB-MIDI event packets and returns a
       packet count.
   * - ``tud_midi_rx_cb()``
     - Announces received data.  Drain the FIFO so later OUT transfers have
       room.

See :doc:`../../examples/device/midi_test`.

MIDI 1.0 host
=============

Set ``CFG_TUH_MIDI`` to the number of simultaneous MIDI streaming interfaces.
The following options size each instance:

.. list-table::
   :header-rows: 1
   :widths: 38 18 44

   * - Option
     - Default
     - What it controls
   * - ``CFG_TUH_MIDI_RX_BUFSIZE`` / ``CFG_TUH_MIDI_TX_BUFSIZE``
     - Host bulk maximum
     - Software FIFO capacity for received and queued event packets.
   * - ``CFG_TUH_MIDI_EP_BUFSIZE``
     - Host bulk maximum
     - Endpoint transfer buffer size.
   * - ``CFG_TUH_MIDI_STREAM_API``
     - ``1``
     - Enables byte-stream packetization/depacketization.  Disable it to save
       code size when the application uses only raw 4-byte event packets.

``tuh_midi_descriptor_cb()`` reports descriptor information before the
interface is ready; begin I/O in ``tuh_midi_mount_cb()``.  Receive data in
``tuh_midi_rx_cb()`` with ``tuh_midi_stream_read()`` or
``tuh_midi_packet_read_n()``.  ``tuh_midi_stream_read()`` stops at a cable
change, so call it in a loop until it returns zero; bytes left in the FIFO can
block later bulk IN transfers.  Writes remain buffered until an endpoint packet
is ready or ``tuh_midi_write_flush()`` is called.

The RX and TX cable counts can differ.  Query them with
``tuh_midi_get_rx_cable_count()`` and ``tuh_midi_get_tx_cable_count()`` before
selecting a cable.  See :doc:`../../examples/host/midi_rx`.

``tuh_midi_read_available()`` reports raw FIFO bytes, while
``tuh_midi_stream_read()`` returns decoded MIDI stream bytes and a cable
number.  ``tuh_midi_packet_read_n()`` keeps the USB event-packet format.
``tuh_midi_write_flush()`` starts a short buffered transfer and returns the
number of bytes submitted.  Mount/unmount callbacks define the lifetime of the
``idx``; RX/TX callbacks announce new input and newly available TX space.

MIDI 2.0 device
===============

Enable ``CFG_TUD_MIDI2`` and add ``TUD_MIDI2_DESCRIPTOR``.  The descriptor
contains alternate setting 0 for USB-MIDI 1.0 fallback and alternate setting 1
for UMP, as required by the MIDI 2.0 class specification.

.. code-block:: c

   #define CFG_TUD_MIDI2            1
   #define CFG_TUD_MIDI2_RX_BUFSIZE 256
   #define CFG_TUD_MIDI2_TX_BUFSIZE 256

.. list-table::
   :header-rows: 1
   :widths: 38 18 44

   * - Option
     - Default
     - What it controls
   * - ``CFG_TUD_MIDI2_RX_EPSIZE`` / ``CFG_TUD_MIDI2_TX_EPSIZE``
     - Device bulk maximum
     - Endpoint transfer buffer bytes.  Keep them at least the descriptor
       packet size, the ``_epsize`` argument of ``TUD_MIDI2_DESCRIPTOR``.
   * - ``CFG_TUD_MIDI2_RX_BUFSIZE`` / ``CFG_TUD_MIDI2_TX_BUFSIZE``
     - Matching endpoint size
     - UMP FIFO bytes per interface.
   * - ``CFG_TUD_MIDI2_NUM_GROUPS``
     - ``1``
     - Number of UMP groups exposed by the default Group Terminal Block.
   * - ``CFG_TUD_MIDI2_EP_NAME`` / ``CFG_TUD_MIDI2_PRODUCT_ID``
     - TinyUSB strings
     - Default endpoint name and product identifier returned by UMP discovery.
       Override ``tud_midi2_ep_name_cb()`` / ``tud_midi2_product_id_cb()`` for
       per-interface values.
   * - ``CFG_TUD_MIDI2_FB_CI_VERSION`` / ``CFG_TUD_MIDI2_FB_SYSEX8_STREAMS``
     - ``0``
     - MIDI-CI version and SysEx8 stream count reported in Function Block Info.
   * - ``CFG_TUD_MIDI2_BLOCK_STRIDX``
     - ``0``
     - Optional string-descriptor index (``iBlockItem``) for the default Group
       Terminal Block; zero means no string.
   * - ``CFG_TUD_MIDI2_GTB_PROTOCOL``
     - ``MIDI2_GTB_PROTOCOL_MIDI2``
     - Default ``bMIDIProtocol`` for the built-in Group Terminal Block and
       ``TUD_MIDI2_GTB_BLOCK`` entries; ``TUD_MIDI2_GTB_BLOCK_PROTOCOL``
       entries set their own.

On alternate setting 1, read and write arrays of 32-bit words with
``tud_midi2_ump_read()`` and ``tud_midi2_ump_write()``.  On alternate setting
0, use ``tud_midi2_packet_read()`` and ``tud_midi2_packet_write()`` for 4-byte
USB-MIDI 1.0 event packets.  Query ``tud_midi2_alt_setting()``,
``tud_midi2_negotiated()`` and ``tud_midi2_protocol()`` when choosing the
format to send; ``tud_midi2_mounted()`` reports whether the endpoints are open.
``tud_midi2_set_itf_cb()`` announces the active alternate setting so the
application can switch its parser and producer.

``tud_midi2_ump_read()`` returns only whole UMP messages: it stops before a
message that does not fit in ``max_words`` or has not fully arrived, and
returns zero on alternate setting 0.  Pass at least 4 words, the largest UMP
message, or a long message at the head of the FIFO is never read.  Drain the
RX FIFO completely in the callback:

.. code-block:: c

   void tud_midi2_rx_cb(uint8_t itf) {
     uint32_t count;
     if (tud_midi2_n_alt_setting(itf) == 1) {
       uint32_t words[16];
       while ((count = tud_midi2_n_ump_read(itf, words, 16)) != 0) {
         process_ump(words, count); // Track message size from each MT field.
       }
     } else {
       uint8_t packets[16 * 4];
       while ((count = tud_midi2_n_packet_read(itf, packets, 16)) != 0) {
         process_midi1(packets, count);
       }
     }
   }

The driver handles standard UMP Stream discovery and protocol negotiation.
At interface open and on selecting alternate setting 1, the protocol is
initialized from the first Group Terminal Block: MIDI 1.0 for its MIDI1 values,
otherwise MIDI 2.0.  Stream negotiation may change it later.
Override ``tud_midi2_gtb_desc_cb()`` to describe a custom Group Terminal Block
topology, ``tud_midi2_fb_name_cb()`` for Function Block names and
``tud_midi2_device_identity_cb()`` to send a Device Identity Notification.  Use
``tud_midi2_stream_msg_cb()`` only when the application must override a built-in
Stream response.

``tud_midi2_n_available()``, ``tud_midi2_n_ump_read()`` and
``tud_midi2_n_ump_write()`` return 32-bit word counts.  The packet APIs return
counts of 4-byte MIDI 1.0 event packets.

See :doc:`../../examples/device/midi2_device` for alternate-setting and
protocol fallback.

MIDI 2.0 host
=============

Set ``CFG_TUH_MIDI2`` to the required interface count.  The host driver detects
both alternate settings, selects the highest available protocol, and completes
``SET_INTERFACE`` before reporting the mount.

``CFG_TUH_MIDI2_RX_BUFSIZE`` and ``CFG_TUH_MIDI2_TX_BUFSIZE`` default to the
host bulk maximum and allocate FIFO storage per instance.  Increase them when
the application can be delayed for several USB transfers; keep UMP data
32-bit aligned in application buffers.

Callback order is important:

.. code-block:: text

   protocol/alternate selection -> descriptor callback -> mount callback
   -> RX/TX callbacks -> unmount callback

``tuh_midi2_descriptor_cb()`` is informational; the interface is not yet ready.
Start I/O only after ``tuh_midi2_mount_cb()``.  In ``tuh_midi2_rx_cb()``, call
``tuh_midi2_ump_read()`` in a loop until it returns zero.  Queue output with
``tuh_midi2_ump_write()`` and use ``tuh_midi2_write_flush()`` when latency
matters.

Use ``tuh_midi2_mounted()``, ``tuh_midi2_get_protocol_version()``,
``tuh_midi2_get_alt_setting_active()`` and ``tuh_midi2_get_cable_count()`` to
inspect the selected transport.  The :doc:`../../examples/host/midi2_host`
example parses the UMP Message Type field to determine whether each message
occupies 1, 2, 3, or 4 words.

``tuh_midi2_ump_write()`` returns the number of words accepted and
``tuh_midi2_write_flush()`` returns the number of bytes submitted.  Preserve
unaccepted words and retry after ``tuh_midi2_tx_cb()``.  Treat the data supplied
to ``tuh_midi2_descriptor_cb()`` as informational only; the interface becomes
usable at ``tuh_midi2_mount_cb()`` and invalid at ``tuh_midi2_umount_cb()``.

Specification used: *USB Device Class Definition for MIDI Devices*, Release
2.0.  It defines the MIDI 1.0-compatible alternate setting, the native UMP
alternate setting, Group Terminal Blocks, and discovery behavior.
