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

#if (CFG_TUH_ENABLED && CFG_TUH_CDC)

#include "host/usbh.h"
#include "host/usbh_classdriver.h"

#include "cdc_host.h"

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF
//--------------------------------------------------------------------+
#if 0
typedef struct {
  tu_fifo_t ff;
  OSAL_MUTEX_DEF(ff_mutex);
}tu_edpt_stream_t;

uint32_t tud_cdc_read_available (void);
uint32_t tud_cdc_read (void *buffer, uint32_t bufsize);
void tud_cdc_read_flush (void);
bool tud_cdc_peek (uint8_t *ui8);

uint32_t tud_cdc_write (void const *buffer, uint32_t bufsize);
uint32_t tud_cdc_write_flush (void);
uint32_t tud_cdc_write_available (void);

//--------------------------------------------------------
tu_edpt_stream_write()
#endif

typedef struct {
  uint8_t daddr;
  uint8_t itf_num;
  uint8_t itf_protocol;

  uint8_t ep_notif;
  uint8_t ep_in;
  uint8_t ep_out;

  cdc_acm_capability_t acm_capability;

  // Bit 0:  DTR (Data Terminal Ready), Bit 1: RTS (Request to Send)
  // uint8_t line_state;

  // FIFO
  tu_fifo_t rx_ff;
  tu_fifo_t tx_ff;

  uint8_t rx_ff_buf[CFG_TUH_CDC_RX_BUFSIZE];
  uint8_t tx_ff_buf[CFG_TUH_CDC_TX_BUFSIZE];

  OSAL_MUTEX_DEF(rx_ff_mutex);
  OSAL_MUTEX_DEF(tx_ff_mutex);

  // Endpoint Transfer buffer
  CFG_TUSB_MEM_ALIGN uint8_t epin_buf[CFG_TUH_CDC_RX_EPSIZE];
  CFG_TUSB_MEM_ALIGN uint8_t epout_buf[CFG_TUH_CDC_TX_EPSIZE];

} cdch_interface_t;

//--------------------------------------------------------------------+
// INTERNAL OBJECT & FUNCTION DECLARATION
//--------------------------------------------------------------------+

CFG_TUSB_MEM_SECTION
static cdch_interface_t cdch_data[CFG_TUH_CDC];

static inline cdch_interface_t* get_itf(uint8_t dev_addr)
{
  for(size_t i=0; i<CFG_TUH_CDC; i++)
  {
    if (cdch_data[i].daddr == dev_addr) return &cdch_data[i];
  }

  return NULL;
}

static cdch_interface_t* find_new_itf(void)
{
  for(size_t i=0; i<CFG_TUH_CDC; i++)
  {
    if (cdch_data[i].daddr == 0) return &cdch_data[i];
  }

  return NULL;
}

//--------------------------------------------------------------------+
// APPLICATION API (parameter validation needed)
//--------------------------------------------------------------------+

bool tuh_cdc_mounted(uint8_t dev_addr)
{
  cdch_interface_t* p_cdc = get_itf(dev_addr);
  return p_cdc != NULL;
}

uint32_t tuh_cdc_write(uint8_t dev_addr, void const* buffer, uint32_t bufsize)
{
  (void) dev_addr;
  (void) buffer;
  (void) bufsize;

  return 0;
}

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
  TU_VERIFY( p_data != NULL && length);

  uint8_t const ep_out = cdch_data[dev_addr-1].ep_out;
  if ( usbh_edpt_busy(dev_addr, ep_out) ) return false;

  return usbh_edpt_xfer(dev_addr, ep_out, (void*)(uintptr_t) p_data, (uint16_t) length);
}

bool tuh_cdc_receive(uint8_t dev_addr, void * p_buffer, uint32_t length, bool is_notify)
{
  (void) is_notify;
  TU_VERIFY( tuh_cdc_mounted(dev_addr) );
  TU_VERIFY( p_buffer != NULL && length );

  uint8_t const ep_in = cdch_data[dev_addr-1].ep_in;
  if ( usbh_edpt_busy(dev_addr, ep_in) ) return false;

  return usbh_edpt_xfer(dev_addr, ep_in, p_buffer, (uint16_t) length);
}

bool tuh_cdc_set_control_line_state(uint8_t dev_addr, bool dtr, bool rts, tuh_xfer_cb_t complete_cb)
{
  cdch_interface_t const * p_cdc = get_itf(dev_addr);
  TU_VERIFY(p_cdc && p_cdc->acm_capability.support_line_request);

  tusb_control_request_t const request =
  {
    .bmRequestType_bit =
    {
      .recipient = TUSB_REQ_RCPT_INTERFACE,
      .type      = TUSB_REQ_TYPE_CLASS,
      .direction = TUSB_DIR_OUT
    },
    .bRequest = CDC_REQUEST_SET_CONTROL_LINE_STATE,
    .wValue   = tu_htole16((uint16_t) ((dtr ? 1u : 0u) | (rts ? 2u : 0u))),
    .wIndex   = tu_htole16(p_cdc->itf_num),
    .wLength  = 0
  };

  tuh_xfer_t xfer =
  {
    .daddr       = dev_addr,
    .ep_addr     = 0,
    .setup       = &request,
    .buffer      = NULL,
    .complete_cb = complete_cb,
    .user_data    = 0
  };

  return tuh_control_xfer(&xfer);
}

//--------------------------------------------------------------------+
// CLASS-USBH API
//--------------------------------------------------------------------+

void cdch_init(void)
{
  tu_memclr(cdch_data, sizeof(cdch_data));

//  for(size_t i=0; i<CFG_TUH_CDC; i++)
//  {
//    cdch_interface_t* p_cdc = &cdch_data[i];
//
//
//  }
}

void cdch_close(uint8_t dev_addr)
{
  cdch_interface_t * p_cdc = get_itf(dev_addr);
  TU_VERIFY(p_cdc, );

  // Invoke application callback
  if (tuh_cdc_umount_cb)
  {
    tuh_cdc_umount_cb(dev_addr);
  }

  tu_memclr(p_cdc, sizeof(cdch_interface_t));
}

bool cdch_xfer_cb(uint8_t dev_addr, uint8_t ep_addr, xfer_result_t event, uint32_t xferred_bytes)
{
  (void) ep_addr;
  tuh_cdc_xfer_isr( dev_addr, event, 0, xferred_bytes );
  return true;
}

//--------------------------------------------------------------------+
// Enumeration
//--------------------------------------------------------------------+

bool cdch_open(uint8_t rhport, uint8_t dev_addr, tusb_desc_interface_t const *itf_desc, uint16_t max_len)
{
  (void) rhport;
  (void) max_len;

  // Only support ACM subclass
  // Protocol 0xFF can be RNDIS device for windows XP
  TU_VERIFY( TUSB_CLASS_CDC                           == itf_desc->bInterfaceClass &&
             CDC_COMM_SUBCLASS_ABSTRACT_CONTROL_MODEL == itf_desc->bInterfaceSubClass &&
             0xFF                                     != itf_desc->bInterfaceProtocol);

  cdch_interface_t * p_cdc = find_new_itf();
  TU_VERIFY(p_cdc);

  p_cdc->daddr        = dev_addr;
  p_cdc->itf_num      = itf_desc->bInterfaceNumber;
  p_cdc->itf_protocol = itf_desc->bInterfaceProtocol;

  //------------- Control Interface -------------//
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

  // Open notification endpoint of control interface if any
  if (itf_desc->bNumEndpoints == 1)
  {
    TU_ASSERT(TUSB_DESC_ENDPOINT == tu_desc_type(p_desc));
    tusb_desc_endpoint_t const * desc_ep = (tusb_desc_endpoint_t const *) p_desc;

    TU_ASSERT( tuh_edpt_open(dev_addr, desc_ep) );
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

      TU_ASSERT(tuh_edpt_open(dev_addr, desc_ep));

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
  if (tuh_cdc_mount_cb) tuh_cdc_mount_cb(dev_addr);

  // notify usbh that driver enumeration is complete
  // itf_num+1 to account for data interface as well
  usbh_driver_set_config_complete(dev_addr, itf_num+1);

  return true;
}

#endif
