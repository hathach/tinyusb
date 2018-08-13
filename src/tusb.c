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
#include "device/usbd_pvt.h"

static bool _initialized = false;


tusb_error_t tusb_init(void)
{
  // skip if already initialized
  if (_initialized) return TUSB_ERROR_NONE;

  TU_VERIFY( tusb_hal_init(), TUSB_ERROR_FAILED ) ; // hardware init

#if MODE_HOST_SUPPORTED
  TU_ASSERT_ERR( usbh_init() ); // host stack init
#endif

#if TUSB_OPT_DEVICE_ENABLED
  TU_ASSERT_ERR ( usbd_init() ); // device stack init
#endif

  _initialized = true;

  return TUSB_ERROR_NONE;
}

#if CFG_TUSB_OS == OPT_OS_NONE
void tusb_task(void)
{
  #if MODE_HOST_SUPPORTED
  usbh_enumeration_task(NULL);
  #endif

  #if TUSB_OPT_DEVICE_ENABLED
  usbd_task(NULL);
  #endif
}
#endif


/*------------------------------------------------------------------*/
/* Debug
 *------------------------------------------------------------------*/
#if CFG_TUSB_DEBUG

char const* const tusb_strerr[TUSB_ERROR_COUNT] =
{
 ERROR_TABLE(ERROR_STRING)
};

#endif
