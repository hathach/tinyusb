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
 * STARTUP_FROM_RESET                                                        *
 *                                                                           *
 *   If defined, the program will startup from power-on/reset. If not        *
 *   defined the program will just loop endlessly from power-on/reset.       *
 *                                                                           *
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

#ifndef STARTUP_FROM_RESET

  .thumb_func
  .weak Reset_Wait
Reset_Wait:
  b .

#endif

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

  .weak MCLK_Handler
  .thumb_set MCLK_Handler,Dummy_Handler

  .weak OSCCTRL_0_Handler
  .thumb_set OSCCTRL_0_Handler,Dummy_Handler

  .weak OSCCTRL_1_Handler
  .thumb_set OSCCTRL_1_Handler,Dummy_Handler

  .weak OSCCTRL_2_Handler
  .thumb_set OSCCTRL_2_Handler,Dummy_Handler

  .weak OSCCTRL_3_Handler
  .thumb_set OSCCTRL_3_Handler,Dummy_Handler

  .weak OSCCTRL_4_Handler
  .thumb_set OSCCTRL_4_Handler,Dummy_Handler

  .weak OSC32KCTRL_Handler
  .thumb_set OSC32KCTRL_Handler,Dummy_Handler

  .weak SUPC_0_Handler
  .thumb_set SUPC_0_Handler,Dummy_Handler

  .weak SUPC_1_Handler
  .thumb_set SUPC_1_Handler,Dummy_Handler

  .weak WDT_Handler
  .thumb_set WDT_Handler,Dummy_Handler

  .weak RTC_Handler
  .thumb_set RTC_Handler,Dummy_Handler

  .weak EIC_0_Handler
  .thumb_set EIC_0_Handler,Dummy_Handler

  .weak EIC_1_Handler
  .thumb_set EIC_1_Handler,Dummy_Handler

  .weak EIC_2_Handler
  .thumb_set EIC_2_Handler,Dummy_Handler

  .weak EIC_3_Handler
  .thumb_set EIC_3_Handler,Dummy_Handler

  .weak EIC_4_Handler
  .thumb_set EIC_4_Handler,Dummy_Handler

  .weak EIC_5_Handler
  .thumb_set EIC_5_Handler,Dummy_Handler

  .weak EIC_6_Handler
  .thumb_set EIC_6_Handler,Dummy_Handler

  .weak EIC_7_Handler
  .thumb_set EIC_7_Handler,Dummy_Handler

  .weak EIC_8_Handler
  .thumb_set EIC_8_Handler,Dummy_Handler

  .weak EIC_9_Handler
  .thumb_set EIC_9_Handler,Dummy_Handler

  .weak EIC_10_Handler
  .thumb_set EIC_10_Handler,Dummy_Handler

  .weak EIC_11_Handler
  .thumb_set EIC_11_Handler,Dummy_Handler

  .weak EIC_12_Handler
  .thumb_set EIC_12_Handler,Dummy_Handler

  .weak EIC_13_Handler
  .thumb_set EIC_13_Handler,Dummy_Handler

  .weak EIC_14_Handler
  .thumb_set EIC_14_Handler,Dummy_Handler

  .weak EIC_15_Handler
  .thumb_set EIC_15_Handler,Dummy_Handler

  .weak FREQM_Handler
  .thumb_set FREQM_Handler,Dummy_Handler

  .weak NVMCTRL_0_Handler
  .thumb_set NVMCTRL_0_Handler,Dummy_Handler

  .weak NVMCTRL_1_Handler
  .thumb_set NVMCTRL_1_Handler,Dummy_Handler

  .weak DMAC_0_Handler
  .thumb_set DMAC_0_Handler,Dummy_Handler

  .weak DMAC_1_Handler
  .thumb_set DMAC_1_Handler,Dummy_Handler

  .weak DMAC_2_Handler
  .thumb_set DMAC_2_Handler,Dummy_Handler

  .weak DMAC_3_Handler
  .thumb_set DMAC_3_Handler,Dummy_Handler

  .weak DMAC_4_Handler
  .thumb_set DMAC_4_Handler,Dummy_Handler

  .weak EVSYS_0_Handler
  .thumb_set EVSYS_0_Handler,Dummy_Handler

  .weak EVSYS_1_Handler
  .thumb_set EVSYS_1_Handler,Dummy_Handler

  .weak EVSYS_2_Handler
  .thumb_set EVSYS_2_Handler,Dummy_Handler

  .weak EVSYS_3_Handler
  .thumb_set EVSYS_3_Handler,Dummy_Handler

  .weak EVSYS_4_Handler
  .thumb_set EVSYS_4_Handler,Dummy_Handler

  .weak PAC_Handler
  .thumb_set PAC_Handler,Dummy_Handler

  .weak TAL_0_Handler
  .thumb_set TAL_0_Handler,Dummy_Handler

  .weak TAL_1_Handler
  .thumb_set TAL_1_Handler,Dummy_Handler

  .weak RAMECC_Handler
  .thumb_set RAMECC_Handler,Dummy_Handler

  .weak SERCOM0_0_Handler
  .thumb_set SERCOM0_0_Handler,Dummy_Handler

  .weak SERCOM0_1_Handler
  .thumb_set SERCOM0_1_Handler,Dummy_Handler

  .weak SERCOM0_2_Handler
  .thumb_set SERCOM0_2_Handler,Dummy_Handler

  .weak SERCOM0_3_Handler
  .thumb_set SERCOM0_3_Handler,Dummy_Handler

  .weak SERCOM1_0_Handler
  .thumb_set SERCOM1_0_Handler,Dummy_Handler

  .weak SERCOM1_1_Handler
  .thumb_set SERCOM1_1_Handler,Dummy_Handler

  .weak SERCOM1_2_Handler
  .thumb_set SERCOM1_2_Handler,Dummy_Handler

  .weak SERCOM1_3_Handler
  .thumb_set SERCOM1_3_Handler,Dummy_Handler

  .weak SERCOM2_0_Handler
  .thumb_set SERCOM2_0_Handler,Dummy_Handler

  .weak SERCOM2_1_Handler
  .thumb_set SERCOM2_1_Handler,Dummy_Handler

  .weak SERCOM2_2_Handler
  .thumb_set SERCOM2_2_Handler,Dummy_Handler

  .weak SERCOM2_3_Handler
  .thumb_set SERCOM2_3_Handler,Dummy_Handler

  .weak SERCOM3_0_Handler
  .thumb_set SERCOM3_0_Handler,Dummy_Handler

  .weak SERCOM3_1_Handler
  .thumb_set SERCOM3_1_Handler,Dummy_Handler

  .weak SERCOM3_2_Handler
  .thumb_set SERCOM3_2_Handler,Dummy_Handler

  .weak SERCOM3_3_Handler
  .thumb_set SERCOM3_3_Handler,Dummy_Handler

  .weak SERCOM4_0_Handler
  .thumb_set SERCOM4_0_Handler,Dummy_Handler

  .weak SERCOM4_1_Handler
  .thumb_set SERCOM4_1_Handler,Dummy_Handler

  .weak SERCOM4_2_Handler
  .thumb_set SERCOM4_2_Handler,Dummy_Handler

  .weak SERCOM4_3_Handler
  .thumb_set SERCOM4_3_Handler,Dummy_Handler

  .weak SERCOM5_0_Handler
  .thumb_set SERCOM5_0_Handler,Dummy_Handler

  .weak SERCOM5_1_Handler
  .thumb_set SERCOM5_1_Handler,Dummy_Handler

  .weak SERCOM5_2_Handler
  .thumb_set SERCOM5_2_Handler,Dummy_Handler

  .weak SERCOM5_3_Handler
  .thumb_set SERCOM5_3_Handler,Dummy_Handler

  .weak USB_0_Handler
  .thumb_set USB_0_Handler,Dummy_Handler

  .weak USB_1_Handler
  .thumb_set USB_1_Handler,Dummy_Handler

  .weak USB_2_Handler
  .thumb_set USB_2_Handler,Dummy_Handler

  .weak USB_3_Handler
  .thumb_set USB_3_Handler,Dummy_Handler

  .weak TCC0_0_Handler
  .thumb_set TCC0_0_Handler,Dummy_Handler

  .weak TCC0_1_Handler
  .thumb_set TCC0_1_Handler,Dummy_Handler

  .weak TCC0_2_Handler
  .thumb_set TCC0_2_Handler,Dummy_Handler

  .weak TCC0_3_Handler
  .thumb_set TCC0_3_Handler,Dummy_Handler

  .weak TCC0_4_Handler
  .thumb_set TCC0_4_Handler,Dummy_Handler

  .weak TCC0_5_Handler
  .thumb_set TCC0_5_Handler,Dummy_Handler

  .weak TCC0_6_Handler
  .thumb_set TCC0_6_Handler,Dummy_Handler

  .weak TCC1_0_Handler
  .thumb_set TCC1_0_Handler,Dummy_Handler

  .weak TCC1_1_Handler
  .thumb_set TCC1_1_Handler,Dummy_Handler

  .weak TCC1_2_Handler
  .thumb_set TCC1_2_Handler,Dummy_Handler

  .weak TCC1_3_Handler
  .thumb_set TCC1_3_Handler,Dummy_Handler

  .weak TCC1_4_Handler
  .thumb_set TCC1_4_Handler,Dummy_Handler

  .weak TCC2_0_Handler
  .thumb_set TCC2_0_Handler,Dummy_Handler

  .weak TCC2_1_Handler
  .thumb_set TCC2_1_Handler,Dummy_Handler

  .weak TCC2_2_Handler
  .thumb_set TCC2_2_Handler,Dummy_Handler

  .weak TCC2_3_Handler
  .thumb_set TCC2_3_Handler,Dummy_Handler

  .weak TCC3_0_Handler
  .thumb_set TCC3_0_Handler,Dummy_Handler

  .weak TCC3_1_Handler
  .thumb_set TCC3_1_Handler,Dummy_Handler

  .weak TCC3_2_Handler
  .thumb_set TCC3_2_Handler,Dummy_Handler

  .weak TCC4_0_Handler
  .thumb_set TCC4_0_Handler,Dummy_Handler

  .weak TCC4_1_Handler
  .thumb_set TCC4_1_Handler,Dummy_Handler

  .weak TCC4_2_Handler
  .thumb_set TCC4_2_Handler,Dummy_Handler

  .weak TC0_Handler
  .thumb_set TC0_Handler,Dummy_Handler

  .weak TC1_Handler
  .thumb_set TC1_Handler,Dummy_Handler

  .weak TC2_Handler
  .thumb_set TC2_Handler,Dummy_Handler

  .weak TC3_Handler
  .thumb_set TC3_Handler,Dummy_Handler

  .weak TC4_Handler
  .thumb_set TC4_Handler,Dummy_Handler

  .weak TC5_Handler
  .thumb_set TC5_Handler,Dummy_Handler

  .weak PDEC_0_Handler
  .thumb_set PDEC_0_Handler,Dummy_Handler

  .weak PDEC_1_Handler
  .thumb_set PDEC_1_Handler,Dummy_Handler

  .weak PDEC_2_Handler
  .thumb_set PDEC_2_Handler,Dummy_Handler

  .weak ADC0_0_Handler
  .thumb_set ADC0_0_Handler,Dummy_Handler

  .weak ADC0_1_Handler
  .thumb_set ADC0_1_Handler,Dummy_Handler

  .weak ADC1_0_Handler
  .thumb_set ADC1_0_Handler,Dummy_Handler

  .weak ADC1_1_Handler
  .thumb_set ADC1_1_Handler,Dummy_Handler

  .weak AC_Handler
  .thumb_set AC_Handler,Dummy_Handler

  .weak DAC_0_Handler
  .thumb_set DAC_0_Handler,Dummy_Handler

  .weak DAC_1_Handler
  .thumb_set DAC_1_Handler,Dummy_Handler

  .weak DAC_2_Handler
  .thumb_set DAC_2_Handler,Dummy_Handler

  .weak DAC_3_Handler
  .thumb_set DAC_3_Handler,Dummy_Handler

  .weak DAC_4_Handler
  .thumb_set DAC_4_Handler,Dummy_Handler

  .weak I2S_Handler
  .thumb_set I2S_Handler,Dummy_Handler

  .weak PCC_Handler
  .thumb_set PCC_Handler,Dummy_Handler

  .weak AES_Handler
  .thumb_set AES_Handler,Dummy_Handler

  .weak TRNG_Handler
  .thumb_set TRNG_Handler,Dummy_Handler

  .weak ICM_Handler
  .thumb_set ICM_Handler,Dummy_Handler

  .weak QSPI_Handler
  .thumb_set QSPI_Handler,Dummy_Handler

  .weak SDHC0_Handler
  .thumb_set SDHC0_Handler,Dummy_Handler

#else

  .thumb_func
  .weak PM_Handler
PM_Handler:
  b .

  .thumb_func
  .weak MCLK_Handler
MCLK_Handler:
  b .

  .thumb_func
  .weak OSCCTRL_0_Handler
OSCCTRL_0_Handler:
  b .

  .thumb_func
  .weak OSCCTRL_1_Handler
OSCCTRL_1_Handler:
  b .

  .thumb_func
  .weak OSCCTRL_2_Handler
OSCCTRL_2_Handler:
  b .

  .thumb_func
  .weak OSCCTRL_3_Handler
OSCCTRL_3_Handler:
  b .

  .thumb_func
  .weak OSCCTRL_4_Handler
OSCCTRL_4_Handler:
  b .

  .thumb_func
  .weak OSC32KCTRL_Handler
OSC32KCTRL_Handler:
  b .

  .thumb_func
  .weak SUPC_0_Handler
SUPC_0_Handler:
  b .

  .thumb_func
  .weak SUPC_1_Handler
SUPC_1_Handler:
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
  .weak EIC_0_Handler
EIC_0_Handler:
  b .

  .thumb_func
  .weak EIC_1_Handler
EIC_1_Handler:
  b .

  .thumb_func
  .weak EIC_2_Handler
EIC_2_Handler:
  b .

  .thumb_func
  .weak EIC_3_Handler
EIC_3_Handler:
  b .

  .thumb_func
  .weak EIC_4_Handler
EIC_4_Handler:
  b .

  .thumb_func
  .weak EIC_5_Handler
EIC_5_Handler:
  b .

  .thumb_func
  .weak EIC_6_Handler
EIC_6_Handler:
  b .

  .thumb_func
  .weak EIC_7_Handler
EIC_7_Handler:
  b .

  .thumb_func
  .weak EIC_8_Handler
EIC_8_Handler:
  b .

  .thumb_func
  .weak EIC_9_Handler
EIC_9_Handler:
  b .

  .thumb_func
  .weak EIC_10_Handler
EIC_10_Handler:
  b .

  .thumb_func
  .weak EIC_11_Handler
EIC_11_Handler:
  b .

  .thumb_func
  .weak EIC_12_Handler
EIC_12_Handler:
  b .

  .thumb_func
  .weak EIC_13_Handler
EIC_13_Handler:
  b .

  .thumb_func
  .weak EIC_14_Handler
EIC_14_Handler:
  b .

  .thumb_func
  .weak EIC_15_Handler
EIC_15_Handler:
  b .

  .thumb_func
  .weak FREQM_Handler
FREQM_Handler:
  b .

  .thumb_func
  .weak NVMCTRL_0_Handler
NVMCTRL_0_Handler:
  b .

  .thumb_func
  .weak NVMCTRL_1_Handler
NVMCTRL_1_Handler:
  b .

  .thumb_func
  .weak DMAC_0_Handler
DMAC_0_Handler:
  b .

  .thumb_func
  .weak DMAC_1_Handler
DMAC_1_Handler:
  b .

  .thumb_func
  .weak DMAC_2_Handler
DMAC_2_Handler:
  b .

  .thumb_func
  .weak DMAC_3_Handler
DMAC_3_Handler:
  b .

  .thumb_func
  .weak DMAC_4_Handler
DMAC_4_Handler:
  b .

  .thumb_func
  .weak EVSYS_0_Handler
EVSYS_0_Handler:
  b .

  .thumb_func
  .weak EVSYS_1_Handler
EVSYS_1_Handler:
  b .

  .thumb_func
  .weak EVSYS_2_Handler
EVSYS_2_Handler:
  b .

  .thumb_func
  .weak EVSYS_3_Handler
EVSYS_3_Handler:
  b .

  .thumb_func
  .weak EVSYS_4_Handler
EVSYS_4_Handler:
  b .

  .thumb_func
  .weak PAC_Handler
PAC_Handler:
  b .

  .thumb_func
  .weak TAL_0_Handler
TAL_0_Handler:
  b .

  .thumb_func
  .weak TAL_1_Handler
TAL_1_Handler:
  b .

  .thumb_func
  .weak RAMECC_Handler
RAMECC_Handler:
  b .

  .thumb_func
  .weak SERCOM0_0_Handler
SERCOM0_0_Handler:
  b .

  .thumb_func
  .weak SERCOM0_1_Handler
SERCOM0_1_Handler:
  b .

  .thumb_func
  .weak SERCOM0_2_Handler
SERCOM0_2_Handler:
  b .

  .thumb_func
  .weak SERCOM0_3_Handler
SERCOM0_3_Handler:
  b .

  .thumb_func
  .weak SERCOM1_0_Handler
SERCOM1_0_Handler:
  b .

  .thumb_func
  .weak SERCOM1_1_Handler
SERCOM1_1_Handler:
  b .

  .thumb_func
  .weak SERCOM1_2_Handler
SERCOM1_2_Handler:
  b .

  .thumb_func
  .weak SERCOM1_3_Handler
SERCOM1_3_Handler:
  b .

  .thumb_func
  .weak SERCOM2_0_Handler
SERCOM2_0_Handler:
  b .

  .thumb_func
  .weak SERCOM2_1_Handler
SERCOM2_1_Handler:
  b .

  .thumb_func
  .weak SERCOM2_2_Handler
SERCOM2_2_Handler:
  b .

  .thumb_func
  .weak SERCOM2_3_Handler
SERCOM2_3_Handler:
  b .

  .thumb_func
  .weak SERCOM3_0_Handler
SERCOM3_0_Handler:
  b .

  .thumb_func
  .weak SERCOM3_1_Handler
SERCOM3_1_Handler:
  b .

  .thumb_func
  .weak SERCOM3_2_Handler
SERCOM3_2_Handler:
  b .

  .thumb_func
  .weak SERCOM3_3_Handler
SERCOM3_3_Handler:
  b .

  .thumb_func
  .weak SERCOM4_0_Handler
SERCOM4_0_Handler:
  b .

  .thumb_func
  .weak SERCOM4_1_Handler
SERCOM4_1_Handler:
  b .

  .thumb_func
  .weak SERCOM4_2_Handler
SERCOM4_2_Handler:
  b .

  .thumb_func
  .weak SERCOM4_3_Handler
SERCOM4_3_Handler:
  b .

  .thumb_func
  .weak SERCOM5_0_Handler
SERCOM5_0_Handler:
  b .

  .thumb_func
  .weak SERCOM5_1_Handler
SERCOM5_1_Handler:
  b .

  .thumb_func
  .weak SERCOM5_2_Handler
SERCOM5_2_Handler:
  b .

  .thumb_func
  .weak SERCOM5_3_Handler
SERCOM5_3_Handler:
  b .

  .thumb_func
  .weak USB_0_Handler
USB_0_Handler:
  b .

  .thumb_func
  .weak USB_1_Handler
USB_1_Handler:
  b .

  .thumb_func
  .weak USB_2_Handler
USB_2_Handler:
  b .

  .thumb_func
  .weak USB_3_Handler
USB_3_Handler:
  b .

  .thumb_func
  .weak TCC0_0_Handler
TCC0_0_Handler:
  b .

  .thumb_func
  .weak TCC0_1_Handler
TCC0_1_Handler:
  b .

  .thumb_func
  .weak TCC0_2_Handler
TCC0_2_Handler:
  b .

  .thumb_func
  .weak TCC0_3_Handler
TCC0_3_Handler:
  b .

  .thumb_func
  .weak TCC0_4_Handler
TCC0_4_Handler:
  b .

  .thumb_func
  .weak TCC0_5_Handler
TCC0_5_Handler:
  b .

  .thumb_func
  .weak TCC0_6_Handler
TCC0_6_Handler:
  b .

  .thumb_func
  .weak TCC1_0_Handler
TCC1_0_Handler:
  b .

  .thumb_func
  .weak TCC1_1_Handler
TCC1_1_Handler:
  b .

  .thumb_func
  .weak TCC1_2_Handler
TCC1_2_Handler:
  b .

  .thumb_func
  .weak TCC1_3_Handler
TCC1_3_Handler:
  b .

  .thumb_func
  .weak TCC1_4_Handler
TCC1_4_Handler:
  b .

  .thumb_func
  .weak TCC2_0_Handler
TCC2_0_Handler:
  b .

  .thumb_func
  .weak TCC2_1_Handler
TCC2_1_Handler:
  b .

  .thumb_func
  .weak TCC2_2_Handler
TCC2_2_Handler:
  b .

  .thumb_func
  .weak TCC2_3_Handler
TCC2_3_Handler:
  b .

  .thumb_func
  .weak TCC3_0_Handler
TCC3_0_Handler:
  b .

  .thumb_func
  .weak TCC3_1_Handler
TCC3_1_Handler:
  b .

  .thumb_func
  .weak TCC3_2_Handler
TCC3_2_Handler:
  b .

  .thumb_func
  .weak TCC4_0_Handler
TCC4_0_Handler:
  b .

  .thumb_func
  .weak TCC4_1_Handler
TCC4_1_Handler:
  b .

  .thumb_func
  .weak TCC4_2_Handler
TCC4_2_Handler:
  b .

  .thumb_func
  .weak TC0_Handler
TC0_Handler:
  b .

  .thumb_func
  .weak TC1_Handler
TC1_Handler:
  b .

  .thumb_func
  .weak TC2_Handler
TC2_Handler:
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
  .weak PDEC_0_Handler
PDEC_0_Handler:
  b .

  .thumb_func
  .weak PDEC_1_Handler
PDEC_1_Handler:
  b .

  .thumb_func
  .weak PDEC_2_Handler
PDEC_2_Handler:
  b .

  .thumb_func
  .weak ADC0_0_Handler
ADC0_0_Handler:
  b .

  .thumb_func
  .weak ADC0_1_Handler
ADC0_1_Handler:
  b .

  .thumb_func
  .weak ADC1_0_Handler
ADC1_0_Handler:
  b .

  .thumb_func
  .weak ADC1_1_Handler
ADC1_1_Handler:
  b .

  .thumb_func
  .weak AC_Handler
AC_Handler:
  b .

  .thumb_func
  .weak DAC_0_Handler
DAC_0_Handler:
  b .

  .thumb_func
  .weak DAC_1_Handler
DAC_1_Handler:
  b .

  .thumb_func
  .weak DAC_2_Handler
DAC_2_Handler:
  b .

  .thumb_func
  .weak DAC_3_Handler
DAC_3_Handler:
  b .

  .thumb_func
  .weak DAC_4_Handler
DAC_4_Handler:
  b .

  .thumb_func
  .weak I2S_Handler
I2S_Handler:
  b .

  .thumb_func
  .weak PCC_Handler
PCC_Handler:
  b .

  .thumb_func
  .weak AES_Handler
AES_Handler:
  b .

  .thumb_func
  .weak TRNG_Handler
TRNG_Handler:
  b .

  .thumb_func
  .weak ICM_Handler
ICM_Handler:
  b .

  .thumb_func
  .weak QSPI_Handler
QSPI_Handler:
  b .

  .thumb_func
  .weak SDHC0_Handler
SDHC0_Handler:
  b .

#endif

/*****************************************************************************
 * Vector Table                                                              *
 *****************************************************************************/

  .section .vectors, "ax"
  .align 0
  .global _vectors
  .extern __stack_end__
#ifdef STARTUP_FROM_RESET
  .extern Reset_Handler
#endif

_vectors:
  .word __stack_end__
#ifdef STARTUP_FROM_RESET
  .word Reset_Handler
#else
  .word Reset_Wait
#endif
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
  .word MCLK_Handler
  .word OSCCTRL_0_Handler
  .word OSCCTRL_1_Handler
  .word OSCCTRL_2_Handler
  .word OSCCTRL_3_Handler
  .word OSCCTRL_4_Handler
  .word OSC32KCTRL_Handler
  .word SUPC_0_Handler
  .word SUPC_1_Handler
  .word WDT_Handler
  .word RTC_Handler
  .word EIC_0_Handler
  .word EIC_1_Handler
  .word EIC_2_Handler
  .word EIC_3_Handler
  .word EIC_4_Handler
  .word EIC_5_Handler
  .word EIC_6_Handler
  .word EIC_7_Handler
  .word EIC_8_Handler
  .word EIC_9_Handler
  .word EIC_10_Handler
  .word EIC_11_Handler
  .word EIC_12_Handler
  .word EIC_13_Handler
  .word EIC_14_Handler
  .word EIC_15_Handler
  .word FREQM_Handler
  .word NVMCTRL_0_Handler
  .word NVMCTRL_1_Handler
  .word DMAC_0_Handler
  .word DMAC_1_Handler
  .word DMAC_2_Handler
  .word DMAC_3_Handler
  .word DMAC_4_Handler
  .word EVSYS_0_Handler
  .word EVSYS_1_Handler
  .word EVSYS_2_Handler
  .word EVSYS_3_Handler
  .word EVSYS_4_Handler
  .word PAC_Handler
  .word TAL_0_Handler
  .word TAL_1_Handler
  .word Dummy_Handler /* Reserved */
  .word RAMECC_Handler
  .word SERCOM0_0_Handler
  .word SERCOM0_1_Handler
  .word SERCOM0_2_Handler
  .word SERCOM0_3_Handler
  .word SERCOM1_0_Handler
  .word SERCOM1_1_Handler
  .word SERCOM1_2_Handler
  .word SERCOM1_3_Handler
  .word SERCOM2_0_Handler
  .word SERCOM2_1_Handler
  .word SERCOM2_2_Handler
  .word SERCOM2_3_Handler
  .word SERCOM3_0_Handler
  .word SERCOM3_1_Handler
  .word SERCOM3_2_Handler
  .word SERCOM3_3_Handler
  .word SERCOM4_0_Handler
  .word SERCOM4_1_Handler
  .word SERCOM4_2_Handler
  .word SERCOM4_3_Handler
  .word SERCOM5_0_Handler
  .word SERCOM5_1_Handler
  .word SERCOM5_2_Handler
  .word SERCOM5_3_Handler
  .word Dummy_Handler /* Reserved */
  .word Dummy_Handler /* Reserved */
  .word Dummy_Handler /* Reserved */
  .word Dummy_Handler /* Reserved */
  .word Dummy_Handler /* Reserved */
  .word Dummy_Handler /* Reserved */
  .word Dummy_Handler /* Reserved */
  .word Dummy_Handler /* Reserved */
  .word Dummy_Handler /* Reserved */
  .word Dummy_Handler /* Reserved */
  .word USB_0_Handler
  .word USB_1_Handler
  .word USB_2_Handler
  .word USB_3_Handler
  .word Dummy_Handler /* Reserved */
  .word TCC0_0_Handler
  .word TCC0_1_Handler
  .word TCC0_2_Handler
  .word TCC0_3_Handler
  .word TCC0_4_Handler
  .word TCC0_5_Handler
  .word TCC0_6_Handler
  .word TCC1_0_Handler
  .word TCC1_1_Handler
  .word TCC1_2_Handler
  .word TCC1_3_Handler
  .word TCC1_4_Handler
  .word TCC2_0_Handler
  .word TCC2_1_Handler
  .word TCC2_2_Handler
  .word TCC2_3_Handler
  .word TCC3_0_Handler
  .word TCC3_1_Handler
  .word TCC3_2_Handler
  .word TCC4_0_Handler
  .word TCC4_1_Handler
  .word TCC4_2_Handler
  .word TC0_Handler
  .word TC1_Handler
  .word TC2_Handler
  .word TC3_Handler
  .word TC4_Handler
  .word TC5_Handler
  .word Dummy_Handler /* Reserved */
  .word Dummy_Handler /* Reserved */
  .word PDEC_0_Handler
  .word PDEC_1_Handler
  .word PDEC_2_Handler
  .word ADC0_0_Handler
  .word ADC0_1_Handler
  .word ADC1_0_Handler
  .word ADC1_1_Handler
  .word AC_Handler
  .word DAC_0_Handler
  .word DAC_1_Handler
  .word DAC_2_Handler
  .word DAC_3_Handler
  .word DAC_4_Handler
  .word I2S_Handler
  .word PCC_Handler
  .word AES_Handler
  .word TRNG_Handler
  .word ICM_Handler
  .word Dummy_Handler /* Reserved */
  .word QSPI_Handler
  .word SDHC0_Handler
_vectors_end:

#ifdef VECTORS_IN_RAM
  .section .vectors_ram, "ax"
  .align 0
  .global _vectors_ram

_vectors_ram:
  .space _vectors_end - _vectors, 0
#endif
