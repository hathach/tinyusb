******************
Using Host Classes
******************

TinyUSB provides application-level host drivers for CDC serial, HID, MIDI 1.0,
MIDI 2.0, and Mass Storage.  A host application is asynchronous: a mount
callback reports a ready interface, I/O is queued, and completion or receive
callbacks advance the application state.

Setup checklist
===============

1. Enable ``CFG_TUH_ENABLED`` and set each ``CFG_TUH_*`` pool size in
   ``tusb_config.h``.  HID and MIDI values count interfaces, so allow for more
   than one interface per physical device.
2. Set ``CFG_TUH_DEVICE_MAX`` for the number of attached devices and enable
   ``CFG_TUH_HUB`` if hubs are required.
3. Initialize a host-capable root port and provide VBUS as required by the
   board.
4. Call ``tuh_task()`` continuously, or run it in a dedicated RTOS task.
5. Start class I/O from its mount callback and requeue receive transfers where
   the class guide requires it.

Typical configuration:

.. code-block:: c

   #define CFG_TUH_ENABLED    1
   #define CFG_TUH_DEVICE_MAX 4
   #define CFG_TUH_HUB        1
   #define CFG_TUH_CDC        1
   #define CFG_TUH_HID        (3 * CFG_TUH_DEVICE_MAX)
   #define CFG_TUH_MSC        1

Common configuration options
============================

.. list-table::
   :header-rows: 1
   :widths: 30 18 52

   * - Option
     - Default
     - What it controls
   * - ``CFG_TUH_ENABLED``
     - Root-port mode
     - Enables the host stack.  The board must also supply VBUS and a
       host-capable controller/PHY.
   * - ``CFG_TUH_MAX_SPEED``
     - Root-port mode
     - Highest bus speed supported by the host build.  It does not make a
       full-speed-only controller operate at high speed.
   * - ``CFG_TUH_DEVICE_MAX``
     - ``1``
     - Number of USB device addresses tracked simultaneously, including hubs.
   * - ``CFG_TUH_HUB``
     - ``0``
     - Number of hubs supported simultaneously.  Hub ports can require higher
       device and class pool counts.
   * - ``CFG_TUH_ENUMERATION_BUFSIZE``
     - ``256`` bytes
     - Temporary descriptor buffer used during enumeration.  Increase it for
       long configuration or HID report descriptors; this consumes static RAM.
   * - ``CFG_TUH_TASK_EVENTS_PER_RUN``
     - ``16``
     - Maximum events handled by one ``tuh_task_ext()`` call.  ``0`` is
       unlimited.
   * - ``CFG_TUH_MEM_SECTION`` / ``CFG_TUH_MEM_ALIGN``
     - Common USB settings / 4-byte alignment
     - Places and aligns host-controller buffers for DMA-accessible RAM.
   * - ``CFG_TUSB_OS``
     - ``OPT_OS_NONE``
     - Selects TinyUSB's synchronization backend.  Set the matching OS option
       when host APIs and ``tuh_task()`` run in different RTOS tasks.

Each ``CFG_TUH_<CLASS>`` value sizes a simultaneous interface pool.  It is not
a VID/PID allowlist and, for composite devices, may need to exceed
``CFG_TUH_DEVICE_MAX``.

Core API and callbacks
======================

.. list-table::
   :header-rows: 1
   :widths: 36 64

   * - API or callback
     - What it does
   * - ``tusb_init()``
     - Initializes a root port with host role and selected speed.  Call it
       after board/VBUS setup and check its boolean result.
   * - ``tuh_task()`` / ``tuh_task_ext()``
     - Advances enumeration and transfers and dispatches callbacks.  The
       extended form controls wait timeout and ISR context.
   * - ``tuh_mount_cb()`` / ``tuh_umount_cb()``
     - Announces a configured device or detachment.  Class mount callbacks
       provide the interface-specific indices used for I/O.
   * - ``tuh_mounted()`` / ``tuh_ready()`` / ``tuh_connected()``
     - Tests whether an address is configured/ready or has merely shown bus
       activity.  Do not start class I/O based only on ``tuh_connected()``.
   * - ``tuh_vid_pid_get()`` / ``tuh_speed_get()`` / ``tuh_bus_info_get()``
     - Returns cached identity, speed, and hub/root-port location for an
       enumerated address.
   * - ``tuh_descriptor_get_device_local()``
     - Copies the cached device descriptor without issuing a USB transfer.
       Other ``tuh_descriptor_get_*()`` calls queue or perform control
       transfers to fetch descriptors.
   * - ``tuh_control_xfer()``
     - Submits a control transfer described by ``tuh_xfer_t``.  A non-null
       completion callback makes it asynchronous; a null callback blocks.
   * - ``tuh_edpt_xfer()``
     - Submits a bulk or interrupt endpoint transfer.  Application class
       drivers normally use their class-specific wrappers instead.

Synchronous host control calls are forbidden from the host task when
``CFG_TUSB_OS_HAS_SCHEDULER`` is true: that task is needed to make the same
transfer complete.  Prefer callbacks for portable application code.

Addresses and indices
=====================

``dev_addr`` identifies an enumerated USB device.  Classes that may expose
multiple interfaces also use a class ``idx``.  Preserve both values supplied by
the mount callback and use the same pair for later API calls.  An index can be
reused after unmount, so discard associated application state in the unmount
callback.

Transfer lifetime
=================

Unless an API explicitly documents a copy, keep a transfer buffer valid and
unchanged until its completion callback.  Buffers used directly by a host
controller may also require alignment, cache maintenance, or placement in
DMA-accessible memory; follow the board's HCD requirements.

Do not block ``tuh_task()`` while waiting for a callback that only it can
dispatch.  Prefer the asynchronous APIs.  Where a class provides a synchronous
helper, use it only from a context in which the host task can still run.

Start with :doc:`../../examples/host/cdc_msc_hid` for CDC, HID, and MSC,
:doc:`../../examples/host/midi_rx` for MIDI 1.0, or
:doc:`../../examples/host/midi2_host` for MIDI 2.0.
