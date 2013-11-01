/**************************************************************************/
/*!
    @file     msc_device.c
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

#if (MODE_DEVICE_SUPPORTED && TUSB_CFG_DEVICE_MSC)

#define _TINY_USB_SOURCE_FILE_
//--------------------------------------------------------------------+
// INCLUDE
//--------------------------------------------------------------------+
#include "common/common.h"
#include "msc_device.h"
#include "tusb_descriptors.h"

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF
//--------------------------------------------------------------------+
typedef struct {
  uint8_t interface_number;
  endpoint_handle_t edpt_in, edpt_out;

  // must be in USB ram
  uint8_t max_lun;
  msc_cmd_block_wrapper_t  cbw;
  msc_cmd_status_wrapper_t csw;
}mscd_interface_t;

STATIC_VAR mscd_interface_t mscd_data TUSB_CFG_ATTR_USBRAM;
//--------------------------------------------------------------------+
// INTERNAL OBJECT & FUNCTION DECLARATION
//--------------------------------------------------------------------+

//--------------------------------------------------------------------+
// USBD-CLASS API
//--------------------------------------------------------------------+
tusb_error_t mscd_open(uint8_t coreid, tusb_descriptor_interface_t const * p_interface_desc, uint16_t *p_length)
{
  ASSERT( ( MSC_SUBCLASS_SCSI == p_interface_desc->bInterfaceSubClass &&
            MSC_PROTOCOL_BOT  == p_interface_desc->bInterfaceProtocol ), TUSB_ERROR_MSC_UNSUPPORTED_PROTOCOL );

  mscd_interface_t * p_msc = &mscd_data;

  //------------- Open Data Pipe -------------//
  tusb_descriptor_endpoint_t const *p_endpoint = (tusb_descriptor_endpoint_t const *) descriptor_next( (uint8_t const*) p_interface_desc );
  for(uint32_t i=0; i<2; i++)
  {
    ASSERT(TUSB_DESC_TYPE_ENDPOINT == p_endpoint->bDescriptorType &&
           TUSB_XFER_BULK == p_endpoint->bmAttributes.xfer, TUSB_ERROR_DESCRIPTOR_CORRUPTED);

    endpoint_handle_t * p_edpt_hdl =  ( p_endpoint->bEndpointAddress &  TUSB_DIR_DEV_TO_HOST_MASK ) ?
        &p_msc->edpt_in : &p_msc->edpt_out;

    (*p_edpt_hdl) = dcd_pipe_open(coreid, p_endpoint, p_interface_desc->bInterfaceClass);
    ASSERT( endpointhandle_is_valid(*p_edpt_hdl), TUSB_ERROR_DCD_FAILED);

    p_endpoint = (tusb_descriptor_endpoint_t const *) descriptor_next( (uint8_t const*)  p_endpoint );
  }

  p_msc->interface_number = p_interface_desc->bInterfaceNumber;

  (*p_length) += sizeof(tusb_descriptor_interface_t) + 2*sizeof(tusb_descriptor_endpoint_t);

  //------------- Queue Endpoint OUT for Command Block Wrapper -------------//
  ASSERT_STATUS( dcd_pipe_xfer(p_msc->edpt_out, &p_msc->cbw, sizeof(msc_cmd_block_wrapper_t), true) );

  return TUSB_ERROR_NONE;
}

tusb_error_t mscd_control_request(uint8_t coreid, tusb_control_request_t const * p_request)
{
  ASSERT(p_request->bmRequestType_bit.type == TUSB_REQUEST_TYPE_CLASS, TUSB_ERROR_DCD_CONTROL_REQUEST_NOT_SUPPORT);

  mscd_interface_t * p_msc = &mscd_data;

  switch(p_request->bRequest)
  {
    case MSC_REQUEST_RESET:
      dcd_pipe_control_xfer(coreid, TUSB_DIR_HOST_TO_DEV, NULL, 0);
    break;

    case MSC_REQUEST_GET_MAX_LUN:
      dcd_pipe_control_xfer(coreid, TUSB_DIR_DEV_TO_HOST, &p_msc->max_lun, 1);
    break;

    default:
      return TUSB_ERROR_DCD_CONTROL_REQUEST_NOT_SUPPORT;
  }

  return TUSB_ERROR_NONE;
}

void mscd_isr(endpoint_handle_t edpt_hdl, tusb_event_t event, uint32_t xferred_bytes)
{
  // TODO failed --> STALL pipe, on clear STALL --> queue endpoint OUT
  mscd_interface_t * p_msc = &mscd_data;

  if ( endpointhandle_is_equal(p_msc->edpt_in, edpt_hdl) )
  {
    return; // currently no need to handle bulk in
  }

  ASSERT( endpointhandle_is_equal(p_msc->edpt_out, edpt_hdl) &&
          xferred_bytes == sizeof(msc_cmd_block_wrapper_t) &&
          event == TUSB_EVENT_XFER_COMPLETE &&
          p_msc->cbw.signature == MSC_CBW_SIGNATURE, VOID_RETURN );

  void *p_buffer = NULL;
  uint16_t actual_length = p_msc->cbw.xfer_bytes;

  p_msc->csw.signature    = MSC_CSW_SIGNATURE;
  p_msc->csw.tag          = p_msc->cbw.tag;
  p_msc->csw.status       = tusbd_msc_scsi_received_isr(edpt_hdl.coreid, p_msc->cbw.lun, p_msc->cbw.command, &p_buffer, &actual_length);
  p_msc->csw.data_residue = 0; // TODO expected length, response length

  ASSERT( p_msc->cbw.xfer_bytes >= actual_length, VOID_RETURN );

  //------------- Data Phase -------------//
  if ( BIT_TEST_(p_msc->cbw.dir, 7) && p_buffer == NULL )
  { // application does not provide data to response --> possibly unsupported SCSI command
    ASSERT( TUSB_ERROR_NONE == dcd_pipe_stall(p_msc->edpt_in), VOID_RETURN );
    p_msc->csw.status = MSC_CSW_STATUS_FAILED;
  }else
  {
    ASSERT( dcd_pipe_queue_xfer( BIT_TEST_(p_msc->cbw.dir, 7) ? p_msc->edpt_in : p_msc->edpt_out,
                                 p_buffer, actual_length) == TUSB_ERROR_NONE, VOID_RETURN);
  }

  //------------- Status Phase -------------//
  ASSERT( dcd_pipe_xfer( p_msc->edpt_in , &p_msc->csw, sizeof(msc_cmd_status_wrapper_t), true) == TUSB_ERROR_NONE, VOID_RETURN );

  //------------- Queue the next CBW -------------//
  ASSERT( dcd_pipe_xfer( p_msc->edpt_out, &p_msc->cbw, sizeof(msc_cmd_block_wrapper_t), true) == TUSB_ERROR_NONE, VOID_RETURN );

}

#endif
