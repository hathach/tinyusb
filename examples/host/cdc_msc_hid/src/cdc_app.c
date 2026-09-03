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

// Give up forwarding to the console after this long without progress. Not every board's
// board_uart_write() reports "no console here" the same way, so the loop below cannot rely
// on the return value alone to know when to stop.
enum { CONSOLE_STALL_MS = 10 };

// Returns bytes written, 0 if the console cannot take them right now, negative if this
// board has no console at all.
static int console_write(const uint8_t *buf, size_t bufsize) {
#if defined(LOGGER_RTT)
  // The console is the debug probe, not a UART: board_uart_write() is a stub on those
  // boards. NO_BLOCK_SKIP writes all or nothing, so 0 means the probe has not drained the
  // up-buffer yet.
  return (int) SEGGER_RTT_Write(0, buf, (unsigned) bufsize);
#else
  // Use board_uart_write directly for non-blocking behavior.
  // board_putchar -> sys_write has a blocking retry loop that causes UART RX overrun.
  return board_uart_write(buf, (int) bufsize);
#endif
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
  uint32_t progress_ms = tusb_time_millis_api();

  do {
    // console write is slow, while waiting forward console -> usbh else its rx can overflow
    if (count) {
      const int written = console_write(buf + wr, count - wr);
      if (written < 0) {
        break; // no console on this board at all
      }
      if (written > 0) {
        wr += (uint32_t) written;
        progress_ms = tusb_time_millis_api();
      } else if (tusb_time_millis_api() - progress_ms > CONSOLE_STALL_MS) {
        // a TX FIFO or an RTT up-buffer is only ever transiently full: nothing draining it
        // must not stall the host task, so drop the rest
        break;
      }
    }
    console_to_usbh(idx);
  } while (wr < count);

  tuh_cdc_write_flush(idx);
}

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
  // while eneumerating new cdc device
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
