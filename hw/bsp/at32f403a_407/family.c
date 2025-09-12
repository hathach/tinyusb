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

#include "at32f403a_407_clock.h"
#include "bsp/board_api.h"
#include "board.h"

void usb_clock48m_select(usb_clk48_s clk_s);
void uart_print_init(uint32_t baudrate);

//--------------------------------------------------------------------+
// Forward USB interrupt events to TinyUSB IRQ Handler
//--------------------------------------------------------------------+
void USBFS_H_CAN1_TX_IRQHandler(void) {
  tud_int_handler(0);
}
void USBFS_L_CAN1_RX0_IRQHandler(void) {
  tud_int_handler(0);
}
void USBFS_MAPH_IRQHandler(void) {
  tud_int_handler(0);
}
void USBFS_MAPL_IRQHandler(void) {
  tud_int_handler(0);
}
void USBFSWakeUp_IRQHandler(void) {
  tud_int_handler(0);
}

void board_init(void) {
  system_clock_config();

  /* config nvic priority group */
  nvic_priority_group_config(NVIC_PRIORITY_GROUP_4);

  /* select usb 48m clcok source */
  usb_clock48m_select(USB_CLK_HICK);

  /* configure systick */
  systick_clock_source_config(SYSTICK_CLOCK_SOURCE_AHBCLK_NODIV);

  /* enable usb clock */
  crm_periph_clock_enable(CRM_USB_PERIPH_CLOCK, TRUE);

  SysTick_Config(SystemCoreClock / 1000);
#if CFG_TUSB_OS == OPT_OS_FREERTOS
  // If freeRTOS is used, IRQ priority is limit by max syscall ( smaller is higher )
  NVIC_SetPriority(USBFS_H_CAN1_TX_IRQn, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY);
  NVIC_SetPriority(USBFS_L_CAN1_RX0_IRQn, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY);
  NVIC_SetPriority(USBFSWakeUp_IRQn, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY);
#endif

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

      /* 120MHz */
      case 120000000:
        crm_usb_clock_div_set(CRM_USB_DIV_2_5);
        break;

      /* 144MHz */
      case 144000000:
        crm_usb_clock_div_set(CRM_USB_DIV_3);
        break;

      /* 168MHz */
      case 168000000:
        crm_usb_clock_div_set(CRM_USB_DIV_3_5);
        break;

      /* 192MHz */
      case 192000000:
        crm_usb_clock_div_set(CRM_USB_DIV_4);
        break;

      default:
        break;
    }
  }
}

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
  /* configure uart param */
  usart_init(PRINT_UART, baudrate, USART_DATA_8BITS, USART_STOP_1_BIT);
  usart_transmitter_enable(PRINT_UART, TRUE);
  usart_enable(PRINT_UART, TRUE);
}

void board_led_write(bool state) {
  gpio_bits_write(LED_PORT, LED_PIN, state ^ (!LED_STATE_ON));
}

uint32_t board_button_read(void) {
  return gpio_input_data_bit_read(BUTTON_PORT, BUTTON_PIN) == BUTTON_STATE_ACTIVE;
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

int board_uart_read(uint8_t *buf, int len) {
  (void) buf;
  (void) len;
  return 0;
}

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
