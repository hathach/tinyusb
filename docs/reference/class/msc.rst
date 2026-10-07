************
Mass Storage
************

Roles: device and host.  TinyUSB implements the USB Mass Storage Bulk-Only
Transport (BOT) and common SCSI commands.  It exposes logical blocks; a
filesystem such as FatFs is a separate application layer.

Device
======

Enable ``CFG_TUD_MSC`` and set ``CFG_TUD_MSC_EP_BUFSIZE``.  The driver supports
a single Mass Storage function, so add one ``TUD_MSC_DESCRIPTOR``.  Multiple
logical units (LUNs) share that function and are selected through the ``lun``
callback argument.

``CFG_TUD_MSC_EP_BUFSIZE`` has no default and is required when MSC is enabled.
It is the class transfer-buffer size, not the media capacity.  Using at least
one logical block is efficient; larger values improve large transfers at the
cost of static RAM.  It must be at least 64 and below 65535 bytes, and should be
a multiple of the bulk endpoint's maximum packet size (512 at high speed) so
that READ10/WRITE10 data is split only on packet boundaries.  Keep it
consistent with controller/DMA constraints.

The application must implement ``tud_msc_test_unit_ready_cb()``,
``tud_msc_capacity_cb()``, ``tud_msc_read10_cb()``, ``tud_msc_write10_cb()``
and ``tud_msc_scsi_cb()``.  The other callbacks below have weak defaults.

.. list-table::
   :header-rows: 1
   :widths: 38 62

   * - Callback
     - Purpose
   * - ``tud_msc_inquiry2_cb()``
     - Fill the INQUIRY response and return its length.  The older
       ``tud_msc_inquiry_cb()`` still works when this returns 0.
   * - ``tud_msc_test_unit_ready_cb()``
     - Report whether media is present and usable.
   * - ``tud_msc_capacity_cb()``
     - Return the logical block count and block size.
   * - ``tud_msc_read10_cb()``
     - Read ``bufsize`` bytes from ``lba`` plus ``offset``.
   * - ``tud_msc_write10_cb()``
     - Write ``bufsize`` bytes to ``lba`` plus ``offset``.
   * - ``tud_msc_scsi_cb()``
     - Handle commands not implemented by the class driver.
   * - ``tud_msc_get_maxlun_cb()``
     - Returns the number of LUNs (default 1); the driver reports this minus
       one to the host.  Implement it for multiple LUNs.
   * - ``tud_msc_is_writable_cb()``
     - Reports write protection before WRITE10.
   * - ``tud_msc_start_stop_cb()``
     - Handles load/eject and start/stop requests, including safe-eject policy.

Read and write callbacks may cover only part of a logical block; honor both
``lba`` and ``offset`` instead of assuming one callback per block.
``TUD_MSC_RET_ERROR`` from read10/write10 overwrites the stored sense with
NOT READY / MEDIUM NOT PRESENT.  For other command callbacks, such as
``tud_msc_scsi_cb()``, set useful sense data with ``tud_msc_set_sense()``
before returning an error.

For temporarily busy media, return ``TUD_MSC_RET_BUSY``; TinyUSB will invoke
the callback again with the same parameters.  For true background I/O, return
``TUD_MSC_RET_ASYNC`` and later call ``tud_msc_async_io_done()`` with the byte
count or error.  Do not report a write complete until data has reached the
durability level promised by the product.  ``tud_msc_write10_complete_cb()`` is
a useful place to flush a cache.

See :doc:`../../examples/device/cdc_msc` for a RAM disk and
:doc:`../../examples/device/msc_dual_lun` for multiple LUNs.

Host
====

Set ``CFG_TUH_MSC`` to 1 to enable the driver; it keeps state for every device
address, so ``CFG_TUH_DEVICE_MAX`` sets how many Mass Storage devices can be
used at once.  ``CFG_TUH_MSC_MAXLUN`` is the LUN limit per device.  After
``tuh_msc_mount_cb(dev_addr)``, query cached geometry with
``tuh_msc_get_block_count()`` and ``tuh_msc_get_block_size()``.

Pass a LUN below both ``tuh_msc_get_maxlun()`` and ``CFG_TUH_MSC_MAXLUN``: the
LUN argument is not range-checked, and ``tuh_msc_get_maxlun()`` returns the
LUN count the device reports without clamping it.  ``CFG_TUH_MSC_MAXLUN``
defaults to 4 and allocates cached state per possible LUN for each of the
``CFG_TUH_DEVICE_MAX`` addresses.  Enumeration only reads the geometry of
LUN 0, so other LUNs read back as 0.

``tuh_msc_read10()`` and ``tuh_msc_write10()`` are asynchronous.  Keep the
buffer valid, correctly aligned, cache coherent, and accessible to the USB
controller until the ``tuh_msc_complete_cb_t`` callback runs.  Check
``tuh_msc_ready()`` before starting another command and inspect the completion
callback's transfer result and command status.

.. list-table::
   :header-rows: 1
   :widths: 40 60

   * - Host API or callback
     - What it does
   * - ``tuh_msc_mounted()`` / ``tuh_msc_ready()``
     - Tests whether the MSC device is present or currently able to accept a
       new SCSI command.
   * - ``tuh_msc_get_maxlun()`` /
       ``tuh_msc_get_block_count()`` /
       ``tuh_msc_get_block_size()``
     - Returns the device-reported LUN count and the geometry cached during
       enumeration (LUN 0 only).
   * - ``tuh_msc_read10()`` / ``tuh_msc_write10()``
     - Queues an integral number of logical blocks and completes through the
       supplied callback.  Returns ``false`` without queuing if the device is
       not mounted or the byte count exceeds 32 bits.
   * - ``tuh_msc_inquiry()`` / ``tuh_msc_request_sense()``
     - Queues standard SCSI identification or detailed-error requests into an
       application-owned response buffer.
   * - ``tuh_msc_scsi_command()``
     - Queues a caller-built command block wrapper for commands without a
       convenience API.
   * - ``tuh_msc_mount_cb()`` / ``tuh_msc_umount_cb()``
     - Announces cached capacity availability or invalidates all filesystem and
       media state for the address.

TinyUSB does not mount a filesystem automatically.  Connect the sector API to
your filesystem's disk I/O layer and invalidate that state in
``tuh_msc_umount_cb()``.  The :doc:`../../examples/host/msc_file_explorer`
example demonstrates this with FatFs.

Practical notes
===============

* Hosts cache filesystem data.  Physical removal or firmware reset without an
  eject/unmount can lose data even when USB transfers succeeded.
* Use the medium-present and write-protect responses consistently; mismatched
  capacity or readiness information causes repeated SCSI recovery traffic.
* Validate every LUN and block range before calculating a storage address.

Specification used: *USB Mass Storage Class Bulk-Only Transport*, Revision
1.0.  Command formats and sense data come from the applicable SCSI command set.
