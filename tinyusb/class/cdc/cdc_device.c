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
#include "common/tusb_common.h"
#include "cdc_device.h"
#include "device/usbd_pvt.h"

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF
//--------------------------------------------------------------------+
TUSB_CFG_ATTR_USBRAM STATIC_VAR cdc_line_coding_t cdcd_line_coding[CONTROLLER_DEVICE_NUMBER];

typedef struct {
  uint8_t interface_number;
  cdc_acm_capability_t acm_capability;
  bool connected;

  uint8_t ep_notif;
  uint8_t ep_in;
  uint8_t ep_out;
}cdcd_data_t;

// TODO multiple rhport
TUSB_CFG_ATTR_USBRAM ATTR_ALIGNED(4) uint8_t _tmp_rx_buf[64];
TUSB_CFG_ATTR_USBRAM ATTR_ALIGNED(4) uint8_t _tmp_tx_buf[64];

#define CFG_TUD_CDC_BUFSIZE   128

FIFO_DEF(_rx_ff, CFG_TUD_CDC_BUFSIZE, uint8_t, true);
FIFO_DEF(_tx_ff, CFG_TUD_CDC_BUFSIZE, uint8_t, true);

//--------------------------------------------------------------------+
// INTERNAL OBJECT & FUNCTION DECLARATION
//--------------------------------------------------------------------+
STATIC_VAR cdcd_data_t cdcd_data[CONTROLLER_DEVICE_NUMBER];

//--------------------------------------------------------------------+
// APPLICATION API
//--------------------------------------------------------------------+
bool tud_n_cdc_connected(uint8_t rhport)
{
  return cdcd_data[rhport].connected;
}

uint32_t tud_n_cdc_available(uint8_t rhport)
{
  return fifo_count(&_rx_ff);
}

int tud_n_cdc_read_char(uint8_t rhport)
{
  uint8_t ch;
  return fifo_read(&_rx_ff, &ch) ? ch : (-1);
}

uint32_t tud_n_cdc_read(uint8_t rhport, void* buffer, uint32_t bufsize)
{
  return fifo_read_n(&_rx_ff, buffer, bufsize);
}

uint32_t tud_n_cdc_write_char(uint8_t rhport, char ch)
{
  return fifo_write(&_tx_ff, &ch);
}

uint32_t tud_n_cdc_write(uint8_t rhport, void const* buffer, uint32_t bufsize)
{
  return fifo_write_n(&_tx_ff, buffer, bufsize);
}


//--------------------------------------------------------------------+
// USBD Driver API
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

tusb_error_t cdcd_open(uint8_t rhport, tusb_desc_interface_t const * p_interface_desc, uint16_t *p_length)
{
  if ( CDC_COMM_SUBCLASS_ABSTRACT_CONTROL_MODEL != p_interface_desc->bInterfaceSubClass) return TUSB_ERROR_CDC_UNSUPPORTED_SUBCLASS;

  if ( !(is_in_range(CDC_COMM_PROTOCOL_ATCOMMAND, p_interface_desc->bInterfaceProtocol, CDC_COMM_PROTOCOL_ATCOMMAND_CDMA) ||
         0xff == p_interface_desc->bInterfaceProtocol) )
  {
    return TUSB_ERROR_CDC_UNSUPPORTED_PROTOCOL;
  }

  uint8_t const * p_desc = descriptor_next ( (uint8_t const *) p_interface_desc );
  cdcd_data_t * p_cdc = &cdcd_data[rhport];

  //------------- Communication Interface -------------//
  (*p_length) = sizeof(tusb_desc_interface_t);

  while( TUSB_DESC_CLASS_SPECIFIC == p_desc[DESCRIPTOR_OFFSET_TYPE] )
  { // Communication Functional Descriptors
    if ( CDC_FUNC_DESC_ABSTRACT_CONTROL_MANAGEMENT == cdc_functional_desc_typeof(p_desc) )
    { // save ACM bmCapabilities
      p_cdc->acm_capability = ((cdc_desc_func_acm_t const *) p_desc)->bmCapabilities;
    }

    (*p_length) += p_desc[DESCRIPTOR_OFFSET_LENGTH];
    p_desc = descriptor_next(p_desc);
  }

  if ( TUSB_DESC_ENDPOINT == p_desc[DESCRIPTOR_OFFSET_TYPE])
  { // notification endpoint if any
    TU_ASSERT( dcd_edpt_open(rhport, (tusb_desc_endpoint_t const *) p_desc), TUSB_ERROR_DCD_OPEN_PIPE_FAILED);

    p_cdc->ep_notif = ((tusb_desc_endpoint_t const *) p_desc)->bEndpointAddress;

    (*p_length) += p_desc[DESCRIPTOR_OFFSET_LENGTH];
    p_desc = descriptor_next(p_desc);
  }

  //------------- Data Interface (if any) -------------//
  if ( (TUSB_DESC_INTERFACE == p_desc[DESCRIPTOR_OFFSET_TYPE]) &&
       (TUSB_CLASS_CDC_DATA      == ((tusb_desc_interface_t const *) p_desc)->bInterfaceClass) )
  {
    (*p_length) += p_desc[DESCRIPTOR_OFFSET_LENGTH];
    p_desc = descriptor_next(p_desc);

    // data endpoints expected to be in pairs
    for(uint32_t i=0; i<2; i++)
    {
      tusb_desc_endpoint_t const *p_endpoint = (tusb_desc_endpoint_t const *) p_desc;
      TU_ASSERT(TUSB_DESC_ENDPOINT == p_endpoint->bDescriptorType, TUSB_ERROR_DESCRIPTOR_CORRUPTED);
      TU_ASSERT(TUSB_XFER_BULK == p_endpoint->bmAttributes.xfer, TUSB_ERROR_DESCRIPTOR_CORRUPTED);

      TU_ASSERT( dcd_edpt_open(rhport, p_endpoint), TUSB_ERROR_DCD_OPEN_PIPE_FAILED);

      if ( p_endpoint->bEndpointAddress &  TUSB_DIR_IN_MASK )
      {
        p_cdc->ep_in = p_endpoint->bEndpointAddress;
      }else
      {
        p_cdc->ep_out = p_endpoint->bEndpointAddress;
      }

      (*p_length) += p_desc[DESCRIPTOR_OFFSET_LENGTH];
      p_desc = descriptor_next( p_desc );
    }
  }

  p_cdc->interface_number   = p_interface_desc->bInterfaceNumber;

  // Prepare for incoming data
  TU_ASSERT( dcd_edpt_xfer(rhport, p_cdc->ep_out, _tmp_rx_buf, sizeof(_tmp_rx_buf)), TUSB_ERROR_DCD_EDPT_XFER);


  return TUSB_ERROR_NONE;
}

void cdcd_close(uint8_t rhport)
{
  // no need to close opened pipe, dcd bus reset will put controller's endpoints to default state
  memclr_(&cdcd_data[rhport], sizeof(cdcd_data_t));

  fifo_clear(&_rx_ff);
  fifo_clear(&_tx_ff);
}

tusb_error_t cdcd_control_request_st(uint8_t rhport, tusb_control_request_t const * p_request)
{
  OSAL_SUBTASK_BEGIN

  tusb_error_t err;

  //------------- Class Specific Request -------------//
  if (p_request->bmRequestType_bit.type != TUSB_REQ_TYPE_CLASS) return TUSB_ERROR_DCD_CONTROL_REQUEST_NOT_SUPPORT;

  if (CDC_REQUEST_GET_LINE_CODING == p_request->bRequest)
  {
    STASK_INVOKE( usbd_control_xfer_st(rhport, (tusb_dir_t) p_request->bmRequestType_bit.direction,
                                            (uint8_t*) &cdcd_line_coding[rhport], min16_of(sizeof(cdc_line_coding_t), p_request->wLength)), err );
  }
  else if (CDC_REQUEST_SET_LINE_CODING == p_request->bRequest)
  {
    STASK_INVOKE( usbd_control_xfer_st(rhport, (tusb_dir_t) p_request->bmRequestType_bit.direction,
                                            (uint8_t*) &cdcd_line_coding[rhport], min16_of(sizeof(cdc_line_coding_t), p_request->wLength)), err );
    // TODO notify application on xfer complete
  }
  else if (CDC_REQUEST_SET_CONTROL_LINE_STATE == p_request->bRequest )
  {
    enum {
      ACTIVE_DTE_PRESENT     = 0x0003,
      ACTIVE_DTE_NOT_PRESENT = 0x0002
    };

    cdcd_data_t * p_cdc = &cdcd_data[rhport];

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

    dcd_control_status(rhport, p_request->bmRequestType_bit.direction);
  }
  else
  {
    dcd_control_stall(rhport); // stall unsupported request
  }

  OSAL_SUBTASK_END
}

tusb_error_t cdcd_xfer_cb(uint8_t rhport, uint8_t ep_addr, tusb_event_t event, uint32_t xferred_bytes)
{
  cdcd_data_t const * p_cdc = &cdcd_data[rhport];

  if ( ep_addr == p_cdc->ep_out )
  {
    fifo_write_n(&_rx_ff, _tmp_rx_buf, xferred_bytes);

    // preparing for next
    TU_ASSERT(dcd_edpt_xfer(rhport, p_cdc->ep_out, _tmp_rx_buf, sizeof(_tmp_rx_buf)), TUSB_ERROR_DCD_EDPT_XFER);

    // fire callback
    tud_cdc_rx_cb(rhport);
  }

  return TUSB_ERROR_NONE;
}

bool tud_n_cdc_flush (uint8_t rhport)
{
  VERIFY( tud_n_cdc_connected(rhport) );

  uint8_t edpt = cdcd_data[rhport].ep_in;

  VERIFY( !dcd_edpt_busy(rhport, edpt) );

  uint16_t count = fifo_read_n(&_tx_ff, _tmp_tx_buf, sizeof(_tmp_tx_buf));
  TU_ASSERT( dcd_edpt_xfer(rhport, edpt, _tmp_tx_buf, count) );

  return true;
}

void cdcd_sof(uint8_t rhport)
{
  tud_n_cdc_flush(rhport);
}

#endif
