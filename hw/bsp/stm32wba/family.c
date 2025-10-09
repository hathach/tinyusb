/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2025 Dalton Caron
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
   manufacturer: STMicroelectronics
*/

#include <stdbool.h>

#include "stm32wbaxx_hal.h"
#include "bsp/board_api.h"
#include "board.h"

//--------------------------------------------------------------------+
// MACRO TYPEDEF CONSTANT ENUM
//--------------------------------------------------------------------+
#ifndef USART_TIMEOUT_TICKS
#define USART_TIMEOUT_TICKS 1000
#endif

static UART_HandleTypeDef uart_handle;

//--------------------------------------------------------------------+
// Forward USB interrupt events to TinyUSB IRQ Handler
//--------------------------------------------------------------------+
void USB_OTG_HS_IRQHandler(void) {
  tud_int_handler(0);
}

//--------------------------------------------------------------------+
// Board porting API
//--------------------------------------------------------------------+

static void board_gpio_configuration(void) {
  GPIO_InitTypeDef gpio_init = {0};

  USART_GPIO_CLK_EN();
  USART_CLK_EN();

  // Configure USART TX pin
  gpio_init.Pin = USART_TX_PIN;
  gpio_init.Mode = GPIO_MODE_AF_PP;
  gpio_init.Pull = GPIO_NOPULL;
  gpio_init.Speed = GPIO_SPEED_FREQ_HIGH;
  gpio_init.Alternate = USART_GPIO_AF;
  HAL_GPIO_Init(USART_TX_GPIO_PORT, &gpio_init);

  // Configure USART RX pin
  gpio_init.Pin = USART_RX_PIN;
  gpio_init.Mode = GPIO_MODE_AF_PP;
  gpio_init.Pull = GPIO_PULLUP;
  gpio_init.Speed = GPIO_SPEED_FREQ_HIGH;
  gpio_init.Alternate = USART_GPIO_AF;
  HAL_GPIO_Init(USART_RX_GPIO_PORT, &gpio_init);

  // Configure the LED
  LED_CLK_EN();
  gpio_init.Pin = LED_PIN;
  gpio_init.Mode = GPIO_MODE_OUTPUT_PP;
  gpio_init.Pull = GPIO_PULLUP;
  gpio_init.Speed = GPIO_SPEED_FREQ_LOW;
  gpio_init.Alternate = 0;
  HAL_GPIO_Init(LED_PORT, &gpio_init);

  // Default LED state is off
  board_led_write(false);

  // Configure the button
  BUTTON_CLK_EN();
  gpio_init.Pin = BUTTON_PIN;
  gpio_init.Mode = GPIO_MODE_INPUT;
  gpio_init.Pull = GPIO_PULLUP;
  gpio_init.Speed = GPIO_SPEED_FREQ_LOW;
  gpio_init.Alternate = 0;
  HAL_GPIO_Init(BUTTON_PORT, &gpio_init);

  // Configure USB DM and DP pins. This is optional, and maintained only for user guidance.
  gpio_init.Pin = (GPIO_PIN_7 | GPIO_PIN_6);
  gpio_init.Mode = GPIO_MODE_INPUT;
  gpio_init.Pull = GPIO_NOPULL;
  gpio_init.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOD, &gpio_init);
}

static void board_uart_configuration(void) {
  uart_handle = ( UART_HandleTypeDef){
      .Instance = USART_DEV,
      .Init.BaudRate = CFG_BOARD_UART_BAUDRATE,
      .Init.WordLength = UART_WORDLENGTH_8B,
      .Init.StopBits = UART_STOPBITS_1,
      .Init.Parity = UART_PARITY_NONE,
      .Init.HwFlowCtl = UART_HWCONTROL_NONE,
      .Init.Mode = UART_MODE_TX_RX,
      .Init.OverSampling = UART_OVERSAMPLING_16
  };
  HAL_UART_Init(&uart_handle);
}

void board_init(void) {
  board_system_clock_config();
  board_gpio_configuration();
  board_uart_configuration();

  #ifdef USB_OTG_HS
  // STM32WBA65/64/62 only has 1 USB HS port

  #if CFG_TUSB_OS == OPT_OS_NONE
  // 1ms tick timer
  SysTick_Config(SystemCoreClock / 1000);
  #elif CFG_TUSB_OS == OPT_OS_FREERTOS
  // Explicitly disable systick to prevent its ISR runs before scheduler start
  SysTick->CTRL &= ~1U;

  // If freeRTOS is used, IRQ priority is limit by max syscall ( smaller is higher )
  NVIC_SetPriority(USB_OTG_HS_IRQn, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY);
  #endif

  // USB clock enable
  __HAL_RCC_USB_OTG_HS_CLK_ENABLE();
  __HAL_RCC_USB_OTG_HS_PHY_CLK_ENABLE();

  // See the reference manual section 11.4.7 for the USB OTG powering sequence

  // Remove the VDDUSB power isolation
  PWR->SVMCR |= PWR_SVMCR_USV;

  // Enable VDD11USB supply by clearing VDD11USBDIS to 0
  PWR->VOSR &= ~PWR_VOSR_VDD11USBDIS;

  // Enable USB OTG internal power by setting USBPWREN to 1
  PWR->VOSR |= PWR_VOSR_USBPWREN;

  // Wait for VDD11USB supply to be ready in VDD11USBRDY = 1
  while ((PWR->VOSR & PWR_VOSR_VDD11USBRDY) == 0) {}

  // Enable USB OTG booster by setting USBBOOSTEN to 1
  PWR->VOSR |= PWR_VOSR_USBBOOSTEN;

  // Wait for USB OTG booster to be ready in USBBOOSTRDY = 1
  while ((PWR->VOSR & PWR_VOSR_USBBOOSTRDY) == 0) {}

  // Enable USB power on Pwrctrl CR2 register
  PWR->SVMCR |= PWR_SVMCR_USV;

  // Set the reference clock selection (must match the clock source)
  SYSCFG->OTGHSPHYCR &= ~SYSCFG_OTGHSPHYCR_CLKSEL;
  SYSCFG->OTGHSPHYCR |= SYSCFG_OTGHSPHYCR_CLKSEL_0 | SYSCFG_OTGHSPHYCR_CLKSEL_1 |
      SYSCFG_OTGHSPHYCR_CLKSEL_3;// 32MHz clock

  // Configuring the SYSCFG registers OTG_HS PHY
  SYSCFG->OTGHSPHYCR |= SYSCFG_OTGHSPHYCR_EN;

  // Disable VBUS sense (B device)
  USB_OTG_HS->GCCFG &= ~USB_OTG_GCCFG_VBDEN;

  // B-peripheral session valid override enable
  USB_OTG_HS->GCCFG |= USB_OTG_GCCFG_VBVALEXTOEN;
  USB_OTG_HS->GCCFG |= USB_OTG_GCCFG_VBVALOVAL;
  #endif // USB_OTG_FS
}

void board_led_write(bool state) { HAL_GPIO_WritePin(LED_PORT, LED_PIN, state ? LED_STATE_ON : (1 - LED_STATE_ON)); }

uint32_t board_button_read(void) { return HAL_GPIO_ReadPin(BUTTON_PORT, BUTTON_PIN) == BUTTON_STATE_ACTIVE; }

int board_uart_read(uint8_t *buf, int len) {
  (void) buf;
  (void) len;
  return 0;
}

int board_uart_write(void const *buf, int len) {
  (void) HAL_UART_Transmit(&uart_handle, (const uint8_t *) buf, len, USART_TIMEOUT_TICKS);
  return len;
}

#if CFG_TUSB_OS == OPT_OS_NONE
volatile uint32_t system_ticks = 0;

void SysTick_Handler(void) {
  HAL_IncTick();
  system_ticks++;
}

uint32_t board_millis(void) { return system_ticks; }
#endif

void HardFault_Handler(void) { asm( "bkpt 1" ); }

// Required by __libc_init_array in startup code if we are compiling using -nostdlib/-nostartfiles.
void _init(void);

void _init(void) {}
