/**************************************************************************/
/*!
    @file     hid_host.c
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

#include "tusb_option.h"

#if (MODE_HOST_SUPPORTED && HOST_CLASS_HID)

#define _TINY_USB_SOURCE_FILE_
//--------------------------------------------------------------------+
// INCLUDE
//--------------------------------------------------------------------+
#include "common/common.h"
#include "hid_host.h"

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF
//--------------------------------------------------------------------+

//--------------------------------------------------------------------+
// HID Interface common functions
//--------------------------------------------------------------------+
static inline tusb_error_t hidh_interface_open(uint8_t dev_addr, uint8_t interface_number, tusb_descriptor_endpoint_t const *p_endpoint_desc, hidh_interface_info_t *p_hid) ATTR_ALWAYS_INLINE;
static inline tusb_error_t hidh_interface_open(uint8_t dev_addr, uint8_t interface_number, tusb_descriptor_endpoint_t const *p_endpoint_desc, hidh_interface_info_t *p_hid)
{
  p_hid->pipe_hdl         = hcd_pipe_open(dev_addr, p_endpoint_desc, TUSB_CLASS_HID);
  p_hid->report_size      = p_endpoint_desc->wMaxPacketSize.size; // TODO get size from report descriptor
  p_hid->interface_number = interface_number;

  ASSERT (pipehandle_is_valid(p_hid->pipe_hdl), TUSB_ERROR_HCD_FAILED);

  return TUSB_ERROR_NONE;
}

static inline void hidh_interface_close(hidh_interface_info_t *p_hid) ATTR_ALWAYS_INLINE;
static inline void hidh_interface_close(hidh_interface_info_t *p_hid)
{
  (void) hcd_pipe_close(p_hid->pipe_hdl);
  memclr_(p_hid, sizeof(hidh_interface_info_t));
}

// called from public API need to validate parameters
tusb_error_t hidh_interface_get_report(uint8_t dev_addr, void * report, hidh_interface_info_t *p_hid) ATTR_ALWAYS_INLINE;
tusb_error_t hidh_interface_get_report(uint8_t dev_addr, void * report, hidh_interface_info_t *p_hid)
{
  //------------- parameters validation -------------//
  // TODO change to use is configured function
  ASSERT_INT  (TUSB_DEVICE_STATE_CONFIGURED, tuh_device_get_state(dev_addr), TUSB_ERROR_DEVICE_NOT_READY);
  ASSERT_PTR  (report, TUSB_ERROR_INVALID_PARA);
  ASSERT_FALSE(hcd_pipe_is_busy(p_hid->pipe_hdl), TUSB_ERROR_INTERFACE_IS_BUSY);

  ASSERT_STATUS( hcd_pipe_xfer(p_hid->pipe_hdl, report, p_hid->report_size, true) ) ;

  return TUSB_ERROR_NONE;
}

//--------------------------------------------------------------------+
// KEYBOARD
//--------------------------------------------------------------------+
#if TUSB_CFG_HOST_HID_KEYBOARD

#define EXPAND_KEYCODE_TO_ASCII(keycode, ascii, shift_modified)  \
  [0][keycode] = ascii,\
  [1][keycode] = shift_modified,\

// TODO size of table should be a macro for application to check boundary
uint8_t const hid_keycode_to_ascii_tbl[2][128] =
{
    HID_KEYCODE_TABLE(EXPAND_KEYCODE_TO_ASCII)
};

STATIC_VAR hidh_interface_info_t keyboardh_data[TUSB_CFG_HOST_DEVICE_MAX]; // does not have addr0, index = dev_address-1

//------------- KEYBOARD PUBLIC API (parameter validation required) -------------//
bool  tuh_hid_keyboard_is_mounted(uint8_t dev_addr)
{
  return tuh_device_is_configured(dev_addr) && pipehandle_is_valid(keyboardh_data[dev_addr-1].pipe_hdl);
}

tusb_error_t tuh_hid_keyboard_get_report(uint8_t dev_addr, void* p_report)
{
  return hidh_interface_get_report(dev_addr, p_report, &keyboardh_data[dev_addr-1]);
}

bool tuh_hid_keyboard_is_busy(uint8_t dev_addr)
{
  return  tuh_hid_keyboard_is_mounted(dev_addr) &&
          hcd_pipe_is_busy( keyboardh_data[dev_addr-1].pipe_hdl );
}

#endif

//--------------------------------------------------------------------+
// MOUSE
//--------------------------------------------------------------------+
#if TUSB_CFG_HOST_HID_MOUSE

STATIC_VAR hidh_interface_info_t mouseh_data[TUSB_CFG_HOST_DEVICE_MAX]; // does not have addr0, index = dev_address-1

//------------- Public API -------------//
bool tuh_hid_mouse_is_mounted(uint8_t dev_addr)
{
  return tuh_device_is_configured(dev_addr) && pipehandle_is_valid(mouseh_data[dev_addr-1].pipe_hdl);
}

bool tuh_hid_mouse_is_busy(uint8_t dev_addr)
{
  return  tuh_hid_mouse_is_mounted(dev_addr) &&
          hcd_pipe_is_busy( mouseh_data[dev_addr-1].pipe_hdl );
}

tusb_error_t tuh_hid_mouse_get_report(uint8_t dev_addr, void * report)
{
  return hidh_interface_get_report(dev_addr, report, &mouseh_data[dev_addr-1]);
}

#endif

//--------------------------------------------------------------------+
// GENERIC
//--------------------------------------------------------------------+
#if TUSB_CFG_HOST_HID_GENERIC

//STATIC_ struct {
//  hidh_interface_info_t
//} generic_data[TUSB_CFG_HOST_DEVICE_MAX];

#endif

//--------------------------------------------------------------------+
// CLASS-USBH API (don't require to verify parameters)
//--------------------------------------------------------------------+
void hidh_init(void)
{
#if TUSB_CFG_HOST_HID_KEYBOARD
  memclr_(&keyboardh_data, sizeof(hidh_interface_info_t)*TUSB_CFG_HOST_DEVICE_MAX);
#endif

#if TUSB_CFG_HOST_HID_MOUSE
  memclr_(&mouseh_data, sizeof(hidh_interface_info_t)*TUSB_CFG_HOST_DEVICE_MAX);
#endif

#if TUSB_CFG_HOST_HID_GENERIC
  hidh_generic_init();
#endif
}

#if 0
TUSB_CFG_ATTR_USBRAM uint8_t report_descriptor[256];
#endif

tusb_error_t hidh_open_subtask(uint8_t dev_addr, tusb_descriptor_interface_t const *p_interface_desc, uint16_t *p_length)
{
  tusb_error_t error;
  uint8_t const *p_desc = (uint8_t const *) p_interface_desc;

  //------------- HID descriptor -------------//
  p_desc += p_desc[DESCRIPTOR_OFFSET_LENGTH];
  tusb_hid_descriptor_hid_t const *p_desc_hid = (tusb_hid_descriptor_hid_t const *) p_desc;
  ASSERT_INT(HID_DESC_TYPE_HID, p_desc_hid->bDescriptorType, TUSB_ERROR_INVALID_PARA);

  //------------- Endpoint Descriptor -------------//
  p_desc += p_desc[DESCRIPTOR_OFFSET_LENGTH];
  tusb_descriptor_endpoint_t const * p_endpoint_desc = (tusb_descriptor_endpoint_t const *) p_desc;
  ASSERT_INT(TUSB_DESC_TYPE_ENDPOINT, p_endpoint_desc->bDescriptorType, TUSB_ERROR_INVALID_PARA);

  OSAL_SUBTASK_BEGIN

  //------------- SET IDLE (0) request -------------//
  OSAL_SUBTASK_INVOKED_AND_WAIT(
    usbh_control_xfer_subtask( dev_addr, bm_request_type(TUSB_DIR_HOST_TO_DEV, TUSB_REQUEST_TYPE_CLASS, TUSB_REQUEST_RECIPIENT_INTERFACE),
                               HID_REQUEST_CONTROL_SET_IDLE, 0, p_interface_desc->bInterfaceNumber,
                               0, NULL ),
    error
  );
	(void) error; // skip if set idle is failed

#if 0
  //------------- Get Report Descriptor TODO HID parser -------------//
  if ( p_desc_hid->bNumDescriptors )
  {
    OSAL_SUBTASK_INVOKED_AND_WAIT(
        usbh_control_xfer_subtask( dev_addr, bm_request_type(TUSB_DIR_DEV_TO_HOST, TUSB_REQUEST_TYPE_STANDARD, TUSB_REQUEST_RECIPIENT_INTERFACE),
                                   TUSB_REQUEST_GET_DESCRIPTOR, (p_desc_hid->bReportType << 8), 0,
                                   p_desc_hid->wReportLength, report_descriptor ),
        error
    );
    (void) error; // if error in getting report descriptor --> treating like there is none
  }
#endif

  if ( HID_SUBCLASS_BOOT == p_interface_desc->bInterfaceSubClass )
  {
    #if TUSB_CFG_HOST_HID_KEYBOARD
    if ( HID_PROTOCOL_KEYBOARD == p_interface_desc->bInterfaceProtocol)
    {
      SUBTASK_ASSERT_STATUS ( hidh_interface_open(dev_addr, p_interface_desc->bInterfaceNumber, p_endpoint_desc, &keyboardh_data[dev_addr-1]) );
      tuh_hid_keyboard_mounted_cb(dev_addr);
    } else
    #endif

    #if TUSB_CFG_HOST_HID_MOUSE
    if ( HID_PROTOCOL_MOUSE == p_interface_desc->bInterfaceProtocol)
    {
      SUBTASK_ASSERT_STATUS ( hidh_interface_open(dev_addr, p_interface_desc->bInterfaceNumber, p_endpoint_desc, &mouseh_data[dev_addr-1]) );
      tuh_hid_mouse_mounted_cb(dev_addr);
    } else
    #endif

    {
      SUBTASK_EXIT(TUSB_ERROR_HIDH_NOT_SUPPORTED_PROTOCOL); // exit & restart task
    }
  }else
  {
    SUBTASK_EXIT(TUSB_ERROR_HIDH_NOT_SUPPORTED_SUBCLASS); // exit & restart task
  }

  *p_length = sizeof(tusb_descriptor_interface_t) + sizeof(tusb_hid_descriptor_hid_t) + sizeof(tusb_descriptor_endpoint_t);

  OSAL_SUBTASK_END
}

void hidh_isr(pipe_handle_t pipe_hdl, tusb_event_t event, uint32_t xferred_bytes)
{
  (void) xferred_bytes; // TODO may need to use this para later

#if TUSB_CFG_HOST_HID_KEYBOARD
  if ( pipehandle_is_equal(pipe_hdl, keyboardh_data[pipe_hdl.dev_addr-1].pipe_hdl) )
  {
    tuh_hid_keyboard_isr(pipe_hdl.dev_addr, event);
    return;
  }
#endif

#if TUSB_CFG_HOST_HID_MOUSE
  if ( pipehandle_is_equal(pipe_hdl, mouseh_data[pipe_hdl.dev_addr-1].pipe_hdl) )
  {
    tuh_hid_mouse_isr(pipe_hdl.dev_addr, event);
    return;
  }
#endif

#if TUSB_CFG_HOST_HID_GENERIC

#endif
}

void hidh_close(uint8_t dev_addr)
{
#if TUSB_CFG_HOST_HID_KEYBOARD
  if ( pipehandle_is_valid( keyboardh_data[dev_addr-1].pipe_hdl ) )
  {
    hidh_interface_close(&keyboardh_data[dev_addr-1]);
    tuh_hid_keyboard_unmounted_cb(dev_addr);
  }
#endif

#if TUSB_CFG_HOST_HID_MOUSE
  if( pipehandle_is_valid( mouseh_data[dev_addr-1].pipe_hdl ) )
  {
    hidh_interface_close(&mouseh_data[dev_addr-1]);
    tuh_hid_mouse_unmounted_cb( dev_addr );
  }
#endif

#if TUSB_CFG_HOST_HID_GENERIC
  hidh_generic_close(dev_addr);
#endif
}



#endif
