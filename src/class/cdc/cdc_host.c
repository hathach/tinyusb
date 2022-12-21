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
  tu_fifo_t ff;

  // mutex: read if ep rx, write if e tx
  OSAL_MUTEX_DEF(ff_mutex);

  // TODO xfer_fifo can skip this buffer
  uint8_t* ep_buf;
  uint16_t ep_bufsize;
  uint8_t ep_addr;
}tu_edpt_stream_t;

bool tu_edpt_stream_init(tu_edpt_stream_t* s, bool use_wr_mutex, bool overwritable,
                         void* ff_buf, uint16_t ff_bufsize,
                         uint8_t* ep_buf, uint16_t ep_bufsize)
{
  osal_mutex_t new_mutex = osal_mutex_create(&s->ff_mutex);
  (void) new_mutex;
  (void) use_wr_mutex;

  tu_fifo_config(&s->ff, ff_buf, ff_bufsize, 1, overwritable);
  tu_fifo_config_mutex(&s->ff, use_wr_mutex ? new_mutex : NULL, use_wr_mutex ? NULL : new_mutex);

  s->ep_buf = ep_buf;
  s->ep_bufsize = ep_bufsize;

  return true;
}

bool tu_edpt_stream_clear(tu_edpt_stream_t* s)
{
  return tu_fifo_clear(&s->ff);
}

bool tu_edpt_stream_write_zlp_if_needed(uint8_t daddr, tu_edpt_stream_t* s, uint32_t last_xferred_bytes)
{
  uint16_t const bulk_packet_size = (tuh_speed_get(daddr) == TUSB_SPEED_HIGH) ? TUSB_EPSIZE_BULK_HS : TUSB_EPSIZE_BULK_FS;

  // ZLP condition: no pending data, last transferred bytes is multiple of packet size
  TU_VERIFY( !tu_fifo_count(&s->ff) && last_xferred_bytes && (0 == (last_xferred_bytes & (bulk_packet_size-1))) );

  if ( usbh_edpt_claim(daddr, s->ep_addr) )
  {
    TU_ASSERT( usbh_edpt_xfer(daddr, s->ep_addr, NULL, 0) );
  }

  return true;
}

uint32_t tu_edpt_stream_write_xfer(uint8_t daddr, tu_edpt_stream_t* s)
{
  // skip if no data
  TU_VERIFY( tu_fifo_count(&s->ff), 0 );

  // Claim the endpoint
  // uint8_t const rhport = 0;
  // TU_VERIFY( usbd_edpt_claim(rhport, p_cdc->ep_in), 0 );
  TU_VERIFY( usbh_edpt_claim(daddr, s->ep_addr) );

  // Pull data from FIFO -> EP buf
  uint16_t const count = tu_fifo_read_n(&s->ff, s->ep_buf, s->ep_bufsize);

  if ( count )
  {
    //TU_ASSERT( usbd_edpt_xfer(rhport, p_cdc->ep_in, p_cdc->epin_buf, count), 0 );
    TU_ASSERT( usbh_edpt_xfer(daddr, s->ep_addr, s->ep_buf, count), 0 );
    return count;
  }else
  {
    // Release endpoint since we don't make any transfer
    // Note: data is dropped if terminal is not connected
    //usbd_edpt_release(rhport, p_cdc->ep_in);

    usbh_edpt_release(daddr, s->ep_addr);
    return 0;
  }
}

uint32_t tu_edpt_stream_write(uint8_t daddr, tu_edpt_stream_t* s, void const *buffer, uint32_t bufsize)
{
  uint16_t ret = tu_fifo_write_n(&s->ff, buffer, (uint16_t) bufsize);

  // flush if queue more than packet size
  uint16_t const bulk_packet_size = (tuh_speed_get(daddr) == TUSB_SPEED_HIGH) ? TUSB_EPSIZE_BULK_HS : TUSB_EPSIZE_BULK_FS;
  if ( (tu_fifo_count(&s->ff) >= bulk_packet_size)
      /* || ((CFG_TUD_CDC_TX_BUFSIZE < BULK_PACKET_SIZE) && tu_fifo_full(&p_cdc->tx_ff)) */ )
  {
    tu_edpt_stream_write_xfer(daddr, s);
  }

  return ret;
}

void tu_edpt_stream_read_xfer_complete(tu_edpt_stream_t* s, uint32_t xferred_bytes)
{
  tu_fifo_write_n(&s->ff, s->ep_buf, (uint16_t) xferred_bytes);
}

uint32_t tu_edpt_stream_read_xfer(uint8_t daddr, tu_edpt_stream_t* s)
{
  uint16_t available = tu_fifo_remaining(&s->ff);

  // Prepare for incoming data but only allow what we can store in the ring buffer.
  // TODO Actually we can still carry out the transfer, keeping count of received bytes
  // and slowly move it to the FIFO when read().
  // This pre-check reduces endpoint claiming
  uint16_t const bulk_packet_size = (tuh_speed_get(daddr) == TUSB_SPEED_HIGH) ? TUSB_EPSIZE_BULK_HS : TUSB_EPSIZE_BULK_FS;
  TU_VERIFY(available >= bulk_packet_size);

  // claim endpoint
  TU_VERIFY(usbh_edpt_claim(daddr, s->ep_addr), 0);

  // fifo can be changed before endpoint is claimed
  available = tu_fifo_remaining(&s->ff);

  if ( available >= bulk_packet_size )
  {
    // multiple of packet size limit by ep bufsize
    uint16_t count = (uint16_t) (available & (bulk_packet_size -1));
    count = tu_min16(count, s->ep_bufsize);

    TU_ASSERT( usbh_edpt_xfer(daddr, s->ep_addr, s->ep_buf, count), 0 );
    return count;
  }else
  {
    // Release endpoint since we don't make any transfer
    usbh_edpt_release(daddr, s->ep_addr);
    return 0;
  }
}

uint32_t tu_edpt_stream_read(uint8_t daddr, tu_edpt_stream_t* s, void* buffer, uint32_t bufsize)
{
  uint32_t num_read = tu_fifo_read_n(&s->ff, buffer, (uint16_t) bufsize);
  tu_edpt_stream_read_xfer(daddr, s);
  return num_read;
}

uint32_t tu_edpt_stream_read_available(tu_edpt_stream_t* s)
{
  return (uint32_t) tu_fifo_count(&s->ff);
}

typedef struct {
  uint8_t daddr;
  uint8_t bInterfaceNumber;
  uint8_t bInterfaceSubClass;
  uint8_t bInterfaceProtocol;

  cdc_acm_capability_t acm_capability;
  uint8_t ep_notif;

  // Bit 0:  DTR (Data Terminal Ready), Bit 1: RTS (Request to Send)
  uint8_t line_state;

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

static inline cdch_interface_t* get_itf_by_ep_addr(uint8_t daddr, uint8_t ep_addr)
{
  for(uint8_t i=0; i<CFG_TUH_CDC; i++)
  {
    cdch_interface_t* p_cdc = &cdch_data[i];
    if ( (p_cdc->daddr == daddr) &&
         (ep_addr == p_cdc->ep_notif || ep_addr == p_cdc->stream.rx.ep_addr || ep_addr == p_cdc->stream.tx.ep_addr))
    {
      return p_cdc;
    }
  }

  return NULL;
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

uint32_t tuh_cdc_write(uint8_t idx, void const* buffer, uint32_t bufsize)
{
  cdch_interface_t* p_cdc = get_itf(idx);
  TU_VERIFY(p_cdc);

  return tu_edpt_stream_write(p_cdc->daddr, &p_cdc->stream.tx, buffer, bufsize);
}

uint32_t tuh_cdc_write_flush(uint8_t idx)
{
  cdch_interface_t* p_cdc = get_itf(idx);
  TU_VERIFY(p_cdc);

  return tu_edpt_stream_write_xfer(p_cdc->daddr, &p_cdc->stream.tx);
}

uint32_t tuh_cdc_read (uint8_t idx, void* buffer, uint32_t bufsize)
{
  cdch_interface_t* p_cdc = get_itf(idx);
  TU_VERIFY(p_cdc);

  return tu_edpt_stream_read(p_cdc->daddr, &p_cdc->stream.rx, buffer, bufsize);
}

uint32_t tuh_cdc_read_available(uint8_t idx)
{
  cdch_interface_t* p_cdc = get_itf(idx);
  TU_VERIFY(p_cdc);

  return tu_edpt_stream_read_available(&p_cdc->stream.rx);
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
    .wIndex   = tu_htole16(p_cdc->bInterfaceNumber),
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

//--------------------------------------------------------------------+
// CLASS-USBH API
//--------------------------------------------------------------------+

void cdch_init(void)
{
  tu_memclr(cdch_data, sizeof(cdch_data));

  for(size_t i=0; i<CFG_TUH_CDC; i++)
  {
    cdch_interface_t* p_cdc = &cdch_data[i];

    tu_edpt_stream_init(&p_cdc->stream.tx, true, false,
                        p_cdc->stream.tx_ff_buf, CFG_TUH_CDC_TX_BUFSIZE,
                        p_cdc->stream.tx_ep_buf, CFG_TUH_CDC_TX_EPSIZE);

    tu_edpt_stream_init(&p_cdc->stream.rx, false, false,
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
      tu_edpt_stream_clear(&p_cdc->stream.tx);
      tu_edpt_stream_clear(&p_cdc->stream.rx);
    }
  }
}

bool cdch_xfer_cb(uint8_t daddr, uint8_t ep_addr, xfer_result_t event, uint32_t xferred_bytes)
{
  // TODO handle stall response, retry failed transfer ...
  TU_ASSERT(event == XFER_RESULT_SUCCESS);

  cdch_interface_t * p_cdc = get_itf_by_ep_addr(daddr, ep_addr);
  TU_ASSERT(p_cdc);

  if ( ep_addr == p_cdc->stream.tx.ep_addr )
  {
    if ( 0 == tu_edpt_stream_write_xfer(daddr, &p_cdc->stream.tx) )
    {
      // If there is no data left, a ZLP should be sent if needed
      // xferred_bytes is multiple of EP Packet size and not zero
      tu_edpt_stream_write_zlp_if_needed(daddr, &p_cdc->stream.tx, xferred_bytes);
    }
  }
  else if ( ep_addr == p_cdc->stream.rx.ep_addr )
  {
    // skip if ZLP
    if (xferred_bytes) tu_edpt_stream_read_xfer_complete(&p_cdc->stream.rx, xferred_bytes);

    // invoke receive callback

    // prepare for next transfer if needed
    tu_edpt_stream_read_xfer(daddr, &p_cdc->stream.rx);
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

bool cdch_open(uint8_t rhport, uint8_t dev_addr, tusb_desc_interface_t const *itf_desc, uint16_t max_len)
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

  p_cdc->daddr              = dev_addr;
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

    TU_ASSERT( tuh_edpt_open(dev_addr, desc_ep) );
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

      TU_ASSERT(tuh_edpt_open(dev_addr, desc_ep));

      if ( tu_edpt_dir(desc_ep->bEndpointAddress) == TUSB_DIR_IN )
      {
        p_cdc->stream.rx.ep_addr = desc_ep->bEndpointAddress;
      }else
      {
        p_cdc->stream.tx.ep_addr = desc_ep->bEndpointAddress;
      }

      p_desc = tu_desc_next(p_desc);
    }
  }

  return true;
}

static void config_cdc_complete(uint8_t daddr, uint8_t itf_num)
{
  uint8_t const idx = tuh_cdc_itf_get_index(daddr, itf_num);

  if (idx != TUSB_INDEX_INVALID)
  {
    if (tuh_cdc_mount_cb) tuh_cdc_mount_cb(idx);

    // Prepare for incoming data
    cdch_interface_t* p_cdc = get_itf(idx);
    tu_edpt_stream_read_xfer(daddr, &p_cdc->stream.rx);
  }

  // notify usbh that driver enumeration is complete
  // itf_num+1 to account for data interface as well
  usbh_driver_set_config_complete(daddr, itf_num+1);
}

#if CFG_TUH_CDC_SET_DTRRTS_ON_ENUM

static void config_set_dtr_rts_complete (tuh_xfer_t* xfer)
{
  uint8_t const itf_num = (uint8_t) tu_le16toh(xfer->setup->wIndex);
  config_cdc_complete(xfer->daddr, itf_num);
}

bool cdch_set_config(uint8_t daddr, uint8_t itf_num)
{
  uint8_t const idx = tuh_cdc_itf_get_index(daddr, itf_num);
  return tuh_cdc_set_control_line_state(idx, CFG_TUH_CDC_SET_DTRRTS_ON_ENUM, config_set_dtr_rts_complete, 0);
}

#else

bool cdch_set_config(uint8_t daddr, uint8_t itf_num)
{
  config_cdc_complete(daddr, itf_num);
  return true;
}

#endif

#endif
