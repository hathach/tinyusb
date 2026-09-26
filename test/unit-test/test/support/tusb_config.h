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

#ifndef TUSB_CONFIG_H_
#define TUSB_CONFIG_H_

// testing framework
#include "unity.h"

#ifdef __cplusplus
 extern "C" {
#endif

//--------------------------------------------------------------------
// COMMON CONFIGURATION
//--------------------------------------------------------------------

// defined by compiler flags for flexibility
#ifndef CFG_TUSB_MCU
  //#error CFG_TUSB_MCU must be defined
  #define CFG_TUSB_MCU  OPT_MCU_NRF5X
#endif

#ifndef CFG_TUSB_RHPORT0_MODE
#define CFG_TUSB_RHPORT0_MODE    (OPT_MODE_DEVICE | OPT_MODE_HIGH_SPEED)
#endif

#define CFG_TUSB_OS              OPT_OS_NONE

// CFG_TUSB_DEBUG is defined by compiler in DEBUG build
#ifndef CFG_TUSB_DEBUG
#define CFG_TUSB_DEBUG           1
#endif

/* USB DMA on some MCUs can only access a specific SRAM region with restriction on alignment.
 * Tinyusb use follows macros to declare transferring memory so that they can be put
 * into those specific section.
 * e.g
 * - CFG_TUSB_MEM SECTION : __attribute__ (( section(".usb_ram") ))
 * - CFG_TUSB_MEM_ALIGN   : __attribute__ ((aligned(4)))
 */
#ifndef CFG_TUSB_MEM_SECTION
#define CFG_TUSB_MEM_SECTION
#endif

#ifndef CFG_TUSB_MEM_ALIGN
#define CFG_TUSB_MEM_ALIGN       __attribute__ ((aligned(4)))
#endif

//--------------------------------------------------------------------
// DEVICE CONFIGURATION
//--------------------------------------------------------------------

#define CFG_TUD_TASK_QUEUE_SZ    100
#define CFG_TUD_ENDPOINT0_SIZE    64

//------------- CLASS -------------//
//#define CFG_TUD_CDC              0
#ifndef CFG_TUD_MSC
#define CFG_TUD_MSC              1
#endif
//#define CFG_TUD_HID              0
//#define CFG_TUD_MIDI             0
//#define CFG_TUD_VENDOR           0

//------------- CDC -------------//

// FIFO size of CDC TX and RX
#define CFG_TUD_CDC_RX_BUFSIZE   512
#define CFG_TUD_CDC_TX_BUFSIZE   512

//------------- MSC -------------//

// Buffer size of Device Mass storage
#define CFG_TUD_MSC_BUFSIZE      512

//------------- HID -------------//

// Should be sufficient to hold ID (if any) + Data
#define CFG_TUD_HID_EP_BUFSIZE    64

//------------- MTP -------------//
#define CFG_TUD_MTP_EP_BUFSIZE          512
#define CFG_TUD_MTP_EP_CONTROL_BUFSIZE  16
#define CFG_TUD_MTP_DEVICEINFO_EXTENSIONS                   "microsoft.com: 1.0; "
#define CFG_TUD_MTP_DEVICEINFO_SUPPORTED_OPERATIONS         MTP_OP_GET_DEVICE_INFO, MTP_OP_OPEN_SESSION
#define CFG_TUD_MTP_DEVICEINFO_SUPPORTED_EVENTS             MTP_EVENT_OBJECT_ADDED
#define CFG_TUD_MTP_DEVICEINFO_SUPPORTED_DEVICE_PROPERTIES  MTP_DEV_PROP_DEVICE_FRIENDLY_NAME
#define CFG_TUD_MTP_DEVICEINFO_CAPTURE_FORMATS              MTP_OBJ_FORMAT_UNDEFINED
#define CFG_TUD_MTP_DEVICEINFO_PLAYBACK_FORMATS             MTP_OBJ_FORMAT_UNDEFINED

#ifdef __cplusplus
 }
#endif

#endif /* TUSB_CONFIG_H_ */
