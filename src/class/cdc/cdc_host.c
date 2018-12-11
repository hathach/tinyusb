/**************************************************************************/
/*!
    @file     cdc_host.c
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

#if (TUSB_OPT_HOST_ENABLED && CFG_TUH_CDC)

#define _TINY_USB_SOURCE_FILE_

#include "common/tusb_common.h"
#include "cdc_host.h"

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF
//--------------------------------------------------------------------+
typedef struct {
  uint8_t itf_num;
  uint8_t itf_protocol;

  uint8_t ep_notif;
  uint8_t ep_in;
  uint8_t ep_out;

  cdc_acm_capability_t acm_capability;

} cdch_data_t;

//--------------------------------------------------------------------+
// INTERNAL OBJECT & FUNCTION DECLARATION
//--------------------------------------------------------------------+
static cdch_data_t cdch_data[CFG_TUSB_HOST_DEVICE_MAX];

bool tuh_cdc_mounted(uint8_t dev_addr)
{
  cdch_data_t* cdc = &cdch_data[dev_addr-1];
  return cdc->ep_in && cdc->ep_out;
}

bool tuh_cdc_is_busy(uint8_t dev_addr, cdc_pipeid_t pipeid)
{
  if ( !tuh_cdc_mounted(dev_addr) ) return false;

  cdch_data_t const * p_cdc = &cdch_data[dev_addr-1];

  switch (pipeid)
  {
    case CDC_PIPE_NOTIFICATION:
      return hcd_edpt_busy(dev_addr, p_cdc->ep_notif );

    case CDC_PIPE_DATA_IN:
      return hcd_edpt_busy(dev_addr, p_cdc->ep_in );

    case CDC_PIPE_DATA_OUT:
      return hcd_edpt_busy(dev_addr, p_cdc->ep_out );

    default:
      return false;
  }
}

//--------------------------------------------------------------------+
// APPLICATION API (parameter validation needed)
//--------------------------------------------------------------------+
bool tuh_cdc_serial_is_mounted(uint8_t dev_addr)
{
  // TODO consider all AT Command as serial candidate
  return tuh_cdc_mounted(dev_addr)                                         &&
      (CDC_COMM_PROTOCOL_ATCOMMAND <= cdch_data[dev_addr-1].itf_protocol) &&
      (cdch_data[dev_addr-1].itf_protocol <= CDC_COMM_PROTOCOL_ATCOMMAND_CDMA);
}

bool tuh_cdc_send(uint8_t dev_addr, void const * p_data, uint32_t length, bool is_notify)
{
  TU_VERIFY( tuh_cdc_mounted(dev_addr) );
  TU_VERIFY( p_data != NULL && length, TUSB_ERROR_INVALID_PARA);

  uint8_t const ep_out = cdch_data[dev_addr-1].ep_out;
  if ( hcd_edpt_busy(dev_addr, ep_out) ) return false;

  return hcd_pipe_xfer(dev_addr, ep_out, (void *) p_data, length, is_notify);
}

bool tuh_cdc_receive(uint8_t dev_addr, void * p_buffer, uint32_t length, bool is_notify)
{
  TU_VERIFY( tuh_cdc_mounted(dev_addr) );
  TU_VERIFY( p_buffer != NULL && length, TUSB_ERROR_INVALID_PARA);

  uint8_t const ep_in = cdch_data[dev_addr-1].ep_in;
  if ( hcd_edpt_busy(dev_addr, ep_in) ) return false;

  return hcd_pipe_xfer(dev_addr, ep_in, p_buffer, length, is_notify);
}

//--------------------------------------------------------------------+
// USBH-CLASS DRIVER API
//--------------------------------------------------------------------+
void cdch_init(void)
{
  tu_memclr(cdch_data, sizeof(cdch_data_t)*CFG_TUSB_HOST_DEVICE_MAX);
}

bool cdch_open(uint8_t rhport, uint8_t dev_addr, tusb_desc_interface_t const *itf_desc, uint16_t *p_length)
{
  // Only support ACM
  TU_VERIFY( CDC_COMM_SUBCLASS_ABSTRACT_CONTROL_MODEL == itf_desc->bInterfaceSubClass);

  // Only support AT commands, no protocol and vendor specific commands.
  TU_VERIFY(tu_within(CDC_COMM_PROTOCOL_NONE, itf_desc->bInterfaceProtocol, CDC_COMM_PROTOCOL_ATCOMMAND_CDMA) ||
            0xff == itf_desc->bInterfaceProtocol);

  uint8_t const * p_desc;
  cdch_data_t * p_cdc;

  p_desc = descriptor_next ( (uint8_t const *) itf_desc );
  p_cdc  = &cdch_data[dev_addr-1];

  p_cdc->itf_num   = itf_desc->bInterfaceNumber;
  p_cdc->itf_protocol = itf_desc->bInterfaceProtocol; // TODO 0xff is consider as rndis candidate, other is virtual Com

  //------------- Communication Interface -------------//
  (*p_length) = sizeof(tusb_desc_interface_t);

  // Communication Functional Descriptors
  while( TUSB_DESC_CLASS_SPECIFIC == p_desc[DESC_OFFSET_TYPE] )
  {
    if ( CDC_FUNC_DESC_ABSTRACT_CONTROL_MANAGEMENT == cdc_functional_desc_typeof(p_desc) )
    {
      // save ACM bmCapabilities
      p_cdc->acm_capability = ((cdc_desc_func_acm_t const *) p_desc)->bmCapabilities;
    }

    (*p_length) += p_desc[DESC_OFFSET_LEN];
    p_desc = descriptor_next(p_desc);
  }

  if ( TUSB_DESC_ENDPOINT == p_desc[DESC_OFFSET_TYPE])
  {
    // notification endpoint
    tusb_desc_endpoint_t const * ep_desc = (tusb_desc_endpoint_t const *) p_desc;

    TU_ASSERT( hcd_edpt_open(rhport, dev_addr, ep_desc) );
    p_cdc->ep_notif = ep_desc->bEndpointAddress;

    (*p_length) += p_desc[DESC_OFFSET_LEN];
    p_desc = descriptor_next(p_desc);
  }

  //------------- Data Interface (if any) -------------//
  if ( (TUSB_DESC_INTERFACE == p_desc[DESC_OFFSET_TYPE]) &&
       (TUSB_CLASS_CDC_DATA == ((tusb_desc_interface_t const *) p_desc)->bInterfaceClass) )
  {
    (*p_length) += p_desc[DESC_OFFSET_LEN];
    p_desc = descriptor_next(p_desc);

    // data endpoints expected to be in pairs
    for(uint32_t i=0; i<2; i++)
    {
      tusb_desc_endpoint_t const *ep_desc = (tusb_desc_endpoint_t const *) p_desc;
      TU_ASSERT(TUSB_DESC_ENDPOINT == ep_desc->bDescriptorType);
      TU_ASSERT(TUSB_XFER_BULK == ep_desc->bmAttributes.xfer);

      TU_ASSERT(hcd_edpt_open(rhport, dev_addr, ep_desc));

      if ( edpt_dir(ep_desc->bEndpointAddress) ==  TUSB_DIR_IN )
      {
        p_cdc->ep_in = ep_desc->bEndpointAddress;
      }else
      {
        p_cdc->ep_out = ep_desc->bEndpointAddress;
      }

      (*p_length) += p_desc[DESC_OFFSET_LEN];
      p_desc = descriptor_next( p_desc );
    }
  }

  // FIXME move to seperate API : connect
  tusb_control_request_t request =
  {
    .bmRequestType_bit = { .recipient = TUSB_REQ_RCPT_INTERFACE, .type = TUSB_REQ_TYPE_CLASS, .direction = TUSB_DIR_OUT },
    .bRequest = CDC_REQUEST_SET_CONTROL_LINE_STATE,
    .wValue = 0x03, // dtr on, cst on
    .wIndex = p_cdc->itf_num,
    .wLength = 0
  };

  TU_ASSERT( usbh_control_xfer(dev_addr, &request, NULL) );

  return true;
}

void cdch_isr(uint8_t dev_addr, uint8_t ep_addr, xfer_result_t event, uint32_t xferred_bytes)
{
  (void) ep_addr;
  tuh_cdc_xfer_isr( dev_addr, event, 0, xferred_bytes );
}

void cdch_close(uint8_t dev_addr)
{
  cdch_data_t * p_cdc = &cdch_data[dev_addr-1];
  tu_memclr(p_cdc, sizeof(cdch_data_t));
}

#endif
