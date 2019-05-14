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

//--------------------------------------------------------------------+
// INCLUDE
//--------------------------------------------------------------------+
#include "keyboard_host_app.h"
#include "app_os_prio.h"

#if CFG_TUH_HID_KEYBOARD

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF
//--------------------------------------------------------------------+
#define QUEUE_KEYBOARD_REPORT_DEPTH   4

//--------------------------------------------------------------------+
// INTERNAL OBJECT & FUNCTION DECLARATION
//--------------------------------------------------------------------+
static osal_queue_t queue_kbd_hdl;
CFG_TUSB_MEM_SECTION static hid_keyboard_report_t usb_keyboard_report;

static inline uint8_t keycode_to_ascii(uint8_t modifier, uint8_t keycode);
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
void tuh_hid_keyboard_isr(uint8_t dev_addr, xfer_result_t event)
{
  switch(event)
  {
    case XFER_RESULT_SUCCESS:
      osal_queue_send(queue_kbd_hdl, &usb_keyboard_report);
      tuh_hid_keyboard_get_report(dev_addr, (uint8_t*) &usb_keyboard_report);
    break;

    case XFER_RESULT_FAILED:
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
  tu_memclr(&usb_keyboard_report, sizeof(hid_keyboard_report_t));

  queue_kbd_hdl = osal_queue_create( QUEUE_KEYBOARD_REPORT_DEPTH, sizeof(hid_keyboard_report_t) );
  TU_ASSERT( queue_kbd_hdl, VOID_RETURN );

  TU_VERIFY( osal_task_create(keyboard_host_app_task, "kbd", 128, NULL, KEYBOARD_APP_TASK_PRIO), );
}

//------------- main task -------------//
void keyboard_host_app_task(void* param)
{
  (void) param;

  OSAL_TASK_BEGIN

  hid_keyboard_report_t kbd_report;
  tusb_error_t error;

  osal_queue_receive(queue_kbd_hdl, &kbd_report, OSAL_TIMEOUT_WAIT_FOREVER, &error);
  (void) error; // suppress compiler warning

  process_kbd_report(&kbd_report);

  OSAL_TASK_END
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
