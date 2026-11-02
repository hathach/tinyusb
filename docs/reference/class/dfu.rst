***
DFU
***

Role: device only.  Device Firmware Upgrade (DFU) has two distinct states:
runtime mode, where normal firmware advertises that it can reboot into an
updater, and DFU mode, where firmware images are transferred.

Runtime mode
============

Enable ``CFG_TUD_DFU_RUNTIME`` and add ``TUD_DFU_RT_DESCRIPTOR``.  After a
DFU_DETACH, TinyUSB calls ``tud_dfu_runtime_reboot_to_dfu_cb()``, where the
application switches to its DFU image:

* with ``bitWillDetach`` set, once the device has completed the request's
  status stage.  That completion is reported by the device controller and is
  not proof that the host received it;
* otherwise on the next USB bus reset, the host's cue to enter DFU mode.

Once DFU_DETACH completes the interface is in appDETACH.  There DFU_GETSTATUS
and DFU_GETSTATE report appDETACH, and any other DFU request, including one
sent in the wrong direction, stalls and returns to appIDLE, cancelling the
detach.  An unplug (on ports that report one) or a SET_CONFIGURATION that
changes the configuration, including to 0, also cancels it, so a later bus
reset does not call the callback; repeating the active one leaves it pending.

The runtime driver deviates from DFU 1.1 section 5.1 and Appendix A.2.2:

* It runs no detach timer.  ``wDetachTimeOut`` and the request's ``wTimeout``
  are ignored and a pending detach never expires: a bus reset arriving any
  time later, before one of the cancelling events above, still calls the
  callback.
* A repeated DFU_DETACH in appDETACH is accepted and re-arms the detach
  instead of stalling; with ``bitWillDetach`` set the callback runs again.

The descriptor's detach attributes must describe the actual behavior.  Set
``bitWillDetach`` only when the firmware detaches and re-attaches by itself.

See :doc:`../../examples/device/dfu_runtime`; its callback only changes the LED
blink rate and does not detach, so it clears ``bitWillDetach``.

.. list-table::
   :header-rows: 1
   :widths: 38 62

   * - Descriptor attribute
     - Meaning
   * - ``DFU_ATTR_CAN_DOWNLOAD``
     - Host may send firmware to the device.
   * - ``DFU_ATTR_CAN_UPLOAD``
     - Host may read firmware from the device; omit it when disclosure is not
       intended.
   * - ``DFU_ATTR_MANIFESTATION_TOLERANT``
     - Device can remain in DFU mode after manifestation without reset.
   * - ``DFU_ATTR_WILL_DETACH``
     - Device performs its own detach/reset after DFU_DETACH.  Clear it when
       the host must issue the USB reset.

DFU mode
========

Enable ``CFG_TUD_DFU`` and set ``CFG_TUD_DFU_XFER_BUFSIZE`` to exactly the
``wTransferSize`` passed to ``TUD_DFU_DESCRIPTOR``.  Alternate settings can
represent partitions or targets; their string descriptors should clearly name
the target presented by each ``alt`` value.

.. list-table::
   :header-rows: 1
   :widths: 38 62

   * - Callback/API
     - Application responsibility
   * - ``tud_dfu_download_cb()``
     - Program one downloaded block, then call
       ``tud_dfu_finish_flashing()`` when the operation completes.
   * - ``tud_dfu_upload_cb()``
     - Fill the buffer with at most the requested number of bytes and return
       the count.
   * - ``tud_dfu_manifest_cb()``
     - Validate/finalize the image, then call ``tud_dfu_finish_flashing()``.
   * - ``tud_dfu_get_timeout_cb()``
     - Return an honest poll timeout for the current target and state.
   * - ``tud_dfu_abort_cb()``
     - Cancel pending storage work and return the target to a safe state.
   * - ``tud_dfu_detach_cb()``
     - Handles DFU_DETACH while in DFU mode; normally records state and resets
       or returns to runtime firmware according to the descriptor attributes.
   * - ``tud_dfu_finish_flashing()``
     - Completes a previously started download or manifestation.  An error
       status moves the state machine into the DFU error state.

Storage can be asynchronous: retain the operation state, return from the
callback, and call ``tud_dfu_finish_flashing(status)`` later.  Pass
``DFU_STATUS_OK`` only after the data is durably written or manifestation is
complete.

The :doc:`../../examples/device/dfu` example exposes two alternate settings
and can be exercised with ``dfu-util``.

Production safety
=================

Treat all DFU fields and image bytes as untrusted.  Bounds-check ``alt``, block
number, offset, and length before accessing storage.  A production updater
should authenticate the complete image, reject rollback when required, avoid
overwriting its recovery path, and remain bootable after loss of power at any
point.  TinyUSB implements the USB transport and DFU state machine; it does not
provide those product-specific security guarantees.

Specification used: *USB Device Class Specification for Device Firmware
Upgrade*, Version 1.1.
