/* 
 * The MIT License (MIT)
 *
 * Copyright (c) 2021 Koji KITAYAMA
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

#if (CFG_TUD_ENABLED && CFG_TUD_VIDEO && CFG_TUD_VIDEO_STREAMING)

#include "device/usbd.h"
#include "device/usbd_pvt.h"

#include "video_device.h"

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF
//--------------------------------------------------------------------+
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

typedef union {
  struct TU_ATTR_PACKED {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bDescriptorSubType;
    uint8_t bFormatIndex;
    uint8_t bNumFrameDescriptors;
  };
  tusb_desc_cs_video_fmt_uncompressed_t uncompressed;
  tusb_desc_cs_video_fmt_mjpeg_t        mjpeg;
  tusb_desc_cs_video_fmt_frame_based_t  frame_based;
} tusb_desc_cs_video_fmt_t;

typedef union {
  struct TU_ATTR_PACKED {
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint8_t  bDescriptorSubType;
    uint8_t  bFrameIndex;
    uint8_t  bmCapabilities;
    uint16_t wWidth;
    uint16_t wHeight;
  };
  tusb_desc_cs_video_frm_uncompressed_t uncompressed;
  tusb_desc_cs_video_frm_mjpeg_t        mjpeg;
  tusb_desc_cs_video_frm_frame_based_t  frame_based;
} tusb_desc_cs_video_frm_t;

/* video streaming interface */
typedef struct TU_ATTR_PACKED {
  uint8_t index_vc;  /* index of bound video control interface */
  uint8_t index_vs;  /* index from the video control interface */
  struct {
    uint16_t beg;    /* Offset of the begging of video streaming interface descriptor */
    uint16_t end;    /* Offset of the end of video streaming interface descriptor */
    uint16_t cur;    /* Offset of the current settings */
    uint16_t ep[2];  /* Offset of endpoint descriptors. 0: streaming, 1: still capture */
  } desc;
  uint8_t *buffer;   /* frame buffer. assume linear buffer. no support for stride access */
  uint32_t bufsize;  /* frame buffer size */
  uint32_t offset;   /* offset for the next payload transfer */
  uint32_t max_payload_transfer_size;
  uint8_t  error_code;/* error code */
  /*------------- From this point, data is not cleared by bus reset -------------*/
  CFG_TUSB_MEM_ALIGN uint8_t ep_buf[CFG_TUD_VIDEO_STREAMING_EP_BUFSIZE]; /* EP transfer buffer for streaming */
} videod_streaming_interface_t;

/* video control interface */
typedef struct TU_ATTR_PACKED {
  void const *beg;  /* The head of the first video control interface descriptor */
  uint16_t    len;  /* Byte length of the descriptors */
  uint16_t    cur;  /* offset for current video control interface */
  uint8_t     stm[CFG_TUD_VIDEO_STREAMING]; /* Indices of streaming interface */
  uint8_t error_code;  /* error code */
  uint8_t power_mode;

  /*------------- From this point, data is not cleared by bus reset -------------*/
  // CFG_TUSB_MEM_ALIGN uint8_t ctl_buf[64]; /* EP transfer buffer for interrupt transfer */

} videod_interface_t;

#define ITF_STM_MEM_RESET_SIZE   offsetof(videod_streaming_interface_t, ep_buf)

//--------------------------------------------------------------------+
// INTERNAL OBJECT & FUNCTION DECLARATION
//--------------------------------------------------------------------+
CFG_TUSB_MEM_SECTION static videod_interface_t _videod_itf[CFG_TUD_VIDEO];
CFG_TUSB_MEM_SECTION static videod_streaming_interface_t _videod_streaming_itf[CFG_TUD_VIDEO_STREAMING];

static uint8_t const _cap_get     = 0x1u; /* support for GET */
static uint8_t const _cap_get_set = 0x3u; /* support for GET and SET */

/** Get interface number from the interface descriptor
 *
 * @param[in] desc    interface descriptor
 *
 * @return bInterfaceNumber */
static inline uint8_t _desc_itfnum(void const *desc)
{
  return ((uint8_t const*)desc)[2];
}

/** Get endpoint address from the endpoint descriptor
 *
 * @param[in] desc    endpoint descriptor
 *
 * @return bEndpointAddress */
static inline uint8_t _desc_ep_addr(void const *desc)
{
  return ((uint8_t const*)desc)[2];
}

/** Get instance of streaming interface
 *
 * @param[in] ctl_idx    instance number of video control
 * @param[in] stm_idx    index number of streaming interface
 *
 * @return instance */
static videod_streaming_interface_t* _get_instance_streaming(uint_fast8_t ctl_idx, uint_fast8_t stm_idx)
{
  videod_interface_t *ctl = &_videod_itf[ctl_idx];
  if (!ctl->beg) return NULL;
  videod_streaming_interface_t *stm = &_videod_streaming_itf[ctl->stm[stm_idx]];
  if (!stm->desc.beg) return NULL;
  return stm;
}

static tusb_desc_vc_itf_t const* _get_desc_vc(videod_interface_t const *self)
{
  return (tusb_desc_vc_itf_t const *)(self->beg + self->cur);
}

static tusb_desc_vs_itf_t const* _get_desc_vs(videod_streaming_interface_t const *self)
{
  if (!self->desc.cur) return NULL;
  void const *desc = _videod_itf[self->index_vc].beg;
  return (tusb_desc_vs_itf_t const*)(desc + self->desc.cur);
}

/** Find the first descriptor of a given type
 *
 * @param[in] beg        The head of descriptor byte array.
 * @param[in] end        The tail of descriptor byte array.
 * @param[in] desc_type  The target descriptor type.
 *
 * @return The pointer for interface descriptor.
 * @retval end   did not found interface descriptor */
static void const* _find_desc(void const *beg, void const *end, uint_fast8_t desc_type)
{
  void const *cur = beg;
  while ((cur < end) && (desc_type != tu_desc_type(cur))) {
    cur = tu_desc_next(cur);
  }
  return cur;
}

/** Find the first descriptor specified by the arguments
 *
 * @param[in] beg        The head of descriptor byte array.
 * @param[in] end        The tail of descriptor byte array.
 * @param[in] desc_type  The target descriptor type
 * @param[in] element_0  The target element following the desc_type
 * @param[in] element_1  The target element following the element_0
 *
 * @return The pointer for interface descriptor.
 * @retval end   did not found interface descriptor */
static void const* _find_desc_3(void const *beg, void const *end,
                                uint_fast8_t desc_type,
                                uint_fast8_t element_0,
                                uint_fast8_t element_1)
{
  for (void const *cur = beg; cur < end; cur = _find_desc(cur, end, desc_type)) {
    uint8_t const *p = (uint8_t const *)cur;
    if ((p[2] == element_0) && (p[3] == element_1)) {
      return cur;
    }
    cur = tu_desc_next(cur);
  }
  return end;
}

/** Return the next interface descriptor which has another interface number.
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
static inline void const* _find_desc_itf(void const *beg, void const *end, uint_fast8_t itfnum, uint_fast8_t altnum)
{
  return _find_desc_3(beg, end, TUSB_DESC_INTERFACE, itfnum, altnum);
}

/** Find the first endpoint descriptor belonging to the current interface descriptor.
 *
 * The search range is from `beg` to `end` or the next interface descriptor.
 *
 * @param[in] beg     The head of descriptor byte array.
 * @param[in] end     The tail of descriptor byte array.
 *
 * @return The pointer for endpoint descriptor.
 * @retval end   did not found endpoint descriptor */
static void const* _find_desc_ep(void const *beg, void const *end)
{
  for (void const *cur = beg; cur < end; cur = tu_desc_next(cur)) {
    uint_fast8_t desc_type = tu_desc_type(cur);
    if (TUSB_DESC_ENDPOINT == desc_type) return cur;
    if (TUSB_DESC_INTERFACE == desc_type) break;
  }
  return end;
}

/** Return the end of the video control descriptor. */
static inline void const* _end_of_control_descriptor(void const *desc)
{
  tusb_desc_vc_itf_t const *vc = (tusb_desc_vc_itf_t const *)desc;
  return desc + vc->std.bLength + vc->ctl.wTotalLength;
}

/** Find the first entity descriptor with the entity ID
 *  specified by the argument belonging to the current video control descriptor.
 *
 * @param[in] desc      The video control interface descriptor.
 * @param[in] entityid  The target entity id.
 *
 * @return The pointer for interface descriptor.
 * @retval end   did not found interface descriptor */
static void const* _find_desc_entity(void const *desc, uint_fast8_t entityid)
{
  void const *end = _end_of_control_descriptor(desc);
  for (void const *cur = desc; cur < end; cur = _find_desc(cur, end, TUSB_DESC_CS_INTERFACE)) {
    tusb_desc_cs_video_entity_itf_t const *itf = (tusb_desc_cs_video_entity_itf_t const *)cur;
    if ((VIDEO_CS_ITF_VC_INPUT_TERMINAL  <= itf->bDescriptorSubtype
         && itf->bDescriptorSubtype < VIDEO_CS_ITF_VC_MAX)
        && itf->bEntityId == entityid) {
      return itf;
    }
    cur = tu_desc_next(cur);
  }
  return end;
}

/** Return the end of the video streaming descriptor. */
static inline void const* _end_of_streaming_descriptor(void const *desc)
{
  tusb_desc_vs_itf_t const *vs = (tusb_desc_vs_itf_t const *)desc;
  return desc + vs->std.bLength + vs->stm.wTotalLength;
}

/** Find the first format descriptor with the specified format number. */
static inline void const *_find_desc_format(void const *beg, void const *end, uint_fast8_t fmtnum)
{
  for (void const *cur = beg; cur < end; cur = _find_desc(cur, end, TUSB_DESC_CS_INTERFACE)) {
    uint8_t const *p = (uint8_t const *)cur;
    uint_fast8_t fmt = p[2];
    if ((fmt == VIDEO_CS_ITF_VS_FORMAT_UNCOMPRESSED ||
         fmt == VIDEO_CS_ITF_VS_FORMAT_MJPEG ||
         fmt == VIDEO_CS_ITF_VS_FORMAT_DV ||
         fmt == VIDEO_CS_ITF_VS_FRAME_FRAME_BASED) &&
        fmtnum == p[3]) {
      return cur;
    }
    cur = tu_desc_next(cur);
  }
  return end;
}

/** Find the first frame descriptor with the specified format number. */
static inline void const *_find_desc_frame(void const *beg, void const *end, uint_fast8_t frmnum)
{
  for (void const *cur = beg; cur < end; cur = _find_desc(cur, end, TUSB_DESC_CS_INTERFACE)) {
    uint8_t const *p = (uint8_t const *)cur;
    uint_fast8_t frm = p[2];
    if ((frm == VIDEO_CS_ITF_VS_FRAME_UNCOMPRESSED ||
         frm == VIDEO_CS_ITF_VS_FRAME_MJPEG ||
         frm == VIDEO_CS_ITF_VS_FRAME_FRAME_BASED) &&
        frmnum == p[3]) {
      return cur;
    }
    cur = tu_desc_next(cur);
  }
  return end;
}

/** Set uniquely determined values to variables that have not been set
 *
 * @param[in,out] param       Target */
static bool _update_streaming_parameters(videod_streaming_interface_t const *stm,
                                         video_probe_and_commit_control_t *param)
{
  tusb_desc_vs_itf_t const *vs = _get_desc_vs(stm);
  uint_fast8_t fmtnum = param->bFormatIndex;
  TU_ASSERT(vs && fmtnum <= vs->stm.bNumFormats);
  if (!fmtnum) {
    if (1 < vs->stm.bNumFormats) return true; /* Need to negotiate all variables. */
    fmtnum = 1;
    param->bFormatIndex = 1;
  }

  /* Set the parameters determined by the format  */
  param->wKeyFrameRate    = 1;
  param->wPFrameRate      = 0;
  param->wCompWindowSize  = 1; /* GOP size? */
  param->wDelay           = 0; /* milliseconds */
  param->dwClockFrequency = 27000000; /* same as MPEG-2 system time clock  */
  param->bmFramingInfo    = 0x3; /* enables FrameID and EndOfFrame */
  param->bPreferedVersion = 1;
  param->bMinVersion      = 1;
  param->bMaxVersion      = 1;
  param->bUsage           = 0;
  param->bBitDepthLuma    = 8;

  void const *end = _end_of_streaming_descriptor(vs);
  tusb_desc_cs_video_fmt_t const *fmt = _find_desc_format(tu_desc_next(vs), end, fmtnum);
  TU_ASSERT(fmt != end);

  switch (fmt->bDescriptorSubType) {
    case VIDEO_CS_ITF_VS_FORMAT_UNCOMPRESSED:
      param->wCompQuality = 1; /* 1 to 10000 */
      break;
  case VIDEO_CS_ITF_VS_FORMAT_MJPEG:
      break;
    default: return false;
  }

  uint_fast8_t frmnum = param->bFrameIndex;
  TU_ASSERT(frmnum <= fmt->bNumFrameDescriptors);
  if (!frmnum) {
    if (1 < fmt->bNumFrameDescriptors) return true;
    frmnum = 1;
    param->bFrameIndex = 1;
  }
  tusb_desc_cs_video_frm_t const *frm = _find_desc_frame(tu_desc_next(fmt), end, frmnum);
  TU_ASSERT(frm != end);

  /* Set the parameters determined by the frame  */
  uint_fast32_t frame_size = param->dwMaxVideoFrameSize;
  if (!frame_size) {
    switch (fmt->bDescriptorSubType) {
      case VIDEO_CS_ITF_VS_FORMAT_UNCOMPRESSED:
        frame_size = (uint_fast32_t)frm->wWidth * frm->wHeight * fmt->uncompressed.bBitsPerPixel / 8;
        break;
      case VIDEO_CS_ITF_VS_FORMAT_MJPEG:
        frame_size = (uint_fast32_t)frm->wWidth * frm->wHeight * 16 / 8; /* YUV422 */
        break;
      default: break;
    }
    param->dwMaxVideoFrameSize = frame_size;
  }

  uint_fast32_t interval = param->dwFrameInterval;
  if (!interval) {
    if ((1 < frm->uncompressed.bFrameIntervalType) ||
        ((0 == frm->uncompressed.bFrameIntervalType) &&
         (frm->uncompressed.dwFrameInterval[1] != frm->uncompressed.dwFrameInterval[0]))) {
      return true;
    }
    interval = frm->uncompressed.dwFrameInterval[0];
    param->dwFrameInterval = interval;
  }
  uint_fast32_t interval_ms = interval / 10000;
  TU_ASSERT(interval_ms);
  uint_fast32_t payload_size = (frame_size + interval_ms - 1) / interval_ms + 2;
  if (CFG_TUD_VIDEO_STREAMING_EP_BUFSIZE < payload_size)
    payload_size = CFG_TUD_VIDEO_STREAMING_EP_BUFSIZE;
  param->dwMaxPayloadTransferSize = payload_size;
  return true;
}

/** Set the minimum, maximum, default values or resolutions to variables which need to negotiate with the host
 *
 * @param[in]     request     GET_MAX, GET_MIN, GET_RES or GET_DEF
 * @param[in,out] param       Target
 */
static bool _negotiate_streaming_parameters(videod_streaming_interface_t const *stm, uint_fast8_t request,
                                            video_probe_and_commit_control_t *param)
{
  uint_fast8_t const fmtnum = param->bFormatIndex;
  if (!fmtnum) {
    switch (request) {
      case VIDEO_REQUEST_GET_MAX:
        if (_get_desc_vs(stm))
          param->bFormatIndex = _get_desc_vs(stm)->stm.bNumFormats;
        break;
      case VIDEO_REQUEST_GET_MIN:
      case VIDEO_REQUEST_GET_DEF:
        param->bFormatIndex = 1;
        break;
      default: return false;
    }
    /* Set the parameters determined by the format  */
    param->wKeyFrameRate    = 1;
    param->wPFrameRate      = 0;
    param->wCompQuality     = 1; /* 1 to 10000 */
    param->wCompWindowSize  = 1; /* GOP size? */
    param->wDelay           = 0; /* milliseconds */
    param->dwClockFrequency = 27000000; /* same as MPEG-2 system time clock  */
    param->bmFramingInfo    = 0x3; /* enables FrameID and EndOfFrame */
    param->bPreferedVersion = 1;
    param->bMinVersion      = 1;
    param->bMaxVersion      = 1;
    param->bUsage           = 0;
    param->bBitDepthLuma    = 8;
    return true;
  }

  uint_fast8_t frmnum = param->bFrameIndex;
  if (!frmnum) {
    tusb_desc_vs_itf_t const *vs = _get_desc_vs(stm);
    TU_ASSERT(vs);
    void const *end = _end_of_streaming_descriptor(vs);
    tusb_desc_cs_video_fmt_t const *fmt = _find_desc_format(tu_desc_next(vs), end, fmtnum);
    switch (request) {
      case VIDEO_REQUEST_GET_MAX:
        frmnum = fmt->bNumFrameDescriptors;
        break;
      case VIDEO_REQUEST_GET_MIN:
        frmnum = 1;
        break;
      case VIDEO_REQUEST_GET_DEF:
        switch (fmt->bDescriptorSubType) {
        case VIDEO_CS_ITF_VS_FORMAT_UNCOMPRESSED:
          frmnum = fmt->uncompressed.bDefaultFrameIndex;
          break;
        case VIDEO_CS_ITF_VS_FORMAT_MJPEG:
          frmnum = fmt->mjpeg.bDefaultFrameIndex;
          break;
        default: return false;
        }
        break;
      default: return false;
    }
    param->bFrameIndex = (uint8_t)frmnum;
    /* Set the parameters determined by the frame */
    tusb_desc_cs_video_frm_t const *frm = _find_desc_frame(tu_desc_next(fmt), end, frmnum);
    uint_fast32_t frame_size;
    switch (fmt->bDescriptorSubType) {
      case VIDEO_CS_ITF_VS_FORMAT_UNCOMPRESSED:
        frame_size = (uint_fast32_t)frm->wWidth * frm->wHeight * fmt->uncompressed.bBitsPerPixel / 8;
        break;
      case VIDEO_CS_ITF_VS_FORMAT_MJPEG:
        frame_size = (uint_fast32_t)frm->wWidth * frm->wHeight * 16 / 8; /* YUV422 */
        break;
      default: return false;
    }
    param->dwMaxVideoFrameSize = frame_size;
    return true;
  }

  if (!param->dwFrameInterval) {
    tusb_desc_vs_itf_t const *vs = _get_desc_vs(stm);
    TU_ASSERT(vs);
    void const *end = _end_of_streaming_descriptor(vs);
    tusb_desc_cs_video_fmt_t const *fmt = _find_desc_format(tu_desc_next(vs), end, fmtnum);
    tusb_desc_cs_video_frm_t const *frm = _find_desc_frame(tu_desc_next(fmt), end, frmnum);

    uint_fast32_t interval, interval_ms;
    switch (request) {
      case VIDEO_REQUEST_GET_MAX:
        {
          uint_fast32_t min_interval, max_interval;
          uint_fast8_t num_intervals = frm->uncompressed.bFrameIntervalType;
          max_interval = num_intervals ? frm->uncompressed.dwFrameInterval[num_intervals - 1]: frm->uncompressed.dwFrameInterval[1];
          min_interval = frm->uncompressed.dwFrameInterval[0];
          interval = max_interval;
          interval_ms = min_interval / 10000;
        }
        break;
      case VIDEO_REQUEST_GET_MIN:
        {
          uint_fast32_t min_interval, max_interval;
          uint_fast8_t num_intervals = frm->uncompressed.bFrameIntervalType;
          max_interval = num_intervals ? frm->uncompressed.dwFrameInterval[num_intervals - 1]: frm->uncompressed.dwFrameInterval[1];
          min_interval = frm->uncompressed.dwFrameInterval[0];
          interval = min_interval;
          interval_ms = max_interval / 10000;
        }
        break;
      case VIDEO_REQUEST_GET_DEF:
        interval = frm->uncompressed.dwDefaultFrameInterval;
        interval_ms = interval / 10000;
        break;
      case VIDEO_REQUEST_GET_RES:
        {
          uint_fast8_t num_intervals = frm->uncompressed.bFrameIntervalType;
          if (num_intervals) {
            interval = 0;
          } else {
            interval = frm->uncompressed.dwFrameInterval[2];
            interval_ms = interval / 10000;
          }
        }
        break;
      default: return false;
    }
    param->dwFrameInterval = interval;
    if (!interval) {
      param->dwMaxPayloadTransferSize = 0;
    } else {
      uint_fast32_t frame_size = param->dwMaxVideoFrameSize;
      uint_fast32_t payload_size;
      if (!interval_ms) {
        payload_size = frame_size + 2;
      } else {
        payload_size = (frame_size + interval_ms - 1) / interval_ms + 2;
      }
      if (CFG_TUD_VIDEO_STREAMING_EP_BUFSIZE < payload_size)
        payload_size = CFG_TUD_VIDEO_STREAMING_EP_BUFSIZE;
      param->dwMaxPayloadTransferSize = payload_size;
    }
    return true;
  }
  return true;
}

/** Close current video control interface.
 *
 * @param[in,out] self     Video control interface context.
 * @param[in]     altnum   The target alternate setting number. */
static bool _close_vc_itf(uint8_t rhport, videod_interface_t *self)
{
  tusb_desc_vc_itf_t const *vc = _get_desc_vc(self);
  /* The next descriptor after the class-specific VC interface header descriptor. */
  void const *cur = (void const*)vc + vc->std.bLength + vc->ctl.bLength;
  /* The end of the video control interface descriptor. */
  void const *end = _end_of_control_descriptor(vc);
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

/** Set the alternate setting to own video control interface.
 *
 * @param[in,out] self     Video control interface context.
 * @param[in]     altnum   The target alternate setting number. */
static bool _open_vc_itf(uint8_t rhport, videod_interface_t *self, uint_fast8_t altnum)
{
  TU_LOG2("    open VC %d\n", altnum);
  void const *beg = self->beg;
  void const *end = beg + self->len;
  /* The first descriptor is a video control interface descriptor. */
  void const *cur = _find_desc_itf(beg, end, _desc_itfnum(beg), altnum);
  TU_LOG2("    cur %d\n", cur - beg);
  TU_VERIFY(cur < end);

  tusb_desc_vc_itf_t const *vc = (tusb_desc_vc_itf_t const *)cur;
  TU_LOG2("    bInCollection %d\n", vc->ctl.bInCollection);
  /* Support for up to 2 streaming interfaces only. */
  TU_ASSERT(vc->ctl.bInCollection <= CFG_TUD_VIDEO_STREAMING);

  /* Update to point the end of the video control interface descriptor. */
  end  = _end_of_control_descriptor(cur);
  /* Advance to the next descriptor after the class-specific VC interface header descriptor. */
  cur += vc->std.bLength + vc->ctl.bLength;
  TU_LOG2("    bNumEndpoints %d\n", vc->std.bNumEndpoints);
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
  self->cur = (uint16_t) ((void const*)vc - beg);
  return true;
}

/** Set the alternate setting to own video streaming interface.
 *
 * @param[in,out] stm      Streaming interface context.
 * @param[in]     altnum   The target alternate setting number. */
static bool _open_vs_itf(uint8_t rhport, videod_streaming_interface_t *stm, uint_fast8_t altnum)
{
  uint_fast8_t i;
  TU_LOG2("    reopen VS %d\n", altnum);
  void const *desc = _videod_itf[stm->index_vc].beg;

  /* Close endpoints of previous settings. */
  for (i = 0; i < TU_ARRAY_SIZE(stm->desc.ep); ++i) {
    uint_fast16_t ofs_ep = stm->desc.ep[i];
    if (!ofs_ep) break;
    uint8_t  ep_adr = _desc_ep_addr(desc + ofs_ep);
    usbd_edpt_close(rhport, ep_adr);
    stm->desc.ep[i] = 0;
    TU_LOG2("    close EP%02x\n", ep_adr);
  }
  /* clear transfer management information */
  stm->buffer  = NULL;
  stm->bufsize = 0;
  stm->offset  = 0;

  /* Find a alternate interface */
  void const *beg = desc + stm->desc.beg;
  void const *end = desc + stm->desc.end;
  void const *cur = _find_desc_itf(beg, end, _desc_itfnum(beg), altnum);
  TU_VERIFY(cur < end);
  uint_fast8_t numeps = ((tusb_desc_interface_t const *)cur)->bNumEndpoints;
  TU_ASSERT(numeps <= TU_ARRAY_SIZE(stm->desc.ep));
  stm->desc.cur = (uint16_t) (cur - desc); /* Save the offset of the new settings */
  if (!altnum) {
    /* initialize streaming settings */
    stm->max_payload_transfer_size = 0;
    video_probe_and_commit_control_t *param =
      (video_probe_and_commit_control_t *)&stm->ep_buf;
    tu_memclr(param, sizeof(*param));
    TU_LOG2("    done 0\n");
    return _update_streaming_parameters(stm, param);
  }
  /* Open endpoints of the new settings. */
  for (i = 0, cur = tu_desc_next(cur); i < numeps; ++i, cur = tu_desc_next(cur)) {
    cur = _find_desc_ep(cur, end);
    TU_ASSERT(cur < end);
    tusb_desc_endpoint_t const *ep = (tusb_desc_endpoint_t const*)cur;
    if (!stm->max_payload_transfer_size) {
      video_probe_and_commit_control_t const *param = (video_probe_and_commit_control_t const*)&stm->ep_buf;
      uint_fast32_t max_size = param->dwMaxPayloadTransferSize;
      if ((TUSB_XFER_ISOCHRONOUS == ep->bmAttributes.xfer) &&
          (tu_edpt_packet_size(ep) < max_size))
      {
        /* FS must be less than or equal to max packet size */
        return false;
      }
      /* Set the negotiated value */
      stm->max_payload_transfer_size = max_size;
    }
    TU_ASSERT(usbd_edpt_open(rhport, ep));
    stm->desc.ep[i] = (uint16_t) (cur - desc);
    TU_LOG2("    open EP%02x\n", _desc_ep_addr(cur));
  }
  /* initialize payload header */
  tusb_video_payload_header_t *hdr = (tusb_video_payload_header_t*)stm->ep_buf;
  hdr->bHeaderLength = sizeof(*hdr);
  hdr->bmHeaderInfo  = 0;

  TU_LOG2("    done\n");
  return true;
}

/** Prepare the next packet payload. */
static uint_fast16_t _prepare_in_payload(videod_streaming_interface_t *stm)
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
static int handle_video_ctl_std_req(uint8_t rhport, uint8_t stage,
                                    tusb_control_request_t const *request,
                                    uint_fast8_t ctl_idx)
{
  switch (request->bRequest) {
    case TUSB_REQ_GET_INTERFACE:
      if (stage == CONTROL_STAGE_SETUP)
      {
        TU_VERIFY(1 == request->wLength, VIDEO_ERROR_UNKNOWN);
        tusb_desc_vc_itf_t const *vc = _get_desc_vc(&_videod_itf[ctl_idx]);
        TU_VERIFY(vc, VIDEO_ERROR_UNKNOWN);

        uint8_t alt_num = vc->std.bAlternateSetting;

        TU_VERIFY(tud_control_xfer(rhport, request, &alt_num, sizeof(alt_num)), VIDEO_ERROR_UNKNOWN);
      }
      return VIDEO_ERROR_NONE;

    case TUSB_REQ_SET_INTERFACE:
      if (stage == CONTROL_STAGE_SETUP)
      {
        TU_VERIFY(0 == request->wLength, VIDEO_ERROR_UNKNOWN);
        TU_VERIFY(_close_vc_itf(rhport, &_videod_itf[ctl_idx]), VIDEO_ERROR_UNKNOWN);
        TU_VERIFY(_open_vc_itf(rhport, &_videod_itf[ctl_idx], request->wValue), VIDEO_ERROR_UNKNOWN);
        tud_control_status(rhport, request);
      }
      return VIDEO_ERROR_NONE;

    default: /* Unknown/Unsupported request */
      TU_BREAKPOINT();
      return VIDEO_ERROR_INVALID_REQUEST;
  }
}

static int handle_video_ctl_cs_req(uint8_t rhport, uint8_t stage,
                                   tusb_control_request_t const *request,
                                   uint_fast8_t ctl_idx)
{
  videod_interface_t *self = &_videod_itf[ctl_idx];

  /* 4.2.1 Interface Control Request */
  switch (TU_U16_HIGH(request->wValue)) {
    case VIDEO_VC_CTL_VIDEO_POWER_MODE:
      switch (request->bRequest) {
        case VIDEO_REQUEST_SET_CUR:
          if (stage == CONTROL_STAGE_SETUP) {
            TU_VERIFY(1 == request->wLength, VIDEO_ERROR_UNKNOWN);
            TU_VERIFY(tud_control_xfer(rhport, request, &self->power_mode, sizeof(self->power_mode)), VIDEO_ERROR_UNKNOWN);
          } else if (stage == CONTROL_STAGE_DATA) {
            if (tud_video_power_mode_cb) return tud_video_power_mode_cb(ctl_idx, self->power_mode);
          }
          return VIDEO_ERROR_NONE;

        case VIDEO_REQUEST_GET_CUR:
          if (stage == CONTROL_STAGE_SETUP)
          {
            TU_VERIFY(1 == request->wLength, VIDEO_ERROR_UNKNOWN);
            TU_VERIFY(tud_control_xfer(rhport, request, &self->power_mode, sizeof(self->power_mode)), VIDEO_ERROR_UNKNOWN);
          }
          return VIDEO_ERROR_NONE;

        case VIDEO_REQUEST_GET_INFO:
          if (stage == CONTROL_STAGE_SETUP)
          {
            TU_VERIFY(1 == request->wLength, VIDEO_ERROR_UNKNOWN);
            TU_VERIFY(tud_control_xfer(rhport, request, (uint8_t*)(uintptr_t) &_cap_get_set, sizeof(_cap_get_set)), VIDEO_ERROR_UNKNOWN);
          }
          return VIDEO_ERROR_NONE;

        default: break;
      }
      break;

    case VIDEO_VC_CTL_REQUEST_ERROR_CODE:
      switch (request->bRequest) {
        case VIDEO_REQUEST_GET_CUR:
          if (stage == CONTROL_STAGE_SETUP)
          {
            TU_VERIFY(tud_control_xfer(rhport, request, &self->error_code, sizeof(uint8_t)), VIDEO_ERROR_UNKNOWN);
          }
          return VIDEO_ERROR_NONE;

        case VIDEO_REQUEST_GET_INFO:
          if (stage == CONTROL_STAGE_SETUP)
          {
            TU_VERIFY(tud_control_xfer(rhport, request, (uint8_t*)(uintptr_t) &_cap_get, sizeof(_cap_get)), VIDEO_ERROR_UNKNOWN);
          }
          return VIDEO_ERROR_NONE;

        default: break;
      }
      break;

    default: break;
  }

  /* Unknown/Unsupported request */
  TU_BREAKPOINT();
  return VIDEO_ERROR_INVALID_REQUEST;
}

static int handle_video_ctl_req(uint8_t rhport, uint8_t stage,
                                tusb_control_request_t const *request,
                                uint_fast8_t ctl_idx)
{
  uint_fast8_t entity_id;
  switch (request->bmRequestType_bit.type) {
    case TUSB_REQ_TYPE_STANDARD:
      return handle_video_ctl_std_req(rhport, stage, request, ctl_idx);

    case TUSB_REQ_TYPE_CLASS:
      entity_id = TU_U16_HIGH(request->wIndex);
      if (!entity_id) {
        return handle_video_ctl_cs_req(rhport, stage, request, ctl_idx);
      } else {
        TU_VERIFY(_find_desc_entity(_get_desc_vc(&_videod_itf[ctl_idx]), entity_id), VIDEO_ERROR_INVALID_REQUEST);
        return VIDEO_ERROR_NONE;
      }

    default:
      return VIDEO_ERROR_INVALID_REQUEST;
  }
}

static int handle_video_stm_std_req(uint8_t rhport, uint8_t stage,
                                    tusb_control_request_t const *request,
                                    uint_fast8_t stm_idx)
{
  videod_streaming_interface_t *self = &_videod_streaming_itf[stm_idx];
  switch (request->bRequest) {
    case TUSB_REQ_GET_INTERFACE:
      if (stage == CONTROL_STAGE_SETUP)
      {
        TU_VERIFY(1 == request->wLength, VIDEO_ERROR_UNKNOWN);
        tusb_desc_vs_itf_t const *vs = _get_desc_vs(self);
        TU_VERIFY(vs, VIDEO_ERROR_UNKNOWN);
        uint8_t alt_num = vs->std.bAlternateSetting;

        TU_VERIFY(tud_control_xfer(rhport, request, &alt_num, sizeof(alt_num)), VIDEO_ERROR_UNKNOWN);
      }
      return VIDEO_ERROR_NONE;

    case TUSB_REQ_SET_INTERFACE:
      if (stage == CONTROL_STAGE_SETUP)
      {
        TU_VERIFY(_open_vs_itf(rhport, self, request->wValue), VIDEO_ERROR_UNKNOWN);
        tud_control_status(rhport, request);
      }
      return VIDEO_ERROR_NONE;

    default: /* Unknown/Unsupported request */
      TU_BREAKPOINT();
      return VIDEO_ERROR_INVALID_REQUEST;
  }
}

static int handle_video_stm_cs_req(uint8_t rhport, uint8_t stage,
                                   tusb_control_request_t const *request,
                                   uint_fast8_t stm_idx)
{
  (void)rhport;
  videod_streaming_interface_t *self = &_videod_streaming_itf[stm_idx];

  /* 4.2.1 Interface Control Request */
  switch (TU_U16_HIGH(request->wValue)) {
    case VIDEO_VS_CTL_STREAM_ERROR_CODE:
      switch (request->bRequest) {
        case VIDEO_REQUEST_GET_CUR:
          if (stage == CONTROL_STAGE_SETUP)
          {
            /* TODO */
            TU_VERIFY(tud_control_xfer(rhport, request, &self->error_code, sizeof(uint8_t)), VIDEO_ERROR_UNKNOWN);
          }
          return VIDEO_ERROR_NONE;

        case VIDEO_REQUEST_GET_INFO:
          if (stage == CONTROL_STAGE_SETUP)
          {
            TU_VERIFY(tud_control_xfer(rhport, request, (uint8_t*)(uintptr_t) &_cap_get, sizeof(_cap_get)), VIDEO_ERROR_UNKNOWN);
          }
          return VIDEO_ERROR_NONE;

        default: break;
      }
      break;

    case VIDEO_VS_CTL_PROBE:
      switch (request->bRequest) {
        case VIDEO_REQUEST_SET_CUR:
          if (stage == CONTROL_STAGE_SETUP) {
            TU_VERIFY(sizeof(video_probe_and_commit_control_t) == request->wLength, VIDEO_ERROR_UNKNOWN);
            TU_VERIFY(tud_control_xfer(rhport, request, self->ep_buf, sizeof(video_probe_and_commit_control_t)),
                      VIDEO_ERROR_UNKNOWN);
          } else if (stage == CONTROL_STAGE_DATA) {
            TU_VERIFY(_update_streaming_parameters(self, (video_probe_and_commit_control_t*)self->ep_buf),
                      VIDEO_ERROR_INVALID_VALUE_WITHIN_RANGE);
          }
          return VIDEO_ERROR_NONE;

        case VIDEO_REQUEST_GET_CUR:
          if (stage == CONTROL_STAGE_SETUP)
          {
            TU_VERIFY(request->wLength, VIDEO_ERROR_UNKNOWN);
            TU_VERIFY(tud_control_xfer(rhport, request, self->ep_buf, sizeof(video_probe_and_commit_control_t)), VIDEO_ERROR_UNKNOWN);
          }
          return VIDEO_ERROR_NONE;

        case VIDEO_REQUEST_GET_MIN:
        case VIDEO_REQUEST_GET_MAX:
        case VIDEO_REQUEST_GET_RES:
        case VIDEO_REQUEST_GET_DEF:
          if (stage == CONTROL_STAGE_SETUP)
          {
            TU_VERIFY(request->wLength, VIDEO_ERROR_UNKNOWN);
            video_probe_and_commit_control_t tmp;
            tmp = *(video_probe_and_commit_control_t*)&self->ep_buf;
            TU_VERIFY(_negotiate_streaming_parameters(self, request->bRequest, &tmp), VIDEO_ERROR_INVALID_VALUE_WITHIN_RANGE);
            TU_VERIFY(tud_control_xfer(rhport, request, &tmp, sizeof(tmp)), VIDEO_ERROR_UNKNOWN);
          }
          return VIDEO_ERROR_NONE;

        case VIDEO_REQUEST_GET_LEN:
          if (stage == CONTROL_STAGE_SETUP)
          {
            TU_VERIFY(2 == request->wLength, VIDEO_ERROR_UNKNOWN);
            uint16_t len = sizeof(video_probe_and_commit_control_t);
            TU_VERIFY(tud_control_xfer(rhport, request, (uint8_t*)&len, sizeof(len)), VIDEO_ERROR_UNKNOWN);
          }
          return VIDEO_ERROR_NONE;

        case VIDEO_REQUEST_GET_INFO:
          if (stage == CONTROL_STAGE_SETUP)
          {
            TU_VERIFY(1 == request->wLength, VIDEO_ERROR_UNKNOWN);
            TU_VERIFY(tud_control_xfer(rhport, request, (uint8_t*)(uintptr_t)&_cap_get_set, sizeof(_cap_get_set)), VIDEO_ERROR_UNKNOWN);
          }
          return VIDEO_ERROR_NONE;

        default: break;
      }
      break;

    case VIDEO_VS_CTL_COMMIT:
      switch (request->bRequest) {
        case VIDEO_REQUEST_SET_CUR:
          if (stage == CONTROL_STAGE_SETUP) {
            TU_VERIFY(sizeof(video_probe_and_commit_control_t) == request->wLength, VIDEO_ERROR_UNKNOWN);
            TU_VERIFY(tud_control_xfer(rhport, request, self->ep_buf, sizeof(video_probe_and_commit_control_t)), VIDEO_ERROR_UNKNOWN);
          } else if (stage == CONTROL_STAGE_DATA) {
            TU_VERIFY(_update_streaming_parameters(self, (video_probe_and_commit_control_t*)self->ep_buf), VIDEO_ERROR_INVALID_VALUE_WITHIN_RANGE);
            if (tud_video_commit_cb) {
              return tud_video_commit_cb(self->index_vc, self->index_vs, (video_probe_and_commit_control_t*)self->ep_buf);
            }
          }
          return VIDEO_ERROR_NONE;

        case VIDEO_REQUEST_GET_CUR:
          if (stage == CONTROL_STAGE_SETUP)
          {
            TU_VERIFY(request->wLength, VIDEO_ERROR_UNKNOWN);
            TU_VERIFY(tud_control_xfer(rhport, request, self->ep_buf, sizeof(video_probe_and_commit_control_t)), VIDEO_ERROR_UNKNOWN);
          }
          return VIDEO_ERROR_NONE;

        case VIDEO_REQUEST_GET_LEN:
          if (stage == CONTROL_STAGE_SETUP)
          {
            TU_VERIFY(2 == request->wLength, VIDEO_ERROR_UNKNOWN);
            uint16_t len = sizeof(video_probe_and_commit_control_t);
            TU_VERIFY(tud_control_xfer(rhport, request, (uint8_t*)&len, sizeof(len)), VIDEO_ERROR_UNKNOWN);
          }
          return VIDEO_ERROR_NONE;

        case VIDEO_REQUEST_GET_INFO:
          if (stage == CONTROL_STAGE_SETUP)
          {
            TU_VERIFY(1 == request->wLength, VIDEO_ERROR_UNKNOWN);
            TU_VERIFY(tud_control_xfer(rhport, request, (uint8_t*)(uintptr_t) &_cap_get_set, sizeof(_cap_get_set)), VIDEO_ERROR_UNKNOWN);
          }
          return VIDEO_ERROR_NONE;

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
  return VIDEO_ERROR_INVALID_REQUEST;
}

static int handle_video_stm_req(uint8_t rhport, uint8_t stage,
                                tusb_control_request_t const *request,
                                uint_fast8_t stm_idx)
{
  switch (request->bmRequestType_bit.type) {
    case TUSB_REQ_TYPE_STANDARD:
      return handle_video_stm_std_req(rhport, stage, request, stm_idx);

    case TUSB_REQ_TYPE_CLASS:
      if (TU_U16_HIGH(request->wIndex)) return VIDEO_ERROR_INVALID_REQUEST;
      return handle_video_stm_cs_req(rhport, stage, request, stm_idx);

    default: return VIDEO_ERROR_INVALID_REQUEST;
  }
  return VIDEO_ERROR_UNKNOWN;
}

//--------------------------------------------------------------------+
// APPLICATION API
//--------------------------------------------------------------------+

bool tud_video_n_connected(uint_fast8_t ctl_idx)
{
  TU_ASSERT(ctl_idx < CFG_TUD_VIDEO);
  videod_streaming_interface_t *stm = _get_instance_streaming(ctl_idx, 0);
  if (stm) return true;
  return false;
}

bool tud_video_n_streaming(uint_fast8_t ctl_idx, uint_fast8_t stm_idx)
{
  TU_ASSERT(ctl_idx < CFG_TUD_VIDEO);
  TU_ASSERT(stm_idx < CFG_TUD_VIDEO_STREAMING);
  videod_streaming_interface_t *stm = _get_instance_streaming(ctl_idx, stm_idx);
  if (!stm || !stm->desc.ep[0]) return false;
  return true;
}

bool tud_video_n_frame_xfer(uint_fast8_t ctl_idx, uint_fast8_t stm_idx, void *buffer, size_t bufsize)
{
  TU_ASSERT(ctl_idx < CFG_TUD_VIDEO);
  TU_ASSERT(stm_idx < CFG_TUD_VIDEO_STREAMING);
  if (!buffer || !bufsize) return false;
  videod_streaming_interface_t *stm = _get_instance_streaming(ctl_idx, stm_idx);
  if (!stm || !stm->desc.ep[0] || stm->buffer) return false;

  /* Find EP address */
  void const *desc = _videod_itf[stm->index_vc].beg;
  uint8_t ep_addr = 0;
  for (uint_fast8_t i = 0; i < CFG_TUD_VIDEO_STREAMING; ++i) {
    uint_fast16_t ofs_ep = stm->desc.ep[i];
    if (!ofs_ep) continue;
    ep_addr = _desc_ep_addr(desc + ofs_ep);
    break;
  }
  if (!ep_addr) return false;

  TU_VERIFY( usbd_edpt_claim(0, ep_addr) );
  /* update the packet header */
  tusb_video_payload_header_t *hdr = (tusb_video_payload_header_t*)stm->ep_buf;
  hdr->FrameID   ^= 1;
  hdr->EndOfFrame = 0;
  /* update the packet data */
  stm->buffer     = (uint8_t*)buffer;
  stm->bufsize    = bufsize;
  uint_fast16_t pkt_len = _prepare_in_payload(stm);
  TU_ASSERT( usbd_edpt_xfer(0, ep_addr, stm->ep_buf, (uint16_t) pkt_len), 0);
  return true;
}

//--------------------------------------------------------------------+
// USBD Driver API
//--------------------------------------------------------------------+
void videod_init(void)
{
  for (uint_fast8_t i = 0; i < CFG_TUD_VIDEO; ++i) {
    videod_interface_t* ctl = &_videod_itf[i];
    tu_memclr(ctl, sizeof(*ctl));
  }
  for (uint_fast8_t i = 0; i < CFG_TUD_VIDEO_STREAMING; ++i) {
    videod_streaming_interface_t *stm = &_videod_streaming_itf[i];
    tu_memclr(stm, ITF_STM_MEM_RESET_SIZE);
  }
}

void videod_reset(uint8_t rhport)
{
  (void) rhport;
  for (uint_fast8_t i = 0; i < CFG_TUD_VIDEO; ++i) {
    videod_interface_t* ctl = &_videod_itf[i];
    tu_memclr(ctl, sizeof(*ctl));
  }
  for (uint_fast8_t i = 0; i < CFG_TUD_VIDEO_STREAMING; ++i) {
    videod_streaming_interface_t *stm = &_videod_streaming_itf[i];
    tu_memclr(stm, ITF_STM_MEM_RESET_SIZE);
  }
}

uint16_t videod_open(uint8_t rhport, tusb_desc_interface_t const * itf_desc, uint16_t max_len)
{
  TU_VERIFY((TUSB_CLASS_VIDEO       == itf_desc->bInterfaceClass) &&
            (VIDEO_SUBCLASS_CONTROL == itf_desc->bInterfaceSubClass) &&
            (VIDEO_ITF_PROTOCOL_15  == itf_desc->bInterfaceProtocol), 0);

  /* Find available interface */
  videod_interface_t *self = NULL;
  uint8_t ctl_idx;
  for (ctl_idx = 0; ctl_idx < CFG_TUD_VIDEO; ++ctl_idx) {
    if (_videod_itf[ctl_idx].beg) continue;
    self = &_videod_itf[ctl_idx];
    break;
  }
  TU_ASSERT(ctl_idx < CFG_TUD_VIDEO, 0);

  void const *end = (void const*)itf_desc + max_len;
  self->beg = itf_desc;
  self->len = max_len;
  /*------------- Video Control Interface -------------*/
  TU_VERIFY(_open_vc_itf(rhport, self, 0), 0);
  tusb_desc_vc_itf_t const *vc = _get_desc_vc(self);
  uint_fast8_t bInCollection   = vc->ctl.bInCollection;
  /* Find the end of the video interface descriptor */
  void const *cur = _next_desc_itf(itf_desc, end);
  for (uint8_t stm_idx = 0; stm_idx < bInCollection; ++stm_idx) {
    videod_streaming_interface_t *stm = NULL;
    /* find free streaming interface handle */
    for (uint8_t i = 0; i < CFG_TUD_VIDEO_STREAMING; ++i) {
      if (_videod_streaming_itf[i].desc.beg) continue;
      stm = &_videod_streaming_itf[i];
      self->stm[stm_idx] = i;
      break;
    }
    TU_ASSERT(stm, 0);
    stm->index_vc = ctl_idx;
    stm->index_vs = stm_idx;
    stm->desc.beg = (uint16_t) ((uintptr_t)cur - (uintptr_t)itf_desc);
    cur = _next_desc_itf(cur, end);
    stm->desc.end = (uint16_t) ((uintptr_t)cur - (uintptr_t)itf_desc);
  }
  self->len = (uint16_t) ((uintptr_t)cur - (uintptr_t)itf_desc);
  return (uint16_t) ((uintptr_t)cur - (uintptr_t)itf_desc);
}

// Invoked when a control transfer occurred on an interface of this class
// Driver response accordingly to the request and the transfer stage (setup/data/ack)
// return false to stall control endpoint (e.g unsupported request)
bool videod_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const * request)
{
  int err;
  TU_VERIFY(request->bmRequestType_bit.recipient == TUSB_REQ_RCPT_INTERFACE);
  uint_fast8_t itfnum = tu_u16_low(request->wIndex);

  /* Identify which control interface to use */
  uint_fast8_t itf;
  for (itf = 0; itf < CFG_TUD_VIDEO; ++itf) {
    void const *desc = _videod_itf[itf].beg;
    if (!desc) continue;
    if (itfnum == _desc_itfnum(desc)) break;
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
    if (itfnum == _desc_itfnum(desc + stm->desc.beg)) break;
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
  videod_interface_t *ctl;
  videod_streaming_interface_t *stm;
  for (itf = 0; itf < CFG_TUD_VIDEO_STREAMING; ++itf) {
    stm = &_videod_streaming_itf[itf];
    uint_fast16_t const ep_ofs = stm->desc.ep[0];
    if (!ep_ofs) continue;
    ctl = &_videod_itf[stm->index_vc];
    void const *desc = ctl->beg;
    if (ep_addr == _desc_ep_addr(desc + ep_ofs)) break;
  }

  TU_ASSERT(itf < CFG_TUD_VIDEO_STREAMING);
  if (stm->offset < stm->bufsize) {
    /* Claim the endpoint */
    TU_VERIFY( usbd_edpt_claim(rhport, ep_addr), 0);
    uint_fast16_t pkt_len = _prepare_in_payload(stm);
    TU_ASSERT( usbd_edpt_xfer(rhport, ep_addr, stm->ep_buf, (uint16_t) pkt_len), 0);
  } else {
    stm->buffer  = NULL;
    stm->bufsize = 0;
    stm->offset  = 0;
    if (tud_video_frame_xfer_complete_cb) {
      tud_video_frame_xfer_complete_cb(stm->index_vc, stm->index_vs);
    }
  }
  return true;
}

#endif
