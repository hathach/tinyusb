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

#ifndef _PL2303_H_
#define _PL2303_H_

#include <stdbool.h>
#include <stdint.h>

// There is no official documentation for the PL2303 chips.
// Reference can be found
// - https://github.com/torvalds/linux/blob/master/drivers/usb/serial/pl2303.h and
//   https://github.com/torvalds/linux/blob/master/drivers/usb/serial/pl2303.c
// - https://github.com/freebsd/freebsd-src/blob/main/sys/dev/usb/serial/uplcom.c

#define PL2303_BENQ_VENDOR_ID 0x04a5
#define PL2303_BENQ_PRODUCT_ID_S81 0x4027

#define PL2303_VENDOR_ID 0x067b
#define PL2303_PRODUCT_ID 0x2303
#define PL2303_PRODUCT_ID_TB 0x2304
#define PL2303_PRODUCT_ID_GC 0x23a3
#define PL2303_PRODUCT_ID_GB 0x23b3
#define PL2303_PRODUCT_ID_GT 0x23c3
#define PL2303_PRODUCT_ID_GL 0x23d3
#define PL2303_PRODUCT_ID_GE 0x23e3
#define PL2303_PRODUCT_ID_GS 0x23f3
#define PL2303_PRODUCT_ID_RSAQ2 0x04bb
#define PL2303_PRODUCT_ID_DCU11 0x1234
#define PL2303_PRODUCT_ID_PHAROS 0xaaa0
#define PL2303_PRODUCT_ID_RSAQ3 0xaaa2
#define PL2303_PRODUCT_ID_CHILITAG 0xaaa8
#define PL2303_PRODUCT_ID_ALDIGA 0x0611
#define PL2303_PRODUCT_ID_MMX 0x0612
#define PL2303_PRODUCT_ID_GPRS 0x0609
#define PL2303_PRODUCT_ID_HCR331 0x331a
#define PL2303_PRODUCT_ID_MOTOROLA 0x0307
#define PL2303_PRODUCT_ID_ZTEK 0xe1f1


#define PL2303_ATEN_VENDOR_ID 0x0557
#define PL2303_ATEN_VENDOR_ID2 0x0547
#define PL2303_ATEN_PRODUCT_ID 0x2008
#define PL2303_ATEN_PRODUCT_UC485 0x2021
#define PL2303_ATEN_PRODUCT_UC232B 0x2022
#define PL2303_ATEN_PRODUCT_ID2 0x2118

#define PL2303_IBM_VENDOR_ID 0x04b3
#define PL2303_IBM_PRODUCT_ID 0x4016

#define PL2303_IODATA_VENDOR_ID 0x04bb
#define PL2303_IODATA_PRODUCT_ID 0x0a03
#define PL2303_IODATA_PRODUCT_ID_RSAQ5 0x0a0e

#define PL2303_ELCOM_VENDOR_ID 0x056e
#define PL2303_ELCOM_PRODUCT_ID 0x5003
#define PL2303_ELCOM_PRODUCT_ID_UCSGT 0x5004

#define PL2303_ITEGNO_VENDOR_ID 0x0eba
#define PL2303_ITEGNO_PRODUCT_ID 0x1080
#define PL2303_ITEGNO_PRODUCT_ID_2080 0x2080

#define PL2303_MA620_VENDOR_ID 0x0df7
#define PL2303_MA620_PRODUCT_ID 0x0620

#define PL2303_RATOC_VENDOR_ID 0x0584
#define PL2303_RATOC_PRODUCT_ID 0xb000

#define PL2303_TRIPP_VENDOR_ID 0x2478
#define PL2303_TRIPP_PRODUCT_ID 0x2008

#define PL2303_RADIOSHACK_VENDOR_ID 0x1453
#define PL2303_RADIOSHACK_PRODUCT_ID 0x4026

#define PL2303_DCU10_VENDOR_ID 0x0731
#define PL2303_DCU10_PRODUCT_ID 0x0528

#define PL2303_SITECOM_VENDOR_ID 0x6189
#define PL2303_SITECOM_PRODUCT_ID 0x2068

/* Alcatel OT535/735 USB cable */
#define PL2303_ALCATEL_VENDOR_ID 0x11f7
#define PL2303_ALCATEL_PRODUCT_ID 0x02df

#define PL2303_SIEMENS_VENDOR_ID 0x11f5
#define PL2303_SIEMENS_PRODUCT_ID_SX1 0x0001
#define PL2303_SIEMENS_PRODUCT_ID_X65 0x0003
#define PL2303_SIEMENS_PRODUCT_ID_X75 0x0004
#define PL2303_SIEMENS_PRODUCT_ID_EF81 0x0005

#define PL2303_SYNTECH_VENDOR_ID 0x0745
#define PL2303_SYNTECH_PRODUCT_ID 0x0001

/* Nokia CA-42 Cable */
#define PL2303_NOKIA_CA42_VENDOR_ID 0x078b
#define PL2303_NOKIA_CA42_PRODUCT_ID 0x1234

/* CA-42 CLONE Cable www.ca-42.com chipset: Prolific Technology Inc */
#define PL2303_CA_42_CA42_VENDOR_ID 0x10b5
#define PL2303_CA_42_CA42_PRODUCT_ID 0xac70

#define PL2303_SAGEM_VENDOR_ID 0x079b
#define PL2303_SAGEM_PRODUCT_ID 0x0027

/* Leadtek GPS 9531 (ID 0413:2101) */
#define PL2303_LEADTEK_VENDOR_ID 0x0413
#define PL2303_LEADTEK_9531_PRODUCT_ID 0x2101

/* USB GSM cable from Speed Dragon Multimedia, Ltd */
#define PL2303_SPEEDDRAGON_VENDOR_ID 0x0e55
#define PL2303_SPEEDDRAGON_PRODUCT_ID 0x110b

/* DATAPILOT Universal-2 Phone Cable */
#define PL2303_DATAPILOT_U2_VENDOR_ID 0x0731
#define PL2303_DATAPILOT_U2_PRODUCT_ID 0x2003

/* Belkin "F5U257" Serial Adapter */
#define PL2303_BELKIN_VENDOR_ID 0x050d
#define PL2303_BELKIN_PRODUCT_ID 0x0257

/* Alcor Micro Corp. USB 2.0 TO RS-232 */
#define PL2303_ALCOR_VENDOR_ID 0x058F
#define PL2303_ALCOR_PRODUCT_ID 0x9720

/* Willcom WS002IN Data Driver (by NetIndex Inc.) */
#define PL2303_WS002IN_VENDOR_ID 0x11f6
#define PL2303_WS002IN_PRODUCT_ID 0x2001

/* Corega CG-USBRS232R Serial Adapter */
#define PL2303_COREGA_VENDOR_ID 0x07aa
#define PL2303_COREGA_PRODUCT_ID 0x002a

/* Y.C. Cable U.S.A., Inc - USB to RS-232 */
#define PL2303_YCCABLE_VENDOR_ID 0x05ad
#define PL2303_YCCABLE_PRODUCT_ID 0x0fba

/* "Superial" USB - Serial */
#define PL2303_SUPERIAL_VENDOR_ID 0x5372
#define PL2303_SUPERIAL_PRODUCT_ID 0x2303

/* Hewlett-Packard POS Pole Displays */
#define PL2303_HP_VENDOR_ID 0x03f0
#define PL2303_HP_LD381GC_PRODUCT_ID 0x0183
#define PL2303_HP_LM920_PRODUCT_ID 0x026b
#define PL2303_HP_TD620_PRODUCT_ID 0x0956
#define PL2303_HP_LD960_PRODUCT_ID 0x0b39
#define PL2303_HP_LD381_PRODUCT_ID 0x0f7f
#define PL2303_HP_LM930_PRODUCT_ID 0x0f9b
#define PL2303_HP_LCM220_PRODUCT_ID 0x3139
#define PL2303_HP_LCM960_PRODUCT_ID 0x3239
#define PL2303_HP_LD220_PRODUCT_ID 0x3524
#define PL2303_HP_LD220TA_PRODUCT_ID 0x4349
#define PL2303_HP_LD960TA_PRODUCT_ID 0x4439
#define PL2303_HP_LM940_PRODUCT_ID 0x5039

/* Cressi Edy (diving computer) PC interface */
#define PL2303_CRESSI_VENDOR_ID 0x04b8
#define PL2303_CRESSI_EDY_PRODUCT_ID 0x0521

/* Zeagle dive computer interface */
#define PL2303_ZEAGLE_VENDOR_ID 0x04b8
#define PL2303_ZEAGLE_N2ITION3_PRODUCT_ID 0x0522

/* Sony, USB data cable for CMD-Jxx mobile phones */
#define PL2303_SONY_VENDOR_ID 0x054c
#define PL2303_SONY_QN3USB_PRODUCT_ID 0x0437

/* Sanwa KB-USB2 multimeter cable (ID: 11ad:0001) */
#define PL2303_SANWA_VENDOR_ID 0x11ad
#define PL2303_SANWA_PRODUCT_ID 0x0001

/* ADLINK ND-6530 RS232,RS485 and RS422 adapter */
#define PL2303_ADLINK_VENDOR_ID 0x0b63
#define PL2303_ADLINK_ND6530_PRODUCT_ID 0x6530
#define PL2303_ADLINK_ND6530GC_PRODUCT_ID 0x653a

/* SMART USB Serial Adapter */
#define PL2303_SMART_VENDOR_ID 0x0b8c
#define PL2303_SMART_PRODUCT_ID 0x2303

/* Allied Telesis VT-Kit3 */
#define PL2303_AT_VENDOR_ID 0x0caa
#define PL2303_AT_VTKIT3_PRODUCT_ID 0x3001

/* quirks */
#define PL2303_QUIRK_UART_STATE_IDX0 1
#define PL2303_QUIRK_LEGACY 2
#define PL2303_QUIRK_ENDPOINT_HACK 4

/* requests and bits */
#define PL2303_SET_LINE_REQUEST_TYPE 0x21     // class request host to device interface
#define PL2303_SET_LINE_REQUEST 0x20          // dec 32

#define PL2303_SET_CONTROL_REQUEST_TYPE 0x21  // class request host to device interface
#define PL2303_SET_CONTROL_REQUEST 0x22       // dec 34
#define PL2303_CONTROL_DTR 0x01               // dec 1
#define PL2303_CONTROL_RTS 0x02               // dec 2

#define PL2303_BREAK_REQUEST_TYPE 0x21        // class request host to device interface
#define PL2303_BREAK_REQUEST 0x23             // dec 35
#define PL2303_BREAK_ON 0xffff
#define PL2303_BREAK_OFF 0x0000

#define PL2303_GET_LINE_REQUEST_TYPE 0xa1     // class request device to host interface
#define PL2303_GET_LINE_REQUEST 0x21          // dec 33

#define PL2303_VENDOR_WRITE_REQUEST_TYPE 0x40 // vendor request host to device interface
#define PL2303_VENDOR_WRITE_REQUEST 0x01      // dec 1
#define PL2303_VENDOR_WRITE_NREQUEST 0x80     // dec 128

#define PL2303_VENDOR_READ_REQUEST_TYPE 0xc0  // vendor request device to host interface
#define PL2303_VENDOR_READ_REQUEST 0x01       // dec 1
#define PL2303_VENDOR_READ_NREQUEST 0x81      // dec 129

#define PL2303_UART_STATE_INDEX 8
#define PL2303_UART_STATE_MSR_MASK 0x8b
#define PL2303_UART_STATE_TRANSIENT_MASK 0x74
#define PL2303_UART_DCD 0x01
#define PL2303_UART_DSR 0x02
#define PL2303_UART_BREAK_ERROR 0x04
#define PL2303_UART_RING 0x08
#define PL2303_UART_FRAME_ERROR 0x10
#define PL2303_UART_PARITY_ERROR 0x20
#define PL2303_UART_OVERRUN_ERROR 0x40
#define PL2303_UART_CTS 0x80

#define PL2303_FLOWCTRL_MASK 0xf0

#define PL2303_CLEAR_HALT_REQUEST_TYPE 0x02   // standard request host to device endpoint

/* registers via vendor read/write requests */
#define PL2303_READ_TYPE_HX_STATUS 0x8080

#define PL2303_HXN_RESET_REG 0x07
#define PL2303_HXN_RESET_UPSTREAM_PIPE 0x02
#define PL2303_HXN_RESET_DOWNSTREAM_PIPE 0x01

#define PL2303_HXN_FLOWCTRL_REG 0x0a
#define PL2303_HXN_FLOWCTRL_MASK 0x1c
#define PL2303_HXN_FLOWCTRL_NONE 0x1c
#define PL2303_HXN_FLOWCTRL_RTS_CTS 0x18
#define PL2303_HXN_FLOWCTRL_XON_XOFF 0x0c

/* type data */
enum pl2303_type {
  TYPE_H,
  TYPE_HX,
  TYPE_TA,
  TYPE_TB,
  TYPE_HXD,
  TYPE_HXN,
  TYPE_COUNT
};

struct pl2303_type_data {
  uint8_t const *name;
  uint32_t const max_baud_rate;
  uint32_t const quirks;
  uint16_t const no_autoxonxoff:1;
  uint16_t const no_divisors:1;
  uint16_t const alt_divisors:1;
};

#define PL2303_TYPE_DATA \
  [TYPE_H] = { \
    .name = (uint8_t*)"H", \
    .max_baud_rate = 1228800, \
    .quirks = PL2303_QUIRK_LEGACY, \
    .no_autoxonxoff = true, \
  }, \
  [TYPE_HX] = { \
    .name = (uint8_t*)"HX", \
    .max_baud_rate = 6000000, \
  }, \
  [TYPE_TA] = { \
    .name = (uint8_t*)"TA", \
    .max_baud_rate = 6000000, \
    .alt_divisors = true, \
  }, \
  [TYPE_TB] = { \
    .name = (uint8_t*)"TB", \
    .max_baud_rate = 12000000, \
    .alt_divisors = true, \
  }, \
  [TYPE_HXD] = { \
    .name = (uint8_t*)"HXD", \
    .max_baud_rate = 12000000, \
  }, \
  [TYPE_HXN] = { \
    .name = (uint8_t*)"G (HXN)", \
    .max_baud_rate = 12000000, \
    .no_divisors = true, \
  }

/* private data types */
struct pl2303_serial_private {
  const struct pl2303_type_data* type;
  uint16_t quirks;
};

typedef struct TU_ATTR_PACKED {
  struct pl2303_serial_private serial_private;
  bool supports_hx_status;
} pl2303_t;

/* buffer sizes for line coding data */
#define PL2303_LINE_CODING_BUFSIZE 7
#define PL2303_LINE_CODING_BAUDRATE_BUFSIZE 4

/* bulk endpoints */
#define PL2303_OUT_EP 0x02
#define PL2303_IN_EP 0x83

/* return return values of pl2303_detect_type() */
#define PL2303_SUPPORTS_HX_STATUS_TRIGGERED -1
#define PL2303_DETECT_TYPE_FAILED -2

#endif /* _PL2303_H_ */
