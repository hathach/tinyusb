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
  It must be a higher *preemption* level than every other interrupt, the system tick included: an
  interrupt at the same level runs to completion first. On CH32V30x the startup enables nesting with
  two preemption bits (IPRIOR bits 7:6), and the BSP sets SysTick to ``0x80`` below USBHS at ``0``.
- Force full speed (``dev_init.speed = TUSB_SPEED_FULL`` in ``tusb_init()``, see
  :ref:`item 2 <wch-usbhs-interrupt-starvation>`) to give software more time to process a transfer
  before the next one completes.
- Keep the configuration simple, e.g. a single isochronous endpoint, to shorten the processing
  time of each transfer.
- Run ``tud_task()`` often enough to handle each isochronous transfer within one (micro)frame:
  125 us at high speed, 1 ms at full speed. On USBHS a later completion can find its OUT packet
  overwritten by the next one, since isochronous OUT cannot be NAKed.

On CH32V30x the USBHS device driver reads ``INT_ST`` and ``RX_LEN`` in the first instructions of its
interrupt entry (see :ref:`item 2 <wch-usbhs-interrupt-starvation>`). This narrows the window but
does not close it: a completion can still be overwritten while interrupts are disabled or a higher
priority interrupt runs. The window can be very short. With Windows 10 polling three interrupt IN
endpoints and then a UAC2 headset's isochronous OUT and IN in each microframe, the IN token came
2 us after the last interrupt endpoint's ACK and overwrote a speaker packet's status unless the
interrupt for that ACK had already returned. One speaker packet in about 12,000 was lost while the
tick interrupt could delay the USB one; with the tick below USBHS and the shorter event handler, none
of 2.4 million packets were lost over 5 minutes.

.. _wch-usbhs-interrupt-starvation:

**2. USBHS interrupt endpoint starvation**

The USBHS driver's ``INT_BUSY`` protection makes endpoints respond with NAK while a transfer
interrupt is pending. This mainly affects interrupt IN endpoints: closely spaced transactions
on different endpoints can reduce throughput or starve an interrupt endpoint, even when its buffer
is ready. Disabling ``INT_BUSY`` is not a safe workaround: an OUT endpoint can accept another packet
before software processes the previous packet and updates its buffer and response.

The window is set by how fast one interrupt is finished and the next one taken: the next
endpoint's transaction is NAKed until the ISR of the previous completion has returned and the next
ISR has cleared its flag. A host polls consecutive interrupt endpoints back to back; Windows 10 on an
xHCI controller sent them about 2 us apart at HS. An endpoint that always follows two completions
starves completely when that path takes longer, and gets every poll when it is shorter, so small
changes in ISR time decide between the two.

The CH32V30x USBHS device driver keeps that path short:

- Its interrupt entry ``dcd_int_handler_fast()`` reads the status registers, NAKs a non-isochronous
  endpoint and clears the transfer flag before any C code runs. The CH32V30x and CH32F20x BSPs vector
  USBHS there; EP0, reset and suspend take the C path. An application keeps ``tud_int_handler(0)``
  working in an ordinary handler, or gets the fast entry by naming ``dcd_int_handler_fast`` as the
  USBHS vector (CH32V30x: with hardware prologue stacking enabled, as WCH's startup does, and
  optionally as a PFIC fast vector); the driver's "Application integration" comment has the details.
- Without an RTOS, the device task reads its event queue without masking the USB interrupt, and the
  ISR queues a completion as a few words, so the task never holds off the next interrupt.

On NanoCH32V305 with Windows 10 at HS, three 20-byte interrupt IN endpoints polled every 125 us
completed 16,000 reports/s on master, the first endpoint almost none; with the fast entry they
complete 24,000 reports/s, every poll. With a UAC2 headset streaming 48 kHz 16-bit stereo in both
directions next to them, the audio stream stalled on master (a lost isochronous IN completion leaves
the microphone endpoint unarmed); it now runs, and over 5 minutes the interrupt endpoints kept every
poll and none of 2.4 million speaker packets were lost. This depends on the host's polling pattern
and on the application's own interrupt load: heavier traffic can still starve an endpoint or lose
isochronous data as in :ref:`item 1 <wch-iso-status-overwrite>`. RTOS builds get only the interrupt
entry; the other WCH controllers get only the no-RTOS queue.

If an application still sees starvation, force full speed on the USBHS controller when its bandwidth
permits it:

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
time the bus NAKs. USBFS clears the flag at the end of its ISR, and SystemView records the ISR entry
inside that window at every ``SYSVIEW`` level; so does an ordinary handler calling
``tud_int_handler(0)`` on USBHS. USBHS's own vector clears the flag before recording for endpoints
other than EP0, so there tracing delays only the completion work and re-arming.
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
and its fix in RP2350 (``LINESTATE_TUNING.SIE_RX_BITSTUFF_FIX``, set at reset), which is not affected.

.. _RP2040 Datasheet: https://datasheets.raspberrypi.com/rp2040/rp2040-datasheet.pdf
.. _RP2350 Datasheet: https://datasheets.raspberrypi.com/rp2350/rp2350-datasheet.pdf

When a hub cuts off a token on the device's port with a bit-stuffing error, the device controller locks up and answers
nothing, SETUP included, until a bus reset; only ``SIE_STATUS.BIT_STUFF_ERROR`` shows it. Besides the VL805 case of the
erratum, a Genesys 05e3:0610 hub does this even in multi-TT mode: when a full-speed device on any of its ports returns
STALL, the host's Clear_TT_Buffer request can cut off the RP2040's transaction in progress. A WCH 1a86:8091 hub was not
affected.

The hub then answers STALL for the locked device, so recovery depends on the host's class driver. On Linux,
``usb-storage`` resets the port and the device re-enumerates; ``snd-usb-audio`` never does, so an audio device stays
dead until replugged; ``cdc_acm`` keeps clearing the TT buffer, about twice per millisecond, which in turn keeps cutting
off the hub's other full-speed devices. Toggling ``MAIN_CTRL.CONTROLLER_EN`` or ``SIE_CTRL.TRANSCEIVER_PD`` does not
recover the controller, and TinyUSB's E15 workaround (no bulk IN armed late in a frame) does not cover this case.

Mitigation in TinyUSB: by default in a build without high-speed support, on every MCU, the MSC device class pads a short
Data-In reply with zeros instead of stalling (``CFG_TUD_MSC_PAD_SHORT_REPLY``, 0 to stall): a reply to a command other
than READ10 when the host's length is at most ``CFG_TUD_MSC_EP_BUFSIZE``, and a short READ10 chunk that is not a
multiple of the packet size. This removes the frequent STALLs of the disk probe and the kernel's periodic MODE SENSE: an
RP2040 next to an STM32 mass storage device locked up in none of 60 disk probes, against 38 of 100 without padding. Any
other STALL on the hub can still trigger it.

Workarounds:

- Connect the RP2040 to a root port, or to a hub that is not affected.
- Detect the lockup and restart the stack. Count it as a lockup only when ``SOF_RD`` stays unchanged for a few
  hundred milliseconds while ``tud_connected()`` is true and ``tud_suspended()`` false: a bus reset also stops
  SOFs. Restart from the task running ``tud_task()`` (``tud_task_stop()`` makes it return true): ``tud_deinit()``,
  at least 2 ms with the pull-up off so the hub sees the disconnect, then ``tusb_init()``. Raspberry Pi's
  debugprobe firmware restarts from a watchdog task instead, which TinyUSB does not support under an RTOS.
  ``tud_disconnect()`` and ``tud_connect()`` only switch the D+ pull-up and do not reset the controller.

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

**IN endpoints beyond the five TX FIFOs (device).** The ESP32-S2/S3 TRMs allow five IN endpoints
active at once, EP0 included, each with its own TX FIFO. ``dcd_dwc2`` opens an interrupt IN endpoint
numbered past the FIFOs (arduino-esp32's never-sent CDC notification ``0x85``) with ``USBActEP`` set
and no FIFO, and refuses its transfers, so ``EPEna`` is never set. Such an endpoint answers the
host's polls with NAK; the TRMs do not define "active" at the ``USBActEP`` level. Leaving it inactive
instead is worse: the core then ignores IN tokens, and every poll ends in a host transaction error
(measured on an STM32U5A5 OTG_HS limited to 4 TX FIFOs, Windows ``usbser`` polling: NAK with
``USBActEP`` set, no handshake without). An ESP32-S3 with ``0x85`` open this way ran byte-exact CDC
loopback on both ports in slave and DMA mode.

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

STMicroelectronics STM32F2/F4 OTG_FS host channel halt
------------------------------------------------------
**Severity: Low** for a host that aborts transfers

Reference: "Host channel-halted interrupt not generated when the channel is disabled", section 2.14.5
of ES0005 (STM32F20x/21x), ES0182 (F405/407/415/417) and ES0206 (F427/437/429/439).

ST: when the application enables and then immediately disables a host channel before the OTG_FS core
has begun the transfer sequence, no channel-halted interrupt is raised; the core otherwise operates
normally. The workaround is not to disable a channel right after enabling it.

``hcd_dwc2`` aborts a transfer by disabling its channel and retires the channel, and lets the
endpoint take a new transfer, only on that channel-halted interrupt: neither the reference manual
nor the databook supports treating a channel as disabled without it. OTG_FS has no DMA, so the
host runs in slave mode, which re-enables a NAKed control or bulk IN after every NAK: a
``tuh_edpt_abort_xfer()`` that lands right after such a re-enable can miss the halt. The abort then
returns after its bounded wait and the endpoint refuses new transfers until the device is
unplugged. Not observed on hardware (no F2/F4 host on the test rigs).

Raspberry Pi RP2040/RP2350
--------------------------
**Severity: Medium** for a host with several devices, especially behind a hub

Reference: `RP2040 Datasheet`_ section 4.1.2.9 (host transfers) and the ``NAK_POLL`` register; no
erratum covers the items below. hathach/tinyusb#3964, #4458.

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
