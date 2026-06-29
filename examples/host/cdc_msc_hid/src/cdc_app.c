/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2022, Ha Thach (tinyusb.org)
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

#include "tusb.h"
#include "bsp/board_api.h"
#include "app.h"

#if CFG_TUSB_OS == OPT_OS_FREERTOS
#ifdef ESP_PLATFORM
  #define CDC_STACK_SIZE      2048
#else
  #define CDC_STACK_SIZE     (3*configMINIMAL_STACK_SIZE/2)
#endif

#if configSUPPORT_STATIC_ALLOCATION
StackType_t  cdc_stack[CDC_STACK_SIZE];
StaticTask_t cdc_taskdef;
#endif

static void cdc_app_task(void* param);

void cdc_app_init(void) {
  #if configSUPPORT_STATIC_ALLOCATION
  (void) xTaskCreateStatic(cdc_app_task, "cdc", CDC_STACK_SIZE, NULL, configMAX_PRIORITIES-2, cdc_stack, &cdc_taskdef);
  #else
  (void) xTaskCreate(cdc_app_task, "cdc", CDC_STACK_SIZE, NULL, configMAX_PRIORITIES-2, NULL);
  #endif
}

static size_t console_read(uint8_t *buf, size_t bufsize) {
  size_t count = 0;
  while (count < bufsize) {
    int ch = board_getchar();
    if (ch <= 0) {
      break;
    }

    buf[count] = (uint8_t) ch;
    count++;
  }

  return count;
}

static void cdc_app_task(void* param) {
  (void) param;

  uint8_t buf[64 + 1]; // +1 for extra null character
  uint32_t const bufsize = sizeof(buf) - 1;

  while (1) {
    uint32_t count = console_read(buf, bufsize);
    buf[count] = 0;

    if (count) {
      // loop over all mounted interfaces
      for (uint8_t idx = 0; idx < CFG_TUH_CDC; idx++) {
        if (tuh_cdc_mounted(idx)) {
          // console --> cdc interfaces
          tuh_cdc_write(idx, buf, count);
          tuh_cdc_write_flush(idx);
        }
      }
    }

    vTaskDelay(1);
  }
}

// Invoked when received new data
void tuh_cdc_rx_cb(uint8_t idx) {
  uint8_t buf[64 + 1]; // +1 for extra null character
  uint32_t const bufsize = sizeof(buf) - 1;

  // forward cdc interfaces -> console
  uint32_t count = tuh_cdc_read(idx, buf, bufsize);
  buf[count] = 0;

  printf("%s", (char *) buf);
}

#else

static size_t console_read(uint8_t *buf, size_t bufsize) {
  size_t count = 0;
  while (count < bufsize) {
    const int ch = board_getchar();
    if (ch < 0) {
      break;
    }
    buf[count] = (uint8_t) ch;
    count++;
  }

  return count;
}

static size_t console_write(const uint8_t *buf, size_t bufsize) {
  // Use board_uart_write directly for non-blocking behavior.
  // board_putchar -> sys_write has a blocking retry loop that causes UART RX overrun.
  int wr = board_uart_write(buf, (int) bufsize);
  return (wr > 0) ? (size_t) wr : 0;
}

// forward from console to usbh
static void console_to_usbh(uint8_t idx) {
  uint8_t buf[64];
  size_t  count = console_read(buf, sizeof(buf));
  if (count > 0) {
    tuh_cdc_write(idx, buf, count);
  }
}

void cdc_app_task(void) {
  const uint8_t idx = 0;

  // Bidirectional forwarding: console <-> host cdc interfaces
  if (!tuh_cdc_mounted(idx)) {
    return;
  }

  // usbh -> uart
  uint8_t  buf[64];
  uint32_t count = tuh_cdc_read(idx, buf, sizeof(buf));
  uint32_t wr    = 0;

  do {
    // uart write is slow, while waiting forward uart -> usbh else uart rx can be overflow
    if (count) {
      wr += console_write(buf + wr, count - wr);
    }
    console_to_usbh(idx);
  } while (wr < count);

  tuh_cdc_write_flush(idx);
}
#endif

//--------------------------------------------------------------------+
// TinyUSB callbacks
//--------------------------------------------------------------------+

// Invoked when a device with CDC interface is mounted
// idx is index of cdc interface in the internal pool.
void tuh_cdc_mount_cb(uint8_t idx) {
  tuh_itf_info_t itf_info = {0};
  tuh_cdc_itf_get_info(idx, &itf_info);

  printf("CDC Interface is mounted: address = %u, itf_num = %u\r\n", itf_info.daddr,
         itf_info.desc.bInterfaceNumber);

  // If CFG_TUH_CDC_LINE_CODING_ON_ENUM is defined, line coding will be set by tinyusb stack
  // while enumerating new cdc device
  cdc_line_coding_t line_coding = {0};
  if (tuh_cdc_get_line_coding_local(idx, &line_coding)) {
    printf("  Baudrate: %" PRIu32 ", Stop Bits : %u\r\n", line_coding.bit_rate, line_coding.stop_bits);
    printf("  Parity  : %u, Data Width: %u\r\n", line_coding.parity, line_coding.data_bits);
  }
}

// Invoked when a device with CDC interface is unmounted
void tuh_cdc_umount_cb(uint8_t idx) {
  tuh_itf_info_t itf_info = {0};
  tuh_cdc_itf_get_info(idx, &itf_info);

  printf("CDC Interface is unmounted: address = %u, itf_num = %u\r\n", itf_info.daddr,
         itf_info.desc.bInterfaceNumber);
}
