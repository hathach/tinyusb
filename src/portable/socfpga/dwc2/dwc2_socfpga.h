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

#ifndef DWC2_SOCFPGA_H_
#define DWC2_SOCFPGA_H_

#ifdef __cplusplus
extern "C" {
#endif

// EP_MAX       : Max number of bi-directional endpoints including EP0
// EP_FIFO_SIZE : Size of dedicated USB SRAM
#if CFG_TUSB_MCU == OPT_MCU_SOCFPGA
  #include "socfpga_usb_otg_reg.h"
  #include "socfpga_interrupt.h"
  #include "socfpga_cache.h"
  #include "socfpga_gpio.h"
  #include "osal_log.h"
  //toggle gpio for usb2 reset
  #define USB_GPIO_TOGGLE 0
#else
  #error "Unsupported MCUs"
#endif

// OTG HS always has higher number of endpoints than FS
#ifdef USB_OTG_HS_PERIPH_BASE
  #define DWC2_EP_MAX   EP_MAX_HS
#else
  #define DWC2_EP_MAX   EP_MAX_FS
#endif

// On Agilex5 socfpga, we associate:
// - Port0 to USB_OTG
static const dwc2_controller_t _dwc2_controller[] = {
    { .reg_base = USBOTG_BASE_ADDR, .irqnum = USB0IRQ, .ep_count = 15, .ep_fifo_size = 8192 },
};

//--------------------------------------------------------------------+
//
//--------------------------------------------------------------------+

static void dwc2_int_handler_wrap() {
  hcd_int_handler(0, true);
}

TU_ATTR_ALWAYS_INLINE static inline void dwc2_int_set(uint8_t rhport, tusb_role_t role, bool enabled) {
  (void) role;
  (void) rhport;
  socfpga_interrupt_err_t intr_ret;

  if (!enabled) {
    (void) interrupt_disable(USB0IRQ);
    return;
  }

  intr_ret = interrupt_register_isr(USB0IRQ, dwc2_int_handler_wrap, NULL);
  if (intr_ret != ERR_OK)
  {
      ERROR("Failed to enable interrupt");
      return;
  }

  intr_ret = interrupt_enable(USB0IRQ, GIC_INTERRUPT_PRIORITY_USB2);
  if (intr_ret != ERR_OK)
  {
      ERROR("Failed to enable interrupt");
      return;
  }

  INFO("Interrupt handler registered successfully");

}

#define dwc2_dcd_int_enable(_rhport)  dwc2_int_set(_rhport, TUSB_ROLE_DEVICE, true)
#define dwc2_dcd_int_disable(_rhport) dwc2_int_set(_rhport, TUSB_ROLE_DEVICE, false)


TU_ATTR_ALWAYS_INLINE static inline void dwc2_remote_wakeup_delay(void) {
  // try to delay for 1 ms
  osal_task_delay(1);
}

// MCU specific PHY init, called BEFORE core reset
// - dwc2 3.30a (H5) use USB_HS_PHYC
// - dwc2 4.11a (U5) use femtoPHY
static inline void dwc2_phy_init(dwc2_regs_t* dwc2, uint8_t hs_phy_type) {
  (void) dwc2;
  (void) hs_phy_type;
}

// MCU specific PHY update, it is called AFTER init() and core reset
static inline void dwc2_phy_update(dwc2_regs_t* dwc2, uint8_t hs_phy_type) {
  // used to set turnaround time for fullspeed, nothing to do in highspeed mode
  (void)dwc2;
  (void) hs_phy_type;
}

//------------- DCache -------------//
#if CFG_TUD_MEM_DCACHE_ENABLE || CFG_TUH_MEM_DCACHE_ENABLE

TU_ATTR_ALWAYS_INLINE static inline uint32_t round_up_to_cache_line_size(uint32_t size) {
  if (size & (CFG_TUSB_MEM_DCACHE_LINE_SIZE_DEFAULT-1)) {
    size = (size & ~(CFG_TUSB_MEM_DCACHE_LINE_SIZE_DEFAULT-1)) + CFG_TUSB_MEM_DCACHE_LINE_SIZE_DEFAULT;
  }
  return size;
}

TU_ATTR_ALWAYS_INLINE static inline bool dwc2_dcache_clean(void const* addr, uint32_t data_size) {
  const uintptr_t addr32 = (uintptr_t) addr;
    data_size = round_up_to_cache_line_size(data_size);
    cache_force_write_back((uint32_t *) addr32, (int32_t) data_size);
  return true;
}

TU_ATTR_ALWAYS_INLINE static inline bool dwc2_dcache_invalidate(void const* addr, uint32_t data_size) {
  const uintptr_t addr32 = (uintptr_t) addr;
    data_size = round_up_to_cache_line_size(data_size);
    cache_force_invalidate((uint32_t *) addr32, (int32_t) data_size);
  return true;
}

TU_ATTR_ALWAYS_INLINE static inline bool dwc2_dcache_clean_invalidate(void const* addr, uint32_t data_size) {
  const uintptr_t addr32 = (uintptr_t) addr;
    data_size = round_up_to_cache_line_size(data_size);
    cache_force_invalidate((uint32_t *) addr32, (int32_t) data_size);
    cache_force_write_back((uint32_t *) addr32, (int32_t) data_size);
  return true;
}
#endif

#ifdef __cplusplus
}
#endif

#endif
