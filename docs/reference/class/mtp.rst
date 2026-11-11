***
MTP
***

Role: device only.  Media Transfer Protocol (MTP) presents objects and object
metadata rather than a host-mounted block device.  It is a better fit than MSC
when device firmware and the host must access managed files concurrently.

Configuration
=============

Set ``CFG_TUD_MTP`` to 1 and add ``TUD_MTP_DESCRIPTOR``.  Configure the endpoint
and control buffers, then advertise only operations, events, properties, and
formats that the application actually implements:

.. code-block:: c

   #define CFG_TUD_MTP                    1
   #define CFG_TUD_MTP_EP_BUFSIZE         512
   #define CFG_TUD_MTP_EP_CONTROL_BUFSIZE 16

   #define CFG_TUD_MTP_DEVICEINFO_EXTENSIONS "microsoft.com: 1.0; "
   #define CFG_TUD_MTP_DEVICEINFO_SUPPORTED_OPERATIONS \
     MTP_OP_GET_DEVICE_INFO, MTP_OP_OPEN_SESSION, MTP_OP_CLOSE_SESSION
   #define CFG_TUD_MTP_DEVICEINFO_SUPPORTED_EVENTS \
     MTP_EVENT_OBJECT_ADDED
   #define CFG_TUD_MTP_DEVICEINFO_SUPPORTED_DEVICE_PROPERTIES \
     MTP_DEV_PROP_DEVICE_FRIENDLY_NAME
   #define CFG_TUD_MTP_DEVICEINFO_CAPTURE_FORMATS \
     MTP_OBJ_FORMAT_UNDEFINED, MTP_OBJ_FORMAT_ASSOCIATION
   #define CFG_TUD_MTP_DEVICEINFO_PLAYBACK_FORMATS \
     MTP_OBJ_FORMAT_UNDEFINED, MTP_OBJ_FORMAT_ASSOCIATION

.. list-table::
   :header-rows: 1
   :widths: 45 15 40

   * - Option
     - Default
     - What it controls
   * - ``CFG_TUD_MTP_EP_BUFSIZE``
     - Required
     - Shared bulk data buffer and maximum data chunk, at most 65535 bytes.
       Larger values improve throughput but consume static RAM.
   * - ``CFG_TUD_MTP_EP_CONTROL_BUFSIZE``
     - Required
     - Staging buffer for MTP class control requests and responses.
   * - ``CFG_TUD_MTP_DEVICEINFO_EXTENSIONS``
     - Required
     - MTP extension string returned by GetDeviceInfo; use an empty string when
       no extension is implemented.
   * - ``CFG_TUD_MTP_DEVICEINFO_SUPPORTED_OPERATIONS``
     - Required
     - Operation codes the host is told it may issue.
   * - ``CFG_TUD_MTP_DEVICEINFO_SUPPORTED_EVENTS``
     - Required
     - Event codes the device may send on the interrupt endpoint.
   * - ``CFG_TUD_MTP_DEVICEINFO_SUPPORTED_DEVICE_PROPERTIES``
     - Required
     - Device property codes supported by the application.
   * - ``CFG_TUD_MTP_DEVICEINFO_CAPTURE_FORMATS`` /
       ``CFG_TUD_MTP_DEVICEINFO_PLAYBACK_FORMATS``
     - Required
     - Object formats the device can create or expose for playback.

The ``CFG_TUD_MTP_DEVICEINFO_*`` lists form the GetDeviceInfo response and are
a contract with the host.

Application flow
================

.. list-table::
   :header-rows: 1
   :widths: 42 58

   * - API or callback
     - What it does
   * - ``tud_mtp_mounted()``
     - Tests whether the bulk OUT and bulk IN endpoints are open.
   * - ``tud_mtp_command_received_cb()``
     - Delivers the operation container and starts the application transaction
       state machine.  A negative return stalls both bulk endpoints.
   * - ``tud_mtp_data_send()`` / ``tud_mtp_data_receive()``
     - Starts or continues the operation's data phase.  Returns ``false``
       without changing phase if the transfer cannot be queued.
   * - ``tud_mtp_response_send()``
     - Queues the final response container with the current transaction ID.
       Returns ``false`` without changing phase if the response cannot be
       queued.  May also be called from ``tud_mtp_data_xfer_cb()`` before the
       data-complete notification; the driver absorbs a pending terminating
       OUT ZLP.  Responding before all OUT payload arrives stalls both bulk
       endpoints and discards the queued response.
   * - ``tud_mtp_event_send()``
     - Copies and queues one asynchronous event.  Returns ``false`` while the
       previous event is still in flight; retry later.
   * - ``tud_mtp_data_xfer_cb()``
     - Supplies or consumes the next chunk of a multi-packet data phase.  A
       negative return stalls both bulk endpoints.
   * - ``tud_mtp_data_complete_cb()``
     - Advances application state after the entire data phase.  A negative
       return stalls both bulk endpoints.
   * - ``tud_mtp_response_complete_cb()``
     - Advances application state after the response phase.
   * - ``tud_mtp_request_*_cb()``
     - Handles cancel, reset, status, extended-event, and vendor control
       requests.  Return ``false`` or a negative length where documented to
       stall an unsupported request.  The driver handles transport recovery
       for Cancel and Device Reset; their callbacks update application state.
       In the error phase, the driver answers Get Device Status without
       calling the application.

``tud_mtp_command_received_cb()`` receives an operation container.  The
application performs any data phase with ``tud_mtp_data_send()`` or
``tud_mtp_data_receive()``, then completes the transaction with
``tud_mtp_response_send()``.  Use ``tud_mtp_event_send()`` for asynchronous
events such as ObjectAdded.

The ``tud_mtp_data_xfer_cb()``, ``tud_mtp_data_complete_cb()``, and
``tud_mtp_response_complete_cb()`` callbacks advance multi-stage transfers.
Validate container lengths, object handles, property codes, and storage bounds
before using them.

The :doc:`../../examples/device/mtp` example is the recommended template.  It
implements a small in-memory object store, core session/object operations,
and an upload.  Replace its storage functions while preserving the
command/data/response state machine.

TinyUSB supplies the USB transport and MTP containers; it does not provide a
filesystem, object database, stable handle allocation, or access arbitration.
Those remain application responsibilities.
