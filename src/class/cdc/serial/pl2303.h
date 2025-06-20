/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2024 Heiko Kuester
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

#ifndef TUSB_PL2303_H
#define TUSB_PL2303_H

#include <stdbool.h>
#include <stdint.h>

// There is no official documentation for the PL2303 chips.
// Reference can be found
// - https://github.com/torvalds/linux/blob/master/drivers/usb/serial/pl2303.h and
//   https://github.com/torvalds/linux/blob/master/drivers/usb/serial/pl2303.c
// - https://github.com/freebsd/freebsd-src/blob/main/sys/dev/usb/serial/uplcom.c

// quirks
#define PL2303_QUIRK_UART_STATE_IDX0      1
#define PL2303_QUIRK_LEGACY               2
#define PL2303_QUIRK_ENDPOINT_HACK        4

// requests and bits
#define PL2303_SET_LINE_REQUEST_TYPE      0x21    // class request host to device interface
#define PL2303_SET_LINE_REQUEST           0x20    // dec 32

#define PL2303_SET_CONTROL_REQUEST_TYPE   0x21    // class request host to device interface
#define PL2303_SET_CONTROL_REQUEST        0x22    // dec 34
#define PL2303_CONTROL_DTR                0x01    // dec 1
#define PL2303_CONTROL_RTS                0x02    // dec 2

#define PL2303_BREAK_REQUEST_TYPE         0x21    // class request host to device interface
#define PL2303_BREAK_REQUEST              0x23    // dec 35
#define PL2303_BREAK_ON                   0xffff
#define PL2303_BREAK_OFF                  0x0000

#define PL2303_GET_LINE_REQUEST_TYPE      0xa1    // class request device to host interface
#define PL2303_GET_LINE_REQUEST           0x21    // dec 33

#define PL2303_VENDOR_WRITE_REQUEST_TYPE  0x40    // vendor request host to device interface
#define PL2303_VENDOR_WRITE_REQUEST       0x01    // dec 1
#define PL2303_VENDOR_WRITE_NREQUEST      0x80    // dec 128

#define PL2303_VENDOR_READ_REQUEST_TYPE   0xc0    // vendor request device to host interface
#define PL2303_VENDOR_READ_REQUEST        0x01    // dec 1
#define PL2303_VENDOR_READ_NREQUEST       0x81    // dec 129

#define PL2303_UART_STATE_INDEX           8
#define PL2303_UART_STATE_MSR_MASK        0x8b
#define PL2303_UART_STATE_TRANSIENT_MASK  0x74
#define PL2303_UART_DCD                   0x01
#define PL2303_UART_DSR                   0x02
#define PL2303_UART_BREAK_ERROR           0x04
#define PL2303_UART_RING                  0x08
#define PL2303_UART_FRAME_ERROR           0x10
#define PL2303_UART_PARITY_ERROR          0x20
#define PL2303_UART_OVERRUN_ERROR         0x40
#define PL2303_UART_CTS                   0x80

#define PL2303_FLOWCTRL_MASK              0xf0

#define PL2303_CLEAR_HALT_REQUEST_TYPE    0x02    // standard request host to device endpoint

// registers via vendor read/write requests
#define PL2303_READ_TYPE_HX_STATUS        0x8080

#define PL2303_HXN_RESET_REG              0x07
#define PL2303_HXN_RESET_UPSTREAM_PIPE    0x02
#define PL2303_HXN_RESET_DOWNSTREAM_PIPE  0x01

#define PL2303_HXN_FLOWCTRL_REG           0x0a
#define PL2303_HXN_FLOWCTRL_MASK          0x1c
#define PL2303_HXN_FLOWCTRL_NONE          0x1c
#define PL2303_HXN_FLOWCTRL_RTS_CTS       0x18
#define PL2303_HXN_FLOWCTRL_XON_XOFF      0x0c

// type data
typedef enum pl2303_type {
  PL2303_TYPE_H = 0, // 0
  PL2303_TYPE_HX,    // 1
  PL2303_TYPE_TA,    // 2
  PL2303_TYPE_TB,    // 3
  PL2303_TYPE_HXD,   // 4
  PL2303_TYPE_HXN,   // 5
  PL2303_TYPE_COUNT,
  PL2303_TYPE_NEED_SUPPORTS_HX_STATUS,
  PL2303_TYPE_UNKNOWN,
} pl2303_type_t;

typedef struct pl2303_type_data {
  uint32_t max_baud_rate;
  uint8_t  quirks;
  uint8_t  no_autoxonxoff : 1;
  uint8_t  no_divisors    : 1;
  uint8_t  alt_divisors   : 1;
} pl2303_type_data_t;

#define PL2303_TYPE_DATA \
  [PL2303_TYPE_H] = { \
    .max_baud_rate = 1228800, .quirks = PL2303_QUIRK_LEGACY, \
    .no_autoxonxoff = 1, .no_divisors = 0, .alt_divisors = 0 \
  }, \
  [PL2303_TYPE_HX] = { \
    .max_baud_rate = 6000000, .quirks = 0, \
    .no_autoxonxoff = 0, .no_divisors = 0, .alt_divisors = 0 \
  }, \
  [PL2303_TYPE_TA] = { \
    .max_baud_rate = 6000000, .quirks = 0, \
    .no_autoxonxoff = 0, .no_divisors = 0, .alt_divisors = 1 \
  }, \
  [PL2303_TYPE_TB] = { \
    .max_baud_rate = 12000000, .quirks = 0, \
    .no_autoxonxoff = 0, .no_divisors = 0, .alt_divisors = 1 \
  }, \
  [PL2303_TYPE_HXD] = { \
    .max_baud_rate = 12000000, .quirks = 0, \
    .no_autoxonxoff = 0, .no_divisors = 0, .alt_divisors = 0 \
  }, \
  [PL2303_TYPE_HXN] = { \
    .max_baud_rate = 12000000, .quirks = 0, \
    .no_autoxonxoff = 0, .no_divisors = 1, .alt_divisors = 0 \
  }

typedef struct TU_ATTR_PACKED {
  pl2303_type_t type;
  uint8_t quirks;
  bool supports_hx_status;
} pl2303_private_t;

// buffer sizes for line coding data
#define PL2303_LINE_CODING_BUFSIZE          7
#define PL2303_LINE_CODING_BAUDRATE_BUFSIZE 4

// bulk endpoints
#define PL2303_OUT_EP                       0x02
#define PL2303_IN_EP                        0x83

#endif // TUSB_PL2303_H
