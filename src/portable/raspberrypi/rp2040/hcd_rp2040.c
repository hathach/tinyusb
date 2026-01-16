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

// Host mode uses one shared endpoint register for non-interrupt endpoint
static hcd_endpoint_t  ep_pool[USB_MAX_ENDPOINTS];
static hcd_endpoint_t *epx = &ep_pool[0]; // current active endpoint

// Flags we set by default in sie_ctrl (we add other bits on top)
enum {
  SIE_CTRL_BASE      = USB_SIE_CTRL_PULLDOWN_EN_BITS | USB_SIE_CTRL_EP0_INT_1BUF_BITS,
  SIE_CTRL_BASE_MASK = USB_SIE_CTRL_PULLDOWN_EN_BITS | USB_SIE_CTRL_EP0_INT_1BUF_BITS | USB_SIE_CTRL_SOF_EN_BITS |
                       USB_SIE_CTRL_KEEP_ALIVE_EN_BITS
};

enum {
  SIE_CTRL_SPEED_DISCONNECT = 0,
  SIE_CTRL_SPEED_LOW        = 1,
  SIE_CTRL_SPEED_FULL       = 2,
};

//--------------------------------------------------------------------+
//
//--------------------------------------------------------------------+

static hcd_endpoint_t *edpt_alloc(void) {
  for (uint i = 0; i < TU_ARRAY_SIZE(ep_pool); i++) {
    hcd_endpoint_t *ep = &ep_pool[i];
    if (ep->hwep.max_packet_size == 0) {
      return ep;
    }
  }
  return NULL;
}

static hcd_endpoint_t *edpt_find(uint8_t daddr, uint8_t ep_addr) {
  for (uint32_t i = 0; i < TU_ARRAY_SIZE(ep_pool); i++) {
    hcd_endpoint_t *ep = &ep_pool[i];
    if ((ep->dev_addr == daddr) && (ep->hwep.max_packet_size > 0) &&
        (ep->hwep.ep_addr == ep_addr || (tu_edpt_number(ep_addr) == 0 && tu_edpt_number(ep->hwep.ep_addr) == 0))) {
      return ep;
    }
  }

  return NULL;
}

// static hcd_endpoint_t* epdt_find_interrupt(uint8_t )

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

static void __tusb_irq_path_func(hw_xfer_complete)(hcd_endpoint_t *ep, xfer_result_t xfer_result) {
  // Mark transfer as done before we tell the tinyusb stack
  uint8_t dev_addr    = ep->dev_addr;
  uint8_t ep_addr     = ep->hwep.ep_addr;
  uint    xferred_len = ep->hwep.xferred_len;
  hw_endpoint_reset_transfer(&ep->hwep);
  hcd_event_xfer_complete(dev_addr, ep_addr, xferred_len, xfer_result, true);
}

static void __tusb_irq_path_func(handle_hwbuf_status_bit)(hcd_endpoint_t *ep, io_rw_32 *ep_reg, io_rw_32 *buf_reg) {
  const bool done = hw_endpoint_xfer_continue(&ep->hwep, ep_reg, buf_reg);
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
    usb_hw_clear->buf_status = bit;

    io_rw_32 *ep_reg  = &usbh_dpram->epx_ctrl;
    io_rw_32 *buf_reg = &usbh_dpram->epx_buf_ctrl;
    handle_hwbuf_status_bit(epx, ep_reg, buf_reg);
  }

  // Check "interrupt" (asynchronous) endpoints for both IN and OUT
  // TODO use clz for better efficiency
  for (uint i = 1; i <= USB_HOST_INTERRUPT_ENDPOINTS && buf_status; i++) {
    // EPX  IN/OUT is bit 0, 1
    // IEP1 IN/OUT is bit 2, 3
    // IEP2 IN/OUT is bit 4, 5
    // etc
    for (uint j = 0; j < 2; j++) {
      bit = 1 << (i * 2 + j);
      if (buf_status & bit) {
        buf_status &= ~bit;
        usb_hw_clear->buf_status = bit;

        for (uint8_t e = 0; e < USB_MAX_ENDPOINTS; e++) {
          hcd_endpoint_t *ep = &ep_pool[e];
          if (ep->interrupt_num == i) {
            io_rw_32 *ep_reg  = &usbh_dpram->int_ep_ctrl[ep->interrupt_num - 1].ctrl;
            io_rw_32 *buf_reg = &usbh_dpram->int_ep_buffer_ctrl[ep->interrupt_num - 1].ctrl;
            handle_hwbuf_status_bit(ep, ep_reg, buf_reg);
            break;
          }
        }
      }
    }
  }

  if (buf_status) {
    panic("Unhandled buffer %d\n", buf_status);
  }
}

//
// static void edpt_scheduler(void) {
// }

static void __tusb_irq_path_func(hcd_rp2040_irq)(void) {
  const uint32_t status = usb_hw->ints;

  if (status & USB_INTS_HOST_CONN_DIS_BITS) {
    uint8_t speed = dev_speed();
    if (speed == SIE_CTRL_SPEED_DISCONNECT) {
      hcd_event_device_remove(RHPORT_NATIVE, true);
    } else {
      if (speed == SIE_CTRL_SPEED_LOW) {
        usb_hw->sie_ctrl = SIE_CTRL_BASE | USB_SIE_CTRL_KEEP_ALIVE_EN_BITS;
      } else {
        usb_hw->sie_ctrl = SIE_CTRL_BASE | USB_SIE_CTRL_SOF_EN_BITS;
      }
      hcd_event_device_attach(RHPORT_NATIVE, true);
    }
    usb_hw_clear->sie_status = USB_SIE_STATUS_SPEED_BITS;
  }

  if (status & USB_INTS_STALL_BITS) {
    // We have rx'd a stall from the device
    // NOTE THIS SHOULD HAVE PRIORITY OVER BUFF_STATUS
    // AND TRANS_COMPLETE as the stall is an alternative response
    // to one of those events
    usb_hw_clear->sie_status = USB_SIE_STATUS_STALL_REC_BITS;
    hw_xfer_complete(epx, XFER_RESULT_STALLED);
  }

  if (status & USB_INTS_BUFF_STATUS_BITS) {
    handle_hwbuf_status();
  }

  if (status & USB_INTS_TRANS_COMPLETE_BITS) {
    usb_hw_clear->sie_status = USB_SIE_STATUS_TRANS_COMPLETE_BITS;

    // only handle setup packet
    if (usb_hw->sie_ctrl & USB_SIE_CTRL_SEND_SETUP_BITS) {
      epx->hwep.xferred_len = 8;
      hw_xfer_complete(epx, XFER_RESULT_SUCCESS);
    } else {
      // Don't care. Will handle this in buff status
    }
  }

  if (status & USB_INTS_ERROR_RX_TIMEOUT_BITS) {
    usb_hw_clear->sie_status = USB_SIE_STATUS_RX_TIMEOUT_BITS;
  }

  if (status & USB_INTS_ERROR_DATA_SEQ_BITS) {
    usb_hw_clear->sie_status = USB_SIE_STATUS_DATA_SEQ_ERROR_BITS;
    panic("Data Seq Error \n");
  }
}

void __tusb_irq_path_func(hcd_int_handler)(uint8_t rhport, bool in_isr) {
  (void)rhport;
  (void)in_isr;
  hcd_rp2040_irq();
}

static void hw_endpoint_init(hcd_endpoint_t *ep, uint8_t dev_addr, const tusb_desc_endpoint_t *ep_desc) {
  const uint8_t    ep_addr        = ep_desc->bEndpointAddress;
  const uint16_t   wMaxPacketSize = tu_edpt_packet_size(ep_desc);
  const uint8_t    transfer_type  = ep_desc->bmAttributes.xfer;
  // const uint8_t    bmInterval     = ep_desc->bInterval;

  ep->hwep.max_packet_size = wMaxPacketSize;
  ep->hwep.ep_addr        = ep_addr;
  ep->dev_addr            = dev_addr;
  ep->transfer_type       = transfer_type;
  ep->need_pre            = need_pre(dev_addr);
  ep->hwep.next_pid       = 0u;

  if (transfer_type != TUSB_XFER_INTERRUPT) {
    ep->hwep.dpram_buf = usbh_dpram->epx_data;
  } else {
    // from 15 interrupt endpoints pool
    uint8_t int_idx;
    for (int_idx = 0; int_idx < USB_HOST_INTERRUPT_ENDPOINTS; int_idx++) {
      if (!tu_bit_test(usb_hw_set->int_ep_ctrl, 1 + int_idx)) {
        ep->interrupt_num = int_idx + 1;
        break;
      }
    }
    assert(int_idx < USB_HOST_INTERRUPT_ENDPOINTS);
    assert(ep_desc->bInterval > 0);

    //------------- dpram buf -------------//
    // 15x64 last bytes of DPRAM for interrupt endpoint buffers
    ep->hwep.dpram_buf = (uint8_t *)(USBCTRL_DPRAM_BASE + USB_DPRAM_MAX - (int_idx + 1u) * 64u);
    uint32_t ep_ctrl     = EP_CTRL_ENABLE_BITS | EP_CTRL_INTERRUPT_PER_BUFFER |
                       (TUSB_XFER_INTERRUPT << EP_CTRL_BUFFER_TYPE_LSB) | hw_data_offset(ep->hwep.dpram_buf) |
                       (uint32_t)((ep_desc->bInterval - 1) << EP_CTRL_HOST_INTERRUPT_INTERVAL_LSB);
    usbh_dpram->int_ep_ctrl[int_idx].ctrl = ep_ctrl;

    //------------- address control -------------//
    const uint8_t epnum     = tu_edpt_number(ep_addr);
    uint32_t      addr_ctrl = (uint32_t)(dev_addr | (epnum << USB_ADDR_ENDP1_ENDPOINT_LSB));
    if (tu_edpt_dir(ep_addr) == TUSB_DIR_OUT) {
      addr_ctrl |= USB_ADDR_ENDP1_INTEP_DIR_BITS;
    }
    if (ep->need_pre) {
      addr_ctrl |= USB_ADDR_ENDP1_INTEP_PREAMBLE_BITS;
    }
    usb_hw->int_ep_addr_ctrl[int_idx] = addr_ctrl;

    // Finally, activate interrupt endpoint
    usb_hw_set->int_ep_ctrl = 1u << ep->interrupt_num;
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
  (void)rhport;
  return usb_hw->sie_status & USB_SIE_STATUS_SPEED_BITS;
}

tusb_speed_t hcd_port_speed_get(uint8_t rhport) {
  (void)rhport;
  switch (dev_speed()) {
    case SIE_CTRL_SPEED_LOW:
      return TUSB_SPEED_LOW;
    case SIE_CTRL_SPEED_FULL:
      return TUSB_SPEED_FULL;
    default:
      return TUSB_SPEED_INVALID;
  }
}

// Close all opened endpoint belong to this device
void hcd_device_close(uint8_t rhport, uint8_t dev_addr) {
  (void)rhport;
  (void)dev_addr;

  // reset epx if it is currently active with unplugged device
  if (epx->hwep.max_packet_size > 0 && epx->dev_addr == dev_addr) {
    // if (epx->hwep.active) {
    //   // need to abort transfer
    // }
    epx->hwep.max_packet_size = 0;
  }

  for (size_t i = 0; i < TU_ARRAY_SIZE(ep_pool); i++) {
    hcd_endpoint_t *ep = &ep_pool[i];
    if (ep->dev_addr == dev_addr && ep->hwep.max_packet_size > 0) {
      if (ep->interrupt_num) {
        // disable interrupt endpoint
        usb_hw_clear->int_ep_ctrl                       = 1u << ep->interrupt_num;
        usb_hw->int_ep_addr_ctrl[ep->interrupt_num - 1] = 0;

        usbh_dpram->int_ep_buffer_ctrl[ep->interrupt_num - 1].ctrl = 0;
        usbh_dpram->int_ep_ctrl[ep->interrupt_num - 1].ctrl        = 0;
      }

      ep->hwep.max_packet_size = 0; // mark as unused
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
  pico_trace("hcd_edpt_open dev_addr %d, ep_addr %d\n", dev_addr, ep_desc->bEndpointAddress);
  hcd_endpoint_t *ep = edpt_alloc();
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

TU_ATTR_ALWAYS_INLINE static inline void sie_start_xfer(uint32_t value) {
  value |= (usb_hw->sie_ctrl & SIE_CTRL_BASE_MASK); // preserve base bits

  // START_TRANS bit on SIE_CTRL has the same behavior as the AVAILABLE bit
  // described in RP2040 Datasheet, release 2.1, section "4.1.2.5.1. Concurrent access".
  // We write everything except the START_TRANS bit first, then wait some cycles.
  usb_hw->sie_ctrl = value;
  busy_wait_at_least_cycles(12);
  usb_hw->sie_ctrl = value | USB_SIE_CTRL_START_TRANS_BITS;
}

// xfer using epx
static void edpt_xfer(hcd_endpoint_t *ep, uint8_t *buffer, tu_fifo_t *ff, uint16_t total_len) {
  if (ep->transfer_type == TUSB_XFER_INTERRUPT) {
    // For interrupt endpoint control and buffer is already configured
    // Note: Interrupt is single buffered only
    io_rw_32 *ep_reg  = &usbh_dpram->int_ep_ctrl[ep->interrupt_num - 1].ctrl;
    io_rw_32 *buf_reg = &usbh_dpram->int_ep_buffer_ctrl[ep->interrupt_num - 1].ctrl;
    hw_endpoint_xfer_start(&ep->hwep, ep_reg, buf_reg, buffer, ff, total_len);
  } else {
    const uint8_t    ep_num = tu_edpt_number(ep->hwep.ep_addr);
    const tusb_dir_t ep_dir = tu_edpt_dir(ep->hwep.ep_addr);

    // ep control
    const uint32_t dpram_offset = hw_data_offset(ep->hwep.dpram_buf);
    const uint32_t ep_ctrl      = EP_CTRL_ENABLE_BITS | EP_CTRL_INTERRUPT_PER_BUFFER |
                             ((uint32_t)ep->transfer_type << EP_CTRL_BUFFER_TYPE_LSB) | dpram_offset;
    usbh_dpram->epx_ctrl = ep_ctrl;

    io_rw_32 *ep_reg  = &usbh_dpram->epx_ctrl;
    io_rw_32 *buf_reg = &usbh_dpram->epx_buf_ctrl;
    hw_endpoint_xfer_start(&ep->hwep, ep_reg, buf_reg, buffer, ff, total_len);

    // addr control
    usb_hw->dev_addr_ctrl = (uint32_t)(ep->dev_addr | (ep_num << USB_ADDR_ENDP_ENDPOINT_LSB));

    epx = ep;

    // start transfer
    const uint32_t sie_ctrl = (ep_dir ? USB_SIE_CTRL_RECEIVE_DATA_BITS : USB_SIE_CTRL_SEND_DATA_BITS) |
                              (ep->need_pre ? USB_SIE_CTRL_PREAMBLE_EN_BITS : 0);
    sie_start_xfer(sie_ctrl);
  }
}

bool hcd_edpt_xfer(uint8_t rhport, uint8_t dev_addr, uint8_t ep_addr, uint8_t *buffer, uint16_t buflen) {
  (void)rhport;

  hcd_endpoint_t *ep = edpt_find(dev_addr, ep_addr);
  TU_ASSERT(ep);

  // Control endpoint can change direction 0x00 <-> 0x80
  if (ep_addr != ep->hwep.ep_addr) {
    ep->hwep.ep_addr  = ep_addr;
    ep->hwep.next_pid = 1; // data and status stage start with DATA1
  }

  edpt_xfer(ep, buffer, NULL, buflen);

  return true;
}

bool hcd_edpt_abort_xfer(uint8_t rhport, uint8_t dev_addr, uint8_t ep_addr) {
  (void)rhport;
  (void)dev_addr;
  (void)ep_addr;
  // TODO not implemented yet
  return false;
}

bool hcd_setup_send(uint8_t rhport, uint8_t dev_addr, const uint8_t setup_packet[8]) {
  (void)rhport;

  // Copy data into setup packet buffer
  for (uint8_t i = 0; i < 8; i++) {
    usbh_dpram->setup_packet[i] = setup_packet[i];
  }

  hcd_endpoint_t *ep = edpt_find(dev_addr, 0x00);
  TU_ASSERT(ep);

  ep->hwep.ep_addr       = 0; // setup is OUT
  ep->hwep.remaining_len = 8;
  ep->hwep.xferred_len   = 0;
  ep->hwep.active        = true;

  epx = ep;

  // Set device address
  usb_hw->dev_addr_ctrl = dev_addr;

  // Set pre if we are a low speed device on full speed hub
  const uint32_t sie_ctrl = USB_SIE_CTRL_SEND_SETUP_BITS | (ep->need_pre ? USB_SIE_CTRL_PREAMBLE_EN_BITS : 0);
  sie_start_xfer(sie_ctrl);

  return true;
}

bool hcd_edpt_clear_stall(uint8_t rhport, uint8_t dev_addr, uint8_t ep_addr) {
  (void)rhport;
  (void)dev_addr;
  (void)ep_addr;

  panic("hcd_clear_stall");
  // return true;
}

#endif
