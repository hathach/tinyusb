/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2023 Ha Thach (thach@tinyusb.org) for Adafruit Industries
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
 */

#ifndef TUSB_FTDI_SIO_H
#define TUSB_FTDI_SIO_H

#include <stdint.h>

// Commands
#define FTDI_SIO_RESET                0 // Reset the port
#define FTDI_SIO_MODEM_CTRL           1 // Set the modem control register
#define FTDI_SIO_SET_FLOW_CTRL        2 // Set flow control register
#define FTDI_SIO_SET_BAUD_RATE        3 // Set baud rate
#define FTDI_SIO_SET_DATA             4 // Set the data characteristics of the port
#define FTDI_SIO_GET_MODEM_STATUS     5 // Retrieve current value of modem status register
#define FTDI_SIO_SET_EVENT_CHAR       6 // Set the event character
#define FTDI_SIO_SET_ERROR_CHAR       7 // Set the error character
#define FTDI_SIO_SET_LATENCY_TIMER    9 // Set the latency timer
#define FTDI_SIO_GET_LATENCY_TIMER   10 // Get the latency timer
#define FTDI_SIO_SET_BITMODE         11 // Set bitbang mode
#define FTDI_SIO_READ_PINS           12 // Read immediate value of pins
#define FTDI_SIO_READ_EEPROM       0x90 // Read EEPROM

// Channel indices for FT2232, FT2232H and FT4232H devices
#define CHANNEL_A 1
#define CHANNEL_B 2
#define CHANNEL_C 3
#define CHANNEL_D 4

// Port Identifier Table
#define PIT_DEFAULT  0 // SIOA
#define PIT_SIOA     1 // SIOA
// The device this driver is tested with one has only one port
#define PIT_SIOB     2 // SIOB
#define PIT_PARALLEL 3 // Parallel

// FTDI_SIO_RESET
#define FTDI_SIO_RESET_REQUEST                    FTDI_SIO_RESET
#define FTDI_SIO_RESET_REQUEST_TYPE               0x40
#define FTDI_SIO_RESET_SIO                        0
#define FTDI_SIO_RESET_PURGE_RX                   1
#define FTDI_SIO_RESET_PURGE_TX                   2

// FTDI_SIO_SET_BAUDRATE
#define FTDI_SIO_SET_BAUDRATE_REQUEST_TYPE        0x40
#define FTDI_SIO_SET_BAUDRATE_REQUEST             3

enum ftdi_sio_baudrate {
  ftdi_sio_b300 = 0,
  ftdi_sio_b600 = 1,
  ftdi_sio_b1200 = 2,
  ftdi_sio_b2400 = 3,
  ftdi_sio_b4800 = 4,
  ftdi_sio_b9600 = 5,
  ftdi_sio_b19200 = 6,
  ftdi_sio_b38400 = 7,
  ftdi_sio_b57600 = 8,
  ftdi_sio_b115200 = 9
};

// FTDI_SIO_SET_DATA
#define FTDI_SIO_SET_DATA_REQUEST                 FTDI_SIO_SET_DATA
#define FTDI_SIO_SET_DATA_REQUEST_TYPE            0x40
#define FTDI_SIO_SET_DATA_PARITY_NONE             (0x0 << 8)
#define FTDI_SIO_SET_DATA_PARITY_ODD              (0x1 << 8)
#define FTDI_SIO_SET_DATA_PARITY_EVEN             (0x2 << 8)
#define FTDI_SIO_SET_DATA_PARITY_MARK             (0x3 << 8)
#define FTDI_SIO_SET_DATA_PARITY_SPACE            (0x4 << 8)
#define FTDI_SIO_SET_DATA_STOP_BITS_1             (0x0 << 11) // same coding as ACM
#define FTDI_SIO_SET_DATA_STOP_BITS_15            (0x1 << 11) // 1.5 not supported, for future use?
#define FTDI_SIO_SET_DATA_STOP_BITS_2             (0x2 << 11)
#define FTDI_SIO_SET_BREAK                        (0x1 << 14)

// FTDI_SIO_MODEM_CTRL
#define FTDI_SIO_SET_MODEM_CTRL_REQUEST_TYPE      0x40
#define FTDI_SIO_SET_MODEM_CTRL_REQUEST           FTDI_SIO_MODEM_CTRL

#define FTDI_SIO_SET_DTR_MASK                     0x1UL
#define FTDI_SIO_SET_DTR_HIGH                     ((FTDI_SIO_SET_DTR_MASK << 8) | 1UL)
#define FTDI_SIO_SET_DTR_LOW                      ((FTDI_SIO_SET_DTR_MASK << 8) | 0UL)
#define FTDI_SIO_SET_RTS_MASK                     0x2UL
#define FTDI_SIO_SET_RTS_HIGH                     ((FTDI_SIO_SET_RTS_MASK << 8) | 2UL)
#define FTDI_SIO_SET_RTS_LOW                      ((FTDI_SIO_SET_RTS_MASK << 8) | 0UL)

// FTDI_SIO_SET_FLOW_CTRL
#define FTDI_SIO_SET_FLOW_CTRL_REQUEST_TYPE       0x40
#define FTDI_SIO_SET_FLOW_CTRL_REQUEST            FTDI_SIO_SET_FLOW_CTRL
#define FTDI_SIO_DISABLE_FLOW_CTRL                0x0
#define FTDI_SIO_RTS_CTS_HS                       (0x1 << 8)
#define FTDI_SIO_DTR_DSR_HS                       (0x2 << 8)
#define FTDI_SIO_XON_XOFF_HS                      (0x4 << 8)

// FTDI_SIO_GET_LATENCY_TIMER
#define  FTDI_SIO_GET_LATENCY_TIMER_REQUEST       FTDI_SIO_GET_LATENCY_TIMER
#define  FTDI_SIO_GET_LATENCY_TIMER_REQUEST_TYPE  0xC0

// FTDI_SIO_SET_LATENCY_TIMER
#define  FTDI_SIO_SET_LATENCY_TIMER_REQUEST       FTDI_SIO_SET_LATENCY_TIMER
#define  FTDI_SIO_SET_LATENCY_TIMER_REQUEST_TYPE  0x40

// FTDI_SIO_SET_EVENT_CHAR
#define  FTDI_SIO_SET_EVENT_CHAR_REQUEST          FTDI_SIO_SET_EVENT_CHAR
#define  FTDI_SIO_SET_EVENT_CHAR_REQUEST_TYPE     0x40

// FTDI_SIO_GET_MODEM_STATUS
#define FTDI_SIO_GET_MODEM_STATUS_REQUEST_TYPE    0xc0
#define FTDI_SIO_GET_MODEM_STATUS_REQUEST         FTDI_SIO_GET_MODEM_STATUS
#define FTDI_SIO_CTS_MASK                         0x10
#define FTDI_SIO_DSR_MASK                         0x20
#define FTDI_SIO_RI_MASK                          0x40
#define FTDI_SIO_RLSD_MASK                        0x80

// FTDI_SIO_SET_BITMODE
#define FTDI_SIO_SET_BITMODE_REQUEST_TYPE         0x40
#define FTDI_SIO_SET_BITMODE_REQUEST              FTDI_SIO_SET_BITMODE

// Possible bitmodes for FTDI_SIO_SET_BITMODE_REQUEST
#define FTDI_SIO_BITMODE_RESET                    0x00
#define FTDI_SIO_BITMODE_CBUS                     0x20

// FTDI_SIO_READ_PINS
#define FTDI_SIO_READ_PINS_REQUEST_TYPE           0xc0
#define FTDI_SIO_READ_PINS_REQUEST                FTDI_SIO_READ_PINS

// FTDI_SIO_READ_EEPROM
#define FTDI_SIO_READ_EEPROM_REQUEST_TYPE         0xc0
#define FTDI_SIO_READ_EEPROM_REQUEST              FTDI_SIO_READ_EEPROM

#define FTDI_FTX_CBUS_MUX_GPIO    0x8
#define FTDI_FT232R_CBUS_MUX_GPIO 0xa

#define FTDI_RS0_CTS  (1 << 4)
#define FTDI_RS0_DSR  (1 << 5)
#define FTDI_RS0_RI   (1 << 6)
#define FTDI_RS0_RLSD (1 << 7)

#define FTDI_RS_DR    1
#define FTDI_RS_OE    (1 << 1)
#define FTDI_RS_PE    (1 << 2)
#define FTDI_RS_FE    (1 << 3)
#define FTDI_RS_BI    (1 << 4)
#define FTDI_RS_THRE  (1 << 5)
#define FTDI_RS_TEMT  (1 << 6)
#define FTDI_RS_FIFO  (1 << 7)

// chip types and names
typedef enum ftdi_chip_type {
  FTDI_SIO = 0,
//  FTDI_FT232A,
  FTDI_FT232B,
  FTDI_FT2232C,
  FTDI_FT232R,
  FTDI_FT232H,
  FTDI_FT2232H,
  FTDI_FT4232H,
  FTDI_FT4232HA,
  FTDI_FT232HP,
  FTDI_FT233HP,
  FTDI_FT2232HP,
  FTDI_FT2233HP,
  FTDI_FT4232HP,
  FTDI_FT4233HP,
  FTDI_FTX,
  FTDI_UNKNOWN
} ftdi_chip_type_t;

#define FTDI_CHIP_NAMES \
  [FTDI_SIO]       = "SIO", /* the serial part of FT8U100AX */ \
/*  [FTDI_FT232A]    = "FT232A", */ \
  [FTDI_FT232B]    = "FT232B", \
  [FTDI_FT2232C]   = "FT2232C/D", \
  [FTDI_FT232R]    = "FT232R", \
  [FTDI_FT232H]    = "FT232H", \
  [FTDI_FT2232H]   = "FTDI_FT2232H", \
  [FTDI_FT4232H]   = "FT4232H", \
  [FTDI_FT4232HA]  = "FT4232HA", \
  [FTDI_FT232HP]   = "FT232HP", \
  [FTDI_FT233HP]   = "FT233HP", \
  [FTDI_FT2232HP]  = "FT2232HP", \
  [FTDI_FT2233HP]  = "FT2233HP", \
  [FTDI_FT4232HP]  = "FT4232HP", \
  [FTDI_FT4233HP]  = "FT4233HP", \
  [FTDI_FTX]       = "FT-X", \
  [FTDI_UNKNOWN]   = "UNKNOWN"

// private interface data
typedef struct ftdi_private {
  ftdi_chip_type_t chip_type;
  uint8_t channel;                  // channel index, or 0 for legacy types
} ftdi_private_t;

#define FTDI_OK           true
#define FTDI_FAIL         false
#define FTDI_NOT_POSSIBLE -1
#define FTDI_REQUESTED    -2

// division and round function overtaken from math.h
#define DIV_ROUND_CLOSEST(x, divisor)(      \
{             \
  typeof(x) __x = x;        \
  typeof(divisor) __d = divisor;      \
  (((typeof(x))-1) > 0 ||       \
   ((typeof(divisor))-1) > 0 ||     \
   (((__x) > 0) == ((__d) > 0))) ?    \
    (((__x) + ((__d) / 2)) / (__d)) : \
    (((__x) - ((__d) / 2)) / (__d));  \
}             \
)

#endif //TUSB_FTDI_SIO_H
