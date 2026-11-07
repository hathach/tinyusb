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

.. _wch-trace-timing:

**3. Tracing shifts bus timing**

The USBFS and USBHS device drivers both enable the ``INT_BUSY`` NAK of
:ref:`item 2 <wch-usbhs-interrupt-starvation>`, so ISR time before the transfer flag is cleared is
time the bus NAKs. USBFS clears the flag at the end of its ISR; USBHS clears it first for endpoints
other than EP0. SystemView records the ISR entry inside that window at every ``SYSVIEW`` level.
Levels 2-4 add records before an endpoint is re-armed, in task context or from the ISR (audio).
Records or logging executed before the transfer flag is cleared widen the window further. Traced
builds can therefore see more NAKs and lower throughput: compare WCH timings only between builds at
the same ``SYSVIEW`` level, and measure throughput untraced.
ch32v20x's FSDEV port (port 0) has no ``INT_BUSY``.

Puya PY32F071/072
---------------------------------
**Severity: Very Low**

Reference: `PY32F07x Reference Manual` USBD chapter

The USB device controller (MUSB-like) has 5 application endpoints EP1-EP5 with fixed FIFO sizes
shared between IN and OUT of the same endpoint number: EP1 = 512 B, EP2-4 = 128 B, EP5 = 64 B.
This is much lower than the max ISO ep size of 1024 for high EP numbers.
Place large isochronous endpoints on EP1 and size descriptors accordingly.

Raspberry Pi RP2040
-------------------
**Severity: High** for a full-speed device behind a high-speed hub

Reference: RP2040-E15 in the `RP2040 Datasheet`_; section 12.7.2.2.4 of the `RP2350 Datasheet`_ describes the cause
and its fix in RP2350.

.. _RP2040 Datasheet: https://datasheets.raspberrypi.com/rp2040/rp2040-datasheet.pdf
.. _RP2350 Datasheet: https://datasheets.raspberrypi.com/rp2350/rp2350-datasheet.pdf

When a hub cuts off a token on the device's port with a bit-stuffing error, the device controller can lock up. It
then answers no token, SETUP included, until the host resets the port. The core keeps running, and the only trace is
``SIE_STATUS.BIT_STUFF_ERROR``.

The erratum describes the Raspberry Pi 4's VL805 hub at the end of a frame, but an ordinary high-speed hub does it too.
When a full-speed bulk or control endpoint behind the hub returns STALL, the host clears the hub's TT buffer for that
endpoint (Clear_TT_Buffer). On a Genesys 05e3:0610 hub, this can cut off the packet in progress on any of the hub's
full-speed ports, the stalled device's and the others alike. This happens even in multi-TT mode, where each port has
its own transaction translator. A capture of the hub's high-speed link showed an RP2040 going silent from the
transaction it had in progress while the hub handled a Clear_TT_Buffer for another port. Any full-speed device's STALL
can trigger it: a mass storage device that stalls the kernel's 192-byte MODE SENSE, about every 25 s during disk
traffic, is enough. A WCH 1a86:8091 hub in multi-TT mode, with the same devices and the same Clear_TT_Buffer requests,
left the RP2040 unaffected in 7 of 7 runs.

The hub then answers for the silent device: STALL to its control and bulk transactions, an error to its periodic ones.
To the host the locked device seems to stall every control request, and how it recovers depends on the class driver.
On a Linux host:

- Mass storage: ``usb-storage`` times out and resets the port, so the device re-enumerates and loses its class state.
- Audio: ``snd-usb-audio`` never resets the port, so the device stays dead until it is unplugged. A ``uac2_headset``
  locked up in 5 of 10 one-minute runs next to a stalling mass storage device.
- CDC: each stalled bulk transfer makes the host clear the hub's TT buffer again, as long as the tty is open: about 2
  Clear_TT_Buffer requests per millisecond, which keep cutting off the other full-speed devices on the hub. An STM32
  mass storage device on another port was reset 115 to 168 times a minute, with I/O errors, in 5 of 5 runs.

This was also measured on a Linux host with that hub in multi-TT mode and a second full-speed device on another port:

- An RP2040 ``cdc_msc`` device was reset about 4 times per enumeration, once per MSC STALL during the disk probe, while
  the other device streamed HID.
- An STM32 (FSDEV) device in the same place was reset 3 times in 41 enumerations.
- An RP2040 streaming HID locked up in 38 of 100 disk probes of an STM32 ``cdc_msc`` device on another port, and in
  none of 60 when that device padded its short replies instead of stalling.

RP2350 is not affected: its controller has the fix (``LINESTATE_TUNING.SIE_RX_BITSTUFF_FIX``, set at reset).

On RP2040, nothing short of a bus reset recovers the controller. Pulsing ``MAIN_CTRL.CONTROLLER_EN`` or
``SIE_CTRL.TRANSCEIVER_PD`` from the bit-stuff error interrupt did not help. TinyUSB's E15 workaround, which keeps bulk
IN buffers from being armed late in a frame, does not cover this case.

Mitigation in TinyUSB: at full speed, the MSC device class pads a short Data-In reply with zeros up to the host's
length instead of stalling, by default on every MCU (``CFG_TUD_MSC_PAD_SHORT_REPLY``, 0 to stall instead), so a
TinyUSB mass storage device triggers no lockup in itself or in an RP2040 on the same hub:

- A reply to a command other than READ10 is padded only when the host's length is at most
  ``CFG_TUD_MSC_EP_BUFSIZE``. A longer host length still gets the STALL.
- A READ10 chunk shorter than requested and not a multiple of the Bulk-In packet size is padded for any host length.
  A partial chunk that is a multiple of the packet size needs no padding: the transfer continues with the next chunk.

That removes the most frequent STALLs, those during the disk probe and the kernel's periodic MODE SENSE. Any other
STALL can still trigger the lockup, from this device or from another full-speed device on the same hub.

Workarounds:

- Connect the RP2040 to a root port, which has no transaction translator. A multi-TT hub is not enough: the Genesys
  hub above was in multi-TT mode.
- Detect the lockup in the application and restart the device stack with ``tud_deinit()`` followed, after a short
  delay, by ``tud_init()``, as Raspberry Pi's debugprobe firmware does. On RP2040 ``tud_deinit()`` resets the USB
  controller block and ``tud_init()`` sets it up again; ``tud_disconnect()`` and ``tud_connect()`` only switch the D+
  pull-up and do not reset the controller. The sign of a lockup is that the frame counter ``SOF_RD`` stops advancing
  while the device is connected and not suspended.

Espressif ESP32-S2/S3/P4
------------------------
**Severity: High** for a full-speed host with a low-speed device behind a full-speed hub

Reference: espressif/esp-idf#15683, where ESP-IDF delays each low-speed control stage by 1 ms.

Affects the full-speed USB OTG core: the only one on ESP32-S2/S3, and USB OTG 1.1 (port 0) on
ESP32-P4. The high-speed core of ESP32-P4 (port 1) reaches low-speed devices through a high-speed
hub's transaction translator and sends no preamble.

To reach a low-speed device through a full-speed hub the host sends a preamble (PRE) packet before
each low-speed packet. The transmitter can hang right after a PRE: the low-speed packet never
follows, the channel reports a transaction error and, after a timeout, the core drops the root port.

On ESP32-S3 with a logic analyzer on D+/D-, the hang only hit the PRE of a transaction the driver
started while the last packet on the bus was a low-speed one (about 2% of those, independent of the
idle gap). A PRE after an SOF or another full-speed packet, inside a transaction, or of the core's
own NAK retries and multi-packet sequence never hung.

On the ESP32-P4 port 0 with two low-speed HID devices and a flash drive behind a full-speed hub, the
root port dropped and re-enumerated every 1.5-2 s in 4 of 5 boots without the mitigation, and never
with it, in either slave or DMA mode.

Mitigation in TinyUSB: ``hcd_dwc2`` lets one preamble channel own the bus at a time and starts the
next one only in a frame after the previous one halted, which puts an SOF between them. A port
disable re-enumerates the root port.

STMicroelectronics STM32 OTG_FS/OTG_HS
--------------------------------------
**Severity: High** for a full-speed host with a low-speed device behind a hub

Reference: "Host packet transmission may hang when connecting through a hub to a low-speed device",
listed in ST's errata sheets for STM32F20x (ES0005), F401 (ES0222), F405/407 (ES0182), F411 (ES0287),
F412 (ES0305), F427/437 (ES0206), F76x/77x (ES0334), H742/743/750/753 (ES0392), H745/747/755/757
(ES0445), H723/725/730/733/735 (ES0491), L47x/48x (ES0250), L496/4A6 (ES0335), L4P5/Q5 (ES0510) and
U575/585 (ES0499). On parts with an OTG_HS core it applies to its full-speed interface.

ST: the transmitter internal state machine may hang, which leads, after a timeout expiry, to a port
disconnect interrupt. There is no workaround; more capacitance on the data lines may reduce the
occurrence.

On an STM32F407 the hang hit even a PRE sent right after an SOF, and starting one low-speed
transaction per frame did not help (9 of 10 boots still lost the port), so the ESP32-S2/S3
mitigation is not enabled for STM32. TinyUSB re-enumerates the root port after a port disable. A
high-speed host with a high-speed hub avoids preambles altogether (the hub's transaction translator
talks low speed).
