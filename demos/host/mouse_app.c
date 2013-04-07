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

//--------------------------------------------------------------------+
// INTERNAL OBJECT & FUNCTION DECLARATION
//--------------------------------------------------------------------+
//TUSB_CFG_ATTR_USBRAM
__attribute__ ((section(".data.$RAM3"))) // TODO hack for USB RAM
tusb_mouse_report_t mouse_report;

//--------------------------------------------------------------------+
// IMPLEMENTATION
//--------------------------------------------------------------------+
void mouse_app_task(void)
{
  for (uint8_t dev_addr = 1; dev_addr <= TUSB_CFG_HOST_DEVICE_MAX; dev_addr++)
  {
    if ( tusbh_hid_mouse_is_supported(dev_addr) )
    {
      switch (tusbh_hid_mouse_status(dev_addr,0))
      {
        case TUSB_INTERFACE_STATUS_READY:
        case TUSB_INTERFACE_STATUS_ERROR: // skip error, get next key
          tusbh_hid_mouse_get_report(dev_addr, 0, (uint8_t*) &mouse_report);
        break;

        case TUSB_INTERFACE_STATUS_COMPLETE:
          // TODO buffer in queue
          if ( mouse_report.buttons || mouse_report.x || mouse_report.y)
          {
            printf("buttons: %c%c%c    (x, y) = (%d, %d)\n",
                   mouse_report.buttons & HID_MOUSEBUTTON_LEFT   ? 'L' : '-',
                   mouse_report.buttons & HID_MOUSEBUTTON_MIDDLE ? 'M' : '-',
                   mouse_report.buttons & HID_MOUSEBUTTON_RIGHT  ? 'R' : '-',
                   mouse_report.x, mouse_report.y);
          }

          memclr_(&mouse_report, sizeof(tusb_mouse_report_t)); // TODO use callback to synchronize
          tusbh_hid_mouse_get_report(dev_addr, 0, (uint8_t*) &mouse_report);
        break;

        case TUSB_INTERFACE_STATUS_BUSY:
        break;

      }
    }
  }
}
