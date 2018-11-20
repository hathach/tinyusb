/*****************************************************************************
 *                   SEGGER Microcontroller GmbH & Co. KG                    *
 *            Solutions for real time microcontroller applications           *
 *****************************************************************************
 *                                                                           *
 *               (c) 2017 SEGGER Microcontroller GmbH & Co. KG               *
 *                                                                           *
 *           Internet: www.segger.com   Support: support@segger.com          *
 *                                                                           *
 *****************************************************************************/

/*****************************************************************************
 *                         Preprocessor Definitions                          *
 *                         ------------------------                          *
 * VECTORS_IN_RAM                                                            *
 *                                                                           *
 *   If defined, an area of RAM will large enough to store the vector table  *
 *   will be reserved.                                                       *
 *                                                                           *
 *****************************************************************************/

  .syntax unified
  .code 16

  .section .init, "ax"
  .align 0

/*****************************************************************************
 * Default Exception Handlers                                                *
 *****************************************************************************/

  .thumb_func
  .weak NMI_Handler
NMI_Handler:
  b .

  .thumb_func
  .weak HardFault_Handler
HardFault_Handler:
  b .

  .thumb_func
  .weak SVC_Handler
SVC_Handler:
  b .

  .thumb_func
  .weak PendSV_Handler
PendSV_Handler:
  b .

  .thumb_func
  .weak SysTick_Handler
SysTick_Handler:
  b .

  .thumb_func
Dummy_Handler:
  b .

#if defined(__OPTIMIZATION_SMALL)

  .weak PM_IRQHandler
  .thumb_set PM_IRQHandler,Dummy_Handler

  .weak SYSCTRL_IRQHandler
  .thumb_set SYSCTRL_IRQHandler,Dummy_Handler

  .weak WDT_IRQHandler
  .thumb_set WDT_IRQHandler,Dummy_Handler

  .weak RTC_IRQHandler
  .thumb_set RTC_IRQHandler,Dummy_Handler

  .weak EIC_IRQHandler
  .thumb_set EIC_IRQHandler,Dummy_Handler

  .weak NVMCTRL_IRQHandler
  .thumb_set NVMCTRL_IRQHandler,Dummy_Handler

  .weak DMAC_IRQHandler
  .thumb_set DMAC_IRQHandler,Dummy_Handler

  .weak USB_IRQHandler
  .thumb_set USB_IRQHandler,Dummy_Handler

  .weak EVSYS_IRQHandler
  .thumb_set EVSYS_IRQHandler,Dummy_Handler

  .weak SERCOM0_IRQHandler
  .thumb_set SERCOM0_IRQHandler,Dummy_Handler

  .weak SERCOM1_IRQHandler
  .thumb_set SERCOM1_IRQHandler,Dummy_Handler

  .weak SERCOM2_IRQHandler
  .thumb_set SERCOM2_IRQHandler,Dummy_Handler

  .weak SERCOM3_IRQHandler
  .thumb_set SERCOM3_IRQHandler,Dummy_Handler

  .weak SERCOM4_IRQHandler
  .thumb_set SERCOM4_IRQHandler,Dummy_Handler

  .weak SERCOM5_IRQHandler
  .thumb_set SERCOM5_IRQHandler,Dummy_Handler

  .weak TCC0_IRQHandler
  .thumb_set TCC0_IRQHandler,Dummy_Handler

  .weak TCC1_IRQHandler
  .thumb_set TCC1_IRQHandler,Dummy_Handler

  .weak TCC2_IRQHandler
  .thumb_set TCC2_IRQHandler,Dummy_Handler

  .weak TC3_IRQHandler
  .thumb_set TC3_IRQHandler,Dummy_Handler

  .weak TC4_IRQHandler
  .thumb_set TC4_IRQHandler,Dummy_Handler

  .weak TC5_IRQHandler
  .thumb_set TC5_IRQHandler,Dummy_Handler

  .weak ADC_IRQHandler
  .thumb_set ADC_IRQHandler,Dummy_Handler

  .weak AC_IRQHandler
  .thumb_set AC_IRQHandler,Dummy_Handler

  .weak DAC_IRQHandler
  .thumb_set DAC_IRQHandler,Dummy_Handler

  .weak I2S_IRQHandler
  .thumb_set I2S_IRQHandler,Dummy_Handler

#else

  .thumb_func
  .weak PM_IRQHandler
PM_IRQHandler:
  b .

  .thumb_func
  .weak SYSCTRL_IRQHandler
SYSCTRL_IRQHandler:
  b .

  .thumb_func
  .weak WDT_IRQHandler
WDT_IRQHandler:
  b .

  .thumb_func
  .weak RTC_IRQHandler
RTC_IRQHandler:
  b .

  .thumb_func
  .weak EIC_IRQHandler
EIC_IRQHandler:
  b .

  .thumb_func
  .weak NVMCTRL_IRQHandler
NVMCTRL_IRQHandler:
  b .

  .thumb_func
  .weak DMAC_IRQHandler
DMAC_IRQHandler:
  b .

  .thumb_func
  .weak USB_IRQHandler
USB_IRQHandler:
  b .

  .thumb_func
  .weak EVSYS_IRQHandler
EVSYS_IRQHandler:
  b .

  .thumb_func
  .weak SERCOM0_IRQHandler
SERCOM0_IRQHandler:
  b .

  .thumb_func
  .weak SERCOM1_IRQHandler
SERCOM1_IRQHandler:
  b .

  .thumb_func
  .weak SERCOM2_IRQHandler
SERCOM2_IRQHandler:
  b .

  .thumb_func
  .weak SERCOM3_IRQHandler
SERCOM3_IRQHandler:
  b .

  .thumb_func
  .weak SERCOM4_IRQHandler
SERCOM4_IRQHandler:
  b .

  .thumb_func
  .weak SERCOM5_IRQHandler
SERCOM5_IRQHandler:
  b .

  .thumb_func
  .weak TCC0_IRQHandler
TCC0_IRQHandler:
  b .

  .thumb_func
  .weak TCC1_IRQHandler
TCC1_IRQHandler:
  b .

  .thumb_func
  .weak TCC2_IRQHandler
TCC2_IRQHandler:
  b .

  .thumb_func
  .weak TC3_IRQHandler
TC3_IRQHandler:
  b .

  .thumb_func
  .weak TC4_IRQHandler
TC4_IRQHandler:
  b .

  .thumb_func
  .weak TC5_IRQHandler
TC5_IRQHandler:
  b .

  .thumb_func
  .weak ADC_IRQHandler
ADC_IRQHandler:
  b .

  .thumb_func
  .weak AC_IRQHandler
AC_IRQHandler:
  b .

  .thumb_func
  .weak DAC_IRQHandler
DAC_IRQHandler:
  b .

  .thumb_func
  .weak I2S_IRQHandler
I2S_IRQHandler:
  b .

#endif

/*****************************************************************************
 * Vector Table                                                              *
 *****************************************************************************/

  .section .vectors, "ax"
  .align 0
  .global _vectors
  .extern __stack_end__
  .extern Reset_Handler

_vectors:
  .word __stack_end__
  .word Reset_Handler
  .word NMI_Handler
  .word HardFault_Handler
  .word 0 /* Reserved */
  .word 0 /* Reserved */
  .word 0 /* Reserved */
  .word 0 /* Reserved */
  .word 0 /* Reserved */
  .word 0 /* Reserved */
  .word 0 /* Reserved */
  .word SVC_Handler
  .word 0 /* Reserved */
  .word 0 /* Reserved */
  .word PendSV_Handler
  .word SysTick_Handler
  .word PM_IRQHandler
  .word SYSCTRL_IRQHandler
  .word WDT_IRQHandler
  .word RTC_IRQHandler
  .word EIC_IRQHandler
  .word NVMCTRL_IRQHandler
  .word DMAC_IRQHandler
  .word USB_IRQHandler
  .word EVSYS_IRQHandler
  .word SERCOM0_IRQHandler
  .word SERCOM1_IRQHandler
  .word SERCOM2_IRQHandler
  .word SERCOM3_IRQHandler
  .word SERCOM4_IRQHandler
  .word SERCOM5_IRQHandler
  .word TCC0_IRQHandler
  .word TCC1_IRQHandler
  .word TCC2_IRQHandler
  .word TC3_IRQHandler
  .word TC4_IRQHandler
  .word TC5_IRQHandler
  .word Dummy_Handler /* Reserved */
  .word Dummy_Handler /* Reserved */
  .word ADC_IRQHandler
  .word AC_IRQHandler
  .word DAC_IRQHandler
  .word Dummy_Handler /* Reserved */
  .word I2S_IRQHandler
_vectors_end:

#ifdef VECTORS_IN_RAM
  .section .vectors_ram, "ax"
  .align 0
  .global _vectors_ram

_vectors_ram:
  .space _vectors_end - _vectors, 0
#endif
