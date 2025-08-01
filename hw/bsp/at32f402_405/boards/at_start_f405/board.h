/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2020, Ha Thach (tinyusb.org)
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

/* metadata:
   name: AT-START-F405
   url: https://www.arterychip.com/en/product/AT32F405.jsp
*/

#ifndef BOARD_H_
#define BOARD_H_

#ifdef __cplusplus
 extern "C" {
#endif

#define USB_VBUS_IGNORE
//#define USB_SOF_OUTPUT_ENABLE

// LED
#define LED_PORT              GPIOF
#define LED_PIN               GPIO_PINS_4
#define LED_STATE_ON          0 // Active Low
#define LED_GPIO_CLK_EN()     crm_periph_clock_enable(CRM_GPIOF_PERIPH_CLOCK, TRUE)

// Button
#define BUTTON_PORT           GPIOA
#define BUTTON_PIN            GPIO_PINS_0
#define BUTTON_STATE_ACTIVE   0
#define BUTTON_GPIO_CLK_EN()  crm_periph_clock_enable(CRM_GPIOA_PERIPH_CLOCK, TRUE)

//USART
#define PRINT_UART                       USART1
#define PRINT_UART_CRM_CLK               CRM_USART1_PERIPH_CLOCK
#define PRINT_UART_TX_PIN                GPIO_PINS_9
#define PRINT_UART_TX_GPIO               GPIOA
#define PRINT_UART_TX_GPIO_CRM_CLK       CRM_GPIOA_PERIPH_CLOCK
#define PRINT_UART_TX_PIN_SOURCE         GPIO_PINS_SOURCE9
#define PRINT_UART_TX_PIN_MUX_NUM        GPIO_MUX_7

//USB
#ifdef BOARD_TUD_RHPORT
    #if BOARD_TUD_RHPORT == 0
        #define USB_ID                           USB_OTG1_ID
        #define OTG_CLOCK                        CRM_OTGFS1_PERIPH_CLOCK
        #define OTG_IRQ                          OTGFS1_IRQn
        #define OTG_IRQ_HANDLER                  OTGFS1_IRQHandler
        #define OTG_WKUP_IRQ                     OTGFS1_WKUP_IRQn
        #define OTG_WKUP_HANDLER                 OTGFS1_WKUP_IRQHandler
        #define OTG_WKUP_EXINT_LINE              EXINT_LINE_18
        #define OTG_PIN_GPIO                     GPIOA
        #define OTG_PIN_GPIO_CLOCK               CRM_GPIOA_PERIPH_CLOCK
        #define OTG_PIN_VBUS                     GPIO_PINS_9
        #define OTG_PIN_VBUS_SOURCE              GPIO_PINS_SOURCE9
        #define OTG_PIN_ID                       GPIO_PINS_10
        #define OTG_PIN_ID_SOURCE                GPIO_PINS_SOURCE10
        #define OTG_PIN_SOF_GPIO                 GPIOA
        #define OTG_PIN_SOF_GPIO_CLOCK           CRM_GPIOA_PERIPH_CLOCK
        #define OTG_PIN_SOF                      GPIO_PINS_8
        #define OTG_PIN_SOF_SOURCE               GPIO_PINS_SOURCE8
        #define OTG_PIN_MUX                      GPIO_MUX_10
        #define USB_SPEED_CORE_ID                USB_FULL_SPEED_CORE_ID
    #elif BOARD_TUD_RHPORT == 1
        #define USB_ID                           USB_OTG2_ID
        #define OTG_CLOCK                        CRM_OTGHS_PERIPH_CLOCK
        #define OTG_IRQ                          OTGHS_IRQn
        #define OTG_IRQ_HANDLER                  OTGHS_IRQHandler
        #define OTG_WKUP_IRQ                     OTGHS_WKUP_IRQn
        #define OTG_WKUP_HANDLER                 OTGHS_WKUP_IRQHandler
        #define OTG_WKUP_EXINT_LINE              EXINT_LINE_20
        #define OTG_PIN_GPIO                     GPIOB
        #define OTG_PIN_GPIO_CLOCK               CRM_GPIOB_PERIPH_CLOCK
        #define OTG_PIN_VBUS                     GPIO_PINS_13
        #define OTG_PIN_VBUS_SOURCE              GPIO_PINS_SOURCE13
        #define OTG_PIN_ID                       GPIO_PINS_12
        #define OTG_PIN_ID_SOURCE                GPIO_PINS_SOURCE12
        #define OTG_PIN_SOF_GPIO                 GPIOA
        #define OTG_PIN_SOF_GPIO_CLOCK           CRM_GPIOA_PERIPH_CLOCK
        #define OTG_PIN_SOF                      GPIO_PINS_4
        #define OTG_PIN_SOF_SOURCE               GPIO_PINS_SOURCE4
        #define OTG_PIN_MUX                      GPIO_MUX_10
        #define USB_SPEED_CORE_ID                USB_HIGH_SPEED_CORE_ID
    #endif
#endif
#ifdef BOARD_TUH_RHPORT
    #if BOARD_TUH_RHPORT == 0
        #define USB_ID                           USB_OTG1_ID
        #define OTG_CLOCK                        CRM_OTGFS1_PERIPH_CLOCK
        #define OTG_IRQ                          OTGFS1_IRQn
        #define OTG_IRQ_HANDLER                  OTGFS1_IRQHandler
        #define OTG_WKUP_IRQ                     OTGFS1_WKUP_IRQn
        #define OTG_WKUP_HANDLER                 OTGFS1_WKUP_IRQHandler
        #define OTG_WKUP_EXINT_LINE              EXINT_LINE_18
        #define OTG_PIN_GPIO                     GPIOA
        #define OTG_PIN_GPIO_CLOCK               CRM_GPIOA_PERIPH_CLOCK
        #define OTG_PIN_VBUS                     GPIO_PINS_9
        #define OTG_PIN_VBUS_SOURCE              GPIO_PINS_SOURCE9
        #define OTG_PIN_ID                       GPIO_PINS_10
        #define OTG_PIN_ID_SOURCE                GPIO_PINS_SOURCE10
        #define OTG_PIN_SOF_GPIO                 GPIOA
        #define OTG_PIN_SOF_GPIO_CLOCK           CRM_GPIOA_PERIPH_CLOCK
        #define OTG_PIN_SOF                      GPIO_PINS_8
        #define OTG_PIN_SOF_SOURCE               GPIO_PINS_SOURCE8
        #define OTG_PIN_MUX                      GPIO_MUX_10
        #define USB_SPEED_CORE_ID                USB_FULL_SPEED_CORE_ID
    #elif BOARD_TUH_RHPORT == 1
        #define USB_ID                           USB_OTG2_ID
        #define OTG_CLOCK                        CRM_OTGHS_PERIPH_CLOCK
        #define OTG_IRQ                          OTGHS_IRQn
        #define OTG_IRQ_HANDLER                  OTGHS_IRQHandler
        #define OTG_WKUP_IRQ                     OTGHS_WKUP_IRQn
        #define OTG_WKUP_HANDLER                 OTGHS_WKUP_IRQHandler
        #define OTG_WKUP_EXINT_LINE              EXINT_LINE_20
        #define OTG_PIN_GPIO                     GPIOB
        #define OTG_PIN_GPIO_CLOCK               CRM_GPIOB_PERIPH_CLOCK
        #define OTG_PIN_VBUS                     GPIO_PINS_13
        #define OTG_PIN_VBUS_SOURCE              GPIO_PINS_SOURCE13
        #define OTG_PIN_ID                       GPIO_PINS_12
        #define OTG_PIN_ID_SOURCE                GPIO_PINS_SOURCE12
        #define OTG_PIN_SOF_GPIO                 GPIOA
        #define OTG_PIN_SOF_GPIO_CLOCK           CRM_GPIOA_PERIPH_CLOCK
        #define OTG_PIN_SOF                      GPIO_PINS_4
        #define OTG_PIN_SOF_SOURCE               GPIO_PINS_SOURCE4
        #define OTG_PIN_MUX                      GPIO_MUX_10
        #define USB_SPEED_CORE_ID                USB_HIGH_SPEED_CORE_ID
    #endif
#endif

//Vbus
static inline void board_vbus_sense_init(void)
{
    #ifdef BOARD_TUD_RHPORT
        #if BOARD_TUD_RHPORT == 0
            *(int*)(0x50000038) |= (1<<21);
        #elif BOARD_TUD_RHPORT == 1
            *(int*)(0x40040038) |= (1<<21);
        #endif
    #endif
    #ifdef BOARD_TUH_RHPORT
        #if BOARD_TUH_RHPORT == 0
            *(int*)(0x50000038) |= (1<<21);
        #elif BOARD_TUH_RHPORT == 1
            *(int*)(0x40040038) |= (1<<21);
        #endif
    #endif
}

#ifdef __cplusplus
 }
#endif

#endif /* BOARD_H_ */
