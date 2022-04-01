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


static usb_device_t *_test_usb_device = NULL;
static pio_usb_configuration_t pio_host_config = PIO_USB_DEFAULT_CONFIG;

extern root_port_t root_port[PIO_USB_ROOT_PORT_CNT];
extern usb_device_t usb_device[PIO_USB_DEVICE_CNT];
extern pio_port_t pio_port[1];
extern root_port_t root_port[PIO_USB_ROOT_PORT_CNT];
extern endpoint_t ep_pool[PIO_USB_EP_POOL_CNT];

//--------------------------------------------------------------------+
// HCD API
//--------------------------------------------------------------------+
bool hcd_init(uint8_t rhport)
{
  // To run USB SOF interrupt in core1, create alarm pool in core1.
  pio_host_config.alarm_pool = (void*)alarm_pool_create(2, 1);
  _test_usb_device = pio_usb_host_init(&pio_host_config);

  return true;
}

void hcd_port_reset(uint8_t rhport)
{
  rhport = RHPORT_PIO(rhport);

  pio_port_t *pp = &pio_port[0];
  root_port_t *root = &root_port[rhport];

  pio_usb_port_reset_start(root, pp);
}

void hcd_port_reset_end(uint8_t rhport)
{
  rhport = RHPORT_PIO(rhport);

  pio_port_t *pp = &pio_port[0];
  root_port_t *root = &root_port[rhport];

  pio_usb_port_reset_end(root, pp);

  busy_wait_us(100);

  // TODO slow speed
  bool fullspeed_flag = true;

  if (fullspeed_flag && get_port_pin_status(root) == PORT_PIN_FS_IDLE) {
    root->root_device = &usb_device[0];
    if (!root->root_device->connected) {
//      configure_fullspeed_host(pp, &pio_host_config, root);
      root->root_device->is_fullspeed = true;
      root->root_device->is_root = true;
      root->root_device->connected = true;
      root->root_device->root = root;
      root->root_device->event = EVENT_CONNECT;
    }
  } else if (!fullspeed_flag && get_port_pin_status(root) == PORT_PIN_LS_IDLE) {
    root->root_device = &usb_device[0];
    if (!root->root_device->connected) {
//      configure_lowspeed_host(pp, &pio_host_config, root);
      root->root_device->is_fullspeed = false;
      root->root_device->is_root = true;
      root->root_device->connected = true;
      root->root_device->root = root;
      root->root_device->event = EVENT_CONNECT;
    }
  }
}

bool hcd_port_connect_status(uint8_t rhport)
{
  root_port_t* port = &root_port[0];
  bool dp = gpio_get(port->pin_dp);
  bool dm = gpio_get(port->pin_dm);
  return dp || dm;
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

  usb_device_t *device = &usb_device[0];

  return pio_usb_endpoint_open(rhport, dev_addr, (uint8_t const*) desc_ep);

#if 0
  static uint8_t ep_id_idx; // TODO remove later

  if (ep_desc->bEndpointAddress == 0)
  {
    device->event = EVENT_NONE;
    device->address = dev_addr;

    ep_id_idx = 0;
  }else  if (ep_desc->bmAttributes.xfer == TUSB_XFER_INTERRUPT) // only support interrupt endpoint
  {
    endpoint_t *ep = NULL;
    for ( int ep_pool_idx = 0; ep_pool_idx < PIO_USB_EP_POOL_CNT; ep_pool_idx++ )
    {
      if ( ep_pool[ep_pool_idx].ep_num == 0 )
      {
        ep = &ep_pool[ep_pool_idx];
        device->endpoint_id[ep_id_idx] = ep_pool_idx + 1;
        ep_id_idx++;

        ep->data_id = 0;
        ep->ep_num = ep_desc->bEndpointAddress;
        ep->interval = ep_desc->bInterval;
        ep->interval_counter = 0;
        ep->size = (uint8_t) tu_edpt_packet_size(ep_desc);
        ep->attr = EP_ATTR_INTERRUPT;

        break;
      }
    }

//    TU_LOG1_INT(device->connected);
//    TU_LOG1_INT(device->root);
//    TU_LOG1_INT(device->is_root);
//    TU_LOG1_INT(device->is_fullspeed);
//
//    TU_LOG1_INT(ep_id_idx);
  }

  return true;
#endif
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

void __no_inline_not_in_flash_func(handle_endpoint_irq)(root_port_t* port, uint32_t flag)
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
      endpoint_t* ep = PIO_USB_EP(ep_idx);
      hcd_event_xfer_complete(ep->dev_addr, ep->ep_num, ep->actual_len, result, true);
    }
  }

  // clear all
  (*ep_reg) &= ~ep_all;
}

// IRQ Handler
void __no_inline_not_in_flash_func(pio_usb_host_irq_handler)(uint8_t root_id)
{
  root_port_t* port = PIO_USB(root_id);

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
