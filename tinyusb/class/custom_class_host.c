/**************************************************************************/
/*!
    @file     custom_class_host.c
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

#if (MODE_HOST_SUPPORTED && CFG_TUSB_HOST_CUSTOM_CLASS)

#define _TINY_USB_SOURCE_FILE_

//--------------------------------------------------------------------+
// INCLUDE
//--------------------------------------------------------------------+
#include "common/tusb_common.h"
#include "custom_class.h"

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF
//--------------------------------------------------------------------+

//--------------------------------------------------------------------+
// INTERNAL OBJECT & FUNCTION DECLARATION
//--------------------------------------------------------------------+
custom_interface_info_t custom_interface[CFG_TUSB_HOST_DEVICE_MAX];

static tusb_error_t cush_validate_paras(uint8_t dev_addr, uint16_t vendor_id, uint16_t product_id, void * p_buffer, uint16_t length)
{
  if ( !tusbh_custom_is_mounted(dev_addr, vendor_id, product_id) )
  {
    return TUSB_ERROR_DEVICE_NOT_READY;
  }

  TU_ASSERT( p_buffer != NULL && length != 0, TUSB_ERROR_INVALID_PARA);

  return TUSB_ERROR_NONE;
}
//--------------------------------------------------------------------+
// APPLICATION API (need to check parameters)
//--------------------------------------------------------------------+
tusb_error_t tusbh_custom_read(uint8_t dev_addr, uint16_t vendor_id, uint16_t product_id, void * p_buffer, uint16_t length)
{
  TU_ASSERT_ERR( cush_validate_paras(dev_addr, vendor_id, product_id, p_buffer, length) );

  if ( !hcd_pipe_is_idle(custom_interface[dev_addr-1].pipe_in) )
  {
    return TUSB_ERROR_INTERFACE_IS_BUSY;
  }

  (void) hcd_pipe_xfer( custom_interface[dev_addr-1].pipe_in, p_buffer, length, true);

  return TUSB_ERROR_NONE;
}

tusb_error_t tusbh_custom_write(uint8_t dev_addr, uint16_t vendor_id, uint16_t product_id, void const * p_data, uint16_t length)
{
  TU_ASSERT_ERR( cush_validate_paras(dev_addr, vendor_id, product_id, p_data, length) );

  if ( !hcd_pipe_is_idle(custom_interface[dev_addr-1].pipe_out) )
  {
    return TUSB_ERROR_INTERFACE_IS_BUSY;
  }

  (void) hcd_pipe_xfer( custom_interface[dev_addr-1].pipe_out, p_data, length, true);

  return TUSB_ERROR_NONE;
}

//--------------------------------------------------------------------+
// USBH-CLASS API
//--------------------------------------------------------------------+
void cush_init(void)
{
  memclr_(&custom_interface, sizeof(custom_interface_info_t) * CFG_TUSB_HOST_DEVICE_MAX);
}

tusb_error_t cush_open_subtask(uint8_t dev_addr, tusb_desc_interface_t const *p_interface_desc, uint16_t *p_length)
{
  // FIXME quick hack to test lpc1k custom class with 2 bulk endpoints
  uint8_t const *p_desc = (uint8_t const *) p_interface_desc;
  p_desc = descriptor_next(p_desc);

  //------------- Bulk Endpoints Descriptor -------------//
  for(uint32_t i=0; i<2; i++)
  {
    tusb_desc_endpoint_t const *p_endpoint = (tusb_desc_endpoint_t const *) p_desc;
    TU_ASSERT(TUSB_DESC_ENDPOINT == p_endpoint->bDescriptorType, TUSB_ERROR_INVALID_PARA);

    pipe_handle_t * p_pipe_hdl =  ( p_endpoint->bEndpointAddress &  TUSB_DIR_IN_MASK ) ?
                         &custom_interface[dev_addr-1].pipe_in : &custom_interface[dev_addr-1].pipe_out;
    *p_pipe_hdl = hcd_pipe_open(dev_addr, p_endpoint, TUSB_CLASS_VENDOR_SPECIFIC);
    TU_ASSERT ( pipehandle_is_valid(*p_pipe_hdl), TUSB_ERROR_HCD_OPEN_PIPE_FAILED );

    p_desc = descriptor_next(p_desc);
  }

  (*p_length) = sizeof(tusb_desc_interface_t) + 2*sizeof(tusb_desc_endpoint_t);
  return TUSB_ERROR_NONE;
}

void cush_isr(pipe_handle_t pipe_hdl, tusb_event_t event)
{

}

void cush_close(uint8_t dev_addr)
{
  tusb_error_t err1, err2;
  custom_interface_info_t * p_interface = &custom_interface[dev_addr-1];

  // TODO re-consider to check pipe valid before calling pipe_close
  if( pipehandle_is_valid( p_interface->pipe_in ) )
  {
    err1 = hcd_pipe_close( p_interface->pipe_in );
  }

  if ( pipehandle_is_valid( p_interface->pipe_out ) )
  {
    err2 = hcd_pipe_close( p_interface->pipe_out );
  }

  memclr_(p_interface, sizeof(custom_interface_info_t));

  TU_ASSERT(err1 == TUSB_ERROR_NONE && err2 == TUSB_ERROR_NONE, (void) 0 );
}

#endif
