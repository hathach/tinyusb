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

  .weak PM_Handler
  .thumb_set PM_Handler,Dummy_Handler

  .weak SYSCTRL_Handler
  .thumb_set SYSCTRL_Handler,Dummy_Handler

  .weak WDT_Handler
  .thumb_set WDT_Handler,Dummy_Handler

  .weak RTC_Handler
  .thumb_set RTC_Handler,Dummy_Handler

  .weak EIC_Handler
  .thumb_set EIC_Handler,Dummy_Handler

  .weak NVMCTRL_Handler
  .thumb_set NVMCTRL_Handler,Dummy_Handler

  .weak DMAC_Handler
  .thumb_set DMAC_Handler,Dummy_Handler

  .weak USB_Handler
  .thumb_set USB_Handler,Dummy_Handler

  .weak EVSYS_Handler
  .thumb_set EVSYS_Handler,Dummy_Handler

  .weak SERCOM0_Handler
  .thumb_set SERCOM0_Handler,Dummy_Handler

  .weak SERCOM1_Handler
  .thumb_set SERCOM1_Handler,Dummy_Handler

  .weak SERCOM2_Handler
  .thumb_set SERCOM2_Handler,Dummy_Handler

  .weak SERCOM3_Handler
  .thumb_set SERCOM3_Handler,Dummy_Handler

  .weak SERCOM4_Handler
  .thumb_set SERCOM4_Handler,Dummy_Handler

  .weak SERCOM5_Handler
  .thumb_set SERCOM5_Handler,Dummy_Handler

  .weak TCC0_Handler
  .thumb_set TCC0_Handler,Dummy_Handler

  .weak TCC1_Handler
  .thumb_set TCC1_Handler,Dummy_Handler

  .weak TCC2_Handler
  .thumb_set TCC2_Handler,Dummy_Handler

  .weak TC3_Handler
  .thumb_set TC3_Handler,Dummy_Handler

  .weak TC4_Handler
  .thumb_set TC4_Handler,Dummy_Handler

  .weak TC5_Handler
  .thumb_set TC5_Handler,Dummy_Handler

  .weak ADC_Handler
  .thumb_set ADC_Handler,Dummy_Handler

  .weak AC_Handler
  .thumb_set AC_Handler,Dummy_Handler

  .weak DAC_Handler
  .thumb_set DAC_Handler,Dummy_Handler

  .weak I2S_Handler
  .thumb_set I2S_Handler,Dummy_Handler

#else

  .thumb_func
  .weak PM_Handler
PM_Handler:
  b .

  .thumb_func
  .weak SYSCTRL_Handler
SYSCTRL_Handler:
  b .

  .thumb_func
  .weak WDT_Handler
WDT_Handler:
  b .

  .thumb_func
  .weak RTC_Handler
RTC_Handler:
  b .

  .thumb_func
  .weak EIC_Handler
EIC_Handler:
  b .

  .thumb_func
  .weak NVMCTRL_Handler
NVMCTRL_Handler:
  b .

  .thumb_func
  .weak DMAC_Handler
DMAC_Handler:
  b .

  .thumb_func
  .weak USB_Handler
USB_Handler:
  b .

  .thumb_func
  .weak EVSYS_Handler
EVSYS_Handler:
  b .

  .thumb_func
  .weak SERCOM0_Handler
SERCOM0_Handler:
  b .

  .thumb_func
  .weak SERCOM1_Handler
SERCOM1_Handler:
  b .

  .thumb_func
  .weak SERCOM2_Handler
SERCOM2_Handler:
  b .

  .thumb_func
  .weak SERCOM3_Handler
SERCOM3_Handler:
  b .

  .thumb_func
  .weak SERCOM4_Handler
SERCOM4_Handler:
  b .

  .thumb_func
  .weak SERCOM5_Handler
SERCOM5_Handler:
  b .

  .thumb_func
  .weak TCC0_Handler
TCC0_Handler:
  b .

  .thumb_func
  .weak TCC1_Handler
TCC1_Handler:
  b .

  .thumb_func
  .weak TCC2_Handler
TCC2_Handler:
  b .

  .thumb_func
  .weak TC3_Handler
TC3_Handler:
  b .

  .thumb_func
  .weak TC4_Handler
TC4_Handler:
  b .

  .thumb_func
  .weak TC5_Handler
TC5_Handler:
  b .

  .thumb_func
  .weak ADC_Handler
ADC_Handler:
  b .

  .thumb_func
  .weak AC_Handler
AC_Handler:
  b .

  .thumb_func
  .weak DAC_Handler
DAC_Handler:
  b .

  .thumb_func
  .weak I2S_Handler
I2S_Handler:
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
  .word PM_Handler
  .word SYSCTRL_Handler
  .word WDT_Handler
  .word RTC_Handler
  .word EIC_Handler
  .word NVMCTRL_Handler
  .word DMAC_Handler
  .word USB_Handler
  .word EVSYS_Handler
  .word SERCOM0_Handler
  .word SERCOM1_Handler
  .word SERCOM2_Handler
  .word SERCOM3_Handler
  .word SERCOM4_Handler
  .word SERCOM5_Handler
  .word TCC0_Handler
  .word TCC1_Handler
  .word TCC2_Handler
  .word TC3_Handler
  .word TC4_Handler
  .word TC5_Handler
  .word Dummy_Handler /* Reserved */
  .word Dummy_Handler /* Reserved */
  .word ADC_Handler
  .word AC_Handler
  .word DAC_Handler
  .word Dummy_Handler /* Reserved */
  .word I2S_Handler
_vectors_end:

#ifdef VECTORS_IN_RAM
  .section .vectors_ram, "ax"
  .align 0
  .global _vectors_ram

_vectors_ram:
  .space _vectors_end - _vectors, 0
#endif
