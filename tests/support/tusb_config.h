/* 
 * The MIT License (MIT)
 *
 * Copyright (c) 2018, hathach (tinyusb.org)
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

#ifndef _TUSB_CONFIG_H_
#define _TUSB_CONFIG_H_

#ifdef __cplusplus
 extern "C" {
#endif

//--------------------------------------------------------------------+
// CONTROLLER CONFIGURATION
//--------------------------------------------------------------------+
#define CFG_TUSB_RHPORT0_MODE        (OPT_MODE_HOST | OPT_MODE_DEVICE)
#define CFG_TUSB_RHPORT1_MODE        (OPT_MODE_NONE)

//--------------------------------------------------------------------+
// HOST CONFIGURATION
//--------------------------------------------------------------------+
#define CFG_TUSB_HOST_DEVICE_MAX          5 // TODO be a part of HUB config

//------------- CLASS -------------//
#define CFG_TUH_HUB                 1
#define CFG_TUH_HID_KEYBOARD        1
#define CFG_TUH_HID_MOUSE           1
#define CFG_TUH_MSC                 1
#define CFG_TUSB_HOST_HID_GENERIC         0
#define CFG_TUH_CDC                 1
#define CFG_TUH_CDC_RNDIS           0

// Test support
#define TEST_CONTROLLER_HOST_START_INDEX \
 ( ((CONTROLLER_HOST_NUMBER == 1) && (CFG_TUSB_RHPORT1_MODE & OPT_MODE_HOST)) ? 1 : 0)

//--------------------------------------------------------------------+
// DEVICE CONFIGURATION
//--------------------------------------------------------------------+
#define CFG_TUD_ENDOINT0_SIZE     64

//------------- CLASS -------------//
#define CFG_TUD_CDC               1
#define CFG_TUD_MSC               1
#define CFG_TUD_HID_KEYBOARD      1
#define CFG_TUD_HID_MOUSE         1


//--------------------------------------------------------------------+
// COMMON CONFIGURATION
//--------------------------------------------------------------------+

#define CFG_TUSB_DEBUG 3

#define CFG_TUSB_OS OPT_OS_NONE
#define CFG_TUSB_MEM_SECTION

#ifdef __cplusplus
 }
#endif


#define RANDOM(n) (rand()%(n))

#endif /* _TUSB_CONFIG_H_ */

/** @} */
