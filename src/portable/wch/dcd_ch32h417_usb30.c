/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2026 Ha Thach (tinyusb.org)
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

// USB3.0 SuperSpeed device driver for the WCH CH32H417/CH32H416 USBSS controller
// (@0x40034000), documented in CH32H417RM-EN chapter 27. The LINK layer is the same IP as the
// CH569 (dcd_ch56x_usb30.c) but the endpoint engine is a reworked chain-DMA design with
// hardware sequence numbers and ERDY (SEQ_AUTO / ERDY_AUTO), per-endpoint persistent halt
// (RB_EP_TX/RX_HALT), and per-chain completion flags. Software drives the LTSSM through the
// USBSS_LINK interrupt. The init/PHY/LINK/EP0 register sequences are transcribed from the WCH
// EVT USBSS device demo; the data-endpoint engine arms one chain per packet (burst 1) for
// correctness-first bring-up, matching the CH569 port's initial path - raise the burst once the
// hardware validates. Buffers may live anywhere in the shared SRAM (all DMA-reachable).
//
// With CFG_TUD_WCH_USB30_FALLBACK this dcd owns both controllers: it counts failed SuperSpeed
// training attempts (LINK DISABLE/INACTIVE) with a TIM12 backstop and hands rhport 0 to the USB2
// driver (ch32h417_usb2_* in dcd_ch32h417_usbhs.c) when the host has no SuperSpeed port.
//
// NOTE: compile-verified; full hardware bring-up (usbtest at 5 Gbps) is pending.

#include "tusb_option.h"

#if CFG_TUD_ENABLED && defined(TUP_USBIP_WCH_USB30_H417) && \
    defined(CFG_TUD_WCH_USBIP_USB30) && CFG_TUD_WCH_USBIP_USB30 == 1

#include "device/dcd.h"
#include "dcd_ch32h417.h"
#include "ch32h417_usb30_reg.h"

#define EP_MAX          8
#define EP0_MAX_SIZE    512

#ifndef CFG_TUD_WCH_USB30_MAX_BURST
#define CFG_TUD_WCH_USB30_MAX_BURST 1
#endif

typedef struct {
  uint8_t *buffer;
  uint16_t total_len;
  uint16_t queued_len;
  uint16_t max_size;
  bool     is_iso;
  bool     valid;
} xfer_ctl_t;

#define XFER_CTL_BASE(_ep, _dir) (&xfer_status[_ep][_dir])
static xfer_ctl_t xfer_status[EP_MAX][2];

// EP0 buffer in shared SRAM (DMA-reachable); SETUP lands here.
TU_ATTR_ALIGNED(16) static uint8_t ep0_buffer[EP0_MAX_SIZE];
static uint8_t ep0_tx_seq; // EP0 IN sequence number [4:0]
static uint8_t pending_addr;
static bool    pending_addr_valid;

//--------------------------------------------------------------------+
// Runtime USB2 fallback state
//--------------------------------------------------------------------+
#if CFG_TUD_WCH_USB30_FALLBACK
enum { FB_USB3_TRAINING, FB_USB3_UP, FB_USB2_ACTIVE };
static uint8_t fb_state;
static uint8_t fb_fail_count;
#define FB_FAIL_LIMIT 3

static void usbss_device_init(bool enable);
static void fallback_timer_start(bool enable);

// Shut the SuperSpeed controller down and bring the USB2 high-speed controller up on rhport 0.
// Must stop TIM12 first: once fb_state==FB_USB2_ACTIVE, dcd_int_handler routes every IRQ to the
// USB2 handler and never clears the TIM12 flag, so a still-running timer would storm the CPU.
static void fallback_to_usb2(uint8_t rhport) {
  fallback_timer_start(false);
  usbss_device_init(false);
  fb_state = FB_USB2_ACTIVE;
  ch32h417_usb2_init(rhport);
  ch32h417_usb2_int_enable();
}
#endif

//--------------------------------------------------------------------+
// PHY / clock / link bring-up (transcribed from the WCH EVT USBSS demo)
//--------------------------------------------------------------------+

static void usbss_rcc_init(bool enable) {
  if (enable) {
    RCC->CTLR |= (uint32_t)RCC_USBSS_PLLON;
    while ((RCC->CTLR & (uint32_t)RCC_USBSS_PLLRDY) != (uint32_t)RCC_USBSS_PLLRDY) {}
    RCC_HBPeriphClockCmd(RCC_HBPeriph_USBSS, ENABLE);
    RCC_PIPECmd(ENABLE);
    RCC_UTMIcmd(ENABLE);
  } else {
    RCC_HBPeriphClockCmd(RCC_HBPeriph_USBSS, DISABLE);
    RCC_UTMIcmd(DISABLE);
    RCC_PIPECmd(DISABLE);
    RCC->CTLR &= ~(uint32_t)RCC_USBSS_PLLON;
  }
}

static uint32_t usbss_phy_cfg(uint8_t addr, uint16_t data) {
  USBSS_PHY_CFG_CR = (1u << 23) | ((uint32_t)addr << 16) | data;
  USBSS_PHY_CFG_DAT = 0x01;
  return USBSS_PHY_CFG_DAT;
}

static void usbss_cfg_mod(void) {
  usbss_phy_cfg(0x03, 0x7C12);
  usbss_phy_cfg(0x0D, 0x79AA);
  usbss_phy_cfg(0x15, 0x4430);
  usbss_phy_cfg(0x13, 0x0010);
  USBSS_PHY_MISC_REG = 0xB0054000;
}

static void ep_chain_init(void) {
  // EP0 buffer for SETUP/data
  USBSSD->UEP0_TX_CTRL = 0;
  USBSSD->UEP0_RX_CTRL = 0;
  USBSSD->UEP0_TX_DMA = (uint32_t)(uintptr_t)ep0_buffer;
  USBSSD->UEP0_RX_DMA = (uint32_t)(uintptr_t)ep0_buffer;
}

static void usbss_device_init(bool enable) {
  if (enable) {
    usbss_rcc_init(true);

    // TX de-emphasis: match the WCH EVT init literal exactly (it programs both DEEMPH bits)
    USBSSD->LINK_CFG = LINK_RX_EQ_EN | LINK_TX_DEEMPH_MASK | LINK_PHY_RESET;
    USBSSD->LINK_CTRL = LINK_P2_MODE | LINK_GO_DISABLED;
    USBSSD->LINK_CFG = LINK_RX_EQ_EN | LINK_TX_DEEMPH_MASK | LINK_LTSSM_MODE | LINK_TOUT_MODE;
    USBSSD->LINK_LPM_CR |= LINK_LPM_EN;
    USBSSD->LINK_CFG |= LINK_RX_TERM_EN;
    USBSSD->LINK_INT_CTRL = LINK_IE_TX_LMP | LINK_IE_RX_LMP | LINK_IE_RX_LMP_TOUT | LINK_IE_STATE_CHG |
                            LINK_IE_WARM_RST | LINK_IE_TERM_PRES;
    USBSSD->LINK_CTRL = LINK_P2_MODE;
    USBSSD->LINK_U1_WKUP_TMR = 120;
    USBSSD->LINK_U1_WKUP_FILTER = 50;
    USBSSD->LINK_U2_WKUP_FILTER = 0;
    USBSSD->LINK_U3_WKUP_FILTER = 0;

    USBSSD->USB_CONTROL |= USBSS_FORCE_RST;
    USBSSD->USB_STATUS = USBSS_UIF_TRANSFER;
    USBSSD->USB_CONTROL = USBSS_UIE_TRANSFER | USBSS_UDIE_SETUP | USBSS_UDIE_STATUS | USBSS_DMA_EN | USBSS_SETUP_FLOW;

    usbss_cfg_mod();

    USBSSD->UEP_TX_EN = 0;
    USBSSD->UEP_RX_EN = 0;
    ep_chain_init();

    NVIC_EnableIRQ(USBSS_IRQn);
    NVIC_EnableIRQ(USBSS_LINK_IRQn);
  } else {
    NVIC_DisableIRQ(USBSS_LINK_IRQn);
    NVIC_DisableIRQ(USBSS_IRQn);
    USBSSD->USB_CONTROL = USBSS_FORCE_RST;
    USBSSD->LINK_CFG |= LINK_PHY_RESET | U3_LINK_RESET;
    for (volatile int i = 0; i < 2000; i++) {}
    USBSSD->USB_CONTROL &= ~USBSS_FORCE_RST;
    USBSSD->LINK_CFG &= ~(LINK_PHY_RESET | U3_LINK_RESET);
    usbss_rcc_init(false);
  }
}

// re-init the endpoint engine after a warm/hot reset (keeps the link up)
static void usbss_reset_init(void) {
  USBSSD->USB_CONTROL |= USBSS_FORCE_RST;
  USBSSD->USB_STATUS = USBSS_UIF_TRANSFER;
  USBSSD->USB_CONTROL = USBSS_UIE_TRANSFER | USBSS_UDIE_SETUP | USBSS_UDIE_STATUS | USBSS_DMA_EN | USBSS_SETUP_FLOW;
  ep_chain_init();
}

//--------------------------------------------------------------------+
// LINK layer (LTSSM) - state-change driven, transcribed from the demo
//--------------------------------------------------------------------+

static void handle_link_irq(uint8_t rhport) {
  uint32_t link_int = USBSSD->LINK_INT_FLAG;
  uint32_t link_state = USBSSD->LINK_STATUS & LINK_STATE_MASK;

  if (link_int & LINK_IF_STATE_CHG) {
    USBSSD->LINK_INT_FLAG = LINK_IF_STATE_CHG;

    switch (link_state) {
      case LINK_STATE_DISABLE:
        USBSSD->LINK_CTRL &= ~LINK_GO_DISABLED;
        // DISABLE and INACTIVE both count as a failed SuperSpeed training attempt
        TU_ATTR_FALLTHROUGH;
      case LINK_STATE_INACTIVE:
#if CFG_TUD_WCH_USB30_FALLBACK
        if (fb_state == FB_USB3_TRAINING && ++fb_fail_count >= FB_FAIL_LIMIT) {
          fallback_to_usb2(rhport); // host has no SuperSpeed port: bring up USB2 on rhport 0
        }
#endif
        break;

      case LINK_STATE_HOTRST:
        usbss_reset_init();
        dcd_event_bus_reset(rhport, TUSB_SPEED_SUPER, true);
        USBSSD->LINK_CTRL &= ~LINK_HOT_RESET;
        break;

      case LINK_STATE_U3:
        dcd_event_bus_signal(rhport, DCD_EVENT_SUSPEND, true);
        break;

      case LINK_STATE_U0:
      case LINK_STATE_RECOVERY:
      case LINK_STATE_POLLING:
      case LINK_STATE_RXDET:
      default:
        break;
    }
  } else if (link_int & LINK_IF_TERM_PRES) {
    USBSSD->LINK_INT_FLAG = LINK_IF_TERM_PRES;
  } else if (link_int & LINK_IF_RX_LMP_TOUT) {
    USBSSD->LINK_INT_FLAG = LINK_IF_RX_LMP_TOUT;
    USBSSD->LINK_CTRL |= LINK_GO_DISABLED;
    USBSSD->LINK_CTRL |= LINK_GO_RX_DET;
  } else if (link_int & LINK_IF_TX_LMP) {
    USBSSD->LINK_INT_FLAG = LINK_IF_TX_LMP;
    USBSSD->LINK_LMP_TX_DATA0 = LMP_LINK_SPEED | LMP_PORT_CAP | LMP_HP;
    USBSSD->LINK_LMP_TX_DATA1 = LMP_UP_STREAM | LMP_NUM_HP_BUF;
    USBSSD->LINK_LMP_TX_DATA2 = 0;
  } else if (link_int & LINK_IF_RX_LMP) {
    USBSSD->LINK_INT_FLAG = LINK_IF_RX_LMP;
    uint32_t rx0 = USBSSD->LINK_LMP_RX_DATA0;
    if ((rx0 & LMP_SUBTYPE_MASK) == LMP_PORT_CFG) {
      // upstream port received Port Configuration: reply Port Config Response = SS training done
      USBSSD->LINK_LMP_TX_DATA0 = LMP_LINK_SPEED | LMP_PORT_CFG_RES | LMP_HP;
      USBSSD->LINK_LMP_TX_DATA1 = 0;
      USBSSD->LINK_LMP_TX_DATA2 = 0;
      USBSSD->LINK_LMP_PORT_CAP |= LINK_LMP_TX_CAP_VLD;
#if CFG_TUD_WCH_USB30_FALLBACK
      fb_state = FB_USB3_UP;
      fb_fail_count = 0;
#endif
      dcd_event_bus_reset(rhport, TUSB_SPEED_SUPER, true);
    } else if ((rx0 & LMP_SUBTYPE_MASK) == LMP_U2_INACT_TOUT) {
      USBSSD->LINK_U2_INACT_TIMER = (rx0 >> 9) & 0xFF;
    }
  } else if (link_int & LINK_IF_WARM_RST) {
    USBSSD->LINK_INT_FLAG = LINK_IF_WARM_RST;
    if (USBSSD->LINK_STATUS & LINK_RX_WARM_RST) {
      usbss_reset_init();
      USBSSD->LINK_CTRL |= LINK_GO_DISABLED;
      __NOP(); __NOP(); __NOP(); __NOP();
      USBSSD->LINK_CTRL &= ~LINK_GO_DISABLED;
      dcd_event_bus_reset(rhport, TUSB_SPEED_SUPER, true);
    }
  }
}

//--------------------------------------------------------------------+
// EP0 control
//--------------------------------------------------------------------+

static void ep0_arm_in(uint16_t len) {
  USBSSD->UEP0_TX_CTRL = USBSS_EP0_TX_DPH | len | ((uint32_t)ep0_tx_seq << 16);
  USBSSD->UEP0_TX_CTRL |= USBSS_EP0_TX_ERDY;
  ep0_tx_seq = (ep0_tx_seq + 1) & 0x1F;
}

static void ep0_arm_out(void) {
  USBSSD->UEP0_RX_CTRL = USBSS_EP0_RX_ERDY | USBSS_EP0_RX_ACK;
}

static void handle_setup(uint8_t rhport) {
  ep0_tx_seq = 0;
  pending_addr_valid = false;
  xfer_status[0][TUSB_DIR_IN].valid = false;
  xfer_status[0][TUSB_DIR_OUT].valid = false;
  dcd_event_setup_received(rhport, ep0_buffer, true);
}

static void handle_ep0_in(uint8_t rhport) {
  xfer_ctl_t *xfer = XFER_CTL_BASE(0, TUSB_DIR_IN);
  if (!xfer->valid || xfer->total_len == 0) {
    return; // a zero-length status IN is completed by the UDIF_STATUS interrupt, not here
  }
  uint16_t remaining = xfer->total_len - xfer->queued_len;
  if (remaining == 0) {
    xfer->valid = false;
    dcd_event_xfer_complete(rhport, 0x80, xfer->queued_len, XFER_RESULT_SUCCESS, true);
    return;
  }
  uint16_t len = TU_MIN(remaining, (uint16_t)EP0_MAX_SIZE);
  memcpy(ep0_buffer, &xfer->buffer[xfer->queued_len], len);
  xfer->queued_len += len;
  ep0_arm_in(len);
}

static void handle_ep0_out(uint8_t rhport) {
  xfer_ctl_t *xfer = XFER_CTL_BASE(0, TUSB_DIR_OUT);
  if (!xfer->valid) {
    return;
  }
  uint16_t rx_len = USBSSD->UEP0_RX_CTRL & USBSS_EP0_RX_LEN_MASK;
  uint16_t remaining = xfer->total_len - xfer->queued_len;
  uint16_t len = TU_MIN(rx_len, remaining);
  memcpy(&xfer->buffer[xfer->queued_len], ep0_buffer, len);
  xfer->queued_len += len;

  if (xfer->queued_len >= xfer->total_len || len < EP0_MAX_SIZE) {
    xfer->valid = false;
    dcd_event_xfer_complete(rhport, 0x00, xfer->queued_len, XFER_RESULT_SUCCESS, true);
  } else {
    ep0_arm_out();
  }
}

//--------------------------------------------------------------------+
// Data endpoints - single chain armed per packet (burst 1)
//--------------------------------------------------------------------+

static void queue_in_packet(uint8_t ep_num, xfer_ctl_t *xfer) {
  uint16_t remaining = xfer->total_len - xfer->queued_len;
  uint16_t tx_len = TU_MIN(remaining, xfer->max_size);
  volatile USBSS_EP_TX_TypeDef *tx = usbss_ep_tx(ep_num);

  tx->UEP_TX_DMA = (uint32_t)(uintptr_t)&xfer->buffer[xfer->queued_len];
  tx->UEP_TX_DMA_OFS = xfer->max_size;
  tx->UEP_TX_CHAIN_LEN = tx_len;
  tx->UEP_TX_CHAIN_EXP_NUMP = 1; // arms the chain
  xfer->queued_len += tx_len;
}

static void queue_out_packet(uint8_t ep_num, xfer_ctl_t *xfer) {
  volatile USBSS_EP_RX_TypeDef *rx = usbss_ep_rx(ep_num);
  rx->UEP_RX_DMA = (uint32_t)(uintptr_t)&xfer->buffer[xfer->queued_len];
  rx->UEP_RX_DMA_OFS = xfer->max_size;
  rx->UEP_RX_CHAIN_MAX_NUMP = 1; // arms the chain
}

static void handle_ep_in(uint8_t rhport, uint8_t ep_num) {
  xfer_ctl_t *xfer = XFER_CTL_BASE(ep_num, TUSB_DIR_IN);
  volatile USBSS_EP_TX_TypeDef *tx = usbss_ep_tx(ep_num);
  tx->UEP_TX_CHAIN_ST |= USBSS_EP_TX_CHAIN_IF; // release completed chain

  if (!xfer->valid) {
    return;
  }
  if (xfer->queued_len >= xfer->total_len) {
    xfer->valid = false;
    dcd_event_xfer_complete(rhport, ep_num | TUSB_DIR_IN_MASK, xfer->queued_len, XFER_RESULT_SUCCESS, true);
  } else {
    queue_in_packet(ep_num, xfer);
  }
}

static void handle_ep_out(uint8_t rhport, uint8_t ep_num) {
  xfer_ctl_t *xfer = XFER_CTL_BASE(ep_num, TUSB_DIR_OUT);
  volatile USBSS_EP_RX_TypeDef *rx = usbss_ep_rx(ep_num);
  uint16_t rx_len = rx->UEP_RX_CHAIN_LEN;
  rx->UEP_RX_CHAIN_ST |= USBSS_EP_RX_CHAIN_IF; // release completed chain

  if (!xfer->valid) {
    return;
  }
  uint16_t remaining = xfer->total_len - xfer->queued_len;
  uint16_t len = TU_MIN(rx_len, TU_MIN(remaining, xfer->max_size));
  xfer->queued_len += len;

  if (xfer->queued_len >= xfer->total_len || len < xfer->max_size) {
    xfer->valid = false;
    dcd_event_xfer_complete(rhport, ep_num, xfer->queued_len, XFER_RESULT_SUCCESS, true);
  } else {
    queue_out_packet(ep_num, xfer);
  }
}

//--------------------------------------------------------------------+
// USB (transfer) interrupt dispatch
//--------------------------------------------------------------------+

static void handle_usb_irq(uint8_t rhport) {
  uint32_t status = USBSSD->USB_STATUS;

  if ((status & USBSS_UDIF_SETUP) && !(status & USBSS_UDIF_STATUS)) {
    USBSSD->USB_STATUS = USBSS_UDIF_SETUP;
    handle_setup(rhport);
  } else if (status & USBSS_UDIF_STATUS) {
    // Control status stage: on the H417 this is its own interrupt (SET_ADDRESS is applied here).
    USBSSD->USB_STATUS = USBSS_UDIF_STATUS;
    if (pending_addr_valid) {
      USBSSD->USB_CONTROL = (USBSSD->USB_CONTROL & 0x00FFFFFF) | ((uint32_t)pending_addr << 24);
      pending_addr_valid = false;
    }
    USBSSD->UEP0_TX_CTRL = 0;
    USBSSD->UEP0_RX_CTRL = 0;
    // Complete the queued zero-length status transfer so usbd runs its status-stage callback
    // (the status ZLP does not raise a UIF_TRANSFER on this controller).
    for (uint8_t dir = 0; dir < 2; dir++) {
      xfer_ctl_t *x = &xfer_status[0][dir];
      if (x->valid && x->total_len == 0) {
        x->valid = false;
        dcd_event_xfer_complete(rhport, (dir == TUSB_DIR_IN) ? 0x80 : 0x00, 0, XFER_RESULT_SUCCESS, true);
      }
    }
  } else if (status & USBSS_UIF_TRANSFER) {
    uint8_t ep_num = USBSS_STATUS_EP_NUM(status);
    bool is_in = USBSS_STATUS_EP_IN(status);
    if (ep_num == 0) {
      if (is_in) { handle_ep0_in(rhport); } else { handle_ep0_out(rhport); }
    } else if (is_in) {
      handle_ep_in(rhport, ep_num);
    } else {
      handle_ep_out(rhport, ep_num);
    }
  }
}

//--------------------------------------------------------------------+
// TIM12 fallback backstop timer (0.5 s), direct-register
//--------------------------------------------------------------------+
#if CFG_TUD_WCH_USB30_FALLBACK
static void fallback_timer_start(bool enable) {
  if (enable) {
    RCC_HB2PeriphClockCmd(RCC_HB2Periph_TIM12, ENABLE);
    RCC_ClocksTypeDef clk;
    RCC_GetClocksFreq(&clk);
    TIM12->PSC = (uint16_t)(clk.HCLK_Frequency / 10000 - 1);
    TIM12->ATRLR = 5000 - 1; // 0.5 s
    TIM12->CNT = 0;
    TIM12->INTFR = 0;
    TIM12->DMAINTENR |= TIM_IT_Update;
    TIM12->CTLR1 |= TIM_CEN;
    NVIC_EnableIRQ(TIM12_IRQn);
  } else {
    NVIC_DisableIRQ(TIM12_IRQn);
    TIM12->CTLR1 &= (uint16_t)~TIM_CEN;
    TIM12->DMAINTENR &= (uint16_t)~TIM_IT_Update;
  }
}

static void handle_timer_irq(uint8_t rhport) {
  if (!(TIM12->INTFR & TIM_IT_Update)) {
    return;
  }
  TIM12->INTFR = (uint16_t)~TIM_IT_Update;
  if (fb_state == FB_USB3_TRAINING && ++fb_fail_count >= FB_FAIL_LIMIT) {
    fallback_to_usb2(rhport);
  }
}
#endif

//--------------------------------------------------------------------+
// dcd API
//--------------------------------------------------------------------+

bool dcd_init(uint8_t rhport, const tusb_rhport_init_t *rh_init) {
  (void)rhport;
  (void)rh_init;
  memset(&xfer_status, 0, sizeof(xfer_status));
  ep0_tx_seq = 0;
  pending_addr_valid = false;

#if CFG_TUD_WCH_USB30_FALLBACK
  fb_state = FB_USB3_TRAINING;
  fb_fail_count = 0;
#endif

  usbss_device_init(true);

#if CFG_TUD_WCH_USB30_FALLBACK
  fallback_timer_start(true);
#endif
  return true;
}

void dcd_int_handler(uint8_t rhport) {
#if CFG_TUD_WCH_USB30_FALLBACK
  if (fb_state == FB_USB2_ACTIVE) {
    ch32h417_usb2_int_handler(rhport);
    return;
  }
  handle_timer_irq(rhport);
  if (fb_state == FB_USB2_ACTIVE) {
    return;
  }
#endif
  if (USBSSD->LINK_INT_FLAG & USBSSD->LINK_INT_CTRL) {
    handle_link_irq(rhport);
  }
  if (USBSSD->USB_STATUS & (USBSS_UIF_TRANSFER | USBSS_UDIF_SETUP | USBSS_UDIF_STATUS)) {
    handle_usb_irq(rhport);
  }
}

void dcd_int_enable(uint8_t rhport) {
#if CFG_TUD_WCH_USB30_FALLBACK
  if (fb_state == FB_USB2_ACTIVE) { ch32h417_usb2_int_enable(); return; }
#endif
  (void)rhport;
  NVIC_EnableIRQ(USBSS_IRQn);
  NVIC_EnableIRQ(USBSS_LINK_IRQn);
}

void dcd_int_disable(uint8_t rhport) {
#if CFG_TUD_WCH_USB30_FALLBACK
  if (fb_state == FB_USB2_ACTIVE) { ch32h417_usb2_int_disable(); return; }
#endif
  (void)rhport;
  NVIC_DisableIRQ(USBSS_IRQn);
  NVIC_DisableIRQ(USBSS_LINK_IRQn);
}

void dcd_set_address(uint8_t rhport, uint8_t dev_addr) {
  (void)dev_addr;
#if CFG_TUD_WCH_USB30_FALLBACK
  if (fb_state == FB_USB2_ACTIVE) {
    // USB2 applies the address in ch32h417_usb2_edpt0_status_complete (called by usbd with the
    // real request); here just arm the status ZLP on the USB2 controller.
    ch32h417_usb2_edpt_xfer(rhport, 0x80, NULL, 0);
    return;
  }
#endif
  // SuperSpeed: apply the address at the status stage (USBSS_UDIF_STATUS), per the SIE
  pending_addr = dev_addr;
  pending_addr_valid = true;
  dcd_edpt_xfer(rhport, 0x80, NULL, 0, false); // ZLP status
}

void dcd_remote_wakeup(uint8_t rhport) {
  (void)rhport;
  USBSSD->LINK_CTRL |= LINK_TX_UX_EXIT;
}

void dcd_sof_enable(uint8_t rhport, bool en) {
  (void)rhport;
  (void)en;
}

void dcd_edpt0_status_complete(uint8_t rhport, const tusb_control_request_t *request) {
#if CFG_TUD_WCH_USB30_FALLBACK
  if (fb_state == FB_USB2_ACTIVE) { ch32h417_usb2_edpt0_status_complete(rhport, request); return; }
#endif
  (void)rhport;
  (void)request;
}

bool dcd_edpt_open(uint8_t rhport, const tusb_desc_endpoint_t *desc_edpt) {
#if CFG_TUD_WCH_USB30_FALLBACK
  if (fb_state == FB_USB2_ACTIVE) { return ch32h417_usb2_edpt_open(rhport, desc_edpt); }
#endif
  (void)rhport;
  const uint8_t ep_num = tu_edpt_number(desc_edpt->bEndpointAddress);
  const tusb_dir_t dir = tu_edpt_dir(desc_edpt->bEndpointAddress);
  TU_ASSERT(ep_num < EP_MAX);
  if (ep_num == 0) {
    return true;
  }

  xfer_ctl_t *xfer = XFER_CTL_BASE(ep_num, dir);
  xfer->max_size = tu_edpt_packet_size(desc_edpt);
  xfer->is_iso = (desc_edpt->bmAttributes.xfer == TUSB_XFER_ISOCHRONOUS);

  if (dir == TUSB_DIR_IN) {
    volatile USBSS_EP_TX_TypeDef *tx = usbss_ep_tx(ep_num);
    tx->UEP_TX_CFG = USBSS_EP_TX_CHAIN_AUTO | USBSS_EP_TX_ERDY_AUTO | USBSS_EP_TX_SEQ_AUTO |
                     (xfer->is_iso ? USBSS_EP_TX_ISO_MODE : 0);
    tx->UEP_TX_CR = (uint8_t)(USBSS_EP_TX_CLR | USBSS_EP_TX_CHAIN_CLR);
    tx->UEP_TX_CR = CFG_TUD_WCH_USB30_MAX_BURST; // ERDY_NUMP advertised
    USBSSD->UEP_TX_EN |= (uint16_t)(1u << ep_num);
  } else {
    volatile USBSS_EP_RX_TypeDef *rx = usbss_ep_rx(ep_num);
    rx->UEP_RX_CFG = USBSS_EP_RX_CHAIN_AUTO | USBSS_EP_RX_ERDY_AUTO | USBSS_EP_RX_SEQ_AUTO |
                     (xfer->is_iso ? USBSS_EP_RX_ISO_MODE : 0);
    rx->UEP_RX_CR = (uint8_t)(USBSS_EP_RX_CLR | USBSS_EP_RX_CHAIN_CLR);
    rx->UEP_RX_CR = CFG_TUD_WCH_USB30_MAX_BURST;
    USBSSD->UEP_RX_EN |= (uint16_t)(1u << ep_num);
  }
  return true;
}

void dcd_edpt_close(uint8_t rhport, uint8_t ep_addr) {
#if CFG_TUD_WCH_USB30_FALLBACK
  if (fb_state == FB_USB2_ACTIVE) { ch32h417_usb2_edpt_close(rhport, ep_addr); return; }
#endif
  (void)rhport;
  const uint8_t ep_num = tu_edpt_number(ep_addr);
  const tusb_dir_t dir = tu_edpt_dir(ep_addr);
  if (dir == TUSB_DIR_IN) {
    usbss_ep_tx(ep_num)->UEP_TX_CR = (uint8_t)(USBSS_EP_TX_CLR | USBSS_EP_TX_CHAIN_CLR);
    USBSSD->UEP_TX_EN &= (uint16_t)~(1u << ep_num);
  } else {
    usbss_ep_rx(ep_num)->UEP_RX_CR = (uint8_t)(USBSS_EP_RX_CLR | USBSS_EP_RX_CHAIN_CLR);
    USBSSD->UEP_RX_EN &= (uint16_t)~(1u << ep_num);
  }
  XFER_CTL_BASE(ep_num, dir)->valid = false;
}

void dcd_edpt_close_all(uint8_t rhport) {
#if CFG_TUD_WCH_USB30_FALLBACK
  if (fb_state == FB_USB2_ACTIVE) { ch32h417_usb2_edpt_close_all(rhport); return; }
#endif
  (void)rhport;
  for (uint8_t ep = 1; ep < EP_MAX; ep++) {
    usbss_ep_tx(ep)->UEP_TX_CR = (uint8_t)(USBSS_EP_TX_CLR | USBSS_EP_TX_CHAIN_CLR);
    usbss_ep_rx(ep)->UEP_RX_CR = (uint8_t)(USBSS_EP_RX_CLR | USBSS_EP_RX_CHAIN_CLR);
    xfer_status[ep][0].valid = false;
    xfer_status[ep][1].valid = false;
  }
  USBSSD->UEP_TX_EN = 0;
  USBSSD->UEP_RX_EN = 0;
}

bool dcd_edpt_xfer(uint8_t rhport, uint8_t ep_addr, uint8_t *buffer, uint16_t total_bytes, bool is_isr) {
  (void)is_isr;
#if CFG_TUD_WCH_USB30_FALLBACK
  if (fb_state == FB_USB2_ACTIVE) { return ch32h417_usb2_edpt_xfer(rhport, ep_addr, buffer, total_bytes); }
#endif
  const uint8_t ep_num = tu_edpt_number(ep_addr);
  const tusb_dir_t dir = tu_edpt_dir(ep_addr);

  xfer_ctl_t *xfer = XFER_CTL_BASE(ep_num, dir);
  xfer->buffer = buffer;
  xfer->total_len = total_bytes;
  xfer->queued_len = 0;
  xfer->valid = true;

  if (ep_num == 0) {
    if (dir == TUSB_DIR_IN) {
      if (total_bytes == 0) {
        // ZLP status for a control-OUT/no-data transfer
        USBSSD->UEP0_TX_CTRL = USBSS_EP0_TX_DPH;
        USBSSD->UEP0_TX_CTRL |= USBSS_EP0_TX_ERDY;
        ep0_arm_out();
      } else {
        handle_ep0_in(rhport);
      }
    } else {
      // control-OUT: data stage or status OUT, arm the receive
      ep0_arm_out();
    }
  } else if (dir == TUSB_DIR_IN) {
    queue_in_packet(ep_num, xfer);
  } else {
    queue_out_packet(ep_num, xfer);
  }
  return true;
}

void dcd_edpt_stall(uint8_t rhport, uint8_t ep_addr) {
#if CFG_TUD_WCH_USB30_FALLBACK
  if (fb_state == FB_USB2_ACTIVE) { ch32h417_usb2_edpt_stall(rhport, ep_addr); return; }
#endif
  (void)rhport;
  const uint8_t ep_num = tu_edpt_number(ep_addr);
  const tusb_dir_t dir = tu_edpt_dir(ep_addr);
  if (ep_num == 0) {
    USBSSD->UEP0_TX_CTRL = USBSS_EP0_TX_STALL;
    USBSSD->UEP0_RX_CTRL = USBSS_EP0_RX_ERDY | USBSS_EP0_RX_STALL;
  } else if (dir == TUSB_DIR_IN) {
    usbss_ep_tx(ep_num)->UEP_TX_CR |= USBSS_EP_TX_HALT;
  } else {
    usbss_ep_rx(ep_num)->UEP_RX_CR |= USBSS_EP_RX_HALT;
  }
}

void dcd_edpt_clear_stall(uint8_t rhport, uint8_t ep_addr) {
#if CFG_TUD_WCH_USB30_FALLBACK
  if (fb_state == FB_USB2_ACTIVE) { ch32h417_usb2_edpt_clear_stall(rhport, ep_addr); return; }
#endif
  (void)rhport;
  const uint8_t ep_num = tu_edpt_number(ep_addr);
  const tusb_dir_t dir = tu_edpt_dir(ep_addr);
  if (ep_num == 0) {
    return;
  }
  if (dir == TUSB_DIR_IN) {
    volatile USBSS_EP_TX_TypeDef *tx = usbss_ep_tx(ep_num);
    tx->UEP_TX_CR = (uint8_t)(USBSS_EP_TX_CLR | USBSS_EP_TX_CHAIN_CLR);
    tx->UEP_TX_CR = CFG_TUD_WCH_USB30_MAX_BURST;
    xfer_ctl_t *xfer = XFER_CTL_BASE(ep_num, TUSB_DIR_IN);
    if (xfer->valid) {
      xfer->queued_len = 0;
      queue_in_packet(ep_num, xfer);
    }
  } else {
    volatile USBSS_EP_RX_TypeDef *rx = usbss_ep_rx(ep_num);
    rx->UEP_RX_CR = (uint8_t)(USBSS_EP_RX_CLR | USBSS_EP_RX_CHAIN_CLR);
    rx->UEP_RX_CR = CFG_TUD_WCH_USB30_MAX_BURST;
    xfer_ctl_t *xfer = XFER_CTL_BASE(ep_num, TUSB_DIR_OUT);
    if (xfer->valid) {
      queue_out_packet(ep_num, xfer);
    }
  }
}

#endif
