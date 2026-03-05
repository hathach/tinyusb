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
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "bsp/board_api.h"

/* Blink pattern
 * - 250 ms  : button is not pressed
 * - 1000 ms : button is pressed (and hold)
 */
enum {
  BLINK_PRESSED = 250,
  BLINK_UNPRESSED = 1000
};

#define HELLO_STR   "Hello from TinyUSB\r\n"

// board test example does not use both device and host stack
#if CFG_TUSB_OS != OPT_OS_NONE
uint32_t tusb_time_millis_api(void) {
  return osal_time_millis();
}

void tusb_time_delay_ms_api(uint32_t ms) {
  osal_task_delay(ms);
}
#endif

//--------------------------------------------------------------------+
//
//--------------------------------------------------------------------+

// Task parameter type: ULONG for ThreadX, void* for FreeRTOS and noos
#if CFG_TUSB_OS == OPT_OS_THREADX
  #define RTOS_PARAM ULONG
#elif CFG_TUSB_OS == OPT_OS_FREERTOS
  #define RTOS_PARAM void*
  static void freertos_init(void);
#else
  #define RTOS_PARAM void*
#endif

static void board_test_loop(RTOS_PARAM param) {
  (void) param;
  uint32_t start_ms = 0;
  (void) start_ms;
  bool led_state = false;

  while (1) {
    uint32_t interval_ms = board_button_read() ? BLINK_PRESSED : BLINK_UNPRESSED;

    int ch = board_getchar();
    if (ch > 0) {
      board_putchar(ch);
      #ifndef LOGGER_UART
      board_uart_write(&ch, 1);
      #endif
    }

    // Blink and print every interval ms
    #if CFG_TUSB_OS == OPT_OS_FREERTOS
    vTaskDelay(interval_ms / portTICK_PERIOD_MS);
    #elif CFG_TUSB_OS == OPT_OS_THREADX
    tx_thread_sleep(_osal_ms2tick(interval_ms));
    #else
    if (tusb_time_millis_api() - start_ms < interval_ms) {
      continue; // not enough time
    }
    #endif
    start_ms = tusb_time_millis_api();

    if (ch < 0) {
      // skip if echoing
      printf(HELLO_STR);

      #ifndef LOGGER_UART
      board_uart_write(HELLO_STR, sizeof(HELLO_STR)-1);
      #endif
    }

    board_led_write(led_state);
    led_state = !led_state; // toggle
  }
}

int main(void) {
  board_init();
  board_led_write(true);

#if CFG_TUSB_OS == OPT_OS_FREERTOS
  freertos_init();
#elif CFG_TUSB_OS == OPT_OS_THREADX
  tx_kernel_enter();
#else
  board_test_loop(NULL);
#endif

  return 0;
}

#ifdef ESP_PLATFORM
void app_main(void) {
  main();
}
#endif

//--------------------------------------------------------------------+
// FreeRTOS
//--------------------------------------------------------------------+
#if CFG_TUSB_OS == OPT_OS_FREERTOS

#ifdef ESP_PLATFORM
#define MAIN_STACK_SIZE   4096
#else
#define MAIN_STACK_SIZE   512
#endif

#if configSUPPORT_STATIC_ALLOCATION
static StackType_t  _main_stack[MAIN_STACK_SIZE];
static StaticTask_t _main_taskdef;
#endif

static void freertos_init(void) {
  #if configSUPPORT_STATIC_ALLOCATION
  xTaskCreateStatic(board_test_loop, "main", MAIN_STACK_SIZE, NULL, 1, _main_stack, &_main_taskdef);
  #else
  xTaskCreate(board_test_loop, "main", MAIN_STACK_SIZE, NULL, 1, NULL);
  #endif

  #ifndef ESP_PLATFORM
  vTaskStartScheduler();
  #endif
}

//--------------------------------------------------------------------+
// ThreadX
//--------------------------------------------------------------------+
#elif CFG_TUSB_OS == OPT_OS_THREADX

#define MAIN_TASK_STACK_SIZE  1024
static TX_THREAD _main_thread;
static ULONG _main_thread_stack[MAIN_TASK_STACK_SIZE / sizeof(ULONG)];

void tx_application_define(void *first_unused_memory) {
  (void) first_unused_memory;
  static CHAR main_thread_name[] = "main";
  tx_thread_create(&_main_thread, main_thread_name, board_test_loop, 0,
                   _main_thread_stack, MAIN_TASK_STACK_SIZE,
                   1, 1, TX_NO_TIME_SLICE, TX_AUTO_START);
}
#endif
