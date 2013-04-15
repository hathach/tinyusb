/*
 * mouse_app.c
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
#include "mouse_app.h"

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF
//--------------------------------------------------------------------+
#define QUEUE_MOUSE_REPORT_DEPTH   5

//--------------------------------------------------------------------+
// INTERNAL OBJECT & FUNCTION DECLARATION
//--------------------------------------------------------------------+
static tusb_mouse_report_t usb_mouse_report TUSB_CFG_ATTR_USBRAM;

OSAL_QUEUE_DEF(queue_mouse_report, QUEUE_MOUSE_REPORT_DEPTH, tusb_mouse_report_t);
static osal_queue_handle_t q_mouse_report_hdl;

//--------------------------------------------------------------------+
// tinyusb callback (ISR context)
//--------------------------------------------------------------------+
void tusbh_hid_mouse_isr(uint8_t dev_addr, uint8_t instance_num, tusb_event_t event)
{
  switch(event)
  {
    case TUSB_EVENT_INTERFACE_OPEN: // application set-up
      osal_queue_flush(q_mouse_report_hdl);
      tusbh_hid_mouse_get_report(dev_addr, instance_num, (uint8_t*) &usb_mouse_report); // first report
    break;

    case TUSB_EVENT_INTERFACE_CLOSE: // application tear-down

    break;

    case TUSB_EVENT_XFER_COMPLETE:
      osal_queue_send(q_mouse_report_hdl, &usb_mouse_report);
      tusbh_hid_mouse_get_report(dev_addr, instance_num, (uint8_t*) &usb_mouse_report);
    break;

    case TUSB_EVENT_XFER_ERROR:
      tusbh_hid_mouse_get_report(dev_addr, instance_num, (uint8_t*) &usb_mouse_report); // ignore & continue
    break;

    default :
    break;
  }
}

//--------------------------------------------------------------------+
// APPLICATION
// NOTICE: MOUSE REPORT IS NOT CORRECT UNTIL A DECENT HID PARSER IS
// IMPLEMENTED, MEANWHILE IT CAN MISS DISPLAY BUTTONS OR X,Y etc
//--------------------------------------------------------------------+
void mouse_app_init(void)
{
  q_mouse_report_hdl = osal_queue_create(&queue_mouse_report);

  // TODO mouse_app_task create
}

//------------- main task -------------//
OSAL_TASK_DECLARE( mouse_app_task )
{
  tusb_error_t error;
  tusb_mouse_report_t mouse_report;

  OSAL_TASK_LOOP_BEGIN

  osal_queue_receive(q_mouse_report_hdl, &mouse_report, OSAL_TIMEOUT_WAIT_FOREVER, &error);

  if ( mouse_report.buttons || mouse_report.x || mouse_report.y)
  {
      printf("buttons: %c%c%c    (x, y) = (%d, %d)\n",
             mouse_report.buttons & HID_MOUSEBUTTON_LEFT   ? 'L' : '-',
             mouse_report.buttons & HID_MOUSEBUTTON_MIDDLE ? 'M' : '-',
             mouse_report.buttons & HID_MOUSEBUTTON_RIGHT  ? 'R' : '-',
             mouse_report.x, mouse_report.y);
  }

  memclr_(&mouse_report, sizeof(tusb_mouse_report_t) );

  OSAL_TASK_LOOP_END
}
