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
#include "tusb.h"

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF PROTOTYPES
//--------------------------------------------------------------------+

/* Blink pattern
 * - 250 ms  : device not mounted
 * - 1000 ms : device mounted
 * - 2500 ms : device is suspended
 */
enum {
  BLINK_NOT_MOUNTED = 250,
  BLINK_MOUNTED     = 1000,
  BLINK_SUSPENDED   = 2500,
};

static uint32_t blink_interval_ms = BLINK_NOT_MOUNTED;

// Task parameter type: ULONG for ThreadX, void* for FreeRTOS and noos
#if CFG_TUSB_OS == OPT_OS_THREADX
  #define RTOS_PARAM ULONG
#elif CFG_TUSB_OS == OPT_OS_FREERTOS
  #define RTOS_PARAM void*
  static void freertos_init(void);
#else
  #define RTOS_PARAM void*
#endif

void led_blinking_task(RTOS_PARAM param);

//--------------------------------------------------------------------+
// USB Device Task
//--------------------------------------------------------------------+
static void usb_device_init(void) {
  tusb_rhport_init_t dev_init = {
    .role  = TUSB_ROLE_DEVICE,
    .speed = TUSB_SPEED_AUTO
  };
  tusb_init(BOARD_TUD_RHPORT, &dev_init);
  board_init_after_tusb();
}

#if CFG_TUSB_OS != OPT_OS_NONE && CFG_TUSB_OS != OPT_OS_PICO
static void usb_device_task(RTOS_PARAM param) {
  (void) param;
  usb_device_init();

  while (1) {
    tud_task();
  }
}
#endif

//--------------------------------------------------------------------+
// Main
//--------------------------------------------------------------------+
int main(void) {
  board_init();

#if CFG_TUSB_OS == OPT_OS_FREERTOS
  freertos_init();

#elif CFG_TUSB_OS == OPT_OS_THREADX
  tx_kernel_enter();

#else
  // noos + pico-sdk: init USB then run polling loop
  usb_device_init();

  while (1) {
    tud_task();
    led_blinking_task(NULL);
  }
#endif
}

#ifdef ESP_PLATFORM
void app_main(void) {
  main();
}
#endif

//--------------------------------------------------------------------+
// Device callbacks
//--------------------------------------------------------------------+
void tud_mount_cb(void) {
  blink_interval_ms = BLINK_MOUNTED;
}

void tud_umount_cb(void) {
  blink_interval_ms = BLINK_NOT_MOUNTED;
}

void tud_suspend_cb(bool remote_wakeup_en) {
  (void) remote_wakeup_en;
  blink_interval_ms = BLINK_SUSPENDED;
}

void tud_resume_cb(void) {
  blink_interval_ms = tud_mounted() ? BLINK_MOUNTED : BLINK_NOT_MOUNTED;
}

//--------------------------------------------------------------------+
// Blinking Task
//--------------------------------------------------------------------+
void led_blinking_task(RTOS_PARAM param) {
  (void) param;
  static uint32_t start_ms = 0;
  static bool led_state = false;

  while (1) {
#if CFG_TUSB_OS == OPT_OS_FREERTOS
    vTaskDelay(blink_interval_ms / portTICK_PERIOD_MS);
#elif CFG_TUSB_OS == OPT_OS_THREADX
    tx_thread_sleep(_osal_ms2tick(blink_interval_ms));
#else
    if (tusb_time_millis_api() - start_ms < blink_interval_ms) {
      return; // not enough time
    }
#endif

    start_ms += blink_interval_ms;
    board_led_write(led_state);
    led_state = 1 - led_state; // toggle
  }
}

//--------------------------------------------------------------------+
// FreeRTOS
//--------------------------------------------------------------------+
#if CFG_TUSB_OS == OPT_OS_FREERTOS

#ifdef ESP_PLATFORM
#define USBD_STACK_SIZE   4096
#else
#define USBD_STACK_SIZE   (3*configMINIMAL_STACK_SIZE/2 * (CFG_TUSB_DEBUG ? 2 : 1))
#endif
#define BLINKY_STACK_SIZE   configMINIMAL_STACK_SIZE

#if configSUPPORT_STATIC_ALLOCATION
static StackType_t  _usb_device_stack[USBD_STACK_SIZE];
static StaticTask_t _usb_device_taskdef;
static StackType_t  _blinky_stack[BLINKY_STACK_SIZE];
static StaticTask_t _blinky_taskdef;
#endif


static void freertos_init(void) {
  #if configSUPPORT_STATIC_ALLOCATION
  xTaskCreateStatic(usb_device_task,     "usbd",   USBD_STACK_SIZE,   NULL, configMAX_PRIORITIES - 1, _usb_device_stack, &_usb_device_taskdef);
  xTaskCreateStatic(led_blinking_task,   "blinky", BLINKY_STACK_SIZE, NULL, 1,                        _blinky_stack,     &_blinky_taskdef);
  #else
  xTaskCreate(usb_device_task,           "usbd",   USBD_STACK_SIZE,   NULL, configMAX_PRIORITIES - 1, NULL);
  xTaskCreate(led_blinking_task,         "blinky", BLINKY_STACK_SIZE, NULL, 1,                        NULL);
  #endif
  #ifndef ESP_PLATFORM
  vTaskStartScheduler();
  #endif
}

//--------------------------------------------------------------------+
// ThreadX
//--------------------------------------------------------------------+
#elif CFG_TUSB_OS == OPT_OS_THREADX

#define USBD_STACK_SIZE   4096
#define BLINKY_STACK_SIZE 512

static TX_THREAD _usb_device_thread;
static ULONG     _usb_device_stack[USBD_STACK_SIZE / sizeof(ULONG)];
static TX_THREAD _blinky_thread;
static ULONG     _blinky_stack[BLINKY_STACK_SIZE / sizeof(ULONG)];

void tx_application_define(void *first_unused_memory) {
  (void) first_unused_memory;
  static CHAR usbd_name[]   = "usbd";
  static CHAR blinky_name[] = "blinky";
  tx_thread_create(&_usb_device_thread, usbd_name, usb_device_task, 0,
                   _usb_device_stack, USBD_STACK_SIZE,
                   0, 0, TX_NO_TIME_SLICE, TX_AUTO_START);
  tx_thread_create(&_blinky_thread, blinky_name, led_blinking_task, 0,
                   _blinky_stack, BLINKY_STACK_SIZE,
                   1, 1, TX_NO_TIME_SLICE, TX_AUTO_START);
}

#endif
