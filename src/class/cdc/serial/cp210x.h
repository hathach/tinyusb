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

#ifndef TUSB_CP210X_H
#define TUSB_CP210X_H

// Protocol details can be found at AN571: CP210x Virtual COM Port Interface
// https://www.silabs.com/documents/public/application-notes/AN571.pdf

// parts are overtaken from vendors driver
// https://www.silabs.com/documents/public/software/cp210x-3.1.0.tar.gz

/* Config request codes */
#define CP210X_IFC_ENABLE      0x00
#define CP210X_SET_BAUDDIV     0x01
#define CP210X_GET_BAUDDIV     0x02
#define CP210X_SET_LINE_CTL    0x03 // Set parity, data bits, stop bits
#define CP210X_GET_LINE_CTL    0x04
#define CP210X_SET_BREAK       0x05
#define CP210X_IMM_CHAR        0x06
#define CP210X_SET_MHS         0x07 // Set DTR, RTS
#define CP210X_GET_MDMSTS      0x08 // Get modem status (DTR, RTS, CTS, DSR, RI, DCD)
#define CP210X_SET_XON         0x09
#define CP210X_SET_XOFF        0x0A
#define CP210X_SET_EVENTMASK   0x0B
#define CP210X_GET_EVENTMASK   0x0C
#define CP210X_SET_CHAR        0x0D
#define CP210X_GET_CHARS       0x0E
#define CP210X_GET_PROPS       0x0F
#define CP210X_GET_COMM_STATUS 0x10
#define CP210X_RESET           0x11
#define CP210X_PURGE           0x12
#define CP210X_SET_FLOW        0x13
#define CP210X_GET_FLOW        0x14
#define CP210X_EMBED_EVENTS    0x15
#define CP210X_GET_EVENTSTATE  0x16
#define CP210X_SET_CHARS       0x19
#define CP210X_GET_BAUDRATE    0x1D
#define CP210X_SET_BAUDRATE    0x1E
#define CP210X_VENDOR_SPECIFIC 0xFF // GPIO, Recipient must be Device

/* SILABSER_IFC_ENABLE_REQUEST_CODE */
#define CP210X_UART_ENABLE       0x0001
#define CP210X_UART_DISABLE        0x0000

/* SILABSER_SET_BAUDDIV_REQUEST_CODE */
#define CP210X_BAUD_RATE_GEN_FREQ      0x384000

/*SILABSER_SET_LINE_CTL_REQUEST_CODE */
#define CP210X_BITS_DATA_MASK        0x0f00
#define CP210X_BITS_DATA_5       0x0500
#define CP210X_BITS_DATA_6       0x0600
#define CP210X_BITS_DATA_7       0x0700
#define CP210X_BITS_DATA_8       0x0800
#define CP210X_BITS_DATA_9       0x0900

#define CP210X_BITS_PARITY_MASK      0x00f0
#define CP210X_BITS_PARITY_NONE      0x0000
#define CP210X_BITS_PARITY_ODD       0x0010
#define CP210X_BITS_PARITY_EVEN      0x0020
#define CP210X_BITS_PARITY_MARK      0x0030
#define CP210X_BITS_PARITY_SPACE     0x0040

#define CP210X_BITS_STOP_MASK        0x000f
#define CP210X_BITS_STOP_1       0x0000
#define CP210X_BITS_STOP_1_5       0x0001
#define CP210X_BITS_STOP_2       0x0002

/* SILABSER_SET_BREAK_REQUEST_CODE */
#define CP210X_BREAK_ON        0x0001
#define CP210X_BREAK_OFF       0x0000

/* SILABSER_SET_MHS_REQUEST_CODE */
#define CP210X_MCR_DTR         0x0001
#define CP210X_MCR_RTS         0x0002
#define CP210X_MCR_ALL         0x0003
#define CP210X_MSR_CTS         0x0010
#define CP210X_MSR_DSR         0x0020
#define CP210X_MSR_RING        0x0040
#define CP210X_MSR_DCD         0x0080
#define CP210X_MSR_ALL         0x00F0

#define CP210X_CONTROL_WRITE_DTR     0x0100
#define CP210X_CONTROL_WRITE_RTS     0x0200

#define CP210X_LSR_BREAK       0x0001
#define CP210X_LSR_FRAMING_ERROR     0x0002
#define CP210X_LSR_HW_OVERRUN        0x0004
#define CP210X_LSR_QUEUE_OVERRUN     0x0008
#define CP210X_LSR_PARITY_ERROR      0x0010
#define CP210X_LSR_ALL         0x001F

// supported baudrates
// reference: datasheets and AN205 "CP210x Baud Rate Support"
#define CP210X_SUPPORTED_BAUDRATES_LIST { \
  300, 600, \
  1200, 1800, 2400, 4000, 4800, 7200, 9600, \
  14400, 16000, 19200, 28800, 38400, 51200, 56000, 57600, 64000, 76800, \
  115200, 128000, 153600, 230400, 250000, 256000, 460800, 500000, 576000, 921600, \
  0 }

#endif //TUSB_CP210X_H
