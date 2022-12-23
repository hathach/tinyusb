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


// Debug level, TUSB_CFG_DEBUG must be at least this level for debug message
#define CDCH_DEBUG   2

#define TU_LOG_CDCH(...)   TU_LOG(CDCH_DEBUG, __VA_ARGS__)

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF
//--------------------------------------------------------------------+

typedef struct {
  uint8_t daddr;
  uint8_t bInterfaceNumber;
  uint8_t bInterfaceSubClass;
  uint8_t bInterfaceProtocol;

  cdc_acm_capability_t acm_capability;
  uint8_t ep_notif;

  cdc_line_coding_t line_coding;  // Baudrate, stop bits, parity, data width
  uint8_t line_state;             // DTR (bit0), RTS (bit1)

  tuh_xfer_cb_t user_control_cb;

  struct {
    tu_edpt_stream_t tx;
    tu_edpt_stream_t rx;

    uint8_t tx_ff_buf[CFG_TUH_CDC_TX_BUFSIZE];
    CFG_TUSB_MEM_ALIGN uint8_t tx_ep_buf[CFG_TUH_CDC_TX_EPSIZE];

    uint8_t rx_ff_buf[CFG_TUH_CDC_TX_BUFSIZE];
    CFG_TUSB_MEM_ALIGN uint8_t rx_ep_buf[CFG_TUH_CDC_TX_EPSIZE];
  } stream;

} cdch_interface_t;

//--------------------------------------------------------------------+
// INTERNAL OBJECT & FUNCTION DECLARATION
//--------------------------------------------------------------------+

CFG_TUSB_MEM_SECTION
static cdch_interface_t cdch_data[CFG_TUH_CDC];

static inline cdch_interface_t* get_itf(uint8_t idx)
{
  TU_ASSERT(idx < CFG_TUH_CDC, NULL);
  cdch_interface_t* p_cdc = &cdch_data[idx];

  return (p_cdc->daddr != 0) ? p_cdc : NULL;
}

static inline uint8_t get_idx_by_ep_addr(uint8_t daddr, uint8_t ep_addr)
{
  for(uint8_t i=0; i<CFG_TUH_CDC; i++)
  {
    cdch_interface_t* p_cdc = &cdch_data[i];
    if ( (p_cdc->daddr == daddr) &&
         (ep_addr == p_cdc->ep_notif || ep_addr == p_cdc->stream.rx.ep_addr || ep_addr == p_cdc->stream.tx.ep_addr))
    {
      return i;
    }
  }

  return TUSB_INDEX_INVALID;
}


static cdch_interface_t* find_new_itf(void)
{
  for(uint8_t i=0; i<CFG_TUH_CDC; i++)
  {
    if (cdch_data[i].daddr == 0) return &cdch_data[i];
  }

  return NULL;
}

//--------------------------------------------------------------------+
// APPLICATION API
//--------------------------------------------------------------------+

uint8_t tuh_cdc_itf_get_index(uint8_t daddr, uint8_t itf_num)
{
  for(uint8_t i=0; i<CFG_TUH_CDC; i++)
  {
    const cdch_interface_t* p_cdc = &cdch_data[i];

    if (p_cdc->daddr == daddr && p_cdc->bInterfaceNumber == itf_num) return i;
  }

  return TUSB_INDEX_INVALID;
}

bool tuh_cdc_itf_get_info(uint8_t idx, tuh_cdc_itf_info_t* info)
{
  cdch_interface_t* p_cdc = get_itf(idx);
  TU_VERIFY(p_cdc && info);

  info->daddr              = p_cdc->daddr;
  info->bInterfaceNumber   = p_cdc->bInterfaceNumber;
  info->bInterfaceSubClass = p_cdc->bInterfaceSubClass;
  info->bInterfaceProtocol = p_cdc->bInterfaceProtocol;

  return true;
}

bool tuh_cdc_mounted(uint8_t idx)
{
  cdch_interface_t* p_cdc = get_itf(idx);
  return p_cdc != NULL;
}

bool tuh_cdc_get_dtr(uint8_t idx)
{
  cdch_interface_t* p_cdc = get_itf(idx);
  TU_VERIFY(p_cdc);

  return (p_cdc->line_state & CDC_CONTROL_LINE_STATE_DTR) ? true : false;
}

bool tuh_cdc_get_rts(uint8_t idx)
{
  cdch_interface_t* p_cdc = get_itf(idx);
  TU_VERIFY(p_cdc);

  return (p_cdc->line_state & CDC_CONTROL_LINE_STATE_RTS) ? true : false;
}

bool tuh_cdc_get_local_line_coding(uint8_t idx, cdc_line_coding_t* line_coding)
{
  cdch_interface_t* p_cdc = get_itf(idx);
  TU_VERIFY(p_cdc);

  *line_coding = p_cdc->line_coding;

  return true;
}

//--------------------------------------------------------------------+
// Write
//--------------------------------------------------------------------+

uint32_t tuh_cdc_write(uint8_t idx, void const* buffer, uint32_t bufsize)
{
  cdch_interface_t* p_cdc = get_itf(idx);
  TU_VERIFY(p_cdc);

  return tu_edpt_stream_write(&p_cdc->stream.tx, buffer, bufsize);
}

uint32_t tuh_cdc_write_flush(uint8_t idx)
{
  cdch_interface_t* p_cdc = get_itf(idx);
  TU_VERIFY(p_cdc);

  return tu_edpt_stream_write_xfer(&p_cdc->stream.tx);
}

bool tuh_cdc_write_clear(uint8_t idx)
{
  cdch_interface_t* p_cdc = get_itf(idx);
  TU_VERIFY(p_cdc);

  return tu_edpt_stream_clear(&p_cdc->stream.tx);
}

uint32_t tuh_cdc_write_available(uint8_t idx)
{
  cdch_interface_t* p_cdc = get_itf(idx);
  TU_VERIFY(p_cdc);

  return tu_edpt_stream_write_available(&p_cdc->stream.tx);
}

//--------------------------------------------------------------------+
// Read
//--------------------------------------------------------------------+

uint32_t tuh_cdc_read (uint8_t idx, void* buffer, uint32_t bufsize)
{
  cdch_interface_t* p_cdc = get_itf(idx);
  TU_VERIFY(p_cdc);

  return tu_edpt_stream_read(&p_cdc->stream.rx, buffer, bufsize);
}

uint32_t tuh_cdc_read_available(uint8_t idx)
{
  cdch_interface_t* p_cdc = get_itf(idx);
  TU_VERIFY(p_cdc);

  return tu_edpt_stream_read_available(&p_cdc->stream.rx);
}

bool tuh_cdc_peek(uint8_t idx, uint8_t* ch)
{
  cdch_interface_t* p_cdc = get_itf(idx);
  TU_VERIFY(p_cdc);

  return tu_edpt_stream_peek(&p_cdc->stream.rx, ch);
}

bool tuh_cdc_read_clear (uint8_t idx)
{
  cdch_interface_t* p_cdc = get_itf(idx);
  TU_VERIFY(p_cdc);

  bool ret = tu_edpt_stream_clear(&p_cdc->stream.rx);
  tu_edpt_stream_read_xfer(&p_cdc->stream.rx);
  return ret;
}

//--------------------------------------------------------------------+
// Control Endpoint API
//--------------------------------------------------------------------+

// internal control complete to update state such as line state, encoding
static void cdch_internal_control_complete(tuh_xfer_t* xfer)
{
  uint8_t const itf_num = (uint8_t) tu_le16toh(xfer->setup->wIndex);
  uint8_t idx = tuh_cdc_itf_get_index(xfer->daddr, itf_num);
  cdch_interface_t* p_cdc = get_itf(idx);
  TU_ASSERT(p_cdc, );

  if (xfer->result == XFER_RESULT_SUCCESS)
  {
    switch(xfer->setup->bRequest)
    {
      case CDC_REQUEST_SET_CONTROL_LINE_STATE:
        p_cdc->line_state = (uint8_t) tu_le16toh(xfer->setup->wValue);
      break;

      case CDC_REQUEST_SET_LINE_CODING:
      {
        uint16_t const len = tu_min16(sizeof(cdc_line_coding_t), tu_le16toh(xfer->setup->wLength));
        memcpy(&p_cdc->line_coding, xfer->buffer, len);
      }
      break;

      default: break;
    }
  }

  xfer->complete_cb = p_cdc->user_control_cb;
  xfer->complete_cb(xfer);
}

bool tuh_cdc_set_control_line_state(uint8_t idx, uint16_t line_state, tuh_xfer_cb_t complete_cb, uintptr_t user_data)
{
  cdch_interface_t* p_cdc = get_itf(idx);
  TU_VERIFY(p_cdc && p_cdc->acm_capability.support_line_request);

  TU_LOG_CDCH("CDC Set Control Line State\r\n");

  tusb_control_request_t const request =
  {
    .bmRequestType_bit =
    {
      .recipient = TUSB_REQ_RCPT_INTERFACE,
      .type      = TUSB_REQ_TYPE_CLASS,
      .direction = TUSB_DIR_OUT
    },
    .bRequest = CDC_REQUEST_SET_CONTROL_LINE_STATE,
    .wValue   = tu_htole16(line_state),
    .wIndex   = tu_htole16((uint16_t) p_cdc->bInterfaceNumber),
    .wLength  = 0
  };

  p_cdc->user_control_cb = complete_cb;
  tuh_xfer_t xfer =
  {
    .daddr       = p_cdc->daddr,
    .ep_addr     = 0,
    .setup       = &request,
    .buffer      = NULL,
    .complete_cb = cdch_internal_control_complete,
    .user_data   = user_data
  };

  return tuh_control_xfer(&xfer);
}

bool tuh_cdc_set_line_coding(uint8_t idx, cdc_line_coding_t const* line_coding, tuh_xfer_cb_t complete_cb, uintptr_t user_data)
{
  cdch_interface_t* p_cdc = get_itf(idx);
  TU_VERIFY(p_cdc && p_cdc->acm_capability.support_line_request);

  TU_LOG_CDCH("CDC Set Line Conding\r\n");

  tusb_control_request_t const request =
  {
    .bmRequestType_bit =
    {
      .recipient = TUSB_REQ_RCPT_INTERFACE,
      .type      = TUSB_REQ_TYPE_CLASS,
      .direction = TUSB_DIR_OUT
    },
    .bRequest = CDC_REQUEST_SET_LINE_CODING,
    .wValue   = 0,
    .wIndex   = tu_htole16(p_cdc->bInterfaceNumber),
    .wLength  = tu_htole16(sizeof(cdc_line_coding_t))
  };

  // use usbh enum buf to hold line coding since user line_coding variable may not live long enough
  // for the transfer to complete
  uint8_t* enum_buf = usbh_get_enum_buf();
  memcpy(enum_buf, line_coding, sizeof(cdc_line_coding_t));

  p_cdc->user_control_cb = complete_cb;
  tuh_xfer_t xfer =
  {
    .daddr       = p_cdc->daddr,
    .ep_addr     = 0,
    .setup       = &request,
    .buffer      = enum_buf,
    .complete_cb = cdch_internal_control_complete,
    .user_data   = user_data
  };

  return tuh_control_xfer(&xfer);
}

//--------------------------------------------------------------------+
// CLASS-USBH API
//--------------------------------------------------------------------+

void cdch_init(void)
{
  tu_memclr(cdch_data, sizeof(cdch_data));

  for(size_t i=0; i<CFG_TUH_CDC; i++)
  {
    cdch_interface_t* p_cdc = &cdch_data[i];

    tu_edpt_stream_init(&p_cdc->stream.tx, true, true, false,
                          p_cdc->stream.tx_ff_buf, CFG_TUH_CDC_TX_BUFSIZE,
                          p_cdc->stream.tx_ep_buf, CFG_TUH_CDC_TX_EPSIZE);

    tu_edpt_stream_init(&p_cdc->stream.rx, true, false, false,
                          p_cdc->stream.rx_ff_buf, CFG_TUH_CDC_RX_BUFSIZE,
                          p_cdc->stream.rx_ep_buf, CFG_TUH_CDC_RX_EPSIZE);
  }
}

void cdch_close(uint8_t daddr)
{
  for(uint8_t idx=0; idx<CFG_TUH_CDC; idx++)
  {
    cdch_interface_t* p_cdc = &cdch_data[idx];
    if (p_cdc->daddr == daddr)
    {
      // Invoke application callback
      if (tuh_cdc_umount_cb) tuh_cdc_umount_cb(idx);

      //tu_memclr(p_cdc, sizeof(cdch_interface_t));
      p_cdc->daddr = 0;
      p_cdc->bInterfaceNumber = 0;
      tu_edpt_stream_close(&p_cdc->stream.tx);
      tu_edpt_stream_close(&p_cdc->stream.rx);
    }
  }
}

bool cdch_xfer_cb(uint8_t daddr, uint8_t ep_addr, xfer_result_t event, uint32_t xferred_bytes)
{
  // TODO handle stall response, retry failed transfer ...
  TU_ASSERT(event == XFER_RESULT_SUCCESS);

  uint8_t const idx = get_idx_by_ep_addr(daddr, ep_addr);
  cdch_interface_t * p_cdc = get_itf(idx);
  TU_ASSERT(p_cdc);

  if ( ep_addr == p_cdc->stream.tx.ep_addr )
  {
    // invoke tx complete callback to possibly refill tx fifo
    if (tuh_cdc_tx_complete_cb) tuh_cdc_tx_complete_cb(idx);

    if ( 0 == tu_edpt_stream_write_xfer(&p_cdc->stream.tx) )
    {
      // If there is no data left, a ZLP should be sent if:
      // - xferred_bytes is multiple of EP Packet size and not zero
      tu_edpt_stream_write_zlp_if_needed(&p_cdc->stream.tx, xferred_bytes);
    }
  }
  else if ( ep_addr == p_cdc->stream.rx.ep_addr )
  {
    tu_edpt_stream_read_xfer_complete(&p_cdc->stream.rx, xferred_bytes);

    // invoke receive callback
    if (tuh_cdc_rx_cb)  tuh_cdc_rx_cb(idx);

    // prepare for next transfer if needed
    tu_edpt_stream_read_xfer(&p_cdc->stream.rx);
  }else if ( ep_addr == p_cdc->ep_notif )
  {
    // TODO handle notification endpoint
  }else
  {
    TU_ASSERT(false);
  }

  return true;
}

//--------------------------------------------------------------------+
// Enumeration
//--------------------------------------------------------------------+

bool cdch_open(uint8_t rhport, uint8_t daddr, tusb_desc_interface_t const *itf_desc, uint16_t max_len)
{
  (void) rhport;

  // Only support ACM subclass
  // Protocol 0xFF can be RNDIS device for windows XP
  TU_VERIFY( TUSB_CLASS_CDC                           == itf_desc->bInterfaceClass &&
             CDC_COMM_SUBCLASS_ABSTRACT_CONTROL_MODEL == itf_desc->bInterfaceSubClass &&
             0xFF                                     != itf_desc->bInterfaceProtocol);

  uint8_t const * p_desc_end = ((uint8_t const*) itf_desc) + max_len;

  cdch_interface_t * p_cdc = find_new_itf();
  TU_VERIFY(p_cdc);

  p_cdc->daddr              = daddr;
  p_cdc->bInterfaceNumber   = itf_desc->bInterfaceNumber;
  p_cdc->bInterfaceSubClass = itf_desc->bInterfaceSubClass;
  p_cdc->bInterfaceProtocol = itf_desc->bInterfaceProtocol;
  p_cdc->line_state         = 0;

  //------------- Control Interface -------------//
  uint8_t const * p_desc = tu_desc_next(itf_desc);

  // Communication Functional Descriptors
  while( (p_desc < p_desc_end) && (TUSB_DESC_CS_INTERFACE == tu_desc_type(p_desc)) )
  {
    if ( CDC_FUNC_DESC_ABSTRACT_CONTROL_MANAGEMENT == cdc_functional_desc_typeof(p_desc) )
    {
      // save ACM bmCapabilities
      p_cdc->acm_capability = ((cdc_desc_func_acm_t const *) p_desc)->bmCapabilities;
    }

    p_desc = tu_desc_next(p_desc);
  }

  // Open notification endpoint of control interface if any
  if (itf_desc->bNumEndpoints == 1)
  {
    TU_ASSERT(TUSB_DESC_ENDPOINT == tu_desc_type(p_desc));
    tusb_desc_endpoint_t const * desc_ep = (tusb_desc_endpoint_t const *) p_desc;

    TU_ASSERT( tuh_edpt_open(daddr, desc_ep) );
    p_cdc->ep_notif = desc_ep->bEndpointAddress;

    p_desc = tu_desc_next(p_desc);
  }

  //------------- Data Interface (if any) -------------//
  if ( (TUSB_DESC_INTERFACE == tu_desc_type(p_desc)) &&
       (TUSB_CLASS_CDC_DATA == ((tusb_desc_interface_t const *) p_desc)->bInterfaceClass) )
  {
    // next to endpoint descriptor
    p_desc = tu_desc_next(p_desc);

    // data endpoints expected to be in pairs
    for(uint32_t i=0; i<2; i++)
    {
      tusb_desc_endpoint_t const *desc_ep = (tusb_desc_endpoint_t const *) p_desc;
      TU_ASSERT(TUSB_DESC_ENDPOINT == desc_ep->bDescriptorType &&
                TUSB_XFER_BULK     == desc_ep->bmAttributes.xfer);

      TU_ASSERT(tuh_edpt_open(daddr, desc_ep));

      if ( tu_edpt_dir(desc_ep->bEndpointAddress) == TUSB_DIR_IN )
      {
        tu_edpt_stream_open(&p_cdc->stream.rx, daddr, desc_ep);
      }else
      {
        tu_edpt_stream_open(&p_cdc->stream.tx, daddr, desc_ep);
      }

      p_desc = tu_desc_next(p_desc);
    }
  }

  return true;
}

enum
{
  CONFIG_SET_CONTROL_LINE_STATE,
  CONFIG_SET_LINE_CODING,
  CONFIG_COMPLETE
};

static void process_cdc_config(tuh_xfer_t* xfer)
{
  uintptr_t const state = xfer->user_data;
  uint8_t const itf_num = (uint8_t) tu_le16toh(xfer->setup->wIndex);
  uint8_t const idx = tuh_cdc_itf_get_index(xfer->daddr, itf_num);
  TU_ASSERT(idx != TUSB_INDEX_INVALID, );

  switch(state)
  {
    case CONFIG_SET_CONTROL_LINE_STATE:
    #if CFG_TUH_CDC_LINE_CONTROL_ON_ENUM
      TU_ASSERT( tuh_cdc_set_control_line_state(idx, CFG_TUH_CDC_LINE_CONTROL_ON_ENUM, process_cdc_config, CONFIG_SET_LINE_CODING), );
      break;
    #endif
    TU_ATTR_FALLTHROUGH;

    case CONFIG_SET_LINE_CODING:
    #ifdef CFG_TUH_CDC_LINE_CODING_ON_ENUM
    {
      cdc_line_coding_t line_coding = CFG_TUH_CDC_LINE_CODING_ON_ENUM;
      TU_ASSERT( tuh_cdc_set_line_coding(idx, &line_coding, process_cdc_config, CONFIG_COMPLETE), );
      break;
    }
    #endif
    TU_ATTR_FALLTHROUGH;

    case CONFIG_COMPLETE:
      if (tuh_cdc_mount_cb) tuh_cdc_mount_cb(idx);

      // Prepare for incoming data
      cdch_interface_t* p_cdc = get_itf(idx);
      tu_edpt_stream_read_xfer(&p_cdc->stream.rx);

      // notify usbh that driver enumeration is complete
      // itf_num+1 to account for data interface as well
      usbh_driver_set_config_complete(xfer->daddr, itf_num+1);
    break;

    default: break;
  }
}

bool cdch_set_config(uint8_t daddr, uint8_t itf_num)
{
  // fake transfer to kick-off process
  tusb_control_request_t request;
  request.wIndex = tu_htole16((uint16_t) itf_num);

  tuh_xfer_t xfer;
  xfer.daddr     = daddr;
  xfer.result    = XFER_RESULT_SUCCESS;
  xfer.setup     = &request;
  xfer.user_data = CONFIG_SET_CONTROL_LINE_STATE;

  process_cdc_config(&xfer);

  return true;
}

#endif
