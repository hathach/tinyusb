/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2024, Brent Kowal (Analog Devices, Inc)
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

#ifndef _TUSB_MUSB_MAX32_H_
#define _TUSB_MUSB_MAX32_H_

#ifdef __cplusplus
 extern "C" {
#endif

#if TU_CHECK_MCU(OPT_MCU_MAX32690, OPT_MCU_MAX32650, OPT_MCU_MAX32666, OPT_MCU_MAX78002)
  #include "mxc_device.h"
  #include "usbhs_regs.h"
#else
  #error "Unsupported MCUs"
#endif


#if CFG_TUD_ENABLED
#define USBHS_M31_CLOCK_RECOVERY

// Mapping of peripheral instances to port. Currently just 1.
static mxc_usbhs_regs_t* const musb_periph_inst[] = {
    MXC_USBHS
};

// Mapping of IRQ numbers to port. Currently just 1.
static const IRQn_Type  musb_irqs[] = {
    USB_IRQn
};

TU_ATTR_ALWAYS_INLINE
static inline void musb_dcd_int_enable(uint8_t rhport)
{
  NVIC_EnableIRQ(musb_irqs[rhport]);
}

TU_ATTR_ALWAYS_INLINE
static inline void musb_dcd_int_disable(uint8_t rhport)
{
  NVIC_DisableIRQ(musb_irqs[rhport]);
}

TU_ATTR_ALWAYS_INLINE
static inline unsigned musb_dcd_get_int_enable(uint8_t rhport)
{
  return NVIC_GetEnableIRQ(musb_irqs[rhport]);
}

TU_ATTR_ALWAYS_INLINE
static inline void musb_dcd_int_clear(uint8_t rhport)
{
    NVIC_ClearPendingIRQ(musb_irqs[rhport]);
}

//Used to save and restore user's register map when interrupt occurs
static volatile unsigned isr_saved_index = 0;

static inline void musb_dcd_int_handler_enter(uint8_t rhport)
{
  uint32_t mxm_int, mxm_int_en, mxm_is;

  //save current register index
  isr_saved_index = musb_periph_inst[rhport]->index;

  //Handle PHY specific events
  mxm_int = musb_periph_inst[rhport]->mxm_int;
  mxm_int_en = musb_periph_inst[rhport]->mxm_int_en;
  mxm_is = mxm_int & mxm_int_en;
  musb_periph_inst[rhport]->mxm_int = mxm_is;

  if (mxm_is & MXC_F_USBHS_MXM_INT_NOVBUS) {
    dcd_event_bus_signal(rhport, DCD_EVENT_UNPLUGGED, true);
  }
}

static inline void musb_dcd_int_handler_exit(uint8_t rhport)
{
  //restore register index
  musb_periph_inst[rhport]->index = isr_saved_index;
}

static inline void musb_dcd_phy_init(uint8_t rhport)
{
  //Interrupt for VBUS disconnect
  musb_periph_inst[rhport]->mxm_int_en |= MXC_F_USBHS_MXM_INT_EN_NOVBUS;

  musb_dcd_int_clear(rhport);

  //Unsuspend the MAC
  musb_periph_inst[rhport]->mxm_suspend = 0;

  // Configure PHY
  musb_periph_inst[rhport]->m31_phy_xcfgi_31_0 = (0x1 << 3) | (0x1 << 11);
  musb_periph_inst[rhport]->m31_phy_xcfgi_63_32 = 0;
  musb_periph_inst[rhport]->m31_phy_xcfgi_95_64 = 0x1 << (72 - 64);
  musb_periph_inst[rhport]->m31_phy_xcfgi_127_96 = 0;


  #ifdef USBHS_M31_CLOCK_RECOVERY
  musb_periph_inst[rhport]->m31_phy_noncry_rstb = 1;
  musb_periph_inst[rhport]->m31_phy_noncry_en = 1;
  musb_periph_inst[rhport]->m31_phy_outclksel = 0;
  musb_periph_inst[rhport]->m31_phy_coreclkin = 0;
  musb_periph_inst[rhport]->m31_phy_xtlsel = 2; /* Select 25 MHz clock */
  #else
  musb_periph_inst[rhport]->m31_phy_noncry_rstb = 0;
  musb_periph_inst[rhport]->m31_phy_noncry_en = 0;
  musb_periph_inst[rhport]->m31_phy_outclksel = 1;
  musb_periph_inst[rhport]->m31_phy_coreclkin = 1;
  musb_periph_inst[rhport]->m31_phy_xtlsel = 3; /* Select 30 MHz clock */
  #endif
  musb_periph_inst[rhport]->m31_phy_pll_en = 1;
  musb_periph_inst[rhport]->m31_phy_oscouten = 1;

  /* Reset PHY */
  musb_periph_inst[rhport]->m31_phy_ponrst = 0;
  musb_periph_inst[rhport]->m31_phy_ponrst = 1;
}

static inline volatile musb_dcd_ctl_regs_t* musb_dcd_ctl_regs(uint8_t rhport)
{
  volatile musb_dcd_ctl_regs_t *regs = (volatile musb_dcd_ctl_regs_t*)((uintptr_t)&(musb_periph_inst[rhport]->faddr));
  return regs;
}

static inline volatile musb_dcd_epn_regs_t* musb_dcd_epn_regs(uint8_t rhport, unsigned epnum)
{
  //Need to set index to map EP registers
  musb_periph_inst[rhport]->index = epnum;
  volatile musb_dcd_epn_regs_t *regs = (volatile musb_dcd_epn_regs_t*)((uintptr_t)&(musb_periph_inst[rhport]->inmaxp));
  return regs;
}

static inline volatile musb_dcd_ep0_regs_t* musb_dcd_ep0_regs(uint8_t rhport)
{
  //Need to set index to map EP0 registers
  musb_periph_inst[rhport]->index = 0;
  volatile musb_dcd_ep0_regs_t *regs = (volatile musb_dcd_ep0_regs_t*)((uintptr_t)&(musb_periph_inst[rhport]->csr0));
  return regs;
}

static volatile void *musb_dcd_ep_get_fifo_ptr(uint8_t rhport, unsigned epnum)
{
  volatile uint32_t *ptr;

  ptr = &(musb_periph_inst[rhport]->fifo0);
  ptr += epnum;

  return (volatile void *) ptr;
}


static inline void musb_dcd_setup_fifo(uint8_t rhport, unsigned epnum, unsigned dir_in, unsigned mps)
{
  (void)mps;

  //Most likely the caller has already grabbed the right register block. But
  //as a precaution save and restore the register bank anyways
  unsigned saved_index = musb_periph_inst[rhport]->index;

  musb_periph_inst[rhport]->index = epnum;

  //Disable double buffering
  if(dir_in) {
    musb_periph_inst[rhport]->incsru |= (MXC_F_USBHS_INCSRU_DPKTBUFDIS | MXC_F_USBHS_INCSRU_MODE);
  } else {
    musb_periph_inst[rhport]->outcsru |= (MXC_F_USBHS_OUTCSRU_DPKTBUFDIS);
  }

  musb_periph_inst[rhport]->index = saved_index;
}

static inline void musb_dcd_reset_fifo(uint8_t rhport, unsigned epnum, unsigned dir_in)
{
  //Most likely the caller has already grabbed the right register block. But
  //as a precaution save and restore the register bank anyways
  unsigned saved_index = musb_periph_inst[rhport]->index;

  musb_periph_inst[rhport]->index = epnum;

  //Disable double buffering
  if(dir_in) {
    musb_periph_inst[rhport]->incsru |= (MXC_F_USBHS_INCSRU_DPKTBUFDIS);
  } else {
    musb_periph_inst[rhport]->outcsru |= (MXC_F_USBHS_OUTCSRU_DPKTBUFDIS);
  }

  musb_periph_inst[rhport]->index = saved_index;
}

#endif // CFG_TUD_ENABLED

#ifdef __cplusplus
 }
#endif

#endif // _TUSB_MUSB_MAX32_H_
