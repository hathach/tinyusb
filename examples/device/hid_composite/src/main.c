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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bsp/board_api.h"
#include "tusb.h"

#include "usb_descriptors.h"

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

void led_blinking_task(void *param);
void hid_task(void *param);

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

  while (1) {
    tud_task(); // tinyusb device task
    led_blinking_task(NULL);
    hid_task(NULL);
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
// USB HID
//--------------------------------------------------------------------+

static void send_hid_report(uint8_t report_id, uint32_t btn) {
  // skip if hid is not ready yet
  if (!tud_hid_ready()) {
    return;
  }

  switch (report_id) {
    case REPORT_ID_KEYBOARD: {
      // use to avoid send multiple consecutive zero report for keyboard
      static bool has_keyboard_key = false;

      if (btn != 0u) {
        uint8_t keycode[6] = {0};
        keycode[0]         = HID_KEY_A;

        tud_hid_keyboard_report(REPORT_ID_KEYBOARD, 0, keycode);
        has_keyboard_key = true;
      } else {
        // send empty key report if previously has key pressed
        if (has_keyboard_key) {
          tud_hid_keyboard_report(REPORT_ID_KEYBOARD, 0, NULL);
        }
        has_keyboard_key = false;
      }
      break;
    }

    case REPORT_ID_MOUSE: {
      int8_t const delta = 5;

      // no button, right + down, no scroll, no pan
      tud_hid_mouse_report(REPORT_ID_MOUSE, 0x00, delta, delta, 0, 0);
      break;
    }

    case REPORT_ID_CONSUMER_CONTROL: {
      // use to avoid send multiple consecutive zero report
      static bool has_consumer_key = false;

      if (btn != 0u) {
        // volume down
        uint16_t volume_down = HID_USAGE_CONSUMER_VOLUME_DECREMENT;
        tud_hid_report(REPORT_ID_CONSUMER_CONTROL, &volume_down, 2);
        has_consumer_key = true;
      } else {
        // send empty key report (release key) if previously has key pressed
        uint16_t empty_key = 0;
        if (has_consumer_key) {
          tud_hid_report(REPORT_ID_CONSUMER_CONTROL, &empty_key, 2);
        }
        has_consumer_key = false;
      }
      break;
    }

    case REPORT_ID_GAMEPAD: {
      // use to avoid send multiple consecutive zero report for keyboard
      static bool has_gamepad_key = false;

      hid_gamepad_report_t report = {.x = 0, .y = 0, .z = 0, .rz = 0, .rx = 0, .ry = 0, .hat = 0, .buttons = 0};

      if (btn != 0u) {
        report.hat     = GAMEPAD_HAT_UP;
        report.buttons = GAMEPAD_BUTTON_A;
        tud_hid_report(REPORT_ID_GAMEPAD, &report, sizeof(report));

        has_gamepad_key = true;
      } else {
        report.hat     = GAMEPAD_HAT_CENTERED;
        report.buttons = 0;
        if (has_gamepad_key) {
          tud_hid_report(REPORT_ID_GAMEPAD, &report, sizeof(report));
        }
        has_gamepad_key = false;
      }
      break;
    }

    case REPORT_ID_STYLUS_PEN: {
      static bool         touch_state = false;
      hid_stylus_report_t report      = {.attr = 0, .x = 0, .y = 0};

      if (btn != 0u) {
        report.attr = STYLUS_ATTR_TIP_SWITCH | STYLUS_ATTR_IN_RANGE;
        report.x    = 100;
        report.y    = 100;
        tud_hid_report(REPORT_ID_STYLUS_PEN, &report, sizeof(report));
        touch_state = true;
      } else {
        report.attr = 0;
        if (touch_state) {
          tud_hid_report(REPORT_ID_STYLUS_PEN, &report, sizeof(report));
        }
        touch_state = false;
      }
      break;
    }

    default: break; // unknown report id
  }
}

// Every 10ms, we will sent 1 report for each HID profile (keyboard, mouse etc ..)
// tud_hid_report_complete_cb() is used to send the next report after previous one is complete
void hid_task(void *param) {
  (void) param;

  while (1) {
#if CFG_TUSB_OS == OPT_OS_FREERTOS
    vTaskDelay(pdMS_TO_TICKS(10));
#else
    // Poll every 10ms
    const uint32_t  interval_ms = 10;
    static uint32_t start_ms    = 0;

    if (tusb_time_millis_api() - start_ms < interval_ms) {
      return; // not enough time
    }
    start_ms += interval_ms;
#endif

    uint32_t const btn = board_button_read();

    // Remote wakeup
    if (tud_suspended() && btn != 0u) {
      // Wake up host if we are in suspend mode
      // and REMOTE_WAKEUP feature is enabled by host
      tud_remote_wakeup();
    } else {
      // Send the 1st of report chain, the rest will be sent by tud_hid_report_complete_cb()
      send_hid_report(REPORT_ID_KEYBOARD, btn);
    }

#if CFG_TUSB_OS != OPT_OS_FREERTOS
    break;
#endif
  }
}

// Invoked when sent REPORT successfully to host
// Application can use this to send the next report
// Note: For composite reports, report[0] is report ID
void tud_hid_report_complete_cb(uint8_t instance, uint8_t const *report, uint16_t len) {
  (void)instance;
  (void)len;

  uint8_t next_report_id = report[0] + 1u;

  if (next_report_id < REPORT_ID_COUNT) {
    send_hid_report(next_report_id, board_button_read());
  }
}

// Invoked when received GET_REPORT control request
// Application must fill buffer report's content and return its length.
// Return zero will cause the stack to STALL request
uint16_t tud_hid_get_report_cb(
    uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t *buffer, uint16_t reqlen) {
  // TODO not Implemented
  (void)instance;
  (void)report_id;
  (void)report_type;
  (void)buffer;
  (void)reqlen;

  return 0;
}

// Invoked when received SET_REPORT control request or
// received data on OUT endpoint ( Report ID = 0, Type = 0 )
void tud_hid_set_report_cb(
    uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t const *buffer, uint16_t bufsize) {
  (void)instance;

  if (report_type == HID_REPORT_TYPE_OUTPUT) {
    // Set keyboard LED e.g Capslock, Numlock etc...
    if (report_id == REPORT_ID_KEYBOARD) {
      // bufsize should be (at least) 1
      if (bufsize < 1) {
        return;
      }

      uint8_t const kbd_leds = buffer[0];

      if ((kbd_leds & KEYBOARD_LED_CAPSLOCK) != 0u) {
        // Capslock On: disable blink, turn led on
        blink_interval_ms = 0;
        board_led_write(true);
      } else {
        // Caplocks Off: back to normal blink
        board_led_write(false);
        blink_interval_ms = BLINK_MOUNTED;
      }
    }
  }
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
    // blink is disabled
    if (0u == blink_interval_ms) {
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
    led_state = 1 - led_state; // toggle

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
  #define USBD_STACK_SIZE     ((3 * configMINIMAL_STACK_SIZE / 2) * (CFG_TUSB_DEBUG ? 2 : 1))
#endif

#define HID_STACK_SIZE      (configMINIMAL_STACK_SIZE * (CFG_TUSB_DEBUG ? 2 : 1))
#define BLINKY_STACK_SIZE   configMINIMAL_STACK_SIZE

#if configSUPPORT_STATIC_ALLOCATION
StackType_t blinky_stack[BLINKY_STACK_SIZE];
StaticTask_t blinky_taskdef;

StackType_t  usb_device_stack[USBD_STACK_SIZE];
StaticTask_t usb_device_taskdef;

StackType_t  hid_stack[HID_STACK_SIZE];
StaticTask_t hid_taskdef;
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

  while (1) {
    tud_task();// tinyusb device task
  }
}

static void freertos_init(void) {
#if configSUPPORT_STATIC_ALLOCATION
  xTaskCreateStatic(led_blinking_task, "blinky", BLINKY_STACK_SIZE, NULL, 1, blinky_stack, &blinky_taskdef);
  xTaskCreateStatic(usb_device_task, "usbd", USBD_STACK_SIZE, NULL, configMAX_PRIORITIES - 1, usb_device_stack, &usb_device_taskdef);
  xTaskCreateStatic(hid_task, "hid", HID_STACK_SIZE, NULL, configMAX_PRIORITIES - 2, hid_stack, &hid_taskdef);
#else
  xTaskCreate(led_blinking_task, "blinky", BLINKY_STACK_SIZE, NULL, 1, NULL);
  xTaskCreate(usb_device_task, "usbd", USBD_STACK_SIZE, NULL, configMAX_PRIORITIES - 1, NULL);
  xTaskCreate(hid_task, "hid", HID_STACK_SIZE, NULL, configMAX_PRIORITIES - 2, NULL);
#endif

#ifndef ESP_PLATFORM
  vTaskStartScheduler();
#endif
}

#endif
