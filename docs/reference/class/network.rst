***********
USB Network
***********

Role: device only.  TinyUSB can present an Ethernet-style interface using
CDC-ECM, RNDIS, or CDC-NCM.  The application connects Ethernet frames to a
network stack such as lwIP.

Choose one driver
=================

``CFG_TUD_ECM_RNDIS`` and ``CFG_TUD_NCM`` are mutually exclusive.

* The ECM/RNDIS driver can expose separate configurations so Windows selects
  RNDIS and macOS selects ECM; Linux can use either.
* NCM aggregates Ethernet datagrams into Network Transfer Blocks and is the
  preferred starting point for current, higher-throughput designs.  Windows
  binding may require the Microsoft OS 2.0 descriptors shown by the example.

Use the matching descriptor macro: ``TUD_CDC_ECM_DESCRIPTOR``,
``TUD_RNDIS_DESCRIPTOR``, or ``TUD_CDC_NCM_DESCRIPTOR``.  Provide a unique
48-bit ``tud_network_mac_address`` and return the same address as a 12-digit
hexadecimal USB string descriptor where the class descriptor references it.
The host uses this address for its end of the link, so give the device's own
network stack a different MAC; the example toggles its least significant bit.

Configuration options
=====================

.. list-table::
   :header-rows: 1
   :widths: 42 16 42

   * - Option
     - Default
     - What it controls
   * - ``CFG_TUD_ECM_RNDIS`` / ``CFG_TUD_NCM``
     - ``0``
     - Selects one network class implementation.  Enabling both is a build
       error.
   * - ``CFG_TUD_NET_MTU``
     - ``1514`` bytes
     - Maximum Ethernet frame including its 14-byte Ethernet header.
   * - ``CFG_TUD_NCM_OUT_NTB_MAX_SIZE``
     - ``3200`` bytes
     - Largest host-to-device NTB received.  At least 2048 bytes (see
       `NCM sizing`_).
   * - ``CFG_TUD_NCM_IN_NTB_MAX_SIZE``
     - ``3200`` bytes
     - Largest device-to-host NTB assembled for transmission.  At least 2048
       bytes (see `NCM sizing`_).
   * - ``CFG_TUD_NCM_OUT_NTB_N`` / ``CFG_TUD_NCM_IN_NTB_N``
     - ``1`` each
     - Number of receive/transmit NTB buffers.  Increasing these can reduce
       stalls at a proportional RAM cost; benchmark before changing them.
   * - ``CFG_TUD_NCM_IN_MAX_DATAGRAMS_PER_NTB``
     - ``8``
     - Maximum Ethernet frames TinyUSB aggregates into a transmit NTB.
   * - ``CFG_TUD_NCM_OUT_MAX_DATAGRAMS_PER_NTB``
     - ``6``
     - Maximum frames the device tells the host to place in one receive NTB.
   * - ``CFG_TUD_NCM_DEFAULT_LINK_UP``
     - Undefined (link up)
     - Initial NCM link state returned by the default
       ``tud_network_default_link_state_cb()``.

Frame flow
==========

For host-to-device frames, TinyUSB calls
``tud_network_recv_cb(src, size)``.  Return ``true`` once the frame is accepted,
then call ``tud_network_recv_renew()`` to receive the next one.  ECM/RNDIS keeps
``src`` valid until that call, but NCM may reuse the storage as soon as the
callback returns, so copy the frame inside the callback.  On ``false``,
ECM/RNDIS drops the frame and renews reception itself; NCM keeps the frame and
offers it again on the next ``tud_network_recv_renew()``, which the application
must call.

For device-to-host frames:

1. Call ``tud_network_can_xmit(size)``.
2. If it returns true, call ``tud_network_xmit(ref, arg)`` once.
3. TinyUSB calls ``tud_network_xmit_cb(dst, ref, arg)``; copy the complete
   Ethernet frame into ``dst`` and return its length.

The drivers do not lock their state: call ``tud_network_recv_renew()``,
``tud_network_can_xmit()`` and ``tud_network_xmit()`` from the task that runs
``tud_task()``.

Use ``tud_network_link_state()`` to notify the host when the logical or physical
link changes.  A mounted USB device is not necessarily a link-up network
interface.

.. list-table::
   :header-rows: 1
   :widths: 40 60

   * - API or callback
     - What it does
   * - ``tud_network_recv_cb()``
     - Offers one received Ethernet frame.  Return ``true`` once it is
       accepted; see above for how long ``src`` stays valid.
   * - ``tud_network_recv_renew()``
     - Lets the driver deliver the next frame (re-arms reception, or offers the
       next NCM datagram).
   * - ``tud_network_can_xmit()`` / ``tud_network_xmit()``
     - Checks whether one frame of ``size`` bytes can be queued now, then
       queues it.  Call ``xmit`` once after each successful check.
   * - ``tud_network_xmit_cb()``
     - Copies the complete frame into TinyUSB's destination and returns its
       actual byte length.
   * - ``tud_network_init_cb()``
     - Not called by either driver.
   * - ``tud_network_set_packet_filter_cb()``
     - Reports ECM and NCM host filter bits so the application can adjust
       multicast or promiscuous delivery.
   * - ``tud_network_default_link_state_cb()`` /
       ``tud_network_link_state()``
     - Supplies the initial NCM link state and later sends link up/down changes
       to the host (ECM and NCM only; ignored in RNDIS mode).

NCM sizing
==========

Keep ``CFG_TUD_NCM_IN_NTB_MAX_SIZE`` and ``CFG_TUD_NCM_OUT_NTB_MAX_SIZE`` at
2048 bytes or more: the class requires the host to select an IN NTB size of at
least 2048 bytes and no more than the device's maximum, and Linux expects an OUT
NTB size of at least 2048 bytes.  NCM buffer sizes have a direct RAM/throughput
tradeoff.  Begin with one IN and one OUT NTB, then measure before increasing
the NTB sizes or their ``*_NTB_N`` counts.  Keep descriptor capabilities and
runtime responses consistent with the enabled NCM features.

The :doc:`../../examples/device/net_lwip_webserver` example includes NCM and
ECM/RNDIS descriptor sets, lwIP integration, DHCP, DNS, link-state changes, and
host setup notes.

Specifications used: *CDC Ethernet Control Model*, Revision 1.2, and *CDC
Network Control Model*, Revision 1.0 (Errata 1).  RNDIS is a vendor protocol,
not a USB-IF CDC subclass.
