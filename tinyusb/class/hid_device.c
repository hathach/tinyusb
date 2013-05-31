/**************************************************************************/
/*!
    @file     hid_device.c
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

#include "tusb_option.h"

#if (MODE_DEVICE_SUPPORTED && DEVICE_CLASS_HID)

#define _TINY_USB_SOURCE_FILE_
//--------------------------------------------------------------------+
// INCLUDE
//--------------------------------------------------------------------+
#include "common/common.h"
#include "hid_device.h"

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF
//--------------------------------------------------------------------+

//--------------------------------------------------------------------+
// INTERNAL OBJECT & FUNCTION DECLARATION
//--------------------------------------------------------------------+

//--------------------------------------------------------------------+
// IMPLEMENTATION
//--------------------------------------------------------------------+
#if TUSB_CFG_DEVICE_HID_KEYBOARD
TUSB_CFG_ATTR_USBRAM tusb_keyboard_report_t hid_keyboard_report;
static volatile bool bKeyChanged = false;
#endif

#if TUSB_CFG_DEVICE_HID_MOUSE
TUSB_CFG_ATTR_USBRAM tusb_mouse_report_t hid_mouse_report;
static volatile bool bMouseChanged = false;
#endif

ErrorCode_t HID_GetReport( USBD_HANDLE_T hHid, USB_SETUP_PACKET* pSetup, uint8_t** pBuffer, uint16_t* plength)
{
  USB_HID_CTRL_T* pHidCtrl = (USB_HID_CTRL_T*) hHid;

  /* ReportID = SetupPacket.wValue.WB.L; */
  if (pSetup->wValue.WB.H == HID_REQUEST_REPORT_INPUT)
    return (ERR_USBD_STALL);          /* Not Supported */

  switch (pHidCtrl->protocol)
  {
    #if TUSB_CFG_DEVICE_HID_KEYBOARD
      case HID_PROTOCOL_KEYBOARD:
        *pBuffer = (uint8_t*) &hid_keyboard_report;
        *plength = sizeof(tusb_keyboard_report_t);

        if (!bKeyChanged)
        {
          memset(pBuffer, 0, *plength);
        }
        bKeyChanged = false;
      break;
    #endif

    #if TUSB_CFG_DEVICE_HID_MOUSE
      case HID_PROTOCOL_MOUSE:
        *pBuffer = (uint8_t*) &hid_mouse_report;
        *plength = sizeof(tusb_mouse_report_t);

        if (!bMouseChanged)
        {
          memset(pBuffer, 0, *plength);
        }
        bMouseChanged = false;
      break;
    #endif

    default:
      break;
  }

  return (LPC_OK);
}

ErrorCode_t HID_SetReport( USBD_HANDLE_T hHid, USB_SETUP_PACKET* pSetup, uint8_t** pBuffer, uint16_t length)
{
  /* we will reuse standard EP0Buf */
  if (length == 0)
    return LPC_OK;

  /* ReportID = SetupPacket.wValue.WB.L; */
  if (pSetup->wValue.WB.H != HID_REQUEST_REPORT_OUTPUT)
    return (ERR_USBD_STALL);          /* Not Supported */

  return (LPC_OK);
}

ErrorCode_t HID_EpIn_Hdlr (USBD_HANDLE_T hUsb, void* data, uint32_t event)
{
  if (USB_EVT_IN == event)
  {
    USB_HID_CTRL_T* pHidCtrl = (USB_HID_CTRL_T*)data;
    switch(pHidCtrl->protocol)
    {
      #if TUSB_CFG_DEVICE_HID_KEYBOARD
        case HID_PROTOCOL_KEYBOARD:
          if (!bKeyChanged)
          {
            memset(&hid_keyboard_report, 0, sizeof(tusb_keyboard_report_t));
          }
          ROM_API->hw->WriteEP(hUsb, pHidCtrl->epin_adr, (uint8_t*) &hid_keyboard_report, sizeof(tusb_keyboard_report_t));
          bKeyChanged = false;
        break;
      #endif

      #if TUSB_CFG_DEVICE_HID_MOUSE
        case HID_PROTOCOL_MOUSE:
          if (!bMouseChanged)
          {
            memset(&hid_mouse_report, 0, sizeof(tusb_mouse_report_t));
          }
          ROM_API->hw->WriteEP(hUsb, pHidCtrl->epin_adr, (uint8_t*) &hid_mouse_report, sizeof(tusb_mouse_report_t));
          bMouseChanged = false;
        break;
      #endif

      default:
        break;
    }
  }

  return LPC_OK;
}

ErrorCode_t HID_EpOut_Hdlr (USBD_HANDLE_T hUsb, void* data, uint32_t event)
{
  if (USB_EVT_OUT == event)
  {
    // not used yet
    // uint8_t outreport[8];
    // USB_HID_CTRL_T* pHidCtrl = (USB_HID_CTRL_T*)data;
    // ROM_API->hw->ReadEP(hUsb, pHidCtrl->epout_adr, outreport);
  }
  return LPC_OK;
}

//--------------------------------------------------------------------+
// CLASS-USBH API (don't require to verify parameters)
//--------------------------------------------------------------------+
tusb_error_t hidd_init(USBD_HANDLE_T hUsb, tusb_descriptor_interface_t const *const pIntfDesc, uint8_t const * const pHIDReportDesc, uint32_t ReportDescLength, uint32_t* mem_base, uint32_t* mem_size)
{
  USB_HID_REPORT_T reports_data =
  {
      .desc      = (uint8_t*) pHIDReportDesc,
      .len       = ReportDescLength,
      .idle_time = 0,
  };

  USBD_HID_INIT_PARAM_T hid_param =
  {
      .mem_base       = *mem_base,
      .mem_size       = *mem_size,

      .intf_desc      = (uint8_t*)pIntfDesc,
      .report_data    = &reports_data,
      .max_reports    = 1,

      /* user defined functions */
      .HID_GetReport  = HID_GetReport,
      .HID_SetReport  = HID_SetReport,
      .HID_EpIn_Hdlr  = HID_EpIn_Hdlr,
      .HID_EpOut_Hdlr = HID_EpOut_Hdlr
  };

  ASSERT( (pIntfDesc != NULL) && (pIntfDesc->bInterfaceClass == USB_DEVICE_CLASS_HUMAN_INTERFACE), ERR_FAILED);

  ASSERT( LPC_OK == ROM_API->hid->init(hUsb, &hid_param), TUSB_ERROR_FAILED );

  /* update memory variables */
  *mem_base += (*mem_size - hid_param.mem_size);
  *mem_size = hid_param.mem_size;

  return TUSB_ERROR_NONE;
}

tusb_error_t hidd_configured(USBD_HANDLE_T hUsb)
{
  #if  TUSB_CFG_DEVICE_HID_KEYBOARD
    ROM_API->hw->WriteEP(hUsb , HID_KEYBOARD_EP_IN , (uint8_t* ) &hid_keyboard_report , sizeof(tusb_keyboard_report_t) ); // initial packet for IN endpoint , will not work if omitted
  #endif

  #if  TUSB_CFG_DEVICE_HID_MOUSE
    ROM_API->hw->WriteEP(hUsb , HID_MOUSE_EP_IN    , (uint8_t* ) &hid_mouse_report    , sizeof(tusb_mouse_report_t) ); // initial packet for IN endpoint, will not work if omitted
  #endif

  return TUSB_ERROR_NONE;
}

#if TUSB_CFG_DEVICE_HID_KEYBOARD
tusb_error_t tusbd_hid_keyboard_send_report(tusb_keyboard_report_t *p_kbd_report)
{
//  uint32_t start_time = systickGetSecondsActive();
//  while (bKeyChanged) // TODO blocking while previous key has yet sent - can use fifo to improve this
//  {
//    ASSERT_MESSAGE(systickGetSecondsActive() - start_time < 5, ERR_FAILED, "HID Keyboard Timeout");
//  }

  if (bKeyChanged)
  {
    return TUSB_ERROR_FAILED;
  }

  ASSERT_PTR(p_kbd_report, TUSB_ERROR_FAILED);

  hid_keyboard_report = *p_kbd_report;
  bKeyChanged = true;

  return TUSB_ERROR_NONE;
}
#endif

#if TUSB_CFG_DEVICE_HID_MOUSE
tusb_error_t tusb_hid_mouse_send(uint8_t buttons, int8_t x, int8_t y)
{
//  uint32_t start_time = systickGetSecondsActive();
//  while (bMouseChanged) // TODO Block while previous key hasn't been sent - can use fifo to improve this
//  {
//    ASSERT_MESSAGE(systickGetSecondsActive() - start_time < 5, ERR_FAILED, "HID Mouse Timeout");
//  }

  if (bMouseChanged)
  {
    return TUSB_ERROR_FAILED;
  }

  hid_mouse_report.buttons = buttons;
  hid_mouse_report.x = x;
  hid_mouse_report.y = y;

  bMouseChanged = true;

  return TUSB_ERROR_NONE;
}
#endif

#endif
