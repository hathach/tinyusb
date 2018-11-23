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

#if CFG_TUD_HID_KEYBOARD
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
CFG_TUSB_MEM_SECTION hid_keyboard_report_t keyboard_report;

//--------------------------------------------------------------------+
// tinyusb callbacks
//--------------------------------------------------------------------+
void keyboard_app_mount(uint8_t rhport)
{

}

void keyboard_app_umount(uint8_t rhport)
{

}

void tud_hid_keyboard_cb(uint8_t rhport, xfer_result_t event, uint32_t xferred_bytes)
{
  switch(event)
  {
    case XFER_RESULT_SUCCESS:
    case XFER_RESULT_FAILED:
    case XFER_RESULT_STALLED:
    default: break;
  }
}

uint16_t tud_hid_keyboard_get_report_cb(hid_report_type_t report_type, uint8_t* buffer, uint16_t reqlen)
{
  // get other than input report is not supported by this keyboard demo
  if ( report_type != HID_REPORT_TYPE_INPUT ) return 0;

  memcpy(buffer, &keyboard_report, reqlen);
  return reqlen;
}

void tud_hid_keyboard_set_report_cb(hid_report_type_t report_type, uint8_t* buffer, uint16_t reqlen)
{
  // set other than output report is not supported by this keyboard demo
  if ( report_type != HID_REPORT_TYPE_OUTPUT ) return;

  uint8_t kbd_led = buffer[0];
  uint32_t interval_divider = 1; // each LED will reduce blinking interval by a half

  if (kbd_led & KEYBOARD_LED_NUMLOCK   ) interval_divider *= 2;
  if (kbd_led & KEYBOARD_LED_CAPSLOCK  ) interval_divider *= 2;
  if (kbd_led & KEYBOARD_LED_SCROLLLOCK) interval_divider *= 2;

  // TODO remove
  extern void led_blinking_set_interval(uint32_t ms);
  led_blinking_set_interval( 1000 / interval_divider);
}

//--------------------------------------------------------------------+
// APPLICATION CODE
//--------------------------------------------------------------------+
void keyboard_app_init(void)
{
  osal_task_create(keyboard_app_task, "kbd", 128, NULL, KEYBOARD_APP_TASK_PRIO);
}

tusb_error_t keyboard_device_subtask(void);

void keyboard_app_task(void* param)
{
  (void) param;

  OSAL_TASK_BEGIN
  keyboard_device_subtask();
  OSAL_TASK_END
}

tusb_error_t keyboard_device_subtask(void)
{
  OSAL_SUBTASK_BEGIN

  osal_task_delay(50);

  if ( tud_mounted() && tud_hid_keyboard_ready(0) )
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

      tud_hid_keyboard_send(0, &keyboard_report );
    }
  }

  OSAL_SUBTASK_END
}

#endif
