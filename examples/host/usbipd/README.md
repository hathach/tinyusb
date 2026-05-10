# USB/IP-over-Ethernet host bridge

A TinyUSB host that forwards URBs from a USB device on its OTG host
port over TCP/IP to a Linux `usbip` client. Once attached, the
device appears in the Linux kernel's USB bus the same way a locally
plugged device would: cdc-acm becomes `/dev/ttyACM<N>`, HID becomes
an evdev node, and so on.

The example targets STM32F429ZI / F439ZI Nucleo-144 with the
on-board LAN8742A PHY. The TinyUSB host stack and the lwIP raw API
both run in the main loop (`tuh_task()` + `sys_check_timeouts()`),
no RTOS.

## Hardware

* STM32F429ZI Nucleo-144 (or F439ZI - same part bar the crypto
  block, not used here).
* USB device under test plugged into the OTG_FS host port (CN13
  USER USB on the Nucleo-144). VBUS enabled by the BSP via PG6 in
  `board_init_after_tusb()`.
* Ethernet via the on-board LAN8742A PHY connected by RMII.
* STLink VCOM for the diagnostic UART (USART3 on PD8/PD9).

## Build

```sh
cd examples/host/usbipd
mkdir build && cd build
cmake -G Ninja -DBOARD=stm32f439nucleo ..
ninja
```

Flash with probe-rs:

```sh
probe-rs run --probe 0483:374b:<serial> --chip STM32F439ZITx \
    usbipd.elf
```

On boot the UART prints the DHCP-assigned address:

```
usbipd example: host + lwip + dhcp + usbip
tusb_init ok
MOUNT: daddr=1 rhport=0 speed=FS vid=0x2e8a pid=0x000c
eth: up, starting dhcp
dhcp: got 192.168.0.182
usbip: listening on tcp/3240
```

## Attaching from a Linux host

```sh
$ usbip list -r 192.168.0.182
Exportable USB devices
======================
 - 192.168.0.182
        1-1: ... (2e8a:000c)

$ sudo modprobe vhci_hcd                 # if not already loaded
$ sudo usbip attach -r 192.168.0.182 -b 1-1
$ dmesg | tail
... usb 5-1: new full-speed USB device number 6 using vhci_hcd
... cdc_acm 5-1:1.1: ttyACM12: USB ACM device

$ sudo bash -c 'echo 0 > /sys/devices/platform/vhci_hcd.0/detach'
```

`usbip attach` exits silently on success, `dmesg` is the
authoritative confirmation. The classic `usbip detach -p 0` works
when the userspace utilities can find their state file; for
scripting the sysfs write above is robust.

To bind the listener to a specific interface instead of
`INADDR_ANY`, build with `-DUSBIPD_BIND_ADDR=...`. The macro is
passed to lwIP's `tcp_bind`, so any `ip_addr_t` initialiser works
(e.g. `IPADDR4_INIT_BYTES(192,168,0,182)`).

## How it works

The device runs three things concurrently on the main thread:
`tuh_task()` drives the TinyUSB host stack, `ethernetif_input()`
polls HAL_ETH and feeds frames to lwIP, `sys_check_timeouts()`
services lwIP's timers.

The USB/IP server in `usbip_server.c` is a single-client state
machine on TCP/3240:

1. `OP_REQ_DEVLIST` -> walk `tuh_descriptor_get_device_local`,
   emit a `usbip_device_desc` per mounted device.
2. `OP_REQ_IMPORT` -> match busid, emit `OP_REP_IMPORT` + dev
   desc. Kernel takes the socket from this point.
3. `CMD_SUBMIT` (control) -> `tuh_control_xfer`. Setup packet
   copied into the inflight slot so its lifetime spans the async
   completion.
4. `CMD_SUBMIT` (bulk/interrupt) -> `tuh_edpt_xfer`. Endpoints
   open lazily via `tuh_edpt_open`, the descriptor sniffed from
   the kernel's `GET_DESCRIPTOR(CONFIGURATION)` reply that flows
   through us (avoids a sync descriptor fetch from inside the
   lwIP recv path).
5. `CMD_UNLINK` -> `tuh_edpt_abort_xfer` fires the natural
   completion callback which sends `RET_SUBMIT(FAILED)` for the
   in-flight URB and frees the EP claim. `RET_UNLINK` reports
   status=0 to acknowledge.

Per-EP submit queue: cdc-acm queues 16 read-ahead URBs on its
bulk-IN but TinyUSB only allows one URB per EP in flight. URBs
that come back rejected from `tuh_edpt_xfer` are kept in the
inflight pool flagged `queued` and submitted in FIFO order from
the completion callback when the EP frees.

## Notes / known limits

* lwIP's `LWIP_IPV4` only, the kernel's `usbip` client speaks
  IPv4 for now too.
* Single client at a time. A second TCP connect while one is
  attached gets dropped on accept.
* Endpoints are not closed on detach. Re-attaching the same
  device tolerates this because `ensure_ep_open` is idempotent
  against the open bit.
* This is a *host* example, the F4 itself does not present a USB
  interface to the upstream port (no `usb_descriptors.c`).
* The example refuses USB `SET_ADDRESS` proxied from the wire
  (would silently desync TinyUSB's view of the device).
* When `usbip attach` is interrupted before `vhci_hcd` finishes
  setting up the device node, a stray write to `/dev/ttyACM<N>`
  (e.g. `echo > /dev/ttyACM12`) creates a regular file at that
  path. The next attach can't make the character device because
  the inode is taken. Symptom: pyserial open returns
  "Inappropriate ioctl for device". Cure: `sudo rm
  /dev/ttyACM<N>` before re-attaching. Generic cdc_acm/devtmpfs
  interaction, not specific to the bridge.

The lwIP netif driver, the LAN8742 PHY driver and the per-board
RMII pin map live in the F4 BSP under `hw/bsp/stm32f4/eth_lwip/`,
`hw/mcu/st/stm32_lan8742/` and `hw/bsp/stm32f4/boards/<board>/board_eth.c`,
pulled in by `family_add_eth()` from the example's CMakeLists.
