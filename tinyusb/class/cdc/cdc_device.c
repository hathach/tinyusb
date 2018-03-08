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

  bool connected;

  endpoint_handle_t edpt_hdl[3]; // notification, data in, data out
}cdcd_data_t;

// TODO multiple port
TUSB_CFG_ATTR_USBRAM uint8_t _tmp_rx_buf[64];
TUSB_CFG_ATTR_USBRAM uint8_t _tmp_tx_buf[64];

#define CFG_TUD_CDC_BUFSIZE   128

FIFO_DEF(_rx_ff, CFG_TUD_CDC_BUFSIZE, uint8_t, true);
FIFO_DEF(_tx_ff, CFG_TUD_CDC_BUFSIZE, uint8_t, true);

//--------------------------------------------------------------------+
// INTERNAL OBJECT & FUNCTION DECLARATION
//--------------------------------------------------------------------+
STATIC_VAR cdcd_data_t cdcd_data[CONTROLLER_DEVICE_NUMBER];

static tusb_error_t cdcd_xfer(uint8_t coreid,  cdc_pipeid_t pipeid, void * p_buffer, uint32_t length, bool is_notify)
{
  ASSERT(tud_mounted(coreid), TUSB_ERROR_USBD_DEVICE_NOT_CONFIGURED);

  cdcd_data_t* p_cdc = &cdcd_data[coreid];

  ASSERT_FALSE ( dcd_pipe_is_busy(p_cdc->edpt_hdl[pipeid]), TUSB_ERROR_INTERFACE_IS_BUSY);
  ASSERT_STATUS( hal_dcd_pipe_xfer(p_cdc->edpt_hdl[pipeid], p_buffer, length, is_notify) );

  return TUSB_ERROR_NONE;
}

//--------------------------------------------------------------------+
// APPLICATION API (Parameters requires validation)
//--------------------------------------------------------------------+
bool tud_cdc_busy(uint8_t coreid, cdc_pipeid_t pipeid)
{
  return dcd_pipe_is_busy( cdcd_data[coreid].edpt_hdl[pipeid] );
}

tusb_error_t tud_cdc_receive(uint8_t coreid, void * p_buffer, uint32_t length, bool is_notify)
{
  return cdcd_xfer(coreid, CDC_PIPE_DATA_OUT, p_buffer, length, is_notify);
}

tusb_error_t tud_cdc_send(uint8_t coreid, void * p_data, uint32_t length, bool is_notify)
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
  for(uint8_t i=0; i<CONTROLLER_DEVICE_NUMBER; i++)
  {
    cdcd_line_coding[i].bit_rate  = 115200;
    cdcd_line_coding[i].stop_bits = 0;
    cdcd_line_coding[i].parity    = 0;
    cdcd_line_coding[i].data_bits = 8;
  }
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
    VERIFY( hal_dcd_pipe_open(coreid, (tusb_descriptor_endpoint_t const *) p_desc, &p_cdc->edpt_hdl[CDC_PIPE_NOTIFICATION]), TUSB_ERROR_DCD_OPEN_PIPE_FAILED);

    (*p_length) += p_desc[DESCRIPTOR_OFFSET_LENGTH];
    p_desc = descriptor_next(p_desc);
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
      ASSERT_(TUSB_DESC_TYPE_ENDPOINT == p_endpoint->bDescriptorType, TUSB_ERROR_DESCRIPTOR_CORRUPTED);
      ASSERT_(TUSB_XFER_BULK == p_endpoint->bmAttributes.xfer, TUSB_ERROR_DESCRIPTOR_CORRUPTED);

      endpoint_handle_t * p_edpt_hdl =  ( p_endpoint->bEndpointAddress &  TUSB_DIR_DEV_TO_HOST_MASK ) ?
          &p_cdc->edpt_hdl[CDC_PIPE_DATA_IN] : &p_cdc->edpt_hdl[CDC_PIPE_DATA_OUT] ;

      ASSERT_( hal_dcd_pipe_open(coreid, p_endpoint, p_edpt_hdl), TUSB_ERROR_DCD_OPEN_PIPE_FAILED);

      (*p_length) += p_desc[DESCRIPTOR_OFFSET_LENGTH];
      p_desc = descriptor_next( p_desc );
    }
  }

  p_cdc->interface_number   = p_interface_desc->bInterfaceNumber;

  // Prepare for incoming data
  hal_dcd_pipe_xfer(p_cdc->edpt_hdl[CDC_PIPE_DATA_OUT], _tmp_rx_buf, sizeof(_tmp_rx_buf), true);


  return TUSB_ERROR_NONE;
}

void cdcd_close(uint8_t coreid)
{
  // no need to close opened pipe, dcd bus reset will put controller's endpoints to default state
  memclr_(&cdcd_data[coreid], sizeof(cdcd_data_t));

  fifo_clear(&_rx_ff);
  fifo_clear(&_tx_ff);
}

tusb_error_t cdcd_control_request_subtask(uint8_t coreid, tusb_control_request_t const * p_request)
{
  //------------- Class Specific Request -------------//
  if (p_request->bmRequestType_bit.type != TUSB_REQUEST_TYPE_CLASS) return TUSB_ERROR_DCD_CONTROL_REQUEST_NOT_SUPPORT;

  switch(p_request->bRequest)
  {
    case CDC_REQUEST_GET_LINE_CODING:
      hal_dcd_control_xfer(coreid, (tusb_direction_t) p_request->bmRequestType_bit.direction,
                            (uint8_t*) &cdcd_line_coding[coreid], min16_of(sizeof(cdc_line_coding_t), p_request->wLength), false );
    break;

    case CDC_REQUEST_SET_LINE_CODING:
      hal_dcd_control_xfer(coreid, (tusb_direction_t) p_request->bmRequestType_bit.direction,
                            (uint8_t*) &cdcd_line_coding[coreid], min16_of(sizeof(cdc_line_coding_t), p_request->wLength), false );
      // TODO notify application on xfer completea
    break;

    case CDC_REQUEST_SET_CONTROL_LINE_STATE: // TODO extract DTE present
    {
      enum {
        ACTIVE_DTE_PRESENT     = 0x0003,
        ACTIVE_DTE_NOT_PRESENT = 0x0002
      };

      cdcd_data_t * p_cdc = &cdcd_data[coreid];

      if (p_request->wValue == ACTIVE_DTE_PRESENT)
      {
        // terminal connected
        p_cdc->connected = true;
      }
      else if (p_request->wValue == ACTIVE_DTE_NOT_PRESENT)
      {
        // terminal disconnected
        p_cdc->connected = false;
      }else
      {
        // De-active --> disconnected
        p_cdc->connected = false;
      }
    }
    break;

    default: return TUSB_ERROR_DCD_CONTROL_REQUEST_NOT_SUPPORT;
  }

  return TUSB_ERROR_NONE;
}

tusb_error_t cdcd_xfer_cb(endpoint_handle_t edpt_hdl, tusb_event_t event, uint32_t xferred_bytes)
{
  cdcd_data_t const * p_cdc = &cdcd_data[edpt_hdl.coreid];

  if ( edpt_equal(edpt_hdl, p_cdc->edpt_hdl[CDC_PIPE_DATA_OUT]) )
  {
    fifo_write_n(&_rx_ff, _tmp_rx_buf, xferred_bytes);

    // preparing for next
    hal_dcd_pipe_xfer(p_cdc->edpt_hdl[CDC_PIPE_DATA_OUT], _tmp_rx_buf, sizeof(_tmp_rx_buf), true);

    // fire callback
    tud_cdc_rx_cb(edpt_hdl.coreid);
  }

  return TUSB_ERROR_NONE;
}

void cdcd_sof(uint8_t coreid)
{
  endpoint_handle_t ep = cdcd_data[coreid].edpt_hdl[CDC_PIPE_DATA_IN];

  if ( !dcd_pipe_is_busy( ep ) )
  {
    uint16_t count = fifo_read_n(&_tx_ff, _tmp_tx_buf, sizeof(_tmp_tx_buf));

    hal_dcd_pipe_xfer(ep, _tmp_tx_buf, count, false);
  }
}

uint32_t tud_cdc_available(uint8_t coreid)
{
  return fifo_count(&_rx_ff);
}

bool tud_cdc_connected(uint8_t coreid)
{
  return cdcd_data[coreid].connected;
}

int tud_cdc_read_char(uint8_t coreid)
{
  uint8_t ch;
  return fifo_read(&_rx_ff, &ch) ? ch : (-1);
}

uint32_t tud_cdc_read(uint8_t coreid, void* buffer, uint32_t bufsize)
{
  return fifo_read_n(&_rx_ff, buffer, bufsize);
}

uint32_t tud_cdc_write_char(uint8_t coreid, char ch)
{
  return fifo_write(&_tx_ff, &ch);
}

uint32_t tud_cdc_write(uint8_t coreid, void const* buffer, uint32_t bufsize)
{
  return fifo_write_n(&_tx_ff, buffer, bufsize);
}


#endif
