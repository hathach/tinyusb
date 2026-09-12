******
USBTMC
******

Role: device only.  USB Test and Measurement Class (USBTMC) carries instrument
commands and responses.  The optional USB488 subclass adds IEEE-488-style
status, trigger, and service-request behavior; SCPI command parsing remains an
application concern.

Configuration and descriptors
=============================

Enable ``CFG_TUD_USBTMC``.  Set ``CFG_TUD_USBTMC_ENABLE_488`` when USB488 is
implemented and ``CFG_TUD_USBTMC_ENABLE_INT_EP`` when an interrupt IN endpoint
is present.  Construct the configuration from
``TUD_USBTMC_IF_DESCRIPTOR``, ``TUD_USBTMC_BULK_DESCRIPTORS``, and, when
enabled, ``TUD_USBTMC_INT_DESCRIPTOR``.

.. list-table::
   :header-rows: 1
   :widths: 42 16 42

   * - Option
     - Default
     - What it controls
   * - ``CFG_TUD_USBTMC``
     - ``0``
     - Enables the device class.  The current driver exposes one instrument
       interface.
   * - ``CFG_TUD_USBTMC_ENABLE_488``
     - ``1``
     - Builds USB488 capability, status-byte, and trigger support.  Set it to
       ``0`` for base USBTMC only.
   * - ``CFG_TUD_USBTMC_ENABLE_INT_EP``
     - Example-defined
     - Selects whether the example descriptor includes the notification
       endpoint; keep this consistent with the capabilities response.
   * - ``CFG_TUD_USBTMC_INT_EP_SIZE``
     - ``2`` bytes
     - Internal interrupt notification buffer.  It must fit the notification
       format and descriptor packet size.

Return a static capabilities structure from
``tud_usbtmc_get_capabilities_cb()``.  Its flags must agree with the descriptor
and callbacks actually implemented.

Message flow
============

.. list-table::
   :header-rows: 1
   :widths: 45 55

   * - API or callback
     - What it does
   * - ``tud_usbtmc_open_cb()``
     - Announces an opened interface; initialize instrument transaction state
       and arrange the first bus read.
   * - ``tud_usbtmc_msgBulkOut_start_cb()`` /
       ``tud_usbtmc_msg_data_cb()``
     - Accepts a device-dependent OUT header and subsequent payload chunks.
       Return ``false`` when the message cannot be accepted.
   * - ``tud_usbtmc_msgBulkIn_request_cb()``
     - Receives a host request for instrument data and should queue the response
       when ready.
   * - ``tud_usbtmc_transmit_dev_msg_data()``
     - Queues response data with EOM/termination flags.  The source remains
       application-owned and must stay unchanged until completion.
   * - ``tud_usbtmc_msgBulkIn_complete_cb()``
     - Releases the response buffer and lets the application queue more data or
       restart command reception.
   * - ``tud_usbtmc_transmit_notification_data()``
     - Copies one interrupt notification when that endpoint is present;
       ``false`` means the previous notification is still pending.
   * - ``tud_usbtmc_start_bus_read()``
     - Arms the next bulk OUT transfer.  Call it after every path that becomes
       ready to receive another command.
   * - ``tud_usbtmc_initiate_*_cb()`` / ``tud_usbtmc_check_*_cb()``
     - Starts and reports progress for abort/clear control requests.
   * - ``tud_usbtmc_get_stb_cb()`` /
       ``tud_usbtmc_msg_trigger_cb()``
     - Supplies the USB488 status byte and handles a USB488 trigger message.

For host-to-instrument messages, TinyUSB calls
``tud_usbtmc_msgBulkOut_start_cb()`` followed by one or more
``tud_usbtmc_msg_data_cb()`` calls.  ``transfer_complete`` marks a USB transfer,
not necessarily the end of the USBTMC message; use the message header and EOM
information to frame commands.

When the host requests instrument data, prepare a response in
``tud_usbtmc_msgBulkIn_request_cb()`` and queue it with
``tud_usbtmc_transmit_dev_msg_data()``.  The driver retains the buffer pointer,
so keep the data valid and unchanged until
``tud_usbtmc_msgBulkIn_complete_cb()``.

Call ``tud_usbtmc_start_bus_read()`` during or soon after open and after each
message/completion path that is ready to accept another command.  Failing to
restart the read is a common reason an instrument answers once and then stops.

Implement abort and clear callbacks as a coherent state machine: stop the
pending operation, report progress through the corresponding check callback,
and restart the bus read when recovery completes.  With USB488 enabled,
``tud_usbtmc_get_stb_cb()`` supplies the status byte and
``tud_usbtmc_msg_trigger_cb()`` handles trigger messages.

The :doc:`../../examples/device/usbtmc` example implements ``*IDN?``, USB488
status/trigger handling, abort/clear, and a PyVISA test script.

Specifications used: *USB Test and Measurement Class Specification*, Revision
1.0, and *USBTMC USB488 Subclass Specification*, Revision 1.0.
