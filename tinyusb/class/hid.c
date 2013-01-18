/*
 * hid.c
 *
 *  Created on: Nov 27, 2012
 *      Author: hathach
 */

/*
 * Software License Agreement (BSD License)
 * Copyright (c) 2012, hathach (tinyusb.net)
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
 * This file is part of the tinyUSB stack.
 */

#include "hid.h"

#if defined DEVICE_CLASS_HID && defined TUSB_CFG_DEVICE

#ifdef TUSB_CFG_DEVICE_HID_KEYBOARD
tusb_keyboard_report_t hid_keyboard_report;
static volatile bool bKeyChanged = false;
#endif

#ifdef TUSB_CFG_DEVICE_HID_MOUSE
USB_HID_MouseReport_t hid_mouse_report;
static volatile bool bMouseChanged = false;
#endif

/**************************************************************************/
/*!
    @brief Handler for HID_GetReport in the USB ROM driver
*/
/**************************************************************************/
ErrorCode_t HID_GetReport( USBD_HANDLE_T hHid, USB_SETUP_PACKET* pSetup, uint8_t** pBuffer, uint16_t* plength)
{
  USB_HID_CTRL_T* pHidCtrl = (USB_HID_CTRL_T*) hHid;

  /* ReportID = SetupPacket.wValue.WB.L; */
  if (pSetup->wValue.WB.H == HID_REPORT_INPUT)
    return (ERR_USBD_STALL);          /* Not Supported */

  switch (pHidCtrl->protocol)
  {
    #ifdef TUSB_CFG_DEVICE_HID_KEYBOARD
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

    #ifdef TUSB_CFG_DEVICE_HID_MOUSE
      case HID_PROTOCOL_MOUSE:
        *pBuffer = (uint8_t*) &hid_mouse_report;
        *plength = sizeof(USB_HID_MouseReport_t);

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

/**************************************************************************/
/*!
    @brief Handler for HIS_SetReport in the USB ROM driver
*/
/**************************************************************************/
ErrorCode_t HID_SetReport( USBD_HANDLE_T hHid, USB_SETUP_PACKET* pSetup, uint8_t** pBuffer, uint16_t length)
{
  /* we will reuse standard EP0Buf */
  if (length == 0)
    return LPC_OK;

  /* ReportID = SetupPacket.wValue.WB.L; */
  if (pSetup->wValue.WB.H != HID_REPORT_OUTPUT)
    return (ERR_USBD_STALL);          /* Not Supported */

  return (LPC_OK);
}

/**************************************************************************/
/*!
    @brief HID endpoint in handler for the USB ROM driver
*/
/**************************************************************************/
ErrorCode_t HID_EpIn_Hdlr (USBD_HANDLE_T hUsb, void* data, uint32_t event)
{
  if (USB_EVT_IN == event)
  {
    USB_HID_CTRL_T* pHidCtrl = (USB_HID_CTRL_T*)data;
    switch(pHidCtrl->protocol)
    {
      #ifdef TUSB_CFG_DEVICE_HID_KEYBOARD
        case HID_PROTOCOL_KEYBOARD:
          if (!bKeyChanged)
          {
            memset(&hid_keyboard_report, 0, sizeof(tusb_keyboard_report_t));
          }
          USBD_API->hw->WriteEP(hUsb, pHidCtrl->epin_adr, (uint8_t*) &hid_keyboard_report, sizeof(tusb_keyboard_report_t));
          bKeyChanged = false;
        break;
      #endif

      #ifdef TUSB_CFG_DEVICE_HID_MOUSE
        case HID_PROTOCOL_MOUSE:
          if (!bMouseChanged)
          {
            memset(&hid_mouse_report, 0, sizeof(USB_HID_MouseReport_t));
          }
          USBD_API->hw->WriteEP(hUsb, pHidCtrl->epin_adr, (uint8_t*) &hid_mouse_report, sizeof(USB_HID_MouseReport_t));
          bMouseChanged = false;
        break;
      #endif

      default:
        break;
    }
  }

  return LPC_OK;
}

/**************************************************************************/
/*!
    @brief HID endpoint out handler for the USB ROM driver
*/
/**************************************************************************/
ErrorCode_t HID_EpOut_Hdlr (USBD_HANDLE_T hUsb, void* data, uint32_t event)
{
  if (USB_EVT_OUT == event)
  {
    // not used yet
    // uint8_t outreport[8];
    // USB_HID_CTRL_T* pHidCtrl = (USB_HID_CTRL_T*)data;
    // USBD_API->hw->ReadEP(hUsb, pHidCtrl->epout_adr, outreport);
  }
  return LPC_OK;
}

/**************************************************************************/
/*!
    @brief Initialises USB HID using the ROM based drivers
*/
/**************************************************************************/
TUSB_Error_t tusb_hid_init(USBD_HANDLE_T hUsb, USB_INTERFACE_DESCRIPTOR const *const pIntfDesc, uint8_t const * const pHIDReportDesc, uint32_t ReportDescLength, uint32_t* mem_base, uint32_t* mem_size)
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

  ASSERT( LPC_OK == USBD_API->hid->init(hUsb, &hid_param), tERROR_FAILED );

  /* update memory variables */
  *mem_base += (*mem_size - hid_param.mem_size);
  *mem_size = hid_param.mem_size;

  return tERROR_NONE;
}

/**************************************************************************/
/*!

*/
/**************************************************************************/
TUSB_Error_t tusb_hid_configured(USBD_HANDLE_T hUsb)
{
  #ifdef  TUSB_CFG_DEVICE_HID_KEYBOARD
    USBD_API->hw->WriteEP(hUsb , HID_KEYBOARD_EP_IN , (uint8_t* ) &hid_keyboard_report , sizeof(tusb_keyboard_report_t) ); // initial packet for IN endpoint , will not work if omitted
  #endif

  #ifdef  TUSB_CFG_DEVICE_HID_MOUSE
    USBD_API->hw->WriteEP(hUsb , HID_MOUSE_EP_IN    , (uint8_t* ) &hid_mouse_report    , sizeof(USB_HID_MouseReport_t) ); // initial packet for IN endpoint, will not work if omitted
  #endif

  return tERROR_NONE;
}

#ifdef TUSB_CFG_DEVICE_HID_KEYBOARD
/**************************************************************************/
/*!
    @brief Send the supplied key codes out via HID USB keyboard emulation

    @param[in]  modifier
                KB modifier code bits (see USB_HID_KB_KEYMODIFIER_CODE)
    @param[in]  keycodes
                A buffer containing up to six keycodes
    @param[in]  numkey
                The number of keys to send (max 6)

    @note Note that for HID KBs, letter codes are not case sensitive. To
          create an upper-case letter, you need to include the correct
          KB modifier code(s), for ex: (1 << HID_KEYMODIFIER_LEFTSHIFT)

    @section EXAMPLE

    @code

    // Send an unmodified 'a' character
    if (usb_isConfigured())
    {
      uint8_t keys[6] = {HID_USAGE_KEYBOARD_aA};
      tusb_hid_keyboard_sendKeys(0x00, keys, 1);
    }

    // Send Windows + 'e' (shortcut for 'explorer.exe')
    if (usb_isConfigured())
    {
      uint8_t keys[6] = {HID_USAGE_KEYBOARD_aA + 'e' - 'a'};
      tusb_hid_keyboard_sendKeys((1<<HID_KEYMODIFIER_LEFTGUI), keys, 1);
    }

    @endcode
*/
/**************************************************************************/
TUSB_Error_t tusb_hid_keyboard_sendKeys(uint8_t modifier, uint8_t keycodes[], uint8_t numkey)
{
//  uint32_t start_time = systickGetSecondsActive();
//  while (bKeyChanged) // TODO blocking while previous key has yet sent - can use fifo to improve this
//  {
//    ASSERT_MESSAGE(systickGetSecondsActive() - start_time < 5, ERR_FAILED, "HID Keyboard Timeout");
//  }

  if (bKeyChanged)
  {
    return tERROR_FAILED;
  }

  ASSERT(keycodes && numkey && numkey <=6, ERR_FAILED);

  hid_keyboard_report.Modifier = modifier;
  memset(hid_keyboard_report.KeyCode, 0, 6);
  memcpy(hid_keyboard_report.KeyCode, keycodes, numkey);

  bKeyChanged = true;

  return tERROR_NONE;
}
#endif

#ifdef TUSB_CFG_DEVICE_HID_MOUSE
/**************************************************************************/
/*!
    @brief Send the supplied mouse event out via HID USB mouse emulation

    @param[in]  buttons
                Indicate which button(s) are being pressed (see
                USB_HID_MOUSE_BUTTON_CODE)
    @param[in]  x
                Position adjustment on the X scale
    @param[in]  y
                Position adjustment on the Y scale

    @section EXAMPLE

    @code

    if (usb_isConfigured())
    {
      // Move the mouse +10 in the X direction and + 10 in the Y direction
      tusb_hid_mouse_send(0x00, 10, 10);
    }

    @endcode
*/
/**************************************************************************/
TUSB_Error_t tusb_hid_mouse_send(uint8_t buttons, int8_t x, int8_t y)
{
//  uint32_t start_time = systickGetSecondsActive();
//  while (bMouseChanged) // TODO Block while previous key hasn't been sent - can use fifo to improve this
//  {
//    ASSERT_MESSAGE(systickGetSecondsActive() - start_time < 5, ERR_FAILED, "HID Mouse Timeout");
//  }

  if (bMouseChanged)
  {
    return tERROR_FAILED;
  }

  hid_mouse_report.Button = buttons;
  hid_mouse_report.X = x;
  hid_mouse_report.Y = y;

  bMouseChanged = true;

  return tERROR_NONE;
}
#endif

#endif
