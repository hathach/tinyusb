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

#if (TUSB_OPT_DEVICE_ENABLED && CFG_TUD_CDC)

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
typedef struct
{
  uint8_t itf_num;
  uint8_t ep_notif;
  uint8_t ep_in;
  uint8_t ep_out;

  // Bit 0:  DTR (Data Terminal Ready), Bit 1: RTS (Request to Send)
  uint8_t line_state;

  /*------------- From this point, data is not cleared by bus reset -------------*/
  char    wanted_char;
  CFG_TUSB_MEM_ALIGN cdc_line_coding_t line_coding;

  // FIFO
  tu_fifo_t rx_ff;
  tu_fifo_t tx_ff;

  uint8_t rx_ff_buf[CFG_TUD_CDC_RX_BUFSIZE];
  uint8_t tx_ff_buf[CFG_TUD_CDC_TX_BUFSIZE];

  // Endpoint Transfer buffer
  CFG_TUSB_MEM_ALIGN uint8_t epin_buf[CFG_TUD_CDC_EPSIZE];
  CFG_TUSB_MEM_ALIGN uint8_t epout_buf[CFG_TUD_CDC_EPSIZE];

}cdcd_interface_t;

#define ITF_BUS_RESET_SZ   offsetof(cdcd_interface_t, wanted_char)

//--------------------------------------------------------------------+
// INTERNAL OBJECT & FUNCTION DECLARATION
//--------------------------------------------------------------------+
CFG_TUSB_ATTR_USBRAM static cdcd_interface_t _cdcd_itf[CFG_TUD_CDC];

//--------------------------------------------------------------------+
// APPLICATION API
//--------------------------------------------------------------------+
bool tud_cdc_n_connected(uint8_t itf)
{
  // DTR (bit 0) active  isconsidered as connected
  return BIT_TEST_(_cdcd_itf[itf].line_state, 0);
}

uint8_t tud_cdc_n_get_line_state (uint8_t itf)
{
  return _cdcd_itf[itf].line_state;
}

void tud_cdc_n_get_line_coding (uint8_t itf, cdc_line_coding_t* coding)
{
  (*coding) = _cdcd_itf[itf].line_coding;
}

void tud_cdc_n_set_wanted_char (uint8_t itf, char wanted)
{
  _cdcd_itf[itf].wanted_char = wanted;
}


//--------------------------------------------------------------------+
// READ API
//--------------------------------------------------------------------+
uint32_t tud_cdc_n_available(uint8_t itf)
{
  return tu_fifo_count(&_cdcd_itf[itf].rx_ff);
}

char tud_cdc_n_read_char(uint8_t itf)
{
  char ch;
  return tu_fifo_read(&_cdcd_itf[itf].rx_ff, &ch) ? ch : (-1);
}

uint32_t tud_cdc_n_read(uint8_t itf, void* buffer, uint32_t bufsize)
{
  return tu_fifo_read_n(&_cdcd_itf[itf].rx_ff, buffer, bufsize);
}

char tud_cdc_n_peek(uint8_t itf, int pos)
{
  char ch;
  return tu_fifo_peek_at(&_cdcd_itf[itf].rx_ff, pos, &ch) ? ch : (-1);
}

void tud_cdc_n_read_flush (uint8_t itf)
{
  tu_fifo_clear(&_cdcd_itf[itf].rx_ff);
}

//--------------------------------------------------------------------+
// WRITE API
//--------------------------------------------------------------------+

uint32_t tud_cdc_n_write_char(uint8_t itf, char ch)
{
  return tu_fifo_write(&_cdcd_itf[itf].tx_ff, &ch) ? 1 : 0;
}

uint32_t tud_cdc_n_write(uint8_t itf, void const* buffer, uint32_t bufsize)
{
  return tu_fifo_write_n(&_cdcd_itf[itf].tx_ff, buffer, bufsize);
}

bool tud_cdc_n_write_flush (uint8_t itf)
{
  cdcd_interface_t* p_cdc = &_cdcd_itf[itf];
  TU_VERIFY( !dcd_edpt_busy(TUD_OPT_RHPORT, p_cdc->ep_in) ); // skip if previous transfer not complete

  uint16_t count = tu_fifo_read_n(&_cdcd_itf[itf].tx_ff, p_cdc->epout_buf, CFG_TUD_CDC_EPSIZE);

  TU_VERIFY( tud_cdc_n_connected(itf) ); // fifo is empty if not connected

  if ( count ) TU_ASSERT( dcd_edpt_xfer(TUD_OPT_RHPORT, p_cdc->ep_in, p_cdc->epout_buf, count) );

  return true;
}


//--------------------------------------------------------------------+
// USBD Driver API
//--------------------------------------------------------------------+
void cdcd_init(void)
{
  arrclr_( _cdcd_itf );

  for(uint8_t i=0; i<CFG_TUD_CDC; i++)
  {
    _cdcd_itf[i].wanted_char = -1;

    // default line coding is : stop bit = 1, parity = none, data bits = 8
    _cdcd_itf[i].line_coding.bit_rate = 115200;
    _cdcd_itf[i].line_coding.stop_bits = 0;
    _cdcd_itf[i].line_coding.parity    = 0;
    _cdcd_itf[i].line_coding.data_bits = 8;

    // config fifo
    tu_fifo_config(&_cdcd_itf[i].rx_ff, _cdcd_itf[i].rx_ff_buf, CFG_TUD_CDC_RX_BUFSIZE, 1, true);
    tu_fifo_config(&_cdcd_itf[i].tx_ff, _cdcd_itf[i].tx_ff_buf, CFG_TUD_CDC_TX_BUFSIZE, 1, false);
  }
}

void cdcd_reset(uint8_t rhport)
{
  (void) rhport;

  for(uint8_t i=0; i<CFG_TUD_CDC; i++)
  {
    memclr_(&_cdcd_itf[i], ITF_BUS_RESET_SZ);
    tu_fifo_clear(&_cdcd_itf[i].rx_ff);
    tu_fifo_clear(&_cdcd_itf[i].tx_ff);
  }
}

tusb_error_t cdcd_open(uint8_t rhport, tusb_desc_interface_t const * p_interface_desc, uint16_t *p_length)
{
  if ( CDC_COMM_SUBCLASS_ABSTRACT_CONTROL_MODEL != p_interface_desc->bInterfaceSubClass) return TUSB_ERROR_CDC_UNSUPPORTED_SUBCLASS;

  if ( !(tu_within(CDC_COMM_PROTOCOL_ATCOMMAND, p_interface_desc->bInterfaceProtocol, CDC_COMM_PROTOCOL_ATCOMMAND_CDMA) ||
         0xff == p_interface_desc->bInterfaceProtocol) )
  {
    return TUSB_ERROR_CDC_UNSUPPORTED_PROTOCOL;
  }

  // Find available interface
  cdcd_interface_t * p_cdc = NULL;
  for(uint8_t i=0; i<CFG_TUD_CDC; i++)
  {
    if ( _cdcd_itf[i].ep_in == 0 )
    {
      p_cdc = &_cdcd_itf[i];
      break;
    }
  }

  //------------- Control Interface -------------//
  p_cdc->itf_num  = p_interface_desc->bInterfaceNumber;

  uint8_t const * p_desc = descriptor_next ( (uint8_t const *) p_interface_desc );
  (*p_length) = sizeof(tusb_desc_interface_t);

  // Communication Functional Descriptors
  while( TUSB_DESC_CLASS_SPECIFIC == p_desc[DESC_OFFSET_TYPE] )
  {
    (*p_length) += p_desc[DESC_OFFSET_LEN];
    p_desc = descriptor_next(p_desc);
  }

  if ( TUSB_DESC_ENDPOINT == p_desc[DESC_OFFSET_TYPE])
  { // notification endpoint if any
    TU_ASSERT( dcd_edpt_open(rhport, (tusb_desc_endpoint_t const *) p_desc), TUSB_ERROR_DCD_OPEN_PIPE_FAILED);

    p_cdc->ep_notif = ((tusb_desc_endpoint_t const *) p_desc)->bEndpointAddress;

    (*p_length) += p_desc[DESC_OFFSET_LEN];
    p_desc = descriptor_next(p_desc);
  }

  //------------- Data Interface (if any) -------------//
  if ( (TUSB_DESC_INTERFACE == p_desc[DESC_OFFSET_TYPE]) &&
       (TUSB_CLASS_CDC_DATA == ((tusb_desc_interface_t const *) p_desc)->bInterfaceClass) )
  {
    // next to endpoint descritpor
    (*p_length) += p_desc[DESC_OFFSET_LEN];
    p_desc = descriptor_next(p_desc);

    // Open endpoint pair with usbd helper
    tusb_desc_endpoint_t const *p_desc_ep = (tusb_desc_endpoint_t const *) p_desc;
    TU_ASSERT_ERR( usbd_open_edpt_pair(rhport, p_desc_ep, TUSB_XFER_BULK, &p_cdc->ep_out, &p_cdc->ep_in) );

    (*p_length) += 2*sizeof(tusb_desc_endpoint_t);
  }

  // Prepare for incoming data
  TU_ASSERT( dcd_edpt_xfer(rhport, p_cdc->ep_out, p_cdc->epin_buf, CFG_TUD_CDC_EPSIZE), TUSB_ERROR_DCD_EDPT_XFER);

  return TUSB_ERROR_NONE;
}

tusb_error_t cdcd_control_request_st(uint8_t rhport, tusb_control_request_t const * p_request)
{
  OSAL_SUBTASK_BEGIN

  //------------- Class Specific Request -------------//
  if (p_request->bmRequestType_bit.type != TUSB_REQ_TYPE_CLASS) return TUSB_ERROR_DCD_CONTROL_REQUEST_NOT_SUPPORT;

  // TODO Support multiple interfaces
  uint8_t const itf = 0;
  cdcd_interface_t* p_cdc = &_cdcd_itf[itf];

  if ( (CDC_REQUEST_GET_LINE_CODING == p_request->bRequest) || (CDC_REQUEST_SET_LINE_CODING == p_request->bRequest) )
  {
    uint16_t len = tu_min16(sizeof(cdc_line_coding_t), p_request->wLength);
    usbd_control_xfer_st(rhport, p_request->bmRequestType_bit.direction, &p_cdc->line_coding, len);

    // Invoke callback
    if (CDC_REQUEST_SET_LINE_CODING == p_request->bRequest)
    {
      if ( tud_cdc_line_coding_cb ) tud_cdc_line_coding_cb(itf, &p_cdc->line_coding);
    }
  }
  else if (CDC_REQUEST_SET_CONTROL_LINE_STATE == p_request->bRequest )
  {
    // CDC PSTN v1.2 section 6.3.12
    // Bit 0: Indicates if DTE is present or not.
    //        This signal corresponds to V.24 signal 108/2 and RS-232 signal DTR (Data Terminal Ready)
    // Bit 1: Carrier control for half-duplex modems.
    //        This signal corresponds to V.24 signal 105 and RS-232 signal RTS (Request to Send)

    p_cdc->line_state = (uint8_t) p_request->wValue;

    dcd_control_status(rhport, p_request->bmRequestType_bit.direction); // ACK control request

    // Invoke callback
    if ( tud_cdc_line_state_cb) tud_cdc_line_state_cb(itf, BIT_TEST_(p_request->wValue, 0), BIT_TEST_(p_request->wValue, 1));
  }
  else
  {
    dcd_control_stall(rhport); // stall unsupported request
  }

  OSAL_SUBTASK_END
}

tusb_error_t cdcd_xfer_cb(uint8_t rhport, uint8_t ep_addr, tusb_event_t event, uint32_t xferred_bytes)
{
  // TODO Support multiple interfaces
  uint8_t const itf = 0;
  cdcd_interface_t* p_cdc = &_cdcd_itf[itf];

  // receive new data
  if ( ep_addr == p_cdc->ep_out )
  {
    char const wanted = p_cdc->wanted_char;

    for(uint32_t i=0; i<xferred_bytes; i++)
    {
      tu_fifo_write(&p_cdc->rx_ff, &p_cdc->epin_buf[i]);

      // Check for wanted char and invoke callback if needed
      if ( tud_cdc_rx_wanted_cb && ( wanted != -1 ) && ( wanted == p_cdc->epin_buf[i] ) )
      {
        tud_cdc_rx_wanted_cb(itf, wanted);
      }
    }

    // invoke receive callback (if there is still data)
    if (tud_cdc_rx_cb && tu_fifo_count(&p_cdc->rx_ff) ) tud_cdc_rx_cb(itf);

    // prepare for next
    TU_ASSERT( dcd_edpt_xfer(rhport, p_cdc->ep_out, p_cdc->epin_buf, CFG_TUD_CDC_EPSIZE), TUSB_ERROR_DCD_EDPT_XFER );
  }

  // nothing to do with in and notif endpoint

  return TUSB_ERROR_NONE;
}

#if CFG_TUD_CDC_FLUSH_ON_SOF
void cdcd_sof(uint8_t rhport)
{
  for(uint8_t i=0; i<CFG_TUD_CDC; i++)
  {
    tud_cdc_n_flush(i);
  }
}
#endif

#endif
