/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
 * Copyright (c) 2023 HiFiPhile
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

#include "stm32_hal.h"
#include "bsp/board_api.h"
#include "board.h"

#ifdef UART_ID
  #if UART_ID == 1
    #define UARTn             HAL_UART1
    #define UARTn_CLK_ENABLE   HAL_RCC_USART1_EnableClock
  #elif UART_ID == 2
    #define UARTn             HAL_UART2
    #define UARTn_CLK_ENABLE   HAL_RCC_USART2_EnableClock
  #endif
#endif

#ifndef UART_GET_INSTANCE
  #define UART_GET_INSTANCE(handle)   ((USART_TypeDef *)((uint32_t)(handle)->instance))
#endif

//--------------------------------------------------------------------+
// Forward USB interrupt events to TinyUSB IRQ Handler
//--------------------------------------------------------------------+
void USB_DRD_FS_IRQHandler(void) {
  tusb_int_handler(0, true);
}

//--------------------------------------------------------------------+
// MACRO TYPEDEF CONSTANT ENUM
//--------------------------------------------------------------------+
#ifdef UART_ID
static hal_uart_handle_t hUSART;
#endif

void board_init(void) {
  HAL_Init();
  board_clock_init();

  // Enable peripheral clocks.
  HAL_RCC_GPIOA_EnableClock();
  HAL_RCC_GPIOB_EnableClock();
  HAL_RCC_GPIOC_EnableClock();
  HAL_RCC_GPIOD_EnableClock();
  HAL_RCC_USB_EnableClock();

#if CFG_TUSB_OS == OPT_OS_NONE
  // 1ms tick timer
  SysTick_Config(SystemCoreClock / 1000);
#elif CFG_TUSB_OS == OPT_OS_FREERTOS
  // Explicitly disable systick to prevent its ISR from running before scheduler start
  SysTick->CTRL &= ~1U;

  // If freeRTOS is used, IRQ priority is limit by max syscall ( smaller is higher )
  NVIC_SetPriority(USB_DRD_FS_IRQn, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY);
#endif

  // LED
  {
    hal_gpio_config_t gpio_config;
    gpio_config.mode = HAL_GPIO_MODE_OUTPUT;
    gpio_config.speed = HAL_GPIO_SPEED_FREQ_LOW;
    gpio_config.pull = HAL_GPIO_PULL_NO;
    gpio_config.output_type = HAL_GPIO_OUTPUT_PUSHPULL;
    gpio_config.init_state = HAL_GPIO_PIN_RESET;

    HAL_GPIO_Init(LED_PORT, LED_PIN, &gpio_config);
  }

  // Button
  {
    hal_gpio_config_t gpio_config;
    gpio_config.mode = HAL_GPIO_MODE_INPUT;
    gpio_config.speed = HAL_GPIO_SPEED_FREQ_LOW;
    gpio_config.pull = BUTTON_STATE_ACTIVE ? HAL_GPIO_PULL_DOWN : HAL_GPIO_PULL_UP;
    HAL_GPIO_Init(BUTTON_PORT, BUTTON_PIN, &gpio_config);
  }

#ifdef UART_ID
  UARTn_CLK_ENABLE();
  // UART
  {
    hal_gpio_config_t  gpio_config;
    gpio_config.mode = HAL_GPIO_MODE_ALTERNATE;
    gpio_config.output_type = HAL_GPIO_OUTPUT_PUSHPULL;
    gpio_config.pull = HAL_GPIO_PULL_NO;
    gpio_config.speed = HAL_GPIO_SPEED_FREQ_LOW;
    gpio_config.alternate = UART_GPIO_AF;
    HAL_GPIO_Init(UART_GPIO_PORT, UART_TX_PIN | UART_RX_PIN, &gpio_config);
  }

  hal_uart_config_t uart_config;
  HAL_UART_Init(&hUSART, UARTn);
  uart_config.baud_rate = 115200;
  uart_config.clock_prescaler = HAL_UART_PRESCALER_DIV1;
  uart_config.word_length = HAL_UART_WORD_LENGTH_8_BIT;
  uart_config.stop_bits = HAL_UART_STOP_BIT_1;
  uart_config.parity = HAL_UART_PARITY_NONE;
  uart_config.direction = HAL_UART_DIRECTION_TX_RX;
  uart_config.hw_flow_ctl = HAL_UART_HW_CONTROL_NONE;
  uart_config.oversampling = HAL_UART_OVERSAMPLING_16;
  uart_config.one_bit_sampling = HAL_UART_ONE_BIT_SAMPLE_DISABLE;

  HAL_UART_SetConfig(&hUSART, &uart_config);

  /* Fifo configuration */
  HAL_UART_SetTxFifoThreshold(&hUSART, HAL_UART_FIFO_THRESHOLD_1_8);
  HAL_UART_SetRxFifoThreshold(&hUSART, HAL_UART_FIFO_THRESHOLD_1_8);
  HAL_UART_EnableFifoMode(&hUSART);

  LL_USART_Enable(UART_GET_INSTANCE(&hUSART));
#endif
}

//--------------------------------------------------------------------+
// Board porting API
//--------------------------------------------------------------------+

void board_led_write(bool state) {
  hal_gpio_pin_state_t pin_state = state ? HAL_GPIO_PIN_SET : HAL_GPIO_PIN_RESET;
  HAL_GPIO_WritePin(LED_PORT, LED_PIN, pin_state);
}

uint32_t board_button_read(void) {
  return BUTTON_STATE_ACTIVE == HAL_GPIO_ReadPin(BUTTON_PORT, BUTTON_PIN);
}

size_t board_get_unique_id(uint8_t id[], size_t max_len) {
  (void) max_len;
  volatile uint32_t * stm32_uuid = (volatile uint32_t *) UID_BASE;
  uint32_t* id32 = (uint32_t*) (uintptr_t) id;
  uint8_t const len = 12;

  id32[0] = stm32_uuid[0];
  id32[1] = stm32_uuid[1];
  id32[2] = stm32_uuid[2];

  return len;
}

int board_uart_read(uint8_t *buf, int len) {
#ifdef UART_ID
  int count = 0;
  while (count < len) {
    if (LL_USART_IsActiveFlag_RXNE_RXFNE(UART_GET_INSTANCE(&hUSART))) {
      buf[count] = (uint8_t) UART_GET_INSTANCE(&hUSART)->RDR;
      count++;
    } else {
      break;
    }
  }
  return count;
#else
  (void) buf; (void) len;
  return -1;
#endif
}

int board_uart_write(void const *buf, int len) {
#ifdef UART_ID
  const uint8_t *p = (const uint8_t *) buf;
  int count = 0;
  while (count < len) {
    if (LL_USART_IsActiveFlag_TXE_TXFNF(UART_GET_INSTANCE(&hUSART))) {
      UART_GET_INSTANCE(&hUSART)->TDR = p[count];
      count++;
    } else {
      break;
    }
  }
  return count;
#else
  (void) buf; (void) len;
  return 0;
#endif
}

#if CFG_TUSB_OS == OPT_OS_NONE
volatile uint32_t system_ticks = 0;

void SysTick_Handler(void) {
  system_ticks++;
  HAL_IncTick();
}

uint32_t tusb_time_millis_api(void) {
  return system_ticks;
}
#endif

void HardFault_Handler(void) {
  __asm("BKPT #0\n");
}

#ifndef __ICCARM__
// Implement _start() since we use linker flag '-nostartfiles'.
extern int main(void);
TU_ATTR_UNUSED void _start(void) {
  // called by startup code
  main();
  while (1) {}
}
#endif

// Required by __libc_init_array in startup code if we are compiling using
// -nostdlib/-nostartfiles.
void _init(void) {

}
