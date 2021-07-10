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

#include "../board.h"
#include "nuclei_sdk_hal.h"

//--------------------------------------------------------------------+
// Forward USB interrupt events to TinyUSB IRQ Handler
//--------------------------------------------------------------------+

void USBFS_IRQHandler(void) { tud_int_handler(0); }

//--------------------------------------------------------------------+
// MACRO TYPEDEF CONSTANT ENUM
//--------------------------------------------------------------------+

#define HXTAL_VALUE \
  ((uint32_t)8000000) /*!< value of the external oscillator in Hz */
#define USB_NO_VBUS_PIN

//--------------------------------------------------------------------+
// LED
//--------------------------------------------------------------------+

#define LED_PORT GPIOC
#define LED_PIN GPIO_PIN_13
#define LED_STATE_ON 1

#define BUTTON_PORT GPIOA
#define BUTTON_PIN GPIO_PIN_0
#define BUTTON_STATE_ACTIVE 1

//--------------------------------------------------------------------+
// UART
//--------------------------------------------------------------------+

#define UART_DEV USART0
#define UART_GPIO_PORT GPIOA
#define UART_TX_PIN GPIO_PIN_9
#define UART_RX_PIN GPIO_PIN_10

void board_init(void) {
  /* Disable interrupts during init */
  __disable_irq();

  /* Reset eclic configuration registers */
  ECLIC->CFG = 0;
  ECLIC->MTH = 0;

  /* Reset eclic interrupt registers */
  for (int32_t i = 0; i < SOC_INT_MAX; i++) {
    ECLIC->CTRL[0].INTIP = 0;
    ECLIC->CTRL[0].INTIE = 0;
    ECLIC->CTRL[0].INTATTR = 0;
    ECLIC->CTRL[0].INTCTRL = 0;
  }

  /* Set 4 bits for interrupt level and 0 bits for priority */
  __ECLIC_SetCfgNlbits(4);

  SystemCoreClockUpdate();

#if CFG_TUSB_OS == OPT_OS_NONE
  SysTimer_SetLoadValue(0);
  SysTimer_SetCompareValue(SystemCoreClock / 1000);
  ECLIC_SetLevelIRQ(SysTimer_IRQn, 3);
  ECLIC_SetTrigIRQ(SysTimer_IRQn, ECLIC_POSTIVE_EDGE_TRIGGER);
  ECLIC_EnableIRQ(SysTimer_IRQn);
  SysTimer_Start();
#endif

  rcu_periph_clock_enable(RCU_GPIOA);
  rcu_periph_clock_enable(RCU_GPIOB);
  rcu_periph_clock_enable(RCU_GPIOC);
  rcu_periph_clock_enable(RCU_GPIOD);
  rcu_periph_clock_enable(RCU_AF);

#ifdef BUTTON_PIN
  gpio_init(BUTTON_PORT, GPIO_MODE_IN_FLOATING, GPIO_OSPEED_50MHZ, BUTTON_PIN);
#endif

#ifdef LED_PIN
  gpio_init(LED_PORT, GPIO_MODE_OUT_PP, GPIO_OSPEED_50MHZ, LED_PIN);
  board_led_write(0);
#endif

#if defined(UART_DEV)
  /* enable GPIO TX and RX clock */
  rcu_periph_clock_enable(GD32_COM_TX_GPIO_CLK);
  rcu_periph_clock_enable(GD32_COM_RX_GPIO_CLK);

  /* enable USART clock */
  rcu_periph_clock_enable(GD32_COM_CLK);

  /* connect port to USARTx_Tx */
  gpio_init(GD32_COM_TX_GPIO_PORT, GPIO_MODE_AF_PP, GPIO_OSPEED_50MHZ,
            GD32_COM_TX_PIN);

  /* connect port to USARTx_Rx */
  gpio_init(GD32_COM_RX_GPIO_PORT, GPIO_MODE_IN_FLOATING, GPIO_OSPEED_50MHZ,
            GD32_COM_RX_PIN);

  /* USART configure */
  usart_deinit(UART_DEV);
  usart_baudrate_set(UART_DEV, 115200U);
  usart_word_length_set(UART_DEV, USART_WL_8BIT);
  usart_stop_bit_set(UART_DEV, USART_STB_1BIT);
  usart_parity_config(UART_DEV, USART_PM_NONE);
  usart_hardware_flow_rts_config(UART_DEV, USART_RTS_DISABLE);
  usart_hardware_flow_cts_config(UART_DEV, USART_CTS_DISABLE);
  usart_receive_config(UART_DEV, USART_RECEIVE_ENABLE);
  usart_transmit_config(UART_DEV, USART_TRANSMIT_ENABLE);
  usart_enable(UART_DEV);
#endif

  /* USB D+ and D- pins don't need to be configured. */
  /* Configure VBUS Pin */
#ifndef USB_NO_VBUS_PIN
  gpio_init(GPIOA, GPIO_MODE_IN_FLOATING, GPIO_OSPEED_50MHZ, GPIO_PIN_9);
#endif

  /* This for ID line debug */
  // gpio_init(GPIOA, GPIO_MODE_AF_OD, GPIO_OSPEED_50MHZ, GPIO_PIN_10);

  /* Enable USB OTG clock */
  usb_rcu_config();

  /* Reset USB OTG peripheral */
  rcu_periph_reset_enable(RCU_USBFSRST);
  rcu_periph_reset_disable(RCU_USBFSRST);

  /* Set IRQ priority and trigger */
  ECLIC_SetLevelIRQ(USBFS_IRQn, 15);
  ECLIC_SetTrigIRQ(USBFS_IRQn, ECLIC_POSTIVE_EDGE_TRIGGER);

  /* Retrieve otg core registers */
  usb_gr* otg_core_regs = (usb_gr*)(USBFS_REG_BASE + USB_REG_OFFSET_CORE);

#ifdef USB_NO_VBUS_PIN
  /* Disable VBUS sense*/
  otg_core_regs->GCCFG |= GCCFG_VBUSIG | GCCFG_PWRON | GCCFG_VBUSBCEN;
#else
  /* Enable VBUS sense via pin PA9 */
  otg_core_regs->GCCFG |= GCCFG_VBUSIG | GCCFG_PWRON | GCCFG_VBUSBCEN;
  otg_core_regs->GCCFG &= ~GCCFG_VBUSIG;
#endif

  /* Enable interrupts globaly */
  __enable_irq();
}

#include "gd32vf103_dbg.h"

#define DBG_KEY_UNLOCK 0x4B5A6978
#define DBG_CMD_RESET 0x1

#define DBG_KEY REG32(DBG + 0x0C)
#define DBG_CMD REG32(DBG + 0x08)

void gd32vf103_reset(void) {
  /* The MTIMER unit of the GD32VF103 doesn't have the MSFRST
   * register to generate a software reset request.
   * BUT instead two undocumented registers in the debug peripheral
   * that allow issueing a software reset.
   * https://github.com/esmil/gd32vf103inator/blob/master/include/gd32vf103/dbg.h
   */
  DBG_KEY = DBG_KEY_UNLOCK;
  DBG_CMD = DBG_CMD_RESET;
}

//--------------------------------------------------------------------+
// Board porting API
//--------------------------------------------------------------------+

void board_led_write(bool state) {
  gpio_bit_write(LED_PORT, LED_PIN, state ? LED_STATE_ON : (1 - LED_STATE_ON));
}

uint32_t board_button_read(void) {
  return BUTTON_STATE_ACTIVE == gpio_input_bit_get(BUTTON_PORT, BUTTON_PIN);
}

int board_uart_read(uint8_t* buf, int len) {
#if defined(UART_DEV)
  int rxsize = len;
  while (rxsize--) {
    *(uint8_t*)buf = usart_read(UART_DEV);
    buf++;
  }
  return len;
#else
  (void)buf;
  (void)len;
  return 0;
#endif
}

int board_uart_write(void const* buf, int len) {
#if defined(UART_DEV)
  int txsize = len;
  while (txsize--) {
    usart_write(UART_DEV, *(uint8_t*)buf);
    buf++;
  }
  return len;
#else
  (void)buf;
  (void)len;
  return 0;
#endif
}

#if CFG_TUSB_OS == OPT_OS_NONE
volatile uint32_t system_ticks = 0;
void eclic_mtip_handler(void) { system_ticks++; }
uint32_t board_millis(void) { return system_ticks; }
#endif

#ifdef USE_FULL_ASSERT
/**
 * @brief  Reports the name of the source file and the source line number
 *         where the assert_param error has occurred.
 * @param  file: pointer to the source file name
 * @param  line: assert_param error line source number
 * @retval None
 */
void assert_failed(char* file, uint32_t line) {
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line
     number,
     tex: printf("Wrong parameters value: file %s on line %d\r\n", file, line)
   */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

// Required by __libc_init_array in startup code if we are compiling using
// -nostdlib/-nostartfiles.
// void _init(void) {}
