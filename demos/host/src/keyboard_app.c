/*
 * keyboard_app.c
 *
 *  Created on: Mar 24, 2013
 *      Author: hathach
 */

/*
 * Software License Agreement (BSD License)
 * Copyright (c) 2012, hathach (tinyusb.org)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
 * SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 *
 * This file is part of the tiny usb stack.
 */

//--------------------------------------------------------------------+
// INCLUDE
//--------------------------------------------------------------------+
#include "keyboard_app.h"

#if TUSB_CFG_HOST_HID_KEYBOARD

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF
//--------------------------------------------------------------------+
#define QUEUE_KEYBOARD_REPORT_DEPTH   5

//--------------------------------------------------------------------+
// INTERNAL OBJECT & FUNCTION DECLARATION
//--------------------------------------------------------------------+
OSAL_TASK_DEF(keyboard_task_def, "keyboard app", keyboard_app_task, 128, KEYBOARD_APP_TASK_PRIO);

OSAL_QUEUE_DEF(queue_kbd_report, QUEUE_KEYBOARD_REPORT_DEPTH, tusb_keyboard_report_t);
static osal_queue_handle_t q_kbd_report_hdl;

static tusb_keyboard_report_t usb_keyboard_report TUSB_CFG_ATTR_USBRAM;

// only convert a-z (case insensitive) +  0-9
static inline uint8_t keycode_to_ascii(uint8_t modifier, uint8_t keycode) ATTR_CONST ATTR_ALWAYS_INLINE;

//--------------------------------------------------------------------+
// tinyusb callback (ISR context)
//--------------------------------------------------------------------+
void tusbh_hid_keyboard_isr(uint8_t dev_addr, uint8_t instance_num, tusb_event_t event)
{
  switch(event)
  {
    case TUSB_EVENT_INTERFACE_OPEN: // application set-up
      osal_queue_flush(q_kbd_report_hdl);
      tusbh_hid_keyboard_get_report(dev_addr, instance_num, (uint8_t*) &usb_keyboard_report); // first report
    break;

    case TUSB_EVENT_INTERFACE_CLOSE: // application tear-down

    break;

    case TUSB_EVENT_XFER_COMPLETE:
      osal_queue_send(q_kbd_report_hdl, &usb_keyboard_report);
      tusbh_hid_keyboard_get_report(dev_addr, instance_num, (uint8_t*) &usb_keyboard_report);
    break;

    case TUSB_EVENT_XFER_ERROR:
      tusbh_hid_keyboard_get_report(dev_addr, instance_num, (uint8_t*) &usb_keyboard_report); // ignore & continue
    break;

    default :
    break;
  }
}

//--------------------------------------------------------------------+
// APPLICATION
//--------------------------------------------------------------------+
void keyboard_app_init(void)
{
  memclr_(&usb_keyboard_report, sizeof(tusb_keyboard_report_t));

  ASSERT( TUSB_ERROR_NONE == osal_task_create(&keyboard_task_def), (void) 0 );
  q_kbd_report_hdl = osal_queue_create(&queue_kbd_report);
  ASSERT_PTR( q_kbd_report_hdl, (void) 0 );
}

//------------- main task -------------//
OSAL_TASK_FUNCTION( keyboard_app_task ) (void* p_task_para)
{
  tusb_error_t error;
  static tusb_keyboard_report_t prev_kbd_report = { 0 }; // previous report to check key released
  tusb_keyboard_report_t kbd_report;

  OSAL_TASK_LOOP_BEGIN

  osal_queue_receive(q_kbd_report_hdl, &kbd_report, OSAL_TIMEOUT_WAIT_FOREVER, &error);

  //------------- example code ignore modifier key -------------//
  for(uint8_t i=0; i<6; i++)
  {
    if ( kbd_report.keycode[i] != prev_kbd_report.keycode[i] )
    {
      if ( 0 != kbd_report.keycode[i]) // key pressed
      {
        printf("%c", keycode_to_ascii(kbd_report.modifier, kbd_report.keycode[i]) );
      }else
      {
        // key released
      }
    }
    prev_kbd_report.keycode[i] = kbd_report.keycode[i];
  }

  OSAL_TASK_LOOP_END
}

//--------------------------------------------------------------------+
// HELPER
//--------------------------------------------------------------------+
static inline uint8_t keycode_to_ascii(uint8_t modifier, uint8_t keycode)
{
  // TODO max of keycode_ascii_tbl
  return keycode > 128 ? 0 :
    hid_keycode_to_ascii_tbl [modifier & (KEYBOARD_MODIFIER_LEFTSHIFT | KEYBOARD_MODIFIER_RIGHTSHIFT) ? 1 : 0] [keycode];
}

#endif
