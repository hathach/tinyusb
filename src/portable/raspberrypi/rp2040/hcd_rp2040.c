/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 * Copyright (c) 2021 Ha Thach (tinyusb.org) for Double Buffered
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

#if CFG_TUH_ENABLED && (CFG_TUSB_MCU == OPT_MCU_RP2040) && !CFG_TUH_RPI_PIO_USB && !CFG_TUH_MAX3421

#include "pico.h"
#include "rp2040_usb.h"

//--------------------------------------------------------------------+
// INCLUDE
//--------------------------------------------------------------------+
#include "osal/osal.h"

#include "host/hcd.h"
#include "host/usbh.h"

// port 0 is native USB port, other is counted as software PIO
#define RHPORT_NATIVE 0

//--------------------------------------------------------------------+
// Low level rp2040 controller functions
//--------------------------------------------------------------------+

#ifndef PICO_USB_HOST_INTERRUPT_ENDPOINTS
#define PICO_USB_HOST_INTERRUPT_ENDPOINTS (USB_MAX_ENDPOINTS - 1)
#endif
static_assert(PICO_USB_HOST_INTERRUPT_ENDPOINTS <= USB_MAX_ENDPOINTS, "");

// Host mode uses one shared endpoint register for non-interrupt endpoint
static struct hw_endpoint ep_pool[1 + PICO_USB_HOST_INTERRUPT_ENDPOINTS];
#define epx (ep_pool[0])

static hw_endpoint_t *ep_active = NULL;

// Flags we set by default in sie_ctrl (we add other bits on top)
enum {
  SIE_CTRL_BASE = USB_SIE_CTRL_SOF_EN_BITS      | USB_SIE_CTRL_KEEP_ALIVE_EN_BITS |
                  USB_SIE_CTRL_PULLDOWN_EN_BITS | USB_SIE_CTRL_EP0_INT_1BUF_BITS
};

//--------------------------------------------------------------------+
//
//--------------------------------------------------------------------+

static hw_endpoint_t *edpt_alloc(void) {
  for (uint i = 0; i < TU_ARRAY_SIZE(ep_pool); i++) {
    hw_endpoint_t *ep = &ep_pool[i];
    if (ep->wMaxPacketSize == 0) {
      return ep;
    }
  }
  return NULL;
}

static hw_endpoint_t *edpt_find(uint8_t daddr, uint8_t ep_addr) {
  for (uint32_t i = 0; i < TU_ARRAY_SIZE(ep_pool); i++) {
    struct hw_endpoint *ep = &ep_pool[i];
    if ((ep->dev_addr == daddr) && (ep->wMaxPacketSize > 0) &&
        (ep->ep_addr == ep_addr || (tu_edpt_number(ep_addr) == 0 && tu_edpt_number(ep->ep_addr) == 0))) {
      return ep;
    }
  }

  return NULL;
}

//--------------------------------------------------------------------+
//
//--------------------------------------------------------------------+

TU_ATTR_ALWAYS_INLINE static inline uint8_t dev_speed(void) {
  return (usb_hw->sie_status & USB_SIE_STATUS_SPEED_BITS) >> USB_SIE_STATUS_SPEED_LSB;
}

TU_ATTR_ALWAYS_INLINE static inline bool need_pre(uint8_t dev_addr) {
  // If this device is different to the speed of the root device
  // (i.e. is a low speed device on a full speed hub) then need pre
  return hcd_port_speed_get(0) != tuh_speed_get(dev_addr);
}

static void __tusb_irq_path_func(hw_xfer_complete)(struct hw_endpoint *ep, xfer_result_t xfer_result) {
  // Mark transfer as done before we tell the tinyusb stack
  uint8_t dev_addr = ep->dev_addr;
  uint8_t ep_addr = ep->ep_addr;
  uint xferred_len = ep->xferred_len;
  hw_endpoint_reset_transfer(ep);
  hcd_event_xfer_complete(dev_addr, ep_addr, xferred_len, xfer_result, true);
}

static void __tusb_irq_path_func(handle_hwbuf_status_bit)(uint bit, struct hw_endpoint *ep) {
  usb_hw_clear->buf_status = bit;
  const bool done          = hw_endpoint_xfer_continue(ep);
  if (done) {
    hw_xfer_complete(ep, XFER_RESULT_SUCCESS);
  }
}

static void __tusb_irq_path_func(handle_hwbuf_status)(void) {
  uint32_t buf_status = usb_hw->buf_status;
  pico_trace("buf_status 0x%08lx\n", buf_status);

  // Check EPX first
  uint32_t bit = 1u;
  if (buf_status & bit) {
    buf_status &= ~bit;
    hw_endpoint_t *ep = ep_active;
    handle_hwbuf_status_bit(bit, ep);
  }

  // Check "interrupt" (asynchronous) endpoints for both IN and OUT
  for (uint i = 1; i <= USB_HOST_INTERRUPT_ENDPOINTS && buf_status; i++) {
    // EPX is bit 0 & 1
    // IEP1 IN  is bit 2
    // IEP1 OUT is bit 3
    // IEP2 IN  is bit 4
    // IEP2 OUT is bit 5
    // IEP3 IN  is bit 6
    // IEP3 OUT is bit 7
    // etc
    for (uint j = 0; j < 2; j++) {
      bit = 1 << (i * 2 + j);
      if (buf_status & bit) {
        buf_status &= ~bit;
        handle_hwbuf_status_bit(bit, &ep_pool[i]);
      }
    }
  }

  if (buf_status) {
    panic("Unhandled buffer %d\n", buf_status);
  }
}

static void __tusb_irq_path_func(hw_trans_complete)(void) {
  if (usb_hw->sie_ctrl & USB_SIE_CTRL_SEND_SETUP_BITS) {
    hw_endpoint_t *ep = ep_active;
    ep->xferred_len = 8;
    hw_xfer_complete(ep, XFER_RESULT_SUCCESS);
  } else {
    // Don't care. Will handle this in buff status
    return;
  }
}

static void __tusb_irq_path_func(hcd_rp2040_irq)(void) {
  const uint32_t status = usb_hw->ints;

  if (status & USB_INTS_HOST_CONN_DIS_BITS) {
    if (dev_speed()) {
      hcd_event_device_attach(RHPORT_NATIVE, true);
    } else {
      hcd_event_device_remove(RHPORT_NATIVE, true);
    }

    usb_hw_clear->sie_status = USB_SIE_STATUS_SPEED_BITS;
  }

  if (status & USB_INTS_STALL_BITS) {
    // We have rx'd a stall from the device
    // NOTE THIS SHOULD HAVE PRIORITY OVER BUFF_STATUS
    // AND TRANS_COMPLETE as the stall is an alternative response
    // to one of those events
    usb_hw_clear->sie_status = USB_SIE_STATUS_STALL_REC_BITS;
    hw_xfer_complete(&epx, XFER_RESULT_STALLED);
  }

  if (status & USB_INTS_BUFF_STATUS_BITS) {
    handle_hwbuf_status();
  }

  if (status & USB_INTS_TRANS_COMPLETE_BITS) {
    usb_hw_clear->sie_status = USB_SIE_STATUS_TRANS_COMPLETE_BITS;
    hw_trans_complete();
  }

  if (status & USB_INTS_ERROR_RX_TIMEOUT_BITS) {
    usb_hw_clear->sie_status = USB_SIE_STATUS_RX_TIMEOUT_BITS;
  }

  if (status & USB_INTS_ERROR_DATA_SEQ_BITS) {
    usb_hw_clear->sie_status = USB_SIE_STATUS_DATA_SEQ_ERROR_BITS;
    TU_LOG(3, "  Seq Error: [0] = 0x%04u  [1] = 0x%04x\r\n", tu_u32_low16(*hwbuf_ctrl_reg_host(&epx)),
           tu_u32_high16(*hwbuf_ctrl_reg_host(&epx)));
    panic("Data Seq Error \n");
  }
}

void __tusb_irq_path_func(hcd_int_handler)(uint8_t rhport, bool in_isr) {
  (void) rhport;
  (void) in_isr;
  hcd_rp2040_irq();
}

static void hw_endpoint_init(hw_endpoint_t *ep, uint8_t dev_addr, const tusb_desc_endpoint_t *ep_desc) {
  const uint8_t    ep_addr        = ep_desc->bEndpointAddress;
  const uint16_t   wMaxPacketSize = tu_edpt_packet_size(ep_desc);
  const uint8_t    transfer_type  = ep_desc->bmAttributes.xfer;
  const uint8_t    bmInterval     = ep_desc->bInterval;
  const uint8_t    num            = tu_edpt_number(ep_addr);
  const tusb_dir_t dir            = tu_edpt_dir(ep_addr);
  ep->ep_addr                     = ep_addr;
  ep->dev_addr                    = dev_addr;
  ep->transfer_type               = transfer_type;

  // Response to a setup packet on EP0 starts with pid of 1
  ep->next_pid = (num == 0 ? 1u : 0u);
  ep->wMaxPacketSize = wMaxPacketSize;

  pico_trace("hw_endpoint_init dev %d ep %02X xfer %d\n", ep->dev_addr, ep->ep_addr, transfer_type);
  pico_trace("dev %d ep %02X setup buffer @ 0x%p\n", ep->dev_addr, ep->ep_addr, ep->hw_data_buf);
  uint dpram_offset = hw_data_offset(ep->hw_data_buf);
  // Bits 0-5 should be 0
  assert(!(dpram_offset & 0b111111));

  // Fill in endpoint control register with buffer offset
  uint32_t ctrl_value = EP_CTRL_ENABLE_BITS | EP_CTRL_INTERRUPT_PER_BUFFER |
                        ((uint32_t)transfer_type << EP_CTRL_BUFFER_TYPE_LSB) | dpram_offset;
  if (bmInterval) {
    ctrl_value |= (uint32_t)((bmInterval - 1) << EP_CTRL_HOST_INTERRUPT_INTERVAL_LSB);
  }

  io_rw_32 *ctrl_reg = hwep_ctrl_reg_host(ep);
  *ctrl_reg          = ctrl_value;
  pico_trace("endpoint control (0x%p) <- 0x%lx\n", ctrl_reg, ctrl_value);

  if (ep != &epx) {
    // Endpoint has its own addr_endp and interrupt bits to be setup!
    // This is an interrupt/async endpoint. so need to set up ADDR_ENDP register with:
    // - device address
    // - endpoint number / direction
    // - preamble
    uint32_t reg = (uint32_t)(dev_addr | (num << USB_ADDR_ENDP1_ENDPOINT_LSB));

    if (dir == TUSB_DIR_OUT) {
      reg |= USB_ADDR_ENDP1_INTEP_DIR_BITS;
    }

    if (need_pre(dev_addr)) {
      reg |= USB_ADDR_ENDP1_INTEP_PREAMBLE_BITS;
    }
    usb_hw->int_ep_addr_ctrl[ep->interrupt_num] = reg;

    // Finally, enable interrupt that endpoint
    usb_hw_set->int_ep_ctrl = 1 << (ep->interrupt_num + 1);

    // If it's an interrupt endpoint we need to set up the buffer control register
  }
}

//--------------------------------------------------------------------+
// HCD API
//--------------------------------------------------------------------+
bool hcd_init(uint8_t rhport, const tusb_rhport_init_t* rh_init) {
  (void) rhport;
  (void) rh_init;
  pico_trace("hcd_init %d\n", rhport);
  assert(rhport == 0);

  // Reset any previous state
  rp2usb_init();

  // Force VBUS detect to always present, for now we assume vbus is always provided (without using VBUS En)
  usb_hw->pwr = USB_USB_PWR_VBUS_DETECT_BITS | USB_USB_PWR_VBUS_DETECT_OVERRIDE_EN_BITS;

  // Remove shared irq if it was previously added so as not to fill up shared irq slots
  irq_remove_handler(USBCTRL_IRQ, hcd_rp2040_irq);

  irq_add_shared_handler(USBCTRL_IRQ, hcd_rp2040_irq, PICO_SHARED_IRQ_HANDLER_HIGHEST_ORDER_PRIORITY);

  // clear epx and interrupt eps
  memset(&ep_pool, 0, sizeof(ep_pool));

  // Enable in host mode with SOF / Keep alive on
  usb_hw->main_ctrl = USB_MAIN_CTRL_CONTROLLER_EN_BITS | USB_MAIN_CTRL_HOST_NDEVICE_BITS;
  usb_hw->sie_ctrl = SIE_CTRL_BASE;
  usb_hw->inte = USB_INTE_BUFF_STATUS_BITS      |
                 USB_INTE_HOST_CONN_DIS_BITS    |
                 USB_INTE_HOST_RESUME_BITS      |
                 USB_INTE_STALL_BITS            |
                 USB_INTE_TRANS_COMPLETE_BITS   |
                 USB_INTE_ERROR_RX_TIMEOUT_BITS |
                 USB_INTE_ERROR_DATA_SEQ_BITS   ;

  return true;
}

bool hcd_deinit(uint8_t rhport) {
  (void) rhport;

  irq_remove_handler(USBCTRL_IRQ, hcd_rp2040_irq);
  reset_block(RESETS_RESET_USBCTRL_BITS);
  unreset_block_wait(RESETS_RESET_USBCTRL_BITS);

  return true;
}

void hcd_port_reset(uint8_t rhport) {
  (void) rhport;
  // TODO: Nothing to do here yet. Perhaps need to reset some state?
}

void hcd_port_reset_end(uint8_t rhport) {
  (void)rhport;
}

bool hcd_port_connect_status(uint8_t rhport) {
  (void) rhport;
  return usb_hw->sie_status & USB_SIE_STATUS_SPEED_BITS;
}

tusb_speed_t hcd_port_speed_get(uint8_t rhport)
{
  (void) rhport;
  assert(rhport == 0);

  // TODO: Should enumval this register
  switch ( dev_speed() )
  {
    case 1:
      return TUSB_SPEED_LOW;
    case 2:
      return TUSB_SPEED_FULL;
    default:
      panic("Invalid speed\n");
      // return TUSB_SPEED_INVALID;
  }
}

// Close all opened endpoint belong to this device
void hcd_device_close(uint8_t rhport, uint8_t dev_addr) {
  pico_trace("hcd_device_close %d\n", dev_addr);
  (void) rhport;

  // reset epx if it is currently active with unplugged device
  if (epx.wMaxPacketSize > 0 && epx.active && epx.dev_addr == dev_addr) {
    epx.wMaxPacketSize         = 0;
    *hwep_ctrl_reg_host(&epx)  = 0;
    *hwbuf_ctrl_reg_host(&epx) = 0;
    hw_endpoint_reset_transfer(&epx);
  }

  // dev0 only has ep0
  if (dev_addr != 0) {
    for (size_t i = 1; i < TU_ARRAY_SIZE(ep_pool); i++) {
      hw_endpoint_t *ep = &ep_pool[i];
      if (ep->dev_addr == dev_addr && ep->wMaxPacketSize > 0) {
        // in case it is an interrupt endpoint, disable it
        usb_hw_clear->int_ep_ctrl = (1 << (ep->interrupt_num + 1));
        usb_hw->int_ep_addr_ctrl[ep->interrupt_num] = 0;

        // unconfigure the endpoint
        ep->wMaxPacketSize       = 0;
        *hwep_ctrl_reg_host(ep)  = 0;
        *hwbuf_ctrl_reg_host(ep) = 0;
        hw_endpoint_reset_transfer(ep);
      }
    }
  }
}

uint32_t hcd_frame_number(uint8_t rhport) {
  (void)rhport;
  return usb_hw->sof_rd;
}

void hcd_int_enable(uint8_t rhport) {
  (void)rhport;
  irq_set_enabled(USBCTRL_IRQ, true);
}

void hcd_int_disable(uint8_t rhport) {
  (void)rhport;
  // todo we should check this is disabling from the correct core; note currently this is never called
  irq_set_enabled(USBCTRL_IRQ, false);
}

//--------------------------------------------------------------------+
// Endpoint API
//--------------------------------------------------------------------+
bool hcd_edpt_open(uint8_t rhport, uint8_t dev_addr, const tusb_desc_endpoint_t *ep_desc) {
  (void)rhport;
  hw_endpoint_t *ep = edpt_alloc();
  TU_ASSERT(ep);

  hw_endpoint_init(ep, dev_addr, ep_desc);

  return true;
}

bool hcd_edpt_close(uint8_t rhport, uint8_t daddr, uint8_t ep_addr) {
  (void)rhport;
  (void)daddr;
  (void)ep_addr;
  return false; // TODO not implemented yet
}

// xfer using epx
static bool edpt_xfer(hw_endpoint_t *ep, uint8_t *buffer, tu_fifo_t *ff, uint16_t total_len) {
  const uint8_t    ep_num = tu_edpt_number(ep->ep_addr);
  const tusb_dir_t ep_dir = tu_edpt_dir(ep->ep_addr);

  ep->remaining_len = total_len;
  ep->xferred_len   = 0;
  ep->active        = true;

  if (ff != NULL) {
    ep->user_fifo    = ff;
    ep->is_xfer_fifo = true;
  } else {
    ep->user_buf     = buffer;
    ep->is_xfer_fifo = false;
  }

  ep_active = ep;

  ep->hw_data_buf   = &usbh_dpram->epx_data[0];
  uint dpram_offset = hw_data_offset(ep->hw_data_buf);

  // Fill in endpoint control register with buffer offset
  uint32_t ctrl_value = EP_CTRL_ENABLE_BITS | EP_CTRL_INTERRUPT_PER_BUFFER |
                        ((uint32_t)ep->transfer_type << EP_CTRL_BUFFER_TYPE_LSB) | dpram_offset;
  usbh_dpram->epx_ctrl = ctrl_value;

  hw_endpoint_start_next_buffer(ep);

  usb_hw->dev_addr_ctrl = (uint32_t)(ep->dev_addr | (ep_num << USB_ADDR_ENDP_ENDPOINT_LSB));
  uint32_t flags        = USB_SIE_CTRL_START_TRANS_BITS | SIE_CTRL_BASE |
                   (ep_dir ? USB_SIE_CTRL_RECEIVE_DATA_BITS : USB_SIE_CTRL_SEND_DATA_BITS) |
                   (need_pre(ep->dev_addr) ? USB_SIE_CTRL_PREAMBLE_EN_BITS : 0);

  // START_TRANS bit on SIE_CTRL seems to exhibit the same behavior as the AVAILABLE bit
  // described in RP2040 Datasheet, release 2.1, section "4.1.2.5.1. Concurrent access".
  // We write everything except the START_TRANS bit first, then wait some cycles.
  usb_hw->sie_ctrl = flags & ~USB_SIE_CTRL_START_TRANS_BITS;
  busy_wait_at_least_cycles(12);
  usb_hw->sie_ctrl = flags;

  return true;
}

bool hcd_edpt_xfer(uint8_t rhport, uint8_t dev_addr, uint8_t ep_addr, uint8_t *buffer, uint16_t buflen) {
  (void)rhport;

  hw_endpoint_t *ep = edpt_find(dev_addr, ep_addr);
  TU_ASSERT(ep);

  // Control endpoint can change direction 0x00 <-> 0x80
  if (tu_edpt_number(ep_addr) == 0) {
    ep->ep_addr  = ep_addr;
    ep->next_pid = 1; // data and status stage start with DATA1
  }

  edpt_xfer(ep, buffer, NULL, buflen);

  #if 0
  // EP should be inactive
  // assert(!ep->active);

  // Control endpoint can change direction 0x00 <-> 0x80
  if (ep_addr != ep->ep_addr) {
    assert(ep_num == 0);

    // Direction has flipped on endpoint control so re init it but with same properties
    hw_endpoint_init(ep, dev_addr, ep_addr, ep->wMaxPacketSize, TUSB_XFER_CONTROL, 0);
  }

  // If a normal transfer (non-interrupt) then initiate using
  // sie ctrl registers. Otherwise, interrupt ep registers should
  // already be configured
  if (ep == &epx) {
    hw_endpoint_xfer_start(ep, buffer, NULL, buflen);

    // That has set up buffer control, endpoint control etc
    // for host we have to initiate the transfer
    usb_hw->dev_addr_ctrl = (uint32_t) (dev_addr | (ep_num << USB_ADDR_ENDP_ENDPOINT_LSB));

    uint32_t flags = USB_SIE_CTRL_START_TRANS_BITS | SIE_CTRL_BASE |
                     (ep_dir ? USB_SIE_CTRL_RECEIVE_DATA_BITS : USB_SIE_CTRL_SEND_DATA_BITS) |
                     (need_pre(dev_addr) ? USB_SIE_CTRL_PREAMBLE_EN_BITS : 0);
    // START_TRANS bit on SIE_CTRL seems to exhibit the same behavior as the AVAILABLE bit
    // described in RP2040 Datasheet, release 2.1, section "4.1.2.5.1. Concurrent access".
    // We write everything except the START_TRANS bit first, then wait some cycles.
    usb_hw->sie_ctrl = flags & ~USB_SIE_CTRL_START_TRANS_BITS;
    busy_wait_at_least_cycles(12);
    usb_hw->sie_ctrl = flags;
  } else {
    hw_endpoint_xfer_start(ep, buffer, NULL, buflen);
  }
  #endif

  return true;
}

bool hcd_edpt_abort_xfer(uint8_t rhport, uint8_t dev_addr, uint8_t ep_addr) {
  (void) rhport;
  (void) dev_addr;
  (void) ep_addr;
  // TODO not implemented yet
  return false;
}

bool hcd_setup_send(uint8_t rhport, uint8_t dev_addr, const uint8_t setup_packet[8]) {
  (void)rhport;

  // Copy data into setup packet buffer
  for (uint8_t i = 0; i < 8; i++) {
    usbh_dpram->setup_packet[i] = setup_packet[i];
  }

  // Configure EP0 struct with setup info for the trans complete
  // hw_endpoint_t *ep = hw_endpoint_allocate((uint8_t)TUSB_XFER_CONTROL);
  hw_endpoint_t *ep = edpt_find(dev_addr, 0x00);
  TU_ASSERT(ep);

  ep->ep_addr       = 0; // setup is OUT
  ep->remaining_len = 8;
  ep->active        = true;

  ep_active = ep;

  // Set device address
  usb_hw->dev_addr_ctrl = dev_addr;

  // Set pre if we are a low speed device on full speed hub
  uint32_t const flags = SIE_CTRL_BASE | USB_SIE_CTRL_SEND_SETUP_BITS | USB_SIE_CTRL_START_TRANS_BITS |
                         (need_pre(dev_addr) ? USB_SIE_CTRL_PREAMBLE_EN_BITS : 0);

  // START_TRANS bit on SIE_CTRL seems to exhibit the same behavior as the AVAILABLE bit
  // described in RP2040 Datasheet, release 2.1, section "4.1.2.5.1. Concurrent access".
  // We write everything except the START_TRANS bit first, then wait some cycles.
  usb_hw->sie_ctrl = flags & ~USB_SIE_CTRL_START_TRANS_BITS;
  busy_wait_at_least_cycles(12);
  usb_hw->sie_ctrl = flags;

  return true;
}

bool hcd_edpt_clear_stall(uint8_t rhport, uint8_t dev_addr, uint8_t ep_addr) {
  (void) rhport;
  (void) dev_addr;
  (void) ep_addr;

  panic("hcd_clear_stall");
  // return true;
}

#endif
