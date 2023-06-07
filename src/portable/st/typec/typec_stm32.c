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
#else
  #error "Unsupported STM32 family"
#endif

//--------------------------------------------------------------------+
//
//--------------------------------------------------------------------+

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


static uint8_t rx_buf[262] TU_ATTR_ALIGNED(4);
static uint32_t rx_count = 0;

static uint8_t tx_buf[262] TU_ATTR_ALIGNED(4);
static uint32_t tx_count;

//--------------------------------------------------------------------+
//
//--------------------------------------------------------------------+

bool tcd_init(uint8_t rhport, tusb_typec_port_type_t port_type) {
  (void) rhport;

  // Initialization phase: CFG1
  UCPD1->CFG1 = (0x0d << UCPD_CFG1_HBITCLKDIV_Pos) | (0x10 << UCPD_CFG1_IFRGAP_Pos) | (0x07 << UCPD_CFG1_TRANSWIN_Pos) |
                (0x01 << UCPD_CFG1_PSC_UCPDCLK_Pos) | (0x1f << UCPD_CFG1_RXORDSETEN_Pos) |
                (0 << UCPD_CFG1_TXDMAEN_Pos) | (0 << UCPD_CFG1_RXDMAEN_Pos);
  UCPD1->CFG1 |= UCPD_CFG1_UCPDEN;

  // General programming sequence (with UCPD configured then enabled)
  if (port_type == TUSB_TYPEC_PORT_SNK) {
    // Enable both CC Phy
    UCPD1->CR = (0x01 << UCPD_CR_ANAMODE_Pos) | (0x03 << UCPD_CR_CCENABLE_Pos);

    // Read Voltage State on CC1 & CC2 fore initial state
    uint32_t vstate_cc[2];
    vstate_cc[0] = (UCPD1->SR >> UCPD_SR_TYPEC_VSTATE_CC1_Pos) & 0x03;
    vstate_cc[1] = (UCPD1->SR >> UCPD_SR_TYPEC_VSTATE_CC2_Pos) & 0x03;

    TU_LOG1_INT(vstate_cc[0]);
    TU_LOG1_INT(vstate_cc[1]);

    // Enable CC1 & CC2 Interrupt
    UCPD1->IMR = UCPD_IMR_TYPECEVT1IE | UCPD_IMR_TYPECEVT2IE;
  }

  return true;
}

// Enable interrupt
void tcd_int_enable (uint8_t rhport) {
  (void) rhport;
  NVIC_EnableIRQ(UCPD1_IRQn);
}

// Disable interrupt
void tcd_int_disable(uint8_t rhport) {
  (void) rhport;
  NVIC_DisableIRQ(UCPD1_IRQn);
}

bool tcd_rx_start(uint8_t rhport, uint8_t* buffer, uint16_t total_bytes) {
  (void) rhport;

  return true;
}

bool tcd_tx_start(uint8_t rhport, uint8_t const* buffer, uint16_t total_bytes) {
  (void) rhport;
  (void) buffer;
  (void) total_bytes;
  return false;
}

void tcd_int_handler(uint8_t rhport) {
  (void) rhport;

  uint32_t sr = UCPD1->SR;
  sr &= UCPD1->IMR;

//  TU_LOG1("UCPD1_IRQHandler: sr = 0x%08X\n", sr);

  if (sr & (UCPD_SR_TYPECEVT1 | UCPD_SR_TYPECEVT2)) {
    uint32_t vstate_cc[2];
    vstate_cc[0] = (UCPD1->SR >> UCPD_SR_TYPEC_VSTATE_CC1_Pos) & 0x03;
    vstate_cc[1] = (UCPD1->SR >> UCPD_SR_TYPEC_VSTATE_CC2_Pos) & 0x03;

    TU_LOG1("VState CC1 = %u, CC2 = %u\n", vstate_cc[0], vstate_cc[1]);

    uint32_t cr = UCPD1->CR;

    // TODO only support SNK for now, required highest voltage for now
    if ((sr & UCPD_SR_TYPECEVT1) && (vstate_cc[0] == 3)) {
      TU_LOG1("Attach CC1\n");
      cr &= ~UCPD_CR_PHYCCSEL;
      cr |= UCPD_CR_PHYRXEN;
    } else if ((sr & UCPD_SR_TYPECEVT2) && (vstate_cc[1] == 3)) {
      TU_LOG1("Attach CC2\n");
      cr |= UCPD_CR_PHYCCSEL;
      cr |= UCPD_CR_PHYRXEN;
    } else {
      TU_LOG1("Detach\n");
      cr &= ~UCPD_CR_PHYRXEN;
    }

    if (cr & UCPD_CR_PHYRXEN) {
      // Enable Interrupt
      UCPD1->IMR |= UCPD_IMR_TXISIE | UCPD_IMR_TXMSGDISCIE | UCPD_IMR_TXMSGSENTIE | UCPD_IMR_TXMSGABTIE | UCPD_IMR_TXUNDIE |
                    UCPD_IMR_RXNEIE | UCPD_IMR_RXORDDETIE | UCPD_IMR_RXHRSTDETIE | UCPD_IMR_RXOVRIE |
                    UCPD_IMR_RXMSGENDIE | UCPD_IMR_HRSTDISCIE | UCPD_IMR_HRSTSENTIE;
    }

    // Enable PD RX
    UCPD1->CR = cr;

    // ack
    UCPD1->ICR = UCPD_ICR_TYPECEVT1CF | UCPD_ICR_TYPECEVT2CF;
  }

  //------------- RX -------------//
  if (sr & UCPD_SR_RXORDDET) {
    // SOP: Start of Packet.
    // UCPD1->RX_ORDSET & UCPD_RX_ORDSET_RXORDSET_Msk;

    // reset count when received SOP
    rx_count = 0;

    // ack
    UCPD1->ICR = UCPD_ICR_RXORDDETCF;
  }

  if (sr & UCPD_SR_RXNE) {
    // TODO DMA later
    do {
      rx_buf[rx_count++] = UCPD1->RXDR;
    } while (UCPD1->SR & UCPD_SR_RXNE);

    // no ack needed
  }

  // End of message
  if (sr & UCPD_SR_RXMSGEND) {

    // Skip if CRC failed
    if (!(sr & UCPD_SR_RXERR)) {
      uint32_t payload_size = UCPD1->RX_PAYSZ;
      // TU_LOG1("RXMSGEND: payload_size = %u, rx count = %u\n", payload_size, pd_rx_count);

      tusb_pd_header_t const* rx_header = (tusb_pd_header_t const*) rx_buf;
      (*(tusb_pd_header_t*) tx_buf) = (tusb_pd_header_t) {
          .msg_type = TUSB_PD_CTRL_GOOD_CRC,
          .data_role = 0, // UFP
          .specs_rev = TUSB_PD_REV30,
          .power_role = 0, // Sink
          .msg_id = rx_header->msg_id,
          .n_data_obj = 0,
          .extended = 0
      };
      tx_count = 0;

      // response with good crc
      UCPD1->TX_ORDSET = PHY_ORDERED_SET_SOP;
      UCPD1->TX_PAYSZ = 2;
      UCPD1->CR |= UCPD_CR_TXSEND; // will trigger TXIS interrupt

      // notify stack after good crc ?
    }

    // ack
    UCPD1->ICR = UCPD_ICR_RXMSGENDCF;
  }

  if (sr & UCPD_SR_RXOVR) {
    TU_LOG1("RXOVR\n");
    TU_LOG1_HEX(rx_count);
    // ack
    UCPD1->ICR = UCPD_ICR_RXOVRCF;
  }

  //------------- TX -------------//
  if (sr & UCPD_SR_TXIS) {
    // TU_LOG1("TXIS\n");

    // TODO DMA later
    do {
      UCPD1->TXDR = tx_buf[tx_count++];
    } while (UCPD1->SR & UCPD_SR_TXIS);

    // no ack needed
  }

  if (sr & UCPD_SR_TXMSGSENT) {
    // all byte sent
    TU_LOG1("TXMSGSENT\n");

    // ack
    UCPD1->ICR = UCPD_ICR_TXMSGSENTCF;
  }

//  if (sr & UCPD_SR_RXNE) {
//    uint8_t data = UCPD1->RXDR;
//    pd_rx_buf[pd_rx_count++] = data;
//    TU_LOG1_HEX(data);
//  }

//  else {
//    TU_LOG_LOCATION();
//  }
}

#endif
