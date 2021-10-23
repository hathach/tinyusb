/* 
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * This file is part of the TinyUSB stack.
 */

#include "tusb_option.h"

#if (TUSB_OPT_HOST_ENABLED && CFG_TUH_CDC)

#include "host/usbh.h"
#include "host/usbh_classdriver.h"

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
static cdch_data_t cdch_data[CFG_TUH_DEVICE_MAX];

static inline cdch_data_t* get_itf(uint8_t dev_addr)
{
  return &cdch_data[dev_addr-1];
}

bool tuh_cdc_mounted(uint8_t dev_addr)
{
  cdch_data_t* cdc = get_itf(dev_addr);
  return cdc->ep_in && cdc->ep_out;
}

bool tuh_cdc_is_busy(uint8_t dev_addr, cdc_pipeid_t pipeid)
{
  if ( !tuh_cdc_mounted(dev_addr) ) return false;

  cdch_data_t const * p_cdc = get_itf(dev_addr);

  switch (pipeid)
  {
    case CDC_PIPE_NOTIFICATION:
      return usbh_edpt_busy(dev_addr, p_cdc->ep_notif );

    case CDC_PIPE_DATA_IN:
      return usbh_edpt_busy(dev_addr, p_cdc->ep_in );

    case CDC_PIPE_DATA_OUT:
      return usbh_edpt_busy(dev_addr, p_cdc->ep_out );

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
      (cdch_data[dev_addr-1].itf_protocol <= CDC_COMM_PROTOCOL_ATCOMMAND_CDMA);
}

bool tuh_cdc_send(uint8_t dev_addr, void const * p_data, uint32_t length, bool is_notify)
{
  (void) is_notify;
  TU_VERIFY( tuh_cdc_mounted(dev_addr) );
  TU_VERIFY( p_data != NULL && length, TUSB_ERROR_INVALID_PARA);

  uint8_t const ep_out = cdch_data[dev_addr-1].ep_out;
  if ( usbh_edpt_busy(dev_addr, ep_out) ) return false;

  return usbh_edpt_xfer(dev_addr, ep_out, (void*)(uintptr_t) p_data, length);
}

bool tuh_cdc_receive(uint8_t dev_addr, void * p_buffer, uint32_t length, bool is_notify)
{
  (void) is_notify;
  TU_VERIFY( tuh_cdc_mounted(dev_addr) );
  TU_VERIFY( p_buffer != NULL && length, TUSB_ERROR_INVALID_PARA);

  uint8_t const ep_in = cdch_data[dev_addr-1].ep_in;
  if ( usbh_edpt_busy(dev_addr, ep_in) ) return false;

  return usbh_edpt_xfer(dev_addr, ep_in, p_buffer, length);
}

bool tuh_cdc_set_control_line_state(uint8_t dev_addr, bool dtr, bool rts, tuh_control_complete_cb_t complete_cb)
{
  cdch_data_t const * p_cdc = get_itf(dev_addr);
  tusb_control_request_t const request =
  {
    .bmRequestType_bit =
    {
      .recipient = TUSB_REQ_RCPT_INTERFACE,
      .type      = TUSB_REQ_TYPE_CLASS,
      .direction = TUSB_DIR_OUT
    },
    .bRequest = CDC_REQUEST_SET_CONTROL_LINE_STATE,
    .wValue   = (rts ? 2 : 0) | (dtr ? 1 : 0),
    .wIndex   = p_cdc->itf_num,
    .wLength  = 0
  };

  TU_ASSERT( tuh_control_xfer(dev_addr, &request, NULL, complete_cb) );
  return true;
}

//--------------------------------------------------------------------+
// USBH-CLASS DRIVER API
//--------------------------------------------------------------------+
void cdch_init(void)
{
  tu_memclr(cdch_data, sizeof(cdch_data));
}

bool cdch_open(uint8_t rhport, uint8_t dev_addr, tusb_desc_interface_t const *itf_desc, uint16_t max_len)
{
  (void) max_len;

  // Only support ACM subclass
  // Protocol 0xFF can be RNDIS device for windows XP
  TU_VERIFY( TUSB_CLASS_CDC                           == itf_desc->bInterfaceClass &&
             CDC_COMM_SUBCLASS_ABSTRACT_CONTROL_MODEL == itf_desc->bInterfaceSubClass &&
             0xFF                                     != itf_desc->bInterfaceProtocol);

  cdch_data_t * p_cdc = get_itf(dev_addr);

  p_cdc->itf_num      = itf_desc->bInterfaceNumber;
  p_cdc->itf_protocol = itf_desc->bInterfaceProtocol;

  //------------- Communication Interface -------------//
  uint16_t drv_len = tu_desc_len(itf_desc);
  uint8_t const * p_desc = tu_desc_next(itf_desc);

  // Communication Functional Descriptors
  while( TUSB_DESC_CS_INTERFACE == tu_desc_type(p_desc) && drv_len <= max_len )
  {
    if ( CDC_FUNC_DESC_ABSTRACT_CONTROL_MANAGEMENT == cdc_functional_desc_typeof(p_desc) )
    {
      // save ACM bmCapabilities
      p_cdc->acm_capability = ((cdc_desc_func_acm_t const *) p_desc)->bmCapabilities;
    }

    drv_len += tu_desc_len(p_desc);
    p_desc = tu_desc_next(p_desc);
  }

  if ( TUSB_DESC_ENDPOINT == tu_desc_type(p_desc) )
  {
    // notification endpoint
    tusb_desc_endpoint_t const * desc_ep = (tusb_desc_endpoint_t const *) p_desc;

    TU_ASSERT( usbh_edpt_open(rhport, dev_addr, desc_ep) );
    p_cdc->ep_notif = desc_ep->bEndpointAddress;

    drv_len += tu_desc_len(p_desc);
    p_desc = tu_desc_next(p_desc);
  }

  //------------- Data Interface (if any) -------------//
  if ( (TUSB_DESC_INTERFACE == tu_desc_type(p_desc)) &&
       (TUSB_CLASS_CDC_DATA == ((tusb_desc_interface_t const *) p_desc)->bInterfaceClass) )
  {
    // next to endpoint descriptor
    drv_len += tu_desc_len(p_desc);
    p_desc = tu_desc_next(p_desc);

    // data endpoints expected to be in pairs
    for(uint32_t i=0; i<2; i++)
    {
      tusb_desc_endpoint_t const *desc_ep = (tusb_desc_endpoint_t const *) p_desc;
      TU_ASSERT(TUSB_DESC_ENDPOINT == desc_ep->bDescriptorType && TUSB_XFER_BULK == desc_ep->bmAttributes.xfer);

      TU_ASSERT(usbh_edpt_open(rhport, dev_addr, desc_ep));

      if ( tu_edpt_dir(desc_ep->bEndpointAddress) == TUSB_DIR_IN )
      {
        p_cdc->ep_in = desc_ep->bEndpointAddress;
      }else
      {
        p_cdc->ep_out = desc_ep->bEndpointAddress;
      }

      drv_len += tu_desc_len(p_desc);
      p_desc = tu_desc_next( p_desc );
    }
  }

  return true;
}

bool cdch_set_config(uint8_t dev_addr, uint8_t itf_num)
{
  (void) dev_addr; (void) itf_num;
  return true;
}

bool cdch_xfer_cb(uint8_t dev_addr, uint8_t ep_addr, xfer_result_t event, uint32_t xferred_bytes)
{
  (void) ep_addr;
  tuh_cdc_xfer_isr( dev_addr, event, 0, xferred_bytes );
  return true;
}

void cdch_close(uint8_t dev_addr)
{
  TU_VERIFY(dev_addr <= CFG_TUH_DEVICE_MAX, );

  cdch_data_t * p_cdc = get_itf(dev_addr);
  tu_memclr(p_cdc, sizeof(cdch_data_t));
}

#endif
