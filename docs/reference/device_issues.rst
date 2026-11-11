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
causing data corruption. Other transfer types are not affected.

Partial workaround:

- Give the USB interrupt the highest priority allowed for an ISR that calls the RTOS (e.g.
  ``configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY`` with FreeRTOS), or the highest priority without an RTOS.
- Force full speed (``dev_init.speed = TUSB_SPEED_FULL`` in ``tusb_init()``, see
  :ref:`item 2 <wch-usbhs-interrupt-starvation>`) to give software more time to process a transfer
  before the next one completes.
- Keep the configuration simple, e.g. a single isochronous endpoint, to shorten the processing
  time of each transfer.
- Run ``tud_task()`` often enough to handle each isochronous transfer within one (micro)frame:
  125 us at high speed, 1 ms at full speed. On USBHS a later completion can find its OUT packet
  overwritten by the next one, since isochronous OUT cannot be NAKed.

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

Raspberry Pi RP2040/RP2350
--------------------------
**Severity: Medium** for a host with several devices, especially behind a hub

Reference: `RP2040 Datasheet`_ section 4.1.2.9 (host transfers) and the ``NAK_POLL`` register; no
erratum covers the items below. hathach/tinyusb#3964, #4458.

.. _RP2040 Datasheet: https://datasheets.raspberrypi.com/rp2040/rp2040-datasheet.pdf

Affects the host controller. It runs transfers through one endpoint buffer pair, EPX, plus hardware
pollers for interrupt endpoints, and raises no interrupt on a NAK. RP2040 repeats a NAKed token after
each ``NAK_POLL`` delay until the device answers or software writes ``STOP_TRANS``; RP2350 can also
stop EPX at a NAK (``NAK_POLL.STOP_EPX_ON_NAK``), which TinyUSB uses there.

**1. The hardware interrupt poller corrupts EPX (both)**

When a hardware poll of an interrupt endpoint times out, such as on a device unplugged behind a hub
before the hub reports it, the controller writes a zero-length FULL|LAST status into the EPX buffer
of the transfer in progress, or makes its next packet fail the DATA PID check after the host has
ACKed it. Either way that packet is lost. On RP2040 a polled interrupt OUT does the same, and the
timeout only showed with low-speed devices.

Mitigation in TinyUSB: the hardware pollers are not used. Interrupt endpoints are polled on EPX from
the SOF interrupt at their interval, and waiting transfers take EPX in turn.

**2. STOP_TRANS on a transaction under way loses its packet (RP2040)**

``STOP_TRANS`` written while a retry is on the wire does not abort it. The host still ACKs the
packet the device returns but writes no buffer status, so the packet is lost; the device, having
seen the ACK, sends its next packet with the other DATA PID, a DATA_SEQ error. Software cannot see
a retry that has started but not yet reached the bus, so checking that the bus is idle before
``STOP_TRANS`` does not prevent it.

Mitigation in TinyUSB: the SOF round-robin stops a full-speed IN that has not progressed for two
frames right after one of its NAKs (``SIE_STATUS.NAK_REC``), while the controller waits out the
``NAK_POLL`` delay. That delay is 8 us for such a transfer while others wait for EPX, so a retry and
its NAK come within the 15 us the SOF interrupt waits for one; without one the next SOF tries again.
A low-speed transaction, or a full-speed OUT with data, takes longer than that window and cannot
start before SOF, so those transfers are stopped after checking that the bus is idle, with a 300 us
``NAK_POLL`` delay. An OUT cut that way is sent again with the same DATA PID, which the device drops
as a repeat; a low-speed IN can still lose a packet. A bulk transfer that gives way to a due
interrupt poll is stopped only with both buffers unarmed and the bus idle (waiting at most 10 us),
so its ``STOP_TRANS`` cannot meet the host's ACK of its last packet either. Each look for the NAK,
or the bus check, and the ``STOP_TRANS`` after it run with interrupts masked (under a microsecond,
or 3 us for the bus check), and a NAK seen after a higher-priority interrupt held the wait is not
trusted: the next one is waited for. An aborted OUT whose retry a stop meets and the device ACKs
(``SIE_STATUS.ACK_REC`` with no buffer status) keeps that packet's DATA PID, so the first packet of
the transfer queued behind the abort is not dropped as a repeat.

On a Raspberry Pi Pico host with a hub, a flash drive and a CDC device replugged every 1.5 s, a stop
after a bus-idle check (with a 300 us ``NAK_POLL`` delay) lost 2 packets in 12 boots; the stop after
a NAK lost none in 16 boots of about 3,200 switches each.

**3. An interrupt OUT the device NAKs holds EPX (RP2040)**

An interrupt IN poll on EPX ends at its NAK. An interrupt OUT does not: it is retried like a bulk
OUT until the round-robin switches it out, two SOFs later, and then waits its turn again. While
interrupt OUT endpoints keep NAKing, a bulk transfer beside them gets about one packet per poll of
each (about 64/N KB/s with N such endpoints). Control transfers still run within a few
milliseconds. On RP2350 a NAK ends the poll.

**4. START_TRANS after a NAK stop resumes on buffer 0 (RP2350)**

A transfer stopped at a NAK and restarted by ``START_TRANS`` alone resumes on buffer 0, not on the
buffer it stopped at. A double-buffered IN then expects the other DATA PID (a DATA_SEQ error, its
packet lost) and an OUT sends its buffers out of order.

Mitigation in TinyUSB: a transfer stopped at a NAK is saved and started afresh, as when EPX switches
to another transfer.
