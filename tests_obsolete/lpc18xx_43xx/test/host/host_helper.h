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
 * This file is part of the TinyUSB stack.
 */

#include "common/common.h"
#include "host/usbh.h"
#include "host/usbh_hcd.h"

static inline void helper_class_init_expect(void)
{ // class code number order
#if CFG_TUH_CDC
  cdch_init_Expect();
#endif

#if HOST_CLASS_HID
  hidh_init_Expect();
#endif

#if CFG_TUH_MSC
  msch_init_Expect();
#endif

  //TODO update more classes
}


static inline void helper_usbh_init_expect(void)
{
  osal_queue_create_IgnoreAndReturn( (osal_queue_handle_t) 0x4566 );
  osal_task_create_IgnoreAndReturn(TUSB_ERROR_NONE);

  osal_semaphore_create_IgnoreAndReturn( (osal_semaphore_handle_t) 0x1234);
  osal_mutex_create_IgnoreAndReturn((osal_mutex_handle_t) 0x789a);
}

static inline void helper_usbh_device_emulate(uint8_t dev_addr, uint8_t hub_addr, uint8_t hub_port, uint8_t hostid, tusb_speed_t speed)
{
  _usbh_devices[dev_addr].rhport  = hostid;
  _usbh_devices[dev_addr].hub_addr = hub_addr;
  _usbh_devices[dev_addr].hub_port = hub_port;
  _usbh_devices[dev_addr].speed    = speed;
  _usbh_devices[dev_addr].state    = dev_addr ? TUSB_DEVICE_STATE_CONFIGURED : TUSB_DEVICE_STATE_UNPLUG;
}






