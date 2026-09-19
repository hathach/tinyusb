**********
CDC Serial
**********

Roles: device and host.  The standard driver implements CDC ACM virtual serial
ports.  The host driver can also expose FTDI, CP210x, CH34x, and PL2303 USB
serial adapters through the same ``tuh_cdc_*`` API.

Device
======

Enable ``CFG_TUD_CDC`` with the required port count and add one
``TUD_CDC_DESCRIPTOR`` per port.

.. list-table::
   :header-rows: 1
   :widths: 36 18 46

   * - Option
     - Default
     - What it controls
   * - ``CFG_TUD_CDC_RX_BUFSIZE`` / ``CFG_TUD_CDC_TX_BUFSIZE``
     - Device bulk maximum
     - Software FIFO capacity in each direction.  Larger FIFOs absorb longer
       application scheduling gaps.
   * - ``CFG_TUD_CDC_RX_EPSIZE`` / ``CFG_TUD_CDC_TX_EPSIZE``
     - Device bulk maximum
     - Endpoint transfer buffer and descriptor packet size.  Use
       speed-appropriate values.
   * - ``CFG_TUD_CDC_NOTIFY``
     - ``0``
     - Enables the interrupt notification endpoint and serial-state API.
   * - ``CFG_TUD_CDC_RX_PERSISTENT`` / ``CFG_TUD_CDC_TX_PERSISTENT``
     - ``0``
     - Keeps the corresponding FIFO contents across disconnect/reconnect.
       Enable only when stale bytes are intentional.
   * - ``CFG_TUD_CDC_RX_NEED_ZLP``
     - ``0``
     - Enables multi-packet receive transfers terminated by a host-sent
       zero-length packet.  Enable only when the host side supports this
       framing.
   * - ``CFG_TUD_CDC_TX_OVERWRITABLE_IF_NOT_CONNECTED``
     - ``1``
     - Allows writes made before DTR connection to replace old queued data
       rather than permanently filling the FIFO.

Use speed-dependent values from an example when the device can enumerate at
high speed.  ``CFG_TUD_CDC_NOTIFY`` enables serial-state notifications.

The common data path is:

.. code-block:: c

   void tud_cdc_rx_cb(uint8_t itf) {
     uint8_t buf[64];
     uint32_t count = tud_cdc_n_available(itf);
     uint32_t room = tud_cdc_n_write_available(itf);
     if (count > sizeof(buf)) count = sizeof(buf);
     if (count > room) count = room;

     count = tud_cdc_n_read(itf, buf, count);
     if (count && tud_cdc_n_write(itf, buf, count) == count) {
       tud_cdc_n_write_flush(itf);
     }
   }

Use ``tud_cdc_n_connected()`` when transmission should depend on DTR.  A port
can be mounted while a terminal has not opened it.  Handle line settings in
``tud_cdc_line_state_cb()`` and ``tud_cdc_line_coding_cb()`` if they affect the
physical UART; TinyUSB does not configure that UART for you.

.. list-table::
   :header-rows: 1
   :widths: 39 61

   * - Device API or callback
     - What it does
   * - ``tud_cdc_n_connected()`` / ``tud_cdc_n_ready()``
     - Tests DTR connection, or whether the port is connected and can accept
       output now.
   * - ``tud_cdc_n_available()`` / ``tud_cdc_n_read()``
     - Reports and removes bytes received from the host.
   * - ``tud_cdc_n_write_available()`` / ``tud_cdc_n_write()``
     - Reports TX FIFO room and copies as many bytes as fit; preserve any
       unwritten remainder.
   * - ``tud_cdc_n_write_flush()``
     - Starts transmission of buffered bytes without waiting for the FIFO to
       fill.
   * - ``tud_cdc_rx_cb()`` / ``tud_cdc_tx_complete_cb()``
     - Announces newly buffered receive data or completion of a transmit
       transfer.
   * - ``tud_cdc_line_state_cb()`` / ``tud_cdc_line_coding_cb()``
     - Reports host DTR/RTS and baud/data/parity/stop settings so a UART bridge
       can apply them.

See :doc:`../../examples/device/cdc_dual_ports` for multiple ports and
:doc:`../../examples/device/cdc_msc` for a composite device.

Host
====

Set ``CFG_TUH_CDC`` to the required number of serial interfaces.  Enable only
the adapter families needed by the product:

.. code-block:: c

   #define CFG_TUH_CDC        1
   #define CFG_TUH_CDC_FTDI   1
   #define CFG_TUH_CDC_CP210X 1
   #define CFG_TUH_CDC_CH34X  1
   #define CFG_TUH_CDC_PL2303 1

The RX/TX software FIFO and endpoint buffers default to
``TUH_EPSIZE_BULK_MAX`` through ``CFG_TUH_CDC_RX_BUFSIZE``,
``CFG_TUH_CDC_TX_BUFSIZE``, ``CFG_TUH_CDC_RX_EPSIZE``, and
``CFG_TUH_CDC_TX_EPSIZE``.  Increase FIFO sizes to tolerate application
latency; endpoint sizes normally stay at the host bulk maximum.  Optional
``CFG_TUH_CDC_LINE_CODING_ON_ENUM`` and
``CFG_TUH_CDC_LINE_CONTROL_ON_ENUM`` values apply initial serial settings as
part of enumeration.

``tuh_cdc_mount_cb(idx)`` reports a ready interface.  Read data in
``tuh_cdc_rx_cb(idx)`` using ``tuh_cdc_read_available()`` and
``tuh_cdc_read()``.  Queue output with ``tuh_cdc_write()`` and call
``tuh_cdc_write_flush()`` when it should leave promptly.

Line-control functions such as ``tuh_cdc_set_baudrate()`` and
``tuh_cdc_set_line_coding()`` accept a completion callback.  Their ``_sync``
forms block and should only be used where the host task can continue running.
Support varies by adapter family; in particular, the combined line-coding call
is not implemented for every non-CDC adapter.

.. list-table::
   :header-rows: 1
   :widths: 39 61

   * - Host API or callback
     - What it does
   * - ``tuh_cdc_mounted()`` / ``tuh_cdc_itf_get_info()``
     - Tests an interface index and returns its address, interface descriptor,
       and serial-driver type.
   * - ``tuh_cdc_read_available()`` / ``tuh_cdc_read()``
     - Reports and removes bytes buffered from the serial device.
   * - ``tuh_cdc_write()`` / ``tuh_cdc_write_flush()``
     - Copies output into the class FIFO and starts a USB transfer.
   * - ``tuh_cdc_set_control_line_state()`` /
       ``tuh_cdc_set_line_coding()``
     - Queues DTR/RTS or baud/framing changes and calls the supplied completion
       callback.
   * - ``tuh_cdc_mount_cb()`` / ``tuh_cdc_umount_cb()``
     - Creates or removes application state for a serial interface index.
   * - ``tuh_cdc_rx_cb()`` / ``tuh_cdc_tx_complete_cb()``
     - Announces buffered input or completion of queued class output.

The :doc:`../../examples/host/cdc_msc_hid` example shows enumeration, 115200
8N1 setup, and bidirectional I/O.

Practical notes
===============

* USB CDC transfers bytes, not UART timing.  Baud rate and framing are host
  requests that an application may honor, translate, or ignore.
* A write call can accept fewer bytes than requested.  Preserve and retry the
  remainder instead of silently dropping it.
* For interactive traffic, flush after a logical message.  For throughput,
  allow the FIFO to fill and flush less often.

Specifications used: *Communications Devices Class*, Revision 1.2 (Errata 1),
and *CDC PSTN Subclass*, Revision 1.2, which defines ACM.
