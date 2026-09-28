Device specific known issues and workarounds
===============================================
This page lists known issues and workarounds for specific devices.

NXP LPC54600
----------------
**Severity: High**

**Not recommended for USB device applications (except high-speed host controller)**

Reference: `LPC54600 Errata Sheet`_

.. _LPC54600 Errata Sheet: https://www.nxp.com/docs/en/errata/ES_LPC546XX.pdf

The LPC54600 series have a very buggy USB controller, with 17 issues listed in the errata which is more than half of the total issues.

Most severe issues are:

- USB.2: In USB high-speed device mode, the NBytes field is not correct after BULK IN transfer
- USB.5: In USB full-speed host mode, linked list on done queue is broken.
- USB.15: USB high-speed device in endpoint TX data corruption

NXP i.MX RT1015/RT1020/RT1024/RT1050/RT1060/RT1064
-----------------------------------------------------
**Severity: High** when an isochronous IN endpoint is used behind a hub

Reference: ERR050101 "USB: Endpoint conflict issue in device mode", listed in the errata sheet of
every part above - `IMXRT1015CE`_, `IMXRT1020CE`_, `IMXRT1024CE`_, `IMXRT1050CE`_, `IMXRT1060CE`_
and `IMXRT1064CE`_. On RT1060 and RT1064 it applies to rev A silicon only and is fixed in rev B; on
RT1015, RT1020, RT1024 and RT1050 it is marked *no fix scheduled*, so all silicon is affected.
RT1010, RT116x, RT117x and RT118x do not list it.

.. _IMXRT1015CE: https://www.nxp.com/docs/en/errata/IMXRT1015CE.pdf
.. _IMXRT1020CE: https://www.nxp.com/docs/en/errata/IMXRT1020CE.pdf
.. _IMXRT1024CE: https://www.nxp.com/docs/en/errata/IMXRT1024CE.pdf
.. _IMXRT1050CE: https://www.nxp.com/docs/en/errata/IMXRT1050CE.pdf
.. _IMXRT1060CE: https://www.nxp.com/docs/en/errata/IMXRT1060CE.pdf
.. _IMXRT1064CE: https://www.nxp.com/docs/en/errata/IMXRT1064CE.pdf

While an isochronous IN endpoint is active, an IN token addressed to *that same endpoint number on
another device sharing the host* can silently unprime one of this device's OUT endpoints - control,
bulk, interrupt or isochronous alike. NXP states the unpriming cannot be detected by software and
raises no interrupt, so the endpoint simply stops answering OUT tokens and the transfer never
completes. Typically seen when the device is behind a hub with other devices attached.

Workaround: give isochronous IN endpoints a number that no other device on the same host uses for
any IN endpoint - endpoints 1-3 are used by nearly every composite device, so choose a high number
(``examples/device/usbtest`` uses endpoint 7 on this family for that reason). Devices without an
isochronous IN endpoint are unaffected.

NXP LPC55S2x/LPC552x
---------------------------------
**Severity: Low** (both need specific conditions)

Reference: `LPC55S2x Errata Sheet`_ USB.3, USB.5

.. _LPC55S2x Errata Sheet: https://www.nxp.com/docs/en/errata/ES_LPC55S2x.pdf

USB.3: As a high-speed device behind certain full-speed hubs, the device does not correctly detect
the host's KJ chirp sequence and can behave erratically due to wrong speed detection. The documented
workaround is to set the FORCE_FS bit in DEVCMDSTAT on bus reset when the reported link speed is
full speed. TinyUSB does not implement this workaround.

USB.5: An isochronous IN endpoint sending a 1024-byte maximum-packet-size packet raises no endpoint
interrupt and its command/status entry is not updated. Workaround: cap the isochronous IN maximum
packet size at 1023 bytes in the descriptor.

WCH CH32F20x/CH32V20x/CH32V30x
---------------------------------
**Severity: High**

Reference: `CH32V30X Reference Manual`_ USBFS/USBHS controller chapter

.. _CH32V30X Reference Manual: https://www.wch-ic.com/downloads/CH32FV2x_V3xRM_PDF.html

.. _wch-iso-status-overwrite:

**1. Isochronous transfer status overwritten**

**Not recommended for USB audio applications**

The controllers have no FIFO for transfer completion status. A later isochronous transfer can
overwrite ``INT_ST`` and ``RX_LEN`` before software finishes processing the previous transfer,
causing data corruption.

.. _wch-usbhs-interrupt-starvation:

**2. USBHS interrupt endpoint starvation**

The USBHS driver's ``INT_BUSY`` protection makes endpoints respond with NAK while a transfer
interrupt is pending. This mainly affects interrupt IN endpoints: closely spaced transactions
on different endpoints can reduce throughput or starve an interrupt endpoint, even when its buffer
is ready. Disabling ``INT_BUSY`` is not a safe workaround: an OUT endpoint can accept another packet
before software processes the previous packet and updates its buffer and response.

For example, on NanoCH32V305 with Windows, three 20-byte interrupt IN endpoints polled every
125 us at HS had NAK rates of approximately 80%, 20%, and 0%; the longest gap between successful
EP1 reports was 4 ms. At FS with 1 ms polling, all three delivered approximately 1,000 reports/s
without sequence gaps in an 8-second test.

Workaround: force full speed on the USBHS controller when the application's bandwidth permits it:

.. code-block:: c

   tusb_rhport_init_t dev_init = {
     .role = TUSB_ROLE_DEVICE,
     .speed = TUSB_SPEED_FULL
   };
   tusb_init(BOARD_TUD_RHPORT, &dev_init);

For CH32V30x examples, keep the build option ``SPEED=high`` to select the USBHS controller and
its physical port; the initialization above selects its operating speed. Use descriptors suitable
for full speed. This reduces the timing pressure but does not remove the shared-register
limitation; validate the application's endpoint mix, especially when using isochronous transfers.

Puya PY32F071/072
---------------------------------
**Severity: Very Low**

Reference: `PY32F07x Reference Manual` USBD chapter

The USB device controller (MUSB-like) has 5 application endpoints EP1-EP5 with fixed FIFO sizes
shared between IN and OUT of the same endpoint number: EP1 = 512 B, EP2-4 = 128 B, EP5 = 64 B.
This is much lower than the max ISO ep size of 1024 for high EP numbers.
Place large isochronous endpoints on EP1 and size descriptors accordingly.
