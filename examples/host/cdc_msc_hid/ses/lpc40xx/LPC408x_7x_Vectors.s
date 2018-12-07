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

  .weak WWDT_IRQHandler
  .thumb_set WWDT_IRQHandler,Dummy_Handler

  .weak TIMER0_IRQHandler
  .thumb_set TIMER0_IRQHandler,Dummy_Handler

  .weak TIMER1_IRQHandler
  .thumb_set TIMER1_IRQHandler,Dummy_Handler

  .weak TIMER2_IRQHandler
  .thumb_set TIMER2_IRQHandler,Dummy_Handler

  .weak TIMER3_IRQHandler
  .thumb_set TIMER3_IRQHandler,Dummy_Handler

  .weak UART0_IRQHandler
  .thumb_set UART0_IRQHandler,Dummy_Handler

  .weak UART1_IRQHandler
  .thumb_set UART1_IRQHandler,Dummy_Handler

  .weak UART2_IRQHandler
  .thumb_set UART2_IRQHandler,Dummy_Handler

  .weak UART3_IRQHandler
  .thumb_set UART3_IRQHandler,Dummy_Handler

  .weak PWM1_IRQHandler
  .thumb_set PWM1_IRQHandler,Dummy_Handler

  .weak I2C0_IRQHandler
  .thumb_set I2C0_IRQHandler,Dummy_Handler

  .weak I2C1_IRQHandler
  .thumb_set I2C1_IRQHandler,Dummy_Handler

  .weak I2C2_IRQHandler
  .thumb_set I2C2_IRQHandler,Dummy_Handler

  .weak SSP0_IRQHandler
  .thumb_set SSP0_IRQHandler,Dummy_Handler

  .weak SSP1_IRQHandler
  .thumb_set SSP1_IRQHandler,Dummy_Handler

  .weak RTC_IRQHandler
  .thumb_set RTC_IRQHandler,Dummy_Handler

  .weak EINT0_IRQHandler
  .thumb_set EINT0_IRQHandler,Dummy_Handler

  .weak EINT1_IRQHandler
  .thumb_set EINT1_IRQHandler,Dummy_Handler

  .weak EINT2_IRQHandler
  .thumb_set EINT2_IRQHandler,Dummy_Handler

  .weak EINT3_IRQHandler
  .thumb_set EINT3_IRQHandler,Dummy_Handler

  .weak ADC_IRQHandler
  .thumb_set ADC_IRQHandler,Dummy_Handler

  .weak BOD_IRQHandler
  .thumb_set BOD_IRQHandler,Dummy_Handler

  .weak USB_IRQHandler
  .thumb_set USB_IRQHandler,Dummy_Handler

  .weak CAN_IRQHandler
  .thumb_set CAN_IRQHandler,Dummy_Handler

  .weak GPDMA_IRQHandler
  .thumb_set GPDMA_IRQHandler,Dummy_Handler

  .weak I2S_IRQHandler
  .thumb_set I2S_IRQHandler,Dummy_Handler

  .weak ETHERNET_IRQHandler
  .thumb_set ETHERNET_IRQHandler,Dummy_Handler

  .weak SDMMC_IRQHandler
  .thumb_set SDMMC_IRQHandler,Dummy_Handler

  .weak MCPWM_IRQHandler
  .thumb_set MCPWM_IRQHandler,Dummy_Handler

  .weak QEI_IRQHandler
  .thumb_set QEI_IRQHandler,Dummy_Handler

  .weak USB_NEED_CLK_IRQHandler
  .thumb_set USB_NEED_CLK_IRQHandler,Dummy_Handler

  .weak UART4_IRQHandler
  .thumb_set UART4_IRQHandler,Dummy_Handler

  .weak SSP2_IRQHandler
  .thumb_set SSP2_IRQHandler,Dummy_Handler

  .weak LCD_IRQHandler
  .thumb_set LCD_IRQHandler,Dummy_Handler

  .weak GPIOINT_IRQHandler
  .thumb_set GPIOINT_IRQHandler,Dummy_Handler

  .weak PWM0_IRQHandler
  .thumb_set PWM0_IRQHandler,Dummy_Handler

  .weak EEPROM_IRQHandler
  .thumb_set EEPROM_IRQHandler,Dummy_Handler

  .weak CMP0_IRQHandler
  .thumb_set CMP0_IRQHandler,Dummy_Handler

  .weak CMP1_IRQHandler
  .thumb_set CMP1_IRQHandler,Dummy_Handler

#else

  .thumb_func
  .weak WWDT_IRQHandler
WWDT_IRQHandler:
  b .

  .thumb_func
  .weak TIMER0_IRQHandler
TIMER0_IRQHandler:
  b .

  .thumb_func
  .weak TIMER1_IRQHandler
TIMER1_IRQHandler:
  b .

  .thumb_func
  .weak TIMER2_IRQHandler
TIMER2_IRQHandler:
  b .

  .thumb_func
  .weak TIMER3_IRQHandler
TIMER3_IRQHandler:
  b .

  .thumb_func
  .weak UART0_IRQHandler
UART0_IRQHandler:
  b .

  .thumb_func
  .weak UART1_IRQHandler
UART1_IRQHandler:
  b .

  .thumb_func
  .weak UART2_IRQHandler
UART2_IRQHandler:
  b .

  .thumb_func
  .weak UART3_IRQHandler
UART3_IRQHandler:
  b .

  .thumb_func
  .weak PWM1_IRQHandler
PWM1_IRQHandler:
  b .

  .thumb_func
  .weak I2C0_IRQHandler
I2C0_IRQHandler:
  b .

  .thumb_func
  .weak I2C1_IRQHandler
I2C1_IRQHandler:
  b .

  .thumb_func
  .weak I2C2_IRQHandler
I2C2_IRQHandler:
  b .

  .thumb_func
  .weak SSP0_IRQHandler
SSP0_IRQHandler:
  b .

  .thumb_func
  .weak SSP1_IRQHandler
SSP1_IRQHandler:
  b .

  .thumb_func
  .weak RTC_IRQHandler
RTC_IRQHandler:
  b .

  .thumb_func
  .weak EINT0_IRQHandler
EINT0_IRQHandler:
  b .

  .thumb_func
  .weak EINT1_IRQHandler
EINT1_IRQHandler:
  b .

  .thumb_func
  .weak EINT2_IRQHandler
EINT2_IRQHandler:
  b .

  .thumb_func
  .weak EINT3_IRQHandler
EINT3_IRQHandler:
  b .

  .thumb_func
  .weak ADC_IRQHandler
ADC_IRQHandler:
  b .

  .thumb_func
  .weak BOD_IRQHandler
BOD_IRQHandler:
  b .

  .thumb_func
  .weak USB_IRQHandler
USB_IRQHandler:
  b .

  .thumb_func
  .weak CAN_IRQHandler
CAN_IRQHandler:
  b .

  .thumb_func
  .weak GPDMA_IRQHandler
GPDMA_IRQHandler:
  b .

  .thumb_func
  .weak I2S_IRQHandler
I2S_IRQHandler:
  b .

  .thumb_func
  .weak ETHERNET_IRQHandler
ETHERNET_IRQHandler:
  b .

  .thumb_func
  .weak SDMMC_IRQHandler
SDMMC_IRQHandler:
  b .

  .thumb_func
  .weak MCPWM_IRQHandler
MCPWM_IRQHandler:
  b .

  .thumb_func
  .weak QEI_IRQHandler
QEI_IRQHandler:
  b .

  .thumb_func
  .weak USB_NEED_CLK_IRQHandler
USB_NEED_CLK_IRQHandler:
  b .

  .thumb_func
  .weak UART4_IRQHandler
UART4_IRQHandler:
  b .

  .thumb_func
  .weak SSP2_IRQHandler
SSP2_IRQHandler:
  b .

  .thumb_func
  .weak LCD_IRQHandler
LCD_IRQHandler:
  b .

  .thumb_func
  .weak GPIOINT_IRQHandler
GPIOINT_IRQHandler:
  b .

  .thumb_func
  .weak PWM0_IRQHandler
PWM0_IRQHandler:
  b .

  .thumb_func
  .weak EEPROM_IRQHandler
EEPROM_IRQHandler:
  b .

  .thumb_func
  .weak CMP0_IRQHandler
CMP0_IRQHandler:
  b .

  .thumb_func
  .weak CMP1_IRQHandler
CMP1_IRQHandler:
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
  .word WWDT_IRQHandler
  .word TIMER0_IRQHandler
  .word TIMER1_IRQHandler
  .word TIMER2_IRQHandler
  .word TIMER3_IRQHandler
  .word UART0_IRQHandler
  .word UART1_IRQHandler
  .word UART2_IRQHandler
  .word UART3_IRQHandler
  .word PWM1_IRQHandler
  .word I2C0_IRQHandler
  .word I2C1_IRQHandler
  .word I2C2_IRQHandler
  .word Dummy_Handler /* Reserved */
  .word SSP0_IRQHandler
  .word SSP1_IRQHandler
  .word Dummy_Handler /* Reserved */
  .word RTC_IRQHandler
  .word EINT0_IRQHandler
  .word EINT1_IRQHandler
  .word EINT2_IRQHandler
  .word EINT3_IRQHandler
  .word ADC_IRQHandler
  .word BOD_IRQHandler
  .word USB_IRQHandler
  .word CAN_IRQHandler
  .word GPDMA_IRQHandler
  .word I2S_IRQHandler
  .word ETHERNET_IRQHandler
  .word SDMMC_IRQHandler
  .word MCPWM_IRQHandler
  .word QEI_IRQHandler
  .word Dummy_Handler /* Reserved */
  .word USB_NEED_CLK_IRQHandler
  .word Dummy_Handler /* Reserved */
  .word UART4_IRQHandler
  .word SSP2_IRQHandler
  .word LCD_IRQHandler
  .word GPIOINT_IRQHandler
  .word PWM0_IRQHandler
  .word EEPROM_IRQHandler
  .word CMP0_IRQHandler
  .word CMP1_IRQHandler
_vectors_end:

#ifdef VECTORS_IN_RAM
  .section .vectors_ram, "ax"
  .align 0
  .global _vectors_ram

_vectors_ram:
  .space _vectors_end - _vectors, 0
#endif
