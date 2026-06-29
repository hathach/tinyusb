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
  BLINK_MOUNTED     = 1000,
  BLINK_SUSPENDED   = 2500,
};

static uint32_t blink_interval_ms = BLINK_NOT_MOUNTED;
static bool     blink_enable      = true;

void led_blinking_task(void *param);
void cdc_task(void *param);
extern void msc_disk_init(void);

#if CFG_TUSB_OS == OPT_OS_FREERTOS
static void usb_device_task(void *param);
static void freertos_init(void);
#endif

/*------------- MAIN -------------*/
int main(void) {
  board_init();

#if CFG_TUSB_OS == OPT_OS_FREERTOS
  freertos_init();
#else
  // init device stack on configured roothub port
  tusb_rhport_init_t dev_init = {.role = TUSB_ROLE_DEVICE, .speed = TUSB_SPEED_AUTO};
  tusb_init(BOARD_TUD_RHPORT, &dev_init);

  board_init_after_tusb();
  msc_disk_init();

  while (1) {
    tud_task(); // tinyusb device task
    led_blinking_task(NULL);

    cdc_task(NULL);
  }
#endif

  return 0;
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
  (void)remote_wakeup_en;
  blink_interval_ms = BLINK_SUSPENDED;
}

// Invoked when usb bus is resumed
void tud_resume_cb(void) {
  blink_interval_ms = tud_mounted() ? BLINK_MOUNTED : BLINK_NOT_MOUNTED;
}


//--------------------------------------------------------------------+
// USB CDC
//--------------------------------------------------------------------+
void cdc_task(void *param) {
  (void) param;

  while (1) {
    // connected() check for DTR bit
    // Most but not all terminal client set this when making connection
    // if ( tud_cdc_connected() )
    {
      // connected and there are data available
      while (tud_cdc_available()) {
        // read data
        char     buf[64];
        uint32_t count = tud_cdc_read(buf, sizeof(buf));
        (void)count;

        // Echo back
        // Note: Skip echo by commenting out write() and write_flush()
        // for throughput test e.g
        //    $ dd if=/dev/zero of=/dev/ttyACM0 count=10000
        tud_cdc_write(buf, count);
      }

      tud_cdc_write_flush();

      // Press on-board button to send Uart status notification
      static cdc_notify_uart_state_t uart_state = {.value = 0};

      static uint32_t btn_prev = 0;
      const uint32_t  btn      = board_button_read();

      if ((btn_prev == 0u) && (btn != 0u)) {
        uart_state.dsr ^= 1;
        uart_state.dcd ^= 1;
        tud_cdc_notify_uart_state(&uart_state);
      }
      btn_prev = btn;
    }

#if CFG_TUSB_OS == OPT_OS_FREERTOS
    vTaskDelay(1);
#else
    break;
#endif
  }
}

// Invoked when cdc when line state changed e.g connected/disconnected
void tud_cdc_line_state_cb(uint8_t itf, bool dtr, bool rts) {
  (void)itf;
  (void)rts;

  if (dtr) {
    // Terminal connected
    blink_enable = false;
    board_led_write(true);
  } else {
    // Terminal disconnected
    blink_enable = true;
  }
}

// Invoked when CDC interface received data from host
void tud_cdc_rx_cb(uint8_t itf) {
  (void)itf;
}

//--------------------------------------------------------------------+
// BLINKING TASK
//--------------------------------------------------------------------+
void led_blinking_task(void *param) {
  (void) param;
  static bool     led_state = false;
#if CFG_TUSB_OS != OPT_OS_FREERTOS
  static uint32_t start_ms  = 0;
#endif

  while (1) {
    if (!blink_enable) {
#if CFG_TUSB_OS == OPT_OS_FREERTOS
      vTaskDelay(1);
      continue;
#else
      return;
#endif
    }

#if CFG_TUSB_OS == OPT_OS_FREERTOS
    vTaskDelay(blink_interval_ms / portTICK_PERIOD_MS);
#else
    // Blink every interval ms
    if (tusb_time_millis_api() - start_ms < blink_interval_ms) {
      return; // not enough time
    }
    start_ms += blink_interval_ms;
#endif

    board_led_write(led_state);
    led_state = !led_state;

#if CFG_TUSB_OS != OPT_OS_FREERTOS
    break;
#endif
  }
}

//--------------------------------------------------------------------+
// FreeRTOS
//--------------------------------------------------------------------+
#if CFG_TUSB_OS == OPT_OS_FREERTOS

#ifdef ESP_PLATFORM
  #define USBD_STACK_SIZE     4096

void app_main(void) {
  main();
}
#else
  // Increase stack size when debug log is enabled
  #define USBD_STACK_SIZE     (configMINIMAL_STACK_SIZE * (CFG_TUSB_DEBUG ? 4 : 2))
#endif

#define CDC_STACK_SIZE      (configMINIMAL_STACK_SIZE * (CFG_TUSB_DEBUG ? 3 : 2))
#define BLINKY_STACK_SIZE   configMINIMAL_STACK_SIZE

#if configSUPPORT_STATIC_ALLOCATION
StackType_t blinky_stack[BLINKY_STACK_SIZE];
StaticTask_t blinky_taskdef;

StackType_t  usb_device_stack[USBD_STACK_SIZE];
StaticTask_t usb_device_taskdef;

StackType_t  cdc_stack[CDC_STACK_SIZE];
StaticTask_t cdc_taskdef;
#endif

// USB Device Driver task
// This top level thread processes all USB events and invokes callbacks.
static void usb_device_task(void *param) {
  (void) param;

  // init device stack on configured roothub port.
  // This should be called after scheduler/kernel is started.
  // Otherwise, it could cause kernel issue since USB IRQ handler uses RTOS queue API.
  tusb_rhport_init_t dev_init = {
    .role = TUSB_ROLE_DEVICE,
    .speed = TUSB_SPEED_AUTO
  };
  tusb_init(BOARD_TUD_RHPORT, &dev_init);

  board_init_after_tusb();
  msc_disk_init();

  while (1) {
    tud_task();// tinyusb device task
    tud_cdc_write_flush();
  }
}

static void freertos_init(void) {
#if configSUPPORT_STATIC_ALLOCATION
  xTaskCreateStatic(led_blinking_task, "blinky", BLINKY_STACK_SIZE, NULL, 1, blinky_stack, &blinky_taskdef);
  xTaskCreateStatic(usb_device_task, "usbd", USBD_STACK_SIZE, NULL, configMAX_PRIORITIES - 1, usb_device_stack, &usb_device_taskdef);
  xTaskCreateStatic(cdc_task, "cdc", CDC_STACK_SIZE, NULL, configMAX_PRIORITIES - 2, cdc_stack, &cdc_taskdef);
#else
  xTaskCreate(led_blinking_task, "blinky", BLINKY_STACK_SIZE, NULL, 1, NULL);
  xTaskCreate(usb_device_task, "usbd", USBD_STACK_SIZE, NULL, configMAX_PRIORITIES - 1, NULL);
  xTaskCreate(cdc_task, "cdc", CDC_STACK_SIZE, NULL, configMAX_PRIORITIES - 2, NULL);
#endif

#ifndef ESP_PLATFORM
  vTaskStartScheduler();
#endif
}

#endif
