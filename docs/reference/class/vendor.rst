***************
Vendor-specific
***************

The device driver provides bulk, and optionally interrupt or isochronous,
transfers for a vendor-defined interface.  There is no generic vendor protocol:
the device descriptors, request semantics, framing, and host software are part
of the product's protocol.

Device
======

Enable ``CFG_TUD_VENDOR`` and add ``TUD_VENDOR_DESCRIPTOR`` for the usual pair
of bulk endpoints.  Buffered mode is the practical default.

.. list-table::
   :header-rows: 1
   :widths: 43 17 40

   * - Option
     - Default
     - What it controls
   * - ``CFG_TUD_VENDOR_RX_BUFSIZE`` /
       ``CFG_TUD_VENDOR_TX_BUFSIZE``
     - Device bulk maximum
     - Software FIFO bytes.  Setting either to zero selects direct mode for
       both directions.
   * - ``CFG_TUD_VENDOR_RX_EPSIZE`` /
       ``CFG_TUD_VENDOR_TX_EPSIZE``
     - Device bulk maximum
     - Bulk endpoint transfer buffers and descriptor packet sizes.
   * - ``CFG_TUD_VENDOR_RX_MANUAL_XFER``
     - ``0``
     - Requires the application to call ``tud_vendor_n_read_xfer()`` to arm
       each buffered bulk OUT transfer.
   * - ``CFG_TUD_VENDOR_RX_NEED_ZLP``
     - ``0``
     - Allows multi-packet receive termination by a zero-length packet; enable
       only when the custom host protocol sends that terminator.
   * - ``CFG_TUD_VENDOR_EP_INT_OUT`` /
       ``CFG_TUD_VENDOR_EP_INT_IN``
     - ``0``
     - Enables optional direct interrupt endpoints.  Their buffer-size options
       default to 64 bytes.
   * - ``CFG_TUD_VENDOR_EP_ISO_OUT`` /
       ``CFG_TUD_VENDOR_EP_ISO_IN``
     - ``0``
     - Enables optional direct isochronous endpoints.  They require alternate
       settings; their buffers default to 64 bytes.
   * - ``CFG_TUD_VENDOR_ALT_SETTINGS``
     - ``0``
     - Enables alternate-setting tracking.  It requires direct mode and is
       required by the optional isochronous endpoints.

Use ``tud_vendor_available()``/``tud_vendor_read()`` for OUT data and
``tud_vendor_write()``/``tud_vendor_write_flush()`` for IN data.  In buffered
mode, ``tud_vendor_rx_cb()`` is only a notification; read the FIFO rather than
using its null buffer argument.

For direct transfers, configure zero RX/TX FIFO sizes and follow the ownership
rules in ``vendor_device.h``.  Optional interrupt, isochronous, and alternate
setting support is controlled by ``CFG_TUD_VENDOR_EP_*`` and
``CFG_TUD_VENDOR_ALT_SETTINGS``.  Interrupt and isochronous OUT endpoints must
be explicitly re-armed after their receive callbacks.

.. list-table::
   :header-rows: 1
   :widths: 43 57

   * - API or callback
     - What it does
   * - ``tud_vendor_n_mounted()``
     - Tests whether an instance has any configured bulk, interrupt, or
       isochronous endpoint open.
   * - ``tud_vendor_n_available()`` / ``tud_vendor_n_read()``
     - Reports and removes bulk OUT FIFO bytes in buffered mode.
   * - ``tud_vendor_n_write_available()`` /
       ``tud_vendor_n_write()``
     - Reports room and copies as many bulk IN bytes as fit.  In direct mode the
       copy is limited to one endpoint buffer.
   * - ``tud_vendor_n_write_flush()``
     - Starts a short buffered IN transfer and returns the number of bytes
       submitted.
   * - ``tud_vendor_rx_cb()`` / ``tud_vendor_tx_cb()``
     - Announces received data or completed output.  In direct mode, consume or
       copy the receive pointer before returning from the callback.
   * - ``tud_vendor_n_int_read_xfer()`` /
       ``tud_vendor_n_iso_read_xfer()``
     - Arms one optional OUT transfer; re-arm after each receive callback.
   * - ``tud_vendor_n_int_write()`` /
       ``tud_vendor_n_iso_write()``
     - Copies and queues at most one optional endpoint buffer and returns the
       accepted byte count.
   * - ``tud_vendor_n_alt()``
     - Returns the host-selected alternate setting when support is enabled.

Handle vendor control requests in ``tud_vendor_control_xfer_cb()`` and perform
the data/status stage with ``tud_control_xfer()`` or
``tud_control_status()``.  Validate ``bmRequestType``, ``bRequest``,
``wIndex``, ``wValue``, and ``wLength`` before accepting a request.

The :doc:`../../examples/device/webusb_serial` example combines a vendor bulk
interface with WebUSB and Microsoft OS 2.0 descriptors.

Host
====

A host cannot interpret an arbitrary vendor interface from its class code.
TinyUSB does not currently offer a supported, protocol-neutral
``tuh_vendor_*`` application API.  For a simple fixed device, enable
``CFG_TUH_API_EDPT_XFER`` and use the descriptor/endpoint APIs demonstrated by
:doc:`../../examples/host/bare_api`.  For a reusable protocol, implement a
custom host class driver that matches devices by descriptors and VID/PID and
owns their enumeration and transfer state.

Define framing, version negotiation, maximum message lengths, timeouts, and
error recovery before deploying a vendor protocol.  Never cast unvalidated
wire data directly to an application structure.
