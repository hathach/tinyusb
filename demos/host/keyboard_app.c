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
//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF
//--------------------------------------------------------------------+

//--------------------------------------------------------------------+
// INTERNAL OBJECT & FUNCTION DECLARATION
//--------------------------------------------------------------------+
tusb_keyboard_report_t keyboard_report TUSB_CFG_ATTR_USBRAM;

//--------------------------------------------------------------------+
// IMPLEMENTATION
//--------------------------------------------------------------------+

// only convert a-z (case insensitive) +  0-9
static inline uint8_t keycode_to_ascii(uint8_t keycode) ATTR_CONST ATTR_ALWAYS_INLINE;
static inline uint8_t keycode_to_ascii(uint8_t keycode)
{
  return
      ( KEYBOARD_KEYCODE_a <= keycode && keycode <= KEYBOARD_KEYCODE_z) ? ( (keycode - KEYBOARD_KEYCODE_a) + 'a' ) :
      ( KEYBOARD_KEYCODE_1 <= keycode && keycode < KEYBOARD_KEYCODE_0)  ? ( (keycode - KEYBOARD_KEYCODE_1) + '1' ) :
      ( KEYBOARD_KEYCODE_0 == keycode)                                  ? '0' : 'x';
}


//--------------------------------------------------------------------+
// tinyusb callback (ISR context)
//--------------------------------------------------------------------+
//void tusbh_hid_keyboard_isr(uint8_t dev_addr, uint8_t instance_num, tusb_event_t event)
//{
//  switch(event)
//  {
//    case TUSB_EVENT_INTERFACE_OPEN:
//      tusbh_hid_keyboard_get_report(dev_addr, 0, (uint8_t*) &keyboard_report); // first report
//    break;
//
//    case TUSB_EVENT_INTERFACE_CLOSE:
//
//    break;
//
//    case TUSB_EVENT_XFER_COMPLETE:
//      tusbh_hid_keyboard_get_report(dev_addr, 0, (uint8_t*) &keyboard_report);
//    break;
//
//    case TUSB_EVENT_XFER_ERROR:
//    break;
//
//    default:
//  }
//}

void keyboard_app_task(void)
{
  for (uint8_t dev_addr = 1; dev_addr <= TUSB_CFG_HOST_DEVICE_MAX; dev_addr++)
  {
    if ( tusbh_hid_keyboard_is_supported(dev_addr) )
    {
      switch (tusbh_hid_keyboard_status(dev_addr,0))
      {
        case TUSB_INTERFACE_STATUS_READY:
        case TUSB_INTERFACE_STATUS_ERROR: // skip error, get next key
          tusbh_hid_keyboard_get_report(dev_addr, 0, (uint8_t*) &keyboard_report);
        break;

        case TUSB_INTERFACE_STATUS_COMPLETE:
          // TODO buffer in queue
          for(uint8_t i=0; i<6; i++)
          {
            if ( keyboard_report.keycode[i] != 0 )
              printf("%c", keycode_to_ascii(keyboard_report.keycode[i]));
          }
          memclr_(&keyboard_report, sizeof(tusb_keyboard_report_t)); // TODO use callback to synchronize
          tusbh_hid_keyboard_get_report(dev_addr, 0, (uint8_t*) &keyboard_report);
        break;

        case TUSB_INTERFACE_STATUS_BUSY:
        break;

      }
    }
  }
}
