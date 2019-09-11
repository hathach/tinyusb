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

  .weak DAC_IRQHandler
  .thumb_set DAC_IRQHandler,Dummy_Handler

  .weak DMA_IRQHandler
  .thumb_set DMA_IRQHandler,Dummy_Handler

  .weak FLASH_IRQHandler
  .thumb_set FLASH_IRQHandler,Dummy_Handler

  .weak ETHERNET_IRQHandler
  .thumb_set ETHERNET_IRQHandler,Dummy_Handler

  .weak SDIO_IRQHandler
  .thumb_set SDIO_IRQHandler,Dummy_Handler

  .weak LCD_IRQHandler
  .thumb_set LCD_IRQHandler,Dummy_Handler

  .weak USB0_IRQHandler
  .thumb_set USB0_IRQHandler,Dummy_Handler

  .weak USB1_IRQHandler
  .thumb_set USB1_IRQHandler,Dummy_Handler

  .weak SCT_IRQHandler
  .thumb_set SCT_IRQHandler,Dummy_Handler

  .weak RITIMER_IRQHandler
  .thumb_set RITIMER_IRQHandler,Dummy_Handler

  .weak TIMER0_IRQHandler
  .thumb_set TIMER0_IRQHandler,Dummy_Handler

  .weak TIMER1_IRQHandler
  .thumb_set TIMER1_IRQHandler,Dummy_Handler

  .weak TIMER2_IRQHandler
  .thumb_set TIMER2_IRQHandler,Dummy_Handler

  .weak TIMER3_IRQHandler
  .thumb_set TIMER3_IRQHandler,Dummy_Handler

  .weak MCPWM_IRQHandler
  .thumb_set MCPWM_IRQHandler,Dummy_Handler

  .weak ADC0_IRQHandler
  .thumb_set ADC0_IRQHandler,Dummy_Handler

  .weak I2C0_IRQHandler
  .thumb_set I2C0_IRQHandler,Dummy_Handler

  .weak I2C1_IRQHandler
  .thumb_set I2C1_IRQHandler,Dummy_Handler

  .weak ADC1_IRQHandler
  .thumb_set ADC1_IRQHandler,Dummy_Handler

  .weak SSP0_IRQHandler
  .thumb_set SSP0_IRQHandler,Dummy_Handler

  .weak SSP1_IRQHandler
  .thumb_set SSP1_IRQHandler,Dummy_Handler

  .weak USART0_IRQHandler
  .thumb_set USART0_IRQHandler,Dummy_Handler

  .weak UART1_IRQHandler
  .thumb_set UART1_IRQHandler,Dummy_Handler

  .weak USART2_IRQHandler
  .thumb_set USART2_IRQHandler,Dummy_Handler

  .weak USART3_IRQHandler
  .thumb_set USART3_IRQHandler,Dummy_Handler

  .weak I2S0_IRQHandler
  .thumb_set I2S0_IRQHandler,Dummy_Handler

  .weak I2S1_IRQHandler
  .thumb_set I2S1_IRQHandler,Dummy_Handler

  .weak SPIFI_IRQHandler
  .thumb_set SPIFI_IRQHandler,Dummy_Handler

  .weak PIN_INT0_IRQHandler
  .thumb_set PIN_INT0_IRQHandler,Dummy_Handler

  .weak PIN_INT1_IRQHandler
  .thumb_set PIN_INT1_IRQHandler,Dummy_Handler

  .weak PIN_INT2_IRQHandler
  .thumb_set PIN_INT2_IRQHandler,Dummy_Handler

  .weak PIN_INT3_IRQHandler
  .thumb_set PIN_INT3_IRQHandler,Dummy_Handler

  .weak PIN_INT4_IRQHandler
  .thumb_set PIN_INT4_IRQHandler,Dummy_Handler

  .weak PIN_INT5_IRQHandler
  .thumb_set PIN_INT5_IRQHandler,Dummy_Handler

  .weak PIN_INT6_IRQHandler
  .thumb_set PIN_INT6_IRQHandler,Dummy_Handler

  .weak PIN_INT7_IRQHandler
  .thumb_set PIN_INT7_IRQHandler,Dummy_Handler

  .weak GINT0_IRQHandler
  .thumb_set GINT0_IRQHandler,Dummy_Handler

  .weak GINT1_IRQHandler
  .thumb_set GINT1_IRQHandler,Dummy_Handler

  .weak EVENTROUTER_IRQHandler
  .thumb_set EVENTROUTER_IRQHandler,Dummy_Handler

  .weak C_CAN1_IRQHandler
  .thumb_set C_CAN1_IRQHandler,Dummy_Handler

  .weak ATIMER_IRQHandler
  .thumb_set ATIMER_IRQHandler,Dummy_Handler

  .weak RTC_IRQHandler
  .thumb_set RTC_IRQHandler,Dummy_Handler

  .weak WWDT_IRQHandler
  .thumb_set WWDT_IRQHandler,Dummy_Handler

  .weak C_CAN0_IRQHandler
  .thumb_set C_CAN0_IRQHandler,Dummy_Handler

  .weak QEI_IRQHandler
  .thumb_set QEI_IRQHandler,Dummy_Handler

#else

  .thumb_func
  .weak DAC_IRQHandler
DAC_IRQHandler:
  b .

  .thumb_func
  .weak DMA_IRQHandler
DMA_IRQHandler:
  b .

  .thumb_func
  .weak FLASH_IRQHandler
FLASH_IRQHandler:
  b .

  .thumb_func
  .weak ETHERNET_IRQHandler
ETHERNET_IRQHandler:
  b .

  .thumb_func
  .weak SDIO_IRQHandler
SDIO_IRQHandler:
  b .

  .thumb_func
  .weak LCD_IRQHandler
LCD_IRQHandler:
  b .

  .thumb_func
  .weak USB0_IRQHandler
USB0_IRQHandler:
  b .

  .thumb_func
  .weak USB1_IRQHandler
USB1_IRQHandler:
  b .

  .thumb_func
  .weak SCT_IRQHandler
SCT_IRQHandler:
  b .

  .thumb_func
  .weak RITIMER_IRQHandler
RITIMER_IRQHandler:
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
  .weak MCPWM_IRQHandler
MCPWM_IRQHandler:
  b .

  .thumb_func
  .weak ADC0_IRQHandler
ADC0_IRQHandler:
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
  .weak ADC1_IRQHandler
ADC1_IRQHandler:
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
  .weak USART0_IRQHandler
USART0_IRQHandler:
  b .

  .thumb_func
  .weak UART1_IRQHandler
UART1_IRQHandler:
  b .

  .thumb_func
  .weak USART2_IRQHandler
USART2_IRQHandler:
  b .

  .thumb_func
  .weak USART3_IRQHandler
USART3_IRQHandler:
  b .

  .thumb_func
  .weak I2S0_IRQHandler
I2S0_IRQHandler:
  b .

  .thumb_func
  .weak I2S1_IRQHandler
I2S1_IRQHandler:
  b .

  .thumb_func
  .weak SPIFI_IRQHandler
SPIFI_IRQHandler:
  b .

  .thumb_func
  .weak PIN_INT0_IRQHandler
PIN_INT0_IRQHandler:
  b .

  .thumb_func
  .weak PIN_INT1_IRQHandler
PIN_INT1_IRQHandler:
  b .

  .thumb_func
  .weak PIN_INT2_IRQHandler
PIN_INT2_IRQHandler:
  b .

  .thumb_func
  .weak PIN_INT3_IRQHandler
PIN_INT3_IRQHandler:
  b .

  .thumb_func
  .weak PIN_INT4_IRQHandler
PIN_INT4_IRQHandler:
  b .

  .thumb_func
  .weak PIN_INT5_IRQHandler
PIN_INT5_IRQHandler:
  b .

  .thumb_func
  .weak PIN_INT6_IRQHandler
PIN_INT6_IRQHandler:
  b .

  .thumb_func
  .weak PIN_INT7_IRQHandler
PIN_INT7_IRQHandler:
  b .

  .thumb_func
  .weak GINT0_IRQHandler
GINT0_IRQHandler:
  b .

  .thumb_func
  .weak GINT1_IRQHandler
GINT1_IRQHandler:
  b .

  .thumb_func
  .weak EVENTROUTER_IRQHandler
EVENTROUTER_IRQHandler:
  b .

  .thumb_func
  .weak C_CAN1_IRQHandler
C_CAN1_IRQHandler:
  b .

  .thumb_func
  .weak ATIMER_IRQHandler
ATIMER_IRQHandler:
  b .

  .thumb_func
  .weak RTC_IRQHandler
RTC_IRQHandler:
  b .

  .thumb_func
  .weak WWDT_IRQHandler
WWDT_IRQHandler:
  b .

  .thumb_func
  .weak C_CAN0_IRQHandler
C_CAN0_IRQHandler:
  b .

  .thumb_func
  .weak QEI_IRQHandler
QEI_IRQHandler:
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
  .word DAC_IRQHandler
  .word Dummy_Handler /* Reserved */
  .word DMA_IRQHandler
  .word Dummy_Handler /* Reserved */
  .word FLASH_IRQHandler
  .word ETHERNET_IRQHandler
  .word SDIO_IRQHandler
  .word LCD_IRQHandler
  .word USB0_IRQHandler
  .word USB1_IRQHandler
  .word SCT_IRQHandler
  .word RITIMER_IRQHandler
  .word TIMER0_IRQHandler
  .word TIMER1_IRQHandler
  .word TIMER2_IRQHandler
  .word TIMER3_IRQHandler
  .word MCPWM_IRQHandler
  .word ADC0_IRQHandler
  .word I2C0_IRQHandler
  .word I2C1_IRQHandler
  .word Dummy_Handler /* Reserved */
  .word ADC1_IRQHandler
  .word SSP0_IRQHandler
  .word SSP1_IRQHandler
  .word USART0_IRQHandler
  .word UART1_IRQHandler
  .word USART2_IRQHandler
  .word USART3_IRQHandler
  .word I2S0_IRQHandler
  .word I2S1_IRQHandler
  .word SPIFI_IRQHandler
  .word Dummy_Handler /* Reserved */
  .word PIN_INT0_IRQHandler
  .word PIN_INT1_IRQHandler
  .word PIN_INT2_IRQHandler
  .word PIN_INT3_IRQHandler
  .word PIN_INT4_IRQHandler
  .word PIN_INT5_IRQHandler
  .word PIN_INT6_IRQHandler
  .word PIN_INT7_IRQHandler
  .word GINT0_IRQHandler
  .word GINT1_IRQHandler
  .word EVENTROUTER_IRQHandler
  .word C_CAN1_IRQHandler
  .word Dummy_Handler /* Reserved */
  .word Dummy_Handler /* Reserved */
  .word ATIMER_IRQHandler
  .word RTC_IRQHandler
  .word Dummy_Handler /* Reserved */
  .word WWDT_IRQHandler
  .word Dummy_Handler /* Reserved */
  .word C_CAN0_IRQHandler
  .word QEI_IRQHandler
_vectors_end:

#ifdef VECTORS_IN_RAM
  .section .vectors_ram, "ax"
  .align 0
  .global _vectors_ram

_vectors_ram:
  .space _vectors_end - _vectors, 0
#endif
