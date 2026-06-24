/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2023 Ha Thach (tinyusb.org)
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
 */

#include "tusb_option.h"
#include "typec/tcd.h"

#if CFG_TUC_ENABLED && defined(TUP_USBIP_TYPEC_STM32)

#include "common/tusb_common.h"

#if CFG_TUSB_MCU == OPT_MCU_STM32G4
  #include "stm32g4xx.h"
  #include "stm32g4xx_ll_dma.h" // for UCPD REQID
#elif CFG_TUSB_MCU == OPT_MCU_STM32U5
  #include "stm32u5xx.h"
  #include "stm32u5xx_ll_dma.h" // for UCPD REQID
  #include "stm32u5xx_ll_system.h" // for DevID/RevID
#else
  #error "Unsupported STM32 family"
#endif

//--------------------------------------------------------------------+
//
//--------------------------------------------------------------------+

enum {
  IMR_ATTACHED = UCPD_IMR_TXMSGDISCIE | UCPD_IMR_TXMSGSENTIE | UCPD_IMR_TXMSGABTIE | UCPD_IMR_TXUNDIE |
                 UCPD_IMR_RXHRSTDETIE | UCPD_IMR_RXOVRIE | UCPD_IMR_RXMSGENDIE | UCPD_IMR_RXORDDETIE |
                 UCPD_IMR_HRSTDISCIE | UCPD_IMR_HRSTSENTIE | UCPD_IMR_FRSEVTIE,

  RX_ORDERED_SET_SOP = 0
};

#define PHY_SYNC1 0x18u
#define PHY_SYNC2 0x11u
#define PHY_SYNC3 0x06u
#define PHY_RST1  0x07u
#define PHY_RST2  0x19u
#define PHY_EOP   0x0Du

#define PHY_ORDERED_SET_SOP          (PHY_SYNC1 | (PHY_SYNC1<<5u) | (PHY_SYNC1<<10u) | (PHY_SYNC2<<15u)) // SOP Ordered set coding
#define PHY_ORDERED_SET_SOP_P        (PHY_SYNC1 | (PHY_SYNC1<<5u) | (PHY_SYNC3<<10u) | (PHY_SYNC3<<15u)) // SOP' Ordered set coding
#define PHY_ORDERED_SET_SOP_PP       (PHY_SYNC1 | (PHY_SYNC3<<5u) | (PHY_SYNC1<<10u) | (PHY_SYNC3<<15u)) // SOP'' Ordered set coding
#define PHY_ORDERED_SET_HARD_RESET   (PHY_RST1  | (PHY_RST1<<5u)  | (PHY_RST1<<10u)  | (PHY_RST2<<15u )) // Hard Reset Ordered set coding
#define PHY_ORDERED_SET_CABLE_RESET  (PHY_RST1  | (PHY_SYNC1<<5u) | (PHY_RST1<<10u)  | (PHY_SYNC3<<15u)) // Cable Reset Ordered set coding
#define PHY_ORDERED_SET_SOP_P_DEBUG  (PHY_SYNC1 | (PHY_RST2<<5u)  | (PHY_RST2<<10u)  | (PHY_SYNC3<<15u)) // SOP' Debug Ordered set coding
#define PHY_ORDERED_SET_SOP_PP_DEBUG (PHY_SYNC1 | (PHY_RST2<<5u)  | (PHY_SYNC3<<10u) | (PHY_SYNC2<<15u)) // SOP'' Debug Ordered set coding


static uint8_t* _rx_buf;
static uint16_t _rx_buf_len;
static uint8_t const* _tx_pending_buf;
static uint16_t _tx_pending_bytes;
static uint16_t _tx_xferring_bytes;
static bool _cc_enabled[TUP_TYPEC_RHPORTS_NUM];
static uint32_t _rx_ordered_set;

static pd_header_t _good_crc = {
    .msg_type   = PD_CTRL_GOOD_CRC,
    .data_role  = 0, // UFP
    .specs_rev  = PD_REV_20,
    .power_role = 0, // Sink
    .msg_id     = 0,
    .n_data_obj = 0,
    .extended   = 0
};

#ifdef DMA1
// address of DMA channel rx, tx for each port
#define CFG_TUC_STM32_DMA  { { DMA1_Channel1_BASE, DMA1_Channel2_BASE } }
#elif defined(GPDMA1)
// address of DMA channel rx, tx for each port
#define CFG_TUC_STM32_DMA  { { GPDMA1_Channel0_BASE, GPDMA1_Channel1_BASE } }
#else
#error "Unsupported STM32 family"
#endif

//--------------------------------------------------------------------+
// DMA
//--------------------------------------------------------------------+

static const uint32_t _dma_addr_arr[TUP_TYPEC_RHPORTS_NUM][2] = CFG_TUC_STM32_DMA;

TU_ATTR_ALWAYS_INLINE static inline uint32_t dma_get_addr(uint8_t rhport, bool is_rx) {
  return _dma_addr_arr[rhport][is_rx ? 0 : 1];
}

static void dma_init(uint8_t rhport, bool is_rx) {
  (void) rhport;
  (void) is_rx;

#if CFG_TUSB_MCU == OPT_MCU_STM32G4
  uint32_t dma_addr = dma_get_addr(rhport, is_rx);
  DMA_Channel_TypeDef* dma_ch = (DMA_Channel_TypeDef*) dma_addr;
  uint32_t req_id;

  if (is_rx) {
    // Peripheral -> Memory, Memory inc, 8-bit, High priority
    dma_ch->CCR = DMA_CCR_MINC | DMA_CCR_PL_1;
    dma_ch->CPAR = (uint32_t) &UCPD1->RXDR;

    req_id = LL_DMAMUX_REQ_UCPD1_RX;
  } else {
    // Memory -> Peripheral, Memory inc, 8-bit, High priority
    dma_ch->CCR = DMA_CCR_MINC | DMA_CCR_PL_1 | DMA_CCR_DIR;
    dma_ch->CPAR = (uint32_t) &UCPD1->TXDR;

    req_id = LL_DMAMUX_REQ_UCPD1_TX;
  }

  // find and set up mux channel TODO support mcu with multiple DMAMUXs
  enum {
    CH_DIFF = DMA1_Channel2_BASE - DMA1_Channel1_BASE
  };
  uint32_t mux_ch_num;

  #ifdef DMA2_BASE
  if (dma_addr > DMA2_BASE) {
    mux_ch_num = 8 * ((dma_addr - DMA2_Channel1_BASE) / CH_DIFF);
  } else
  #endif
  {
    mux_ch_num = (dma_addr - DMA1_Channel1_BASE) / CH_DIFF;
  }

  DMAMUX_Channel_TypeDef* mux_ch = DMAMUX1_Channel0 + mux_ch_num;

  uint32_t mux_ccr = mux_ch->CCR & ~(DMAMUX_CxCR_DMAREQ_ID);
  mux_ccr |= req_id;
  mux_ch->CCR = mux_ccr;
#endif
}

TU_ATTR_ALWAYS_INLINE static inline void dma_start(uint8_t rhport, bool is_rx, void const* buf, uint16_t len) {
  DMA_Channel_TypeDef* dma_ch = (DMA_Channel_TypeDef*) dma_get_addr(rhport, is_rx);
#if CFG_TUSB_MCU == OPT_MCU_STM32G4
  dma_ch->CMAR = (uint32_t) buf;
  dma_ch->CNDTR = len;
  dma_ch->CCR |= DMA_CCR_EN;
#elif CFG_TUSB_MCU == OPT_MCU_STM32U5
  if (is_rx) {
    // Peripheral -> Memory, Memory inc, 8-bit
    dma_ch->CTR1 = DMA_CTR1_DINC | DMA_CTR1_DAP;
    dma_ch->CTR2 = LL_GPDMA1_REQUEST_UCPD1_RX;
    dma_ch->CSAR = (uint32_t) &UCPD1->RXDR;
    dma_ch->CDAR = (uint32_t) buf;
    dma_ch->CBR1 = len;
  } else {
    // Memory -> Peripheral, Memory inc, 8-bit
    dma_ch->CTR1 = DMA_CTR1_SINC | DMA_CTR1_DAP;
    dma_ch->CTR2 = DMA_CTR2_DREQ | LL_GPDMA1_REQUEST_UCPD1_TX;
    dma_ch->CSAR = (uint32_t) buf;
    dma_ch->CDAR = (uint32_t) &UCPD1->TXDR;
    dma_ch->CBR1 = len;
  }
  // High priority
  dma_ch->CCR = DMA_CCR_PRIO | DMA_CCR_EN;
#endif
}

TU_ATTR_ALWAYS_INLINE static inline void dma_stop(uint8_t rhport, bool is_rx) {
  DMA_Channel_TypeDef* dma_ch = (DMA_Channel_TypeDef*) dma_get_addr(rhport, is_rx);
#if CFG_TUSB_MCU == OPT_MCU_STM32G4
  dma_ch->CCR &= ~DMA_CCR_EN;
#elif CFG_TUSB_MCU == OPT_MCU_STM32U5
  dma_ch->CCR |= DMA_CCR_SUSP;
  while((dma_ch->CSR & (DMA_CSR_SUSPF | DMA_CSR_IDLEF)) == 0U);
  dma_ch->CCR = DMA_CCR_RESET;
#endif
}

TU_ATTR_ALWAYS_INLINE static inline bool dma_enabled(uint8_t rhport, bool is_rx) {
  DMA_Channel_TypeDef* dma_ch = (DMA_Channel_TypeDef*) dma_get_addr(rhport, is_rx);
  return dma_ch->CCR & DMA_CCR_EN;
}

TU_ATTR_ALWAYS_INLINE static inline void dma_tx_start(uint8_t rhport, void const* buf, uint16_t len) {
  UCPD1->TX_ORDSET = PHY_ORDERED_SET_SOP;
  UCPD1->TX_PAYSZ = len;
  dma_start(rhport, false, buf, len);
  UCPD1->CR |= UCPD_CR_TXSEND;
}

TU_ATTR_ALWAYS_INLINE static inline void dma_tx_stop(uint8_t rhport) {
  dma_stop(rhport, false);
}

//--------------------------------------------------------------------+
//
//--------------------------------------------------------------------+

bool tcd_init(uint8_t rhport, uint32_t port_type) {
  (void) rhport;

  // Init DMA for RX, TX
  dma_init(rhport, true);
  dma_init(rhport, false);

  // Initialization phase: CFG1, detect all SOPs
  UCPD1->CFG1 = (0x0d << UCPD_CFG1_HBITCLKDIV_Pos) | (0x10 << UCPD_CFG1_IFRGAP_Pos) | (0x07 << UCPD_CFG1_TRANSWIN_Pos) |
                (0x01 << UCPD_CFG1_PSC_UCPDCLK_Pos) | (0x1f << UCPD_CFG1_RXORDSETEN_Pos);
  UCPD1->CFG1 |= UCPD_CFG1_UCPDEN;

#ifdef UCPD_CFG2_RXAFILTEN
  // Enable Rx analog filter
  UCPD1->CFG2 |= UCPD_CFG2_RXAFILTEN;
#endif

  // The CC pull-up (Rp) and pull-down (Rd) must be trimmed for some STM32U5 devices.
#ifdef UCPD_CFG3_TRIM_CC1_RP
  uint32_t dev_id = LL_DBGMCU_GetDeviceID();
  uint32_t rev_id = LL_DBGMCU_GetRevisionID();

  // Values taken from STM32U5 UCPD middleware.
  // The list might be not complete, a case has been raised to ST.
  if (((dev_id == 0x482UL) && (rev_id == 0x3000UL)) ||
      ((dev_id == 0x481UL) && (rev_id == 0x2001UL)) ||
      ((dev_id == 0x481UL) && (rev_id == 0x3000UL)) ||
      ((dev_id == 0x481UL) && (rev_id == 0x3001UL)) ||
      ((dev_id == 0x476UL) && (rev_id == 0x1000UL))) {
    const uint8_t trim_3a0_cc1 = (*(uint8_t*) 0x0BFA0545) & 0x0F;
    const uint8_t trim_3a0_cc2 = (*(uint8_t*) 0x0BFA0547) & 0x0F;
    const uint8_t trim_rd_cc1  = (*(uint8_t*) 0x0BFA0544) & 0x0F;
    const uint8_t trim_rd_cc2  = (*(uint8_t*) 0x0BFA0546) & 0x0F;

    UCPD1->CFG3 = trim_3a0_cc1 << UCPD_CFG3_TRIM_CC1_RP_Pos | trim_3a0_cc2 << UCPD_CFG3_TRIM_CC2_RP_Pos |
                  trim_rd_cc1  << UCPD_CFG3_TRIM_CC1_RD_Pos | trim_rd_cc2  << UCPD_CFG3_TRIM_CC2_RD_Pos;
  }
#endif

  // General programming sequence (with UCPD configured then enabled)
  if (port_type == TUSB_TYPEC_PORT_SNK) {
    tcd_connect(rhport);
  }

#if CFG_TUSB_MCU == OPT_MCU_STM32G4
  // Disable dead battery in PWR's CR3
  PWR->CR3 |= PWR_CR3_UCPD_DBDIS;
#elif CFG_TUSB_MCU == OPT_MCU_STM32U5
  // Disable dead battery in PWR's UCPDR
  PWR->UCPDR |= PWR_UCPDR_UCPD_DBDIS;
#endif

  return true;
}

// Enable interrupt
void tcd_int_enable(uint8_t rhport) {
  (void) rhport;
  NVIC_EnableIRQ(UCPD1_IRQn);
}

// Disable interrupt
void tcd_int_disable(uint8_t rhport) {
  (void) rhport;
  NVIC_DisableIRQ(UCPD1_IRQn);
}

void tcd_connect(uint8_t rhport) {
  if (_cc_enabled[rhport]) {
    return;
  }

  _cc_enabled[rhport] = true;

  // Set analog mode and enable both CC Phy as sink.
  uint32_t cr = UCPD1->CR;
  cr &= ~(UCPD_CR_PHYRXEN | UCPD_CR_PHYCCSEL | UCPD_CR_CCENABLE);
  cr |= (0x01 << UCPD_CR_ANAMODE_Pos) | UCPD_CR_CCENABLE_0 | UCPD_CR_CCENABLE_1;
  UCPD1->CR = cr;

  UCPD1->ICR = UCPD_ICR_TYPECEVT1CF | UCPD_ICR_TYPECEVT2CF;
  UCPD1->IMR |= UCPD_IMR_TYPECEVT1IE | UCPD_IMR_TYPECEVT2IE;
}

void tcd_disconnect(uint8_t rhport) {
  if (!_cc_enabled[rhport]) {
    return;
  }

  // Mask UCPD interrupts/DMA early to avoid the ISR touching DMA/pointers while we tear down.
  UCPD1->IMR &= ~(UCPD_IMR_TYPECEVT1IE | UCPD_IMR_TYPECEVT2IE | IMR_ATTACHED);
  UCPD1->CFG1 &= ~(UCPD_CFG1_RXDMAEN | UCPD_CFG1_TXDMAEN);

  _cc_enabled[rhport] = false;

  if (dma_enabled(rhport, true)) {
    dma_stop(rhport, true);
  }

  if (dma_enabled(rhport, false)) {
    dma_tx_stop(rhport);
  }

  _rx_buf = NULL;
  _rx_buf_len = 0;
  _tx_pending_buf = NULL;
  _tx_pending_bytes = 0;
  _tx_xferring_bytes = 0;

  uint32_t cr = UCPD1->CR;
  cr &= ~(UCPD_CR_PHYRXEN | UCPD_CR_PHYCCSEL | UCPD_CR_CCENABLE);
  UCPD1->CR = cr;

  UCPD1->ICR = UCPD_ICR_TYPECEVT1CF | UCPD_ICR_TYPECEVT2CF;

  tcd_event_cc_changed(rhport, 0, 0, false);
}

bool tcd_msg_receive(uint8_t rhport, uint8_t* buffer, uint16_t total_bytes) {
  _rx_buf = buffer;
  _rx_buf_len = total_bytes;
  dma_start(rhport, true, buffer, total_bytes);
  return true;
}

bool tcd_msg_send(uint8_t rhport, uint8_t const* buffer, uint16_t total_bytes) {
  (void) rhport;

  if (dma_enabled(rhport, false)) {
    // DMA is busy, probably sending GoodCRC, save as pending TX
    _tx_pending_buf = buffer;
    _tx_pending_bytes = total_bytes;
  }else {
    // DMA is free, start sending
    _tx_pending_buf = NULL;
    _tx_pending_bytes = 0;

    _tx_xferring_bytes = total_bytes;
    dma_tx_start(rhport, buffer, total_bytes);
  }

  return true;
}

void tcd_int_handler(uint8_t rhport) {
  (void) rhport;

  uint32_t sr = UCPD1->SR;
  sr &= UCPD1->IMR;

  if (sr & (UCPD_SR_TYPECEVT1 | UCPD_SR_TYPECEVT2)) {
    uint32_t v_cc[2];
    v_cc[0] = (UCPD1->SR >> UCPD_SR_TYPEC_VSTATE_CC1_Pos) & 0x03;
    v_cc[1] = (UCPD1->SR >> UCPD_SR_TYPEC_VSTATE_CC2_Pos) & 0x03;

    TU_LOG3("VState CC1 = %lu, CC2 = %lu\r\n", v_cc[0], v_cc[1]);

    uint32_t cr = UCPD1->CR;

    // TODO only support SNK for now, required highest voltage for now
    // Determine attach/detach by checking both CC vstates.
    if (v_cc[0] >= 1) {
      TU_LOG3("Attach CC1\r\n");
      cr &= ~(UCPD_CR_PHYCCSEL | UCPD_CR_CCENABLE);
      cr |= UCPD_CR_PHYRXEN | UCPD_CR_CCENABLE_0;
    } else if (v_cc[1] >= 1) {
      TU_LOG3("Attach CC2\r\n");
      cr &= ~UCPD_CR_CCENABLE;
      cr |= (UCPD_CR_PHYCCSEL | UCPD_CR_PHYRXEN | UCPD_CR_CCENABLE_1);
    } else {
      TU_LOG3("Detach\r\n");
      cr &= ~UCPD_CR_PHYRXEN;
      cr |= UCPD_CR_CCENABLE_0 | UCPD_CR_CCENABLE_1;
    }

    if (cr & UCPD_CR_PHYRXEN) {
      // Attached
      UCPD1->IMR |= IMR_ATTACHED;
      UCPD1->CFG1 |= UCPD_CFG1_RXDMAEN | UCPD_CFG1_TXDMAEN;
    } else {
      // Detached
      UCPD1->CFG1 &= ~(UCPD_CFG1_RXDMAEN | UCPD_CFG1_TXDMAEN);
      UCPD1->IMR &= ~IMR_ATTACHED;
    }

    // notify stack
    tcd_event_cc_changed(rhport, v_cc[0], v_cc[1], true);

    UCPD1->CR = cr;

    // ack
    UCPD1->ICR = UCPD_ICR_TYPECEVT1CF | UCPD_ICR_TYPECEVT2CF;
  }

  //------------- RX -------------//
  if (sr & UCPD_SR_RXORDDET) {
    // SOP: Start of Packet.
    _rx_ordered_set = UCPD1->RX_ORDSET & UCPD_RX_ORDSET_RXORDSET;
    TU_LOG3("SOP %lu\r\n", _rx_ordered_set);

    // ack
    UCPD1->ICR = UCPD_ICR_RXORDDETCF;
  }

  // Received full message
  if (sr & UCPD_SR_RXMSGEND) {
    TU_LOG3("RX MSG END\r\n");

    // stop TX
    dma_stop(rhport, true);

    if (_rx_ordered_set == RX_ORDERED_SET_SOP) {
      uint8_t result;

      if (!(sr & UCPD_SR_RXERR)) {
        // Send GoodCRC in response, unless the received message is itself a GoodCRC.
        // TODO move this to usbc stack
        if (_rx_buf) {
          pd_header_t const* rx_header = (pd_header_t const*) _rx_buf;
          bool is_good_crc = (rx_header->n_data_obj == 0) && (rx_header->msg_type == PD_CTRL_GOOD_CRC);

          if (!is_good_crc) {
            _good_crc.msg_id = rx_header->msg_id;
            dma_tx_start(rhport, &_good_crc, 2);
          }
        }

        result = XFER_RESULT_SUCCESS;
      } else {
        // CRC failed
        result = XFER_RESULT_FAILED;
      }

      // notify stack
      tcd_event_rx_complete(rhport, UCPD1->RX_PAYSZ, result, true);
    } else {
      TU_LOG3("Ignore SOP* RX %lu\r\n", _rx_ordered_set);
    }

    // ack
    UCPD1->ICR = UCPD_ICR_RXMSGENDCF;

    if (_rx_ordered_set != RX_ORDERED_SET_SOP && _rx_buf != NULL && _rx_buf_len > 0) {
      tcd_msg_receive(rhport, _rx_buf, _rx_buf_len);
    }
  }

  if (sr & UCPD_SR_RXOVR) {
    TU_LOG3("RXOVR\r\n");
    // ack
    UCPD1->ICR = UCPD_ICR_RXOVRCF;
  }

  // Handle Hard Reset received from source. If not cleared here, the
  // flag will remain set and re-enter the ISR immediately
  if (sr & UCPD_SR_RXHRSTDET) {
    TU_LOG3("Hard Reset received\r\n");
    if (_rx_buf != NULL && _rx_buf_len > 0) {
      dma_stop(rhport, true);
      tcd_msg_receive(rhport, _rx_buf, _rx_buf_len);
    }
    UCPD1->ICR = UCPD_ICR_RXHRSTDETCF;
  }

  //------------- TX -------------//
  // All tx events: complete and error
  if (sr & (UCPD_SR_TXMSGSENT | (UCPD_SR_TXMSGDISC | UCPD_SR_TXMSGABT | UCPD_SR_TXUND))) {
    // force TX stop
    dma_tx_stop(rhport);

    uint16_t const xferred_bytes = _tx_xferring_bytes - UCPD1->TX_PAYSZ;
    uint8_t result;

    if ( sr & UCPD_SR_TXMSGSENT ) {
      TU_LOG3("TX MSG SENT\r\n");
      result = XFER_RESULT_SUCCESS;
      // ack
      UCPD1->ICR = UCPD_ICR_TXMSGSENTCF;
    }else {
      TU_LOG3("TX Error\r\n");
      result = XFER_RESULT_FAILED;
      // ack
      UCPD1->ICR = UCPD_SR_TXMSGDISC | UCPD_SR_TXMSGABT | UCPD_SR_TXUND;
    }

    // start pending TX if any
    if (_tx_pending_buf && _tx_pending_bytes ) {
      // Start the pending TX
      dma_tx_start(rhport, _tx_pending_buf, _tx_pending_bytes);

      // clear pending
      _tx_pending_buf = NULL;
      _tx_pending_bytes = 0;
    }

    // notify stack
    tcd_event_tx_complete(rhport, xferred_bytes, result, true);
  }
}

#endif
