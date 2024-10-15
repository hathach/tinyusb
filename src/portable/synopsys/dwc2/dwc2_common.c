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

#define DWC2_COMMON_DEBUG   2

#if defined(TUP_USBIP_DWC2) && (CFG_TUH_ENABLED || CFG_TUD_ENABLED)

#include "common/tusb_common.h"
#include "dwc2_common.h"

static void reset_core(dwc2_regs_t* dwc2) {
  // reset core
  dwc2->grstctl |= GRSTCTL_CSRST;

  // wait for reset bit is cleared
  // TODO version 4.20a should wait for RESET DONE mask
  while (dwc2->grstctl & GRSTCTL_CSRST) {}

  // wait for AHB master IDLE
  while (!(dwc2->grstctl & GRSTCTL_AHBIDL)) {}

  // wait for device mode ?
}

static bool phy_hs_supported(dwc2_regs_t* dwc2, const tusb_rhport_init_t* rh_init) {
  (void) dwc2;


#if !TUD_OPT_HIGH_SPEED
  return false;
#else
  return dwc2->ghwcfg2_bm.hs_phy_type != GHWCFG2_HSPHY_NOT_SUPPORTED;
#endif
}

static void phy_fs_init(dwc2_regs_t* dwc2, const tusb_rhport_init_t* rh_init) {
  TU_LOG(DWC2_COMMON_DEBUG, "Fullspeed PHY init\r\n");

  // Select FS PHY
  dwc2->gusbcfg |= GUSBCFG_PHYSEL;

  // MCU specific PHY init before reset
  dwc2_phy_init(dwc2, GHWCFG2_HSPHY_NOT_SUPPORTED);

  // Reset core after selecting PHY
  reset_core(dwc2);

  // USB turnaround time is critical for certification where long cables and 5-Hubs are used.
  // So if you need the AHB to run at less than 30 MHz, and if USB turnaround time is not critical,
  // these bits can be programmed to a larger value. Default is 5
  dwc2->gusbcfg = (dwc2->gusbcfg & ~GUSBCFG_TRDT_Msk) | (5u << GUSBCFG_TRDT_Pos);

  // MCU specific PHY update post reset
  dwc2_phy_update(dwc2, GHWCFG2_HSPHY_NOT_SUPPORTED);

  // set max speed
  dwc2->dcfg = (dwc2->dcfg & ~DCFG_DSPD_Msk) | (DCFG_DSPD_FS << DCFG_DSPD_Pos);
}

static void phy_hs_init(dwc2_regs_t* dwc2, const tusb_rhport_init_t* rh_init) {
  uint32_t gusbcfg = dwc2->gusbcfg;

  // De-select FS PHY
  gusbcfg &= ~GUSBCFG_PHYSEL;

  if (dwc2->ghwcfg2_bm.hs_phy_type == GHWCFG2_HSPHY_ULPI) {
    TU_LOG(DWC2_COMMON_DEBUG, "Highspeed ULPI PHY init\r\n");

    // Select ULPI PHY (external)
    gusbcfg |= GUSBCFG_ULPI_UTMI_SEL;

    // ULPI is always 8-bit interface
    gusbcfg &= ~GUSBCFG_PHYIF16;

    // ULPI select single data rate
    gusbcfg &= ~GUSBCFG_DDRSEL;

    // default internal VBUS Indicator and Drive
    gusbcfg &= ~(GUSBCFG_ULPIEVBUSD | GUSBCFG_ULPIEVBUSI);

    // Disable FS/LS ULPI
    gusbcfg &= ~(GUSBCFG_ULPIFSLS | GUSBCFG_ULPICSM);
  } else {
    TU_LOG(DWC2_COMMON_DEBUG, "Highspeed UTMI+ PHY init\r\n");

    // Select UTMI+ PHY (internal)
    gusbcfg &= ~GUSBCFG_ULPI_UTMI_SEL;

    // Set 16-bit interface if supported
    if (dwc2->ghwcfg4_bm.phy_data_width) {
      gusbcfg |= GUSBCFG_PHYIF16;  // 16 bit
    }else {
      gusbcfg &= ~GUSBCFG_PHYIF16; // 8 bit
    }
  }

  // Apply config
  dwc2->gusbcfg = gusbcfg;

  // mcu specific phy init
  dwc2_phy_init(dwc2, dwc2->ghwcfg2_bm.hs_phy_type);

  // Reset core after selecting PHY
  reset_core(dwc2);

  // Set turn-around, must after core reset otherwise it will be clear
  // - 9 if using 8-bit PHY interface
  // - 5 if using 16-bit PHY interface
  gusbcfg &= ~GUSBCFG_TRDT_Msk;
  gusbcfg |= (dwc2->ghwcfg4_bm.phy_data_width ? 5u : 9u) << GUSBCFG_TRDT_Pos;
  dwc2->gusbcfg = gusbcfg;

  // MCU specific PHY update post reset
  dwc2_phy_update(dwc2, dwc2->ghwcfg2_bm.hs_phy_type);

  // Set max speed
  uint32_t dcfg = dwc2->dcfg;
  dcfg &= ~DCFG_DSPD_Msk;
  dcfg |= DCFG_DSPD_HS << DCFG_DSPD_Pos;

  // XCVRDLY: transceiver delay between xcvr_sel and txvalid during device chirp is required
  // when using with some PHYs such as USB334x (USB3341, USB3343, USB3346, USB3347)
  if (dwc2->ghwcfg2_bm.hs_phy_type == GHWCFG2_HSPHY_ULPI) {
    dcfg |= DCFG_XCVRDLY;
  }

  dwc2->dcfg = dcfg;
}

static bool check_dwc2(dwc2_regs_t* dwc2) {
#if CFG_TUSB_DEBUG >= DWC2_COMMON_DEBUG
  // print guid, gsnpsid, ghwcfg1, ghwcfg2, ghwcfg3, ghwcfg4
  // Run 'python dwc2_info.py' and check dwc2_info.md for bit-field value and comparison with other ports
  volatile uint32_t const* p = (volatile uint32_t const*) &dwc2->guid;
  TU_LOG1("guid, gsnpsid, ghwcfg1, ghwcfg2, ghwcfg3, ghwcfg4\r\n");
  for (size_t i = 0; i < 5; i++) {
    TU_LOG1("0x%08" PRIX32 ", ", p[i]);
  }
  TU_LOG1("0x%08" PRIX32 "\r\n", p[5]);
#endif

  // For some reason: GD32VF103 gsnpsid and all hwcfg register are always zero (skip it)
  (void) dwc2;
#if !TU_CHECK_MCU(OPT_MCU_GD32VF103)
  uint32_t const gsnpsid = dwc2->gsnpsid & GSNPSID_ID_MASK;
  TU_ASSERT(gsnpsid == DWC2_OTG_ID || gsnpsid == DWC2_FS_IOT_ID || gsnpsid == DWC2_HS_IOT_ID);
#endif

  return true;
}

//--------------------------------------------------------------------
//
//--------------------------------------------------------------------
bool dwc2_controller_init(uint8_t rhport, const tusb_rhport_init_t* rh_init) {
  (void) rh_init;
  dwc2_regs_t* dwc2 = DWC2_REG(rhport);

  // Check Synopsys ID register, failed if controller clock/power is not enabled
  TU_ASSERT(check_dwc2(dwc2));

  if (phy_hs_supported(dwc2, rh_init)) {
    phy_hs_init(dwc2, rh_init); // Highspeed
  } else {
    phy_fs_init(dwc2, rh_init); // core does not support highspeed or hs phy is not present
  }

  return true;
}

#endif
