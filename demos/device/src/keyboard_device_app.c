/**************************************************************************/
/*!
    @file     keyboard_device_app.c
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

#include "keyboard_device_app.h"

#if TUSB_CFG_DEVICE_HID_KEYBOARD
//--------------------------------------------------------------------+
// INCLUDE
//--------------------------------------------------------------------+
#include "app_os_prio.h"

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF
//--------------------------------------------------------------------+

//--------------------------------------------------------------------+
// INTERNAL OBJECT & FUNCTION DECLARATION
//--------------------------------------------------------------------+
OSAL_TASK_DEF(keyboard_device_app_task, 128, KEYBOARD_APP_TASK_PRIO);

TUSB_CFG_ATTR_USBRAM hid_keyboard_report_t keyboard_report;

//--------------------------------------------------------------------+
// tinyusb callbacks
//--------------------------------------------------------------------+
void tusbd_hid_keyboard_mounted_cb(uint8_t coreid)
{

}

void tusbd_hid_keyboard_unmounted_cb(uint8_t coreid)
{

}

void tusbd_hid_keyboard_cb(uint8_t coreid, tusb_event_t event, uint32_t xferred_bytes)
{
  switch(event)
  {
    case TUSB_EVENT_XFER_COMPLETE:
    case TUSB_EVENT_XFER_ERROR:
    case TUSB_EVENT_XFER_STALLED:
    default: break;
  }
}

uint16_t tusbd_hid_keyboard_get_report_cb(uint8_t coreid, hid_request_report_type_t report_type, void** pp_report, uint16_t requested_length)
{
  // get other than input report is not supported by this keyboard demo
  if ( report_type != HID_REQUEST_REPORT_INPUT ) return 0;

  (*pp_report) = &keyboard_report;
  return requested_length;
}

void tusbd_hid_keyboard_set_report_cb(uint8_t coreid, hid_request_report_type_t report_type, uint8_t p_report_data[], uint16_t length)
{
  // set other than output report is not supported by this keyboard demo
  if ( report_type != HID_REQUEST_REPORT_OUTPUT ) return;

  uint8_t kbd_led = p_report_data[0];
  uint32_t interval_divider = 1; // each LED will reduce blinking interval by a half

  if (kbd_led & KEYBOARD_LED_NUMLOCK   ) interval_divider *= 2;
  if (kbd_led & KEYBOARD_LED_CAPSLOCK  ) interval_divider *= 2;
  if (kbd_led & KEYBOARD_LED_SCROLLLOCK) interval_divider *= 2;

  led_blinking_set_interval( 1000 / interval_divider);
}

//--------------------------------------------------------------------+
// APPLICATION CODE
//--------------------------------------------------------------------+
void keyboard_device_app_init(void)
{
  ASSERT( TUSB_ERROR_NONE == osal_task_create( OSAL_TASK_REF(keyboard_device_app_task) ), VOID_RETURN);
}

OSAL_TASK_FUNCTION( keyboard_device_app_task , p_task_para)
{
  OSAL_TASK_LOOP_BEGIN

  osal_task_delay(50);

  if ( tusbd_is_configured(0) && !tusbd_hid_keyboard_is_busy(0) )
  {
    static uint32_t button_mask = 0;
    uint32_t new_button_mask = board_buttons();

    //------------- button pressed -------------//
    if (button_mask != new_button_mask)
    {
      button_mask = new_button_mask;

      for (uint8_t i=0; i<6; i++)
      { // demo support up to 6 buttons, button0 = 'a', button1 = 'b', etc ...
        keyboard_report.keycode[i] =  BIT_TEST_(button_mask, i) ? (0x04+i) : 0;
      }

      tusbd_hid_keyboard_send(0, &keyboard_report );
    }
  }


  OSAL_TASK_LOOP_END
}

#endif
