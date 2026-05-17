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

#include "tusb_option.h"

#if CFG_TUD_ENABLED && defined(TUP_USBIP_WCH_USBSS) && defined(CFG_TUD_WCH_USBIP_USBSS) && \
  (CFG_TUD_WCH_USBIP_USBSS == 1)

#include <string.h>

#include "ch32h417.h"
#include "ch32h417_flash.h"
#include "ch32h417_rcc.h"
#include "ch32h417_usb.h"

#include "device/dcd.h"

#define EP_MAX                8u
#define EP0_STATUS_IDLE       0xffu
// TODO: Derive this from the SuperSpeed endpoint companion MaxBurst field when descriptors advertise bursts.
#define EP_CHAIN_PACKET_LIMIT 1u

#define USBSS_PHY_CFG_CR      (*((__IO uint32_t *)0x400341f8))
#define USBSS_PHY_CFG_DAT     (*((__IO uint32_t *)0x400341fc))

#define USBSS_FUNC_CFG        (*((__IO uint32_t *)0x5003c018))

#define LMP_HP                0u
#define LMP_SUBTYPE_MASK      (0x0fu << 5)
#define LMP_SET_LINK_FUNC     (0x01u << 5)
#define LMP_U2_INACT_TOUT     (0x02u << 5)
#define LMP_PORT_CAP          (0x04u << 5)
#define LMP_PORT_CFG          (0x05u << 5)
#define LMP_PORT_CFG_RES      (0x06u << 5)
#define LMP_LINK_SPEED        (1u << 9)
#define LMP_NUM_HP_BUF        (4u << 0)
#define LMP_UP_STREAM         (2u << 16)

typedef struct {
  uint8_t *buffer;
  uint16_t total_len;
  uint16_t queued_len;
  uint16_t max_size;
} xfer_ctl_t;

typedef struct {
  xfer_ctl_t xfer[EP_MAX][2];
  uint8_t    dev_addr;
  uint8_t    ep0_status_ep;
  bool       set_addr_pending;
  bool       link_configured;
  bool       link_low_power;
  bool       link_reset_seen;
  bool       force_pm;
  bool       u1_enabled;
  bool       u2_enabled;
  uint8_t    chip_revision;
  uint32_t   last_lmp_subtype;
} dcd_data_t;

static dcd_data_t _dcd;

TU_ATTR_ALIGNED(4) static uint8_t ep0_buffer[512];

static inline volatile USBSS_EP_TX_TypeDef *ep_tx_reg(uint8_t ep_num) {
  return (volatile USBSS_EP_TX_TypeDef *)(&USBSSD->EP1_TX + ((ep_num - 1u) * 2u));
}

static inline volatile USBSS_EP_RX_TypeDef *ep_rx_reg(uint8_t ep_num) {
  return (volatile USBSS_EP_RX_TypeDef *)(&USBSSD->EP1_RX + ((ep_num - 1u) * 2u));
}

static inline xfer_ctl_t *xfer_ctl(uint8_t ep_num, tusb_dir_t dir) {
  return &_dcd.xfer[ep_num][dir];
}

static void delay_cycles(uint32_t count) {
  while (count--) {
    __NOP();
  }
}

static uint8_t chip_revision_get(void) {
  FLASH_BOOT_GetMode();
  return (uint8_t)(((*((__I uint32_t *)0x1ffff704)) >> 4) & 0x0fu);
}

static inline bool chip_has_rx_set_fc(void) {
  // WCH's USBSS example only enables RX_SET_FC on revision 3 and newer.
  // Older revisions use SET_LINK_FUNC LMPs for the equivalent U1/U2 policy.
  return _dcd.chip_revision >= 3u;
}

static void usbss_phy_cfg(uint8_t addr, uint16_t data) {
  USBSS_PHY_CFG_CR  = (1u << 23) | ((uint32_t)addr << 16) | data;
  USBSS_PHY_CFG_DAT = 0x01u;
}

static void usbss_pll_enable(bool en) {
  if (en) {
    RCC->CTLR |= RCC_USBSS_PLLON;
    while ((RCC->CTLR & RCC_USBSS_PLLRDY) == 0u) {}
  } else {
    RCC->CTLR &= ~RCC_USBSS_PLLON;
  }
}

static void usbss_clock_enable(bool en) {
  if (en) {
    usbss_pll_enable(true);
    RCC_HBPeriphClockCmd(RCC_HBPeriph_USBSS, ENABLE);
    RCC_PIPECmd(ENABLE);
    RCC_UTMIcmd(ENABLE);
    RCC_USBSS_PLLCmd(ENABLE);
  } else {
    RCC_HBPeriphClockCmd(RCC_HBPeriph_USBSS, DISABLE);
    RCC_UTMIcmd(DISABLE);
    RCC_PIPECmd(DISABLE);
    if ((RCC->PLLCFGR & RCC_SYSPLL_SEL) != RCC_SYSPLL_USBSS) {
      RCC_USBSS_PLLCmd(DISABLE);
    }
  }
}

static void usbss_phy_init(void) {
  usbss_phy_cfg(0x03, 0x7c12);
  usbss_phy_cfg(0x0d, 0x79aa);
  usbss_phy_cfg(0x15, 0x4430);
  usbss_phy_cfg(0x13, 0x0010);
  USBSS_FUNC_CFG = 0xb0054000;
}

static void usbss_set_address(uint8_t dev_addr) {
  USBSSD->USB_CONTROL = (USBSSD->USB_CONTROL & ~USBSS_DEV_ADDR_MASK) | ((uint32_t)dev_addr << 24);
}

static void link_power_apply(void) {
  uint32_t link_cfg = USBSSD->LINK_CFG & ~(LINK_U1_ALLOW | LINK_U2_ALLOW);

  if (_dcd.force_pm || _dcd.u1_enabled) {
    link_cfg |= LINK_U1_ALLOW;
  }

  if (_dcd.force_pm || _dcd.u2_enabled) {
    link_cfg |= LINK_U2_ALLOW;
  }

  USBSSD->LINK_CFG = link_cfg;
}

static void link_power_set(bool u1_enabled, bool u2_enabled) {
  _dcd.u1_enabled = u1_enabled;
  _dcd.u2_enabled = u2_enabled;
  link_power_apply();
}

static void ep0_rx_ready(void) {
  USBSSD->UEP0_RX_CTRL = USBSS_EP0_RX_ERDY | USBSS_EP0_RX_ACK;
}

static void ep0_tx_ready(uint32_t value) {
  USBSSD->UEP0_TX_CTRL = USBSS_EP0_TX_DPH | value;
  USBSSD->UEP0_TX_CTRL |= USBSS_EP0_TX_ERDY;
}

static void ep0_prepare(void) {
  USBSSD->UEP0_TX_DMA     = (uint32_t)ep0_buffer;
  USBSSD->UEP0_RX_DMA     = (uint32_t)ep0_buffer;
  USBSSD->UEP0_TX_DMA_OFS = 0;
  USBSSD->UEP0_RX_DMA_OFS = 0;
  USBSSD->UEP0_TX_CTRL    = 0;
  ep0_rx_ready();

  xfer_ctl(0, TUSB_DIR_OUT)->max_size = 512;
  xfer_ctl(0, TUSB_DIR_IN)->max_size  = 512;
  _dcd.ep0_status_ep                  = EP0_STATUS_IDLE;
}

static void edpt_clear(uint8_t ep_num, tusb_dir_t dir) {
  if (ep_num == 0) {
    USBSSD->UEP0_TX_CTRL = 0;
    ep0_rx_ready();
  } else if (dir == TUSB_DIR_IN) {
    volatile USBSS_EP_TX_TypeDef *ep = ep_tx_reg(ep_num);
    ep->UEP_TX_CR                    = USBSS_EP_TX_CLR | USBSS_EP_TX_CHAIN_CLR;
    ep->UEP_TX_CR                    = EP_CHAIN_PACKET_LIMIT;
    ep->UEP_TX_ST                    = USBSS_EP_TX_INT_FLAG;
    ep->UEP_TX_CHAIN_ST              = USBSS_EP_TX_CHAIN_IF;
  } else {
    volatile USBSS_EP_RX_TypeDef *ep = ep_rx_reg(ep_num);
    ep->UEP_RX_CR                    = USBSS_EP_RX_CLR | USBSS_EP_RX_CHAIN_CLR;
    ep->UEP_RX_CR                    = EP_CHAIN_PACKET_LIMIT;
    ep->UEP_RX_ST                    = USBSS_EP_RX_INT_FLAG;
    ep->UEP_RX_CHAIN_ST              = USBSS_EP_RX_CHAIN_IF;
  }
}

static void endpoints_reset(void) {
  USBSSD->USB_CONTROL |= USBSS_USB_CLR_ALL;
  USBSSD->USB_CONTROL &= ~USBSS_USB_CLR_ALL;
  USBSSD->UEP_TX_EN = 0;
  USBSSD->UEP_RX_EN = 0;

  for (uint8_t ep = 1; ep < EP_MAX; ep++) {
    edpt_clear(ep, TUSB_DIR_IN);
    edpt_clear(ep, TUSB_DIR_OUT);
  }

  ep0_prepare();
}

static void protocol_reset(void) {
  USBSSD->USB_CONTROL |= USBSS_FORCE_RST;
  USBSSD->USB_STATUS  = USBSS_UIF_TRANSFER | USBSS_UDIF_SETUP | USBSS_UDIF_STATUS | USBSS_UIF_ITP | USBSS_UIF_RX_PING |
                        USBSS_UIF_FIFO_RXOV | USBSS_UIF_FIFO_TXOV;
  USBSSD->USB_CONTROL = USBSS_UIE_TRANSFER | USBSS_UDIE_SETUP | USBSS_UDIE_STATUS | USBSS_DMA_EN | USBSS_SETUP_FLOW;
}

static void usbss_hw_init(void) {
  usbss_clock_enable(true);
  usbss_phy_init();

  USBSSD->LINK_CFG  = LINK_RX_EQ_EN | LINK_TX_DEEMPH_MASK | LINK_PHY_RESET;
  USBSSD->LINK_CTRL = LINK_P2_MODE | LINK_GO_DISABLED;
  USBSSD->LINK_CFG  = LINK_RX_EQ_EN | LINK_TX_DEEMPH_MASK | LINK_LTSSM_MODE | LINK_TOUT_MODE;
  USBSSD->LINK_LPM_CR |= LINK_LPM_EN;
  USBSSD->LINK_CFG |= LINK_RX_TERM_EN;
  USBSSD->LINK_INT_CTRL = LINK_IE_TX_LMP | LINK_IE_RX_LMP | LINK_IE_RX_LMP_TOUT |
                          LINK_IE_STATE_CHG | LINK_IE_WARM_RST | LINK_IE_TERM_PRES;
  if (chip_has_rx_set_fc()) {
    USBSSD->LINK_INT_CTRL |= LINK_IE_RX_SET_FC;
  }
  USBSSD->LINK_CTRL     = LINK_P2_MODE;
  USBSSD->LINK_U1_WKUP_TMR    = 120;
  USBSSD->LINK_U1_WKUP_FILTER = 50;
  USBSSD->LINK_U2_WKUP_FILTER = 0;
  USBSSD->LINK_U3_WKUP_FILTER = 0;

  protocol_reset();
  endpoints_reset();
}

static void usbss_hw_deinit(void) {
  USBSSD->USB_CONTROL = USBSS_FORCE_RST;
  USBSSD->LINK_CFG |= LINK_PHY_RESET | U3_LINK_RESET;
  USBSSD->LINK_CTRL |= LINK_GO_DISABLED;
  delay_cycles(1000);
  USBSSD->USB_CONTROL &= ~USBSS_FORCE_RST;
  USBSSD->LINK_CFG &= ~(LINK_PHY_RESET | U3_LINK_RESET);
  usbss_clock_enable(false);
}

static void queue_ep0_xfer(uint8_t ep_addr, uint8_t *buffer, uint16_t total_bytes) {
  const tusb_dir_t dir  = tu_edpt_dir(ep_addr);
  xfer_ctl_t      *xfer = xfer_ctl(0, dir);

  xfer->buffer     = buffer;
  xfer->total_len  = total_bytes;
  xfer->queued_len = 0;

  if (total_bytes == 0) {
    _dcd.ep0_status_ep = ep_addr;
    if (dir == TUSB_DIR_IN) {
      ep0_tx_ready(0);
      ep0_rx_ready();
    } else {
      ep0_rx_ready();
    }
    return;
  }

  if (dir == TUSB_DIR_IN) {
    const uint16_t len = tu_min16(total_bytes, 512);
    memcpy(ep0_buffer, buffer, len);
    xfer->queued_len     = len;
    USBSSD->UEP0_TX_DMA  = (uint32_t)ep0_buffer;
    ep0_tx_ready(len);
  } else {
    USBSSD->UEP0_RX_DMA  = (uint32_t)ep0_buffer;
    ep0_rx_ready();
  }
}

static uint8_t chain_packet_count(uint16_t bytes, uint16_t max_size) {
  uint16_t packets = (bytes == 0) ? 1 : (uint16_t)((bytes + max_size - 1u) / max_size);
  return (uint8_t)tu_min16(packets, EP_CHAIN_PACKET_LIMIT);
}

static void queue_data_xfer(uint8_t ep_num, tusb_dir_t dir, xfer_ctl_t *xfer) {
  const uint16_t remaining   = (uint16_t)(xfer->total_len - xfer->queued_len);
  const uint8_t  nump        = chain_packet_count(remaining, xfer->max_size);
  const uint16_t chain_bytes = (remaining == 0) ? 0 : tu_min16(remaining, (uint16_t)(nump * xfer->max_size));
  const uint16_t last_len =
    (chain_bytes == 0) ? 0 : ((chain_bytes % xfer->max_size) ? (chain_bytes % xfer->max_size) : xfer->max_size);

  if (dir == TUSB_DIR_IN) {
    volatile USBSS_EP_TX_TypeDef *ep = ep_tx_reg(ep_num);
    ep->UEP_TX_DMA                   = (uint32_t)(chain_bytes ? &xfer->buffer[xfer->queued_len] : ep0_buffer);
    ep->UEP_TX_DMA_OFS               = xfer->max_size;
    ep->UEP_TX_CHAIN_LEN             = last_len;
    ep->UEP_TX_CHAIN_EXP_NUMP        = nump;
    ep->UEP_TX_CHAIN_ST              = USBSS_EP_TX_CHAIN_IF;
    xfer->queued_len                 = (uint16_t)(xfer->queued_len + chain_bytes);
  } else {
    volatile USBSS_EP_RX_TypeDef *ep = ep_rx_reg(ep_num);
    ep->UEP_RX_DMA                   = (uint32_t)(chain_bytes ? &xfer->buffer[xfer->queued_len] : ep0_buffer);
    ep->UEP_RX_DMA_OFS               = xfer->max_size;
    ep->UEP_RX_CHAIN_MAX_NUMP        = nump;
    ep->UEP_RX_CHAIN_ST              = USBSS_EP_RX_CHAIN_IF;
  }
}

static void handle_ep0_in(uint8_t rhport) {
  xfer_ctl_t *xfer = xfer_ctl(0, TUSB_DIR_IN);

  if (xfer->queued_len >= xfer->total_len) {
    USBSSD->UEP0_TX_CTRL = USBSS_EP0_TX_DPH;
    ep0_rx_ready();
    dcd_event_xfer_complete(rhport, TU_EP0_IN, xfer->queued_len, XFER_RESULT_SUCCESS, true);
    return;
  }

  const uint16_t len = tu_min16((uint16_t)(xfer->total_len - xfer->queued_len), 512);
  const uint8_t  seq = (uint8_t)(((USBSSD->UEP0_TX_CTRL >> 16) & 0x1fu) + 1u);
  memcpy(ep0_buffer, &xfer->buffer[xfer->queued_len], len);
  xfer->queued_len     = (uint16_t)(xfer->queued_len + len);
  ep0_tx_ready(((uint32_t)seq << 16) | len);
}

static void handle_ep0_out(uint8_t rhport) {
  xfer_ctl_t    *xfer   = xfer_ctl(0, TUSB_DIR_OUT);
  const uint16_t rx_len = (uint16_t)(USBSSD->UEP0_RX_CTRL & USBSS_EP0_RX_LEN_MASK);

  if (rx_len > 0 && xfer->buffer != NULL) {
    memcpy(&xfer->buffer[xfer->queued_len], ep0_buffer, rx_len);
  }
  xfer->queued_len = (uint16_t)(xfer->queued_len + rx_len);
  ep0_rx_ready();

  if (xfer->queued_len >= xfer->total_len || rx_len < xfer->max_size) {
    dcd_event_xfer_complete(rhport, TU_EP0_OUT, xfer->queued_len, XFER_RESULT_SUCCESS, true);
  }
}

static void handle_data_in(uint8_t rhport, uint8_t ep_num) {
  volatile USBSS_EP_TX_TypeDef *ep   = ep_tx_reg(ep_num);
  xfer_ctl_t                   *xfer = xfer_ctl(ep_num, TUSB_DIR_IN);

  ep->UEP_TX_CHAIN_ST = USBSS_EP_TX_CHAIN_IF;
  if (xfer->queued_len >= xfer->total_len) {
    dcd_event_xfer_complete(rhport, tu_edpt_addr(ep_num, TUSB_DIR_IN), xfer->queued_len, XFER_RESULT_SUCCESS, true);
  } else {
    queue_data_xfer(ep_num, TUSB_DIR_IN, xfer);
  }
}

static void handle_data_out(uint8_t rhport, uint8_t ep_num) {
  volatile USBSS_EP_RX_TypeDef *ep   = ep_rx_reg(ep_num);
  xfer_ctl_t                   *xfer = xfer_ctl(ep_num, TUSB_DIR_OUT);

  uint16_t      rx_len  = ep->UEP_RX_CHAIN_LEN;
  const uint8_t rx_nump = ep->UEP_RX_CHAIN_NUMP;
  if (rx_nump > 1) {
    rx_len = (uint16_t)(((uint16_t)(rx_nump - 1u) * xfer->max_size) + rx_len);
  }

  ep->UEP_RX_CHAIN_ST = USBSS_EP_RX_CHAIN_IF;
  xfer->queued_len    = (uint16_t)(xfer->queued_len + rx_len);

  if (xfer->queued_len >= xfer->total_len || rx_len < (uint16_t)(rx_nump * xfer->max_size)) {
    dcd_event_xfer_complete(rhport, tu_edpt_addr(ep_num, TUSB_DIR_OUT), xfer->queued_len, XFER_RESULT_SUCCESS, true);
  } else {
    queue_data_xfer(ep_num, TUSB_DIR_OUT, xfer);
  }
}

static void handle_transfer(uint8_t rhport, uint32_t status) {
  const uint8_t    ep_num = (uint8_t)((status & USBSS_EP_ID_MASK) >> 8);
  const tusb_dir_t dir    = (status & USBSS_EP_DIR_MASK) ? TUSB_DIR_IN : TUSB_DIR_OUT;

  if (ep_num == 0) {
    if (dir == TUSB_DIR_IN) {
      handle_ep0_in(rhport);
    } else {
      handle_ep0_out(rhport);
    }
  } else if (dir == TUSB_DIR_IN) {
    handle_data_in(rhport, ep_num);
  } else {
    handle_data_out(rhport, ep_num);
  }
}

static void handle_link_reset(uint8_t rhport) {
  protocol_reset();
  endpoints_reset();
  usbss_set_address(0);
  _dcd.dev_addr         = 0;
  _dcd.set_addr_pending = false;
  _dcd.link_configured  = false;
  _dcd.link_low_power   = false;
  _dcd.link_reset_seen  = true;
  _dcd.last_lmp_subtype = 0;
  _dcd.force_pm         = false;
  link_power_set(false, false);
  dcd_event_bus_reset(rhport, TUSB_SPEED_SUPER, true);
}

static void handle_link_configured(uint8_t rhport) {
  if (!_dcd.link_reset_seen) {
    // U0 can be the first reliable indication that SuperSpeed link training completed.
    // Notify TinyUSB of SS speed without resetting the USBSS protocol/endpoint state.
    _dcd.link_reset_seen = true;
    dcd_event_bus_reset(rhport, TUSB_SPEED_SUPER, true);
  }

  _dcd.link_configured = true;
}

static void tx_port_cap_lmp(void) {
  USBSSD->LINK_LMP_TX_DATA0 = LMP_LINK_SPEED | LMP_PORT_CAP | LMP_HP;
  USBSSD->LINK_LMP_TX_DATA1 = LMP_UP_STREAM | LMP_NUM_HP_BUF;
  USBSSD->LINK_LMP_TX_DATA2 = 0;
}

static void tx_port_cfg_res_lmp(void) {
  USBSSD->LINK_LMP_TX_DATA0 = LMP_LINK_SPEED | LMP_PORT_CFG_RES | LMP_HP;
  USBSSD->LINK_LMP_TX_DATA1 = 0;
  USBSSD->LINK_LMP_TX_DATA2 = 0;
  USBSSD->LINK_LMP_PORT_CAP |= LINK_LMP_TX_CAP_VLD;
}

static void handle_link_irq(uint8_t rhport) {
  const uint32_t link_int   = USBSSD->LINK_INT_FLAG;
  const uint32_t link_state = USBSSD->LINK_STATUS & LINK_STATE_MASK;

  if (chip_has_rx_set_fc() && (link_int & LINK_IF_RX_SET_FC)) {
    // Revision 3+ reports forced power-management policy through RX_SET_FC.
    USBSSD->LINK_INT_FLAG = LINK_IF_RX_SET_FC;
    _dcd.force_pm = (USBSSD->LINK_LMP_PORT_CAP & FORCE_PM) != 0;
    link_power_apply();
  }

  if (link_int & LINK_IF_STATE_CHG) {
    USBSSD->LINK_INT_FLAG = LINK_IF_STATE_CHG;

    if (link_state == LINK_STATE_RXDET) {
      _dcd.link_configured = false;
      _dcd.link_low_power = false;
      _dcd.link_reset_seen = false;
      _dcd.force_pm = false;
      link_power_set(false, false);
    } else if (link_state == LINK_STATE_RECOVERY) {
      if (_dcd.link_low_power) {
        _dcd.link_low_power = false;
        delay_cycles(4000);
        usbss_phy_cfg(0x12, 0x67c8);
      }
    } else if (link_state == LINK_STATE_DISABLE) {
      USBSSD->LINK_CTRL &= ~LINK_GO_DISABLED;
    } else if (link_state == LINK_STATE_U0) {
      handle_link_configured(rhport);
    } else if (link_state == LINK_STATE_U1 || link_state == LINK_STATE_U2 || link_state == LINK_STATE_U3) {
      _dcd.link_low_power = true;
      usbss_phy_cfg(0x12, (uint16_t)(0x67c8 & ~(1u << 9)));
    } else if (link_state == LINK_STATE_HOTRST) {
      handle_link_reset(rhport);
      USBSSD->LINK_CTRL &= ~LINK_HOT_RESET;
    }
  }

  if (link_int & LINK_IF_TERM_PRES) {
    USBSSD->LINK_INT_FLAG = LINK_IF_TERM_PRES;
    if ((USBSSD->LINK_STATUS & LINK_RX_TERM_PRES) == 0) {
      dcd_event_bus_signal(rhport, DCD_EVENT_UNPLUGGED, true);
      _dcd.link_reset_seen = false;
      _dcd.link_configured = false;
    }
  }

  if (link_int & LINK_IF_TX_LMP) {
    USBSSD->LINK_INT_FLAG = LINK_IF_TX_LMP;
    tx_port_cap_lmp();
  }

  if (link_int & LINK_IF_RX_LMP) {
    USBSSD->LINK_INT_FLAG = LINK_IF_RX_LMP;

    const uint32_t lmp_data0 = USBSSD->LINK_LMP_RX_DATA0;
    _dcd.last_lmp_subtype = lmp_data0 & LMP_SUBTYPE_MASK;

    if (_dcd.last_lmp_subtype == LMP_PORT_CAP) {
      tx_port_cap_lmp();
    } else if (_dcd.last_lmp_subtype == LMP_PORT_CFG) {
      tx_port_cfg_res_lmp();
      _dcd.link_configured = true;
    } else if (_dcd.last_lmp_subtype == LMP_PORT_CFG_RES) {
      USBSSD->LINK_LMP_PORT_CAP |= LINK_LMP_TX_CAP_VLD;
      _dcd.link_configured = true;
    } else if (_dcd.last_lmp_subtype == LMP_U2_INACT_TOUT) {
      USBSSD->LINK_U2_INACT_TIMER = (uint8_t)((lmp_data0 >> 9) & 0xffu);
    } else if (!chip_has_rx_set_fc() && _dcd.last_lmp_subtype == LMP_SET_LINK_FUNC) {
      // Pre-revision-3 silicon uses SET_LINK_FUNC instead of RX_SET_FC.
      if (lmp_data0 & (0x02u << 9)) {
        link_power_set(true, true);
      } else {
        link_power_set(false, false);
      }
    }
  }

  if (link_int & LINK_IF_RX_LMP_TOUT) {
    USBSSD->LINK_INT_FLAG = LINK_IF_RX_LMP_TOUT;
    USBSSD->LINK_CTRL |= LINK_GO_DISABLED;
    USBSSD->LINK_CTRL |= LINK_GO_RX_DET;
    // Timeout recovery changes LTSSM state; ignore any stale flags from this snapshot.
    return;
  }

  if (link_int & LINK_IF_WARM_RST) {
    USBSSD->LINK_INT_FLAG = LINK_IF_WARM_RST;
    if (USBSSD->LINK_STATUS & LINK_RX_WARM_RST) {
      handle_link_reset(rhport);
      USBSSD->LINK_CTRL |= LINK_GO_DISABLED;
      delay_cycles(16);
      USBSSD->LINK_CTRL &= ~LINK_GO_DISABLED;
    }
  }
}

bool dcd_init(uint8_t rhport, const tusb_rhport_init_t *rh_init) {
  (void)rhport;
  (void)rh_init;

  memset(&_dcd, 0, sizeof(_dcd));
  _dcd.ep0_status_ep = EP0_STATUS_IDLE;
  _dcd.chip_revision = chip_revision_get();
  usbss_hw_init();
  return true;
}

bool dcd_deinit(uint8_t rhport) {
  (void)rhport;
  dcd_int_disable(rhport);
  usbss_hw_deinit();
  return true;
}

void dcd_int_enable(uint8_t rhport) {
  (void)rhport;
  NVIC_EnableIRQ(USBSS_IRQn);
  NVIC_EnableIRQ(USBSS_LINK_IRQn);
}

void dcd_int_disable(uint8_t rhport) {
  (void)rhport;
  NVIC_DisableIRQ(USBSS_LINK_IRQn);
  NVIC_DisableIRQ(USBSS_IRQn);
}

void dcd_connect(uint8_t rhport) {
  (void)rhport;
  USBSSD->LINK_CTRL &= ~LINK_GO_DISABLED;
  USBSSD->LINK_CFG |= LINK_RX_TERM_EN;
}

void dcd_disconnect(uint8_t rhport) {
  (void)rhport;
  USBSSD->LINK_CFG &= ~LINK_RX_TERM_EN;
  USBSSD->LINK_CTRL |= LINK_GO_DISABLED;
}

void dcd_set_address(uint8_t rhport, uint8_t dev_addr) {
  _dcd.dev_addr = dev_addr;
  _dcd.set_addr_pending = true;
  dcd_edpt_xfer(rhport, TU_EP0_IN, NULL, 0, false);
}

void dcd_edpt0_status_complete(uint8_t rhport, const tusb_control_request_t *request) {
  (void)rhport;

  if (_dcd.set_addr_pending &&
      request->bmRequestType_bit.recipient == TUSB_REQ_RCPT_DEVICE &&
      request->bmRequestType_bit.type == TUSB_REQ_TYPE_STANDARD &&
      request->bRequest == TUSB_REQ_SET_ADDRESS) {
    usbss_set_address(_dcd.dev_addr);
    _dcd.set_addr_pending = false;
  } else if (request->bmRequestType_bit.recipient == TUSB_REQ_RCPT_DEVICE &&
             request->bmRequestType_bit.type == TUSB_REQ_TYPE_STANDARD) {
    if (request->bRequest == TUSB_REQ_SET_FEATURE) {
      if (request->wValue == TUSB_REQ_FEATURE_U1_ENABLE) {
        _dcd.u1_enabled = true;
        link_power_apply();
      } else if (request->wValue == TUSB_REQ_FEATURE_U2_ENABLE) {
        _dcd.u2_enabled = true;
        link_power_apply();
      }
    } else if (request->bRequest == TUSB_REQ_CLEAR_FEATURE) {
      if (request->wValue == TUSB_REQ_FEATURE_U1_ENABLE) {
        _dcd.u1_enabled = false;
        link_power_apply();
      } else if (request->wValue == TUSB_REQ_FEATURE_U2_ENABLE) {
        _dcd.u2_enabled = false;
        link_power_apply();
      }
    } else if (request->bRequest == TUSB_REQ_SET_ISOCH_DELAY) {
      USBSSD->LINK_ISO_DLY = request->wValue;
    }
  }
}

void dcd_remote_wakeup(uint8_t rhport) {
  (void)rhport;
  USBSSD->LINK_CTRL |= LINK_TX_UX_EXIT;
}

void dcd_sof_enable(uint8_t rhport, bool en) {
  (void)rhport;
  if (en) {
    USBSSD->USB_CONTROL |= USBSS_UIE_ITP | USBSS_ITP_EN;
  } else {
    USBSSD->USB_CONTROL &= ~(USBSS_UIE_ITP | USBSS_ITP_EN);
  }
}

void dcd_edpt_close_all(uint8_t rhport) {
  (void)rhport;

  USBSSD->UEP_TX_EN = 0;
  USBSSD->UEP_RX_EN = 0;
  for (uint8_t ep = 1; ep < EP_MAX; ep++) {
    edpt_clear(ep, TUSB_DIR_IN);
    edpt_clear(ep, TUSB_DIR_OUT);
    memset(&_dcd.xfer[ep], 0, sizeof(_dcd.xfer[ep]));
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

  xfer_ctl_t *xfer = xfer_ctl(ep_num, dir);
  xfer->max_size = tu_edpt_packet_size(desc_edpt);
  const bool is_iso = desc_edpt->bmAttributes.xfer == TUSB_XFER_ISOCHRONOUS;

  if (dir == TUSB_DIR_IN) {
    volatile USBSS_EP_TX_TypeDef *ep = ep_tx_reg(ep_num);
    ep->UEP_TX_CFG =
      USBSS_EP_TX_CHAIN_AUTO | USBSS_EP_TX_ERDY_AUTO | USBSS_EP_TX_SEQ_AUTO | (is_iso ? USBSS_EP_TX_ISO_MODE : 0);
    ep->UEP_TX_CR = USBSS_EP_TX_CLR | USBSS_EP_TX_CHAIN_CLR;
    ep->UEP_TX_CR = EP_CHAIN_PACKET_LIMIT;
    USBSSD->UEP_TX_EN |= (uint16_t)(1u << ep_num);
  } else {
    volatile USBSS_EP_RX_TypeDef *ep = ep_rx_reg(ep_num);
    ep->UEP_RX_CFG =
      USBSS_EP_RX_CHAIN_AUTO | USBSS_EP_RX_ERDY_AUTO | USBSS_EP_RX_SEQ_AUTO | (is_iso ? USBSS_EP_RX_ISO_MODE : 0);
    ep->UEP_RX_CR = USBSS_EP_RX_CLR | USBSS_EP_RX_CHAIN_CLR;
    ep->UEP_RX_CR = EP_CHAIN_PACKET_LIMIT;
    USBSSD->UEP_RX_EN |= (uint16_t)(1u << ep_num);
  }

  return true;
}

void dcd_edpt_close(uint8_t rhport, uint8_t ep_addr) {
  (void)rhport;

  const uint8_t    ep_num = tu_edpt_number(ep_addr);
  const tusb_dir_t dir    = tu_edpt_dir(ep_addr);

  if (ep_num == 0 || ep_num >= EP_MAX) {
    return;
  }

  if (dir == TUSB_DIR_IN) {
    USBSSD->UEP_TX_EN &= (uint16_t)~(1u << ep_num);
  } else {
    USBSSD->UEP_RX_EN &= (uint16_t)~(1u << ep_num);
  }
  edpt_clear(ep_num, dir);
}

bool dcd_edpt_xfer(uint8_t rhport, uint8_t ep_addr, uint8_t *buffer, uint16_t total_bytes, bool is_isr) {
  (void)rhport;
  (void)is_isr;

  const uint8_t    ep_num = tu_edpt_number(ep_addr);
  const tusb_dir_t dir    = tu_edpt_dir(ep_addr);
  TU_ASSERT(ep_num < EP_MAX);

  if (ep_num == 0) {
    queue_ep0_xfer(ep_addr, buffer, total_bytes);
    return true;
  }

  xfer_ctl_t *xfer = xfer_ctl(ep_num, dir);
  xfer->buffer     = buffer;
  xfer->total_len  = total_bytes;
  xfer->queued_len = 0;
  queue_data_xfer(ep_num, dir, xfer);

  return true;
}

void dcd_edpt_stall(uint8_t rhport, uint8_t ep_addr) {
  (void)rhport;

  const uint8_t    ep_num = tu_edpt_number(ep_addr);
  const tusb_dir_t dir    = tu_edpt_dir(ep_addr);

  if (ep_num == 0) {
    if (dir == TUSB_DIR_IN) {
      USBSSD->UEP0_TX_CTRL = USBSS_EP0_TX_STALL;
    } else {
      USBSSD->UEP0_RX_CTRL = USBSS_EP0_RX_ERDY | USBSS_EP0_RX_STALL;
    }
  } else if (dir == TUSB_DIR_IN) {
    volatile USBSS_EP_TX_TypeDef *ep = ep_tx_reg(ep_num);
    uint8_t cr = (uint8_t)((ep->UEP_TX_CR & (uint8_t)~USBSS_EP_TX_ERDY_NUMP_MASK) |
                           USBSS_EP_TX_HALT | EP_CHAIN_PACKET_LIMIT);
    ep->UEP_TX_CR = cr;
    // Wake a flow-controlled endpoint so the next host service attempt sees STALL.
    ep->UEP_TX_ST = USBSS_EP_TX_ERDY_REQ;
  } else {
    volatile USBSS_EP_RX_TypeDef *ep = ep_rx_reg(ep_num);
    uint8_t cr = (uint8_t)((ep->UEP_RX_CR & (uint8_t)~USBSS_EP_RX_ERDY_NUMP_MASK) |
                           USBSS_EP_RX_HALT | EP_CHAIN_PACKET_LIMIT);
    ep->UEP_RX_CR = cr;
    // Wake a flow-controlled endpoint so the next host service attempt sees STALL.
    ep->UEP_RX_ST = USBSS_EP_RX_ERDY_REQ;
  }
}

void dcd_edpt_clear_stall(uint8_t rhport, uint8_t ep_addr) {
  (void)rhport;

  const uint8_t    ep_num = tu_edpt_number(ep_addr);
  const tusb_dir_t dir    = tu_edpt_dir(ep_addr);

  if (ep_num == 0) {
    ep0_prepare();
  } else if (dir == TUSB_DIR_IN) {
    edpt_clear(ep_num, TUSB_DIR_IN);
  } else {
    edpt_clear(ep_num, TUSB_DIR_OUT);
  }
}

void dcd_int_handler(uint8_t rhport) {
  handle_link_irq(rhport);

  const uint32_t status = USBSSD->USB_STATUS;

  if ((status & USBSS_UDIF_SETUP) && !(status & USBSS_UDIF_STATUS)) {
    _dcd.ep0_status_ep = EP0_STATUS_IDLE;
    _dcd.set_addr_pending = false;
    USBSSD->UEP0_TX_CTRL = 0;
    USBSSD->UEP0_RX_CTRL = 0;

    USBSSD->USB_STATUS = USBSS_UDIF_SETUP;
    dcd_event_setup_received(rhport, ep0_buffer, true);
  } else if (status & USBSS_UDIF_STATUS) {
    USBSSD->USB_STATUS = USBSS_UDIF_STATUS;

    if (_dcd.set_addr_pending) {
      usbss_set_address(_dcd.dev_addr);
      _dcd.set_addr_pending = false;
    }

    USBSSD->UEP0_TX_CTRL = 0;
    ep0_rx_ready();

    if (_dcd.ep0_status_ep != EP0_STATUS_IDLE) {
      const uint8_t ep_addr = _dcd.ep0_status_ep;
      _dcd.ep0_status_ep    = EP0_STATUS_IDLE;
      dcd_event_xfer_complete(rhport, ep_addr, 0, XFER_RESULT_SUCCESS, true);
    }
  } else if (status & USBSS_UIF_TRANSFER) {
    handle_transfer(rhport, status);
  } else if ((status & USBSS_UIF_ITP) && (USBSSD->USB_CONTROL & USBSS_UIE_ITP)) {
    dcd_event_sof(rhport, USBSSD->USB_ITP & USBSS_ITP_INTERVAL_MASK, true);
    USBSSD->USB_STATUS = USBSS_UIF_ITP;
  } else if (status & (USBSS_UIF_FIFO_RXOV | USBSS_UIF_FIFO_TXOV | USBSS_UIF_RX_PING)) {
    USBSSD->USB_STATUS = status & (USBSS_UIF_FIFO_RXOV | USBSS_UIF_FIFO_TXOV | USBSS_UIF_RX_PING);
  }
}

#endif
