/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2021, Ha Thach (tinyusb.org)
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


#ifndef DWC2_AT32_H_
#define DWC2_AT32_H_

#define DWC2_EP_MAX TUP_DCD_ENDPOINT_MAX

#if CFG_TUSB_MCU == OPT_MCU_AT32F415
  #include <at32f415.h>
  #define OTG1_FIFO_SIZE           1280
  #define OTG1_IRQn                OTGFS1_IRQn
  #define DWC2_OTG1_REG_BASE       0x50000000UL
#elif CFG_TUSB_MCU == OPT_MCU_AT32F435_437
  #include <at32f435_437.h>
  #define OTG1_FIFO_SIZE           1280
  #define OTG2_FIFO_SIZE           1280
  #define OTG1_IRQn                OTGFS1_IRQn
  #define OTG2_IRQn                OTGFS2_IRQn
  #define DWC2_OTG1_REG_BASE       0x50000000UL
  #define DWC2_OTG2_REG_BASE       0x40040000UL
#elif CFG_TUSB_MCU == OPT_MCU_AT32F423
  #include <at32f423.h>
  #define OTG1_FIFO_SIZE           1280
  #define OTG1_IRQn                OTGFS1_IRQn
  #define DWC2_OTG1_REG_BASE       0x50000000UL
#elif CFG_TUSB_MCU == OPT_MCU_AT32F402_405
  #include <at32f402_405.h>
  #define OTG1_FIFO_SIZE           1280
  #define OTG2_FIFO_SIZE           4096
  #define OTG1_IRQn                OTGFS1_IRQn
  #define OTG2_IRQn                OTGHS_IRQn
  #define DWC2_OTG1_REG_BASE       0x50000000UL
  #define DWC2_OTG2_REG_BASE       0x40040000UL //OTGHS
#elif CFG_TUSB_MCU == OPT_MCU_AT32F425
  #include <at32f425.h>
  #define OTG1_FIFO_SIZE           1280
  #define OTG1_IRQn                OTGFS1_IRQn
  #define DWC2_OTG1_REG_BASE       0x50000000UL
#endif

#ifdef __cplusplus
 extern "C" {
#endif

 static const dwc2_controller_t _dwc2_controller[] = {
{.reg_base = DWC2_OTG1_REG_BASE, .irqnum = OTG1_IRQn, .ep_count = DWC2_EP_MAX, .ep_fifo_size = OTG1_FIFO_SIZE},
#if defined DWC2_OTG2_REG_BASE
  {.reg_base = DWC2_OTG2_REG_BASE, .irqnum = OTG2_IRQn, .ep_count = DWC2_EP_MAX, .ep_fifo_size = OTG2_FIFO_SIZE}
#endif
 };

 TU_ATTR_ALWAYS_INLINE static inline void dwc2_int_set(uint8_t rhport, tusb_role_t role, bool enabled) {
   (void) role;
   const IRQn_Type irqn = (IRQn_Type) _dwc2_controller[rhport].irqnum;
   if (enabled) {
     NVIC_EnableIRQ(irqn);
   } else {
     NVIC_DisableIRQ(irqn);
   }
 }

 TU_ATTR_ALWAYS_INLINE static inline void dwc2_dcd_int_enable(uint8_t rhport) { NVIC_EnableIRQ(_dwc2_controller[rhport].irqnum);
 }

 TU_ATTR_ALWAYS_INLINE static inline void dwc2_dcd_int_disable(uint8_t rhport) {
   NVIC_DisableIRQ(_dwc2_controller[rhport].irqnum);
 }

 TU_ATTR_ALWAYS_INLINE static inline void dwc2_remote_wakeup_delay(void) {
   // try to delay for 1 ms
   uint32_t count = system_core_clock / 1000;
   while (count--) __asm volatile("nop");
 }

 // MCU specific PHY init, called BEFORE core reset
 TU_ATTR_ALWAYS_INLINE static inline void dwc2_phy_init(dwc2_regs_t *dwc2, uint8_t hs_phy_type) {
   (void) dwc2;
   // Enable on-chip HS PHY
   if (hs_phy_type == GHWCFG2_HSPHY_UTMI || hs_phy_type == GHWCFG2_HSPHY_UTMI_ULPI) {
   } else if (hs_phy_type == GHWCFG2_HSPHY_NOT_SUPPORTED) {
   }
 }

 // MCU specific PHY update, it is called AFTER init() and core reset
 TU_ATTR_ALWAYS_INLINE static inline void dwc2_phy_update(dwc2_regs_t *dwc2, uint8_t hs_phy_type) {
   (void) dwc2;
   (void) hs_phy_type;

   dwc2->stm32_gccfg |= STM32_GCCFG_PWRDWN | STM32_GCCFG_DCDEN | STM32_GCCFG_PDEN;
 }

#ifdef __cplusplus
}
#endif

#endif /* DWC2_GD32_H_ */
