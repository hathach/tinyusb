/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2026, TinyUSB contributors
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

#ifndef DCD_CH32_USBHS_H41X_H_
#define DCD_CH32_USBHS_H41X_H_

#define USBHS_EP_T_RES_MASK  USBHS_UEP_T_RES_MASK
#define USBHS_EP_T_RES_ACK   USBHS_UEP_T_RES_ACK
#ifdef USBHS_UEP_T_RES_NYET
#define USBHS_EP_T_RES_NYET  USBHS_UEP_T_RES_NYET
#else
#define USBHS_EP_T_RES_NYET  (1 << 0)
#endif
#define USBHS_EP_T_RES_NAK   USBHS_UEP_T_RES_NAK
#define USBHS_EP_T_RES_STALL USBHS_UEP_T_RES_STALL
#ifdef USBHS_UEP_T_TOG_MASK
#define USBHS_EP_T_TOG_MASK  USBHS_UEP_T_TOG_MASK
#else
#define USBHS_EP_T_TOG_MASK  (3 << 3)
#endif
#define USBHS_EP_T_TOG_0     USBHS_UEP_T_TOG_DATA0
#define USBHS_EP_T_TOG_1     USBHS_UEP_T_TOG_DATA1
#define USBHS_EP_T_AUTOTOG   0

#define USBHS_EP_R_RES_MASK  USBHS_UEP_R_RES_MASK
#define USBHS_EP_R_RES_ACK   USBHS_UEP_R_RES_ACK
#ifdef USBHS_UEP_R_RES_NYET
#define USBHS_EP_R_RES_NYET  USBHS_UEP_R_RES_NYET
#else
#define USBHS_EP_R_RES_NYET  (1 << 0)
#endif
#define USBHS_EP_R_RES_NAK   USBHS_UEP_R_RES_NAK
#define USBHS_EP_R_RES_STALL USBHS_UEP_R_RES_STALL
#ifdef USBHS_UEP_R_TOG_MASK
#define USBHS_EP_R_TOG_MASK  USBHS_UEP_R_TOG_MASK
#else
#define USBHS_EP_R_TOG_MASK  (3 << 3)
#endif
#define USBHS_EP_R_TOG_0     USBHS_UEP_R_TOG_DATA0
#define USBHS_EP_R_TOG_1     USBHS_UEP_R_TOG_DATA1
#define USBHS_EP_R_AUTOTOG   0

#define EP_TX_LEN(ep)     *(volatile uint16_t *)((volatile uint16_t *)&(USBHSD->UEP0_TX_LEN) + (ep) * 2)
#define EP_TX_CTRL(ep)    *(volatile uint8_t *)((volatile uint8_t *)&(USBHSD->UEP0_TX_CTRL) + (ep) * 4)
#define EP_RX_CTRL(ep)    *(volatile uint8_t *)((volatile uint8_t *)&(USBHSD->UEP0_RX_CTRL) + (ep) * 4)
#define EP_RX_MAX_LEN(ep) *(volatile uint16_t *)((volatile uint16_t *)&(USBHSD->UEP0_MAX_LEN) + (ep) * 2)
#define EP_RX_LEN(ep)     *(volatile uint16_t *)((volatile uint16_t *)&(USBHSD->UEP0_RX_LEN) + (ep) * 2)

#define EP_TX_DMA_ADDR(ep) *(volatile uint32_t *)((volatile uint32_t *)&(USBHSD->UEP1_TX_DMA) + (ep - 1))
#define EP_RX_DMA_ADDR(ep) *(volatile uint32_t *)((volatile uint32_t *)&(USBHSD->UEP1_RX_DMA) + (ep - 1))

static inline void wch_usbhs_dcd_hw_init(void) {
  USBHSD->CONTROL = USBHS_UD_RST_LINK | USBHS_UD_PHY_SUSPENDM;
  USBHSD->INT_EN = 0;
  USBHSD->INT_EN = USBHS_UDIE_BUS_RST | USBHS_UDIE_SUSPEND | USBHS_UDIE_BUS_SLEEP |
                   USBHS_UDIE_TRANSFER | USBHS_UDIE_LINK_RDY;

  USBHSD->UEP_TX_EN = USBHS_UEP0_T_EN;
  USBHSD->UEP_RX_EN = USBHS_UEP0_R_EN;
  USBHSD->UEP_TX_ISO = 0;
  USBHSD->UEP_RX_ISO = 0;
  USBHSD->UEP_TX_TOG_AUTO = 0xFE;
  USBHSD->UEP_RX_TOG_AUTO = 0xFE;
  USBHSD->UEP_TX_BURST = 0;
  USBHSD->UEP_TX_BURST_MODE = 0;
  USBHSD->UEP_RX_BURST = 0;
  USBHSD->UEP_RX_RES_MODE = 0;
  USBHSD->UEP_AF_MODE = 0;
}

static inline void wch_usbhs_dcd_connect(void) {
  USBHSD->BASE_MODE = USBHS_UD_SPEED_HIGH;
  USBHSD->CONTROL = USBHS_UD_DEV_EN | USBHS_UD_DMA_EN | USBHS_UD_LPM_EN | USBHS_UD_PHY_SUSPENDM;
}

static inline void wch_usbhs_dcd_sof_enable(bool en) {
  if (en) {
    USBHSD->INT_EN |= USBHS_UDIE_SOF_ACT;
  } else {
    USBHSD->INT_EN &= (uint8_t) ~USBHS_UDIE_SOF_ACT;
  }
}

static inline void wch_usbhs_edpt_enable(uint8_t ep_num, tusb_dir_t dir, bool is_iso) {
  if (dir == TUSB_DIR_OUT) {
    USBHSD->UEP_RX_EN |= (uint16_t)(1u << ep_num);
    if (is_iso) {
      USBHSD->UEP_RX_ISO |= (uint16_t)(1u << ep_num);
    }
  } else {
    USBHSD->UEP_TX_EN |= (uint16_t)(1u << ep_num);
    if (is_iso) {
      USBHSD->UEP_TX_ISO |= (uint16_t)(1u << ep_num);
    }
  }
}

static inline void wch_usbhs_edpt_enable_iso_in(uint8_t ep_num) {
  USBHSD->UEP_TX_EN |= (uint16_t)(1u << ep_num);
}

static inline void wch_usbhs_edpt_disable(uint8_t ep_num, tusb_dir_t dir) {
  if (dir == TUSB_DIR_OUT) {
    USBHSD->UEP_RX_ISO &= (uint16_t) ~(1u << ep_num);
    USBHSD->UEP_RX_EN &= (uint16_t) ~(1u << ep_num);
  } else {
    USBHSD->UEP_TX_ISO &= (uint16_t) ~(1u << ep_num);
    USBHSD->UEP_TX_EN &= (uint16_t) ~(1u << ep_num);
  }
}

static inline void wch_usbhs_edpt_disable_iso_in(uint8_t ep_num) {
  USBHSD->UEP_TX_EN &= (uint16_t) ~(1u << ep_num);
}

static inline void wch_usbhs_edpt_close_all(void) {
  USBHSD->UEP_TX_EN = USBHS_UEP0_T_EN;
  USBHSD->UEP_RX_EN = USBHS_UEP0_R_EN;
  USBHSD->UEP_TX_ISO = 0;
  USBHSD->UEP_RX_ISO = 0;
}

static inline uint16_t wch_usbhs_edpt_rx_len(uint8_t ep_num) {
  return EP_RX_LEN(ep_num);
}

static inline void wch_usbhs_edpt_rx_done_clear(uint8_t ep_num) {
  EP_RX_CTRL(ep_num) &= (uint8_t) ~USBHS_UEP_R_DONE;
}

static inline void wch_usbhs_edpt_tx_done_clear(uint8_t ep_num) {
  EP_TX_CTRL(ep_num) &= (uint8_t) ~USBHS_UEP_T_DONE;
}

static inline bool wch_usbhs_edpt_setup_received(uint8_t ep_num) {
  return ep_num == 0 && (EP_RX_CTRL(0) & USBHS_UEP_R_SETUP_IS);
}

#endif
