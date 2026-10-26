/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2025 Ennebi Elettronica (https://ennebielettronica.com)
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
// MACRO CONSTANT TYPEDEF PROTYPES
//--------------------------------------------------------------------+

/* Blink pattern
 * - 250 ms  : device not mounted
 * - 1000 ms : device mounted
 * - 2500 ms : device is suspended
 */
enum {
  BLINK_NOT_MOUNTED = 250,
  BLINK_MOUNTED = 1000,
  BLINK_SUSPENDED = 2500,
};

static uint32_t blink_interval_ms = BLINK_NOT_MOUNTED;

void led_blinking_task(void* param);

#if CFG_TUSB_OS == OPT_OS_FREERTOS
void usb_device_task(void* param);
void freertos_init(void);
#endif

/*------------- MAIN -------------*/
int main(void) {
  board_init();

#if CFG_TUSB_OS == OPT_OS_FREERTOS
  freertos_init();
#else
  // init device stack on configured roothub port
  tusb_rhport_init_t dev_init = {
    .role = TUSB_ROLE_DEVICE,
    .speed = TUSB_SPEED_AUTO
  };
  tusb_init(BOARD_TUD_RHPORT, &dev_init);
  board_init_after_tusb();

  while (1) {
    tud_task(); // tinyusb device task
    led_blinking_task(NULL);
  }
#endif
}

//--------------------------------------------------------------------+
// Device callbacks
//--------------------------------------------------------------------+

// Invoked when device is mounted
void tud_mount_cb(void) {
  blink_interval_ms = BLINK_MOUNTED;
}

// Invoked when device is unmounted
void tud_umount_cb(void) {
  blink_interval_ms = BLINK_NOT_MOUNTED;
}

// Invoked when usb bus is suspended
// remote_wakeup_en : if host allow us  to perform remote wakeup
// Within 7ms, device must draw an average of current less than 2.5 mA from bus
void tud_suspend_cb(bool remote_wakeup_en) {
  (void) remote_wakeup_en;
  blink_interval_ms = BLINK_SUSPENDED;
}

// Invoked when usb bus is resumed
void tud_resume_cb(void) {
  blink_interval_ms = tud_mounted() ? BLINK_MOUNTED : BLINK_NOT_MOUNTED;
}

//--------------------------------------------------------------------+
// BLINKING TASK
//--------------------------------------------------------------------+
void led_blinking_task(void* param) {
  (void) param;
  static bool led_state = false;

#if CFG_TUSB_OS == OPT_OS_FREERTOS
  while (1) {
    vTaskDelay(blink_interval_ms / portTICK_PERIOD_MS);
    board_led_write(led_state);
    led_state = 1 - led_state; // toggle
  }
#else
  static uint32_t start_ms = 0;

  // Blink every interval ms
  if (tusb_time_millis_api() - start_ms < blink_interval_ms) return; // not enough time
  start_ms += blink_interval_ms;

  board_led_write(led_state);
  led_state = 1 - led_state; // toggle
#endif
}

//--------------------------------------------------------------------+
// FreeRTOS
//--------------------------------------------------------------------+
#if CFG_TUSB_OS == OPT_OS_FREERTOS

#define BLINKY_STACK_SIZE   configMINIMAL_STACK_SIZE

#ifdef ESP_PLATFORM
  #define USBD_STACK_SIZE   4096
  void app_main(void) {
    main();
  }
#else
  // Increase stack size when debug log is enabled
  #define USBD_STACK_SIZE   ((3*configMINIMAL_STACK_SIZE/2) * (CFG_TUSB_DEBUG ? 2 : 1))
#endif

#if configSUPPORT_STATIC_ALLOCATION
StackType_t  blinky_stack[BLINKY_STACK_SIZE];
StaticTask_t blinky_taskdef;

StackType_t  usb_device_stack[USBD_STACK_SIZE];
StaticTask_t usb_device_taskdef;
#endif

// USB Device Driver task: processes all usb events and invokes callbacks
void usb_device_task(void* param) {
  (void) param;

  // init device stack on configured roothub port. Must be called after the
  // scheduler starts: the USB IRQ handler uses RTOS queue APIs.
  tusb_rhport_init_t dev_init = {
    .role = TUSB_ROLE_DEVICE,
    .speed = TUSB_SPEED_AUTO
  };
  tusb_init(BOARD_TUD_RHPORT, &dev_init);
  board_init_after_tusb();

  while (1) {
    tud_task(); // put thread to waiting state until there is a new event
  }
}

void freertos_init(void) {
  #if configSUPPORT_STATIC_ALLOCATION
  TU_ASSERT(xTaskCreateStatic(led_blinking_task, "blinky", BLINKY_STACK_SIZE, NULL, 1,
                              blinky_stack, &blinky_taskdef) != NULL, );
  TU_ASSERT(xTaskCreateStatic(usb_device_task, "usbd", USBD_STACK_SIZE, NULL, configMAX_PRIORITIES - 1,
                              usb_device_stack, &usb_device_taskdef) != NULL, );
  #else
  TU_ASSERT(xTaskCreate(led_blinking_task, "blinky", BLINKY_STACK_SIZE, NULL, 1, NULL) == pdPASS, );
  TU_ASSERT(xTaskCreate(usb_device_task, "usbd", USBD_STACK_SIZE, NULL, configMAX_PRIORITIES - 1, NULL) == pdPASS, );
  #endif

  // espressif starts the scheduler before app_main
  #ifndef ESP_PLATFORM
  vTaskStartScheduler();
  #endif
}
#endif
