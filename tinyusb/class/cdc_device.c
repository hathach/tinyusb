/**************************************************************************/
/*!
    @file     cdc_device.c
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

#if (MODE_DEVICE_SUPPORTED && TUSB_CFG_DEVICE_CDC)

#define _TINY_USB_SOURCE_FILE_
//--------------------------------------------------------------------+
// INCLUDE
//--------------------------------------------------------------------+
#include "common/common.h"
#include "cdc_device.h"

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF
//--------------------------------------------------------------------+
TUSB_CFG_ATTR_USBRAM STATIC_VAR cdc_line_coding_t cdcd_line_coding[CONTROLLER_DEVICE_NUMBER];

typedef struct {
  uint8_t interface_number;
  cdc_acm_capability_t acm_capability;

  endpoint_handle_t edpt_hdl[3]; // notification, data in, data out
}cdcd_data_t;

//--------------------------------------------------------------------+
// INTERNAL OBJECT & FUNCTION DECLARATION
//--------------------------------------------------------------------+
STATIC_VAR cdcd_data_t cdcd_data[CONTROLLER_DEVICE_NUMBER];

static tusb_error_t cdcd_xfer(uint8_t coreid,  cdc_pipeid_t pipeid, void * p_buffer, uint32_t length, bool is_notify)
{
  ASSERT(tusbd_is_configured(coreid), TUSB_ERROR_USBD_DEVICE_NOT_CONFIGURED);

  cdcd_data_t* p_cdc = &cdcd_data[coreid];

  ASSERT_FALSE ( dcd_pipe_is_busy(p_cdc->edpt_hdl[pipeid]), TUSB_ERROR_INTERFACE_IS_BUSY);
  ASSERT_STATUS( dcd_pipe_xfer(p_cdc->edpt_hdl[pipeid], p_buffer, length, is_notify) );

  return TUSB_ERROR_NONE;
}

//--------------------------------------------------------------------+
// APPLICATION API (Parameters requires validation)
//--------------------------------------------------------------------+
bool tusbd_cdc_is_busy(uint8_t coreid, cdc_pipeid_t pipeid)
{
  return dcd_pipe_is_busy( cdcd_data[coreid].edpt_hdl[pipeid] );
}

tusb_error_t tusbd_cdc_receive(uint8_t coreid, void * p_buffer, uint32_t length, bool is_notify)
{
  return cdcd_xfer(coreid, CDC_PIPE_DATA_OUT, p_buffer, length, is_notify);
}

tusb_error_t tusbd_cdc_send(uint8_t coreid, void * p_data, uint32_t length, bool is_notify)
{
  return cdcd_xfer(coreid, CDC_PIPE_DATA_IN, p_data, length, is_notify);
}

//--------------------------------------------------------------------+
// USBD-CLASS API
//--------------------------------------------------------------------+
void cdcd_init(void)
{
  memclr_(cdcd_data, sizeof(cdcd_data_t)*CONTROLLER_DEVICE_NUMBER);

  // default line coding is : stop bit = 1, parity = none, data bits = 8
  memclr_(cdcd_line_coding, sizeof(cdc_line_coding_t)*CONTROLLER_DEVICE_NUMBER);
  for(uint8_t i=0; i<CONTROLLER_DEVICE_NUMBER; i++) cdcd_line_coding[i].data_bits = 8;
}

tusb_error_t cdcd_open(uint8_t coreid, tusb_descriptor_interface_t const * p_interface_desc, uint16_t *p_length)
{
  if ( CDC_COMM_SUBCLASS_ABSTRACT_CONTROL_MODEL != p_interface_desc->bInterfaceSubClass) return TUSB_ERROR_CDC_UNSUPPORTED_SUBCLASS;

  if ( !(is_in_range(CDC_COMM_PROTOCOL_ATCOMMAND, p_interface_desc->bInterfaceProtocol, CDC_COMM_PROTOCOL_ATCOMMAND_CDMA) ||
         0xff == p_interface_desc->bInterfaceProtocol) )
  {
    return TUSB_ERROR_CDC_UNSUPPORTED_PROTOCOL;
  }

  uint8_t const * p_desc = descriptor_next ( (uint8_t const *) p_interface_desc );
  cdcd_data_t * p_cdc = &cdcd_data[coreid];

  //------------- Communication Interface -------------//
  (*p_length) = sizeof(tusb_descriptor_interface_t);

  while( TUSB_DESC_TYPE_INTERFACE_CLASS_SPECIFIC == p_desc[DESCRIPTOR_OFFSET_TYPE] )
  { // Communication Functional Descriptors
    if ( CDC_FUNC_DESC_ABSTRACT_CONTROL_MANAGEMENT == cdc_functional_desc_typeof(p_desc) )
    { // save ACM bmCapabilities
      p_cdc->acm_capability = ((cdc_desc_func_abstract_control_management_t const *) p_desc)->bmCapabilities;
    }

    (*p_length) += p_desc[DESCRIPTOR_OFFSET_LENGTH];
    p_desc = descriptor_next(p_desc);
  }

  if ( TUSB_DESC_TYPE_ENDPOINT == p_desc[DESCRIPTOR_OFFSET_TYPE])
  { // notification endpoint if any
    p_cdc->edpt_hdl[CDC_PIPE_NOTIFICATION] = dcd_pipe_open(coreid, (tusb_descriptor_endpoint_t const *) p_desc, TUSB_CLASS_CDC);

    (*p_length) += p_desc[DESCRIPTOR_OFFSET_LENGTH];
    p_desc = descriptor_next(p_desc);

    ASSERT(endpointhandle_is_valid(p_cdc->edpt_hdl[CDC_PIPE_NOTIFICATION]), TUSB_ERROR_DCD_OPEN_PIPE_FAILED);
  }

  //------------- Data Interface (if any) -------------//
  if ( (TUSB_DESC_TYPE_INTERFACE == p_desc[DESCRIPTOR_OFFSET_TYPE]) &&
       (TUSB_CLASS_CDC_DATA      == ((tusb_descriptor_interface_t const *) p_desc)->bInterfaceClass) )
  {
    (*p_length) += p_desc[DESCRIPTOR_OFFSET_LENGTH];
    p_desc = descriptor_next(p_desc);

    // data endpoints expected to be in pairs
    for(uint32_t i=0; i<2; i++)
    {
      tusb_descriptor_endpoint_t const *p_endpoint = (tusb_descriptor_endpoint_t const *) p_desc;
      ASSERT_INT(TUSB_DESC_TYPE_ENDPOINT, p_endpoint->bDescriptorType, TUSB_ERROR_DESCRIPTOR_CORRUPTED);
      ASSERT_INT(TUSB_XFER_BULK, p_endpoint->bmAttributes.xfer, TUSB_ERROR_DESCRIPTOR_CORRUPTED);

      endpoint_handle_t * p_edpt_hdl =  ( p_endpoint->bEndpointAddress &  TUSB_DIR_DEV_TO_HOST_MASK ) ?
          &p_cdc->edpt_hdl[CDC_PIPE_DATA_IN] : &p_cdc->edpt_hdl[CDC_PIPE_DATA_OUT] ;

      (*p_edpt_hdl) = dcd_pipe_open(coreid, p_endpoint, TUSB_CLASS_CDC);
      ASSERT ( endpointhandle_is_valid(*p_edpt_hdl), TUSB_ERROR_DCD_OPEN_PIPE_FAILED );

      (*p_length) += p_desc[DESCRIPTOR_OFFSET_LENGTH];
      p_desc = descriptor_next( p_desc );
    }
  }

  p_cdc->interface_number   = p_interface_desc->bInterfaceNumber;

  tusbd_cdc_mounted_cb(coreid);

  return TUSB_ERROR_NONE;
}

void cdcd_close(uint8_t coreid)
{
  // no need to close opened pipe, dcd bus reset will put controller's endpoints to default state
  memclr_(&cdcd_data[coreid], sizeof(cdcd_data_t));

  tusbd_cdc_unmounted_cb(coreid);
}

tusb_error_t cdcd_control_request_subtask(uint8_t coreid, tusb_control_request_t const * p_request)
{
  //------------- Class Specific Request -------------//
  if (p_request->bmRequestType_bit.type != TUSB_REQUEST_TYPE_CLASS) return TUSB_ERROR_DCD_CONTROL_REQUEST_NOT_SUPPORT;

  switch(p_request->bRequest)
  {
    case CDC_REQUEST_GET_LINE_CODING:
      dcd_pipe_control_xfer(coreid, (tusb_direction_t) p_request->bmRequestType_bit.direction,
                            (uint8_t*) &cdcd_line_coding[coreid], min16_of(sizeof(cdc_line_coding_t), p_request->wLength), false );
    break;

    case CDC_REQUEST_SET_LINE_CODING:
      dcd_pipe_control_xfer(coreid, (tusb_direction_t) p_request->bmRequestType_bit.direction,
                            (uint8_t*) &cdcd_line_coding[coreid], min16_of(sizeof(cdc_line_coding_t), p_request->wLength), false );
      // TODO notify application on xfer completea
    break;

    case CDC_REQUEST_SET_CONTROL_LINE_STATE: // TODO extract DTE present
    break;

    default: return TUSB_ERROR_DCD_CONTROL_REQUEST_NOT_SUPPORT;
  }

  return TUSB_ERROR_NONE;
}

tusb_error_t cdcd_xfer_cb(endpoint_handle_t edpt_hdl, tusb_event_t event, uint32_t xferred_bytes)
{
  cdcd_data_t const * p_cdc = &cdcd_data[edpt_hdl.coreid];

  for(cdc_pipeid_t pipeid=CDC_PIPE_NOTIFICATION; pipeid < CDC_PIPE_ERROR; pipeid++ )
  {
    if ( endpointhandle_is_equal(edpt_hdl, p_cdc->edpt_hdl[pipeid]) )
    {
      tusbd_cdc_xfer_cb(edpt_hdl.coreid, event, pipeid, xferred_bytes);
      break;
    }
  }

  return TUSB_ERROR_NONE;
}

#endif
