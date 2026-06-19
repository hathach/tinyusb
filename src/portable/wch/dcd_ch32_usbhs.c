/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2022 Greg Davill
 * Copyright (c) 2023 Denis Krasutski
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

#if CFG_TUD_ENABLED && defined(TUP_USBIP_WCH_USBHS) && defined(CFG_TUD_WCH_USBIP_USBHS) && \
  (CFG_TUD_WCH_USBIP_USBHS == 1)
  #include "ch32_usbhs_reg.h"

  #include "device/dcd.h"

  #if CFG_TUSB_MCU == OPT_MCU_CH32H417
    #include "usbhs_h41x.h"
  #else
    #include "usbhs_f20x_v30x.h"
  #endif

  // Max number of bi-directional endpoints including EP0
  #define EP_MAX TUP_DCD_ENDPOINT_MAX

typedef struct {
  uint8_t *buffer;
  uint16_t total_len;
  uint16_t queued_len;
  uint16_t max_size;
  bool     is_iso;
  bool     valid;
} xfer_ctl_t;

  #define XFER_CTL_BASE(_ep, _dir) &xfer_status[_ep][_dir]
static xfer_ctl_t xfer_status[EP_MAX][2];

/* Endpoint Buffer */
TU_ATTR_ALIGNED(4) static uint8_t ep0_buffer[CFG_TUD_ENDPOINT0_SIZE];
static bool ep0_tog;
static bool ep_data_tog[EP_MAX][2];

static void set_ep_toggle(uint8_t ep_num, tusb_dir_t ep_dir, bool data1) {
  if (ep_dir == TUSB_DIR_IN) {
    EP_TX_CTRL(ep_num) = (EP_TX_CTRL(ep_num) & ~(USBHS_EP_T_TOG_MASK)) |
                         (data1 ? USBHS_EP_T_TOG_1 : USBHS_EP_T_TOG_0);
  } else {
    EP_RX_CTRL(ep_num) = (EP_RX_CTRL(ep_num) & ~(USBHS_EP_R_TOG_MASK)) |
                         (data1 ? USBHS_EP_R_TOG_1 : USBHS_EP_R_TOG_0);
  }
}

static void queue_in_packet(uint8_t ep_num, xfer_ctl_t* xfer) {
  uint16_t remaining = xfer->total_len - xfer->queued_len;
  uint16_t tx_len = TU_MIN(remaining, xfer->max_size);

  if (ep_num == 0) {
    memcpy(ep0_buffer, &xfer->buffer[xfer->queued_len], tx_len);
  } else {
    EP_TX_DMA_ADDR(ep_num) = (uint32_t) &xfer->buffer[xfer->queued_len];
  }

  EP_TX_LEN(ep_num) = tx_len;
  xfer->queued_len += tx_len;

  if (ep_num == 0) {
    EP_TX_CTRL(0) = USBHS_EP_T_RES_ACK | (ep0_tog ? USBHS_EP_T_TOG_1 : USBHS_EP_T_TOG_0);
    ep0_tog = !ep0_tog;
  } else if (xfer->is_iso) {
    wch_usbhs_edpt_enable_iso_in(ep_num);
    EP_TX_CTRL(ep_num) = (EP_TX_CTRL(ep_num) & ~(USBHS_EP_T_RES_MASK)) | USBHS_EP_T_RES_NYET;
  } else {
    set_ep_toggle(ep_num, TUSB_DIR_IN, ep_data_tog[ep_num][TUSB_DIR_IN]);
    EP_TX_CTRL(ep_num) = (EP_TX_CTRL(ep_num) & ~(USBHS_EP_T_RES_MASK)) | USBHS_EP_T_RES_ACK;
  }
}

static void queue_out_packet(uint8_t ep_num, xfer_ctl_t* xfer) {
  uint16_t remaining = xfer->total_len - xfer->queued_len;
  uint16_t rx_len = TU_MIN(remaining, xfer->max_size);

  if (ep_num > 0) {
    EP_RX_DMA_ADDR(ep_num) = (uint32_t) &xfer->buffer[xfer->queued_len];
    EP_RX_MAX_LEN(ep_num) = rx_len;
  }

  if (ep_num == 0) {
    EP_RX_CTRL(0) = (EP_RX_CTRL(0) & ~(USBHS_EP_R_RES_MASK)) | USBHS_EP_R_RES_ACK;
  } else if (xfer->is_iso) {
    EP_RX_CTRL(ep_num) = (EP_RX_CTRL(ep_num) & ~(USBHS_EP_R_RES_MASK)) | USBHS_EP_R_RES_NYET;
  } else {
    set_ep_toggle(ep_num, TUSB_DIR_OUT, ep_data_tog[ep_num][TUSB_DIR_OUT]);
    EP_RX_CTRL(ep_num) = (EP_RX_CTRL(ep_num) & ~(USBHS_EP_R_RES_MASK)) | USBHS_EP_R_RES_ACK;
  }
}

static void update_in(uint8_t rhport, uint8_t ep_num, bool force) {
  xfer_ctl_t* xfer = XFER_CTL_BASE(ep_num, TUSB_DIR_IN);
  if (!xfer->valid) {
    return;
  }

  if (!force && ep_num != 0 && !xfer->is_iso) {
    ep_data_tog[ep_num][TUSB_DIR_IN] = !ep_data_tog[ep_num][TUSB_DIR_IN];
  }

  if (force || (xfer->total_len > xfer->queued_len)) {
    queue_in_packet(ep_num, xfer);
  } else {
    xfer->valid = false;
    if (ep_num == 0) {
      EP_TX_CTRL(0) = USBHS_EP_T_RES_NAK | (ep0_tog ? USBHS_EP_T_TOG_1 : USBHS_EP_T_TOG_0);
    } else {
      if (xfer->is_iso) {
        wch_usbhs_edpt_disable_iso_in(ep_num);
      }
      EP_TX_CTRL(ep_num) = (EP_TX_CTRL(ep_num) & ~(USBHS_EP_T_RES_MASK)) | USBHS_EP_T_RES_NAK;
    }
    dcd_event_xfer_complete(rhport, ep_num | TUSB_DIR_IN_MASK, xfer->queued_len, XFER_RESULT_SUCCESS, true);
  }
}

static void update_out(uint8_t rhport, uint8_t ep_num, uint16_t rx_len) {
  xfer_ctl_t* xfer = XFER_CTL_BASE(ep_num, TUSB_DIR_OUT);
  if (!xfer->valid) {
    return;
  }

  uint16_t remaining = xfer->total_len - xfer->queued_len;
  uint16_t len = TU_MIN(rx_len, TU_MIN(remaining, xfer->max_size));

  if (ep_num == 0) {
    memcpy(&xfer->buffer[xfer->queued_len], ep0_buffer, len);
  }

  xfer->queued_len += len;

  if (ep_num != 0 && !xfer->is_iso) {
    ep_data_tog[ep_num][TUSB_DIR_OUT] = !ep_data_tog[ep_num][TUSB_DIR_OUT];
  }

  if ((xfer->queued_len == xfer->total_len) || (len < xfer->max_size)) {
    xfer->valid = false;
    if (ep_num == 0) {
      EP_RX_CTRL(0) = (EP_RX_CTRL(0) & ~(USBHS_EP_R_RES_MASK)) | USBHS_EP_R_RES_NAK;
    }
    dcd_event_xfer_complete(rhport, ep_num, xfer->queued_len, XFER_RESULT_SUCCESS, true);
  }

  if (ep_num != 0) {
    if (xfer->valid) {
      queue_out_packet(ep_num, xfer);
    } else {
      uint8_t rx_res = xfer->is_iso ? USBHS_EP_R_RES_NYET : USBHS_EP_R_RES_NAK;
      EP_RX_CTRL(ep_num) = (EP_RX_CTRL(ep_num) & ~(USBHS_EP_R_RES_MASK)) | rx_res;
    }
  }
}

bool dcd_init(uint8_t rhport, const tusb_rhport_init_t *rh_init) {
  (void)rhport;
  (void)rh_init;

  memset(&xfer_status, 0, sizeof(xfer_status));
  memset(ep_data_tog, 0, sizeof(ep_data_tog));
  ep0_tog = true;

  wch_usbhs_dcd_hw_init();

  for (int ep = 0; ep < EP_MAX; ep++) {
    EP_TX_LEN(ep)  = 0;
    EP_TX_CTRL(ep) = USBHS_EP_T_RES_NAK | USBHS_EP_T_TOG_0;
    EP_RX_CTRL(ep) = USBHS_EP_R_RES_NAK | USBHS_EP_R_TOG_0;

    EP_RX_MAX_LEN(ep) = 0;
  }

  USBHSD->UEP0_DMA                      = (uint32_t)ep0_buffer;
  USBHSD->UEP0_MAX_LEN                  = CFG_TUD_ENDPOINT0_SIZE;
  xfer_status[0][TUSB_DIR_OUT].max_size = CFG_TUD_ENDPOINT0_SIZE;
  xfer_status[0][TUSB_DIR_IN].max_size  = CFG_TUD_ENDPOINT0_SIZE;

  USBHSD->DEV_AD = 0;
  wch_usbhs_dcd_connect();

  return true;
}

void dcd_int_enable(uint8_t rhport) {
  (void)rhport;
  NVIC_EnableIRQ(USBHS_IRQn);
}

void dcd_int_disable(uint8_t rhport) {
  (void)rhport;
  NVIC_DisableIRQ(USBHS_IRQn);
}

void dcd_edpt_close_all(uint8_t rhport) {
  (void)rhport;

  memset(ep_data_tog, 0, sizeof(ep_data_tog));

  for (size_t ep = 1; ep < EP_MAX; ep++) {
    EP_TX_LEN(ep)  = 0;
    EP_TX_CTRL(ep) = USBHS_EP_T_RES_NAK | USBHS_EP_T_TOG_0;
    EP_RX_CTRL(ep) = USBHS_EP_R_RES_NAK | USBHS_EP_R_TOG_0;

    EP_RX_MAX_LEN(ep) = 0;
  }

  wch_usbhs_edpt_close_all();
}

void dcd_set_address(uint8_t rhport, uint8_t dev_addr) {
  (void)dev_addr;

  // Response with zlp status
  dcd_edpt_xfer(rhport, 0x80, NULL, 0, false);
}

void dcd_remote_wakeup(uint8_t rhport) {
  (void)rhport;
}

void dcd_sof_enable(uint8_t rhport, bool en) {
  (void)rhport;
  wch_usbhs_dcd_sof_enable(en);
}

void dcd_edpt0_status_complete(uint8_t rhport, const tusb_control_request_t *request) {
  (void)rhport;
  if (request->bmRequestType_bit.recipient == TUSB_REQ_RCPT_DEVICE &&
      request->bmRequestType_bit.type == TUSB_REQ_TYPE_STANDARD && request->bRequest == TUSB_REQ_SET_ADDRESS) {
    USBHSD->DEV_AD = (uint8_t)request->wValue;
  }
}

bool dcd_edpt_open(uint8_t rhport, const tusb_desc_endpoint_t *desc_edpt) {
  (void)rhport;

  const uint8_t    ep_num = tu_edpt_number(desc_edpt->bEndpointAddress);
  const tusb_dir_t dir    = tu_edpt_dir(desc_edpt->bEndpointAddress);

  TU_ASSERT(ep_num < EP_MAX);

  if (ep_num == 0) {
    return true;
  }

  xfer_ctl_t *xfer = XFER_CTL_BASE(ep_num, dir);
  xfer->max_size   = tu_edpt_packet_size(desc_edpt);
  ep_data_tog[ep_num][dir] = false;

  xfer->is_iso = (desc_edpt->bmAttributes.xfer == TUSB_XFER_ISOCHRONOUS);
  if (dir == TUSB_DIR_OUT) {
    wch_usbhs_edpt_enable(ep_num, dir, xfer->is_iso);
    EP_RX_CTRL(ep_num) = USBHS_EP_R_RES_NAK | USBHS_EP_R_TOG_0;
    EP_RX_MAX_LEN(ep_num) = xfer->max_size;
  } else {
    wch_usbhs_edpt_enable(ep_num, dir, xfer->is_iso);
    EP_TX_LEN(ep_num)  = 0;
    EP_TX_CTRL(ep_num) = USBHS_EP_T_RES_NAK | USBHS_EP_T_TOG_0;
  }

  return true;
}

void dcd_edpt_close(uint8_t rhport, uint8_t ep_addr) {
  (void)rhport;

  const uint8_t    ep_num = tu_edpt_number(ep_addr);
  const tusb_dir_t dir    = tu_edpt_dir(ep_addr);

  if (dir == TUSB_DIR_OUT) {
    EP_RX_CTRL(ep_num)    = USBHS_EP_R_RES_NAK | USBHS_EP_R_TOG_0;
    EP_RX_MAX_LEN(ep_num) = 0;
    ep_data_tog[ep_num][TUSB_DIR_OUT] = false;
  } else { // TUSB_DIR_IN
    EP_TX_CTRL(ep_num) = USBHS_EP_T_RES_NAK | USBHS_EP_T_TOG_0;
    EP_TX_LEN(ep_num)  = 0;
    ep_data_tog[ep_num][TUSB_DIR_IN] = false;
  }
  wch_usbhs_edpt_disable(ep_num, dir);
}

  #if 0
bool dcd_edpt_iso_alloc(uint8_t rhport, uint8_t ep_addr, uint16_t largest_packet_size) {
  (void) rhport;
  (void) ep_addr;
  (void) largest_packet_size;
  return false;
}

bool dcd_edpt_iso_activate(uint8_t rhport, tusb_desc_endpoint_t const * desc_ep) {
  (void) rhport;
  (void) desc_ep;
  return false;
}
  #endif

void dcd_edpt_stall(uint8_t rhport, uint8_t ep_addr) {
  (void)rhport;

  const uint8_t    ep_num = tu_edpt_number(ep_addr);
  const tusb_dir_t dir    = tu_edpt_dir(ep_addr);

  if (dir == TUSB_DIR_OUT) {
    EP_RX_CTRL(ep_num) = USBHS_EP_R_RES_STALL;
  } else {
    EP_TX_LEN(ep_num) = 0;
    EP_TX_CTRL(ep_num) = USBHS_EP_T_RES_STALL;
  }
}

void dcd_edpt_clear_stall(uint8_t rhport, uint8_t ep_addr) {
  (void)rhport;

  const uint8_t    ep_num = tu_edpt_number(ep_addr);
  const tusb_dir_t dir    = tu_edpt_dir(ep_addr);

  if (dir == TUSB_DIR_OUT) {
    EP_RX_CTRL(ep_num) = USBHS_EP_R_RES_NAK | USBHS_EP_R_TOG_0;
    ep_data_tog[ep_num][TUSB_DIR_OUT] = false;
  } else {
    EP_TX_CTRL(ep_num) = USBHS_EP_T_RES_NAK | USBHS_EP_T_TOG_0;
    ep_data_tog[ep_num][TUSB_DIR_IN] = false;
  }
}

bool dcd_edpt_xfer(uint8_t rhport, uint8_t ep_addr, uint8_t *buffer, uint16_t total_bytes, bool is_isr) {
  (void)is_isr;
  (void)rhport;
  const uint8_t    ep_num = tu_edpt_number(ep_addr);
  const tusb_dir_t dir    = tu_edpt_dir(ep_addr);

  xfer_ctl_t *xfer     = XFER_CTL_BASE(ep_num, dir);
  xfer->buffer         = buffer;
  xfer->total_len      = total_bytes;
  xfer->queued_len     = 0;
  xfer->valid         = true;

  if (ep_num == 0 && dir == TUSB_DIR_OUT) {
    if (total_bytes == 0) {
      EP_RX_CTRL(0) = (EP_RX_CTRL(0) & ~(USBHS_EP_R_TOG_MASK)) | USBHS_EP_R_TOG_1;
    } else {
      EP_RX_CTRL(0) ^= USBHS_EP_R_TOG_1;
    }
  }

  if (dir == TUSB_DIR_IN) {
    update_in(rhport, ep_num, true);
  } else {
    queue_out_packet(ep_num, xfer);
  }

  return true;
}

void dcd_int_handler(uint8_t rhport) {
  (void)rhport;

  uint8_t int_flag   = USBHSD->INT_FG;
  uint8_t int_status = USBHSD->INT_ST;

  #if CFG_TUSB_MCU == OPT_MCU_CH32H417
  if (int_flag & USBHS_UDIF_TRANSFER) {
    uint8_t const ep_num = int_status & USBHS_UDIS_EP_ID_MASK;
    tusb_dir_t const ep_dir = (int_status & USBHS_UDIS_EP_DIR) ? TUSB_DIR_IN : TUSB_DIR_OUT;

    if (ep_dir == TUSB_DIR_OUT) {
      if (wch_usbhs_edpt_setup_received(ep_num)) {
        tusb_control_request_t const* setup =
            (tusb_control_request_t const*) ep0_buffer;
        wch_usbhs_edpt_rx_done_clear(0);
        ep0_tog = true;
        EP_RX_CTRL(0) = (setup->wLength == 0) ? USBHS_EP_R_RES_ACK : USBHS_EP_R_RES_NAK;
        EP_TX_CTRL(0) = USBHS_EP_T_RES_NAK;
        dcd_event_setup_received(rhport, ep0_buffer, true);

        USBHSD->INT_FG = USBHS_UDIF_TRANSFER;
        return;
      }

      uint16_t rx_len = wch_usbhs_edpt_rx_len(ep_num);
      wch_usbhs_edpt_rx_done_clear(ep_num);
      update_out(rhport, ep_num, rx_len);
    } else {
      wch_usbhs_edpt_tx_done_clear(ep_num);
      update_in(rhport, ep_num, false);
    }

    USBHSD->INT_FG = USBHS_UDIF_TRANSFER;
  } else if (int_flag & USBHS_UDIF_RX_SOF) {
    uint32_t frame_count = USBHSD->FRAME_NO & USBHS_UD_FRAME_NO;
    dcd_event_sof(rhport, frame_count, true);
    USBHSD->INT_FG = USBHS_UDIF_RX_SOF;
  } else if (int_flag & USBHS_UDIF_BUS_RST) {
    dcd_event_bus_reset(rhport, TUSB_SPEED_HIGH, true);

    USBHSD->DEV_AD = 0;
    memset(ep_data_tog, 0, sizeof(ep_data_tog));
    ep0_tog = true;
    EP_RX_CTRL(0) = USBHS_EP_R_RES_ACK | USBHS_EP_R_TOG_0;
    EP_TX_CTRL(0) = USBHS_EP_T_RES_NAK | USBHS_EP_T_TOG_0;

    USBHSD->INT_FG = USBHS_UDIF_BUS_RST;
  } else if (int_flag & (USBHS_UDIF_SUSPEND | USBHS_UDIF_BUS_SLEEP)) {
    dcd_event_t event = {.rhport = rhport, .event_id = DCD_EVENT_SUSPEND};
    dcd_event_handler(&event, true);

    USBHSD->INT_FG = int_flag & (USBHS_UDIF_SUSPEND | USBHS_UDIF_BUS_SLEEP);
  } else if (int_flag & USBHS_UDIF_LINK_RDY) {
    USBHSD->INT_FG = USBHS_UDIF_LINK_RDY;
  } else {
    USBHSD->INT_FG = int_flag;
  }
  #else
  if (int_flag & USBHS_TRANSFER_FLAG) {
    const uint8_t token = int_status & MASK_UIS_TOKEN;
    const uint8_t ep_num = int_status & MASK_UIS_ENDP;
    const uint16_t len = USBHSD->RX_LEN;

    if (token == USBHS_TOKEN_PID_SOF) {
      uint32_t frame_count = USBHSD->FRAME_NO & USBHS_FRAME_NO_NUM_MASK;
      dcd_event_sof(rhport, frame_count, true);
    } else if (token == USBHS_TOKEN_PID_OUT) {
      update_out(rhport, ep_num, len);
    } else if (token == USBHS_TOKEN_PID_IN) {
      update_in(rhport, ep_num, false);
    }
    USBHSD->INT_FG = (int_flag & USBHS_TRANSFER_FLAG); /* Clear flag */
  } else if (int_flag & USBHS_SETUP_FLAG) {
    tusb_control_request_t const* setup =
        (tusb_control_request_t const*) ep0_buffer;
    ep0_tog = true;
    EP_RX_CTRL(0) = (setup->wLength == 0) ? USBHS_EP_R_RES_ACK : USBHS_EP_R_RES_NAK;
    EP_TX_CTRL(0) = USBHS_EP_T_RES_NAK;

    dcd_event_setup_received(0, ep0_buffer, true);

    USBHSD->INT_FG = USBHS_SETUP_FLAG; /* Clear flag */
  } else if (int_flag & USBHS_BUS_RST_FLAG) {
    // TODO CH32 does not detect actual speed at this time (should be known at end of reset)
    // This interrupt probably triggered at start of bus reset
    //    tusb_speed_t actual_speed;
    //    switch(USBHSD->SPEED_TYPE & USBHS_SPEED_TYPE_MASK){
    //      case USBHS_SPEED_TYPE_HIGH:
    //        actual_speed = TUSB_SPEED_HIGH;
    //        break;
    //      case USBHS_SPEED_TYPE_FULL:
    //        actual_speed = TUSB_SPEED_FULL;
    //        break;
    //      case USBHS_SPEED_TYPE_LOW:
    //        actual_speed = TUSB_SPEED_LOW;
    //        break;
    //      default:
    //        TU_ASSERT(0,);
    //        break;
    //    }
    //    dcd_event_bus_reset(0, actual_speed, true);

    dcd_event_bus_reset(0, TUSB_SPEED_HIGH, true);

    USBHSD->DEV_AD = 0;
    memset(ep_data_tog, 0, sizeof(ep_data_tog));
    ep0_tog = true;
    EP_RX_CTRL(0)  = USBHS_EP_R_RES_ACK | USBHS_EP_R_TOG_0;
    EP_TX_CTRL(0)  = USBHS_EP_T_RES_NAK | USBHS_EP_T_TOG_0;

    USBHSD->INT_FG = USBHS_BUS_RST_FLAG; /* Clear flag */
  } else if (int_flag & USBHS_SUSPEND_FLAG) {
    dcd_event_t event = {.rhport = rhport, .event_id = DCD_EVENT_SUSPEND};
    dcd_event_handler(&event, true);

    USBHSD->INT_FG = USBHS_SUSPEND_FLAG; /* Clear flag */
  } else {
    // Unhandled interrupt
    USBHSD->INT_FG = int_flag; /* Clear all flags */
  }
  #endif
}
#endif
