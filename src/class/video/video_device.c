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

typedef struct
{
  void const *beg;  /* The head of the first video control interface descriptor */
  uint16_t    len;  /* Byte length of the descriptors */
  uint16_t    ofs[3];  /* offsets for video control/streaming interface. 0:control 1:streaming 2:streaming */
  uint8_t power_mode;
  uint8_t error_code;

  /*------------- From this point, data is not cleared by bus reset -------------*/

  // Endpoint Transfer buffer
  //  CFG_TUSB_MEM_ALIGN uint8_t epout_buf[CFG_TUD_CDC_EP_BUFSIZE];
  //  CFG_TUSB_MEM_ALIGN uint8_t epin_buf[CFG_TUD_CDC_EP_BUFSIZE];
  uint8_t ctl_buf;

} videod_interface_t;

#define ITF_MEM_RESET_SIZE   offsetof(cdcd_interface_t, wanted_char)

//--------------------------------------------------------------------+
// INTERNAL OBJECT & FUNCTION DECLARATION
//--------------------------------------------------------------------+
CFG_TUSB_MEM_SECTION static videod_interface_t _videod_itf[CFG_TUD_VIDEO];

static uint8_t const _cap_get     = 0x1u; /* support for GET */
static uint8_t const _cap_get_set = 0x3u; /* support for GET and SET */

static tusb_desc_vc_itf_t const* _get_desc_vc(videod_interface_t const *self)
{
  return (tusb_desc_vc_itf_t const *)(self->beg + self->ofs[0]);
}

static uint16_t* _get_desc_ofs(videod_interface_t *self, unsigned itfnum)
{
  void const *beg = self->beg;
  uint16_t   *ofs = self->ofs;
  for (unsigned i = 1; i < sizeof(self->ofs)/sizeof(self->ofs[0]); ++i) {
    if (!ofs[i]) continue;
    tusb_desc_interface_t const* itf = (tusb_desc_interface_t const*)(beg + ofs[i]);
    if (itfnum == itf->bInterfaceNumber) return &ofs[i];
  }
  return NULL;
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
  unsigned itfnum = ((tusb_desc_interface_t const*)cur)->bInterfaceNumber;
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
static void const* _find_desc_itf(void const *beg, void const *end, unsigned itfnum, unsigned altnum)
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

/** Find the first entity descriptor with the specified entity ID in the video control interface descriptor.
 *
 * @param[in] vc        The video control interface descriptor.
 * @param[in] entityid  The target entity id.
 *
 * @return The pointer for interface descriptor.
 * @retval end   did not found interface descriptor */
static void const* _find_desc_entity(tusb_desc_vc_itf_t const *vc, unsigned entityid)
{
  void const *beg = (void const*)vc;
  void const *end = beg + vc->std.bLength + vc->ctl.bLength + vc->ctl.wTotalLength;
  for (void const *cur = beg; cur < end; cur = _find_desc(cur, end, TUSB_DESC_CS_INTERFACE)) {
    tusb_desc_cs_video_entity_itf_t const *itf = (tusb_desc_cs_video_entity_itf_t const *)cur;
    if ((VIDEO_CS_VC_INTERFACE_INPUT_TERMINAL  == itf->bDescriptorSubtype ||
         VIDEO_CS_VC_INTERFACE_OUTPUT_TERMINAL == itf->bDescriptorSubtype) &&
        itf->bEntityId == entityid) {
      return itf;
    }
    cur = tu_desc_next(cur);
  }
  return end;
}

static bool _close_vc_itf(uint8_t rhport, videod_interface_t *self)
{
  tusb_desc_vc_itf_t const *vc = _get_desc_vc(self);
  /* The next descriptor after the class-specific VC interface header descriptor. */
  void const *cur = (void const*)vc + vc->std.bLength + vc->ctl.bLength;
  /* The end of the video control interface descriptor. */
  void const *end = cur + vc->ctl.wTotalLength;
  if (vc->std.bNumEndpoints) {
    /* Find the notification endpoint descriptor. */
    cur = _find_desc(cur, end, TUSB_DESC_ENDPOINT);
    TU_ASSERT(cur < end);
    tusb_desc_endpoint_t const *notif = (tusb_desc_endpoint_t const *)cur;
    usbd_edpt_close(rhport, notif->bEndpointAddress);
  }
  self->ofs[0] = 0;
  return true;
}

static bool _open_vc_itf(uint8_t rhport, videod_interface_t *self, unsigned altnum)
{
  void const *beg = self->beg;
  void const *end = beg + self->len;
  /* The first descriptor is a video control interface descriptor. */
  unsigned itfnum = ((tusb_desc_interface_t const *)beg)->bInterfaceNumber;
  void const *cur = _find_desc_itf(beg, end, itfnum, altnum);
  TU_VERIFY(cur < end);

  tusb_desc_vc_itf_t const *vc = (tusb_desc_vc_itf_t const *)cur;
  /* Support for up to 2 streaming interfaces only. */
  TU_ASSERT(vc->ctl.bInCollection < 3);

  /* Advance to the next descriptor after the class-specific VC interface header descriptor. */
  cur += vc->std.bLength + vc->ctl.bLength;
  /* Update to point the end of the video control interface descriptor. */
  end  = cur + vc->ctl.wTotalLength;
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
  self->ofs[0] = (void const*)vc - beg;
  return true;
}

/** Set the specified alternate setting to own video control interface.
 *
 * @param[in,out] self     The context.
 * @param[in]     altnum   The target alternate setting number. */
static bool _set_vc_itf(uint8_t rhport, videod_interface_t *self, unsigned altnum)
{
  void const *beg = self->beg;
  void const *end = beg + self->len;
  /* The head descriptor is a video control interface descriptor. */
  unsigned itfnum = ((tusb_desc_interface_t const *)beg)->bInterfaceNumber;
  void const *cur = _find_desc_itf(beg, end, itfnum, altnum);
  TU_VERIFY(cur < end);

  tusb_desc_vc_itf_t const *vc = (tusb_desc_vc_itf_t const *)cur;
  /* Support for up to 2 streaming interfaces only. */
  TU_VERIFY(vc->ctl.bInCollection < 3);

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
    TU_VERIFY(1 == vc->std.bNumEndpoints);
    /* Find the notification endpoint descriptor. */
    cur = _find_desc(cur, end, TUSB_DESC_ENDPOINT);
    TU_VERIFY(cur < end);
    tusb_desc_endpoint_t const *notif = (tusb_desc_endpoint_t const *)cur;
    /* Open the notification endpoint */
    TU_ASSERT(usbd_edpt_open(rhport, notif));
    self->ep_notif = notif->bEndpointAddress;
  }
  self->ofs[0] = (void const*)vc - beg;
  return true;
}

/** Set the specified alternate setting to own video control interface.
 *
 * @param[in,out] self     The context.
 * @param[in]     itfnum   The target interface number. */
static bool _close_vs_itf(uint8_t rhport, videod_interface_t *self, unsigned itfnum)
{
  uint16_t *ofs = _get_desc_ofs(self, itfnum);
  if (!ofs) return true;
  tusb_desc_vs_itf_t const *vs = (tusb_desc_vs_itf_t const*)(self->beg + *ofs);
  /* The next of the video streaming interface header descriptor. */
  void const *cur = (void const*)vs + vs->std.bLength + vs->stm.bLength;
  /* The end of the video streaming interface descriptor. */
  void const *end = cur + vs->stm.wTotalLength;
  if (unsigned i = 0; i < vs->std.bNumEndpoints; ++i) {
    cur = _find_desc(cur, end, TUSB_DESC_ENDPOINT);
    TU_ASSERT(cur < end);
    tusb_desc_endpoint_t const *ep = (tusb_desc_endpoint_t const *)cur;
    usbd_edpt_close(rhport, ep->bEndpointAddress);
    cur += tu_desc_len(cur);
  }
  *ofs = 0;
  return true;
}

/** Set the specified alternate setting to own video control interface.
 *
 * @param[in,out] self     The context.
 * @param[in]     itfnum   The target interface number.
 * @param[in]     altnum   The target alternate setting number. */
static bool _open_vs_itf(uint8_t rhport, videod_interface_t *self, unsigned itfnum, unsigned altnum)
{
  uint16_t   *ofs = NULL;
  for (unsigned i = 1; i < sizeof(self->ofs)/sizeof(self->ofs[0]); ++i) {
    if (!self->ofs[i]) {
      ofs = &self->ofs[i];
      break;
    }
  }
  if (!ofs) return false;

  tusb_desc_vc_itf_t const *vc = _get_desc_vc(self);
  void const *end = self->beg + self->len;
  /* Set the end of the video control interface descriptor. */
  void const *cur = (void const*)vc + vc->std.bLength + vc->ctl.bLength + vc->ctl.wTotalLength;

  cur = _find_desc_itf(cur, end, itfnum, altnum);
  TU_VERIFY(cur < end);
  tusb_desc_vs_itf_t const *vs = (tusb_desc_vs_itf_t const*)cur;
  /* Support for up to 2 endpoint only. */
  TU_ASSERT(vs->std.bNumEndpoints < 3);
  /* Advance to the next descriptor after the class-specific VS interface header descriptor. */
  cur += vs->std.bLength + vs->stm.bLength;
  /* Update to point the end of the video control interface descriptor. */
  end  = cur + vs->stm.wTotalLength;
  for (unsigned i = 0; i < vs->std.bNumEndpoints; ++i) {
    cur = _find_desc(cur, end, TUSB_DESC_ENDPOINT);
    TU_VERIFY(cur < end);
    tusb_desc_endpoint_t const *ep = (tusb_desc_endpoint_t const *)cur;
    TU_ASSERT(usbd_edpt_open(rhport, ep));
    cur += tu_desc_len(cur);
  }
  *ofs = (void const*)vs - self->beg;
  return true;
}

/** Set the specified alternate setting to own video control interface.
 *
 * @param[in,out] self     The context.
 * @param[in]     itfnum   The target interface number.
 * @param[in]     altnum   The target alternate setting number. */
static bool _set_vs_itf(uint8_t rhport, videod_interface_t *self, unsigned itfnum, unsigned altnum)
{
  unsigned i;
  tusb_desc_vc_itf_t const *vc = _get_desc_vc(self);
  void const *end = self->beg + self->len;
  /* Set the end of the video control interface descriptor. */
  void const *cur = (void const*)vc + vc->std.bLength + vc->ctl.bLength + vc->ctl.wTotalLength;

  /* Check itfnum is valid */
  unsigned bInCollection = vc->ctl.bInCollection;
  for (i = 0; (i < bInCollection) && (vc->ctl.baInterfaceNr[i] != itfnum); ++i) ;
  TU_VERIFY(i < bInCollection);

  cur = _find_desc_itf(cur, end, itfnum, altnum);
  TU_VERIFY(cur < end);
  tusb_desc_vs_itf_t const *vs = (tusb_desc_vs_itf_t const*)cur;
  /* Advance to the next descriptor after the class-specific VS interface header descriptor. */
  cur += vs->std.bLength + vs->stm.bLength;
  /* Update to point the end of the video control interface descriptor. */
  end  = cur + vs->stm.wTotalLength;

  switch (vs->stm.bDescriptorSubType) {
  default: return false;
  case VIDEO_CS_VS_INTERFACE_INPUT_HEADER:
    /* Support for up to 2 endpoint only. */
    TU_VERIFY(vc->std.bNumEndpoints < 3);
    if (self->ep_sti) {
      usbd_edpt_close(rhport, self->ep_sti);
      self->ep_sti = 0;
    }
    if (self->ep_in) {
      usbd_edpt_close(rhport, self->ep_in);
      self->ep_in  = 0;
    }
    if (i = 0; i < vs->std.bNumEndpoints; ++i) {
      cur = _find_desc(cur, end, TUSB_DESC_ENDPOINT);
      TU_VERIFY(cur < end);
      tusb_desc_endpoint_t const *ep = (tusb_desc_endpoint_t const *)cur;
      if (vs->stm.bEndpointAddress == ep->bEndpointAddress) {
        /* video input endpoint */
        TU_ASSERT(!self->ep_in);
        TU_ASSERT(usbd_edpt_open(rhport, ep));
        self->ep_in  = ep->bEndpointAddress;
      } else {
        /* still image input endpoint */
        TU_ASSERT(!self->ep_sti);
        TU_ASSERT(usbd_edpt_open(rhport, ep));
        self->ep_sti = ep->bEndpointAddress;
      }
      cur += tu_desc_len(cur);
    }
    break;
  case VIDEO_CS_VS_INTERFACE_OUTPUT_HEADER:
    /* Support for up to 1 endpoint only. */
    TU_VERIFY(vc->std.bNumEndpoints < 2);
    if (self->ep_out) {
      usbd_edpt_close(rhport, self->ep_out);
      self->ep_out = 0;
    }
    if (vs->std.bNumEndpoints) {
      cur = _find_desc(cur, end, TUSB_DESC_ENDPOINT);
      TU_VERIFY(cur < end);
      tusb_desc_endpoint_t const *ep = (tusb_desc_endpoint_t const *)cur;
      if (vs->stm.bEndpointAddress == ep->bEndpointAddress) {
        /* video output endpoint */
        TU_ASSERT(usbd_edpt_open(rhport, ep));
        self->ep_out = ep->bEndpointAddress;
      }
    }
    break;
  }
  for (unsigned i = 1; i < sizeof(self->ofs)/sizeof(ofs[0]); ++i) {
    if (!self->ofs[i]) {
      return true;
    }
    tusb_desc_interface_t const* itf = (tusb_desc_interface_t const*)(beg + ofs[i]);
    if (itfnum == itf->bInterfaceNumber) return itf;
  }
  return NULL;
  for (i = 0; i < sizeof(self->vs)/sizeof(self->vs[0]); ++i) {
    if (!self->vs[i] || self->vs[i].stm.bInterfaceNumber == vs->stm.bInterfaceNumber) {
      self->ofs[i] = (void const*)vs - self->beg;
      return true;
    }
  }
  return false;
}

static int handle_video_ctl_std_req_get_itf(uint8_t rhport, uint8_t stage, tusb_control_request_t const *request, unsigned itf)
{
  if (stage != CONTROL_STAGE_SETUP)
    return VIDEO_NO_ERROR;
  videod_interface_t *self = &_videod_itf[itf];
  TU_VERIFY(1 == request->wLength, VIDEO_UNKNOWN);
  tusb_desc_interface_t const *p = _get_desc_cur_itf(self, itfnum);
  if (!p) return VIDEO_UNKNOWN;
  if (tud_control_xfer(rhport, request, &p->bAlternateSettings, sizeof(p->bAlternateSettings)))
    return VIDEO_NO_ERROR;
  return VIDEO_UNKNOWN;
}

/** Handle a standard request to the video control interface. */
static int handle_video_ctl_std_req(uint8_t rhport, uint8_t stage, tusb_control_request_t const *request, unsigned itf)
{
  switch (p_request->bRequest) {
  case TUSB_REQ_GET_INTERFACE:
    handle_video_ctl_std_req_get_itf(rhport, stage, request, itf);
  case TUSB_REQ_SET_INTERFACE:
    if (stage != CONTROL_STAGE_SETUP) return VIDEO_NO_ERROR;
    if (_set_vc_itf(rhport, &_videod_itf[itf], request->wValue))
      return VIDEO_NO_ERROR;
    return VIDEO_UNKNOWN;
  default: /* Unknown/Unsupported request */
    TU_BREAKPOINT();
    return VIDEO_INVALID_REQUEST;
  }
}

static int handle_video_ctl_cs_req(uint8_t rhport, uint8_t stage, tusb_control_request_t const *request, unsigned itf)
{
  videod_interface_t *self = &_videod_itf[i];
  /* 4.2.1 Interface Control Request */
  switch (TU_U16_HIGH(request->wValue)) {
  case VIDEO_VC_CTL_VIDEO_POWER_MODE:
    switch (p_request->bRequest) {
    case VIDEO_REQUEST_SET_CUR:
      if (stage == CONTROL_STAGE_SETUP) {
        TU_LOG2("  Set Power Mode\r\n");
        TU_VERIFY(1 == request->wLength, VIDEO_UNKNOWN);
        if (!tud_control_xfer(rhport, request, &self->power_mode, sizeof(self->power_mode)))
          return VIDEO_UNKNOWN;
      } else if (stage == CONTROL_STAGE_ACK) {
        if (tud_video_power_mode_cb) return tud_video_power_mode_cb(itf, &self->power_mode);
      }
      return VIDEO_NO_ERROR;
    case VIDEO_REQUEST_GET_CUR:
      if (stage == CONTROL_STAGE_SETUP) {
        TU_LOG2("  Get Power Mode\r\n");
        TU_VERIFY(1 == request->wLength, VIDEO_UNKNOWN);
        if (!tud_control_xfer(rhport, request, &self->power_mode, sizeof(self->power_mode)))
          return VIDEO_UNKNOWN;
      }
      return VIDEO_NO_ERROR;
    case VIDEO_REQUEST_GET_INFO:
      if (stage == CONTROL_STAGE_SETUP) {
        TU_LOG2("  Get Info Power Mode\r\n");
        TU_VERIFY(1 == request->wLength, VIDEO_UNKNOWN);
        if (!tud_control_xfer(rhport, request, &_cap_get_set, sizeof(_cap_get_set)))
          return VIDEO_UNKNOWN;
      }
      return VIDEO_NO_ERROR;
    default: break;
    }
    break;
  case VIDEO_VC_CTL_REQUEST_ERROR_CODE:
    switch (p_request->bRequest) {
    case VIDEO_REQUEST_GET_CUR:
      if (stage == CONTROL_STAGE_SETUP) {
        TU_LOG2("  Get Error Code\r\n");
        if (!tud_control_xfer(rhport, request, &self->error_code, sizeof(self->error_code)))
          return VIDEO_UNKNOWN;
      }
      return VIDEO_NO_ERROR;
    case VIDEO_REQUEST_GET_INFO:
      if (stage == CONTROL_STAGE_SETUP) {
        TU_LOG2("  Get Info Error Code\r\n");
        if (tud_control_xfer(rhport, request, &_cap_get, sizeof(_cap_get)))
          return VIDEO_UNKNOWN;
      }
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

static int handle_video_ctl_req(uint8_t rhport, uint8_t stage, tusb_control_request_t const *request, unsigned itf)
{
  switch (p_request->bmRequestType_bit.type) {
  case TUSB_REQ_TYPE_STANDARD:
    return handle_video_ctl_std_req(rhport, stage, request, itf);
  case TUSB_REQ_TYPE_CLASS:
    if (!TU_U16_HIGH(request->wIndex)) {
      return handle_video_ctl_cs_req(rhport, stage, request, itf);
    } else {
      /* TODO: */
      return VIDEO_INVALID_REQUEST;
    }
  default:
    return VIDEO_INVALID_REQUEST;
  }
}

static int handle_video_stm_std_req(uint8_t rhport, uint8_t stage, tusb_control_request_t const *request, unsigned itf)
{
  switch (p_request->bRequest) {
  case TUSB_REQ_GET_INTERFACE:
    handle_video_ctl_std_req_get_itf(rhport, stage, request, itf);
  case TUSB_REQ_SET_INTERFACE:
    videod_interface_t *self   = &_videod_itf[itf];
    unsigned            itfnum = tu_u16_low(p_request->wIndex);
    if (stage != CONTROL_STAGE_SETUP) return VIDEO_NO_ERROR;
    if (_set_vs_itf(rhport, self, itfnum, request->wValue))
      return VIDEO_NO_ERROR;
    return VIDEO_UNKNOWN;
  default: /* Unknown/Unsupported request */
    TU_BREAKPOINT();
    return VIDEO_INVALID_REQUEST;
  }
}

static int handle_video_stm_cs_req(uint8_t rhport, uint8_t stage, tusb_control_request_t const *request, unsigned itf)
{
  videod_interface_t *self = &_videod_itf[i];
  /* 4.2.1 Interface Control Request */
  switch (TU_U16_HIGH(request->wValue)) {
  case VIDEO_VS_CTL_PROBE:
    switch (p_request->bRequest) {
    case VIDEO_REQUEST_SET_CUR:
      if (stage == CONTROL_STAGE_SETUP) {
        TU_LOG2("  Set Power Mode\r\n");
        TU_VERIFY(1 == request->wLength, VIDEO_UNKNOWN);
        if (!tud_control_xfer(rhport, request, &self->power_mode, sizeof(self->power_mode)))
          return VIDEO_UNKNOWN;
      } else if (stage == CONTROL_STAGE_ACK) {
        if (tud_video_power_mode_cb) return tud_video_power_mode_cb(itf, &self->power_mode);
      }
      return VIDEO_NO_ERROR;
    case VIDEO_REQUEST_GET_CUR:
      if (stage == CONTROL_STAGE_SETUP) {
        TU_LOG2("  Get Power Mode\r\n");
        TU_VERIFY(1 == request->wLength, VIDEO_UNKNOWN);
        if (!tud_control_xfer(rhport, request, &self->power_mode, sizeof(self->power_mode)))
          return VIDEO_UNKNOWN;
      }
      return VIDEO_NO_ERROR;
    case VIDEO_REQUEST_GET_INFO:
      if (stage == CONTROL_STAGE_SETUP) {
        TU_LOG2("  Get Info Power Mode\r\n");
        TU_VERIFY(1 == request->wLength, VIDEO_UNKNOWN);
        if (!tud_control_xfer(rhport, request, &_cap_get_set, sizeof(_cap_get_set)))
          return VIDEO_UNKNOWN;
      }
      return VIDEO_NO_ERROR;
    default: break;
    }
    break;
  case VIDEO_VS_CTL_COMMIT:
    switch (p_request->bRequest) {
    case VIDEO_REQUEST_GET_CUR:
      if (stage == CONTROL_STAGE_SETUP) {
        TU_LOG2("  Get Error Code\r\n");
        if (!tud_control_xfer(rhport, request, &self->error_code, sizeof(self->error_code)))
          return VIDEO_UNKNOWN;
      }
      return VIDEO_NO_ERROR;
    case VIDEO_REQUEST_GET_INFO:
      if (stage == CONTROL_STAGE_SETUP) {
        TU_LOG2("  Get Info Error Code\r\n");
        if (tud_control_xfer(rhport, request, &_cap_get, sizeof(_cap_get)))
          return VIDEO_UNKNOWN;
      }
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

static int handle_video_stm_req(uint8_t rhport, uint8_t stage, tusb_control_request_t const *request, unsigned itf)
{
  switch (p_request->bmRequestType_bit.type) {
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

static void _prep_out_transaction(cdcd_interface_t* p_cdc)
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
  self->len = max_len;
  /*------------- Video Control Interface -------------*/
  if (!_open_vc_itf(rhport, self, 0)) return 0;
  tusb_desc_vc_itf_t const *vc = _get_desc_vc(self);
  unsigned bInCollection       = vc->ctl.bInCollection;
  /* Update end */
  void const *cur = _next_desc_itf(itf_desc, end);
  for (unsigned i = 0; i < bInCollection; ++i) {
    cur = _next_desc_itf(cur, end);
  }
  self->len = (uintptr_t)cur - (uintptr_t)itf_desc;
  /*------------- Video Stream Interface -------------*/
  unsigned itfnum = 0;
  for (unsigned i = 0; i < bInCollection; ++i) {
    itfnum = vc->ctl.baInterfaceNr[i];
    if (!_open_vs_itf(rhport, self, itfnum, 0)) return 0;
  }
  return end - cur;
}

// Invoked when a control transfer occurred on an interface of this class
// Driver response accordingly to the request and the transfer stage (setup/data/ack)
// return false to stall control endpoint (e.g unsupported request)
bool videod_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const * request)
{
  int err;
  if (p_request->bmRequestType_bit.recipient != TUSB_REQ_RCPT_INTERFACE) {
    return false;
  }
  unsigned itfnum = tu_u16_low(p_request->wIndex);
  /* Identify which interface to use */
  int itf;
  tusb_desc_vc_itf_t const *vc = NULL;
  for (itf = 0; itf < CFG_TUD_VIDEO; ++itf) {
    vc = _videod_itf[itf].vc;
    if (!vc) continue;
    unsigned beg_itfnum = vc->bInterfaceNumber;
    unsigned end_itfnum = vc->ctl.bInCollection;
    if (beg_itfnum <= itfnum && itfnum < end_itfnum)
      break;
  }
  if (itf == CFG_TUD_VIDEO) return false;

  if (itfnum == vc->bInterfaceNumber) {
    /* To video control interface */
    err = handle_video_ctl_req(rhport, stage, request, itf);
  } else {
    /* To video streaming interface */
    err = handle_video_stm_req(rhport, stage, request, itf);
  }
  _videod_itf[itf].error_code = (uint8_t)err;
  if (err) return false;
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
