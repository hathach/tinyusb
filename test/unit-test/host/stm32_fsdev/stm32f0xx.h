// SPDX-License-Identifier: MIT
// Stand-in for the vendor header: the fixture maps the register block and PMA to RAM.
#ifndef STM32F0XX_H_
#define STM32F0XX_H_
#include <stdint.h>

extern uint32_t test_usb_regs[0x60 / 4];
extern uint32_t test_pma[1024 / 4];

#define USB_BASE    ((uintptr_t) test_usb_regs)
#define USB_PMAADDR ((uintptr_t) test_pma)
#define USB_BCDR_DPPU 0x8000u

typedef enum {
  USB_IRQn = 31
} IRQn_Type;

static inline void NVIC_EnableIRQ(IRQn_Type irq) { (void) irq; }
static inline void NVIC_DisableIRQ(IRQn_Type irq) { (void) irq; }
static inline void __DSB(void) {}
static inline void __ISB(void) {}
#endif
