
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

  #if defined(PICO_RP2350) && PICO_RP2350 == 1
    #define HAS_STOP_EPX_ON_NAK
  #endif

// port 0 is native USB port, other is counted as software PIO
  #define RHPORT_NATIVE 0

  //--------------------------------------------------------------------+
  // INCLUDE
  //--------------------------------------------------------------------+
  #include "rp2040_usb.h"
  #include "osal/osal.h"

  #include "host/hcd.h"
  #include "host/usbh.h"

//--------------------------------------------------------------------+
//
//--------------------------------------------------------------------+

// Host mode uses one shared endpoint register for non-interrupt endpoint
static hw_endpoint_t  ep_pool[USB_MAX_ENDPOINTS];
static hw_endpoint_t *epx = &ep_pool[0]; // current active endpoint

  #ifndef HAS_STOP_EPX_ON_NAK
static volatile bool epx_switch_request = false;
  #endif

enum {
  SIE_CTRL_SPEED_DISCONNECT = 0,
  SIE_CTRL_SPEED_LOW        = 1,
  SIE_CTRL_SPEED_FULL       = 2,
};

enum {
  EPX_CTRL_DEFAULT = EP_CTRL_ENABLE_BITS | EP_CTRL_INTERRUPT_PER_BUFFER | offsetof(usb_host_dpram_t, epx_data)
};

//--------------------------------------------------------------------+
//
//--------------------------------------------------------------------+

static hw_endpoint_t *edpt_alloc(void) {
  for (uint i = 1; i < TU_ARRAY_SIZE(ep_pool); i++) {
    hw_endpoint_t *ep = &ep_pool[i];
    if (ep->max_packet_size == 0) {
      return ep;
    }
  }
  return NULL;
}

static hw_endpoint_t *edpt_find(uint8_t daddr, uint8_t ep_addr) {
  for (uint32_t i = 0; i < TU_ARRAY_SIZE(ep_pool); i++) {
    hw_endpoint_t *ep = &ep_pool[i];
    if ((ep->dev_addr == daddr) && (ep->max_packet_size > 0) &&
        (ep->ep_addr == ep_addr || (tu_edpt_number(ep_addr) == 0 && tu_edpt_number(ep->ep_addr) == 0))) {
      return ep;
    }
  }

  return NULL;
}

TU_ATTR_ALWAYS_INLINE static inline io_rw_32 *dpram_int_ep_ctrl(uint8_t int_num) {
  return &usbh_dpram->int_ep_ctrl[int_num - 1].ctrl;
}

TU_ATTR_ALWAYS_INLINE static inline io_rw_32 *dpram_int_ep_buffer_ctrl(uint8_t int_num) {
  return &usbh_dpram->int_ep_buffer_ctrl[int_num - 1].ctrl;
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

//--------------------------------------------------------------------+
// EPX
//--------------------------------------------------------------------+
TU_ATTR_ALWAYS_INLINE static inline void sie_stop_xfer(void) {
  uint32_t sie_ctrl = (usb_hw->sie_ctrl & SIE_CTRL_BASE_MASK) | USB_SIE_CTRL_STOP_TRANS_BITS;
  usb_hw->sie_ctrl  = sie_ctrl;
  while (usb_hw->sie_ctrl & USB_SIE_CTRL_STOP_TRANS_BITS) {}
}

static void __tusb_irq_path_func(sie_start_xfer)(bool send_setup, bool is_rx, bool need_pre) {
  uint32_t sie_ctrl = usb_hw->sie_ctrl & SIE_CTRL_BASE_MASK; // preserve base bits
  if (send_setup) {
    sie_ctrl |= USB_SIE_CTRL_SEND_SETUP_BITS;
  } else {
    sie_ctrl |= (is_rx ? USB_SIE_CTRL_RECEIVE_DATA_BITS : USB_SIE_CTRL_SEND_DATA_BITS);
  }
  if (need_pre) {
    sie_ctrl |= USB_SIE_CTRL_PREAMBLE_EN_BITS;
  }

  // START_TRANS bit on SIE_CTRL has the same behavior as the AVAILABLE bit
  // described in RP2040 Datasheet, release 2.1, section "4.1.2.5.1. Concurrent access".!
  // We write everything except the START_TRANS bit first, then wait some cycles.
  usb_hw->sie_ctrl = sie_ctrl;
  busy_wait_at_least_cycles(12);
  usb_hw->sie_ctrl = sie_ctrl | USB_SIE_CTRL_START_TRANS_BITS;
}

// prepare epx_ctrl register for new endpoint
TU_ATTR_ALWAYS_INLINE static inline void epx_ctrl_prepare(uint8_t transfer_type) {
  usbh_dpram->epx_ctrl = EPX_CTRL_DEFAULT | ((uint32_t)transfer_type << EP_CTRL_BUFFER_TYPE_LSB);
}

// Save buffer context for EPX preemption (called after STOP_TRANS).
// Undo PID toggle and buffer accounting for buffers NOT completed on the wire.
// A buffer completed on wire means: controller reached STATUS phase (ACK received).
//   OUT completed: FULL cleared to 0 in STATUS phase (was 1 when armed)
//   IN  completed: FULL set to 1 in STATUS phase (was 0 when armed)
// So undo when: AVAIL=1 (never started), or (OUT: FULL=1) or (IN: FULL=0)
static void __tusb_irq_path_func(epx_save_context)(hw_endpoint_t *ep) {
  uint32_t   buf_ctrl = usbh_dpram->epx_buf_ctrl;
  const bool is_out   = (tu_edpt_dir(ep->ep_addr) == TUSB_DIR_OUT);

  do {
    const uint16_t bc16 = (uint16_t)buf_ctrl;
    if (bc16) {
      const bool avail = (bc16 & USB_BUF_CTRL_AVAIL);
      const bool full  = (bc16 & USB_BUF_CTRL_FULL);
      if (avail || (is_out ? full : !full)) {
        const uint16_t buf_len = bc16 & USB_BUF_CTRL_LEN_MASK;
        ep->remaining_len += buf_len;
        ep->next_pid ^= 1u;
        if (is_out) {
          ep->user_buf -= buf_len;
        }
      }
    }

    if (usbh_dpram->epx_ctrl & EP_CTRL_DOUBLE_BUFFERED_BITS) {
      buf_ctrl >>= 16;
    } else {
      buf_ctrl = 0;
    }
  } while (buf_ctrl > 0);

  usbh_dpram->epx_buf_ctrl = 0;

  ep->state = EPSTATE_PENDING;
}

// switch epx to new endpoint and start the transfer
static void __tusb_irq_path_func(epx_switch_ep)(hw_endpoint_t *ep) {
  const bool is_setup = (ep->state == EPSTATE_PENDING_SETUP);

  epx       = ep; // switch pointer
  ep->state = EPSTATE_ACTIVE;

  if (is_setup) {
    // panic("new setup \n");
    usb_hw->dev_addr_ctrl = ep->dev_addr;
    sie_start_xfer(true, false, ep->need_pre);
  } else {
    const bool is_rx   = (tu_edpt_dir(ep->ep_addr) == TUSB_DIR_IN);
    io_rw_32  *ep_reg  = &usbh_dpram->epx_ctrl;
    io_rw_32 *buf_reg = &usbh_dpram->epx_buf_ctrl;

    epx_ctrl_prepare(ep->transfer_type);
    rp2usb_buffer_start(ep, ep_reg, buf_reg, is_rx);

    usb_hw->dev_addr_ctrl = (uint32_t)(ep->dev_addr | (tu_edpt_number(ep->ep_addr) << USB_ADDR_ENDP_ENDPOINT_LSB));
    sie_start_xfer(is_setup, is_rx, ep->need_pre);
  }
}

// Round-robin find next pending ep after current epx
static hw_endpoint_t *__tusb_irq_path_func(epx_next_pending)(hw_endpoint_t *cur_ep) {
  const uint cur_idx = (uint)(cur_ep - &ep_pool[0]);
  for (uint i = cur_idx + 1; i < TU_ARRAY_SIZE(ep_pool); i++) {
    if (ep_pool[i].state >= EPSTATE_PENDING) {
      return &ep_pool[i];
    }
  }
  for (uint i = 0; i < cur_idx; i++) {
    if (ep_pool[i].state >= EPSTATE_PENDING) {
      return &ep_pool[i];
    }
  }
  return NULL;
}


//--------------------------------------------------------------------+
// Interrupt handlers
//--------------------------------------------------------------------+
static void __tusb_irq_path_func(xfer_complete_isr)(hw_endpoint_t *ep, xfer_result_t xfer_result, bool is_more) {
  // Mark transfer as done before we tell the tinyusb stack
  uint32_t xferred_len = ep->xferred_len;
  rp2usb_reset_transfer(ep);
  hcd_event_xfer_complete(ep->dev_addr, ep->ep_addr, xferred_len, xfer_result, true);

  // Carry more transfer on epx
  if (is_more) {
    hw_endpoint_t *next_ep = epx_next_pending(epx);
    if (next_ep != NULL) {
      epx_switch_ep(next_ep);
    }
  }
}

static void __tusb_irq_path_func(handle_buf_status_isr)(void) {
  pico_trace("buf_status 0x%08lx\n", buf_status);
  enum {
    BUF_STATUS_EPX = 1u
  };

  // Check EPX first (bit 0).
  // Double-buffered: if both buffers completed at once, buf_status re-sets
  // immediately after clearing (datasheet Table 406). Process the second buffer too.
  while (usb_hw->buf_status & BUF_STATUS_EPX) {
    const uint8_t buf_id     = (usb_hw->buf_cpu_should_handle & BUF_STATUS_EPX) ? 1 : 0;
    usb_hw_clear->buf_status = 1u; // clear

    io_rw_32 *ep_reg  = &usbh_dpram->epx_ctrl;
    io_rw_32 *buf_reg = &usbh_dpram->epx_buf_ctrl;
  #ifndef HAS_STOP_EPX_ON_NAK
    // Any packet completion (mid-transfer or final) means data is flowing.
    // Clear switch request so the 2-SOF fallback only fires for NAK-retrying endpoints.
    epx_switch_request = false;
  #endif
    if (rp2usb_xfer_continue(epx, ep_reg, buf_reg, buf_id, tu_edpt_dir(epx->ep_addr) == TUSB_DIR_IN)) {
      xfer_complete_isr(epx, XFER_RESULT_SUCCESS, true);
    }
  }

  // Check "interrupt" (asynchronous) endpoints for both IN and OUT
  uint32_t buf_status = usb_hw->buf_status & ~(uint32_t)BUF_STATUS_EPX;
  while (buf_status) {
    // ctz/clz is faster than loop which has only a few bit set in general
    const uint8_t  idx       = (uint8_t)__builtin_ctz(buf_status);
    const uint32_t bit       = TU_BIT(idx);
    usb_hw_clear->buf_status = bit;
    buf_status &= ~bit;

    // IN transfer for even i, OUT transfer for odd i
    // EPX  is bit 0. Bit 1 is not used
    // IEP1 IN/OUT is bit 2, 3
    // IEP2 IN/OUT is bit 4, 5 etc
    const uint8_t epnum = idx >> 1u;
    for (size_t e = 0; e < TU_ARRAY_SIZE(ep_pool); e++) {
      hw_endpoint_t *ep = &ep_pool[e];
      if (ep->interrupt_num == epnum) {
        io_rw_32  *ep_reg  = dpram_int_ep_ctrl(ep->interrupt_num);
        io_rw_32  *buf_reg = dpram_int_ep_buffer_ctrl(ep->interrupt_num);
        const bool done    = rp2usb_xfer_continue(ep, ep_reg, buf_reg, 0, tu_edpt_dir(ep->ep_addr) == TUSB_DIR_IN);
        if (done) {
          xfer_complete_isr(ep, XFER_RESULT_SUCCESS, false);
        }
        break;
      }
    }
  }
}

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
    usb_hw_clear->sie_status = USB_SIE_STATUS_STALL_REC_BITS;
    xfer_complete_isr(epx, XFER_RESULT_STALLED, true);
  }

  if (status & USB_INTS_ERROR_RX_TIMEOUT_BITS) {
    usb_hw_clear->sie_status = USB_SIE_STATUS_RX_TIMEOUT_BITS;

    const uint32_t sie_ctrl = (usb_hw->sie_ctrl & SIE_CTRL_BASE_MASK) | USB_SIE_CTRL_STOP_TRANS_BITS;
    usb_hw->sie_ctrl        = sie_ctrl;
    // while (usb_hw->sie_ctrl & USB_SIE_CTRL_STOP_TRANS_BITS) {}

    // Even if STOP_TRANS bit is clear, controller maybe in middle of retrying and may re-raise timeout once extra time
    // Only handle if epx is active, don't carry more epx transfer since STOP_TRANS is raced and not safe.
    if (epx->state == EPSTATE_ACTIVE) {
      xfer_complete_isr(epx, XFER_RESULT_FAILED, false);
    }
  }

  if (status & USB_INTS_TRANS_COMPLETE_BITS) {
    // only applies for epx, interrupt endpoint does not seem to raise this
    usb_hw_clear->sie_status = USB_SIE_STATUS_TRANS_COMPLETE_BITS;
    if (usb_hw->sie_ctrl & USB_SIE_CTRL_SEND_SETUP_BITS) {
      uint32_t sie_ctrl = usb_hw->sie_ctrl & SIE_CTRL_BASE_MASK;
      usb_hw->sie_ctrl  = sie_ctrl; // clear setup bit
      epx->xferred_len  = 8;
      xfer_complete_isr(epx, XFER_RESULT_SUCCESS, true);
    }
  }

  if (status & USB_INTS_BUFF_STATUS_BITS) {
    handle_buf_status_isr();
  }

  // SOF-based round-robin MUST run BEFORE BUFF_STATUS to avoid processing
  // buf_status on the wrong EPX after a completion+switch in handle_buf_status_isr.
  #ifdef HAS_STOP_EPX_ON_NAK
  if (status & USB_INTS_EPX_STOPPED_ON_NAK_BITS) {
    usb_hw_clear->nak_poll = USB_NAK_POLL_EPX_STOPPED_ON_NAK_BITS;
    hw_endpoint_t *next_ep = epx_next_pending(epx);
    if (next_ep != NULL) {
      epx_save_context(epx);
      epx_switch_ep(next_ep);
    } else {
      usb_hw_clear->nak_poll = USB_NAK_POLL_STOP_EPX_ON_NAK_BITS;
      sie_start_xfer(false, TUSB_DIR_IN == tu_edpt_dir(epx->ep_addr), epx->need_pre);
    }
  }
  #else
  // RP2040: on SOF, switch EPX if another endpoint is pending.
  // First SOF sets epx_switch_request. If a transfer completes before next SOF, the flag is
  // cleared (data is flowing, no need to force-switch). Second SOF with flag still set means
  // no data exchanged (endpoint NAK-retrying): STOP_TRANS is safe and we switch.
  // This avoids stopping mid-data-transfer which corrupts double-buffered PID tracking.
  if (status & USB_INTS_HOST_SOF_BITS) {
    (void)usb_hw->sof_rd; // clear SOF by reading SOF_RD
    hw_endpoint_t *next_ep = epx_next_pending(epx);
    if (next_ep == NULL) {
      usb_hw_clear->inte = USB_INTE_HOST_SOF_BITS;
      usb_hw->nak_poll   = USB_NAK_POLL_RESET;
      epx_switch_request = false;
    } else if (epx->state == EPSTATE_ACTIVE) {
      if (epx_switch_request) {
        // Second SOF with no transfer completion: endpoint is NAK-retrying, safe to switch.
        epx_switch_request = false;
        sie_stop_xfer();
        epx_save_context(epx);
        epx_switch_ep(next_ep);
      } else {
        epx_switch_request = true;
      }
    }
  }
  #endif

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

//--------------------------------------------------------------------+
// HCD API
//--------------------------------------------------------------------+
bool hcd_init(uint8_t rhport, const tusb_rhport_init_t *rh_init) {
  (void)rhport;
  (void)rh_init;
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
  usb_hw->sie_ctrl  = SIE_CTRL_BASE;
  usb_hw->inte      = USB_INTE_BUFF_STATUS_BITS | USB_INTE_HOST_CONN_DIS_BITS | USB_INTE_HOST_RESUME_BITS |
                 USB_INTE_STALL_BITS | USB_INTE_TRANS_COMPLETE_BITS | USB_INTE_ERROR_RX_TIMEOUT_BITS |
                 USB_INTE_ERROR_DATA_SEQ_BITS;

  #ifdef HAS_STOP_EPX_ON_NAK
  usb_hw_set->inte = USB_INTE_EPX_STOPPED_ON_NAK_BITS;
  #endif

  return true;
}

bool hcd_deinit(uint8_t rhport) {
  (void)rhport;
  irq_remove_handler(USBCTRL_IRQ, hcd_rp2040_irq);
  reset_block(RESETS_RESET_USBCTRL_BITS);
  unreset_block_wait(RESETS_RESET_USBCTRL_BITS);
  return true;
}

void hcd_port_reset(uint8_t rhport) {
  (void)rhport;
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

  if (dev_addr == 0) {
    return; // address 0 is for device enumeration
  }

  rp2usb_critical_enter();

  for (size_t i = 0; i < TU_ARRAY_SIZE(ep_pool); i++) {
    hw_endpoint_t *ep = &ep_pool[i];
    if (ep->dev_addr == dev_addr && ep->max_packet_size > 0) {
      ep->state = EPSTATE_IDLE; // clear any pending transfer

      if (ep->interrupt_num > 0) {
        // disable interrupt endpoint
        usb_hw_clear->int_ep_ctrl                       = TU_BIT(ep->interrupt_num);
        usb_hw->int_ep_addr_ctrl[ep->interrupt_num - 1] = 0;

        io_rw_32 *ep_reg  = dpram_int_ep_ctrl(ep->interrupt_num);
        io_rw_32 *buf_reg = dpram_int_ep_buffer_ctrl(ep->interrupt_num);
        *buf_reg          = 0;
        *ep_reg           = 0;
      }

      ep->max_packet_size = 0; // mark as unused
    }
  }

  rp2usb_critical_exit();
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
  hw_endpoint_t *ep;
  if (dev_addr == 0) {
    ep = &ep_pool[0];
  } else {
    ep = edpt_alloc();
  }
  TU_ASSERT(ep);

  const uint8_t  ep_addr         = ep_desc->bEndpointAddress;
  const uint16_t max_packet_size = tu_edpt_packet_size(ep_desc);

  ep->max_packet_size = max_packet_size;
  ep->ep_addr         = ep_addr;
  ep->dev_addr        = dev_addr;
  ep->transfer_type   = ep_desc->bmAttributes.xfer;
  ep->need_pre        = need_pre(dev_addr);
  ep->next_pid        = 0u;

  if (ep->transfer_type != TUSB_XFER_INTERRUPT) {
    ep->dpram_buf = usbh_dpram->epx_data;
  } else {
    // from 15 interrupt endpoints pool
    uint8_t int_idx;
    for (int_idx = 0; int_idx < USB_HOST_INTERRUPT_ENDPOINTS; int_idx++) {
      if (!tu_bit_test(usb_hw->int_ep_ctrl, 1 + int_idx)) {
        ep->interrupt_num = int_idx + 1;
        break;
      }
    }
    assert(int_idx < USB_HOST_INTERRUPT_ENDPOINTS);
    assert(ep_desc->bInterval > 0);

    //------------- dpram buf -------------//
    // 15x64 last bytes of DPRAM for interrupt endpoint buffers
    ep->dpram_buf    = (uint8_t *)(USBCTRL_DPRAM_BASE + USB_DPRAM_MAX - (int_idx + 1u) * 64u);
    uint32_t ep_ctrl = EP_CTRL_ENABLE_BITS | EP_CTRL_INTERRUPT_PER_BUFFER |
                       (TUSB_XFER_INTERRUPT << EP_CTRL_BUFFER_TYPE_LSB) | hw_data_offset(ep->dpram_buf) |
                       ((uint32_t)(ep_desc->bInterval - 1) << EP_CTRL_HOST_INTERRUPT_INTERVAL_LSB);
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
    usb_hw_set->int_ep_ctrl = TU_BIT(ep->interrupt_num);
  }

  return true;
}

bool hcd_edpt_close(uint8_t rhport, uint8_t daddr, uint8_t ep_addr) {
  (void)rhport;
  (void)daddr;
  (void)ep_addr;
  return false; // TODO not implemented yet
}

bool hcd_edpt_abort_xfer(uint8_t rhport, uint8_t dev_addr, uint8_t ep_addr) {
  (void)rhport;
  (void)dev_addr;
  (void)ep_addr;
  // TODO not implemented yet
  return false;
}

bool hcd_edpt_xfer(uint8_t rhport, uint8_t dev_addr, uint8_t ep_addr, uint8_t *buffer, uint16_t buflen) {
  (void)rhport;

  hw_endpoint_t *ep = edpt_find(dev_addr, ep_addr);
  TU_ASSERT(ep);

  if (ep->interrupt_num > 0) {
    // For interrupt endpoint control and buffer is already configured
    // Note: Interrupt is single buffered only
    io_rw_32 *ep_reg  = dpram_int_ep_ctrl(ep->interrupt_num);
    io_rw_32 *buf_reg = dpram_int_ep_buffer_ctrl(ep->interrupt_num);
    rp2usb_xfer_start(ep, ep_reg, buf_reg, buffer, NULL, buflen);
  } else {
    // Control endpoint can change direction 0x00 <-> 0x80 when changing stages
    if (ep_addr != ep->ep_addr) {
      ep->ep_addr  = ep_addr;
      ep->next_pid = 1; // data and status stage start with DATA1
    }

    // If EPX is busy with another transfer, mark as pending
    rp2usb_critical_enter();
    if (epx->state == EPSTATE_ACTIVE) {
      ep->user_buf      = buffer;
      ep->remaining_len = buflen;
      ep->state         = EPSTATE_PENDING;

  #ifdef HAS_STOP_EPX_ON_NAK
      usb_hw_set->nak_poll = USB_NAK_POLL_STOP_EPX_ON_NAK_BITS;
  #else
      // Only enable SOF round-robin for non-control endpoints
      usb_hw->nak_poll = (300 << USB_NAK_POLL_DELAY_FS_LSB) | (300 << USB_NAK_POLL_DELAY_LS_LSB);
      usb_hw_set->inte = USB_INTE_HOST_SOF_BITS;
  #endif
    } else {
      io_rw_32 *ep_reg  = &usbh_dpram->epx_ctrl;
      io_rw_32 *buf_reg = &usbh_dpram->epx_buf_ctrl;

      epx = ep;

      epx_ctrl_prepare(ep->transfer_type);
      rp2usb_xfer_start(ep, ep_reg, buf_reg, buffer, NULL, buflen); // prepare bufctrl
      usb_hw->dev_addr_ctrl = (uint32_t)(ep->dev_addr | (tu_edpt_number(ep->ep_addr) << USB_ADDR_ENDP_ENDPOINT_LSB));
      sie_start_xfer(false, tu_edpt_dir(ep->ep_addr) == TUSB_DIR_IN, ep->need_pre);
    }
    rp2usb_critical_exit();
  }

  return true;
}

bool hcd_setup_send(uint8_t rhport, uint8_t dev_addr, const uint8_t setup_packet[8]) {
  (void)rhport;

  hw_endpoint_t *ep = edpt_find(dev_addr, 0x00);
  TU_ASSERT(ep);

  rp2usb_critical_enter();

  // Copy data into setup packet buffer (usbh only schedules one setup at a time)
  for (uint8_t i = 0; i < 8; i++) {
    usbh_dpram->setup_packet[i] = setup_packet[i];
  }

  ep->ep_addr       = 0; // setup is OUT
  ep->remaining_len = 8;
  ep->xferred_len   = 0;

  // If EPX is busy, mark as pending setup (DPRAM already has the packet)
  if (epx->state == EPSTATE_ACTIVE) {
    ep->state = EPSTATE_PENDING_SETUP;
  #ifdef HAS_STOP_EPX_ON_NAK
    usb_hw_set->nak_poll = USB_NAK_POLL_STOP_EPX_ON_NAK_BITS;
  #else
    usb_hw->nak_poll = (300 << USB_NAK_POLL_DELAY_FS_LSB) | (300 << USB_NAK_POLL_DELAY_LS_LSB);
    usb_hw_set->inte = USB_INTE_HOST_SOF_BITS;
  #endif
  } else {
    epx       = ep;
    ep->state = EPSTATE_ACTIVE;

    usb_hw->dev_addr_ctrl = ep->dev_addr;
    sie_start_xfer(true, tu_edpt_dir(ep->ep_addr) == TUSB_DIR_IN, ep->need_pre);
  }

  rp2usb_critical_exit();
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
