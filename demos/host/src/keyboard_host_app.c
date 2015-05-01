/**************************************************************************/
/*!
    @file     keyboard_host_app.c
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

//--------------------------------------------------------------------+
// INCLUDE
//--------------------------------------------------------------------+
#include "keyboard_host_app.h"
#include "app_os_prio.h"

#if TUSB_CFG_HOST_HID_KEYBOARD

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF
//--------------------------------------------------------------------+
#define QUEUE_KEYBOARD_REPORT_DEPTH   4

//--------------------------------------------------------------------+
// INTERNAL OBJECT & FUNCTION DECLARATION
//--------------------------------------------------------------------+
OSAL_TASK_DEF(keyboard_host_app_task, 128, KEYBOARD_APP_TASK_PRIO);
OSAL_QUEUE_DEF(queue_kbd_def, QUEUE_KEYBOARD_REPORT_DEPTH, hid_keyboard_report_t);

static osal_queue_handle_t queue_kbd_hdl;
TUSB_CFG_ATTR_USBRAM static hid_keyboard_report_t usb_keyboard_report;

static inline uint8_t keycode_to_ascii(uint8_t modifier, uint8_t keycode) ATTR_CONST ATTR_ALWAYS_INLINE;
static inline void process_kbd_report(hid_keyboard_report_t const * report);

//--------------------------------------------------------------------+
// tinyusb callbacks
//--------------------------------------------------------------------+
void tuh_hid_keyboard_mounted_cb(uint8_t dev_addr)
{
  // application set-up
  printf("\na Keyboard device (address %d) is mounted\n", dev_addr);

  osal_queue_flush(queue_kbd_hdl);
  tuh_hid_keyboard_get_report(dev_addr, (uint8_t*) &usb_keyboard_report); // first report
}

void tuh_hid_keyboard_unmounted_cb(uint8_t dev_addr)
{
  // application tear-down
  printf("\na Keyboard device (address %d) is unmounted\n", dev_addr);
}

// invoked ISR context
void tuh_hid_keyboard_isr(uint8_t dev_addr, tusb_event_t event)
{
  switch(event)
  {
    case TUSB_EVENT_XFER_COMPLETE:
      (void) osal_queue_send(queue_kbd_hdl, &usb_keyboard_report);
      tuh_hid_keyboard_get_report(dev_addr, (uint8_t*) &usb_keyboard_report);
    break;

    case TUSB_EVENT_XFER_ERROR:
      tuh_hid_keyboard_get_report(dev_addr, (uint8_t*) &usb_keyboard_report); // ignore & continue
    break;

    default :
    break;
  }
}

//--------------------------------------------------------------------+
// APPLICATION
//--------------------------------------------------------------------+
void keyboard_host_app_init(void)
{
  memclr_(&usb_keyboard_report, sizeof(hid_keyboard_report_t));

  queue_kbd_hdl = osal_queue_create( OSAL_QUEUE_REF(queue_kbd_def) );
  ASSERT_PTR( queue_kbd_hdl, VOID_RETURN );

  ASSERT( TUSB_ERROR_NONE == osal_task_create( OSAL_TASK_REF(keyboard_host_app_task) ) ,
          VOID_RETURN);
}

//------------- main task -------------//
OSAL_TASK_FUNCTION( keyboard_host_app_task, p_task_para)
{
  (void) p_task_para;

  OSAL_TASK_LOOP_BEGIN

  hid_keyboard_report_t kbd_report;
  tusb_error_t error;

  osal_queue_receive(queue_kbd_hdl, &kbd_report, OSAL_TIMEOUT_WAIT_FOREVER, &error);
  (void) error; // suppress compiler warning

  process_kbd_report(&kbd_report);

  OSAL_TASK_LOOP_END
}

//--------------------------------------------------------------------+
// HELPER
//--------------------------------------------------------------------+
// look up new key in previous keys
static inline bool find_key_in_report(hid_keyboard_report_t const *p_report, uint8_t keycode)
{
  for(uint8_t i=0; i<6; i++)
  {
    if (p_report->keycode[i] == keycode)  return true;
  }

  return false;
}

static inline void process_kbd_report(hid_keyboard_report_t const *p_new_report)
{
  static hid_keyboard_report_t prev_report = { 0, 0, {0} }; // previous report to check key released

  //------------- example code ignore control (non-printable) key affects -------------//
  for(uint8_t i=0; i<6; i++)
  {
    if ( p_new_report->keycode[i] )
    {
      if ( find_key_in_report(&prev_report, p_new_report->keycode[i]) )
      {
        // exist in previous report means the current key is holding
      }else
      {
        // not existed in previous report means the current key is pressed
        uint8_t ch = keycode_to_ascii(p_new_report->modifier, p_new_report->keycode[i]);
        putchar(ch);
        if ( ch == '\r' ) putchar('\n'); // added new line for enter key
      }
    }
    // TODO example skips key released
  }

  prev_report = *p_new_report;
}

static inline uint8_t keycode_to_ascii(uint8_t modifier, uint8_t keycode)
{
  // TODO max of keycode_ascii_tbl
  return keycode > 128 ? 0 :
    hid_keycode_to_ascii_tbl [modifier & (KEYBOARD_MODIFIER_LEFTSHIFT | KEYBOARD_MODIFIER_RIGHTSHIFT) ? 1 : 0] [keycode];
}

#endif
