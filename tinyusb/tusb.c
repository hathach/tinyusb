/**************************************************************************/
/*!
    @file     tusb.c
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

#define _TINY_USB_SOURCE_FILE_

#include "tusb.h"

tusb_error_t tusb_init(void)
{
  ASSERT_STATUS( hal_init() ) ; // hardware init

#if MODE_HOST_SUPPORTED
  ASSERT_STATUS( usbh_init() ); // host stack init
#endif

#if MODE_DEVICE_SUPPORTED
  ASSERT_STATUS ( usbd_init() ); // device stack init
#endif

#if (TUSB_CFG_CONTROLLER_0_MODE)
  hal_interrupt_enable(0);
#endif

#if (TUSB_CFG_CONTROLLER_1_MODE)
  hal_interrupt_enable(1);
#endif

  return TUSB_ERROR_NONE;
}

/** \ingroup group_application_api
 * \brief USB interrupt handler
 * \param[in]  coreid Controller ID where the interrupt happened
 * \note This function must be called by HAL layer or Application for the stack to manage USB events/transfers.
 */
void tusb_isr(uint8_t coreid)
{
#if MODE_HOST_SUPPORTED
  hcd_isr(coreid);
#endif

#if MODE_DEVICE_SUPPORTED
  dcd_isr(coreid);
#endif

}

#if TUSB_CFG_OS == TUSB_OS_NONE
void tusb_task_runner(void)
{
  #if MODE_HOST_SUPPORTED
  usbh_enumeration_task(NULL);
  #endif

  #if MODE_DEVICE_SUPPORTED
  usbd_task(NULL);
  #endif
}
#endif
