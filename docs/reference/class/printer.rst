*******
Printer
*******

Role: device only.  The Printer class provides bulk OUT data from a host print
spooler, optional bulk IN data for a bidirectional printer, and standard device
ID, port-status, and soft-reset requests.

Configuration
=============

Enable ``CFG_TUD_PRINTER`` and define the RX/TX FIFO and endpoint sizes.  Add a
``TUD_PRINTER_DESCRIPTOR``; protocol 2 is the bidirectional interface used by
the TinyUSB example.

.. code-block:: c

   #define CFG_TUD_PRINTER            1
   #define CFG_TUD_PRINTER_RX_BUFSIZE 64
   #define CFG_TUD_PRINTER_TX_BUFSIZE 64
   #define CFG_TUD_PRINTER_RX_EPSIZE  64
   #define CFG_TUD_PRINTER_TX_EPSIZE  64

.. list-table::
   :header-rows: 1
   :widths: 42 17 41

   * - Option
     - Default
     - What it controls
   * - ``CFG_TUD_PRINTER_RX_BUFSIZE`` /
       ``CFG_TUD_PRINTER_TX_BUFSIZE``
     - Required when enabled
     - Software FIFO capacity per printer interface.  TX is still allocated
       even if the application normally only receives print data.
   * - ``CFG_TUD_PRINTER_RX_EPSIZE`` /
       ``CFG_TUD_PRINTER_TX_EPSIZE``
     - Device bulk maximum
     - Endpoint transfer buffer and descriptor packet size in each direction.

Data received from the host is available through
``tud_printer_read_available()`` and ``tud_printer_read()``.  Send status or
bidirectional data with ``tud_printer_write()`` and flush when the response
should leave promptly.  Use the ``tud_printer_n_*`` forms for multiple
interfaces.

.. list-table::
   :header-rows: 1
   :widths: 42 58

   * - Data API or callback
     - What it does
   * - ``tud_printer_n_read_available()`` /
       ``tud_printer_n_read()``
     - Reports and removes bytes sent by the print spooler.
   * - ``tud_printer_n_write_available()`` /
       ``tud_printer_n_write()``
     - Reports TX room and copies as much bidirectional response data as fits.
   * - ``tud_printer_n_write_flush()``
     - Starts a short TX transfer and returns the number of bytes submitted.
   * - ``tud_printer_n_read_flush()`` /
       ``tud_printer_n_write_clear()``
     - Discards pending receive or transmit FIFO data, for example on soft
       reset.
   * - ``tud_printer_rx_cb()`` /
       ``tud_printer_tx_complete_cb()``
     - Announces new input or newly available TX capacity.

Control callbacks
=================

``tud_printer_get_device_id_cb()`` must return an IEEE 1284 device ID.  Its
first two bytes are the big-endian total length, including those two bytes, and
the buffer must remain valid through transfer completion.  A typical text body
is ``MFG:Vendor;MDL:Model;CMD:PCL;CLS:PRINTER;``.

Return current online, error, and paper state from
``tud_printer_get_port_status_cb()`` using
``tusb_printer_port_status_t``.  In ``tud_printer_soft_reset_cb()``, cancel the
current print job and reset the class-facing parser without resetting unrelated
parts of a composite device.

``tud_printer_request_complete_cb()`` marks the end of the control transfer and
is the safe point to reuse request-specific application state, including a
dynamically selected device-ID buffer.

See :doc:`../../examples/device/printer_to_cdc` for bidirectional data and all
three class requests.

Specification used: *USB Device Class Definition for Printing Devices*,
Version 1.1.
