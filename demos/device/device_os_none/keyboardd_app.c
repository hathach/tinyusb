/**************************************************************************/
/*!
    @file     keyboardd_app.c
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
    (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
    LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
    ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
    SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

    This file is part of the tinyusb stack.
*/
/**************************************************************************/

#include "keyboardd_app.h"

#if TUSB_CFG_DEVICE_HID_KEYBOARD
//--------------------------------------------------------------------+
// INCLUDE
//--------------------------------------------------------------------+

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF
//--------------------------------------------------------------------+

//--------------------------------------------------------------------+
// INTERNAL OBJECT & FUNCTION DECLARATION
//--------------------------------------------------------------------+
OSAL_TASK_DEF(keyboardd_app_task, 128, KEYBOARDD_APP_TASK_PRIO);

ATTR_USB_MIN_ALIGNMENT hid_keyboard_report_t keyboard_report TUSB_CFG_ATTR_USBRAM;

static uint8_t keyboardd_report_count; // number of reports sent each mounted

//--------------------------------------------------------------------+
// tinyusb Callbacks
//--------------------------------------------------------------------+
void tusbd_hid_keyboard_isr(uint8_t coreid, tusb_event_t event, uint32_t xferred_bytes)
{

}

void tusbd_hid_keyboard_mounted_cb(uint8_t coreid)
{
  keyboardd_report_count = 0;
}

//--------------------------------------------------------------------+
// APPLICATION CODE
//--------------------------------------------------------------------+
void keyboardd_app_init(void)
{
  ASSERT( TUSB_ERROR_NONE == osal_task_create( OSAL_TASK_REF(keyboardd_app_task) ), VOID_RETURN);
}

OSAL_TASK_FUNCTION( keyboardd_app_task ) (void* p_task_para)
{
  OSAL_TASK_LOOP_BEGIN

  if (tusbd_is_configured(0) && (keyboardd_report_count++ < 5) )
  {
    if (!tusbd_hid_keyboard_is_busy(0))
    {
      //------------- Key pressed -------------//
      keyboard_report.keycode[0] = 0x04;
      tusbd_hid_keyboard_send(0, &keyboard_report );

      while( tusbd_hid_keyboard_is_busy(0) )
      { // delay for transfer complete
        osal_task_delay(10);
      }

      //------------- Key released -------------//
      if (!tusbd_hid_keyboard_is_busy(0))
      {
        keyboard_report.keycode[0] = 0x00;
        tusbd_hid_keyboard_send(0, &keyboard_report );
      }
    }
  }

  osal_task_delay(1000);

  OSAL_TASK_LOOP_END
}

#endif
