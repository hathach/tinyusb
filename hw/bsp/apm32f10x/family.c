/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
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
   manufacturer: Geehy
*/

#include "apm32f10x.h"
#include "apm32f10x_rcm.h"
#include "apm32f10x_gpio.h"
#include "apm32f10x_usart.h"
#include "apm32f10x_misc.h"
#include "bsp/board_api.h"
#include "board.h"

#if CFG_TUSB_MCU == OPT_MCU_APM32F107
void OTG_FS_IRQHandler(void);
#endif

#if CFG_TUSB_OS == OPT_OS_NONE
void SysTick_Handler(void);
void SVC_Handler(void);
void PendSV_Handler(void);
#endif
void HardFault_Handler(void);
void _init(void);

//--------------------------------------------------------------------+
// Forward USB interrupt events to TinyUSB IRQ Handler
//--------------------------------------------------------------------+

#if CFG_TUSB_MCU == OPT_MCU_APM32F103
void USBD1_HP_CAN1_TX_IRQHandler(void) {
  tud_int_handler(0);
}

void USBD1_LP_CAN1_RX0_IRQHandler(void) {
  tud_int_handler(0);
}

#elif CFG_TUSB_MCU == OPT_MCU_APM32F107
void OTG_FS_IRQHandler(void) {
  tusb_int_handler(0, true);
}
#endif

//--------------------------------------------------------------------+
// Board Init
//--------------------------------------------------------------------+
void board_init(void) {
  SystemClockConfig();

#if CFG_TUSB_MCU == OPT_MCU_APM32F103
  RCM_EnableAPB1PeriphClock(RCM_APB1_PERIPH_USB);
#elif CFG_TUSB_MCU == OPT_MCU_APM32F107
  RCM_EnableAHBPeriphClock(RCM_AHB_PERIPH_OTG_FS);
  
  GPIO_Config_T otg_gpio_config;
  GPIO_ConfigStructInit(&otg_gpio_config);
  otg_gpio_config.pin = GPIO_PIN_11 | GPIO_PIN_12;
  otg_gpio_config.mode = GPIO_MODE_AF_PP;
  otg_gpio_config.speed = GPIO_SPEED_50MHz;
  GPIO_Config(GPIOA, &otg_gpio_config);
#endif

#if CFG_TUSB_OS == OPT_OS_NONE
  // 1ms tick timer
  SysTick_Config(SystemCoreClock / 1000);
#elif CFG_TUSB_OS == OPT_OS_FREERTOS
  // Explicitly disable systick to prevent its ISR from running before scheduler start
  SysTick->CTRL &= ~1U;

  // If freeRTOS is used, IRQ priority is limit by max syscall ( smaller is higher )
#if CFG_TUSB_MCU == OPT_MCU_APM32F103
  NVIC_SetPriority(USBD1_HP_CAN1_TX_IRQn, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY);
  NVIC_SetPriority(USBD1_LP_CAN1_RX0_IRQn, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY);
#elif CFG_TUSB_MCU == OPT_MCU_APM32F107
  NVIC_SetPriority(OTG_FS_IRQn, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY);
#endif
#endif

  LED_GPIO_CLK_EN();
  GPIO_Config_T led_gpio_config;
  GPIO_ConfigStructInit(&led_gpio_config);
  led_gpio_config.pin = LED_PIN;
  led_gpio_config.mode = GPIO_MODE_OUT_PP;
  led_gpio_config.speed = GPIO_SPEED_50MHz;
  GPIO_Config(LED_PORT, &led_gpio_config);

  // Button (PA1 = KEY1)
  BUTTON_GPIO_CLK_EN();
  GPIO_Config_T btn_gpio_config;
  GPIO_ConfigStructInit(&btn_gpio_config);
  btn_gpio_config.pin = BUTTON_PIN;
  btn_gpio_config.mode = GPIO_MODE_IN_PU;
  btn_gpio_config.speed = GPIO_SPEED_50MHz;
  GPIO_Config(BUTTON_PORT, &btn_gpio_config);

  //--------------------------------------------------------------------+
  // UART (USART1: PA9 = TX, PA10 = RX)
  //--------------------------------------------------------------------+
  UART_GPIO_CLK_EN();
  RCM_EnableAPB2PeriphClock(RCM_APB2_PERIPH_USART1);

  // PA9 = TX (AF push-pull)
  GPIO_Config_T uart_tx_config;
  GPIO_ConfigStructInit(&uart_tx_config);
  uart_tx_config.pin = UART_TX_PIN;
  uart_tx_config.mode = GPIO_MODE_AF_PP;
  uart_tx_config.speed = GPIO_SPEED_50MHz;
  GPIO_Config(UART_TX_PORT, &uart_tx_config);

  // PA10 = RX (floating input)
  GPIO_Config_T uart_rx_config;
  GPIO_ConfigStructInit(&uart_rx_config);
  uart_rx_config.pin = UART_RX_PIN;
  uart_rx_config.mode = GPIO_MODE_IN_FLOATING;
  uart_rx_config.speed = GPIO_SPEED_50MHz;
  GPIO_Config(UART_RX_PORT, &uart_rx_config);

  // USART1 config: 115200 8N1
  USART_Config_T usart_config;
  USART_ConfigStructInit(&usart_config);
  usart_config.baudRate = CFG_BOARD_UART_BAUDRATE;
  usart_config.wordLength = USART_WORD_LEN_8B;
  usart_config.stopBits = USART_STOP_BIT_1;
  usart_config.parity = USART_PARITY_NONE;
  usart_config.mode = USART_MODE_TX_RX;
  usart_config.hardwareFlow = USART_HARDWARE_FLOW_NONE;
  USART_Config(USART1, &usart_config);
  USART_Enable(USART1);

  board_led_write(false);
}

//--------------------------------------------------------------------+
// Board porting API
//--------------------------------------------------------------------+
void board_led_write(bool state) {
  if (state) {
    LED_PORT->BC = LED_PIN;  // LED On (active low)
  } else {
    LED_PORT->BSC = LED_PIN; // LED Off
  }
}

uint32_t board_button_read(void) {
  // Button is active low (pull-up), return 1 when pressed
  return (GPIO_ReadInputBit(BUTTON_PORT, BUTTON_PIN) == 0) ? 1 : 0;
}

size_t board_get_unique_id(uint8_t id[], size_t max_len) {
  (void) max_len;
  volatile uint32_t *apm32_uuid = ((volatile uint32_t *) 0x1FFFF7E8);
  uint32_t *id32 = (uint32_t *) (uintptr_t) id;
  uint8_t const len = 12;

  id32[0] = apm32_uuid[0];
  id32[1] = apm32_uuid[1];
  id32[2] = apm32_uuid[2];

  return len;
}

int board_uart_read(uint8_t *buf, int len) {
  int count = 0;
  while (count < len) {
    if (USART_ReadStatusFlag(USART1, USART_FLAG_RXBNE)) {
      buf[count++] = (uint8_t) USART_RxData(USART1);
    } else {
      break;
    }
  }
  return count;
}

int board_uart_write(void const *buf, int len) {
  const uint8_t *p = (const uint8_t *) buf;
  int count = 0;
  while (count < len) {
    if (USART_ReadStatusFlag(USART1, USART_FLAG_TXBE)) {
      USART_TxData(USART1, p[count++]);
    }
  }
  return count;
}

#if CFG_TUSB_OS == OPT_OS_NONE
volatile uint32_t system_ticks = 0;

void SysTick_Handler(void) {
  system_ticks++;
}

uint32_t tusb_time_millis_api(void) {
  return system_ticks;
}

void SVC_Handler(void) {
}

void PendSV_Handler(void) {
}
#endif

void HardFault_Handler(void) {
  #if defined(__GNUC__)
    __asm__("BKPT #0\n");
  #elif defined(__ICCARM__)
    __asm("BKPT #0");
  #endif
  while(1) {}
}

void _init(void) {
}
