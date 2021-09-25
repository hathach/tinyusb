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

typedef union {
  tusb_desc_cs_video_ctl_itf_hdr_t ctl;
  tusb_desc_cs_video_stm_itf_hdr_t stm;
} tusb_desc_video_itf_hdr_t;

typedef struct TU_ATTR_PACKED {
  uint8_t bLength;
  uint8_t bDescriptorType;
  uint8_t bDescriptorSubtype;
  uint8_t bEntityId;
} tusb_desc_cs_video_entity_itf_t;

typedef struct TU_ATTR_PACKED {
  void const *beg;  /* The head of the first video control interface descriptor */
  uint16_t length;  /* Byte length of the video control interface descriptors */
  uint16_t offset;  /* offset bytes for the current video control interface descripter */
  uint8_t  error_code;  /* error code set by previous transaction */
  uint8_t  power_mode;  /* current power mode */
} videod_control_interface_t;

typedef struct TU_ATTR_PACKED {
  uint8_t index_vc;  /* index of video control interface */
  uint8_t error_code;/* error code for video control/streaming interface. 0:control 1:streaming 2:streaming */
  struct {
    uint16_t beg;    /* Offset of the beggining of video streaming interface descriptor */
    uint16_t end;    /* Offset of the end of video streaming interface descriptor */
    uint16_t cur;    /* Offset of the current settings */
    uint16_t ep[2];  /* Offset of endpoint descriptors */
  } desc;

  uint8_t  *buffer; /* frame buffer. assume linear buffer. no support for stride access */
  uint32_t bufsize; /* frame buffer */
  uint32_t offset;  /* offset for the next payload transfer */
  uint32_t max_payload_transfer_size;
  uint8_t  ep_buf[CFG_TUD_VIDEO_EP_BUFSIZE];
} videod_streaming_interface_t;

typedef struct TU_ATTR_PACKED {
  void const *beg;  /* The head of the first video control interface descriptor */
  uint16_t    len;  /* Byte length of the descriptors */
  uint16_t    cur;  /* offset for current video control interface */
  uint8_t     stm[CFG_TUD_VIDEO_STREAMING]; /* Indices of streaming interface */
  uint8_t error_code;  /* error code for video control/streaming interface. */
  uint8_t power_mode;

  /*------------- From this point, data is not cleared by bus reset -------------*/

  // Endpoint Transfer buffer
  //  CFG_TUSB_MEM_ALIGN uint8_t epout_buf[CFG_TUD_CDC_EP_BUFSIZE];
  //  CFG_TUSB_MEM_ALIGN uint8_t epin_buf[CFG_TUD_CDC_EP_BUFSIZE];
  uint8_t ctl_buf[64];

} videod_interface_t;

#define ITF_MEM_RESET_SIZE   offsetof(videod_interface_t, ctl_buf)

//--------------------------------------------------------------------+
// INTERNAL OBJECT & FUNCTION DECLARATION
//--------------------------------------------------------------------+
CFG_TUSB_MEM_SECTION static videod_interface_t _videod_itf[CFG_TUD_VIDEO];
CFG_TUSB_MEM_SECTION static videod_streaming_interface_t _videod_streaming_itf[CFG_TUD_VIDEO_STREAMING];

static uint8_t const _cap_get     = 0x1u; /* support for GET */
static uint8_t const _cap_get_set = 0x3u; /* support for GET and SET */
static video_probe_and_commit_control_t const def_stm_settings = {
  .bmHint = 0,
  .bFormatIndex = 1,
  .bFrameIndex = 1,
  .dwFrameInterval = (10000000/10),
  .wKeyFrameRate = 1,
  .wPFrameRate = 0,
  .wCompQuality = 1, /* 1 to 10000 */
  .wCompWindowSize = 1, /* Maybe it is match to GOP size */
  .wDelay = 240, /* milliseconds */
  .dwMaxVideoFrameSize = 128 * 96 * 12 / 8,
  .dwMaxPayloadTransferSize = 256, /* Maybe it is the maximum packet size under this settings */
  .dwClockFrequency = 27000000, /* same as MPEG-2 system time clock  */
  .bmFramingInfo = 0,
  .bPreferedVersion = 1,
  .bMinVersion = 1,
  .bMaxVersion = 1,
  .bUsage = 0,
  .bBitDepthLuma = 8,
  .bmSettings = 0,
  .bMaxNumberOfRefFramesPlus1 = 0,
  .bmRateControlModes = 0,
  .bmLayoutPerStream = 0
};

static inline uint8_t _desc_itfnum(void const *desc)
{
  return ((uint8_t const*)desc)[2];
}

static inline uint8_t _desc_ep_addr(void const *desc)
{
  return ((uint8_t const*)desc)[2];
}

static tusb_desc_vc_itf_t const* _get_desc_vc(videod_interface_t const *self)
{
  return (tusb_desc_vc_itf_t const *)(self->beg + self->cur);
}

static tusb_desc_vs_itf_t const *_get_desc_vs(videod_streaming_interface_t const *self)
{
  if (!self->desc.cur) return NULL;
  void const *desc = _videod_itf[self->index_vc].beg;
  return (tusb_desc_vs_itf_t const*)(desc + self->desc.cur);
}

/** Find the first descriptor with the specified descriptor type.
 *
 * @param[in] beg     The head of descriptor byte array.
 * @param[in] end     The tail of descriptor byte array.
 * @param[in] target  The target descriptor type.
 *
 * @return The pointer for interface descriptor.
 * @retval end   did not found interface descriptor */
static void const* _find_desc(void const *beg, void const *end, uint8_t target)
{
  void const *cur = beg;
  while ((cur < end) && (target != tu_desc_type(cur))) {
    cur = tu_desc_next(cur);
  }
  return cur;
}

/** Return the next interface descriptor except alternate ones.
 *
 * @param[in] beg     The head of descriptor byte array.
 * @param[in] end     The tail of descriptor byte array.
 *
 * @return The pointer for interface descriptor.
 * @retval end   did not found interface descriptor */
static void const* _next_desc_itf(void const *beg, void const *end)
{
  void const *cur = beg;
  uint_fast8_t itfnum = ((tusb_desc_interface_t const*)cur)->bInterfaceNumber;
  while ((cur < end) &&
         (itfnum == ((tusb_desc_interface_t const*)cur)->bInterfaceNumber)) {
    cur = _find_desc(tu_desc_next(cur), end, TUSB_DESC_INTERFACE);
  }
  return cur;
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
static void const* _find_desc_itf(void const *beg, void const *end, uint_fast8_t itfnum, uint_fast8_t altnum)
{
  for (void const *cur = beg; cur < end; cur = _find_desc(cur, end, TUSB_DESC_INTERFACE)) {
    tusb_desc_interface_t const *itf = (tusb_desc_interface_t const *)cur;
    if (itf->bInterfaceNumber == itfnum && itf->bAlternateSetting == altnum) {
      return itf;
    }
    cur = tu_desc_next(cur);
  }
  return end;
}

/** Find the first endpoint descriptor in front of the next interface descriptor.
 *
 * @param[in] beg     The head of descriptor byte array.
 * @param[in] end     The tail of descriptor byte array.
 *
 * @return The pointer for endpoint descriptor.
 * @retval end   did not found endpoint descriptor */
static void const* _find_desc_ep(void const *beg, void const *end)
{
  for (void const *cur = beg; cur < end; cur = tu_desc_next(cur)) {
    uint8_t desc_type = tu_desc_type(cur);
    if (TUSB_DESC_ENDPOINT == desc_type) return cur;
    if (TUSB_DESC_INTERFACE == desc_type) break;
  }
  return end;
}

/** Find the first entity descriptor with the specified entity ID in the video control interface descriptor.
 *
 * @param[in] vc        The video control interface descriptor.
 * @param[in] entityid  The target entity id.
 *
 * @return The pointer for interface descriptor.
 * @retval end   did not found interface descriptor */
static void const* _find_desc_entity(tusb_desc_vc_itf_t const *vc, uint_fast8_t entityid)
{
  void const *beg = (void const*)vc;
  void const *end = beg + vc->std.bLength + vc->ctl.wTotalLength;
  for (void const *cur = beg; cur < end; cur = _find_desc(cur, end, TUSB_DESC_CS_INTERFACE)) {
    tusb_desc_cs_video_entity_itf_t const *itf = (tusb_desc_cs_video_entity_itf_t const *)cur;
    if ((VIDEO_CS_VC_INTERFACE_INPUT_TERMINAL  <= itf->bDescriptorSubtype
         && itf->bDescriptorSubtype < VIDEO_CS_VC_INTERFACE_MAX)
        && itf->bEntityId == entityid) {
      return itf;
    }
    cur = tu_desc_next(cur);
  }
  return end;
}

/** Close current video control interface.
 *
 * @param[in,out] self     The context.
 * @param[in]     altnum   The target alternate setting number. */
static bool _close_vc_itf(uint8_t rhport, videod_interface_t *self)
{
  tusb_desc_vc_itf_t const *vc = _get_desc_vc(self);
  /* The next descriptor after the class-specific VC interface header descriptor. */
  void const *cur = (void const*)vc + vc->std.bLength + vc->ctl.bLength;
  /* The end of the video control interface descriptor. */
  void const *end = (void const*)vc + vc->std.bLength + vc->ctl.wTotalLength;
  if (vc->std.bNumEndpoints) {
    /* Find the notification endpoint descriptor. */
    cur = _find_desc(cur, end, TUSB_DESC_ENDPOINT);
    TU_ASSERT(cur < end);
    tusb_desc_endpoint_t const *notif = (tusb_desc_endpoint_t const *)cur;
    usbd_edpt_close(rhport, notif->bEndpointAddress);
  }
  self->cur = 0;
  return true;
}

/** Set the specified alternate setting to own video control interface.
 *
 * @param[in,out] self     The context.
 * @param[in]     altnum   The target alternate setting number. */
static bool _open_vc_itf(uint8_t rhport, videod_interface_t *self, uint_fast8_t altnum)
{
  TU_LOG2("    open VC %d\r\n", altnum);
  void const *beg = self->beg;
  void const *end = beg + self->len;
  /* The first descriptor is a video control interface descriptor. */
  void const *cur = _find_desc_itf(beg, end, _desc_itfnum(beg), altnum);
  TU_LOG2("    cur %ld\r\n", cur - beg);
  TU_VERIFY(cur < end);

  tusb_desc_vc_itf_t const *vc = (tusb_desc_vc_itf_t const *)cur;
  TU_LOG2("    bInCollection %d\r\n", vc->ctl.bInCollection);
  /* Support for up to 2 streaming interfaces only. */
  TU_ASSERT(vc->ctl.bInCollection <= CFG_TUD_VIDEO_STREAMING);

  /* Update to point the end of the video control interface descriptor. */
  end  = cur + vc->std.bLength + vc->ctl.wTotalLength;
  /* Advance to the next descriptor after the class-specific VC interface header descriptor. */
  cur += vc->std.bLength + vc->ctl.bLength;
  TU_LOG2("    bNumEndpoints %d\r\n", vc->std.bNumEndpoints);
  /* Open the notification endpoint if it exist. */
  if (vc->std.bNumEndpoints) {
    /* Support for 1 endpoint only. */
    TU_VERIFY(1 == vc->std.bNumEndpoints);
    /* Find the notification endpoint descriptor. */
    cur = _find_desc(cur, end, TUSB_DESC_ENDPOINT);
    TU_VERIFY(cur < end);
    tusb_desc_endpoint_t const *notif = (tusb_desc_endpoint_t const *)cur;
    /* Open the notification endpoint */
    TU_ASSERT(usbd_edpt_open(rhport, notif));
  }
  self->cur = (void const*)vc - beg;
  return true;
}

/** Set the specified alternate setting to own video control interface.
 *
 * @param[in,out] self     The context.
 * @param[in]     altnum   The target alternate setting number. */
static bool _open_vs_itf(uint8_t rhport, videod_streaming_interface_t *stm, uint_fast8_t altnum)
{
  uint_fast8_t i;
  TU_LOG1("    reopen VS %d\r\n", altnum);
  void const *desc = _videod_itf[stm->index_vc].beg;

  /* Close endpoints of previous settings. */
  for (i = 0; i < TU_ARRAY_SIZE(stm->desc.ep); ++i) {
    uint_fast16_t ofs_ep = stm->desc.ep[i];
    if (!ofs_ep) break;
    uint_fast8_t  ep_adr = _desc_ep_addr(desc + ofs_ep);
    usbd_edpt_close(rhport, ep_adr);
    stm->desc.ep[i] = 0;
    TU_LOG1("    close EP%02x\n", ep_adr);
  }

  /* Find a alternate interface */
  void const *beg = desc + stm->desc.beg;
  void const *end = desc + stm->desc.end;
  void const *cur = _find_desc_itf(beg, end, _desc_itfnum(beg), altnum);
  TU_VERIFY(cur < end);
  uint_fast8_t numeps = ((tusb_desc_interface_t const *)cur)->bNumEndpoints;
  TU_ASSERT(numeps <= TU_ARRAY_SIZE(stm->desc.ep));
  stm->desc.cur = cur - desc; /* Save the offset of the new settings */
  /* Open endpoints of the new settings. */
  for (i = 0, cur = tu_desc_next(cur); i < numeps; ++i, cur = tu_desc_next(cur)) {
    cur = _find_desc_ep(cur, end);
    TU_ASSERT(cur < end);
    TU_ASSERT(usbd_edpt_open(rhport, (tusb_desc_endpoint_t const *)cur));
    stm->desc.ep[i] = cur - desc;
    stm->max_payload_transfer_size = def_stm_settings.dwMaxPayloadTransferSize;
    TU_LOG1("    open EP%02x\n", _desc_ep_addr(cur));
  }
  /* initialize payload header */
  tusb_video_payload_header_t *hdr = (tusb_video_payload_header_t*)stm->ep_buf;
  hdr->bHeaderLength = sizeof(*hdr);
  hdr->bmHeaderInfo  = 0;

  return true;
}

static uint_fast16_t _prepair_in_payload(videod_streaming_interface_t *stm)
{
  uint_fast16_t remaining = stm->bufsize - stm->offset;
  uint_fast16_t hdr_len   = stm->ep_buf[0];
  uint_fast16_t pkt_len   = stm->max_payload_transfer_size;
  if (hdr_len + remaining < pkt_len) {
    pkt_len = hdr_len + remaining;
  }
  uint_fast16_t data_len = pkt_len - hdr_len;
  memcpy(&stm->ep_buf[hdr_len], stm->buffer + stm->offset, data_len);
  stm->offset += data_len;
  remaining -= data_len;
  if (!remaining) {
    tusb_video_payload_header_t *hdr = (tusb_video_payload_header_t*)stm->ep_buf;
    hdr->EndOfFrame = 1;
  }
  return hdr_len + data_len;
}

/** Handle a standard request to the video control interface. */
static int handle_video_ctl_std_req(uint8_t rhport, uint8_t stage, tusb_control_request_t const *request, uint_fast8_t itf)
{
  switch (request->bRequest) {
  case TUSB_REQ_GET_INTERFACE:
    if (stage != CONTROL_STAGE_SETUP) return VIDEO_NO_ERROR;
    TU_VERIFY(1 == request->wLength, VIDEO_UNKNOWN);
    tusb_desc_vc_itf_t const *vc = _get_desc_vc(&_videod_itf[itf]);
    if (!vc) return VIDEO_UNKNOWN;
    if (tud_control_xfer(rhport, request,
                         (void*)&vc->std.bAlternateSetting,
                         sizeof(vc->std.bAlternateSetting)))
      return VIDEO_NO_ERROR;
    return VIDEO_UNKNOWN;
  case TUSB_REQ_SET_INTERFACE:
    if (stage != CONTROL_STAGE_SETUP) return VIDEO_NO_ERROR;
    TU_VERIFY(0 == request->wLength, VIDEO_UNKNOWN);
    if (!_close_vc_itf(rhport, &_videod_itf[itf]))
      return VIDEO_UNKNOWN;
    if (!_open_vc_itf(rhport, &_videod_itf[itf], request->wValue))
      return VIDEO_UNKNOWN;
    tud_control_status(rhport, request);
    return VIDEO_NO_ERROR;
  default: /* Unknown/Unsupported request */
    TU_BREAKPOINT();
    return VIDEO_INVALID_REQUEST;
  }
}

static int handle_video_ctl_cs_req(uint8_t rhport, uint8_t stage, tusb_control_request_t const *request, uint_fast8_t itf)
{
  videod_interface_t *self = &_videod_itf[itf];
  /* 4.2.1 Interface Control Request */
  switch (TU_U16_HIGH(request->wValue)) {
  case VIDEO_VC_CTL_VIDEO_POWER_MODE:
    TU_LOG2("    Power Mode ");
    switch (request->bRequest) {
    case VIDEO_REQUEST_SET_CUR:
      if (stage == CONTROL_STAGE_SETUP) {
        TU_LOG2("Set\r\n");
        TU_VERIFY(1 == request->wLength, VIDEO_UNKNOWN);
        if (!tud_control_xfer(rhport, request, &self->power_mode, sizeof(self->power_mode)))
          return VIDEO_UNKNOWN;
      } else if (stage == CONTROL_STAGE_ACK) {
        if (tud_video_power_mode_cb) return tud_video_power_mode_cb(itf, self->power_mode);
      }
      return VIDEO_NO_ERROR;
    case VIDEO_REQUEST_GET_CUR:
      if (stage != CONTROL_STAGE_SETUP) return VIDEO_NO_ERROR;
      TU_LOG2("Get\r\n");
      TU_VERIFY(1 == request->wLength, VIDEO_UNKNOWN);
      if (!tud_control_xfer(rhport, request, &self->power_mode, sizeof(self->power_mode)))
        return VIDEO_UNKNOWN;
      return VIDEO_NO_ERROR;
    case VIDEO_REQUEST_GET_INFO:
      if (stage != CONTROL_STAGE_SETUP) return VIDEO_NO_ERROR;
      TU_LOG2("GetInfo\r\n");
      TU_VERIFY(1 == request->wLength, VIDEO_UNKNOWN);
      if (!tud_control_xfer(rhport, request, (uint8_t*)&_cap_get_set, sizeof(_cap_get_set)))
        return VIDEO_UNKNOWN;
      return VIDEO_NO_ERROR;
    default: break;
    }
    break;
  case VIDEO_VC_CTL_REQUEST_ERROR_CODE:
    TU_LOG2("  Error Code");
    switch (request->bRequest) {
    case VIDEO_REQUEST_GET_CUR:
      if (stage != CONTROL_STAGE_SETUP) return VIDEO_NO_ERROR;
      TU_LOG2(" Get\r\n");
      if (!tud_control_xfer(rhport, request, &self->error_code, sizeof(uint8_t)))
        return VIDEO_UNKNOWN;
      return VIDEO_NO_ERROR;
    case VIDEO_REQUEST_GET_INFO:
      if (stage != CONTROL_STAGE_SETUP) return VIDEO_NO_ERROR;
      TU_LOG2(" GetInfo\r\n");
      if (tud_control_xfer(rhport, request, (uint8_t*)&_cap_get, sizeof(_cap_get)))
        return VIDEO_UNKNOWN;
      return VIDEO_NO_ERROR;
    default: break;
    }
    break;
  default: break;
  }
  /* Unknown/Unsupported request */
  TU_BREAKPOINT();
  return VIDEO_INVALID_REQUEST;
}

static int handle_video_ctl_req(uint8_t rhport, uint8_t stage, tusb_control_request_t const *request, uint_fast8_t itf)
{
  uint_fast8_t entity_id;
  switch (request->bmRequestType_bit.type) {
  case TUSB_REQ_TYPE_STANDARD:
    return handle_video_ctl_std_req(rhport, stage, request, itf);
  case TUSB_REQ_TYPE_CLASS:
    entity_id = TU_U16_HIGH(request->wIndex);
    if (!entity_id) {
      return handle_video_ctl_cs_req(rhport, stage, request, itf);
    } else {
      if (!_find_desc_entity(_get_desc_vc(&_videod_itf[itf]), entity_id))
        return VIDEO_INVALID_REQUEST;
      return VIDEO_INVALID_REQUEST;
    }
  default:
    return VIDEO_INVALID_REQUEST;
  }
}

static int handle_video_stm_std_req(uint8_t rhport, uint8_t stage, tusb_control_request_t const *request, uint_fast8_t itf)
{
  videod_streaming_interface_t *self = &_videod_streaming_itf[itf];
  switch (request->bRequest) {
  case TUSB_REQ_GET_INTERFACE:
    if (stage != CONTROL_STAGE_SETUP) return VIDEO_NO_ERROR;
    TU_VERIFY(1 == request->wLength, VIDEO_UNKNOWN);
    tusb_desc_vs_itf_t const *vs = _get_desc_vs(self);
    if (!vs) return VIDEO_UNKNOWN;
    if (tud_control_xfer(rhport, request,
                         (void*)&vs->std.bAlternateSetting,
                         sizeof(vs->std.bAlternateSetting)))
      return VIDEO_NO_ERROR;
    return VIDEO_UNKNOWN;
  case TUSB_REQ_SET_INTERFACE:
    if (stage != CONTROL_STAGE_SETUP) return VIDEO_NO_ERROR;
    if (!_open_vs_itf(rhport, self, request->wValue))
      return VIDEO_UNKNOWN;
    tud_control_status(rhport, request);
    return VIDEO_NO_ERROR;
  default: /* Unknown/Unsupported request */
    TU_BREAKPOINT();
    return VIDEO_INVALID_REQUEST;
  }
}

static int handle_video_stm_cs_req(uint8_t rhport, uint8_t stage, tusb_control_request_t const *request, uint_fast8_t itf)
{
  (void)rhport;
  videod_streaming_interface_t *self = &_videod_streaming_itf[itf];
  /* 4.2.1 Interface Control Request */
  switch (TU_U16_HIGH(request->wValue)) {
  case VIDEO_VS_CTL_STREAM_ERROR_CODE:
    TU_LOG2("    Error Code ");
    switch (request->bRequest) {
    case VIDEO_REQUEST_GET_CUR:
      if (stage != CONTROL_STAGE_SETUP) return VIDEO_NO_ERROR;
      /* TODO */
      if (!tud_control_xfer(rhport, request, &self->error_code, sizeof(uint8_t)))
        return VIDEO_UNKNOWN;
      return VIDEO_NO_ERROR;
    case VIDEO_REQUEST_GET_INFO:
      if (stage != CONTROL_STAGE_SETUP) return VIDEO_NO_ERROR;
      TU_LOG2("GetInfo\r\n");
      if (tud_control_xfer(rhport, request, (uint8_t*)&_cap_get, sizeof(_cap_get)))
        return VIDEO_UNKNOWN;
      return VIDEO_NO_ERROR;
    default: break;
    }
    break;
  case VIDEO_VS_CTL_PROBE:
    switch (request->bRequest) {
    case VIDEO_REQUEST_SET_CUR:
      if (stage == CONTROL_STAGE_SETUP) {
        TU_VERIFY(sizeof(video_probe_and_commit_control_t) == request->wLength, VIDEO_UNKNOWN);
        if (!tud_control_xfer(rhport, request, self->ep_buf, sizeof(video_probe_and_commit_control_t)))
          return VIDEO_UNKNOWN;
      } else if (stage == CONTROL_STAGE_ACK) {
        if (tud_video_probe_set_cb)
          return tud_video_probe_set_cb(itf, (video_probe_and_commit_control_t const*)self->ep_buf);
      }
      return VIDEO_NO_ERROR;
    case VIDEO_REQUEST_GET_CUR:
      if (stage != CONTROL_STAGE_SETUP) return VIDEO_NO_ERROR;
      TU_VERIFY(request->wLength, VIDEO_UNKNOWN);
      if (tud_control_xfer(rhport, request, (void*)&def_stm_settings,
                           sizeof(video_probe_and_commit_control_t)))
        return VIDEO_NO_ERROR;
      return VIDEO_UNKNOWN;
    case VIDEO_REQUEST_GET_MIN:
      if (stage != CONTROL_STAGE_SETUP) return VIDEO_NO_ERROR;
      TU_VERIFY(request->wLength, VIDEO_UNKNOWN);
      if (tud_control_xfer(rhport, request, (void*)&def_stm_settings,
                           sizeof(video_probe_and_commit_control_t)))
        return VIDEO_NO_ERROR;
      return VIDEO_UNKNOWN;
    case VIDEO_REQUEST_GET_MAX:
      if (stage != CONTROL_STAGE_SETUP) return VIDEO_NO_ERROR;
      TU_VERIFY(request->wLength, VIDEO_UNKNOWN);
      if (tud_control_xfer(rhport, request, (void*)&def_stm_settings,
                           sizeof(video_probe_and_commit_control_t)))
        return VIDEO_NO_ERROR;
      return VIDEO_UNKNOWN;
    case VIDEO_REQUEST_GET_RES:
      return VIDEO_UNKNOWN;
    case VIDEO_REQUEST_GET_DEF:
      return VIDEO_UNKNOWN;
    case VIDEO_REQUEST_GET_LEN:
      return VIDEO_UNKNOWN;
    case VIDEO_REQUEST_GET_INFO:
      if (stage != CONTROL_STAGE_SETUP) return VIDEO_NO_ERROR;
      TU_VERIFY(1 == request->wLength, VIDEO_UNKNOWN);
      if (tud_control_xfer(rhport, request, (uint8_t*)&_cap_get_set, sizeof(_cap_get_set)))
        return VIDEO_NO_ERROR;
      return VIDEO_UNKNOWN;
    default: break;
    }
    break;
  case VIDEO_VS_CTL_COMMIT:
    switch (request->bRequest) {
    case VIDEO_REQUEST_SET_CUR:
      if (stage == CONTROL_STAGE_SETUP) {
        TU_VERIFY(sizeof(video_probe_and_commit_control_t) == request->wLength, VIDEO_UNKNOWN);
        if (!tud_control_xfer(rhport, request, self->ep_buf, sizeof(video_probe_and_commit_control_t)))
          return VIDEO_UNKNOWN;
      } else if (stage == CONTROL_STAGE_ACK) {
        if (tud_video_commit_set_cb)
          return tud_video_commit_set_cb(itf, (video_probe_and_commit_control_t const*)self->ep_buf);
      }
      return VIDEO_NO_ERROR;
    case VIDEO_REQUEST_GET_CUR:
      if (stage != CONTROL_STAGE_SETUP) return VIDEO_NO_ERROR;
      TU_VERIFY(request->wLength, VIDEO_UNKNOWN);
      if (tud_control_xfer(rhport, request, (void*)&def_stm_settings,
                           sizeof(video_probe_and_commit_control_t)))
        return VIDEO_NO_ERROR;
      return VIDEO_UNKNOWN;
    case VIDEO_REQUEST_GET_INFO:
      if (stage != CONTROL_STAGE_SETUP) return VIDEO_NO_ERROR;
      TU_VERIFY(1 == request->wLength, VIDEO_UNKNOWN);
      if (tud_control_xfer(rhport, request, (uint8_t*)&_cap_get_set, sizeof(_cap_get_set)))
        return VIDEO_NO_ERROR;
      return VIDEO_UNKNOWN;
    default: break;
    }
    break;
  case VIDEO_VS_CTL_STILL_PROBE:
  case VIDEO_VS_CTL_STILL_COMMIT:
  case VIDEO_VS_CTL_STILL_IMAGE_TRIGGER:
  case VIDEO_VS_CTL_GENERATE_KEY_FRAME:
  case VIDEO_VS_CTL_UPDATE_FRAME_SEGMENT:
  case VIDEO_VS_CTL_SYNCH_DELAY_CONTROL:
    /* TODO */
    break;
  default: break;
  }
  /* Unknown/Unsupported request */
  TU_BREAKPOINT();
  return VIDEO_INVALID_REQUEST;
}

static int handle_video_stm_req(uint8_t rhport, uint8_t stage, tusb_control_request_t const *request, uint_fast8_t itf)
{
  switch (request->bmRequestType_bit.type) {
  case TUSB_REQ_TYPE_STANDARD:
    return handle_video_stm_std_req(rhport, stage, request, itf);
  case TUSB_REQ_TYPE_CLASS:
    if (TU_U16_HIGH(request->wIndex))
      return VIDEO_INVALID_REQUEST;
    return handle_video_stm_cs_req(rhport, stage, request, itf);
  default:
    return VIDEO_INVALID_REQUEST;
  }
  return VIDEO_UNKNOWN;
}

//--------------------------------------------------------------------+
// APPLICATION API
//--------------------------------------------------------------------+

/* itf     control interface number */
bool tud_video_n_connected(uint8_t itf)
{
  (void)itf;
  // DTR (bit 0) active  is considered as connected
  return tud_ready();
}

/* itf     streaming interface number */
bool tud_video_n_streaming(uint8_t itf)
{
  videod_streaming_interface_t *stm = &_videod_streaming_itf[itf];
  if (stm->desc.ep[0])
    return true;
  return false;
}

bool tud_video_n_frame_xfer(uint8_t itf, uint32_t pts, void *buffer, size_t bufsize)
{
  (void)pts;

  if (!buffer || !buffer)
    return false;
  if (!tud_video_n_streaming(itf))
    return false;
  videod_streaming_interface_t *stm = &_videod_streaming_itf[itf];
  if (stm->buffer)
    return false;

  /* find EP address */
  void const *desc = _videod_itf[stm->index_vc].beg;
  uint_fast8_t ep_addr = 0;
  for (uint_fast8_t i = 0; i < CFG_TUD_VIDEO_STREAMING; ++i) {
    uint_fast16_t ofs_ep = stm->desc.ep[i];
    if (!ofs_ep) continue;
    ep_addr = _desc_ep_addr(desc + ofs_ep);
    break;
  }
  if (!ep_addr)
    return false;

  TU_VERIFY( usbd_edpt_claim(0, ep_addr));
  /* update the packet header */
  tusb_video_payload_header_t *hdr = (tusb_video_payload_header_t*)stm->ep_buf;
  hdr->FrameID   ^= 1;
  hdr->EndOfFrame = 0;
  /* update the packet data */
  stm->buffer     = (uint8_t*)buffer;
  stm->bufsize    = bufsize;
  uint_fast16_t pkt_len = _prepair_in_payload(stm);
  TU_ASSERT( usbd_edpt_xfer(0, ep_addr, stm->ep_buf, pkt_len), 0);
  return true;
}

//--------------------------------------------------------------------+
// USBD Driver API
//--------------------------------------------------------------------+
void videod_init(void)
{
  tu_memclr(_videod_itf, sizeof(_videod_itf));

  for (unsigned i = 0; i < CFG_TUD_VIDEO; ++i)
  {
    // videod_interface_t* p_video = &_videod_itf[i];

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
  TU_VERIFY((TUSB_CLASS_VIDEO           == itf_desc->bInterfaceClass) &&
            (VIDEO_SUBCLASS_CONTROL     == itf_desc->bInterfaceSubClass) &&
            (VIDEO_INT_PROTOCOL_CODE_15 == itf_desc->bInterfaceProtocol), 0);

  /* Find available interface */
  videod_interface_t *self = NULL;
  uint_fast8_t itf;
  for (itf = 0; itf < CFG_TUD_VIDEO; ++itf) {
    if (_videod_itf[itf].beg)
      continue;
    self = &_videod_itf[itf];
    break;
  }
  TU_ASSERT(itf < CFG_TUD_VIDEO, 0);

  void const *end = (void const*)itf_desc + max_len;
  self->beg = itf_desc;
  self->len = max_len;
  /*------------- Video Control Interface -------------*/
  if (!_open_vc_itf(rhport, self, 0))
    return 0;
  tusb_desc_vc_itf_t const *vc = _get_desc_vc(self);
  uint_fast8_t bInCollection       = vc->ctl.bInCollection;
  /* Find the end of the video interface descriptor */
  void const *cur = _next_desc_itf(itf_desc, end);
  for (uint_fast8_t i = 0; i < bInCollection; ++i) {
    videod_streaming_interface_t *stm = NULL;
    /* find free streaming interface handle */
    for (uint_fast8_t j = 0; j < TU_ARRAY_SIZE(_videod_streaming_itf); ++j) {
      if (_videod_streaming_itf[i].desc.beg)
        continue;
      stm = &_videod_streaming_itf[i];
      self->stm[i] = j;
      break;
    }
    TU_ASSERT(stm, 0);
    stm->index_vc = itf;
    stm->desc.beg = (uintptr_t)cur - (uintptr_t)itf_desc;
    cur = _next_desc_itf(cur, end);
    stm->desc.end = (uintptr_t)cur - (uintptr_t)itf_desc;
  }
  self->len = (uintptr_t)cur - (uintptr_t)itf_desc;
  return (uintptr_t)cur - (uintptr_t)itf_desc;
}

// Invoked when a control transfer occurred on an interface of this class
// Driver response accordingly to the request and the transfer stage (setup/data/ack)
// return false to stall control endpoint (e.g unsupported request)
bool videod_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const * request)
{
  int err;
  if (request->bmRequestType_bit.recipient != TUSB_REQ_RCPT_INTERFACE) {
    return false;
  }
  uint_fast8_t itfnum = tu_u16_low(request->wIndex);

  /* Identify which control interface to use */
  uint_fast8_t itf;
  for (itf = 0; itf < CFG_TUD_VIDEO; ++itf) {
    void const *desc = _videod_itf[itf].beg;
    if (!desc) continue;
    if (itfnum == _desc_itfnum(desc))
      break;
  }
  if (itf < CFG_TUD_VIDEO) {
    err = handle_video_ctl_req(rhport, stage, request, itf);
    _videod_itf[itf].error_code = (uint8_t)err;
    if (err) return false;
    return true;
  }

  /* Identify which streaming interface to use */
  for (itf = 0; itf < CFG_TUD_VIDEO_STREAMING; ++itf) {
    videod_streaming_interface_t *stm = &_videod_streaming_itf[itf];
    if (!stm->desc.beg) continue;
    void const *desc = _videod_itf[stm->index_vc].beg;
    if (itfnum == _desc_itfnum(desc + stm->desc.beg))
      break;
  }
  if (itf < CFG_TUD_VIDEO_STREAMING) {
    err = handle_video_stm_req(rhport, stage, request, itf);
    _videod_streaming_itf[itf].error_code = (uint8_t)err;
    if (err) return false;
    return true;
  }
  return false;
}

bool videod_xfer_cb(uint8_t rhport, uint8_t ep_addr, xfer_result_t result, uint32_t xferred_bytes)
{
  (void)result; (void)xferred_bytes;

  /* find streaming handle */
  uint_fast8_t itf;
  videod_streaming_interface_t *stm;
  for (itf = 0; itf < CFG_TUD_VIDEO_STREAMING; ++itf) {
    stm = &_videod_streaming_itf[itf];
    uint_fast16_t const ep_ofs = stm->desc.ep[0];
    if (!ep_ofs) continue;
    void const *desc = _videod_itf[stm->index_vc].beg;
    if (ep_addr == _desc_ep_addr(desc + ep_ofs))
      break;
  }
  TU_ASSERT(itf < CFG_TUD_VIDEO_STREAMING);
  if (stm->offset < stm->bufsize) {
    /* Claim the endpoint */
    TU_VERIFY( usbd_edpt_claim(rhport, ep_addr), 0);
    uint_fast16_t pkt_len = _prepair_in_payload(stm);
    TU_ASSERT( usbd_edpt_xfer(rhport, ep_addr, stm->ep_buf, pkt_len), 0);
  } else {
    stm->buffer  = NULL;
    stm->bufsize = 0;
    stm->offset  = 0;
    if (tud_video_frame_xfer_complete_cb)
      tud_video_frame_xfer_complete_cb(itf);
  }
  return true;
}

#endif
