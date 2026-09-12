***
HID
***

Roles: device and host.  Human Interface Device (HID) transfers input, output,
and feature reports described by a compact HID report descriptor.

Device
======

Enable ``CFG_TUD_HID`` for the required number of HID interfaces and set
``CFG_TUD_HID_EP_BUFSIZE`` to the largest report transferred on an interrupt
endpoint.  Use ``TUD_HID_DESCRIPTOR`` for IN-only HID or
``TUD_HID_INOUT_DESCRIPTOR`` when an interrupt OUT endpoint is required.

``CFG_TUD_HID_EP_BUFSIZE`` defaults to 64 bytes and allocates one endpoint
buffer per enabled HID instance.  It must include the report ID byte when the
report descriptor uses IDs.  Increasing it permits longer reports but consumes
static RAM; it does not change the report descriptor automatically.

Define a report descriptor and return it from
``tud_hid_descriptor_report_cb()``.  TinyUSB provides templates including
``TUD_HID_REPORT_DESC_KEYBOARD()``, ``TUD_HID_REPORT_DESC_MOUSE()``, and
``TUD_HID_REPORT_DESC_GENERIC_INOUT()``.  When several report types share one
interface, give each a distinct ``HID_REPORT_ID()`` and include that ID in API
calls and report handling.

Send only when the interface is ready:

.. code-block:: c

   if (tud_hid_ready()) {
     tud_hid_keyboard_report(REPORT_ID_KEYBOARD, modifier, keycodes);
   }

Use ``tud_hid_report()`` for a custom layout.  Handle host-to-device output or
feature reports in ``tud_hid_set_report_cb()`` and provide requested input or
feature data in ``tud_hid_get_report_cb()``.  The bytes and lengths must match
the report descriptor exactly.

.. list-table::
   :header-rows: 1
   :widths: 40 60

   * - Device API or callback
     - What it does
   * - ``tud_hid_n_ready()``
     - Tests whether the interrupt IN endpoint for an instance can accept a
       report.
   * - ``tud_hid_n_report()``
     - Copies and queues a custom input report.  ``false`` means it was not
       queued; the caller may reuse its source buffer after the call returns.
   * - ``tud_hid_n_keyboard_report()`` /
       ``tud_hid_n_mouse_report()`` /
       ``tud_hid_n_gamepad_report()``
     - Builds and queues a standard TinyUSB report structure.  Its descriptor
       template must match the chosen helper.
   * - ``tud_hid_descriptor_report_cb()``
     - Returns the report descriptor for an instance.  The returned storage
       must remain valid.
   * - ``tud_hid_get_report_cb()``
     - Fills a control GET_REPORT response and returns its byte count.  Returning
       zero stalls the request.
   * - ``tud_hid_set_report_cb()``
     - Receives an output or feature report from either the control endpoint or
       interrupt OUT endpoint.
   * - ``tud_hid_set_protocol_cb()`` / ``tud_hid_set_idle_cb()``
     - Applies host boot/report protocol and idle-rate requests.  HID idle rate
       units are 4 ms.
   * - ``tud_hid_report_complete_cb()`` /
       ``tud_hid_report_failed_cb()``
     - Announces that the internal endpoint buffer is reusable, or reports the
       number of bytes transferred before failure.

For keyboards and buttons, send a release report as well as the press report;
otherwise the host can retain a stuck key.  The
:doc:`../../examples/device/hid_composite` and
:doc:`../../examples/device/hid_generic_inout` examples show both patterns.

Host
====

Set ``CFG_TUH_HID`` to the maximum simultaneous HID interfaces, not merely the
number of physical devices.  A keyboard with media controls or a composite
controller can expose several interfaces.  Size
``CFG_TUH_HID_EPIN_BUFSIZE`` and ``CFG_TUH_HID_EPOUT_BUFSIZE`` for the largest
reports the application accepts.

.. list-table::
   :header-rows: 1
   :widths: 39 17 44

   * - Option
     - Default
     - What it controls
   * - ``CFG_TUH_HID_EPIN_BUFSIZE``
     - ``64`` bytes
     - Largest interrupt IN report that can be received per HID instance.
   * - ``CFG_TUH_HID_EPOUT_BUFSIZE``
     - ``64`` bytes
     - Largest interrupt OUT report that can be sent per HID instance.
   * - ``CFG_TUH_HID_SET_PROTOCOL_ON_ENUM``
     - ``1``
     - Sends SET_PROTOCOL during enumeration for boot-capable interfaces.
       Set it to ``0`` if the application will select protocol later.

``tuh_hid_mount_cb()`` supplies ``dev_addr``, interface ``idx``, and the report
descriptor.  Parse and retain the information needed to decode later reports,
then queue the first receive:

.. code-block:: c

   void tuh_hid_mount_cb(uint8_t dev_addr, uint8_t idx,
                         uint8_t const *desc, uint16_t desc_len) {
     parse_report_descriptor(desc, desc_len);
     tuh_hid_receive_report(dev_addr, idx);
   }

   void tuh_hid_report_received_cb(uint8_t dev_addr, uint8_t idx,
                                   uint8_t const *report, uint16_t len) {
     process_report(report, len);
     tuh_hid_receive_report(dev_addr, idx); // Re-arm interrupt IN.
   }

If a report descriptor is larger than ``CFG_TUH_ENUMERATION_BUFSIZE``, the
mount callback can receive ``desc == NULL`` and ``desc_len == 0``.  Increase
the enumeration buffer or handle that case without dereferencing the pointer.

Use ``tuh_hid_send_report()`` for an interrupt OUT report and
``tuh_hid_get_report()``/``tuh_hid_set_report()`` for control transfers.  Boot
keyboards and mice can use boot protocol; all other devices require report
protocol and parsing of their descriptor.

.. list-table::
   :header-rows: 1
   :widths: 40 60

   * - Host API or callback
     - What it does
   * - ``tuh_hid_mounted()`` / ``tuh_hid_itf_get_info()``
     - Tests a device/interface pair and retrieves its cached interface
       descriptor information.
   * - ``tuh_hid_interface_protocol()`` / ``tuh_hid_get_protocol()``
     - Distinguishes keyboard/mouse/none interface protocol from the active
       boot/report transfer protocol.
   * - ``tuh_hid_receive_ready()`` / ``tuh_hid_receive_report()``
     - Tests and arms one interrupt IN transfer.  Re-arm it after every receive
       callback for continuous input.
   * - ``tuh_hid_send_ready()`` / ``tuh_hid_send_report()``
     - Tests and queues one interrupt OUT report.  The send callback releases
       the internal endpoint buffer; the source is copied before return.
   * - ``tuh_hid_get_report()`` / ``tuh_hid_set_report()``
     - Starts a control endpoint report request.  Completion callbacks report
       zero length on a stall or transfer error; keep the caller's report
       buffer valid until that callback.
   * - ``tuh_hid_set_protocol()``
     - Requests boot or report protocol on boot-capable interfaces; completion
       is reported asynchronously.
   * - ``tuh_hid_mount_cb()`` / ``tuh_hid_umount_cb()``
     - Supplies the report descriptor at mount and announces when the interface
       index is no longer valid.

See :doc:`../../examples/host/cdc_msc_hid` for keyboard/mouse handling and
:doc:`../../examples/host/hid_controller` for controller input and output.

Specification used: *Device Class Definition for Human Interface Devices
(HID)*, Version 1.11.  HID Usage Tables define the individual usage pages and
codes used inside report descriptors.
