/* 
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
 * Copyright (c) 2020 Reinhard Panhuber, Jerzy Kasenberg
 * Copyright (c) 2021 Koji KITAYAMA
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

#if (TUSB_OPT_DEVICE_ENABLED && CFG_TUD_VIDEO)

#include "device/usbd.h"
#include "device/usbd_pvt.h"

#include "video_device.h"

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF
//--------------------------------------------------------------------+
typedef struct
{
  uint8_t itf_ctl;
  uint8_t itf_in;
  uint8_t itf_out;
  uint8_t const *desc_beg;
  uint8_t const *desc_end;
  tusb_desc_video_control_interface_t const          *ctl;
  tusb_desc_video_streaming_interface_input_t const  *stm_in;
  tusb_desc_video_streaming_interface_output_t const *stm_out;
  uint8_t ep_notif; /* notification */
  uint8_t ep_in;    /* video IN */
  uint8_t ep_sti;   /* still image IN */
  uint8_t ep_out;   /* video OUT */

  /*------------- From this point, data is not cleared by bus reset -------------*/

  // Endpoint Transfer buffer
  CFG_TUSB_MEM_ALIGN uint8_t epout_buf[CFG_TUD_CDC_EP_BUFSIZE];
  CFG_TUSB_MEM_ALIGN uint8_t epin_buf[CFG_TUD_CDC_EP_BUFSIZE];

} videod_interface_t;

#define ITF_MEM_RESET_SIZE   offsetof(cdcd_interface_t, wanted_char)

//--------------------------------------------------------------------+
// INTERNAL OBJECT & FUNCTION DECLARATION
//--------------------------------------------------------------------+
CFG_TUSB_MEM_SECTION static videod_interface_t _videod_itf[CFG_TUD_VIDEO];

static tusb_desc_interface_t const* videod_next_interface_desc(void const *beg, void const *end)
{
  for (void const *cur = tu_desc_next(beg); cur < end; cur = tu_desc_next(cur)) {
    if (TUSB_DESC_INTERFACE != tu_desc_type(cur))
      return (uint8_t const*)cur;
  }
}

static tusb_desc_interface_t const* videod_find_interface_desc(void const *beg, void const *end, unsigned itfnum)
{
  tusb_desc_interface_t const *cur = (tusb_desc_interface_t const *)beg;
  for (; cur; cur = videod_next_interface_desc(cur, end)) {
    if (cur->bInterfaceNumber == itfnum)
      return cur;
  }
  return NULL;
}

static void _prep_out_transaction (cdcd_interface_t* p_cdc)
{
  uint8_t const rhport = TUD_OPT_RHPORT;
  uint16_t available = tu_fifo_remaining(&p_cdc->rx_ff);

  // Prepare for incoming data but only allow what we can store in the ring buffer.
  // TODO Actually we can still carry out the transfer, keeping count of received bytes
  // and slowly move it to the FIFO when read().
  // This pre-check reduces endpoint claiming
  TU_VERIFY(available >= sizeof(p_cdc->epout_buf), );

  // claim endpoint
  TU_VERIFY(usbd_edpt_claim(rhport, p_cdc->ep_out), );

  // fifo can be changed before endpoint is claimed
  available = tu_fifo_remaining(&p_cdc->rx_ff);

  if ( available >= sizeof(p_cdc->epout_buf) )
  {
    usbd_edpt_xfer(rhport, p_cdc->ep_out, p_cdc->epout_buf, sizeof(p_cdc->epout_buf));
  }else
  {
    // Release endpoint since we don't make any transfer
    usbd_edpt_release(rhport, p_cdc->ep_out);
  }
}

//--------------------------------------------------------------------+
// APPLICATION API
//--------------------------------------------------------------------+
bool tud_video_n_connected(uint8_t itf)
{
  // DTR (bit 0) active  is considered as connected
  return tud_ready() && tu_bit_test(_cdcd_itf[itf].line_state, 0);
}

//--------------------------------------------------------------------+
// READ API
//--------------------------------------------------------------------+

//--------------------------------------------------------------------+
// WRITE API
//--------------------------------------------------------------------+

//--------------------------------------------------------------------+
// USBD Driver API
//--------------------------------------------------------------------+
void videod_init(void)
{
  tu_memclr(_videod_itf, sizeof(_videod_itf));

  for (unsigned i = 0; i < CFG_TUD_VIDEO; ++i)
  {
    videod_interface_t* p_video = &_videod_itf[i];

    // TODO
  }
}

void videod_reset(uint8_t rhport)
{
  (void) rhport;

  for (unsigned i = 0; i < CFG_TUD_VIDEO; ++i)
  {
    videod_interface_t* p_video = &_videod_itf[i];

    // TODO
    tu_memclr(p_video, ITF_MEM_RESET_SIZE);
  }
}

uint16_t videod_open(uint8_t rhport, tusb_desc_interface_t const * itf_desc, uint16_t max_len)
{
  TU_VERIFY(TUSB_CLASS_VIDEO           == itf_desc->bInterfaceClass &&
            VIDEO_SUBCLASS_CONTROL     == itf_desc->bInterfaceSubClass &&
            VIDEO_INT_PROTOCOL_CODE_15 == itf_desc->bFunctionProtool, 0);

  /* Find available interface */
  videod_interface_t *p_video = NULL;
  for (unsigned video_id = 0; video_id < CFG_TUD_VIDEO; ++video_id) {
    if (!_videod_itf[video_id].itf_in && !_videod_itf[video_id].itf_out) {
      p_video = &_videod_itf[video_id];
      break;
    }
  }
  TU_ASSERT(p_video, 0);

  /*------------- Video Control Interface -------------*/
  TU_VERIFY(itf_desc->bNumEndpoints < 2, 0); /* support only 1 notification endpoint */
  unsigned itf_ctl = itf_desc->bInterfaceNumber;
  unsigned drv_len = sizeof(*itf_desc);
  tusb_desc_endpoint_t const * desc_ep = NULL;

  uint8_t const *p_desc = tu_desc_next(itf_desc);
  tusb_desc_video_control_interface_t const *ctl = (tusb_desc_video_control_interface_t const*)p_desc;
  TU_VERIFY(TUSB_DESC_CS_INTERFACE       == ctl->bDescriptorType &&
            VIDEO_CS_VC_INTERFACE_HEADER == ctl->bDescriptorSubType, 0);

  if (itf_desc->bNumEndpoints) { /* has a notification endpoint */
    p_desc = tu_desc_next(ctl);
    /* skip to the notification endpoint descriptor */
    while (TUSB_DESC_ENDPOINT != tu_desc_type(p_desc) && drv_len <= max_len ) {
      p_desc   = tu_desc_next(p_desc);
    }
    desc_ep = (tusb_desc_endpoint_t const *)p_desc;
  }
  drv_len += tu_desc_len(ctl)  + ctl->wTotalLength;
  p_desc   = tu_desc_next(ctl) + ctl->wTotalLength;
  TU_VERIFY(drv_len <= max_len, 0);

  /*------------- Video Stream Interface -------------*/
  uint8_t itf_in  = 0, itf_out = 0;
  tusb_desc_video_streaming_interface_input_t  const *stm_in  = NULL;
  tusb_desc_video_streaming_interface_output_t const *stm_out = NULL;

  for (unsigned itf_stm = 0; itf_stm < ctl->bInCollection; ++itf_stm) {
    itf_desc = (tusb_desc_interface_t const *)p_desc;
    TU_VERIFY(TUSB_DESC_INTERFACE        == itf_desc->bDescriptorType &&
              0                          == itf_desc->bInterfaceNumber &&
              TUSB_CLASS_VIDEO           == itf_desc->bInterfaceClass &&
              VIDEO_SUBCLASS_STREAMING   == itf_desc->bInterfaceSubClass &&
              VIDEO_INT_PROTOCOL_CODE_15 == itf_desc->bFunctionProtool &&
              0 == itf_desc->bNumEndpoints, 0);
    drv_len += sizeof(*itf_desc);
    p_desc   = tu_desc_next(itf_desc);
    TU_VERIFY(TUSB_DESC_CS_INTERFACE == tu_desc_type(p_desc));
    tusb_desc_video_streaming_interface_t const *stm = (tusb_desc_video_streaming_interface_t const *)p_desc;
    TU_VERIFY(TUSB_DESC_CS_INTERFACE == stm->bDescriptorType);
    switch (stm->bDescriptorSubType) {
    default: return 0;
    case VIDEO_CS_VS_INTERFACE_INPUT_HEADER:
      TU_VERIFY(0 == itf_in, 0);
      itf_in = itf_desc->bInterfaceNumber;
      stm_in = (tusb_desc_video_streaming_interface_input_t const*)stm;
      break;
    case VIDEO_CS_VS_INTERFACE_OUTPUT_HEADER:
      TU_VERIFY(0 == itf_out, 0);
      itf_out = itf_desc->bInterfaceNumber;
      stm_out = (tusb_desc_video_streaming_interface_output_t const*)stm;
      break;
    }
    drv_len += tu_desc_len(stm)  + stm->wTotalLength;
    p_desc   = tu_desc_next(stm) + ctl->wTotalLength;
  }
  if (desc_ep) {
    /* open the notification endpoint */
    TU_ASSERT( usbd_edpt_open(rhport, desc_ep), 0);
    p_video->ep_notif = desc_ep->bEndpointAddress;
  }
  p_video->itf_ctl = itf_ctl;
  p_video->itf_in  = itf_in;
  p_video->itf_out = itf_out;
  p_video->ctl     = ctl;
  p_video->stm_in  = stm_in;
  p_video->stm_out = stm_out;

  return drv_len;
}

// Invoked when a control transfer occurred on an interface of this class
// Driver response accordingly to the request and the transfer stage (setup/data/ack)
// return false to stall control endpoint (e.g unsupported request)
bool videod_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const * request)
{
  /* Standard request */
  if (p_request->bmRequestType_bit.type == TUSB_REQ_TYPE_STANDARD) {
    tusb_desc_interface_t const *itf;
    unsigned itf;
    switch (p_request->bRequest) {
    case TUSB_REQ_GET_INTERFACE:
      itf = tu_u16_low(p_request->wIndex);
      return audiod_get_interface(rhport, p_request);
    case TUSB_REQ_SET_INTERFACE:
      itf = videod_find_interface_desc(void const *beg, void const *end, unsigned itfnum)
      itf = tu_u16_low(p_request->wIndex);
      
      return audiod_set_interface(rhport, p_request);
      /* Unknown/Unsupported request */
    default: TU_BREAKPOINT(); return false;
    }
  }

  uint8_t itf = 0;
  videod_interface_t* p_video = _videod_itf;

  // Identify which interface to use
  for ( ; ; itf++, p_video++) {
    if (itf >= TU_ARRAY_SIZE(_videod_itf)) return false;
    if ( p_video->itf_num == request->wIndex ) break;
  }

  if (stage == CONTROL_STAGE_SETUP) {
    TU_VERIFY((TUSB_DIR_IN_MASK & request->bmRequestType) ==
	      (TUSB_DIR_IN_MASK & request->bRequest));
  }


  unsigned cs = TU_U16_HIGH(request->wValue);


  unsigned req = request->bRequest;
  switch (request->bRequest) {
  case VIDEO_REQUEST_GET_INFO:
    TU_VERIFY(1 == request->wLength);
    
    break;
  case VIDEO_REQUEST_SET_CUR:
    if (stage == CONTROL_STAGE_SETUP) {
        TU_LOG2("  Set Current Setting Attribute\r\n");
        tud_control_xfer(rhport, request, &p_video->line_coding, sizeof(cdc_line_coding_t));
    } else if ( stage == CONTROL_STAGE_ACK) {
      if ( tud_cdc_line_coding_cb ) tud_cdc_line_coding_cb(itf, &p_video->line_coding);
    }
    break;
  case VIDEO_REQUEST_GET_CUR:
    if (stage == CONTROL_STAGE_SETUP) {
        TU_LOG2("  Set Current Setting Attribute\r\n");
        tud_control_xfer(rhport, request, &p_video->line_coding, sizeof(cdc_line_coding_t));
    } else if ( stage == CONTROL_STAGE_ACK) {
      if ( tud_cdc_line_coding_cb ) tud_cdc_line_coding_cb(itf, &p_video->line_coding);
    }
    break;
  default: return false; // stall unsupported request
  }

  return true;
}

bool videod_xfer_cb(uint8_t rhport, uint8_t ep_addr, xfer_result_t result, uint32_t xferred_bytes)
{
  (void) result;

  uint8_t itf;
  videod_interface_t* p_video;

  // Identify which interface to use
  for (itf = 0; itf < CFG_TUD_CDC; itf++)
  {
    p_video = &_videod_itf[itf];
    if ( ( ep_addr == p_video->ep_out ) || ( ep_addr == p_video->ep_in ) ) break;
  }
  TU_ASSERT(itf < CFG_TUD_CDC);

  // Received new data
  if ( ep_addr == p_video->ep_out )
  {
    tu_fifo_write_n(&p_video->rx_ff, &p_video->epout_buf, xferred_bytes);
    
    // Check for wanted char and invoke callback if needed
    if ( tud_cdc_rx_wanted_cb && (((signed char) p_video->wanted_char) != -1) )
    {
      for ( uint32_t i = 0; i < xferred_bytes; i++ )
      {
        if ( (p_video->wanted_char == p_video->epout_buf[i]) && !tu_fifo_empty(&p_video->rx_ff) )
        {
          tud_cdc_rx_wanted_cb(itf, p_video->wanted_char);
        }
      }
    }
    
    // invoke receive callback (if there is still data)
    if (tud_cdc_rx_cb && !tu_fifo_empty(&p_video->rx_ff) ) tud_cdc_rx_cb(itf);
    
    // prepare for OUT transaction
    _prep_out_transaction(p_video);
  }
  
  // Data sent to host, we continue to fetch from tx fifo to send.
  // Note: This will cause incorrect baudrate set in line coding.
  //       Though maybe the baudrate is not really important !!!
  if ( ep_addr == p_video->ep_in )
  {
    // invoke transmit callback to possibly refill tx fifo
    if ( tud_cdc_tx_complete_cb ) tud_cdc_tx_complete_cb(itf);

    if ( 0 == tud_cdc_n_write_flush(itf) )
    {
      // If there is no data left, a ZLP should be sent if
      // xferred_bytes is multiple of EP Packet size and not zero
      if ( !tu_fifo_count(&p_video->tx_ff) && xferred_bytes && (0 == (xferred_bytes & (BULK_PACKET_SIZE-1))) )
      {
        if ( usbd_edpt_claim(rhport, p_video->ep_in) )
        {
          usbd_edpt_xfer(rhport, p_video->ep_in, NULL, 0);
        }
      }
    }
  }

  // nothing to do with notif endpoint for now

  return true;
}

#endif
