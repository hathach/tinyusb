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

/** \ingroup group_usbd
 *  @{ */

#ifndef _TUSB_USBD_H_
#define _TUSB_USBD_H_

#ifdef __cplusplus
 extern "C" {
#endif

#include "common/tusb_common.h"
#include "device/dcd.h"

/// \brief Descriptor pointer collector to all the needed.
typedef struct {
  void const * device;            ///< pointer to device descriptor \ref tusb_desc_device_t
  void const * config;            ///< pointer to the whole configuration descriptor, starting by \ref tusb_desc_configuration_t

  uint8_t const** string_arr;     ///< a array of pointers to string descriptors
  uint16_t        string_count;

  struct {
    uint8_t const* generic;
    uint8_t const* boot_keyboard;
    uint8_t const* boot_mouse;
  } hid_report;

}tud_desc_set_t;

// Must be defined by application
extern tud_desc_set_t tud_desc_set;

//--------------------------------------------------------------------+
// Application API
//--------------------------------------------------------------------+

// Task function should be called in main/rtos loop
void tud_task (void);

// Check if device is connected and configured
bool tud_mounted(void);

// Check if device is suspended
bool tud_suspended(void);

// Check if device is ready to transfer
static inline bool tud_ready(void)
{
  return tud_mounted() && !tud_suspended();
}

// Remote wake up host, only if suspended and enabled by host
bool tud_remote_wakeup(void);

//--------------------------------------------------------------------+
// Application Callbacks (WEAK is optional)
//--------------------------------------------------------------------+

// Invoked when device is mounted (configured)
ATTR_WEAK void tud_mount_cb(void);

// Invoked when device is unmounted
ATTR_WEAK void tud_umount_cb(void);

// Invoked when usb bus is suspended
// Within 7ms, device must draw an average of current less than 2.5 mA from bus
ATTR_WEAK void tud_suspend_cb(bool remote_wakeup_en);

// Invoked when usb bus is resumed
ATTR_WEAK void tud_resume_cb(void);

#ifdef __cplusplus
 }
#endif

#endif /* _TUSB_USBD_H_ */

/** @} */
