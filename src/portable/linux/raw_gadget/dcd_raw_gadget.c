/*
 * SPDX-FileCopyrightText: Copyright (c) 2018, Ha Thach (tinyusb.org)
 * SPDX-FileCopyrightText: Copyright (c) 2026 Roman Buchert
 * SPDX-License-Identifier: MIT
 *
 * This file is part of the TinyUSB stack.
 */

#include "tusb_option.h"

#if CFG_TUD_ENABLED && CFG_TUSB_MCU == OPT_MCU_LINUX_RAW_GADGET

#include "device/dcd.h"
#include "raw_gadget_hal.h"

//--------------------------------------------------------------------+
// Internal Helpers
//--------------------------------------------------------------------+

static raw_gadget_handle_t raw_gadget_handle_from_rhport(uint8_t rhport)
{
  return (raw_gadget_handle_t) rhport;
}

static raw_gadget_speed_t raw_gadget_speed_from_tusb(tusb_speed_t speed)
{
  switch (speed)
  {
    case TUSB_SPEED_LOW:
      return RAW_GADGET_SPEED_LOW;

    case TUSB_SPEED_FULL:
      return RAW_GADGET_SPEED_FULL;

    case TUSB_SPEED_HIGH:
    case TUSB_SPEED_AUTO:
      return RAW_GADGET_SPEED_HIGH;

    case TUSB_SPEED_INVALID:
    default:
      return RAW_GADGET_SPEED_INVALID;
  }
}

static tusb_speed_t raw_gadget_speed_to_tusb(raw_gadget_speed_t speed)
{
  switch (speed)
  {
    case RAW_GADGET_SPEED_LOW:
      return TUSB_SPEED_LOW;

    case RAW_GADGET_SPEED_FULL:
      return TUSB_SPEED_FULL;

    case RAW_GADGET_SPEED_HIGH:
      return TUSB_SPEED_HIGH;

    case RAW_GADGET_SPEED_INVALID:
    default:
      return TUSB_SPEED_INVALID;
  }
}

static xfer_result_t raw_gadget_transfer_result_to_tusb(raw_gadget_transfer_result_t result)
{
  switch (result)
  {
    case RAW_GADGET_TRANSFER_RESULT_SUCCESS:
      return XFER_RESULT_SUCCESS;

    case RAW_GADGET_TRANSFER_RESULT_STALLED:
      return XFER_RESULT_STALLED;

    case RAW_GADGET_TRANSFER_RESULT_CANCELLED:
    case RAW_GADGET_TRANSFER_RESULT_FAILED:
    default:
      return XFER_RESULT_FAILED;
  }
}

static void raw_gadget_event_callback(raw_gadget_handle_t handle,
                                      raw_gadget_event_t const *event)
{
  uint8_t const rhport = (uint8_t) handle;

  if (event == NULL)
  {
    return;
  }

  switch (event->type)
  {
    case RAW_GADGET_EVENT_RESET:
      dcd_event_bus_reset(rhport, raw_gadget_speed_to_tusb(event->data.reset.speed), false);
      break;

    case RAW_GADGET_EVENT_SUSPEND:
      dcd_event_bus_signal(rhport, DCD_EVENT_SUSPEND, false);
      break;

    case RAW_GADGET_EVENT_RESUME:
      dcd_event_bus_signal(rhport, DCD_EVENT_RESUME, false);
      break;

    case RAW_GADGET_EVENT_DISCONNECT:
      dcd_event_bus_signal(rhport, DCD_EVENT_UNPLUGGED, false);
      break;

    case RAW_GADGET_EVENT_SETUP_RECEIVED:
      dcd_event_setup_received(rhport, event->data.setup.data, false);
      break;

    case RAW_GADGET_EVENT_TRANSFER_COMPLETE:
      dcd_event_xfer_complete(
          rhport, event->data.transfer.endpoint_address,
          (uint32_t) event->data.transfer.transferred_bytes,
          (uint8_t) raw_gadget_transfer_result_to_tusb(event->data.transfer.result), false);
      break;

    case RAW_GADGET_EVENT_CONNECT:
    default:
      break;
  }
}

//--------------------------------------------------------------------+
// Device API
//--------------------------------------------------------------------+

bool dcd_init(uint8_t rhport, tusb_rhport_init_t const *rh_init)
{
  raw_gadget_speed_t speed;

  if (rh_init == NULL)
  {
    return false;
  }

  speed = raw_gadget_speed_from_tusb(rh_init->speed);
  if (speed == RAW_GADGET_SPEED_INVALID)
  {
    return false;
  }

  return raw_gadget_init(raw_gadget_handle_from_rhport(rhport), speed,
                         raw_gadget_event_callback) == RAW_GADGET_RESULT_SUCCESS;
}

bool dcd_deinit(uint8_t rhport)
{
  return raw_gadget_deinit(raw_gadget_handle_from_rhport(rhport)) ==
         RAW_GADGET_RESULT_SUCCESS;
}

/* Raw Gadget is serviced by worker threads rather than a hardware ISR. */
void dcd_int_handler(uint8_t rhport)
{
  (void) rhport;
}

void dcd_int_enable(uint8_t rhport)
{
  (void) rhport;
}

void dcd_int_disable(uint8_t rhport)
{
  (void) rhport;
}

void dcd_set_address(uint8_t rhport, uint8_t dev_addr)
{
  (void) dev_addr;

  /* Raw Gadget handles the address internally after the status stage. */
  (void) dcd_edpt_xfer(rhport, 0x80u, NULL, 0u, false);
}

void dcd_remote_wakeup(uint8_t rhport)
{
  /* The Raw Gadget userspace API does not expose remote wakeup. */
  (void) rhport;
}

void dcd_connect(uint8_t rhport)
{
  (void) raw_gadget_connect(raw_gadget_handle_from_rhport(rhport));
}

void dcd_disconnect(uint8_t rhport)
{
  /* Raw Gadget has no reversible software-disconnect operation. */
  (void) raw_gadget_disconnect(raw_gadget_handle_from_rhport(rhport));
}

void dcd_sof_enable(uint8_t rhport, bool enabled)
{
  /* Raw Gadget does not expose SOF events. */
  (void) rhport;
  (void) enabled;
}

//--------------------------------------------------------------------+
// Endpoint API
//--------------------------------------------------------------------+

bool dcd_edpt_open(uint8_t rhport, tusb_desc_endpoint_t const *ep_desc)
{
  raw_gadget_handle_t const handle = raw_gadget_handle_from_rhport(rhport);
  raw_gadget_result_t result;

  if ((ep_desc == NULL) || (ep_desc->bLength < sizeof(*ep_desc)))
  {
    return false;
  }

  /* USB_RAW_IOCTL_CONFIGURE must precede the first endpoint enable. */
  result = raw_gadget_configure(handle);
  if (result != RAW_GADGET_RESULT_SUCCESS)
  {
    return false;
  }

  result = raw_gadget_endpoint_open(handle, (uint8_t const *) ep_desc, ep_desc->bLength);
  return result == RAW_GADGET_RESULT_SUCCESS;
}

bool dcd_edpt_iso_alloc(uint8_t rhport, uint8_t ep_addr, uint16_t largest_packet_size)
{
  (void) rhport;
  (void) ep_addr;
  (void) largest_packet_size;

  /* Linux Raw Gadget does not provide complete isochronous support. */
  return false;
}

bool dcd_edpt_iso_activate(uint8_t rhport, tusb_desc_endpoint_t const *ep_desc)
{
  (void) rhport;
  (void) ep_desc;

  return false;
}

void dcd_edpt_close_all(uint8_t rhport)
{
  (void) raw_gadget_endpoint_close_all(raw_gadget_handle_from_rhport(rhport));
}

bool dcd_edpt_xfer(uint8_t rhport, uint8_t ep_addr, uint8_t *buffer,
                   uint16_t total_bytes, bool is_isr)
{
  (void) is_isr;

  return raw_gadget_endpoint_transfer(raw_gadget_handle_from_rhport(rhport), ep_addr, buffer,
                                      total_bytes) == RAW_GADGET_RESULT_SUCCESS;
}

bool dcd_edpt_xfer_fifo(uint8_t rhport, uint8_t ep_addr, tu_fifo_t *fifo,
                        uint16_t total_bytes, bool is_isr)
{
  (void) rhport;
  (void) ep_addr;
  (void) fifo;
  (void) total_bytes;
  (void) is_isr;

  return false;
}

void dcd_edpt_stall(uint8_t rhport, uint8_t ep_addr)
{
  (void) raw_gadget_endpoint_stall(raw_gadget_handle_from_rhport(rhport), ep_addr);
}

void dcd_edpt_clear_stall(uint8_t rhport, uint8_t ep_addr)
{
  (void) raw_gadget_endpoint_clear_stall(raw_gadget_handle_from_rhport(rhport), ep_addr);
}

#endif
