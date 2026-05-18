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

#ifndef DCD_CH32_USBHS_F20X_V30X_H_
#define DCD_CH32_USBHS_F20X_V30X_H_

#define EP_TX_LEN(ep)     *(volatile uint16_t *)((volatile uint16_t *)&(USBHSD->UEP0_TX_LEN) + (ep) * 2)
#define EP_TX_CTRL(ep)    *(volatile uint8_t *)((volatile uint8_t *)&(USBHSD->UEP0_TX_CTRL) + (ep) * 4)
#define EP_RX_CTRL(ep)    *(volatile uint8_t *)((volatile uint8_t *)&(USBHSD->UEP0_RX_CTRL) + (ep) * 4)
#define EP_RX_MAX_LEN(ep) *(volatile uint16_t *)((volatile uint16_t *)&(USBHSD->UEP0_MAX_LEN) + (ep) * 2)

#define EP_TX_DMA_ADDR(ep) *(volatile uint32_t *)((volatile uint32_t *)&(USBHSD->UEP1_TX_DMA) + (ep - 1))
#define EP_RX_DMA_ADDR(ep) *(volatile uint32_t *)((volatile uint32_t *)&(USBHSD->UEP1_RX_DMA) + (ep - 1))

static inline void wch_usbhs_dcd_hw_init(void) {
  USBHSD->HOST_CTRL = 0x00;
  USBHSD->HOST_CTRL = USBHS_PHY_SUSPENDM;

  USBHSD->CONTROL = 0;

#if TUD_OPT_HIGH_SPEED
  USBHSD->CONTROL = USBHS_DMA_EN | USBHS_INT_BUSY_EN | USBHS_HIGH_SPEED;
#else
  #error OPT_MODE_FULL_SPEED not currently supported on CH32
  USBHSD->CONTROL = USBHS_DMA_EN | USBHS_INT_BUSY_EN | USBHS_FULL_SPEED;
#endif

  USBHSD->INT_EN = 0;
  USBHSD->INT_EN = USBHS_SETUP_ACT_EN | USBHS_TRANSFER_EN | USBHS_BUS_RST_EN | USBHS_SUSPEND_EN | USBHS_ISO_ACT_EN;

  USBHSD->ENDP_CONFIG = USBHS_EP0_T_EN | USBHS_EP0_R_EN;
  USBHSD->ENDP_TYPE = 0x00;
  USBHSD->BUF_MODE = 0x00;
}

static inline void wch_usbhs_dcd_connect(void) {
  USBHSD->CONTROL |= USBHS_DEV_PU_EN;
}

static inline void wch_usbhs_dcd_sof_enable(bool en) {
  if (en) {
    USBHSD->INT_EN |= USBHS_SOF_ACT_EN;
  } else {
    USBHSD->INT_EN &= ~(USBHS_SOF_ACT_EN);
  }
}

static inline void wch_usbhs_edpt_enable(uint8_t ep_num, tusb_dir_t dir, bool is_iso) {
  if (dir == TUSB_DIR_OUT) {
    USBHSD->ENDP_CONFIG |= (USBHS_EP0_R_EN << ep_num);
    if (is_iso) {
      USBHSD->ENDP_TYPE |= (USBHS_EP0_R_TYP << ep_num);
    }
  } else {
    if (is_iso) {
      USBHSD->ENDP_TYPE |= (USBHS_EP0_T_TYP << ep_num);
    } else {
      USBHSD->ENDP_CONFIG |= (USBHS_EP0_T_EN << ep_num);
    }
  }
}

static inline void wch_usbhs_edpt_enable_iso_in(uint8_t ep_num) {
  USBHSD->ENDP_CONFIG |= (USBHS_EP0_T_EN << ep_num);
}

static inline void wch_usbhs_edpt_disable(uint8_t ep_num, tusb_dir_t dir) {
  if (dir == TUSB_DIR_OUT) {
    USBHSD->ENDP_TYPE &= ~(USBHS_EP0_R_TYP << ep_num);
    USBHSD->ENDP_CONFIG &= ~(USBHS_EP0_R_EN << ep_num);
  } else {
    USBHSD->ENDP_TYPE &= ~(USBHS_EP0_T_TYP << ep_num);
    USBHSD->ENDP_CONFIG &= ~(USBHS_EP0_T_EN << ep_num);
  }
}

static inline void wch_usbhs_edpt_disable_iso_in(uint8_t ep_num) {
  USBHSD->ENDP_CONFIG &= ~(USBHS_EP0_T_EN << ep_num);
}

static inline void wch_usbhs_edpt_close_all(void) {
  USBHSD->ENDP_CONFIG = USBHS_EP0_T_EN | USBHS_EP0_R_EN;
}

#endif
