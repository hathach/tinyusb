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
#include "mouse_host_app.h"
#include "app_os_prio.h"

#if CFG_TUH_HID_MOUSE

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF
//--------------------------------------------------------------------+
#define QUEUE_MOUSE_REPORT_DEPTH   4

//--------------------------------------------------------------------+
// INTERNAL OBJECT & FUNCTION DECLARATION
//--------------------------------------------------------------------+
static osal_queue_t queue_mouse_hdl;
CFG_TUSB_MEM_SECTION static hid_mouse_report_t usb_mouse_report;

static inline void process_mouse_report(hid_mouse_report_t const * p_report);

//--------------------------------------------------------------------+
// tinyusb callbacks
//--------------------------------------------------------------------+
void tuh_hid_mouse_mounted_cb(uint8_t dev_addr)
{
  // application set-up
  printf("\na Mouse device (address %d) is mounted\n", dev_addr);

  osal_queue_flush(queue_mouse_hdl);
  (void) tuh_hid_mouse_get_report(dev_addr, (uint8_t*) &usb_mouse_report); // first report
}

void tuh_hid_mouse_unmounted_cb(uint8_t dev_addr)
{
  // application tear-down
  printf("\na Mouse device (address %d) is unmounted\n", dev_addr);
}

// invoked ISR context
void tuh_hid_mouse_isr(uint8_t dev_addr, xfer_result_t event)
{
  switch(event)
  {
    case XFER_RESULT_SUCCESS:
      osal_queue_send(queue_mouse_hdl, &usb_mouse_report);
      (void) tuh_hid_mouse_get_report(dev_addr, (uint8_t*) &usb_mouse_report);
    break;

    case XFER_RESULT_FAILED:
      (void) tuh_hid_mouse_get_report(dev_addr, (uint8_t*) &usb_mouse_report); // ignore & continue
    break;

    default :
    break;
  }
}

//--------------------------------------------------------------------+
// APPLICATION CODE
// NOTICE: MOUSE REPORT IS NOT CORRECT UNTIL A DECENT HID PARSER IS
// IMPLEMENTED, MEANWHILE IT CAN MISS DISPLAY BUTTONS OR X,Y etc
//--------------------------------------------------------------------+
void mouse_host_app_init(void)
{
  tu_memclr(&usb_mouse_report, sizeof(hid_mouse_report_t));

  queue_mouse_hdl = osal_queue_create( QUEUE_MOUSE_REPORT_DEPTH, sizeof(hid_mouse_report_t) );
  TU_ASSERT( queue_mouse_hdl, VOID_RETURN);

  TU_VERIFY( osal_task_create(mouse_host_app_task, "mouse", 128, NULL, MOUSE_APP_TASK_PRIO), );
}

//------------- main task -------------//
void mouse_host_app_task(void* param)
{
  (void) param;

  OSAL_TASK_BEGIN

  tusb_error_t error;
  hid_mouse_report_t mouse_report;

  osal_queue_receive(queue_mouse_hdl, &mouse_report, OSAL_TIMEOUT_WAIT_FOREVER, &error);
	(void) error; // suppress compiler's warnings
	
  process_mouse_report(&mouse_report);

  OSAL_TASK_END
}

//--------------------------------------------------------------------+
// HELPER
//--------------------------------------------------------------------+
void cursor_movement(int8_t x, int8_t y, int8_t wheel)
{
  //------------- X -------------//
  if ( x < 0)
  {
    printf(ANSI_CURSOR_BACKWARD(%d), (-x)); // move left
  }else if ( x > 0)
  {
    printf(ANSI_CURSOR_FORWARD(%d), x); // move right
  }else { }

  //------------- Y -------------//
  if ( y < 0)
  {
    printf(ANSI_CURSOR_UP(%d), (-y)); // move up
  }else if ( y > 0)
  {
    printf(ANSI_CURSOR_DOWN(%d), y); // move down
  }else { }

  //------------- wheel -------------//
  if (wheel < 0)
  {
    printf(ANSI_SCROLL_UP(%d), (-wheel)); // scroll up
  }else if (wheel > 0)
  {
    printf(ANSI_SCROLL_DOWN(%d), wheel); // scroll down
  }else { }
}

static inline void process_mouse_report(hid_mouse_report_t const * p_report)
{
  static hid_mouse_report_t prev_report = { 0, 0, 0, 0 };

  //------------- button state  -------------//
  uint8_t button_changed_mask = p_report->buttons ^ prev_report.buttons;
  if ( button_changed_mask & p_report->buttons)
  {
    printf(" %c%c%c ",
       p_report->buttons & MOUSE_BUTTON_LEFT   ? 'L' : '-',
       p_report->buttons & MOUSE_BUTTON_MIDDLE ? 'M' : '-',
       p_report->buttons & MOUSE_BUTTON_RIGHT  ? 'R' : '-');
  }

  //------------- cursor movement -------------//
  cursor_movement(p_report->x, p_report->y, p_report->wheel);
}

#endif
