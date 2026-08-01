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
     - Largest host-to-device NTB received.  Linux expects at least 2048 bytes.
   * - ``CFG_TUD_NCM_IN_NTB_MAX_SIZE``
     - ``3200`` bytes
     - Largest device-to-host NTB assembled for transmission.
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

Frame flow
==========

For host-to-device frames, TinyUSB calls
``tud_network_recv_cb(src, size)``.  Copy or pass the frame to the network stack
and call ``tud_network_recv_renew()`` when the supplied packet storage is no
longer needed.  Return ``false`` if the frame cannot be accepted.

For device-to-host frames:

1. Call ``tud_network_can_xmit(size)``.
2. If it returns true, call ``tud_network_xmit(ref, arg)`` once.
3. TinyUSB calls ``tud_network_xmit_cb(dst, ref, arg)``; copy the complete
   Ethernet frame into ``dst`` and return its length.

Use ``tud_network_link_state()`` to notify the host when the logical or physical
link changes.  A mounted USB device is not necessarily a link-up network
interface.

.. list-table::
   :header-rows: 1
   :widths: 40 60

   * - API or callback
     - What it does
   * - ``tud_network_recv_cb()``
     - Offers one received Ethernet frame.  Return ``true`` only if the
       application accepted the buffer or copied the frame.
   * - ``tud_network_recv_renew()``
     - Releases the offered receive storage and permits the next host packet.
   * - ``tud_network_can_xmit()`` / ``tud_network_xmit()``
     - Reserves room and requests one device-to-host frame.  Call ``xmit`` only
       once after a successful capacity check.
   * - ``tud_network_xmit_cb()``
     - Copies the complete frame into TinyUSB's destination and returns its
       actual byte length.
   * - ``tud_network_init_cb()``
     - Resets the application network state when the ECM/RNDIS driver is
       initialized or reset.
   * - ``tud_network_set_packet_filter_cb()``
     - Reports NCM host filter bits so the application can adjust multicast or
       promiscuous delivery.
   * - ``tud_network_default_link_state_cb()`` /
       ``tud_network_link_state()``
     - Supplies the initial NCM link state and later sends link up/down changes
       to the host.

NCM sizing
==========

NCM buffer sizes have a direct RAM/throughput tradeoff.  The class requires an
OUT NTB size of at least 2048 bytes.  Begin with one IN and one OUT NTB, then
measure before increasing ``CFG_TUD_NCM_IN_NTB_MAX_SIZE``,
``CFG_TUD_NCM_OUT_NTB_MAX_SIZE``, or their ``*_NTB_N`` counts.  Keep descriptor
capabilities and runtime responses consistent with the enabled NCM features.

The :doc:`../../examples/device/net_lwip_webserver` example includes NCM and
ECM/RNDIS descriptor sets, lwIP integration, DHCP, DNS, link-state changes, and
host setup notes.

Specifications used: *CDC Ethernet Control Model*, Revision 1.2, and *CDC
Network Control Model*, Revision 1.0 (Errata 1).  RNDIS is a vendor protocol,
not a USB-IF CDC subclass.
