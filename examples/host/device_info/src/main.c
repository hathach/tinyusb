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

/* Host example will get device descriptors of attached devices and print it out via uart/rtt (logger) as follows:
 *    Device 1: ID 046d:c52f SN 11223344
      Device Descriptor:
        bLength             18
        bDescriptorType     1
        bcdUSB              0200
        bDeviceClass        0
        bDeviceSubClass     0
        bDeviceProtocol     0
        bMaxPacketSize0     8
        idVendor            0x046d
        idProduct           0xc52f
        bcdDevice           2200
        iManufacturer       1     Logitech
        iProduct            2     USB Receiver
        iSerialNumber       0
        bNumConfigurations  1
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "bsp/board_api.h"
#include "tusb.h"

// English
#define LANGUAGE_ID 0x0409

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF PROTYPES
//--------------------------------------------------------------------+
enum {
  BLINK_NOT_MOUNTED = 250,
  BLINK_MOUNTED = 1000,
  BLINK_SUSPENDED = 2500,
};
static uint32_t blink_interval_ms = BLINK_NOT_MOUNTED;

// Declare for buffer for usb transfer, may need to be in USB/DMA section and
// multiple of dcache line size if dcache is enabled (for some ports).
CFG_TUH_MEM_SECTION struct {
  TUH_EPBUF_TYPE_DEF(tusb_desc_device_t, device);
  TUH_EPBUF_DEF(serial, 64*sizeof(uint16_t));
  TUH_EPBUF_DEF(buf, 128*sizeof(uint16_t));
} desc;

void led_blinking_task(void* param);
static void print_utf16(uint16_t* temp_buf, size_t buf_len);

#if CFG_TUSB_OS == OPT_OS_FREERTOS
void init_freertos_task(void);
#endif

//--------------------------------------------------------------------
// Main
//--------------------------------------------------------------------
static void init_tinyusb(void) {
  // init host stack on configured roothub port
  tusb_rhport_init_t host_init = {
    .role = TUSB_ROLE_HOST,
    .speed = TUSB_SPEED_AUTO
  };
  tusb_init(BOARD_TUH_RHPORT, &host_init);

  if (board_init_after_tusb) {
    board_init_after_tusb();
  }
}

int main(void) {
  board_init();
  printf("TinyUSB Device Info Example\r\n");

#if CFG_TUSB_OS == OPT_OS_FREERTOS
  init_freertos_task();
#else
  init_tinyusb();
  while (1) {
    tuh_task();     // tinyusb host task
    led_blinking_task(NULL);
  }
  return 0;
#endif
}

/*------------- TinyUSB Callbacks -------------*/

// Invoked when device is mounted (configured)
void tuh_mount_cb(uint8_t daddr) {
  blink_interval_ms = BLINK_MOUNTED;

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

  // Get String descriptor using Sync API

  printf("  iManufacturer       %u     ", desc.device.iManufacturer);
  if (desc.device.iManufacturer != 0) {
    xfer_result = tuh_descriptor_get_manufacturer_string_sync(daddr, LANGUAGE_ID, desc.buf, sizeof(desc.buf));
    if (XFER_RESULT_SUCCESS == xfer_result) {
      print_utf16((uint16_t*)(uintptr_t) desc.buf, sizeof(desc.buf)/2);
    }
  }
  printf("\r\n");

  printf("  iProduct            %u     ", desc.device.iProduct);
  if (desc.device.iProduct != 0) {
    xfer_result = tuh_descriptor_get_product_string_sync(daddr, LANGUAGE_ID, desc.buf, sizeof(desc.buf));
    if (XFER_RESULT_SUCCESS == xfer_result) {
      print_utf16((uint16_t*)(uintptr_t) desc.buf, sizeof(desc.buf)/2);
    }
  }
  printf("\r\n");

  printf("  iSerialNumber       %u     ", desc.device.iSerialNumber);
  printf((char*)desc.serial); // serial is already to UTF-8
  printf("\r\n");

  printf("  bNumConfigurations  %u\r\n", desc.device.bNumConfigurations);
}

// Invoked when device is unmounted (bus reset/unplugged)
void tuh_umount_cb(uint8_t daddr) {
  blink_interval_ms = BLINK_NOT_MOUNTED;
  printf("Device removed, address = %d\r\n", daddr);
}

//--------------------------------------------------------------------+
// String Descriptor Helper
//--------------------------------------------------------------------+

static void _convert_utf16le_to_utf8(const uint16_t* utf16, size_t utf16_len, uint8_t* utf8, size_t utf8_len) {
  // TODO: Check for runover.
  (void) utf8_len;
  // Get the UTF-16 length out of the data itself.

  for (size_t i = 0; i < utf16_len; i++) {
    uint16_t chr = utf16[i];
    if (chr < 0x80) {
      *utf8++ = chr & 0xffu;
    } else if (chr < 0x800) {
      *utf8++ = (uint8_t) (0xC0 | (chr >> 6 & 0x1F));
      *utf8++ = (uint8_t) (0x80 | (chr >> 0 & 0x3F));
    } else {
      // TODO: Verify surrogate.
      *utf8++ = (uint8_t) (0xE0 | (chr >> 12 & 0x0F));
      *utf8++ = (uint8_t) (0x80 | (chr >> 6 & 0x3F));
      *utf8++ = (uint8_t) (0x80 | (chr >> 0 & 0x3F));
    }
    // TODO: Handle UTF-16 code points that take two entries.
  }
}

// Count how many bytes a utf-16-le encoded string will take in utf-8.
static int _count_utf8_bytes(const uint16_t* buf, size_t len) {
  size_t total_bytes = 0;
  for (size_t i = 0; i < len; i++) {
    uint16_t chr = buf[i];
    if (chr < 0x80) {
      total_bytes += 1;
    } else if (chr < 0x800) {
      total_bytes += 2;
    } else {
      total_bytes += 3;
    }
    // TODO: Handle UTF-16 code points that take two entries.
  }
  return (int) total_bytes;
}

static void print_utf16(uint16_t* temp_buf, size_t buf_len) {
  if ((temp_buf[0] & 0xff) == 0) return;  // empty
  size_t utf16_len = ((temp_buf[0] & 0xff) - 2) / sizeof(uint16_t);
  size_t utf8_len = (size_t) _count_utf8_bytes(temp_buf + 1, utf16_len);
  _convert_utf16le_to_utf8(temp_buf + 1, utf16_len, (uint8_t*) temp_buf, sizeof(uint16_t) * buf_len);
  ((uint8_t*) temp_buf)[utf8_len] = '\0';

  printf("%s", (char*) temp_buf);
}

//--------------------------------------------------------------------+
// Blinking Task
//--------------------------------------------------------------------+
void led_blinking_task(void* param) {
  (void) param;
  static uint32_t start_ms = 0;
  static bool led_state = false;

  while (1) {
#if CFG_TUSB_OS == OPT_OS_FREERTOS
    vTaskDelay(blink_interval_ms / portTICK_PERIOD_MS);
#else
    if (board_millis() - start_ms < blink_interval_ms) {
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

#define BLINKY_STACK_SIZE   configMINIMAL_STACK_SIZE

#if TUSB_MCU_VENDOR_ESPRESSIF
  #define USB_STACK_SIZE     4096
#else
  // Increase stack size when debug log is enabled
  #define USB_STACK_SIZE    (3*configMINIMAL_STACK_SIZE/2) * (CFG_TUSB_DEBUG ? 2 : 1)
#endif


// static task
#if configSUPPORT_STATIC_ALLOCATION
StackType_t blinky_stack[BLINKY_STACK_SIZE];
StaticTask_t blinky_taskdef;

StackType_t  usb_stack[USB_STACK_SIZE];
StaticTask_t usb_taskdef;
#endif

#if TUSB_MCU_VENDOR_ESPRESSIF
void app_main(void) {
  main();
}
#endif

void usb_host_task(void *param) {
  (void) param;
  init_tinyusb();
  while (1) {
    tuh_task();
  }
}

void init_freertos_task(void) {
#if configSUPPORT_STATIC_ALLOCATION
  xTaskCreateStatic(led_blinking_task, "blinky", BLINKY_STACK_SIZE, NULL, 1, blinky_stack, &blinky_taskdef);
  xTaskCreateStatic(usb_host_task, "usbh", USB_STACK_SIZE, NULL, configMAX_PRIORITIES-1, usb_stack, &usb_taskdef);
#else
  xTaskCreate(led_blinking_task, "blinky", BLINKY_STACK_SIZE, NULL, 1, NULL);
  xTaskCreate(usb_host_task, "usbh", USB_STACK_SIZE, NULL, configMAX_PRIORITIES - 1, NULL);
#endif

  // skip starting scheduler (and return) for ESP32-S2 or ESP32-S3
#if !TUSB_MCU_VENDOR_ESPRESSIF
  vTaskStartScheduler();
#endif
}

#endif
