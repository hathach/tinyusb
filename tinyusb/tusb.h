/**************************************************************************/
/*!
    @file     tusb.h
    @author   hathach (tinyusb.org)

    @section LICENSE

    Software License Agreement (BSD License)

    Copyright (c) 2013, hathach (tinyusb.org)
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:
    1. Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
    2. Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.
    3. Neither the name of the copyright holders nor the
    names of its contributors may be used to endorse or promote products
    derived from this software without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ''AS IS'' AND ANY
    EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
    WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
    DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE LIABLE FOR ANY
    DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
    INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
    LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION HOWEVER CAUSED AND
    ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    INCLUDING NEGLIGENCE OR OTHERWISE ARISING IN ANY WAY OUT OF THE USE OF THIS
    SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

	  This file is part of the tinyusb stack.
*/
/**************************************************************************/

#ifndef _TUSB_H_
#define _TUSB_H_

#ifdef __cplusplus
 extern "C" {
#endif

//--------------------------------------------------------------------+
// INCLUDE
//--------------------------------------------------------------------+
#include "common/common.h"
#include "hal/hal.h"
#include "osal/osal.h"

//------------- HOST -------------//
#if MODE_HOST_SUPPORTED
  #include "host/usbh.h"

  #if HOST_CLASS_HID
    #include "class/hid_host.h"
  #endif

  #if TUSB_CFG_HOST_MSC
    #include "class/msc_host.h"
  #endif

  #if TUSB_CFG_HOST_CDC
    #include "class/cdc_host.h"
  #endif

  #if TUSB_CFG_HOST_CUSTOM_CLASS
    #include "class/custom_class.h"
  #endif

#endif

//------------- DEVICE -------------//
#if MODE_DEVICE_SUPPORTED
  #include "device/usbd.h"

  #if DEVICE_CLASS_HID
    #include "class/hid_device.h"
  #endif

  #if TUSB_CFG_DEVICE_CDC
    #include "class/cdc_device.h"
  #endif

  #if TUSB_CFG_DEVICE_MSC
    #include "class/msc_device.h"
  #endif
#endif


//--------------------------------------------------------------------+
// APPLICATION API
//--------------------------------------------------------------------+
/** \ingroup group_application_api
 *  @{ */

/** \brief Initialize the usb stack
 * \return Error Code of the \ref TUSB_ERROR enum
 * \note   Function will initialize the stack according to configuration in the configure file (tusb_config.h)
 */
tusb_error_t tusb_init(void);

#if TUSB_CFG_OS == TUSB_OS_NONE
/** \brief Run all tinyusb's internal tasks (e.g host task, device task).
 * \note   This function is only required when using no RTOS (\ref TUSB_CFG_OS == TUSB_OS_NONE). All the stack functions
 *         & callback are invoked within this function, so it should be called periodically within the mainloop
 *
    @code
    int main(void)
    {
      your_init_code();
      tusb_init();

      while(1) // the mainloop
      {
        your_application_code();

        tusb_task_runner(); // handle tinyusb event, task etc ...
      }
    }
    @endcode
 *
 */
void tusb_task_runner(void);
#endif

/** @} */

#ifdef __cplusplus
 }
#endif

#endif /* _TUSB_H_ */

