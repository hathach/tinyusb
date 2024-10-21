/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2024 Ha Thach (tinyusb.org)
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

#if CFG_TUH_ENABLED && defined(TUP_USBIP_DWC2)

// Debug level for DWC2
#define DWC2_DEBUG    2

#include "host/hcd.h"
#include "dwc2_common.h"

enum {
  HPRT_W1C_MASK = HPRT_CONN_DETECT | HPRT_ENABLE | HPRT_EN_CHANGE | HPRT_OVER_CURRENT_CHANGE
};

//--------------------------------------------------------------------+
// Controller API
//--------------------------------------------------------------------+

// optional hcd configuration, called by tuh_configure()
bool hcd_configure(uint8_t rhport, uint32_t cfg_id, const void* cfg_param) {
  (void) rhport;
  (void) cfg_id;
  (void) cfg_param;

  return false;
}

// Initialize controller to host mode
bool hcd_init(uint8_t rhport, const tusb_rhport_init_t* rh_init) {
  dwc2_regs_t* dwc2 = DWC2_REG(rhport);

  // Core Initialization
  const bool is_highspeed = dwc2_core_is_highspeed(dwc2, rh_init);
  const bool is_dma = dwc2_dma_enabled(dwc2, TUSB_ROLE_HOST);
  TU_ASSERT(dwc2_core_init(rhport, is_highspeed, is_dma));

  //------------- 3.1 Host Initialization -------------//

  // FS/LS PHY Clock Select
  uint32_t hcfg = dwc2->hcfg;
  if (is_highspeed) {
    hcfg &= ~HCFG_FSLS_ONLY;
  } else {
    hcfg &= ~HCFG_FSLS_ONLY; // since we are using FS PHY
    hcfg &= ~HCFG_FSLS_PHYCLK_SEL;

    if (dwc2->ghwcfg2_bm.hs_phy_type == GHWCFG2_HSPHY_ULPI &&
        dwc2->ghwcfg2_bm.fs_phy_type == GHWCFG2_FSPHY_DEDICATED) {
      // dedicated FS PHY with 48 mhz
      hcfg |= HCFG_FSLS_PHYCLK_SEL_48MHZ;
    } else {
      // shared HS PHY running at full speed
      hcfg |= HCFG_FSLS_PHYCLK_SEL_30_60MHZ;
    }
  }
  dwc2->hcfg = hcfg;

  // force host mode and wait for mode switch
  dwc2->gusbcfg = (dwc2->gusbcfg & ~GUSBCFG_FDMOD) | GUSBCFG_FHMOD;
  while( (dwc2->gintsts & GINTSTS_CMOD) != GINTSTS_CMODE_HOST) {}

  dwc2->hprt = HPRT_W1C_MASK; // clear all write-1-clear bits
  dwc2->hprt = HPRT_POWER; // turn on VBUS

  // Enable required interrupts
  dwc2->gintmsk |= GINTMSK_OTGINT | GINTSTS_CONIDSTSCHNG | GINTMSK_PRTIM; // | GINTMSK_WUIM;
  dwc2->gahbcfg |= GAHBCFG_GINT;   // Enable global interrupt

  return true;
}

// Enable USB interrupt
void hcd_int_enable (uint8_t rhport) {
  dwc2_int_set(rhport, TUSB_ROLE_HOST, true);
}

// Disable USB interrupt
void hcd_int_disable(uint8_t rhport) {
  dwc2_int_set(rhport, TUSB_ROLE_HOST, false);
}

// Get frame number (1ms)
uint32_t hcd_frame_number(uint8_t rhport) {
  (void) rhport;
  return 0;
}

//--------------------------------------------------------------------+
// Port API
//--------------------------------------------------------------------+

// Get the current connect status of roothub port
bool hcd_port_connect_status(uint8_t rhport) {
  dwc2_regs_t* dwc2 = DWC2_REG(rhport);
  return dwc2->hprt & HPRT_CONN_STATUS;
}

// Reset USB bus on the port. Return immediately, bus reset sequence may not be complete.
// Some port would require hcd_port_reset_end() to be invoked after 10ms to complete the reset sequence.
void hcd_port_reset(uint8_t rhport) {
  dwc2_regs_t* dwc2 = DWC2_REG(rhport);
  dwc2->hprt = HPRT_RESET;
}

// Complete bus reset sequence, may be required by some controllers
void hcd_port_reset_end(uint8_t rhport) {
  dwc2_regs_t* dwc2 = DWC2_REG(rhport);
  uint32_t hprt = dwc2->hprt & ~HPRT_W1C_MASK; // skip w1c bits
  hprt &= ~HPRT_RESET;
  dwc2->hprt = hprt;
}

// Get port link speed
tusb_speed_t hcd_port_speed_get(uint8_t rhport) {
  (void) rhport;

  return TUSB_SPEED_FULL;
}

// HCD closes all opened endpoints belong to this device
void hcd_device_close(uint8_t rhport, uint8_t dev_addr) {
  (void) rhport;
  (void) dev_addr;
}

//--------------------------------------------------------------------+
// Endpoints API
//--------------------------------------------------------------------+

// Open an endpoint
bool hcd_edpt_open(uint8_t rhport, uint8_t dev_addr, tusb_desc_endpoint_t const * ep_desc) {
  (void) rhport;
  (void) dev_addr;
  (void) ep_desc;

  return false;
}

// Submit a transfer, when complete hcd_event_xfer_complete() must be invoked
bool hcd_edpt_xfer(uint8_t rhport, uint8_t dev_addr, uint8_t ep_addr, uint8_t * buffer, uint16_t buflen) {
  (void) rhport;
  (void) dev_addr;
  (void) ep_addr;
  (void) buffer;
  (void) buflen;

  return false;
}

// Abort a queued transfer. Note: it can only abort transfer that has not been started
// Return true if a queued transfer is aborted, false if there is no transfer to abort
bool hcd_edpt_abort_xfer(uint8_t rhport, uint8_t dev_addr, uint8_t ep_addr) {
  (void) rhport;
  (void) dev_addr;
  (void) ep_addr;

  return false;
}

// Submit a special transfer to send 8-byte Setup Packet, when complete hcd_event_xfer_complete() must be invoked
bool hcd_setup_send(uint8_t rhport, uint8_t dev_addr, uint8_t const setup_packet[8]) {
  (void) rhport;
  (void) dev_addr;
  (void) setup_packet;

  return false;
}

// clear stall, data toggle is also reset to DATA0
bool hcd_edpt_clear_stall(uint8_t rhport, uint8_t dev_addr, uint8_t ep_addr) {
  (void) rhport;
  (void) dev_addr;
  (void) ep_addr;

  return false;
}

//--------------------------------------------------------------------
// HCD Event Handler
//--------------------------------------------------------------------

#if 1
static void handle_rxflvl_irq(uint8_t rhport) {
  dwc2_regs_t* dwc2 = DWC2_REG(rhport);
  // volatile uint32_t const* rx_fifo = dwc2->fifo[0];

  // Pop control word off FIFO
  uint32_t const grxstsp = dwc2->grxstsp;
  (void) grxstsp;
  // uint8_t const pktsts = (grxstsp & GRXSTSP_PKTSTS_Msk) >> GRXSTSP_PKTSTS_Pos;
  // uint8_t const epnum = (grxstsp & GRXSTSP_EPNUM_Msk) >> GRXSTSP_EPNUM_Pos;
  // uint16_t const bcnt = (grxstsp & GRXSTSP_BCNT_Msk) >> GRXSTSP_BCNT_Pos;
  // dwc2_epout_t* epout = &dwc2->epout[epnum];
}
#endif

/* Handle Host Port interrupt, possible source are:
   - Connection Detection
   - Enable Change
   - Over Current Change
*/
TU_ATTR_ALWAYS_INLINE static inline void handle_hprt_irq(uint8_t rhport, bool in_isr) {
  dwc2_regs_t* dwc2 = DWC2_REG(rhport);
  uint32_t hprt = dwc2->hprt;

  if (hprt & HPRT_CONN_DETECT) {
    // Port Connect Detect
    dwc2->hprt = HPRT_CONN_DETECT; // clear

    if (hprt & HPRT_CONN_STATUS) {
      hcd_event_device_attach(rhport, in_isr);
    } else {
      hcd_event_device_remove(rhport, in_isr);
    }
  }
}

/* Interrupt Hierarchy

   HCINTn.XferCompl     HCINTMSKn.XferComplMsk
         |                       |
         +---------- AND --------+
                      |
     HAINT.CHn            HAINTMSK.CHn
         |                       |
         +---------- AND --------+
                      |
    GINTSTS.PrtInt         GINTMSK.PrtInt
         |                       |
         +---------- AND --------+
                      |
             GAHBCFG.GblIntrMsk
                      |
                    IRQn
 */
void hcd_int_handler(uint8_t rhport, bool in_isr) {
  dwc2_regs_t* dwc2 = DWC2_REG(rhport);
  const uint32_t int_mask = dwc2->gintmsk;
  const uint32_t int_status = dwc2->gintsts & int_mask;

  TU_LOG1_HEX(int_status);

  if (int_status & GINTSTS_CONIDSTSCHNG) {
    // Connector ID status change
    dwc2->gintsts = GINTSTS_CONIDSTSCHNG;

    //if (dwc2->gotgctl)
    // dwc2->hprt = HPRT_POWER; // power on port to turn on VBUS
    //dwc2->gintmsk |= GINTMSK_PRTIM;
    // TODO wait for SRP if OTG
  }

  if (int_status & GINTSTS_HPRTINT) {
    TU_LOG1_HEX(dwc2->hprt);
    handle_hprt_irq(rhport, in_isr);
  }

  // RxFIFO non-empty interrupt handling.
  if (int_status & GINTSTS_RXFLVL) {
    // RXFLVL bit is read-only
    dwc2->gintmsk &= ~GINTMSK_RXFLVLM; // disable RXFLVL interrupt while reading

    do {
      handle_rxflvl_irq(rhport); // read all packets
    } while(dwc2->gintsts & GINTSTS_RXFLVL);

    dwc2->gintmsk |= GINTMSK_RXFLVLM;
  }

}

#endif
