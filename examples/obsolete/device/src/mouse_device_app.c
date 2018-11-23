  /**************************************************************************/
/*!
    @file     mouse_device_app.c
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

#include "mouse_device_app.h"

#if CFG_TUD_HID_MOUSE
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
CFG_TUSB_MEM_SECTION hid_mouse_report_t mouse_report;

//--------------------------------------------------------------------+
// tinyusb callbacks
//--------------------------------------------------------------------+
void mouse_app_mount(uint8_t rhport)
{

}

void mouse_app_umount(uint8_t rhport)
{

}

void tud_hid_mouse_cb(uint8_t rhport, xfer_result_t event, uint32_t xferred_bytes)
{
  switch(event)
  {
    case TUSB_EVENT_XFER_COMPLETE:
    case TUSB_EVENT_XFER_ERROR:
    case TUSB_EVENT_XFER_STALLED:
    default: break;
  }
}

uint16_t tud_hid_mouse_get_report_cb(hid_report_type_t report_type, uint8_t* buffer, uint16_t reqlen)
{
  if ( report_type != HID_REPORT_TYPE_INPUT ) return 0; // not support other report type for this mouse demo

  memcpy(buffer, &mouse_report, reqlen);
  return reqlen;
}

//--------------------------------------------------------------------+
// APPLICATION CODE
//--------------------------------------------------------------------+
void mouse_app_init(void)
{
  osal_task_create(mouse_app_task, "mouse", 128, NULL, MOUSE_APP_TASK_PRIO);
}

void mouse_app_subtask(void);

void mouse_app_task(void * param)
{
  (void) param;
  OSAL_TASK_BEGIN
  mouse_app_subtask();
  OSAL_TASK_END
}

void mouse_app_subtask(void)
{
  OSAL_SUBTASK_BEGIN

  osal_task_delay(20);

  if ( tud_mounted() &&  !tud_hid_mouse_is_busy(0) )
  {
    static uint8_t prev_mouse_buttons = 0;

    enum {
      BUTTON_UP           = 0, // map to button mask
      BUTTON_DOWN         = 1,
      BUTTON_LEFT         = 2,
      BUTTON_RIGHT        = 3,
      BUTTON_CLICK_LEFT   = 4,
      BUTTON_CLICK_RIGHT  = 5,
      BUTTON_CLICK_MIDDLE = 6
    };

    enum { MOUSE_RESOLUTION  = 5 };

    uint32_t button_mask = board_buttons();
    tu_memclr(&mouse_report, sizeof(hid_mouse_report_t));

    if ( BIT_TEST_(button_mask, BUTTON_UP           ) ) mouse_report.y = -MOUSE_RESOLUTION;
    if ( BIT_TEST_(button_mask, BUTTON_DOWN         ) ) mouse_report.y = MOUSE_RESOLUTION;

    if ( BIT_TEST_(button_mask, BUTTON_LEFT         ) ) mouse_report.x = -MOUSE_RESOLUTION;
    if ( BIT_TEST_(button_mask, BUTTON_RIGHT        ) ) mouse_report.x = MOUSE_RESOLUTION;

    if ( BIT_TEST_(button_mask, BUTTON_CLICK_LEFT   ) ) mouse_report.buttons |= MOUSE_BUTTON_LEFT;
    if ( BIT_TEST_(button_mask, BUTTON_CLICK_RIGHT  ) ) mouse_report.buttons |= MOUSE_BUTTON_RIGHT;
    if ( BIT_TEST_(button_mask, BUTTON_CLICK_MIDDLE ) ) mouse_report.buttons |= MOUSE_BUTTON_MIDDLE;

    if ( ! (prev_mouse_buttons == mouse_report.buttons && mouse_report.y == 0 && mouse_report.x == 0 && mouse_report.wheel == 0) )
    { // send report if clicked buttons are changed or there is any movement x, y, wheel
      prev_mouse_buttons = mouse_report.buttons;

      tud_hid_mouse_send(0, &mouse_report);
    }
  }

  OSAL_SUBTASK_END
}

#endif

