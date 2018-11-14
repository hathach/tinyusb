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

  .weak PM_INTREQ_IRQHandler
  .thumb_set PM_INTREQ_IRQHandler,Dummy_Handler

  .weak MCLK_INTREQ_IRQHandler
  .thumb_set MCLK_INTREQ_IRQHandler,Dummy_Handler

  .weak OSCCTRL_INTREQ_0_IRQHandler
  .thumb_set OSCCTRL_INTREQ_0_IRQHandler,Dummy_Handler

  .weak OSCCTRL_INTREQ_1_IRQHandler
  .thumb_set OSCCTRL_INTREQ_1_IRQHandler,Dummy_Handler

  .weak OSCCTRL_INTREQ_2_IRQHandler
  .thumb_set OSCCTRL_INTREQ_2_IRQHandler,Dummy_Handler

  .weak OSCCTRL_INTREQ_3_IRQHandler
  .thumb_set OSCCTRL_INTREQ_3_IRQHandler,Dummy_Handler

  .weak OSCCTRL_INTREQ_4_IRQHandler
  .thumb_set OSCCTRL_INTREQ_4_IRQHandler,Dummy_Handler

  .weak OSC32KCTRL_INTREQ_IRQHandler
  .thumb_set OSC32KCTRL_INTREQ_IRQHandler,Dummy_Handler

  .weak SUPC_INTREQ_0_IRQHandler
  .thumb_set SUPC_INTREQ_0_IRQHandler,Dummy_Handler

  .weak SUPC_INTREQ_1_IRQHandler
  .thumb_set SUPC_INTREQ_1_IRQHandler,Dummy_Handler

  .weak WDT_INTREQ_IRQHandler
  .thumb_set WDT_INTREQ_IRQHandler,Dummy_Handler

  .weak RTC_INTREQ_IRQHandler
  .thumb_set RTC_INTREQ_IRQHandler,Dummy_Handler

  .weak EIC_INTREQ_0_IRQHandler
  .thumb_set EIC_INTREQ_0_IRQHandler,Dummy_Handler

  .weak EIC_INTREQ_1_IRQHandler
  .thumb_set EIC_INTREQ_1_IRQHandler,Dummy_Handler

  .weak EIC_INTREQ_2_IRQHandler
  .thumb_set EIC_INTREQ_2_IRQHandler,Dummy_Handler

  .weak EIC_INTREQ_3_IRQHandler
  .thumb_set EIC_INTREQ_3_IRQHandler,Dummy_Handler

  .weak EIC_INTREQ_4_IRQHandler
  .thumb_set EIC_INTREQ_4_IRQHandler,Dummy_Handler

  .weak EIC_INTREQ_5_IRQHandler
  .thumb_set EIC_INTREQ_5_IRQHandler,Dummy_Handler

  .weak EIC_INTREQ_6_IRQHandler
  .thumb_set EIC_INTREQ_6_IRQHandler,Dummy_Handler

  .weak EIC_INTREQ_7_IRQHandler
  .thumb_set EIC_INTREQ_7_IRQHandler,Dummy_Handler

  .weak EIC_INTREQ_8_IRQHandler
  .thumb_set EIC_INTREQ_8_IRQHandler,Dummy_Handler

  .weak EIC_INTREQ_9_IRQHandler
  .thumb_set EIC_INTREQ_9_IRQHandler,Dummy_Handler

  .weak EIC_INTREQ_10_IRQHandler
  .thumb_set EIC_INTREQ_10_IRQHandler,Dummy_Handler

  .weak EIC_INTREQ_11_IRQHandler
  .thumb_set EIC_INTREQ_11_IRQHandler,Dummy_Handler

  .weak EIC_INTREQ_12_IRQHandler
  .thumb_set EIC_INTREQ_12_IRQHandler,Dummy_Handler

  .weak EIC_INTREQ_13_IRQHandler
  .thumb_set EIC_INTREQ_13_IRQHandler,Dummy_Handler

  .weak EIC_INTREQ_14_IRQHandler
  .thumb_set EIC_INTREQ_14_IRQHandler,Dummy_Handler

  .weak EIC_INTREQ_15_IRQHandler
  .thumb_set EIC_INTREQ_15_IRQHandler,Dummy_Handler

  .weak FREQM_INTREQ_IRQHandler
  .thumb_set FREQM_INTREQ_IRQHandler,Dummy_Handler

  .weak NVMCTRL_INTREQ_0_IRQHandler
  .thumb_set NVMCTRL_INTREQ_0_IRQHandler,Dummy_Handler

  .weak NVMCTRL_INTREQ_1_IRQHandler
  .thumb_set NVMCTRL_INTREQ_1_IRQHandler,Dummy_Handler

  .weak DMAC_INTREQ_0_IRQHandler
  .thumb_set DMAC_INTREQ_0_IRQHandler,Dummy_Handler

  .weak DMAC_INTREQ_1_IRQHandler
  .thumb_set DMAC_INTREQ_1_IRQHandler,Dummy_Handler

  .weak DMAC_INTREQ_2_IRQHandler
  .thumb_set DMAC_INTREQ_2_IRQHandler,Dummy_Handler

  .weak DMAC_INTREQ_3_IRQHandler
  .thumb_set DMAC_INTREQ_3_IRQHandler,Dummy_Handler

  .weak DMAC_INTREQ_4_IRQHandler
  .thumb_set DMAC_INTREQ_4_IRQHandler,Dummy_Handler

  .weak EVSYS_INTREQ_0_IRQHandler
  .thumb_set EVSYS_INTREQ_0_IRQHandler,Dummy_Handler

  .weak EVSYS_INTREQ_1_IRQHandler
  .thumb_set EVSYS_INTREQ_1_IRQHandler,Dummy_Handler

  .weak EVSYS_INTREQ_2_IRQHandler
  .thumb_set EVSYS_INTREQ_2_IRQHandler,Dummy_Handler

  .weak EVSYS_INTREQ_3_IRQHandler
  .thumb_set EVSYS_INTREQ_3_IRQHandler,Dummy_Handler

  .weak EVSYS_INTREQ_4_IRQHandler
  .thumb_set EVSYS_INTREQ_4_IRQHandler,Dummy_Handler

  .weak PAC_INTREQ_IRQHandler
  .thumb_set PAC_INTREQ_IRQHandler,Dummy_Handler

  .weak TAL_INTREQ_0_IRQHandler
  .thumb_set TAL_INTREQ_0_IRQHandler,Dummy_Handler

  .weak TAL_INTREQ_1_IRQHandler
  .thumb_set TAL_INTREQ_1_IRQHandler,Dummy_Handler

  .weak RAMECC_INTREQ_IRQHandler
  .thumb_set RAMECC_INTREQ_IRQHandler,Dummy_Handler

  .weak SERCOM0_INTREQ_0_IRQHandler
  .thumb_set SERCOM0_INTREQ_0_IRQHandler,Dummy_Handler

  .weak SERCOM0_INTREQ_1_IRQHandler
  .thumb_set SERCOM0_INTREQ_1_IRQHandler,Dummy_Handler

  .weak SERCOM0_INTREQ_2_IRQHandler
  .thumb_set SERCOM0_INTREQ_2_IRQHandler,Dummy_Handler

  .weak SERCOM0_INTREQ_3_IRQHandler
  .thumb_set SERCOM0_INTREQ_3_IRQHandler,Dummy_Handler

  .weak SERCOM1_INTREQ_0_IRQHandler
  .thumb_set SERCOM1_INTREQ_0_IRQHandler,Dummy_Handler

  .weak SERCOM1_INTREQ_1_IRQHandler
  .thumb_set SERCOM1_INTREQ_1_IRQHandler,Dummy_Handler

  .weak SERCOM1_INTREQ_2_IRQHandler
  .thumb_set SERCOM1_INTREQ_2_IRQHandler,Dummy_Handler

  .weak SERCOM1_INTREQ_3_IRQHandler
  .thumb_set SERCOM1_INTREQ_3_IRQHandler,Dummy_Handler

  .weak SERCOM2_INTREQ_0_IRQHandler
  .thumb_set SERCOM2_INTREQ_0_IRQHandler,Dummy_Handler

  .weak SERCOM2_INTREQ_1_IRQHandler
  .thumb_set SERCOM2_INTREQ_1_IRQHandler,Dummy_Handler

  .weak SERCOM2_INTREQ_2_IRQHandler
  .thumb_set SERCOM2_INTREQ_2_IRQHandler,Dummy_Handler

  .weak SERCOM2_INTREQ_3_IRQHandler
  .thumb_set SERCOM2_INTREQ_3_IRQHandler,Dummy_Handler

  .weak SERCOM3_INTREQ_0_IRQHandler
  .thumb_set SERCOM3_INTREQ_0_IRQHandler,Dummy_Handler

  .weak SERCOM3_INTREQ_1_IRQHandler
  .thumb_set SERCOM3_INTREQ_1_IRQHandler,Dummy_Handler

  .weak SERCOM3_INTREQ_2_IRQHandler
  .thumb_set SERCOM3_INTREQ_2_IRQHandler,Dummy_Handler

  .weak SERCOM3_INTREQ_3_IRQHandler
  .thumb_set SERCOM3_INTREQ_3_IRQHandler,Dummy_Handler

  .weak SERCOM4_INTREQ_0_IRQHandler
  .thumb_set SERCOM4_INTREQ_0_IRQHandler,Dummy_Handler

  .weak SERCOM4_INTREQ_1_IRQHandler
  .thumb_set SERCOM4_INTREQ_1_IRQHandler,Dummy_Handler

  .weak SERCOM4_INTREQ_2_IRQHandler
  .thumb_set SERCOM4_INTREQ_2_IRQHandler,Dummy_Handler

  .weak SERCOM4_INTREQ_3_IRQHandler
  .thumb_set SERCOM4_INTREQ_3_IRQHandler,Dummy_Handler

  .weak SERCOM5_INTREQ_0_IRQHandler
  .thumb_set SERCOM5_INTREQ_0_IRQHandler,Dummy_Handler

  .weak SERCOM5_INTREQ_1_IRQHandler
  .thumb_set SERCOM5_INTREQ_1_IRQHandler,Dummy_Handler

  .weak SERCOM5_INTREQ_2_IRQHandler
  .thumb_set SERCOM5_INTREQ_2_IRQHandler,Dummy_Handler

  .weak SERCOM5_INTREQ_3_IRQHandler
  .thumb_set SERCOM5_INTREQ_3_IRQHandler,Dummy_Handler

  .weak CAN0_INTREQ_IRQHandler
  .thumb_set CAN0_INTREQ_IRQHandler,Dummy_Handler

  .weak CAN1_INTREQ_IRQHandler
  .thumb_set CAN1_INTREQ_IRQHandler,Dummy_Handler

  .weak USB_INTREQ_0_IRQHandler
  .thumb_set USB_INTREQ_0_IRQHandler,Dummy_Handler

  .weak USB_INTREQ_1_IRQHandler
  .thumb_set USB_INTREQ_1_IRQHandler,Dummy_Handler

  .weak USB_INTREQ_2_IRQHandler
  .thumb_set USB_INTREQ_2_IRQHandler,Dummy_Handler

  .weak USB_INTREQ_3_IRQHandler
  .thumb_set USB_INTREQ_3_IRQHandler,Dummy_Handler

  .weak TCC0_INTREQ_0_IRQHandler
  .thumb_set TCC0_INTREQ_0_IRQHandler,Dummy_Handler

  .weak TCC0_INTREQ_1_IRQHandler
  .thumb_set TCC0_INTREQ_1_IRQHandler,Dummy_Handler

  .weak TCC0_INTREQ_2_IRQHandler
  .thumb_set TCC0_INTREQ_2_IRQHandler,Dummy_Handler

  .weak TCC0_INTREQ_3_IRQHandler
  .thumb_set TCC0_INTREQ_3_IRQHandler,Dummy_Handler

  .weak TCC0_INTREQ_4_IRQHandler
  .thumb_set TCC0_INTREQ_4_IRQHandler,Dummy_Handler

  .weak TCC0_INTREQ_5_IRQHandler
  .thumb_set TCC0_INTREQ_5_IRQHandler,Dummy_Handler

  .weak TCC0_INTREQ_6_IRQHandler
  .thumb_set TCC0_INTREQ_6_IRQHandler,Dummy_Handler

  .weak TCC1_INTREQ_0_IRQHandler
  .thumb_set TCC1_INTREQ_0_IRQHandler,Dummy_Handler

  .weak TCC1_INTREQ_1_IRQHandler
  .thumb_set TCC1_INTREQ_1_IRQHandler,Dummy_Handler

  .weak TCC1_INTREQ_2_IRQHandler
  .thumb_set TCC1_INTREQ_2_IRQHandler,Dummy_Handler

  .weak TCC1_INTREQ_3_IRQHandler
  .thumb_set TCC1_INTREQ_3_IRQHandler,Dummy_Handler

  .weak TCC1_INTREQ_4_IRQHandler
  .thumb_set TCC1_INTREQ_4_IRQHandler,Dummy_Handler

  .weak TCC2_INTREQ_0_IRQHandler
  .thumb_set TCC2_INTREQ_0_IRQHandler,Dummy_Handler

  .weak TCC2_INTREQ_1_IRQHandler
  .thumb_set TCC2_INTREQ_1_IRQHandler,Dummy_Handler

  .weak TCC2_INTREQ_2_IRQHandler
  .thumb_set TCC2_INTREQ_2_IRQHandler,Dummy_Handler

  .weak TCC2_INTREQ_3_IRQHandler
  .thumb_set TCC2_INTREQ_3_IRQHandler,Dummy_Handler

  .weak TCC3_INTREQ_0_IRQHandler
  .thumb_set TCC3_INTREQ_0_IRQHandler,Dummy_Handler

  .weak TCC3_INTREQ_1_IRQHandler
  .thumb_set TCC3_INTREQ_1_IRQHandler,Dummy_Handler

  .weak TCC3_INTREQ_2_IRQHandler
  .thumb_set TCC3_INTREQ_2_IRQHandler,Dummy_Handler

  .weak TCC4_INTREQ_0_IRQHandler
  .thumb_set TCC4_INTREQ_0_IRQHandler,Dummy_Handler

  .weak TCC4_INTREQ_1_IRQHandler
  .thumb_set TCC4_INTREQ_1_IRQHandler,Dummy_Handler

  .weak TCC4_INTREQ_2_IRQHandler
  .thumb_set TCC4_INTREQ_2_IRQHandler,Dummy_Handler

  .weak TC0_INTREQ_IRQHandler
  .thumb_set TC0_INTREQ_IRQHandler,Dummy_Handler

  .weak TC1_INTREQ_IRQHandler
  .thumb_set TC1_INTREQ_IRQHandler,Dummy_Handler

  .weak TC2_INTREQ_IRQHandler
  .thumb_set TC2_INTREQ_IRQHandler,Dummy_Handler

  .weak TC3_INTREQ_IRQHandler
  .thumb_set TC3_INTREQ_IRQHandler,Dummy_Handler

  .weak TC4_INTREQ_IRQHandler
  .thumb_set TC4_INTREQ_IRQHandler,Dummy_Handler

  .weak TC5_INTREQ_IRQHandler
  .thumb_set TC5_INTREQ_IRQHandler,Dummy_Handler

  .weak PDEC_INTREQ_0_IRQHandler
  .thumb_set PDEC_INTREQ_0_IRQHandler,Dummy_Handler

  .weak PDEC_INTREQ_1_IRQHandler
  .thumb_set PDEC_INTREQ_1_IRQHandler,Dummy_Handler

  .weak PDEC_INTREQ_2_IRQHandler
  .thumb_set PDEC_INTREQ_2_IRQHandler,Dummy_Handler

  .weak ADC0_INTREQ_0_IRQHandler
  .thumb_set ADC0_INTREQ_0_IRQHandler,Dummy_Handler

  .weak ADC0_INTREQ_1_IRQHandler
  .thumb_set ADC0_INTREQ_1_IRQHandler,Dummy_Handler

  .weak ADC1_INTREQ_0_IRQHandler
  .thumb_set ADC1_INTREQ_0_IRQHandler,Dummy_Handler

  .weak ADC1_INTREQ_1_IRQHandler
  .thumb_set ADC1_INTREQ_1_IRQHandler,Dummy_Handler

  .weak AC_INTREQ_IRQHandler
  .thumb_set AC_INTREQ_IRQHandler,Dummy_Handler

  .weak DAC_INTREQ_0_IRQHandler
  .thumb_set DAC_INTREQ_0_IRQHandler,Dummy_Handler

  .weak DAC_INTREQ_1_IRQHandler
  .thumb_set DAC_INTREQ_1_IRQHandler,Dummy_Handler

  .weak DAC_INTREQ_2_IRQHandler
  .thumb_set DAC_INTREQ_2_IRQHandler,Dummy_Handler

  .weak DAC_INTREQ_3_IRQHandler
  .thumb_set DAC_INTREQ_3_IRQHandler,Dummy_Handler

  .weak DAC_INTREQ_4_IRQHandler
  .thumb_set DAC_INTREQ_4_IRQHandler,Dummy_Handler

  .weak I2S_INTREQ_IRQHandler
  .thumb_set I2S_INTREQ_IRQHandler,Dummy_Handler

  .weak PCC_INTREQ_IRQHandler
  .thumb_set PCC_INTREQ_IRQHandler,Dummy_Handler

  .weak AES_INTREQ_IRQHandler
  .thumb_set AES_INTREQ_IRQHandler,Dummy_Handler

  .weak TRNG_INTREQ_IRQHandler
  .thumb_set TRNG_INTREQ_IRQHandler,Dummy_Handler

  .weak ICM_INTREQ_IRQHandler
  .thumb_set ICM_INTREQ_IRQHandler,Dummy_Handler

  .weak QSPI_INTREQ_IRQHandler
  .thumb_set QSPI_INTREQ_IRQHandler,Dummy_Handler

  .weak SDHC0_INTREQ_IRQHandler
  .thumb_set SDHC0_INTREQ_IRQHandler,Dummy_Handler

#else

  .thumb_func
  .weak PM_INTREQ_IRQHandler
PM_INTREQ_IRQHandler:
  b .

  .thumb_func
  .weak MCLK_INTREQ_IRQHandler
MCLK_INTREQ_IRQHandler:
  b .

  .thumb_func
  .weak OSCCTRL_INTREQ_0_IRQHandler
OSCCTRL_INTREQ_0_IRQHandler:
  b .

  .thumb_func
  .weak OSCCTRL_INTREQ_1_IRQHandler
OSCCTRL_INTREQ_1_IRQHandler:
  b .

  .thumb_func
  .weak OSCCTRL_INTREQ_2_IRQHandler
OSCCTRL_INTREQ_2_IRQHandler:
  b .

  .thumb_func
  .weak OSCCTRL_INTREQ_3_IRQHandler
OSCCTRL_INTREQ_3_IRQHandler:
  b .

  .thumb_func
  .weak OSCCTRL_INTREQ_4_IRQHandler
OSCCTRL_INTREQ_4_IRQHandler:
  b .

  .thumb_func
  .weak OSC32KCTRL_INTREQ_IRQHandler
OSC32KCTRL_INTREQ_IRQHandler:
  b .

  .thumb_func
  .weak SUPC_INTREQ_0_IRQHandler
SUPC_INTREQ_0_IRQHandler:
  b .

  .thumb_func
  .weak SUPC_INTREQ_1_IRQHandler
SUPC_INTREQ_1_IRQHandler:
  b .

  .thumb_func
  .weak WDT_INTREQ_IRQHandler
WDT_INTREQ_IRQHandler:
  b .

  .thumb_func
  .weak RTC_INTREQ_IRQHandler
RTC_INTREQ_IRQHandler:
  b .

  .thumb_func
  .weak EIC_INTREQ_0_IRQHandler
EIC_INTREQ_0_IRQHandler:
  b .

  .thumb_func
  .weak EIC_INTREQ_1_IRQHandler
EIC_INTREQ_1_IRQHandler:
  b .

  .thumb_func
  .weak EIC_INTREQ_2_IRQHandler
EIC_INTREQ_2_IRQHandler:
  b .

  .thumb_func
  .weak EIC_INTREQ_3_IRQHandler
EIC_INTREQ_3_IRQHandler:
  b .

  .thumb_func
  .weak EIC_INTREQ_4_IRQHandler
EIC_INTREQ_4_IRQHandler:
  b .

  .thumb_func
  .weak EIC_INTREQ_5_IRQHandler
EIC_INTREQ_5_IRQHandler:
  b .

  .thumb_func
  .weak EIC_INTREQ_6_IRQHandler
EIC_INTREQ_6_IRQHandler:
  b .

  .thumb_func
  .weak EIC_INTREQ_7_IRQHandler
EIC_INTREQ_7_IRQHandler:
  b .

  .thumb_func
  .weak EIC_INTREQ_8_IRQHandler
EIC_INTREQ_8_IRQHandler:
  b .

  .thumb_func
  .weak EIC_INTREQ_9_IRQHandler
EIC_INTREQ_9_IRQHandler:
  b .

  .thumb_func
  .weak EIC_INTREQ_10_IRQHandler
EIC_INTREQ_10_IRQHandler:
  b .

  .thumb_func
  .weak EIC_INTREQ_11_IRQHandler
EIC_INTREQ_11_IRQHandler:
  b .

  .thumb_func
  .weak EIC_INTREQ_12_IRQHandler
EIC_INTREQ_12_IRQHandler:
  b .

  .thumb_func
  .weak EIC_INTREQ_13_IRQHandler
EIC_INTREQ_13_IRQHandler:
  b .

  .thumb_func
  .weak EIC_INTREQ_14_IRQHandler
EIC_INTREQ_14_IRQHandler:
  b .

  .thumb_func
  .weak EIC_INTREQ_15_IRQHandler
EIC_INTREQ_15_IRQHandler:
  b .

  .thumb_func
  .weak FREQM_INTREQ_IRQHandler
FREQM_INTREQ_IRQHandler:
  b .

  .thumb_func
  .weak NVMCTRL_INTREQ_0_IRQHandler
NVMCTRL_INTREQ_0_IRQHandler:
  b .

  .thumb_func
  .weak NVMCTRL_INTREQ_1_IRQHandler
NVMCTRL_INTREQ_1_IRQHandler:
  b .

  .thumb_func
  .weak DMAC_INTREQ_0_IRQHandler
DMAC_INTREQ_0_IRQHandler:
  b .

  .thumb_func
  .weak DMAC_INTREQ_1_IRQHandler
DMAC_INTREQ_1_IRQHandler:
  b .

  .thumb_func
  .weak DMAC_INTREQ_2_IRQHandler
DMAC_INTREQ_2_IRQHandler:
  b .

  .thumb_func
  .weak DMAC_INTREQ_3_IRQHandler
DMAC_INTREQ_3_IRQHandler:
  b .

  .thumb_func
  .weak DMAC_INTREQ_4_IRQHandler
DMAC_INTREQ_4_IRQHandler:
  b .

  .thumb_func
  .weak EVSYS_INTREQ_0_IRQHandler
EVSYS_INTREQ_0_IRQHandler:
  b .

  .thumb_func
  .weak EVSYS_INTREQ_1_IRQHandler
EVSYS_INTREQ_1_IRQHandler:
  b .

  .thumb_func
  .weak EVSYS_INTREQ_2_IRQHandler
EVSYS_INTREQ_2_IRQHandler:
  b .

  .thumb_func
  .weak EVSYS_INTREQ_3_IRQHandler
EVSYS_INTREQ_3_IRQHandler:
  b .

  .thumb_func
  .weak EVSYS_INTREQ_4_IRQHandler
EVSYS_INTREQ_4_IRQHandler:
  b .

  .thumb_func
  .weak PAC_INTREQ_IRQHandler
PAC_INTREQ_IRQHandler:
  b .

  .thumb_func
  .weak TAL_INTREQ_0_IRQHandler
TAL_INTREQ_0_IRQHandler:
  b .

  .thumb_func
  .weak TAL_INTREQ_1_IRQHandler
TAL_INTREQ_1_IRQHandler:
  b .

  .thumb_func
  .weak RAMECC_INTREQ_IRQHandler
RAMECC_INTREQ_IRQHandler:
  b .

  .thumb_func
  .weak SERCOM0_INTREQ_0_IRQHandler
SERCOM0_INTREQ_0_IRQHandler:
  b .

  .thumb_func
  .weak SERCOM0_INTREQ_1_IRQHandler
SERCOM0_INTREQ_1_IRQHandler:
  b .

  .thumb_func
  .weak SERCOM0_INTREQ_2_IRQHandler
SERCOM0_INTREQ_2_IRQHandler:
  b .

  .thumb_func
  .weak SERCOM0_INTREQ_3_IRQHandler
SERCOM0_INTREQ_3_IRQHandler:
  b .

  .thumb_func
  .weak SERCOM1_INTREQ_0_IRQHandler
SERCOM1_INTREQ_0_IRQHandler:
  b .

  .thumb_func
  .weak SERCOM1_INTREQ_1_IRQHandler
SERCOM1_INTREQ_1_IRQHandler:
  b .

  .thumb_func
  .weak SERCOM1_INTREQ_2_IRQHandler
SERCOM1_INTREQ_2_IRQHandler:
  b .

  .thumb_func
  .weak SERCOM1_INTREQ_3_IRQHandler
SERCOM1_INTREQ_3_IRQHandler:
  b .

  .thumb_func
  .weak SERCOM2_INTREQ_0_IRQHandler
SERCOM2_INTREQ_0_IRQHandler:
  b .

  .thumb_func
  .weak SERCOM2_INTREQ_1_IRQHandler
SERCOM2_INTREQ_1_IRQHandler:
  b .

  .thumb_func
  .weak SERCOM2_INTREQ_2_IRQHandler
SERCOM2_INTREQ_2_IRQHandler:
  b .

  .thumb_func
  .weak SERCOM2_INTREQ_3_IRQHandler
SERCOM2_INTREQ_3_IRQHandler:
  b .

  .thumb_func
  .weak SERCOM3_INTREQ_0_IRQHandler
SERCOM3_INTREQ_0_IRQHandler:
  b .

  .thumb_func
  .weak SERCOM3_INTREQ_1_IRQHandler
SERCOM3_INTREQ_1_IRQHandler:
  b .

  .thumb_func
  .weak SERCOM3_INTREQ_2_IRQHandler
SERCOM3_INTREQ_2_IRQHandler:
  b .

  .thumb_func
  .weak SERCOM3_INTREQ_3_IRQHandler
SERCOM3_INTREQ_3_IRQHandler:
  b .

  .thumb_func
  .weak SERCOM4_INTREQ_0_IRQHandler
SERCOM4_INTREQ_0_IRQHandler:
  b .

  .thumb_func
  .weak SERCOM4_INTREQ_1_IRQHandler
SERCOM4_INTREQ_1_IRQHandler:
  b .

  .thumb_func
  .weak SERCOM4_INTREQ_2_IRQHandler
SERCOM4_INTREQ_2_IRQHandler:
  b .

  .thumb_func
  .weak SERCOM4_INTREQ_3_IRQHandler
SERCOM4_INTREQ_3_IRQHandler:
  b .

  .thumb_func
  .weak SERCOM5_INTREQ_0_IRQHandler
SERCOM5_INTREQ_0_IRQHandler:
  b .

  .thumb_func
  .weak SERCOM5_INTREQ_1_IRQHandler
SERCOM5_INTREQ_1_IRQHandler:
  b .

  .thumb_func
  .weak SERCOM5_INTREQ_2_IRQHandler
SERCOM5_INTREQ_2_IRQHandler:
  b .

  .thumb_func
  .weak SERCOM5_INTREQ_3_IRQHandler
SERCOM5_INTREQ_3_IRQHandler:
  b .

  .thumb_func
  .weak CAN0_INTREQ_IRQHandler
CAN0_INTREQ_IRQHandler:
  b .

  .thumb_func
  .weak CAN1_INTREQ_IRQHandler
CAN1_INTREQ_IRQHandler:
  b .

  .thumb_func
  .weak USB_INTREQ_0_IRQHandler
USB_INTREQ_0_IRQHandler:
  b .

  .thumb_func
  .weak USB_INTREQ_1_IRQHandler
USB_INTREQ_1_IRQHandler:
  b .

  .thumb_func
  .weak USB_INTREQ_2_IRQHandler
USB_INTREQ_2_IRQHandler:
  b .

  .thumb_func
  .weak USB_INTREQ_3_IRQHandler
USB_INTREQ_3_IRQHandler:
  b .

  .thumb_func
  .weak TCC0_INTREQ_0_IRQHandler
TCC0_INTREQ_0_IRQHandler:
  b .

  .thumb_func
  .weak TCC0_INTREQ_1_IRQHandler
TCC0_INTREQ_1_IRQHandler:
  b .

  .thumb_func
  .weak TCC0_INTREQ_2_IRQHandler
TCC0_INTREQ_2_IRQHandler:
  b .

  .thumb_func
  .weak TCC0_INTREQ_3_IRQHandler
TCC0_INTREQ_3_IRQHandler:
  b .

  .thumb_func
  .weak TCC0_INTREQ_4_IRQHandler
TCC0_INTREQ_4_IRQHandler:
  b .

  .thumb_func
  .weak TCC0_INTREQ_5_IRQHandler
TCC0_INTREQ_5_IRQHandler:
  b .

  .thumb_func
  .weak TCC0_INTREQ_6_IRQHandler
TCC0_INTREQ_6_IRQHandler:
  b .

  .thumb_func
  .weak TCC1_INTREQ_0_IRQHandler
TCC1_INTREQ_0_IRQHandler:
  b .

  .thumb_func
  .weak TCC1_INTREQ_1_IRQHandler
TCC1_INTREQ_1_IRQHandler:
  b .

  .thumb_func
  .weak TCC1_INTREQ_2_IRQHandler
TCC1_INTREQ_2_IRQHandler:
  b .

  .thumb_func
  .weak TCC1_INTREQ_3_IRQHandler
TCC1_INTREQ_3_IRQHandler:
  b .

  .thumb_func
  .weak TCC1_INTREQ_4_IRQHandler
TCC1_INTREQ_4_IRQHandler:
  b .

  .thumb_func
  .weak TCC2_INTREQ_0_IRQHandler
TCC2_INTREQ_0_IRQHandler:
  b .

  .thumb_func
  .weak TCC2_INTREQ_1_IRQHandler
TCC2_INTREQ_1_IRQHandler:
  b .

  .thumb_func
  .weak TCC2_INTREQ_2_IRQHandler
TCC2_INTREQ_2_IRQHandler:
  b .

  .thumb_func
  .weak TCC2_INTREQ_3_IRQHandler
TCC2_INTREQ_3_IRQHandler:
  b .

  .thumb_func
  .weak TCC3_INTREQ_0_IRQHandler
TCC3_INTREQ_0_IRQHandler:
  b .

  .thumb_func
  .weak TCC3_INTREQ_1_IRQHandler
TCC3_INTREQ_1_IRQHandler:
  b .

  .thumb_func
  .weak TCC3_INTREQ_2_IRQHandler
TCC3_INTREQ_2_IRQHandler:
  b .

  .thumb_func
  .weak TCC4_INTREQ_0_IRQHandler
TCC4_INTREQ_0_IRQHandler:
  b .

  .thumb_func
  .weak TCC4_INTREQ_1_IRQHandler
TCC4_INTREQ_1_IRQHandler:
  b .

  .thumb_func
  .weak TCC4_INTREQ_2_IRQHandler
TCC4_INTREQ_2_IRQHandler:
  b .

  .thumb_func
  .weak TC0_INTREQ_IRQHandler
TC0_INTREQ_IRQHandler:
  b .

  .thumb_func
  .weak TC1_INTREQ_IRQHandler
TC1_INTREQ_IRQHandler:
  b .

  .thumb_func
  .weak TC2_INTREQ_IRQHandler
TC2_INTREQ_IRQHandler:
  b .

  .thumb_func
  .weak TC3_INTREQ_IRQHandler
TC3_INTREQ_IRQHandler:
  b .

  .thumb_func
  .weak TC4_INTREQ_IRQHandler
TC4_INTREQ_IRQHandler:
  b .

  .thumb_func
  .weak TC5_INTREQ_IRQHandler
TC5_INTREQ_IRQHandler:
  b .

  .thumb_func
  .weak PDEC_INTREQ_0_IRQHandler
PDEC_INTREQ_0_IRQHandler:
  b .

  .thumb_func
  .weak PDEC_INTREQ_1_IRQHandler
PDEC_INTREQ_1_IRQHandler:
  b .

  .thumb_func
  .weak PDEC_INTREQ_2_IRQHandler
PDEC_INTREQ_2_IRQHandler:
  b .

  .thumb_func
  .weak ADC0_INTREQ_0_IRQHandler
ADC0_INTREQ_0_IRQHandler:
  b .

  .thumb_func
  .weak ADC0_INTREQ_1_IRQHandler
ADC0_INTREQ_1_IRQHandler:
  b .

  .thumb_func
  .weak ADC1_INTREQ_0_IRQHandler
ADC1_INTREQ_0_IRQHandler:
  b .

  .thumb_func
  .weak ADC1_INTREQ_1_IRQHandler
ADC1_INTREQ_1_IRQHandler:
  b .

  .thumb_func
  .weak AC_INTREQ_IRQHandler
AC_INTREQ_IRQHandler:
  b .

  .thumb_func
  .weak DAC_INTREQ_0_IRQHandler
DAC_INTREQ_0_IRQHandler:
  b .

  .thumb_func
  .weak DAC_INTREQ_1_IRQHandler
DAC_INTREQ_1_IRQHandler:
  b .

  .thumb_func
  .weak DAC_INTREQ_2_IRQHandler
DAC_INTREQ_2_IRQHandler:
  b .

  .thumb_func
  .weak DAC_INTREQ_3_IRQHandler
DAC_INTREQ_3_IRQHandler:
  b .

  .thumb_func
  .weak DAC_INTREQ_4_IRQHandler
DAC_INTREQ_4_IRQHandler:
  b .

  .thumb_func
  .weak I2S_INTREQ_IRQHandler
I2S_INTREQ_IRQHandler:
  b .

  .thumb_func
  .weak PCC_INTREQ_IRQHandler
PCC_INTREQ_IRQHandler:
  b .

  .thumb_func
  .weak AES_INTREQ_IRQHandler
AES_INTREQ_IRQHandler:
  b .

  .thumb_func
  .weak TRNG_INTREQ_IRQHandler
TRNG_INTREQ_IRQHandler:
  b .

  .thumb_func
  .weak ICM_INTREQ_IRQHandler
ICM_INTREQ_IRQHandler:
  b .

  .thumb_func
  .weak QSPI_INTREQ_IRQHandler
QSPI_INTREQ_IRQHandler:
  b .

  .thumb_func
  .weak SDHC0_INTREQ_IRQHandler
SDHC0_INTREQ_IRQHandler:
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
  .word PM_INTREQ_IRQHandler
  .word MCLK_INTREQ_IRQHandler
  .word OSCCTRL_INTREQ_0_IRQHandler
  .word OSCCTRL_INTREQ_1_IRQHandler
  .word OSCCTRL_INTREQ_2_IRQHandler
  .word OSCCTRL_INTREQ_3_IRQHandler
  .word OSCCTRL_INTREQ_4_IRQHandler
  .word OSC32KCTRL_INTREQ_IRQHandler
  .word SUPC_INTREQ_0_IRQHandler
  .word SUPC_INTREQ_1_IRQHandler
  .word WDT_INTREQ_IRQHandler
  .word RTC_INTREQ_IRQHandler
  .word EIC_INTREQ_0_IRQHandler
  .word EIC_INTREQ_1_IRQHandler
  .word EIC_INTREQ_2_IRQHandler
  .word EIC_INTREQ_3_IRQHandler
  .word EIC_INTREQ_4_IRQHandler
  .word EIC_INTREQ_5_IRQHandler
  .word EIC_INTREQ_6_IRQHandler
  .word EIC_INTREQ_7_IRQHandler
  .word EIC_INTREQ_8_IRQHandler
  .word EIC_INTREQ_9_IRQHandler
  .word EIC_INTREQ_10_IRQHandler
  .word EIC_INTREQ_11_IRQHandler
  .word EIC_INTREQ_12_IRQHandler
  .word EIC_INTREQ_13_IRQHandler
  .word EIC_INTREQ_14_IRQHandler
  .word EIC_INTREQ_15_IRQHandler
  .word FREQM_INTREQ_IRQHandler
  .word NVMCTRL_INTREQ_0_IRQHandler
  .word NVMCTRL_INTREQ_1_IRQHandler
  .word DMAC_INTREQ_0_IRQHandler
  .word DMAC_INTREQ_1_IRQHandler
  .word DMAC_INTREQ_2_IRQHandler
  .word DMAC_INTREQ_3_IRQHandler
  .word DMAC_INTREQ_4_IRQHandler
  .word EVSYS_INTREQ_0_IRQHandler
  .word EVSYS_INTREQ_1_IRQHandler
  .word EVSYS_INTREQ_2_IRQHandler
  .word EVSYS_INTREQ_3_IRQHandler
  .word EVSYS_INTREQ_4_IRQHandler
  .word PAC_INTREQ_IRQHandler
  .word TAL_INTREQ_0_IRQHandler
  .word TAL_INTREQ_1_IRQHandler
  .word Dummy_Handler /* Reserved */
  .word RAMECC_INTREQ_IRQHandler
  .word SERCOM0_INTREQ_0_IRQHandler
  .word SERCOM0_INTREQ_1_IRQHandler
  .word SERCOM0_INTREQ_2_IRQHandler
  .word SERCOM0_INTREQ_3_IRQHandler
  .word SERCOM1_INTREQ_0_IRQHandler
  .word SERCOM1_INTREQ_1_IRQHandler
  .word SERCOM1_INTREQ_2_IRQHandler
  .word SERCOM1_INTREQ_3_IRQHandler
  .word SERCOM2_INTREQ_0_IRQHandler
  .word SERCOM2_INTREQ_1_IRQHandler
  .word SERCOM2_INTREQ_2_IRQHandler
  .word SERCOM2_INTREQ_3_IRQHandler
  .word SERCOM3_INTREQ_0_IRQHandler
  .word SERCOM3_INTREQ_1_IRQHandler
  .word SERCOM3_INTREQ_2_IRQHandler
  .word SERCOM3_INTREQ_3_IRQHandler
  .word SERCOM4_INTREQ_0_IRQHandler
  .word SERCOM4_INTREQ_1_IRQHandler
  .word SERCOM4_INTREQ_2_IRQHandler
  .word SERCOM4_INTREQ_3_IRQHandler
  .word SERCOM5_INTREQ_0_IRQHandler
  .word SERCOM5_INTREQ_1_IRQHandler
  .word SERCOM5_INTREQ_2_IRQHandler
  .word SERCOM5_INTREQ_3_IRQHandler
  .word Dummy_Handler /* Reserved */
  .word Dummy_Handler /* Reserved */
  .word Dummy_Handler /* Reserved */
  .word Dummy_Handler /* Reserved */
  .word Dummy_Handler /* Reserved */
  .word Dummy_Handler /* Reserved */
  .word Dummy_Handler /* Reserved */
  .word Dummy_Handler /* Reserved */
  .word CAN0_INTREQ_IRQHandler
  .word CAN1_INTREQ_IRQHandler
  .word USB_INTREQ_0_IRQHandler
  .word USB_INTREQ_1_IRQHandler
  .word USB_INTREQ_2_IRQHandler
  .word USB_INTREQ_3_IRQHandler
  .word Dummy_Handler /* Reserved */
  .word TCC0_INTREQ_0_IRQHandler
  .word TCC0_INTREQ_1_IRQHandler
  .word TCC0_INTREQ_2_IRQHandler
  .word TCC0_INTREQ_3_IRQHandler
  .word TCC0_INTREQ_4_IRQHandler
  .word TCC0_INTREQ_5_IRQHandler
  .word TCC0_INTREQ_6_IRQHandler
  .word TCC1_INTREQ_0_IRQHandler
  .word TCC1_INTREQ_1_IRQHandler
  .word TCC1_INTREQ_2_IRQHandler
  .word TCC1_INTREQ_3_IRQHandler
  .word TCC1_INTREQ_4_IRQHandler
  .word TCC2_INTREQ_0_IRQHandler
  .word TCC2_INTREQ_1_IRQHandler
  .word TCC2_INTREQ_2_IRQHandler
  .word TCC2_INTREQ_3_IRQHandler
  .word TCC3_INTREQ_0_IRQHandler
  .word TCC3_INTREQ_1_IRQHandler
  .word TCC3_INTREQ_2_IRQHandler
  .word TCC4_INTREQ_0_IRQHandler
  .word TCC4_INTREQ_1_IRQHandler
  .word TCC4_INTREQ_2_IRQHandler
  .word TC0_INTREQ_IRQHandler
  .word TC1_INTREQ_IRQHandler
  .word TC2_INTREQ_IRQHandler
  .word TC3_INTREQ_IRQHandler
  .word TC4_INTREQ_IRQHandler
  .word TC5_INTREQ_IRQHandler
  .word Dummy_Handler /* Reserved */
  .word Dummy_Handler /* Reserved */
  .word PDEC_INTREQ_0_IRQHandler
  .word PDEC_INTREQ_1_IRQHandler
  .word PDEC_INTREQ_2_IRQHandler
  .word ADC0_INTREQ_0_IRQHandler
  .word ADC0_INTREQ_1_IRQHandler
  .word ADC1_INTREQ_0_IRQHandler
  .word ADC1_INTREQ_1_IRQHandler
  .word AC_INTREQ_IRQHandler
  .word DAC_INTREQ_0_IRQHandler
  .word DAC_INTREQ_1_IRQHandler
  .word DAC_INTREQ_2_IRQHandler
  .word DAC_INTREQ_3_IRQHandler
  .word DAC_INTREQ_4_IRQHandler
  .word I2S_INTREQ_IRQHandler
  .word PCC_INTREQ_IRQHandler
  .word AES_INTREQ_IRQHandler
  .word TRNG_INTREQ_IRQHandler
  .word ICM_INTREQ_IRQHandler
  .word Dummy_Handler /* Reserved */
  .word QSPI_INTREQ_IRQHandler
  .word SDHC0_INTREQ_IRQHandler
_vectors_end:

#ifdef VECTORS_IN_RAM
  .section .vectors_ram, "ax"
  .align 0
  .global _vectors_ram

_vectors_ram:
  .space _vectors_end - _vectors, 0
#endif
