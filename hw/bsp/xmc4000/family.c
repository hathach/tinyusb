/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2021, Ha Thach (tinyusb.org)
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

#include "xmc_gpio.h"
#include "xmc_scu.h"

#include "bsp/board.h"
#include "board.h"


//--------------------------------------------------------------------+
// Forward USB interrupt events to TinyUSB IRQ Handler
//--------------------------------------------------------------------+
void USB0_0_IRQHandler(void)
{
  tud_int_handler(0);
}

void board_init(void)
{
  board_clock_init();
  SystemCoreClockUpdate();

  // LED
  XMC_GPIO_CONFIG_t led_cfg;
  led_cfg.mode = XMC_GPIO_MODE_OUTPUT_PUSH_PULL;
  led_cfg.output_level = XMC_GPIO_OUTPUT_LEVEL_HIGH;
  led_cfg.output_strength = XMC_GPIO_OUTPUT_STRENGTH_MEDIUM;
  XMC_GPIO_Init(LED_PIN, &led_cfg);

  // Button
  XMC_GPIO_CONFIG_t button_cfg;
  button_cfg.mode = XMC_GPIO_MODE_INPUT_TRISTATE;
  XMC_GPIO_Init(BUTTON_PIN, &button_cfg);

#if CFG_TUSB_OS == OPT_OS_NONE
  // 1ms tick timer
  SysTick_Config(SystemCoreClock / 1000);

#elif CFG_TUSB_OS == OPT_OS_FREERTOS
  // Explicitly disable systick to prevent its ISR runs before scheduler start
  SysTick->CTRL &= ~1U;

  // If freeRTOS is used, IRQ priority is limit by max syscall ( smaller is higher )
  NVIC_SetPriority(USB0_0_IRQn, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY );
#endif

  // USB Power Enable
  XMC_SCU_RESET_DeassertPeripheralReset(XMC_SCU_PERIPHERAL_RESET_USB0);
  XMC_SCU_POWER_EnableUsb();
}

//--------------------------------------------------------------------+
// Board porting API
//--------------------------------------------------------------------+

void board_led_write(bool state)
{
  uint32_t is_high = state ? LED_STATE_ON : (1-LED_STATE_ON);

  XMC_GPIO_SetOutputLevel(LED_PIN, is_high ? XMC_GPIO_OUTPUT_LEVEL_HIGH : XMC_GPIO_OUTPUT_LEVEL_LOW);
}

uint32_t board_button_read(void)
{
  return BUTTON_STATE_ACTIVE == XMC_GPIO_GetInput(BUTTON_PIN);
}

int board_uart_read(uint8_t* buf, int len)
{
#ifdef UART_DEV
  for(int i=0;i<len;i++) {
    buf[i] = uart_getc(uart_inst);
  }
  return len;
#else
  (void) buf; (void) len;
  return 0;
#endif
}

int board_uart_write(void const * buf, int len)
{
#ifdef UART_DEV
  char const* bufch = (char const*) buf;
  for(int i=0;i<len;i++) {
    uart_putc(uart_inst, bufch[i]);
  }
  return len;
#else
  (void) buf; (void) len;
  return 0;
#endif
}

#if CFG_TUSB_OS  == OPT_OS_NONE
volatile uint32_t system_ticks = 0;
void SysTick_Handler(void)
{
  system_ticks++;
}

uint32_t board_millis(void)
{
  return system_ticks;
}
#endif
