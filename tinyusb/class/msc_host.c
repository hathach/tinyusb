/**************************************************************************/
/*!
    @file     msc_host.c
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

#if MODE_HOST_SUPPORTED & TUSB_CFG_HOST_MSC

#define _TINY_USB_SOURCE_FILE_

//--------------------------------------------------------------------+
// INCLUDE
//--------------------------------------------------------------------+
#include "common/common.h"
#include "msc_host.h"

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF
//--------------------------------------------------------------------+
typedef struct {
  pipe_handle_t bulk_in, bulk_out;
  uint8_t interface_number;
  uint8_t max_lun;

  msc_cmd_block_wrapper_t cbw;
  msc_cmd_status_wrapper_t csw;
}msch_interface_t;

STATIC_VAR msch_interface_t msch_data[TUSB_CFG_HOST_DEVICE_MAX] TUSB_CFG_ATTR_USBRAM; // TODO to be static

// TODO rename this
STATIC_VAR uint8_t msch_buffer[10] TUSB_CFG_ATTR_USBRAM;

//--------------------------------------------------------------------+
// INTERNAL OBJECT & FUNCTION DECLARATION
//--------------------------------------------------------------------+

//--------------------------------------------------------------------+
// IMPLEMENTATION
//--------------------------------------------------------------------+

//--------------------------------------------------------------------+
// CLASS-USBH API (don't require to verify parameters)
//--------------------------------------------------------------------+
void msch_init(void)
{
  memclr_(msch_data, sizeof(msch_interface_t)*TUSB_CFG_HOST_DEVICE_MAX);
}

tusb_error_t msch_open_subtask(uint8_t dev_addr, tusb_descriptor_interface_t const *p_interface_desc, uint16_t *p_length)
{
  tusb_error_t error;

  OSAL_SUBTASK_BEGIN

  if (! ( MSC_SUBCLASS_SCSI == p_interface_desc->bInterfaceSubClass &&
          MSC_PROTOCOL_BOT  == p_interface_desc->bInterfaceProtocol ) )
  {
    return TUSB_ERROR_MSCH_UNSUPPORTED_PROTOCOL;
  }

  //------------- Open Data Pipe -------------//
  tusb_descriptor_endpoint_t const *p_endpoint = (tusb_descriptor_endpoint_t const *) descriptor_next( p_interface_desc );
  for(uint32_t i=0; i<2; i++)
  {
    ASSERT_INT(TUSB_DESC_TYPE_ENDPOINT, p_endpoint->bDescriptorType, TUSB_ERROR_USBH_DESCRIPTOR_CORRUPTED);

    pipe_handle_t * p_pipe_hdl =  ( p_endpoint->bEndpointAddress &  TUSB_DIR_DEV_TO_HOST_MASK ) ?
        &msch_data[dev_addr-1].bulk_in : &msch_data[dev_addr-1].bulk_out;

    (*p_pipe_hdl) = hcd_pipe_open(dev_addr, p_endpoint, TUSB_CLASS_MSC);
    ASSERT ( pipehandle_is_valid(*p_pipe_hdl), TUSB_ERROR_HCD_OPEN_PIPE_FAILED );

    p_endpoint = (tusb_descriptor_endpoint_t const *) descriptor_next( p_endpoint );
  }

  msch_data[dev_addr-1].interface_number = p_interface_desc->bInterfaceNumber;
  (*p_length) += sizeof(tusb_descriptor_interface_t) + 2*sizeof(tusb_descriptor_endpoint_t);

  //------------- Get Max Lun -------------//
  OSAL_SUBTASK_INVOKED_AND_WAIT(
    usbh_control_xfer_subtask( dev_addr, bm_request_type(TUSB_DIR_DEV_TO_HOST, TUSB_REQUEST_TYPE_CLASS, TUSB_REQUEST_RECIPIENT_INTERFACE),
                               MSC_REQUEST_GET_MAX_LUN, 0, msch_data[dev_addr-1].interface_number,
                               1, msch_buffer ),
    error
  );

  SUBTASK_ASSERT( TUSB_ERROR_NONE != error /* && TODO STALL means zero */);

  msch_data[dev_addr-1].max_lun = msch_buffer[0];

  //------------- SCSI Inquiry -------------//


  tusbh_msc_mounted_cb(dev_addr);

  OSAL_SUBTASK_END

  return TUSB_ERROR_NONE;
}

void msch_isr(pipe_handle_t pipe_hdl, tusb_event_t event)
{

}

void msch_close(uint8_t dev_addr)
{
  (void) hcd_pipe_close(msch_data[dev_addr-1].bulk_in);
  (void) hcd_pipe_close(msch_data[dev_addr-1].bulk_out);

  memclr_(&msch_data[dev_addr-1], sizeof(msch_interface_t));
}


#endif
