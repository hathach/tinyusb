# Linux Raw Gadget device controller driver

This directory implements a TinyUSB device controller driver (DCD) for the
Linux Raw Gadget userspace API. It allows TinyUSB device examples and tests to
run as software USB devices through a Linux USB device controller, normally
`dummy_hcd`/`dummy_udc`.

## Architecture

The port consists of two layers:

- `dcd_raw_gadget.c` adapts the TinyUSB DCD API to the platform-neutral Raw
  Gadget HAL.
- `raw_gadget_*.c` implements the Linux Raw Gadget lifecycle, UDC discovery,
  endpoint management, event handling and asynchronous transfers.

Raw Gadget ioctls block while waiting for USB activity. The implementation
therefore uses one event worker and at most one transfer worker per endpoint
address. TinyUSB callbacks are serialized and are always delivered with
`in_isr == false`.

Endpoint zero requires special handling because TinyUSB and Raw Gadget expose
different control-transfer semantics:

- TinyUSB may submit an IN data stage in several endpoint-sized chunks, while
  Raw Gadget expects the complete EP0 IN data stage in one ioctl. The HAL
  aggregates these chunks before issuing `USB_RAW_IOCTL_EP0_WRITE`.
- Raw Gadget completes the status stage as part of data-bearing control
  requests. The subsequent logical zero-length status transfer submitted by
  TinyUSB is therefore completed synthetically.
- Requests without a data stage use Raw Gadget's delayed-status mechanism and
  require a real EP0 ioctl.

A transfer generation counter prevents completions from transfers cancelled by
a bus reset from reaching the new TinyUSB device state.

## Requirements

- Linux with `CONFIG_USB_RAW_GADGET` enabled
- `raw_gadget` kernel module
- A userspace-accessible `/dev/raw-gadget`
- A USB device controller visible in `/sys/class/udc`
- `dummy_hcd` when no physical UDC is used
- POSIX threads

The current UDC discovery maps TinyUSB root-hub port `n` to a UDC named
`dummy_udc.n`.

## Supported functionality

The port supports the transfer types provided reliably by the Linux Raw Gadget
interface:

- Control
- Bulk
- Interrupt

The implementation has been exercised with TinyUSB CDC ACM, including
enumeration, class control requests, interrupt endpoint configuration,
bidirectional bulk transfers, zero-length packets and a 256 KiB echo transfer.

## Limitations

### Isochronous transfers

Isochronous endpoints are intentionally not supported. Linux Raw Gadget does
not provide complete isochronous transfer support, so
`dcd_edpt_iso_alloc()` and `dcd_edpt_iso_activate()` return `false`. This is a
platform limitation, not an unimplemented transfer path in the DCD.

### Software disconnect and remote wakeup

Raw Gadget does not expose a reversible software-disconnect ioctl or a remote
wakeup operation. `dcd_disconnect()` and `dcd_remote_wakeup()` therefore have
no effect. Closing the Raw Gadget file descriptor disconnects and destroys the
instance during DCD deinitialization.

### Start-of-frame events

Raw Gadget does not expose SOF events. `dcd_sof_enable()` is a no-op.

### Endpoint FIFO API

The optional `dcd_edpt_xfer_fifo()` API is not implemented. TinyUSB uses the
normal buffer transfer API for this port.

## Threading and callback rules

The HAL owns all worker threads. Application code must not call the HAL
functions directly; TinyUSB accesses them through `dcd_raw_gadget.c`.

Raw Gadget event callbacks:

- may run on an implementation-owned worker thread;
- are serialized by the HAL;
- must not block indefinitely;
- must not retain pointers to event objects after returning.

## Security and permissions

Access to `/dev/raw-gadget` normally requires elevated privileges. Prefer a
narrow udev rule or dedicated group over running the complete test application
as root. The exact policy is distribution-specific and is intentionally not
installed by this port.
