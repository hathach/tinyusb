// Originally designed by Hong Xuyao

#include <stdint.h>
#include <stdio.h>
#include <f1c100s-irq.h>
#include <arm32.h>

#define __irq  __attribute__ ((interrupt ("IRQ")))

#ifndef __IO
#define __IO volatile
#endif

typedef struct {
  __IO uint32_t INTC_VECTOR_REG;    // 0x00
  __IO uint32_t INTC_BASE_ADDR_REG; // 0x04
  uint32_t resv1[1];                // 0x08
  __IO uint32_t NMI_INT_CTRL_REG;   // 0x0c
  __IO uint32_t INTC_PEND_REG[2];   // 0x10
  uint32_t resv2[2];                // 0x18
  __IO uint32_t INTC_EN_REG[2];     // 0x20
  uint32_t resv3[2];                // 0x28
  __IO uint32_t INTC_MASK_REG[2];   // 0x30
  uint32_t resv4[2];                // 0x38
  __IO uint32_t INTC_RESP_REG[2];   // 0x40
  uint32_t resv5[2];                // 0x48
  __IO uint32_t INTC_FF_REG[2];     // 0x50
  uint32_t resv6[2];                // 0x58
  __IO uint32_t INTC_PRIO_REG[4];   // 0x60
} INTC_TypeDef;

#ifndef COUNTOF
#define COUNTOF(ar) (sizeof(ar)/sizeof(ar[0]))
#endif

#define INTC  ((INTC_TypeDef*)0x01C20400)

static IRQHandleTypeDef irq_table[64] __attribute__((used, aligned(32)));

void arm32_do_irq(struct arm_regs_t * regs)
{
  uint8_t nIRQ = f1c100s_intc_get_nirq();

  // ForceIRQ flag must be cleared by ISR
  // Otherwise ISR will be entered repeatedly
  INTC->INTC_FF_REG[nIRQ / 32] &= ~(1 << nIRQ);
  // Call the drivers ISR
  f1c100s_intc_dispatch(nIRQ);
  // Clear pending at the end of ISR
  f1c100s_intc_clear_pend(nIRQ);
}

void arm32_do_fiq(struct arm_regs_t * regs)
{
  // Call the drivers ISR
  f1c100s_intc_dispatch(0);
  // Clear pending at the end of ISR.
  f1c100s_intc_clear_pend(0);
}

/*
* Read active IRQ number
* @return: none
*/
uint8_t f1c100s_intc_get_nirq(void)
{
  return ((INTC->INTC_VECTOR_REG >> 2) & 0x3F);
}

/*
* Execute ISR corresponding to IRQ number
* @nIRQ: IRQ number
* @return: none
*/
void f1c100s_intc_dispatch(uint8_t nIRQ)
{
  IRQHandleTypeDef handle = irq_table[nIRQ];
  if (handle)
    handle();
}

/*
* Set handler function for specified IRQ
* @nIRQ: IRQ number
* @handle: Handle function
* @return: none
*/
void f1c100s_intc_set_isr(uint8_t nIRQ, IRQHandleTypeDef handle)
{
  if (nIRQ < COUNTOF(irq_table)) {
    irq_table[nIRQ] = handle;
  }
}

/*
* Enable IRQ
* @nIRQ: IRQ number
* @return: none
*/
void f1c100s_intc_enable_irq(uint8_t nIRQ)
{
  INTC->INTC_EN_REG[nIRQ / 32] |= (1 << (nIRQ % 32));
}

/*
* Disable IRQ
* @nIRQ: IRQ number
* @return: none
*/
void f1c100s_intc_disable_irq(uint8_t nIRQ)
{
  INTC->INTC_EN_REG[nIRQ / 32] &= ~(1 << (nIRQ % 32));
}

/*
* Mask IRQ
* @nIRQ: IRQ number
* @return: none
*/
void f1c100s_intc_mask_irq(uint8_t nIRQ)
{
  INTC->INTC_MASK_REG[nIRQ / 32] |= (1 << (nIRQ % 32));
}

/*
* Unmask IRQ
* @nIRQ: IRQ number
* @return: none
*/
void f1c100s_intc_unmask_irq(uint8_t nIRQ)
{
  INTC->INTC_MASK_REG[nIRQ / 32] &= ~(1 << (nIRQ % 32));
}

/*
* Immediately trigger IRQ
* @nIRQ: IRQ number
* @return: none
*/
void f1c100s_intc_force_irq(uint8_t nIRQ)
{
  // This bit is to be cleared in IRQ handler
  INTC->INTC_FF_REG[nIRQ / 32] = (1 << (nIRQ % 32));
}

/*
* Clear pending flag
* @nIRQ: IRQ number
* @return: none
*/
void f1c100s_intc_clear_pend(uint8_t nIRQ)
{
  INTC->INTC_PEND_REG[nIRQ / 32] = (1 << (nIRQ % 32));
}


/*
* Initialize IRQ module
* @return: none
*/
void f1c100s_intc_init(void)
{
  INTC->INTC_EN_REG[0] = INTC->INTC_EN_REG[1] = 0;
  INTC->INTC_MASK_REG[0] = INTC->INTC_MASK_REG[1] = 0;
  INTC->INTC_FF_REG[0] = INTC->INTC_FF_REG[1] = 0;
  INTC->INTC_RESP_REG[0] = INTC->INTC_RESP_REG[1] = 0;
  INTC->INTC_PEND_REG[0] = INTC->INTC_PEND_REG[1] = ~0UL;
  INTC->INTC_BASE_ADDR_REG = 0;
  INTC->NMI_INT_CTRL_REG = 0;
  for (unsigned int i = 0; i < COUNTOF(irq_table); i++) {
    irq_table[i] = 0;
  }
}
