/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2021 Ha Thach (tinyusb.org)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * This file is part of the TinyUSB stack.
 */

#include "tusb_option.h"

#if CFG_TUH_ENABLED && (CFG_TUSB_MCU == OPT_MCU_RP2040) && CFG_TUH_RPI_PIO_USB

#include "pico.h"
#include "hardware/pio.h"
#include "pio_usb.h"

//--------------------------------------------------------------------+
// INCLUDE
//--------------------------------------------------------------------+
#include "osal/osal.h"

#include "host/hcd.h"
#include "host/usbh.h"

#define RHPORT_OFFSET     1
#define RHPORT_PIO(_x)    ((_x)-RHPORT_OFFSET)

static pio_usb_configuration_t pio_host_config = PIO_USB_DEFAULT_CONFIG;

//--------------------------------------------------------------------+
// HCD API
//--------------------------------------------------------------------+
bool hcd_init(uint8_t rhport)
{
  // To run USB SOF interrupt in core1, create alarm pool in core1.
  pio_host_config.alarm_pool = (void*)alarm_pool_create(2, 1);
  (void) pio_usb_host_init(&pio_host_config);

  return true;
}

void hcd_port_reset(uint8_t rhport)
{
  rhport = RHPORT_PIO(rhport);
  pio_usb_hw_port_reset_start(rhport);
}

void hcd_port_reset_end(uint8_t rhport)
{
  rhport = RHPORT_PIO(rhport);
  pio_usb_hw_port_reset_end(rhport);
}

bool hcd_port_connect_status(uint8_t rhport)
{
  rhport = RHPORT_PIO(rhport);

  pio_hw_root_port_t *root = PIO_USB_HW_RPORT(rhport);
  port_pin_status_t line_state = pio_hw_get_line_state(root);

  return line_state != PORT_PIN_SE0;
}

tusb_speed_t hcd_port_speed_get(uint8_t rhport)
{
  // TODO determine link speed
  return TUSB_SPEED_FULL;
}

// Close all opened endpoint belong to this device
void hcd_device_close(uint8_t rhport, uint8_t dev_addr)
{
}

uint32_t hcd_frame_number(uint8_t rhport)
{
  return 0;
}

void hcd_int_enable(uint8_t rhport)
{
}

void hcd_int_disable(uint8_t rhport)
{
}

//--------------------------------------------------------------------+
// Endpoint API
//--------------------------------------------------------------------+

bool hcd_edpt_open(uint8_t rhport, uint8_t dev_addr, tusb_desc_endpoint_t const * desc_ep)
{
  rhport = RHPORT_PIO(rhport);
  return pio_usb_endpoint_open(rhport, dev_addr, (uint8_t const*) desc_ep);
}

bool hcd_edpt_xfer(uint8_t rhport, uint8_t dev_addr, uint8_t ep_addr, uint8_t * buffer, uint16_t buflen)
{
  rhport = RHPORT_PIO(rhport);
  return pio_usb_endpoint_transfer(rhport, dev_addr, ep_addr, buffer, buflen);
}

bool hcd_setup_send(uint8_t rhport, uint8_t dev_addr, uint8_t const setup_packet[8])
{
  rhport = RHPORT_PIO(rhport);
  return pio_usb_endpoint_send_setup(rhport, dev_addr, setup_packet);
}

//bool hcd_edpt_busy(uint8_t dev_addr, uint8_t ep_addr)
//{
//    // EPX is shared, so multiple device addresses and endpoint addresses share that
//    // so if any transfer is active on epx, we are busy. Interrupt endpoints have their own
//    // EPX so ep->active will only be busy if there is a pending transfer on that interrupt endpoint
//    // on that device
//    pico_trace("hcd_edpt_busy dev addr %d ep_addr 0x%x\n", dev_addr, ep_addr);
//    struct hw_endpoint *ep = get_dev_ep(dev_addr, ep_addr);
//    assert(ep);
//    bool busy = ep->active;
//    pico_trace("busy == %d\n", busy);
//    return busy;
//}

bool hcd_edpt_clear_stall(uint8_t dev_addr, uint8_t ep_addr)
{
  (void) dev_addr;
  (void) ep_addr;

  return true;
}

void __no_inline_not_in_flash_func(handle_endpoint_irq)(pio_hw_root_port_t* port, uint32_t flag)
{
  volatile uint32_t* ep_reg;
  xfer_result_t result;

  if ( flag == PIO_USB_INTS_ENDPOINT_COMPLETE_BITS )
  {
    ep_reg = &port->ep_complete;
    result = XFER_RESULT_SUCCESS;
  }
  else if ( flag == PIO_USB_INTS_ENDPOINT_ERROR_BITS )
  {
    ep_reg = &port->ep_error;
    result = XFER_RESULT_FAILED;
  }
  else if ( flag == PIO_USB_INTS_ENDPOINT_STALLED_BITS )
  {
    ep_reg = &port->ep_stalled;
    result = XFER_RESULT_STALLED;
  }
  else
  {
    // something wrong
    return;
  }

  const uint32_t ep_all = *ep_reg;

  for(uint8_t ep_idx = 0; ep_idx < PIO_USB_EP_POOL_CNT; ep_idx++)
  {
    uint32_t const mask = (1u << ep_idx);

    if (ep_all & mask)
    {
      pio_hw_endpoint_t* ep = PIO_USB_HW_EP(ep_idx);
      hcd_event_xfer_complete(ep->dev_addr, ep->ep_num, ep->actual_len, result, true);
    }
  }

  // clear all
  (*ep_reg) &= ~ep_all;
}

// IRQ Handler
void __no_inline_not_in_flash_func(pio_usb_host_irq_handler)(uint8_t root_id)
{
  pio_hw_root_port_t* port = PIO_USB_HW_RPORT(root_id);

  if ( port->ints & PIO_USB_INTS_CONNECT_BITS )
  {
    port->ints &= ~PIO_USB_INTS_CONNECT_BITS;

    hcd_event_device_attach(root_id+1, true);
  }

  if ( port->ints & PIO_USB_INTS_DISCONNECT_BITS )
  {
    port->ints &= ~PIO_USB_INTS_DISCONNECT_BITS;
    hcd_event_device_remove(root_id+1, true);
  }

  if ( port->ints & PIO_USB_INTS_ENDPOINT_COMPLETE_BITS )
  {
    port->ints &= ~PIO_USB_INTS_ENDPOINT_COMPLETE_BITS;
    handle_endpoint_irq(port, PIO_USB_INTS_ENDPOINT_COMPLETE_BITS);
  }

  if ( port->ints & PIO_USB_INTS_ENDPOINT_ERROR_BITS )
  {
    port->ints &= ~PIO_USB_INTS_ENDPOINT_ERROR_BITS;
    handle_endpoint_irq(port, PIO_USB_INTS_ENDPOINT_ERROR_BITS);
  }

  if ( port->ints & PIO_USB_INTS_ENDPOINT_STALLED_BITS )
  {
    port->ints &= ~PIO_USB_INTS_ENDPOINT_STALLED_BITS;
    handle_endpoint_irq(port, PIO_USB_INTS_ENDPOINT_STALLED_BITS);
  }
}

#endif
