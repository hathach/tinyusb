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
   manufacturer: Artery
*/

#include "at32f425_clock.h"
#include "board.h"
#include "bsp/board_api.h"

void uart_print_init(uint32_t baudrate);
void usb_clock48m_select(usb_clk48_s clk_s);
void led_and_button_init(void);

//--------------------------------------------------------------------+
// Forward USB interrupt events to TinyUSB IRQ Handler
//--------------------------------------------------------------------+
void OTGFS1_IRQHandler(void) {
  tusb_int_handler(0, true);
}
void OTGFS1_WKUP_IRQHandler(void) {
  tusb_int_handler(0, true);
}

void board_init(void) {
  /* config nvic priority group */
  nvic_priority_group_config(NVIC_PRIORITY_GROUP_4);

  /* config system clock */
  system_clock_config();

  /* enable usb clock */
  crm_periph_clock_enable(OTG_CLOCK, TRUE);

  /* select usb 48m clcok source */
  usb_clock48m_select(USB_CLK_HEXT);

  /* vbus ignore */
  board_vbus_sense_init();

  /* configure systick */
  systick_clock_source_config(SYSTICK_CLOCK_SOURCE_AHBCLK_NODIV);
  SysTick_Config(SystemCoreClock / 1000);
#if CFG_TUSB_OS == OPT_OS_FREERTOS
  // If freeRTOS is used, IRQ priority is limit by max syscall ( smaller is higher )
  NVIC_SetPriority(OTG_IRQ, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY);
#endif

/* otgfs use vbus pin */
#ifndef USB_VBUS_IGNORE
  gpio_init_type gpio_init_struct;
  crm_periph_clock_enable(OTG_PIN_GPIO_CLOCK, TRUE);
  gpio_default_para_init(&gpio_init_struct);
  gpio_init_struct.gpio_drive_strength = GPIO_DRIVE_STRENGTH_STRONGER;
  gpio_init_struct.gpio_out_type = GPIO_OUTPUT_PUSH_PULL;
  gpio_init_struct.gpio_mode = GPIO_MODE_MUX;
  gpio_init_struct.gpio_pins = OTG_PIN_VBUS;
  gpio_init_struct.gpio_pull = GPIO_PULL_DOWN;
  gpio_pin_mux_config(OTG_PIN_GPIO, OTG_PIN_VBUS_SOURCE, OTG_PIN_MUX);
  gpio_init(OTG_PIN_GPIO, &gpio_init_struct);
#endif

  /* config led and key */
  led_and_button_init();

  /* config usart printf */
  uart_print_init(115200);
  printf("usart printf config success!\r\n");
}

//--------------------------------------------------------------------+
// Board porting API
//--------------------------------------------------------------------+
/**
  * @brief  usb 48M clock select
  * @param  clk_s:USB_CLK_HICK, USB_CLK_HEXT
  * @retval none
  */
void usb_clock48m_select(usb_clk48_s clk_s) {
  if (clk_s == USB_CLK_HICK) {
    crm_usb_clock_source_select(CRM_USB_CLOCK_SOURCE_HICK);

    /* enable the acc calibration ready interrupt */
    crm_periph_clock_enable(CRM_ACC_PERIPH_CLOCK, TRUE);

    /* update the c1\c2\c3 value */
    acc_write_c1(7980);
    acc_write_c2(8000);
    acc_write_c3(8020);

    /* open acc calibration */
    acc_calibration_mode_enable(ACC_CAL_HICKTRIM, TRUE);
  } else {
    /* usb divider reset */
    crm_usb_div_reset();

    switch (system_core_clock) {
      /* 48MHz */
      case 48000000:
        crm_usb_clock_div_set(CRM_USB_DIV_1);
        break;

      /* 72MHz */
      case 72000000:
        crm_usb_clock_div_set(CRM_USB_DIV_1_5);
        break;

      /* 96MHz */
      case 96000000:
        crm_usb_clock_div_set(CRM_USB_DIV_2);
        break;

      default:
        break;
    }
  }
}

void led_and_button_init(void) {
  /* LED */
  gpio_init_type gpio_led_init_struct;
  /* enable the led clock */
  LED_GPIO_CLK_EN();
  /* set default parameter */
  gpio_default_para_init(&gpio_led_init_struct);
  /* configure the led gpio */
  gpio_led_init_struct.gpio_drive_strength = GPIO_DRIVE_STRENGTH_STRONGER;
  gpio_led_init_struct.gpio_out_type = GPIO_OUTPUT_PUSH_PULL;
  gpio_led_init_struct.gpio_mode = GPIO_MODE_OUTPUT;
  gpio_led_init_struct.gpio_pins = LED_PIN;
  gpio_led_init_struct.gpio_pull = GPIO_PULL_NONE;
  gpio_init(LED_PORT, &gpio_led_init_struct);
  /* Button */
  gpio_init_type gpio_button_init_struct;
  /* enable the button clock */
  BUTTON_GPIO_CLK_EN();
  /* set default parameter */
  gpio_default_para_init(&gpio_button_init_struct);
  /* configure the button gpio */
  gpio_button_init_struct.gpio_drive_strength = GPIO_DRIVE_STRENGTH_STRONGER;
  gpio_button_init_struct.gpio_out_type = GPIO_OUTPUT_PUSH_PULL;
  gpio_button_init_struct.gpio_mode = GPIO_MODE_INPUT;
  gpio_button_init_struct.gpio_pins = BUTTON_PIN;
  gpio_button_init_struct.gpio_pull = GPIO_PULL_DOWN;
  gpio_init(BUTTON_PORT, &gpio_button_init_struct);
}

/**
  * @brief  initialize uart
  * @param  baudrate: uart baudrate
  * @retval none
  */
void uart_print_init(uint32_t baudrate) {
  gpio_init_type gpio_init_struct;
  /* enable the uart and gpio clock */
  crm_periph_clock_enable(PRINT_UART_CRM_CLK, TRUE);
  crm_periph_clock_enable(PRINT_UART_TX_GPIO_CRM_CLK, TRUE);
  gpio_default_para_init(&gpio_init_struct);
  /* configure the uart tx pin */
  gpio_init_struct.gpio_drive_strength = GPIO_DRIVE_STRENGTH_STRONGER;
  gpio_init_struct.gpio_out_type = GPIO_OUTPUT_PUSH_PULL;
  gpio_init_struct.gpio_mode = GPIO_MODE_MUX;
  gpio_init_struct.gpio_pins = PRINT_UART_TX_PIN;
  gpio_init_struct.gpio_pull = GPIO_PULL_NONE;
  gpio_init(PRINT_UART_TX_GPIO, &gpio_init_struct);
  gpio_pin_mux_config(PRINT_UART_TX_GPIO, PRINT_UART_TX_PIN_SOURCE, PRINT_UART_TX_PIN_MUX_NUM);
  /* configure uart param */
  usart_init(PRINT_UART, baudrate, USART_DATA_8BITS, USART_STOP_1_BIT);
  usart_transmitter_enable(PRINT_UART, TRUE);
  usart_enable(PRINT_UART, TRUE);
}

// Get characters from UART. Return number of read bytes
int board_uart_read(uint8_t *buf, int len) {
  (void) buf;
  (void) len;
  return 0;
}

// Send characters to UART. Return number of sent bytes
int board_uart_write(void const *buf, int len) {
#if CFG_TUSB_OS == OPT_OS_NONE
  int txsize = len;
  u16 timeout = 0xffff;
  while (txsize--) {
    while (usart_flag_get(PRINT_UART, USART_TDBE_FLAG) == RESET) {
      timeout--;
      if (timeout == 0) {
        return 0;
      }
    }
    PRINT_UART->dt = (*((uint8_t const *) buf) & 0x01FF);
    buf++;
  }
  return len;
#else
  (void) buf;
  (void) len;
  return 0;
#endif
}

void board_led_write(bool state) {
  gpio_bits_write(LED_PORT, LED_PIN, state ^ (!LED_STATE_ON));
}

uint32_t board_button_read(void) {
  return gpio_input_data_bit_read(BUTTON_PORT, BUTTON_PIN);
}

size_t board_get_unique_id(uint8_t id[], size_t max_len) {
  (void) max_len;
  volatile uint32_t *at32_uuid = ((volatile uint32_t *) 0x1FFFF7E8);
  uint32_t *id32 = (uint32_t *) (uintptr_t) id;
  uint8_t const len = 12;

  id32[0] = at32_uuid[0];
  id32[1] = at32_uuid[1];
  id32[2] = at32_uuid[2];

  return len;
}


#if CFG_TUSB_OS == OPT_OS_NONE

volatile uint32_t system_ticks = 0;
void SysTick_Handler(void) {
  system_ticks++;
}

uint32_t board_millis(void) {
  return system_ticks;
}

void SVC_Handler(void) {
}

void PendSV_Handler(void) {
}
#endif

void HardFault_Handler(void) {
  __asm("BKPT #0\n");
}

// Required by __libc_init_array in startup code if we are compiling using
// -nostdlib/-nostartfiles.
void _init(void) {
}

#ifdef USE_FULL_ASSERT
void assert_failed(const char *file, uint32_t line) {
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     tex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
