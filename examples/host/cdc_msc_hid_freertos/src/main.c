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

#if TU_CHECK_MCU(OPT_MCU_ESP32S2, OPT_MCU_ESP32S3)
  // ESP-IDF need "freertos/" prefix in include path.
  // CFG_TUSB_OS_INC_PATH should be defined accordingly.
  #include "freertos/FreeRTOS.h"
  #include "freertos/semphr.h"
  #include "freertos/queue.h"
  #include "freertos/task.h"
  #include "freertos/timers.h"

  #define USBH_STACK_SIZE     4096
#else
  #include "FreeRTOS.h"
  #include "semphr.h"
  #include "queue.h"
  #include "task.h"
  #include "timers.h"

  // Increase stack size when debug log is enabled
  #define USBH_STACK_SIZE    (3*configMINIMAL_STACK_SIZE/2) * (CFG_TUSB_DEBUG ? 2 : 1)
#endif


//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF PROTOTYPES
//--------------------------------------------------------------------+
/* Blink pattern
 * - 250 ms  : device not mounted
 * - 1000 ms : device mounted
 * - 2500 ms : device is suspended
 */
enum  {
  BLINK_NOT_MOUNTED = 250,
  BLINK_MOUNTED = 1000,
  BLINK_SUSPENDED = 2500,
};

// static timer & task
#if configSUPPORT_STATIC_ALLOCATION
StaticTimer_t blinky_tmdef;

StackType_t  usb_host_stack[USBH_STACK_SIZE];
StaticTask_t usb_host_taskdef;
#endif

TimerHandle_t blinky_tm;

static void led_blinky_cb(TimerHandle_t xTimer);
static void usb_host_task(void* param);

extern void cdc_app_init(void);
extern void hid_app_init(void);
extern void msc_app_init(void);

#if CFG_TUH_ENABLED && CFG_TUH_MAX3421
// API to read/rite MAX3421's register. Implemented by TinyUSB
extern uint8_t tuh_max3421_reg_read(uint8_t rhport, uint8_t reg, bool in_isr);
extern bool tuh_max3421_reg_write(uint8_t rhport, uint8_t reg, uint8_t data, bool in_isr);
#endif

/*------------- MAIN -------------*/
int main(void) {
  board_init();

  printf("TinyUSB Host CDC MSC HID with FreeRTOS Example\r\n");

  // Create soft timer for blinky, task for tinyusb stack
#if configSUPPORT_STATIC_ALLOCATION
  blinky_tm = xTimerCreateStatic(NULL, pdMS_TO_TICKS(BLINK_MOUNTED), true, NULL, led_blinky_cb, &blinky_tmdef);
  xTaskCreateStatic(usb_host_task, "usbh", USBH_STACK_SIZE, NULL, configMAX_PRIORITIES-1, usb_host_stack, &usb_host_taskdef);
#else
  blinky_tm = xTimerCreate(NULL, pdMS_TO_TICKS(BLINK_NOT_MOUNTED), true, NULL, led_blinky_cb);
  xTaskCreate(usb_host_task, "usbd", USBH_STACK_SIZE, NULL, configMAX_PRIORITIES-1, NULL);
#endif

  xTimerStart(blinky_tm, 0);

  // skip starting scheduler (and return) for ESP32-S2 or ESP32-S3
#if !TU_CHECK_MCU(OPT_MCU_ESP32S2, OPT_MCU_ESP32S3)
  vTaskStartScheduler();
#endif

  return 0;
}

#if TU_CHECK_MCU(OPT_MCU_ESP32S2, OPT_MCU_ESP32S3)
void app_main(void) {
  main();
}
#endif

// USB Host task
// This top level thread process all usb events and invoke callbacks
static void usb_host_task(void *param) {
  (void) param;

  // init host stack on configured roothub port
  tuh_init(BOARD_TUH_RHPORT);

  if (board_init_after_tusb) {
    board_init_after_tusb();
  }

#if CFG_TUH_ENABLED && CFG_TUH_MAX3421
  // FeatherWing MAX3421E use MAX3421E's GPIO0 for VBUS enable
  enum { IOPINS1_ADDR  = 20u << 3, /* 0xA0 */ };
  tuh_max3421_reg_write(BOARD_TUH_RHPORT, IOPINS1_ADDR, 0x01, false);
#endif

  cdc_app_init();
  hid_app_init();
  msc_app_init();

  // RTOS forever loop
  while (1) {
    // put this thread to waiting state until there is new events
    tuh_task();

    // following code only run if tuh_task() process at least 1 event
  }
}

//--------------------------------------------------------------------+
// TinyUSB Callbacks
//--------------------------------------------------------------------+

void tuh_mount_cb(uint8_t dev_addr) {
  // application set-up
  printf("A device with address %d is mounted\r\n", dev_addr);
}

void tuh_umount_cb(uint8_t dev_addr) {
  // application tear-down
  printf("A device with address %d is unmounted \r\n", dev_addr);
}

//--------------------------------------------------------------------+
// BLINKING TASK
//--------------------------------------------------------------------+
static void led_blinky_cb(TimerHandle_t xTimer) {
  (void) xTimer;
  static bool led_state = false;

  board_led_write(led_state);
  led_state = 1 - led_state; // toggle
}
