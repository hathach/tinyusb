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
   name: AT-START-F425
   url: https://www.arterychip.com/en/product/AT32F425.jsp
*/

#ifndef BOARD_H_
#define BOARD_H_

#ifdef __cplusplus
 extern "C" {
#endif

#define USB_VBUS_IGNORE

// LED
#define LED_PORT              GPIOC
#define LED_PIN               GPIO_PINS_2
#define LED_STATE_ON          0 // Active Low
#define LED_GPIO_CLK_EN()     crm_periph_clock_enable(CRM_GPIOC_PERIPH_CLOCK, TRUE)

// Button
#define BUTTON_PORT           GPIOA
#define BUTTON_PIN            GPIO_PINS_0
#define BUTTON_STATE_ACTIVE   1
#define BUTTON_GPIO_CLK_EN()  crm_periph_clock_enable(CRM_GPIOA_PERIPH_CLOCK, TRUE)

// Usart
#define PRINT_UART                       USART1
#define PRINT_UART_CRM_CLK               CRM_USART1_PERIPH_CLOCK
#define PRINT_UART_TX_PIN                GPIO_PINS_9
#define PRINT_UART_TX_GPIO               GPIOA
#define PRINT_UART_TX_GPIO_CRM_CLK       CRM_GPIOA_PERIPH_CLOCK
#define PRINT_UART_TX_PIN_SOURCE         GPIO_PINS_SOURCE9
#define PRINT_UART_TX_PIN_MUX_NUM        GPIO_MUX_1

//USB
#define USB_ID                           0
#define OTG_CLOCK                        CRM_OTGFS1_PERIPH_CLOCK
#define OTG_IRQ                          OTGFS1_IRQn
#define OTG_IRQ_HANDLER                  OTGFS1_IRQHandler
#define OTG_WKUP_IRQ                     OTGFS1_WKUP_IRQn
#define OTG_WKUP_HANDLER                 OTGFS1_WKUP_IRQHandler
#define OTG_WKUP_EXINT_LINE              EXINT_LINE_18
#define OTG_PIN_GPIO                     GPIOA
#define OTG_PIN_GPIO_CLOCK               CRM_GPIOA_PERIPH_CLOCK
#define OTG_PIN_DP                       GPIO_PINS_12
#define OTG_PIN_DP_SOURCE                GPIO_PINS_SOURCE12
#define OTG_PIN_DM                       GPIO_PINS_11
#define OTG_PIN_DM_SOURCE                GPIO_PINS_SOURCE11
#define OTG_PIN_VBUS                     GPIO_PINS_9
#define OTG_PIN_VBUS_SOURCE              GPIO_PINS_SOURCE9
#define OTG_PIN_ID                       GPIO_PINS_10
#define OTG_PIN_ID_SOURCE                GPIO_PINS_SOURCE10
#define OTG_PIN_SOF_GPIO                 GPIOA
#define OTG_PIN_SOF_GPIO_CLOCK           CRM_GPIOA_PERIPH_CLOCK
#define OTG_PIN_SOF                      GPIO_PINS_8
#define OTG_PIN_SOF_SOURCE               GPIO_PINS_SOURCE8
#define OTG_PIN_MUX                      GPIO_MUX_3

static inline void board_vbus_sense_init(void)
{
    *(int*)(0x50000038) |= (1<<21);
}

#ifdef __cplusplus
 }
#endif

#endif /* BOARD_H_ */
