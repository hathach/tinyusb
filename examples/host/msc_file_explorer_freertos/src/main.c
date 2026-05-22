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

#include <string.h>

#include "bsp/board_api.h"
#include "tusb.h"
#ifdef ESP_PLATFORM
  // ESP-IDF need "freertos/" prefix in include path.
  // CFG_TUSB_OS_INC_PATH should be defined accordingly.
  #include "freertos/FreeRTOS.h"
  #include "freertos/task.h"
  #include "freertos/timers.h"
#else
  #include "FreeRTOS.h"
  #include "task.h"
  #include "timers.h"
#endif

#include "msc_app.h"

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF PROTYPES
//--------------------------------------------------------------------+
#ifdef ESP_PLATFORM
  #define USBH_STACK_SIZE     4096
#else
  // Increase stack size when debug log is enabled.
  #define USBH_STACK_SIZE    (configMINIMAL_STACK_SIZE * (CFG_TUSB_DEBUG ? 4 : 3))
#endif

enum {
  BLINK_MOUNTED = 1000,
};

#if configSUPPORT_STATIC_ALLOCATION
StaticTimer_t blinky_tmdef;

StackType_t  usb_host_stack[USBH_STACK_SIZE];
StaticTask_t usb_host_taskdef;
#endif

TimerHandle_t blinky_tm;

static void led_blinky_cb(TimerHandle_t xTimer);
static void usb_host_task(void* param);

/*------------- MAIN -------------*/
int main(void) {
  board_init();

  printf("TinyUSB Host MassStorage Explorer FreeRTOS Example\r\n");

  // Create soft timer for blinky and task for TinyUSB host stack.
#if configSUPPORT_STATIC_ALLOCATION
  blinky_tm = xTimerCreateStatic(NULL, pdMS_TO_TICKS(BLINK_MOUNTED), true, NULL, led_blinky_cb, &blinky_tmdef);
  xTaskCreateStatic(usb_host_task, "usbh", USBH_STACK_SIZE, NULL, configMAX_PRIORITIES - 1, usb_host_stack,
                    &usb_host_taskdef);
#else
  blinky_tm = xTimerCreate(NULL, pdMS_TO_TICKS(BLINK_MOUNTED), true, NULL, led_blinky_cb);
  xTaskCreate(usb_host_task, "usbh", USBH_STACK_SIZE, NULL, configMAX_PRIORITIES - 1, NULL);
#endif

  xTimerStart(blinky_tm, 0);

  // only start scheduler for non-espressif mcu
#ifndef ESP_PLATFORM
  vTaskStartScheduler();
#endif

  return 0;
}

#ifdef ESP_PLATFORM
void app_main(void) {
  main();
}
#endif

// USB Host task
// This top-level thread processes all USB events and invokes callbacks.
static void usb_host_task(void* param) {
  (void) param;

  // init host stack on configured roothub port
  tusb_rhport_init_t host_init = {
    .role = TUSB_ROLE_HOST,
    .speed = TUSB_SPEED_AUTO
  };

  if (!tusb_init(BOARD_TUH_RHPORT, &host_init)) {
    printf("Failed to init USB Host Stack\r\n");
    vTaskSuspend(NULL);
  }

  board_init_after_tusb();

#if CFG_TUH_ENABLED && CFG_TUH_MAX3421
  // FeatherWing MAX3421E uses MAX3421E GPIO0 for VBUS enable.
  enum { IOPINS1_ADDR = 20u << 3 };
  tuh_max3421_reg_write(BOARD_TUH_RHPORT, IOPINS1_ADDR, 0x01, false);
#endif

  if (!msc_app_init()) {
    printf("Failed to init MSC app\r\n");
    vTaskSuspend(NULL);
  }

  while (1) {
    // TinyUSB host task.
    tuh_task();
  }
}

//--------------------------------------------------------------------+
// TinyUSB Callbacks
//--------------------------------------------------------------------+

void tuh_mount_cb(uint8_t dev_addr) {
  (void) dev_addr;
}

void tuh_umount_cb(uint8_t dev_addr) {
  (void) dev_addr;
}

//--------------------------------------------------------------------+
// Blinking Task
//--------------------------------------------------------------------+
static void led_blinky_cb(TimerHandle_t xTimer) {
  (void) xTimer;
  static bool led_state = false;

  board_led_write(led_state);
  led_state = 1 - led_state; // toggle
}
