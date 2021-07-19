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
typedef struct {
  uint8_t num;
  uint8_t alt;
} itf_setting_t;

typedef struct {
  tusb_desc_interface_t            std;
  tusb_desc_cs_video_ctl_itf_hdr_t ctl;
} tusb_desc_vc_itf_t;

typedef struct {
  tusb_desc_interface_t            std;
  tusb_desc_cs_video_stm_itf_hdr_t stm;
} tusb_desc_vs_itf_t;

typedef struct TU_ATTR_PACKED {
  uint8_t bLength;
  uint8_t bDescriptorType;
  uint8_t bDescriptorSubtype;
  union {
    uint8_t bId;
    uint8_t bTerminalId;
    uint8_t bUnitId;
  };
} tusb_desc_cs_video_entity_itf_t;

typedef struct
{
  void const *beg;
  void const *end;
  tusb_desc_vc_itf_t const *vc;    /* current video control interface */
  tusb_desc_vs_itf_t const *vs[2]; /* current video streaming interfaces */
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

/** Find the first descriptor with the specified descriptor type.
 *
 * @param[in] beg     The head of descriptor byte array.
 * @param[in] end     The tail of descriptor byte array.
 * @param[in] target  The target descriptor type.
 *
 * @return The pointer for interface descriptor.
 * @retval end   did not found interface descriptor */
static void const* videod_find_desc(void const *beg, void const *end, uint8_t target)
{
  for (void const *cur = beg; cur < end; cur = tu_desc_next(cur)) {
    if (target != tu_desc_type(cur))
      return (uint8_t const*)cur;
  }
  return end;
}

/** Find the first interface descriptor with the specified interface number and alternate setting number.
 *
 * @param[in] beg     The head of descriptor byte array.
 * @param[in] end     The tail of descriptor byte array.
 * @param[in] itfnum  The target interface number.
 * @param[in] altnum  The target alternate setting number.
 *
 * @return The pointer for interface descriptor.
 * @retval end   did not found interface descriptor */
static void const* videod_find_desc_itf(void const *beg, void const *end, unsigned itfnum, unsigned altnum)
{
  for (void const *cur = beg; cur < end; cur = videod_find_desc(cur, end, TUSB_DESC_INTERFACE)) {
    tusb_desc_interface_t const *itf = (tusb_desc_interface_t const *)cur;
    if (itf->bInterfaceNumber == itfnum && itf->bAlternateSettings == altnum) {
      return itf;
    }
    cur = tu_desc_next(cur);
  }
  return end;
}

/** Find the first input or output terminal descriptor with the specified terminal id.
 *
 * @param[in] beg     The head of descriptor byte array.
 * @param[in] end     The tail of descriptor byte array.
 * @param[in] termid  The target terminal id.
 *
 * @return The pointer for interface descriptor.
 * @retval end   did not found interface descriptor */
static void const* videod_find_desc_term(void const *beg, void const *end, unsigned termid)
{
  for (void const *cur = beg; cur < end; cur = videod_find_desc(cur, end, TUSB_DESC_CS_INTERFACE)) {
    tusb_desc_cs_video_entity_itf_t const *itf = (tusb_desc_cs_video_entity_itf_t const *)cur;
    if ((VIDEO_CS_VC_INTERFACE_INPUT_TERMINAL  == itf->bDescriptorSubtype ||
         VIDEO_CS_VC_INTERFACE_OUTPUT_TERMINAL == itf->bDescriptorSubtype) &&
        itf->bTerminalId == termid) {
      return itf;
    }
    cur = tu_desc_next(cur);
  }
  return end;
}

/** Find the first selector/processing/extension/encoding unit descriptor with the specified unit id.
 *
 * @param[in] beg     The head of descriptor byte array.
 * @param[in] end     The tail of descriptor byte array.
 * @param[in] unitid  The target unit id.
 *
 * @return The pointer for interface descriptor.
 * @retval end   did not found interface descriptor */
static void const* videod_find_desc_unit(void const *beg, void const *end, unsigned unitid)
{
  for (void const *cur = beg; cur < end; cur = videod_find_desc(cur, end, TUSB_DESC_CS_INTERFACE)) {
    tusb_desc_cs_video_entity_itf_t const *itf = (tusb_desc_cs_video_entity_itf_t const *)cur;
    if (VIDEO_CS_VC_INTERFACE_SELECTOR_UNIT <= itf->bDescriptorSubtype &&
        itf->bDescriptorSubtype <= VIDEO_CS_VC_INTERFACE_ENCODING_UNIT &&
        itf->bUnitId == unitid) {
      return itf;
    }
    cur = tu_desc_next(cur);
  }
  return end;
}

/** Set the specified alternate setting to own video control interface.
 *
 * @param[in,out] self     The context.
 * @param[in]     altnum   The target alternate setting number.
 *
 * @return The next descriptor after the video control interface descriptor.
 * @retval NULL   did not found interface descriptor or alternate setting */
static void const* videod_set_vc_itf(videod_interface_t *self, unsigned altnum)
{
  void const *beg = self->beg;
  void const *end = self->end;
  /* The head descriptor is a video control interface descriptor. */
  unsigned itfnum = ((tusb_desc_interface_t const *)beg)->bInterfaceNumber;
  void const *cur = videod_find_desc_itf(beg, end, itfnum, altnum);
  TU_VERIFY(cur < end, NULL);

  tusb_desc_vc_itf_t const *vc = (tusb_desc_vc_itf_t const *)cur;
  /* Support for up to 2 streaming interfaces only. */
  TU_VERIFY(vc->ctl.bInCollection < 3, NULL);

  /* Close the previous notification endpoint if it is opened */
  if (self->ep_notif) {
    usbd_edpt_close(rhport, self->ep_notif);
    self->ep_notif = 0;
  }
  /* Advance to the next descriptor after the class-specific VC interface header descriptor. */
  cur += vc->std.bLength + vc->ctl.bLength;
  /* Update to point the end of the video control interface descriptor. */
  end  = cur + vc->ctl.wTotalLength;
  /* Open the notification endpoint if it exist. */
  if (vc->std.bNumEndpoints) {
    /* Support for 1 endpoint only. */
    TU_VERIFY(1 == vc->std.bNumEndpoints, NULL);
    /* Find the notification endpoint descriptor. */
    cur = videod_find_desc(cur, end, TUSB_DESC_ENDPOINT);
    TU_VERIFY(cur < end, NULL);
    tusb_desc_endpoint_t const *notif = (tusb_desc_endpoint_t const *)cur;
    /* Open the notification endpoint */
    TU_ASSERT(usbd_edpt_open(rhport, notif), NULL);
    self->ep_notif = notif->bEndpointAddress;
  }
  self->vc = vc;
  return end;
}

/** Set the specified alternate setting to own video control interface.
 *
 * @param[in,out] self     The context.
 * @param[in]     itfnum   The target interface number.
 * @param[in]     altnum   The target alternate setting number.
 *
 * @return The next descriptor after the video control interface descriptor.
 * @retval NULL   did not found interface descriptor or alternate setting */
static void const* videod_set_vs_itf(videod_interface_t *self, unsigned itfnum, unsigned altnum)
{
  unsigned i;
  tusb_desc_vc_itf_t const *vc = self->vc;
  void const *end = self->end;
  /* Set the end of the video control interface descriptor. */
  void const *cur = (void const*)vc + vc->std.bLength + vc->ctl.bLength + vc->ctl.wTotalLength;

  /* Check itfnum is valid */
  unsigned bInCollection = self->vc->ctl.bInCollection;
  for (i = 0; (i < bInCollection) && (vc->ctl.baInterfaceNr[i] != itfnum); ++i) ;
  TU_VERIFY(i < bInCollection, NULL);
  
  cur = videod_find_desc_itf(cur, end, itfnum, altnum);
  TU_VERIFY(cur < end, NULL);
  tusb_desc_vs_itf_t const *vs = (tusb_desc_vs_itf_t const*)cur;
  /* Advance to the next descriptor after the class-specific VS interface header descriptor. */
  cur += vs->std.bLength + vs->stm.bLength;
  /* Update to point the end of the video control interface descriptor. */
  end  = cur + vs->stm.wTotalLength;

  switch (vs->stm.bDescriptorSubType) {
  default: return end;
  case VIDEO_CS_VS_INTERFACE_INPUT_HEADER:
    /* Support for up to 2 endpoint only. */
    TU_VERIFY(vc->std.bNumEndpoints < 3, NULL);
    if (self->ep_sti) {
      usbd_edpt_close(rhport, self->ep_sti);
      self->ep_sti = 0;
    }
    if (self->ep_in) {
      usbd_edpt_close(rhport, self->ep_in);
      self->ep_in  = 0;
    }
    if (i = 0; i < vs->std.bNumEndpoints; ++i) {
      cur = videod_find_desc(cur, end, TUSB_DESC_ENDPOINT);
      TU_VERIFY(cur < end, NULL);
      tusb_desc_endpoint_t const *ep = (tusb_desc_endpoint_t const *)cur;
      if (vs->stm.bEndpointAddress == ep->bEndpointAddress) {
        /* video input endpoint */
        TU_ASSERT(!self->ep_in, NULL);
        TU_ASSERT(usbd_edpt_open(rhport, ep), NULL);
        self->ep_in  = ep->bEndpointAddress;
      } else {
        /* still image input endpoint */
        TU_ASSERT(!self->ep_sti, NULL);
        TU_ASSERT(usbd_edpt_open(rhport, ep), NULL);
        self->ep_sti = ep->bEndpointAddress;
      }
      cur += tu_desc_len(cur);
    }
    break;
  case VIDEO_CS_VS_INTERFACE_OUTPUT_HEADER:
    /* Support for up to 1 endpoint only. */
    TU_VERIFY(vc->std.bNumEndpoints < 2, NULL);
    if (self->ep_out) {
      usbd_edpt_close(rhport, self->ep_out);
      self->ep_out = 0;
    }
    if (vs->std.bNumEndpoints) {
      cur = videod_find_desc(cur, end, TUSB_DESC_ENDPOINT);
      TU_VERIFY(cur < end, NULL);
      tusb_desc_endpoint_t const *ep = (tusb_desc_endpoint_t const *)cur;
      if (vs->stm.bEndpointAddress == ep->bEndpointAddress) {
        /* video output endpoint */
        TU_ASSERT(usbd_edpt_open(rhport, ep), NULL);
        self->ep_out = ep->bEndpointAddress;
      }
    }
    break;
  }
  for (i = 0; i < sizeof(self->vs)/sizeof(self->vs[0]); ++i) {
    if (!self->vs[i] || self->vs[i].stm.bInterfaceNumber == vs->stm.bInterfaceNumber) {
      self->vs[i] = vs;
      return end;
    }
  }
  return NULL;
}

static bool videod_get_itf(uint8_t rhport, videod_interface_t *self, tusb_control_request_t const * request)
{
  unsigned altnum = tu_u16_low(p_request->wValue);
  unsigned itfnum = tu_u16_low(p_request->wLength);

  tusb_desc_vc_itf_t const *vc = self->vc;
  if (vc->std.bInterfaceNumber == itfnum) {
    tud_control_xfer(rhport, request, &vc->std.bAlternateSettings, sizeof(vc->std.bAlternateSettings));
    return true;
  }
  for (unsigned i = 0; i < vc->ctl.bInCollection; ++i) {
    tusb_desc_vs_itf_t const *vs = self->vs[i];
    if (!vs || vs->std.bInterfaceNumber == itfnum) {
      continue;
    }
    tud_control_xfer(rhport, request, &vs->std.bAlternateSettings, sizeof(vs->std.bAlternateSettings));
    return true;
  }
  return false;
}

static bool videod_set_itf(uint8_t rhport, videod_interface_t *self, tusb_control_request_t const * request)
{
  (void)rhport;
  unsigned altnum = tu_u16_low(p_request->wValue);
  unsigned itfnum = tu_u16_low(p_request->wLength);

  tusb_desc_vc_itf_t const *vc = self->vc;
  if (vc->std.bInterfaceNumber == itfnum) {
    if (videod_set_vc_itf(self, altnum))
      return true;
    return false;
  }
  for (unsigned i = 0; i < vc->ctl.bInCollection; ++i) {
    tusb_desc_vs_itf_t const *vs = self->vs[i];
    if (!vs || vs->std.bInterfaceNumber == itfnum) {
      continue;
    }
    if (videod_set_vs_itf(self, itfnum, altnum))
      return true;
    return false;
  }
  return false;
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
  videod_interface_t *self = NULL;
  for (unsigned i = 0; i < CFG_TUD_VIDEO; ++i) {
    if (!_videod_itf[i].vc) {
      self = &_videod_itf[i];
      break;
    }
  }
  TU_ASSERT(self, 0);

  void const *end = (void const*)itf_desc + max_len;
  self->beg = itf_desc;
  self->end = end;
  /*------------- Video Control Interface -------------*/
  void const* cur = videod_set_vc_itf(self, 0);
  TU_VERIFY(cur, 0);
  unsigned bInCollection = self->vc->ctl.bInCollection;
  /*------------- Video Stream Interface -------------*/
  unsigned itfnum = 0;
  for (unsigned i = 0; i < bInCollection; ++i) {
    itfnum = vc->ctl.baInterfaceNr[i];
    cur = videod_set_vs_itf(self, itfnum, 0);
    TU_VERIFY(cur, 0);
  }

  /* Skip alternate setting interfaces */
  while (cur < end && TUSB_DESC_INTERFACE == tu_desc_type(cur)) {
    tusb_desc_vs_itf_t const *vs = (tusb_desc_vs_itf_t const *)cur;
    if (itfnum                              != vs->std.bInterfaceNumber ||
        TUSB_DESC_CS_INTERFACE              != vs->stm.bDescriptorType  ||
        (VIDEO_CS_VS_INTERFACE_INPUT_HEADER != vs->stm.bDescriptorSubType &&
         VIDEO_CS_VS_INTERFACE_OUTPUT_HEADER!= vs->stm.bDescriptorSubType)) {
      break;
    }
    cur += itf->std.bLength + itf->stm.bLength + itf->stm.wTotalLength;
  }
  self->end = cur;
  return end - cur;
}

// Invoked when a control transfer occurred on an interface of this class
// Driver response accordingly to the request and the transfer stage (setup/data/ack)
// return false to stall control endpoint (e.g unsupported request)
bool videod_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const * request)
{
  if (p_request->bmRequestType_bit.recipient != TUSB_REQ_RCPT_INTERFACE) {
    return false;
  }
  unsigned itfnum = tu_u16_low(p_request->wIndex);
  /* Identify which interface to use */
  videod_interface_t *self = NULL;
  for (unsigned i = 0; i < CFG_TUD_VIDEO; ++i) {
    if (_videod_itf[i].vc->bInterfaceNumber == itfnum) {
      self = &_videod_itf[i];
      break;
    }
  }
  if (!self) return false;

  /* Standard request */
  if (p_request->bmRequestType_bit.type == TUSB_REQ_TYPE_STANDARD) {
    if (stage != CONTROL_STAGE_SETUP) return true;
    switch (p_request->bRequest) {
    case TUSB_REQ_GET_INTERFACE:
      return videod_get_itf(rhport, self, request);
    case TUSB_REQ_SET_INTERFACE:
      return videod_set_itf(rhport, self, request);
    default: /* Unknown/Unsupported request */
      TU_BREAKPOINT();
      return false;
    }
  }

  unsigned cs  = TU_U16_HIGH(request->wValue);
  unsigned uid = TU_U16_HIGH(request->wIndex);

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
