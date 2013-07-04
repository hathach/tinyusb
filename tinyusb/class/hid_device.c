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
#include "tusb_descriptors.h"

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF
//--------------------------------------------------------------------+
typedef struct {
  tusb_descriptor_interface_t const * p_interface_desc;
  tusb_hid_descriptor_hid_t const * p_hid_desc;
  uint8_t const * p_report_desc;
//  volatile tusb_interface_status_t status;
}hidd_interface_t;

#if TUSB_CFG_DEVICE_HID_KEYBOARD
STATIC_ hidd_interface_t keyboard_intf =
{
    .p_interface_desc = &app_tusb_desc_configuration.keyboard_interface,
    .p_hid_desc       = &app_tusb_desc_configuration.keyboard_hid,
    .p_report_desc    = app_tusb_keyboard_desc_report
};
#endif

tusb_error_t hidd_control_request(uint8_t coreid, tusb_control_request_t const * p_request)
{
#if TUSB_CFG_DEVICE_HID_KEYBOARD
  if (p_request->bmRequestType.type == TUSB_REQUEST_TYPE_STANDARD) // standard request to hid
  {
    uint8_t const desc_type  = u16_high_u8(p_request->wValue);
    uint8_t const desc_index = u16_low_u8 (p_request->wValue);

    if ( p_request->bRequest == TUSB_REQUEST_GET_DESCRIPTOR &&
        desc_type == HID_DESC_TYPE_REPORT)
    {
      dcd_pipe_control_write(coreid, keyboard_intf.p_report_desc,
                             keyboard_intf.p_hid_desc->wReportLength);
    }else
    {
      ASSERT_STATUS(TUSB_ERROR_FAILED);
    }
  }else if ( p_request->wIndex == keyboard_intf.p_interface_desc->bInterfaceNumber) // class request
  {
    switch(p_request->bRequest)
    {
      case HID_REQUEST_CONTROL_SET_IDLE:
        // TODO hidd idle rate, no data phase
      break;

      case HID_REQUEST_CONTROL_SET_REPORT:
        // TODO hidd set report, has data phase
// TODO verify data read from control pipe
//        ; uint8_t hehe[10]= { 0 };
//        dcd_pipe_control_read(coreid, hehe, p_request->wLength);
      break;

      case HID_REQUEST_CONTROL_GET_REPORT:
      case HID_REQUEST_CONTROL_GET_IDLE:
      case HID_REQUEST_CONTROL_GET_PROTOCOL:
      case HID_REQUEST_CONTROL_SET_PROTOCOL:
      default:
        ASSERT_STATUS(TUSB_ERROR_NOT_SUPPORTED_YET);
        return TUSB_ERROR_NOT_SUPPORTED_YET;
    }
  }else
  {
    ASSERT_STATUS(TUSB_ERROR_FAILED);
  }
#endif

  return TUSB_ERROR_NONE;
}

tusb_error_t hidd_init(uint8_t coreid, tusb_descriptor_interface_t const * p_interface_desc, uint16_t *p_length)
{
  uint8_t const *p_desc = (uint8_t const *) p_interface_desc;

  //------------- HID descriptor -------------//
  p_desc += p_desc[DESCRIPTOR_OFFSET_LENGTH];
  tusb_hid_descriptor_hid_t const *p_desc_hid = (tusb_hid_descriptor_hid_t const *) p_desc;
  ASSERT_INT(HID_DESC_TYPE_HID, p_desc_hid->bDescriptorType, TUSB_ERROR_HIDD_DESCRIPTOR_INTERFACE);

  //------------- Endpoint Descriptor -------------//
  p_desc += p_desc[DESCRIPTOR_OFFSET_LENGTH];
  tusb_descriptor_endpoint_t const *p_desc_endpoint = (tusb_descriptor_endpoint_t const *) p_desc;
  ASSERT_INT(TUSB_DESC_TYPE_ENDPOINT, p_desc_endpoint->bDescriptorType, TUSB_ERROR_HIDD_DESCRIPTOR_INTERFACE);

   if (p_interface_desc->bInterfaceSubClass == HID_SUBCLASS_BOOT)
  {
    switch(p_interface_desc->bInterfaceProtocol)
    {
      #if TUSB_CFG_DEVICE_HID_KEYBOARD
      case HID_PROTOCOL_KEYBOARD:
        ASSERT_STATUS( dcd_pipe_open(coreid, p_desc_endpoint) );
      break;
      #endif

      #if TUSB_CFG_DEVICE_HID_MOUSE
      case HID_PROTOCOL_MOUSE:
        ASSERT_STATUS( hidd_interface_init(p_interface_desc,
                                           app_tusb_mouse_desc_report, p_desc_hid->wReportLength,
                                           hidd_mouse_buffer , sizeof(hidd_mouse_buffer)) );
      break;
      #endif

      default: // TODO unknown, unsupported protocol --> skip this interface
        return TUSB_ERROR_HIDD_DESCRIPTOR_INTERFACE;
    }
    *p_length = sizeof(tusb_descriptor_interface_t) + sizeof(tusb_hid_descriptor_hid_t) + sizeof(tusb_descriptor_endpoint_t);
  }else
  {
    // open generic
    *p_length = 0;
    return TUSB_ERROR_HIDD_DESCRIPTOR_INTERFACE;
  }


  return TUSB_ERROR_NONE;
}

#if defined(CAP_DEVICE_ROMDRIVER) && TUSB_CFG_DEVICE_USE_ROM_DRIVER
#include "device/dcd_nxp_romdriver.h" // TODO remove rom driver dependency


//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF
//--------------------------------------------------------------------+
#if TUSB_CFG_DEVICE_HID_KEYBOARD
TUSB_CFG_ATTR_USBRAM uint8_t hidd_keyboard_buffer[1024]; // TODO memory reduce
TUSB_CFG_ATTR_USBRAM tusb_keyboard_report_t hid_keyboard_report;
static volatile bool bKeyChanged = false;
#endif

#if TUSB_CFG_DEVICE_HID_MOUSE
TUSB_CFG_ATTR_USBRAM uint8_t hidd_mouse_buffer[1024]; // TODO memory reduce
TUSB_CFG_ATTR_USBRAM tusb_mouse_report_t hid_mouse_report;
static volatile bool bMouseChanged = false;
#endif

//--------------------------------------------------------------------+
// INTERNAL OBJECT & FUNCTION DECLARATION
//--------------------------------------------------------------------+
static tusb_error_t hidd_interface_init(tusb_descriptor_interface_t const *p_interface_desc, uint8_t const * const p_report_desc,
                                 uint32_t report_length, uint8_t* mem_base, uint32_t mem_size);

ErrorCode_t HID_GetReport( USBD_HANDLE_T hHid, USB_SETUP_PACKET* pSetup, uint8_t** pBuffer, uint16_t* plength);
ErrorCode_t HID_SetReport( USBD_HANDLE_T hHid, USB_SETUP_PACKET* pSetup, uint8_t** pBuffer, uint16_t length);
ErrorCode_t HID_EpIn_Hdlr (USBD_HANDLE_T hUsb, void* data, uint32_t event);
ErrorCode_t HID_EpOut_Hdlr (USBD_HANDLE_T hUsb, void* data, uint32_t event);


//--------------------------------------------------------------------+
// APPLICATION API
//--------------------------------------------------------------------+
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
tusb_error_t tusbd_hid_mouse_send_report(tusb_mouse_report_t *p_mouse_report)
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

  hid_mouse_report = *p_mouse_report;
  bMouseChanged = true;

  return TUSB_ERROR_NONE;
}
#endif

//--------------------------------------------------------------------+
// CLASS-USBH API (don't require to verify parameters)
//--------------------------------------------------------------------+
tusb_error_t hidd_configured(void)
{
  #if  TUSB_CFG_DEVICE_HID_KEYBOARD
    ROM_API->hw->WriteEP(romdriver_hdl , HID_KEYBOARD_EP_IN , (uint8_t* ) &hid_keyboard_report , sizeof(tusb_keyboard_report_t) ); // initial packet for IN endpoint , will not work if omitted
  #endif

  #if  TUSB_CFG_DEVICE_HID_MOUSE
    ROM_API->hw->WriteEP(romdriver_hdl , HID_MOUSE_EP_IN    , (uint8_t* ) &hid_mouse_report    , sizeof(tusb_mouse_report_t) ); // initial packet for IN endpoint, will not work if omitted
  #endif

  return TUSB_ERROR_NONE;
}

tusb_error_t hidd_init(tusb_descriptor_interface_t const * p_interface_desc, uint16_t *p_length)
{
  uint8_t const *p_desc = (uint8_t const *) p_interface_desc;

  //------------- HID descriptor -------------//
  p_desc += p_desc[DESCRIPTOR_OFFSET_LENGTH];
  tusb_hid_descriptor_hid_t const *p_desc_hid = (tusb_hid_descriptor_hid_t const *) p_desc;
  ASSERT_INT(HID_DESC_TYPE_HID, p_desc_hid->bDescriptorType, TUSB_ERROR_HIDD_DESCRIPTOR_INTERFACE);

   if (p_interface_desc->bInterfaceSubClass == HID_SUBCLASS_BOOT)
  {
    switch(p_interface_desc->bInterfaceProtocol)
    {
      #if TUSB_CFG_DEVICE_HID_KEYBOARD
      case HID_PROTOCOL_KEYBOARD:
        ASSERT_STATUS( hidd_interface_init(p_interface_desc,
                                           app_tusb_keyboard_desc_report, p_desc_hid->wReportLength,
                                           hidd_keyboard_buffer , sizeof(hidd_keyboard_buffer)) );
      break;
      #endif

      #if TUSB_CFG_DEVICE_HID_MOUSE
      case HID_PROTOCOL_MOUSE:
        ASSERT_STATUS( hidd_interface_init(p_interface_desc,
                                           app_tusb_mouse_desc_report, p_desc_hid->wReportLength,
                                           hidd_mouse_buffer , sizeof(hidd_mouse_buffer)) );
      break;
      #endif

      default: // TODO unknown, unsupported protocol --> skip this interface
        return TUSB_ERROR_HIDD_DESCRIPTOR_INTERFACE;
    }
    *p_length = sizeof(tusb_descriptor_interface_t) + sizeof(tusb_hid_descriptor_hid_t) + sizeof(tusb_descriptor_endpoint_t);
  }else
  {
    // open generic
    *p_length = 0;
    return TUSB_ERROR_HIDD_DESCRIPTOR_INTERFACE;
  }


  return TUSB_ERROR_NONE;
}

tusb_error_t hidd_interface_init(tusb_descriptor_interface_t const *p_interface_desc, uint8_t const * const p_report_desc,
                                 uint32_t report_length, uint8_t* mem_base, uint32_t mem_size)
{
  USB_HID_REPORT_T reports_data =
  {
      .desc      = (uint8_t*) p_report_desc,
      .len       = report_length,
      .idle_time = 0,
  };

  USBD_HID_INIT_PARAM_T hid_param =
  {
      .mem_base       = (uint32_t) mem_base,
      .mem_size       = mem_size,

      .intf_desc      = (uint8_t*)p_interface_desc,
      .report_data    = &reports_data,
      .max_reports    = 1,

      /* user defined functions */
      .HID_GetReport  = HID_GetReport,
      .HID_SetReport  = HID_SetReport,
      .HID_EpIn_Hdlr  = HID_EpIn_Hdlr,
      .HID_EpOut_Hdlr = HID_EpOut_Hdlr
  };

  ASSERT( LPC_OK == ROM_API->hid->init(romdriver_hdl, &hid_param), TUSB_ERROR_FAILED );

  return TUSB_ERROR_NONE;
}

//--------------------------------------------------------------------+
// IMPLEMENTATION
//--------------------------------------------------------------------+
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
#endif

#endif
