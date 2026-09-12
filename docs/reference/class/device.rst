********************
Using Device Classes
********************

A device class needs three matching pieces: a nonzero ``CFG_TUD_*`` instance
count, class descriptors in the configuration descriptor, and the required
application callbacks.  Start from the nearest device example instead of
writing descriptors from scratch.

Setup checklist
===============

1. Enable the device stack and each class in ``tusb_config.h``.  A class value
   is normally the maximum number of simultaneous class instances, not a
   boolean.
2. Add the matching ``TUD_*_DESCRIPTOR`` macro to the configuration descriptor
   and include its ``TUD_*_DESC_LEN`` in the total length.
3. Assign unique interface numbers and endpoint addresses.  Some functions use
   more than one interface; for example, CDC ACM normally uses two.
4. Implement the descriptor and class callbacks used by the example.
5. Call ``tud_task()`` regularly, or run it in a dedicated RTOS task.

For example, one CDC ACM function starts with:

.. code-block:: c

   // tusb_config.h
   #define CFG_TUD_ENABLED 1
   #define CFG_TUD_CDC     1

   // One entry inside the configuration descriptor
   TUD_CDC_DESCRIPTOR(ITF_NUM_CDC, 0, EPNUM_CDC_NOTIF, 8,
                      EPNUM_CDC_OUT, EPNUM_CDC_IN, 64),

See :doc:`../../integration` for stack initialization and the full descriptor
callback pattern.

Common configuration options
============================

These options apply before the class-specific settings described on the other
pages.  Defaults come from ``src/tusb_option.h``.

.. list-table::
   :header-rows: 1
   :widths: 30 18 52

   * - Option
     - Default
     - What it controls
   * - ``CFG_TUD_ENABLED``
     - Root-port mode
     - Enables the device stack.  Set it explicitly when the selected root-port
       mode does not already select device operation.
   * - ``CFG_TUD_MAX_SPEED``
     - Root-port mode
     - Highest speed for which the device stack and descriptors are built.
       High-speed devices also need valid qualifier and other-speed
       descriptors.
   * - ``CFG_TUD_ENDPOINT0_SIZE``
     - ``64`` bytes
     - Control endpoint maximum packet size.  It must match ``bMaxPacketSize0``
       in the device descriptor and the controller's capability.
   * - ``CFG_TUD_ENDPOINT0_BUFSIZE``
     - Endpoint 0 size
     - Staging space for control transfers.  Increase it when a class control
       request must hold more than one endpoint packet.
   * - ``CFG_TUD_INTERFACE_MAX``
     - ``16``
     - Maximum total USB interfaces across the active configuration, including
       every interface used by composite functions.
   * - ``CFG_TUD_TASK_EVENTS_PER_RUN``
     - ``16``
     - Maximum events handled by one ``tud_task_ext()`` call.  ``0`` removes
       the limit; a smaller value reduces one-call latency at the cost of more
       task invocations.
   * - ``CFG_TUD_ENDPPOINT_MAX``
     - Controller maximum
     - Highest endpoint-number pool retained by the stack.  Lowering it can
       save RAM, but it must cover every configured endpoint number.
   * - ``CFG_TUD_MEM_SECTION`` / ``CFG_TUD_MEM_ALIGN``
     - Common USB settings / 4-byte alignment
     - Places and aligns controller-facing buffers for DMA.  Override these
       when the device controller requires a particular RAM region or
       alignment.

``CFG_TUD_ENDPPOINT_MAX`` contains the double ``P`` for compatibility; use the
spelling shown above.

Core API and callbacks
======================

.. list-table::
   :header-rows: 1
   :widths: 36 64

   * - API or callback
     - What it does
   * - ``tusb_init()``
     - Initializes a root port with an explicit role and speed.  Call it before
       the task function and check its boolean result.
   * - ``tud_task()`` / ``tud_task_ext()``
     - Dispatches bus, control, class, and completion events.  The extended
       form selects a wait timeout and states whether the call is from an ISR.
   * - ``tud_connected()`` / ``tud_mounted()`` / ``tud_ready()``
     - Reports progressively stronger states: bus activity, configured by the
       host, and configured plus not suspended.  Use ``tud_ready()`` before
       initiating normal traffic.
   * - ``tud_suspended()`` / ``tud_remote_wakeup()``
     - Tests suspend state and requests remote wakeup.  Wakeup succeeds only
       when the host enabled it and the device is suspended.
   * - ``tud_disconnect()`` / ``tud_connect()``
     - Controls the USB pull-up to force a logical detach or attach.  These
       return ``false`` when the controller cannot provide the operation.
   * - ``tud_mount_cb()`` / ``tud_umount_cb()``
     - Announces configuration and removal.  Initialize or discard
       configuration-dependent application state here.
   * - ``tud_suspend_cb()`` / ``tud_resume_cb()``
     - Announces bus power-state changes.  The suspend callback also reports
       whether remote wakeup was enabled by the host.
   * - ``tud_descriptor_*_cb()``
     - Supplies device, configuration, string, BOS, and high-speed companion
       descriptors on request.  Returned storage must remain valid through the
       control transfer.
   * - ``tud_control_xfer()`` / ``tud_control_status()``
     - Completes the data/status stages of an application-handled control
       request.  The data length is truncated to the request's ``wLength``.

Interfaces and instances
========================

Single-instance helpers such as ``tud_cdc_read()`` operate on instance zero.
Their ``_n_`` forms, such as ``tud_cdc_n_read(itf, ...)``, select a class
instance when the corresponding ``CFG_TUD_*`` value is greater than one.
Class instance numbers are not necessarily USB ``bInterfaceNumber`` values.

Endpoint direction is always described from the USB device's point of view:

* IN sends data from the device to the host.
* OUT receives data from the host at the device.

Buffers and callbacks
=====================

Class callbacks run when ``tud_task()`` processes an event, unless a header
explicitly labels a helper as ISR-safe.  Keep callbacks short and move lengthy
work to an application task.

Buffered write APIs return the number of bytes accepted, which can be shorter
than requested.  Check the return value and use the class's flush function when
latency matters.  Size endpoint and software buffers for the active bus speed;
copy the full-speed/high-speed pattern from an example that supports both.

Before testing on hardware, verify that:

* the configuration descriptor's total length and interface count are exact;
* every endpoint address is unique within the configuration;
* descriptor packet sizes agree with the relevant ``CFG_TUD_*_EPSIZE`` values;
* callbacks never retain a pointer whose documented lifetime has ended.
