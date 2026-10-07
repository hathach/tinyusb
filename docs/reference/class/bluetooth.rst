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
     - Not used by the driver.  The event endpoint size is the
       ``_ep_evt_size`` argument of ``TUD_BTH_DESCRIPTOR``.
   * - ``CFG_TUD_BTH_DATA_EPSIZE``
     - ``64`` bytes
     - ACL bulk endpoint packet size.  Keep it consistent with the descriptor
       and active bus speed.
   * - ``CFG_TUD_BTH_HISTORICAL_COMPATIBLE``
     - ``0``
     - Also accepts device-addressed HCI commands with ``bRequest = 0xe0``;
       see below.

Some hosts send HCI commands with ``bRequest = 0xe0``, and the Bluetooth Core
specification (v5.3, Vol 4, Part B, 2.2.1) says the controller should accept
them.  Enable ``CFG_TUD_BTH_HISTORICAL_COMPATIBLE`` when the device must work
with such hosts; otherwise those commands are stalled.

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
     - Starts sending a controller-to-host HCI event; ``false`` means the
       endpoint is still busy or the transfer was not started.
   * - ``tud_bt_acl_data_send()``
     - Starts sending controller-to-host ACL data; ``false`` means the
       endpoint is still busy or the transfer was not started.
   * - ``tud_bt_event_sent_cb()`` /
       ``tud_bt_acl_data_sent_cb()``
     - Reports completion and releases the corresponding application-owned
       send buffer.

The host delivers HCI commands through ``tud_bt_hci_cmd_cb()`` and ACL data
through ``tud_bt_acl_data_received_cb()``.  The controller sends HCI events with
``tud_bt_event_send()`` and ACL data with ``tud_bt_acl_data_send()``.

Both receive callbacks pass a pointer into the driver's own buffer, which the
next transfer reuses once the callback returns.  Consume or copy the data
before returning.

The send APIs do not copy: the controller reads the buffer directly.  Place it
in ``CFG_TUD_MEM_SECTION`` and align it with ``CFG_TUD_MEM_ALIGN`` (see
:doc:`device`), to whole D-cache lines when ``CFG_TUD_MEM_DCACHE_ENABLE`` is
set.  Keep it valid and unchanged until
``tud_bt_event_sent_cb()`` or ``tud_bt_acl_data_sent_cb()``.  Check the boolean
return value before considering a packet sent.

There is currently no dedicated Bluetooth device example.  Use the public API
in ``src/class/bth/bth_device.h`` together with the Bluetooth Core USB
Transport and HCI packet formats.
