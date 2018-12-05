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

  .weak RIT_IRQHandler
  .thumb_set RIT_IRQHandler,Dummy_Handler

  .weak SSP1_IRQHandler
  .thumb_set SSP1_IRQHandler,Dummy_Handler

  .weak I2C_IRQHandler
  .thumb_set I2C_IRQHandler,Dummy_Handler

  .weak CT16B0_IRQHandler
  .thumb_set CT16B0_IRQHandler,Dummy_Handler

  .weak CT16B1_IRQHandler
  .thumb_set CT16B1_IRQHandler,Dummy_Handler

  .weak CT32B0_IRQHandler
  .thumb_set CT32B0_IRQHandler,Dummy_Handler

  .weak CT32B1_IRQHandler
  .thumb_set CT32B1_IRQHandler,Dummy_Handler

  .weak SSP0_IRQHandler
  .thumb_set SSP0_IRQHandler,Dummy_Handler

  .weak USART_IRQHandler
  .thumb_set USART_IRQHandler,Dummy_Handler

  .weak USB_IRQHandler
  .thumb_set USB_IRQHandler,Dummy_Handler

  .weak USB_FIQ_IRQHandler
  .thumb_set USB_FIQ_IRQHandler,Dummy_Handler

  .weak ADC_IRQHandler
  .thumb_set ADC_IRQHandler,Dummy_Handler

  .weak WWDT_IRQHandler
  .thumb_set WWDT_IRQHandler,Dummy_Handler

  .weak BOD_IRQHandler
  .thumb_set BOD_IRQHandler,Dummy_Handler

  .weak FLASH_IRQHandler
  .thumb_set FLASH_IRQHandler,Dummy_Handler

  .weak USBWAKEUP_IRQHandler
  .thumb_set USBWAKEUP_IRQHandler,Dummy_Handler

#else

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
  .weak RIT_IRQHandler
RIT_IRQHandler:
  b .

  .thumb_func
  .weak SSP1_IRQHandler
SSP1_IRQHandler:
  b .

  .thumb_func
  .weak I2C_IRQHandler
I2C_IRQHandler:
  b .

  .thumb_func
  .weak CT16B0_IRQHandler
CT16B0_IRQHandler:
  b .

  .thumb_func
  .weak CT16B1_IRQHandler
CT16B1_IRQHandler:
  b .

  .thumb_func
  .weak CT32B0_IRQHandler
CT32B0_IRQHandler:
  b .

  .thumb_func
  .weak CT32B1_IRQHandler
CT32B1_IRQHandler:
  b .

  .thumb_func
  .weak SSP0_IRQHandler
SSP0_IRQHandler:
  b .

  .thumb_func
  .weak USART_IRQHandler
USART_IRQHandler:
  b .

  .thumb_func
  .weak USB_IRQHandler
USB_IRQHandler:
  b .

  .thumb_func
  .weak USB_FIQ_IRQHandler
USB_FIQ_IRQHandler:
  b .

  .thumb_func
  .weak ADC_IRQHandler
ADC_IRQHandler:
  b .

  .thumb_func
  .weak WWDT_IRQHandler
WWDT_IRQHandler:
  b .

  .thumb_func
  .weak BOD_IRQHandler
BOD_IRQHandler:
  b .

  .thumb_func
  .weak FLASH_IRQHandler
FLASH_IRQHandler:
  b .

  .thumb_func
  .weak USBWAKEUP_IRQHandler
USBWAKEUP_IRQHandler:
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
  .word Dummy_Handler /* Reserved */
  .word Dummy_Handler /* Reserved */
  .word RIT_IRQHandler
  .word Dummy_Handler /* Reserved */
  .word SSP1_IRQHandler
  .word I2C_IRQHandler
  .word CT16B0_IRQHandler
  .word CT16B1_IRQHandler
  .word CT32B0_IRQHandler
  .word CT32B1_IRQHandler
  .word SSP0_IRQHandler
  .word USART_IRQHandler
  .word USB_IRQHandler
  .word USB_FIQ_IRQHandler
  .word ADC_IRQHandler
  .word WWDT_IRQHandler
  .word BOD_IRQHandler
  .word FLASH_IRQHandler
  .word Dummy_Handler /* Reserved */
  .word Dummy_Handler /* Reserved */
  .word USBWAKEUP_IRQHandler
_vectors_end:

#ifdef VECTORS_IN_RAM
  .section .vectors_ram, "ax"
  .align 0
  .global _vectors_ram

_vectors_ram:
  .space _vectors_end - _vectors, 0
#endif
