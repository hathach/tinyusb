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

/* This example demonstrates dynamic switching between device and host modes:
 * - Press button to switch between device and host modes
 * - Device mode: CDC echo (echoes input back to output)
 * - Host mode: Prints connected device information
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "bsp/board_api.h"
#include "tusb.h"

#if CFG_TUSB_OS == OPT_OS_FREERTOS
  #ifdef ESP_PLATFORM
    #define USBD_STACK_SIZE     4096
    #define USBH_STACK_SIZE     4096
  #else
    // Increase stack size when debug log is enabled
    #define USBD_STACK_SIZE    (3*configMINIMAL_STACK_SIZE/2) * (CFG_TUSB_DEBUG ? 2 : 1)
    #define USBH_STACK_SIZE    (3*configMINIMAL_STACK_SIZE/2) * (CFG_TUSB_DEBUG ? 2 : 1)
  #endif

  #define CDC_STACK_SIZE      (configMINIMAL_STACK_SIZE * (CFG_TUSB_DEBUG ? 2 : 1))
  #define BLINKY_STACK_SIZE   configMINIMAL_STACK_SIZE
#endif

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF PROTOTYPES
//--------------------------------------------------------------------+

// English
#define LANGUAGE_ID 0x0409

/* Blink pattern
 * - 250 ms  : not mounted
 * - 1000 ms : mounted
 * - 2500 ms : suspended
 */
enum {
  BLINK_NOT_MOUNTED = 250,
  BLINK_MOUNTED = 1000,
  BLINK_SUSPENDED = 2500,
};

#if CFG_TUSB_OS == OPT_OS_FREERTOS
// static task for FreeRTOS
#if configSUPPORT_STATIC_ALLOCATION
StackType_t blinky_stack[BLINKY_STACK_SIZE];
StaticTask_t blinky_taskdef;

StackType_t usb_stack[USBD_STACK_SIZE > USBH_STACK_SIZE ? USBD_STACK_SIZE : USBH_STACK_SIZE];
StaticTask_t usb_taskdef;

StackType_t cdc_stack[CDC_STACK_SIZE];
StaticTask_t cdc_taskdef;
#endif
#endif

static uint32_t blink_interval_ms = BLINK_NOT_MOUNTED;
static tusb_role_t current_role = TUSB_ROLE_DEVICE;

#if CFG_TUSB_OS == OPT_OS_FREERTOS
static void usb_task(void *param);
void led_blinking_task(void *param);
void cdc_task(void *params);
#else
void led_blinking_task(void);
void cdc_task(void);
#endif
void usb_mode_switch(void);
static void print_device_info(uint8_t daddr);
static void print_utf16(uint16_t* temp_buf, size_t buf_len);

// Declare buffer for USB transfer
CFG_TUH_MEM_SECTION struct {
  TUH_EPBUF_TYPE_DEF(tusb_desc_device_t, device);
  TUH_EPBUF_DEF(serial, 64*sizeof(uint16_t));
  TUH_EPBUF_DEF(buf, 128*sizeof(uint16_t));
} desc;

//--------------------------------------------------------------------+
// Main
//--------------------------------------------------------------------+

int main(void) {
  board_init();

  printf("\r\n======================================\r\n");
  printf("TinyUSB Dynamic Switch Example\r\n");
  printf("Press button to switch between device and host modes\r\n");
  printf("Starting in DEVICE mode...\r\n");
  printf("======================================\r\n\r\n");

#if CFG_TUSB_OS == OPT_OS_FREERTOS
  // Create FreeRTOS tasks
#if configSUPPORT_STATIC_ALLOCATION
  xTaskCreateStatic(led_blinking_task, "blinky", BLINKY_STACK_SIZE, NULL, 1, blinky_stack, &blinky_taskdef);
  xTaskCreateStatic(usb_task, "usb", USBD_STACK_SIZE > USBH_STACK_SIZE ? USBD_STACK_SIZE : USBH_STACK_SIZE,
                    NULL, configMAX_PRIORITIES-1, usb_stack, &usb_taskdef);
  xTaskCreateStatic(cdc_task, "cdc", CDC_STACK_SIZE, NULL, configMAX_PRIORITIES - 2, cdc_stack, &cdc_taskdef);
#else
  xTaskCreate(led_blinking_task, "blinky", BLINKY_STACK_SIZE, NULL, 1, NULL);
  xTaskCreate(usb_task, "usb", USBD_STACK_SIZE > USBH_STACK_SIZE ? USBD_STACK_SIZE : USBH_STACK_SIZE,
              NULL, configMAX_PRIORITIES - 1, NULL);
  xTaskCreate(cdc_task, "cdc", CDC_STACK_SIZE, NULL, configMAX_PRIORITIES - 2, NULL);
#endif

#ifndef ESP_PLATFORM
  // only start scheduler for non-espressif mcu
  vTaskStartScheduler();
#endif

#else
  // Initialize in device mode by default
  tusb_rhport_init_t dev_init = {
    .role = TUSB_ROLE_DEVICE,
    .speed = TUSB_SPEED_AUTO
  };
  tusb_init(BOARD_RHPORT, &dev_init);
  current_role = TUSB_ROLE_DEVICE;

  board_init_after_tusb();

  while (1) {
    // Check for button press to switch modes
    static bool pending_switch = false;
    if (board_button_read()) {
      if (!pending_switch) {
        pending_switch = true;
        usb_mode_switch();
      }
    } else {
      pending_switch = false;
    }

    // Process USB tasks based on current mode
    if (current_role == TUSB_ROLE_DEVICE) {
      tud_task();
      cdc_task();
    } else {
      tuh_task();
    }

    led_blinking_task();
  }
#endif
}

#ifdef ESP_PLATFORM
void app_main(void) {
  main();
}
#endif

#if CFG_TUSB_OS == OPT_OS_FREERTOS
// USB Task for FreeRTOS
// This top level thread processes all usb events and mode switching
static void usb_task(void *param) {
  (void) param;

  // init device stack on configured roothub port
  // This should be called after scheduler/kernel is started.
  // Otherwise it could cause kernel issue since USB IRQ handler does use RTOS queue API.
  tusb_rhport_init_t dev_init = {
    .role = TUSB_ROLE_DEVICE,
    .speed = TUSB_SPEED_AUTO
  };
  tusb_init(BOARD_RHPORT, &dev_init);
  current_role = TUSB_ROLE_DEVICE;

  board_init_after_tusb();

  // RTOS forever loop
  while (1) {
    // Check for button press to switch modes
    static bool pending_switch = false;
    if (board_button_read()) {
      if (!pending_switch) {
        pending_switch = true;
        usb_mode_switch();
      }
    } else {
      pending_switch = false;
    }

    // Process USB tasks based on current mode
    // Use _ext version to allow return and read button state
    if (current_role == TUSB_ROLE_DEVICE) {
      tud_task_ext(10, false);
    } else {
      tuh_task_ext(10, false);
    }
  }
}
#endif

//--------------------------------------------------------------------+
// Mode Switching
//--------------------------------------------------------------------+

void usb_mode_switch(void) {
  printf("\r\n--- Switching USB mode ---\r\n");

  // Deinitialize current mode
  if (current_role == TUSB_ROLE_DEVICE) {
    printf("Stopping DEVICE mode...\r\n");
    tusb_deinit(BOARD_RHPORT);
  } else {
    printf("Stopping HOST mode...\r\n");
    tusb_deinit(BOARD_RHPORT);
  }

#if CFG_TUSB_OS == OPT_OS_FREERTOS
  vTaskDelay(pdMS_TO_TICKS(100)); // Small delay for clean transition
#else
  tusb_time_delay_ms_api(100); // Small delay for clean transition
#endif  // Switch to the other mode
  if (current_role == TUSB_ROLE_DEVICE) {
    printf("Starting HOST mode...\r\n");
    tusb_rhport_init_t host_init = {
      .role = TUSB_ROLE_HOST,
      .speed = TUSB_SPEED_AUTO
    };
    tusb_init(BOARD_RHPORT, &host_init);
    current_role = TUSB_ROLE_HOST;
  } else {
    printf("Starting DEVICE mode...\r\n");
    tusb_rhport_init_t dev_init = {
      .role = TUSB_ROLE_DEVICE,
      .speed = TUSB_SPEED_AUTO
    };
    tusb_init(BOARD_RHPORT, &dev_init);
    current_role = TUSB_ROLE_DEVICE;
  }

  blink_interval_ms = BLINK_NOT_MOUNTED;
  printf("Mode switch complete!\r\n\r\n");
}

//--------------------------------------------------------------------+
// Device Mode: CDC Task
//--------------------------------------------------------------------+

#if CFG_TUSB_OS == OPT_OS_FREERTOS
void cdc_task(void *params) {
  (void) params;

  // RTOS forever loop
  while (1) {
    // Only process CDC when in device mode
    if (current_role == TUSB_ROLE_DEVICE) {
      // Connected and there are data available
      while (tud_cdc_available()) {
        uint8_t buf[64];

        // Read data
        uint32_t count = tud_cdc_read(buf, sizeof(buf));

        // Echo back
        tud_cdc_write(buf, count);

        // Add newline for carriage return
        for (uint32_t i = 0; i < count; i++) {
          if (buf[i] == '\r') {
            tud_cdc_write_char('\n');
            break;
          }
        }
      }

      tud_cdc_write_flush();
    }

    vTaskDelay(pdMS_TO_TICKS(10));
  }
}
#else
void cdc_task(void) {
  // Connected and there are data available
  if (tud_cdc_available()) {
    uint8_t buf[64];

    // Read data
    uint32_t count = tud_cdc_read(buf, sizeof(buf));

    // Echo back
    for (uint32_t i = 0; i < count; i++) {
      tud_cdc_write_char(buf[i]);

      if (buf[i] == '\r') {
        tud_cdc_write_char('\n');
      }
    }

    tud_cdc_write_flush();
  }
}
#endif

//--------------------------------------------------------------------+
// Device Callbacks
//--------------------------------------------------------------------+

// Invoked when device is mounted
void tud_mount_cb(void) {
  printf("[DEVICE] Mounted\r\n");
  blink_interval_ms = BLINK_MOUNTED;
}

// Invoked when device is unmounted
void tud_umount_cb(void) {
  printf("[DEVICE] Unmounted\r\n");
  blink_interval_ms = BLINK_NOT_MOUNTED;
}

// Invoked when usb bus is suspended
void tud_suspend_cb(bool remote_wakeup_en) {
  (void) remote_wakeup_en;
  printf("[DEVICE] Suspended\r\n");
  blink_interval_ms = BLINK_SUSPENDED;
}

// Invoked when usb bus is resumed
void tud_resume_cb(void) {
  printf("[DEVICE] Resumed\r\n");
  blink_interval_ms = tud_mounted() ? BLINK_MOUNTED : BLINK_NOT_MOUNTED;
}

//--------------------------------------------------------------------+
// Host Callbacks
//--------------------------------------------------------------------+

// Invoked when device is mounted (configured)
void tuh_mount_cb(uint8_t daddr) {
  printf("[HOST] Device attached, address = %d\r\n", daddr);
  blink_interval_ms = BLINK_MOUNTED;
  print_device_info(daddr);
}

// Invoked when device is unmounted (unplugged)
void tuh_umount_cb(uint8_t daddr) {
  printf("[HOST] Device removed, address = %d\r\n", daddr);
  blink_interval_ms = BLINK_NOT_MOUNTED;
}

//--------------------------------------------------------------------+
// Host Device Info
//--------------------------------------------------------------------+

static void print_device_info(uint8_t daddr) {
  // Get Device Descriptor
  uint8_t xfer_result = tuh_descriptor_get_device_sync(daddr, &desc.device, 18);
  if (XFER_RESULT_SUCCESS != xfer_result) {
    printf("Failed to get device descriptor\r\n");
    return;
  }

  printf("Device %u: ID %04x:%04x SN ", daddr, desc.device.idVendor, desc.device.idProduct);

  xfer_result = XFER_RESULT_FAILED;
  if (desc.device.iSerialNumber != 0) {
    xfer_result = tuh_descriptor_get_serial_string_sync(daddr, LANGUAGE_ID, desc.serial, sizeof(desc.serial));
  }
  if (XFER_RESULT_SUCCESS != xfer_result) {
    uint16_t* serial = (uint16_t*)(uintptr_t) desc.serial;
    serial[0] = (uint16_t) ((TUSB_DESC_STRING << 8) | (2 * 3 + 2));
    serial[1] = 'n';
    serial[2] = '/';
    serial[3] = 'a';
    serial[4] = 0;
  }
  print_utf16((uint16_t*)(uintptr_t) desc.serial, sizeof(desc.serial)/2);
  printf("\r\n");

  printf("Device Descriptor:\r\n");
  printf("  bLength             %u\r\n", desc.device.bLength);
  printf("  bDescriptorType     %u\r\n", desc.device.bDescriptorType);
  printf("  bcdUSB              %04x\r\n", desc.device.bcdUSB);
  printf("  bDeviceClass        %u\r\n", desc.device.bDeviceClass);
  printf("  bDeviceSubClass     %u\r\n", desc.device.bDeviceSubClass);
  printf("  bDeviceProtocol     %u\r\n", desc.device.bDeviceProtocol);
  printf("  bMaxPacketSize0     %u\r\n", desc.device.bMaxPacketSize0);
  printf("  idVendor            0x%04x\r\n", desc.device.idVendor);
  printf("  idProduct           0x%04x\r\n", desc.device.idProduct);
  printf("  bcdDevice           %04x\r\n", desc.device.bcdDevice);

  // Get Manufacturer string
  if (desc.device.iManufacturer) {
    if (XFER_RESULT_SUCCESS == tuh_descriptor_get_manufacturer_string_sync(daddr, LANGUAGE_ID, desc.buf, sizeof(desc.buf))) {
      printf("  iManufacturer       %u     ", desc.device.iManufacturer);
      print_utf16((uint16_t*)(uintptr_t) desc.buf, sizeof(desc.buf)/2);
      printf("\r\n");
    }
  }

  // Get Product string
  if (desc.device.iProduct) {
    if (XFER_RESULT_SUCCESS == tuh_descriptor_get_product_string_sync(daddr, LANGUAGE_ID, desc.buf, sizeof(desc.buf))) {
      printf("  iProduct            %u     ", desc.device.iProduct);
      print_utf16((uint16_t*)(uintptr_t) desc.buf, sizeof(desc.buf)/2);
      printf("\r\n");
    }
  }

  // Get Serial string
  if (desc.device.iSerialNumber) {
    printf("  iSerialNumber       %u     ", desc.device.iSerialNumber);
    print_utf16((uint16_t*)(uintptr_t) desc.serial, sizeof(desc.serial)/2);
    printf("\r\n");
  } else {
    printf("  iSerialNumber       0\r\n");
  }

  printf("  bNumConfigurations  %u\r\n", desc.device.bNumConfigurations);
  printf("\r\n");
}

static void print_utf16(uint16_t* temp_buf, size_t buf_len) {
  if (temp_buf[0] == 0 || (temp_buf[0] >> 8) != TUSB_DESC_STRING) {
    printf("(invalid)");
    return;
  }

  size_t chr_count = (temp_buf[0] & 0xff) / 2 - 1;
  if (chr_count > buf_len - 1) {
    chr_count = buf_len - 1;
  }

  for (size_t i = 0; i < chr_count; i++) {
    uint16_t ch = temp_buf[1 + i];
    if (ch <= 0x7F) {
      putchar((char) ch);
    } else {
      // TODO support UTF16 to UTF8 conversion
      putchar('?');
    }
  }
}

//--------------------------------------------------------------------+
// Blinking Task
//--------------------------------------------------------------------+

#if CFG_TUSB_OS == OPT_OS_FREERTOS
void led_blinking_task(void *param) {
  (void) param;
  static bool led_state = false;

  // RTOS forever loop
  while (1) {
    board_led_write(led_state);
    led_state = 1 - led_state; // toggle
    vTaskDelay(pdMS_TO_TICKS(blink_interval_ms));
  }
}
#else
void led_blinking_task(void) {
  static uint32_t start_ms = 0;
  static bool led_state = false;

  // Blink every interval ms
  if (board_millis() - start_ms < blink_interval_ms) return; // not enough time
  start_ms += blink_interval_ms;

  board_led_write(led_state);
  led_state = 1 - led_state; // toggle
}
#endif
