/**************************************************************************/
/*!
    @file     mouse_host_app.c
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
#include "mouse_host_app.h"
#include "app_os_prio.h"

#if TUSB_CFG_HOST_HID_MOUSE

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF
//--------------------------------------------------------------------+
#define QUEUE_MOUSE_REPORT_DEPTH   4

//--------------------------------------------------------------------+
// INTERNAL OBJECT & FUNCTION DECLARATION
//--------------------------------------------------------------------+
OSAL_TASK_DEF(mouse_host_app_task, 128, MOUSE_APP_TASK_PRIO);
OSAL_QUEUE_DEF(queue_mouse_def, QUEUE_MOUSE_REPORT_DEPTH, hid_mouse_report_t);

static osal_queue_handle_t queue_mouse_hdl;
TUSB_CFG_ATTR_USBRAM static hid_mouse_report_t usb_mouse_report;

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
void tuh_hid_mouse_isr(uint8_t dev_addr, tusb_event_t event)
{
  switch(event)
  {
    case TUSB_EVENT_XFER_COMPLETE:
      (void) osal_queue_send(queue_mouse_hdl, &usb_mouse_report);
      (void) tuh_hid_mouse_get_report(dev_addr, (uint8_t*) &usb_mouse_report);
    break;

    case TUSB_EVENT_XFER_ERROR:
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
  memclr_(&usb_mouse_report, sizeof(hid_mouse_report_t));

  queue_mouse_hdl = osal_queue_create( OSAL_QUEUE_REF(queue_mouse_def) );
  ASSERT_PTR( queue_mouse_hdl, VOID_RETURN);

  ASSERT( TUSB_ERROR_NONE == osal_task_create( OSAL_TASK_REF(mouse_host_app_task) ),
          VOID_RETURN );
}

//------------- main task -------------//
OSAL_TASK_FUNCTION( mouse_host_app_task, p_task_para)
{
  (void) p_task_para;

  OSAL_TASK_LOOP_BEGIN

  tusb_error_t error;
  hid_mouse_report_t mouse_report;

  osal_queue_receive(queue_mouse_hdl, &mouse_report, OSAL_TIMEOUT_WAIT_FOREVER, &error);
	(void) error; // suppress compiler's warnings
	
  process_mouse_report(&mouse_report);

  OSAL_TASK_LOOP_END
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
