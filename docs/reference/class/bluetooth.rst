*************
Bluetooth HCI
*************

Role: device only.  This driver transports Bluetooth HCI commands, events, and
ACL data over USB.  It does not implement a Bluetooth controller, Link Manager,
or host stack; the application must provide that functionality.

Configuration and descriptors
=============================

Enable ``CFG_TUD_BTH`` and use ``TUD_BTH_DESCRIPTOR`` in the configuration
descriptor.

.. list-table::
   :header-rows: 1
   :widths: 38 17 45

   * - Option
     - Default
     - What it controls
   * - ``CFG_TUD_BTH_ISO_ALT_COUNT``
     - Required
     - Number of isochronous voice alternate settings.  Pass one paired
       IN/OUT packet size per setting to ``TUD_BTH_DESCRIPTOR``.
   * - ``CFG_TUD_BTH_EVENT_EPSIZE``
     - ``16`` bytes
     - Maximum HCI event interrupt-IN packet.
   * - ``CFG_TUD_BTH_DATA_EPSIZE``
     - ``64`` bytes
     - ACL bulk endpoint packet size.  Keep it consistent with the descriptor
       and active bus speed.
   * - ``CFG_TUD_BTH_HISTORICAL_COMPATIBLE``
     - ``0``
     - Uses the legacy HCI command request value required by some historical
       controller implementations.

Set ``CFG_TUD_BTH_HISTORICAL_COMPATIBLE`` only for a controller that requires
the legacy ``bRequest = 0xe0`` behavior described by the Bluetooth Core
specification.  It is not a general compatibility switch.

Data path
=========

.. list-table::
   :header-rows: 1
   :widths: 38 62

   * - API or callback
     - What it does
   * - ``tud_bt_hci_cmd_cb()``
     - Delivers one host HCI command to the controller implementation.
   * - ``tud_bt_acl_data_received_cb()``
     - Delivers received host-to-controller ACL bytes.
   * - ``tud_bt_event_send()``
     - Queues a controller-to-host HCI event; ``false`` means it was not
       accepted.
   * - ``tud_bt_acl_data_send()``
     - Queues controller-to-host ACL data; ``false`` means it was not accepted.
   * - ``tud_bt_event_sent_cb()`` /
       ``tud_bt_acl_data_sent_cb()``
     - Reports completion and releases the corresponding application-owned
       send buffer.

The host delivers HCI commands through ``tud_bt_hci_cmd_cb()`` and ACL data
through ``tud_bt_acl_data_received_cb()``.  The controller sends HCI events with
``tud_bt_event_send()`` and ACL data with ``tud_bt_acl_data_send()``.

The send APIs do not copy the whole packet.  Keep each buffer valid and
unchanged until ``tud_bt_event_sent_cb()`` or ``tud_bt_acl_data_sent_cb()``.
Check the boolean return value before considering a packet queued.

There is currently no dedicated Bluetooth device example.  Use the public API
in ``src/class/bth/bth_device.h`` together with the Bluetooth Core USB
Transport and HCI packet formats.
