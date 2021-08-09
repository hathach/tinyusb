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

#define ITF_MEM_RESET_SIZE   offsetof(videod_interface_t, ctl_buf)

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

static tusb_desc_vs_itf_t const *_get_desc_vs(videod_interface_t const *self, unsigned itfnum)
{
  uint16_t const *ofs = _get_desc_ofs((videod_interface_t*)self, itfnum);
  if (!ofs) return NULL;
  return (tusb_desc_vs_itf_t const*)(self->beg + *ofs);
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
  void const *end = beg + vc->std.bLength + vc->ctl.wTotalLength;
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
  self->ofs[0] = 0;
  return true;
}

/** Set the specified alternate setting to own video control interface.
 *
 * @param[in,out] self     The context.
 * @param[in]     altnum   The target alternate setting number. */
static bool _open_vc_itf(uint8_t rhport, videod_interface_t *self, unsigned altnum)
{
  TU_LOG2("    open VC %d\r\n", altnum);
  void const *beg = self->beg;
  void const *end = beg + self->len;
  /* The first descriptor is a video control interface descriptor. */
  unsigned itfnum = ((tusb_desc_interface_t const *)beg)->bInterfaceNumber;
  void const *cur = _find_desc_itf(beg, end, itfnum, altnum);
  TU_LOG2("    cur %ld\r\n", cur - beg);
  TU_VERIFY(cur < end);

  tusb_desc_vc_itf_t const *vc = (tusb_desc_vc_itf_t const *)cur;
  TU_LOG2("    bInCollection %d\r\n", vc->ctl.bInCollection);
  /* Support for up to 2 streaming interfaces only. */
  TU_ASSERT(vc->ctl.bInCollection < 3);

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
  void const *end = (void const*)vs + vs->std.bLength + vs->stm.wTotalLength;
  for (unsigned i = 0; i < vs->std.bNumEndpoints; ++i) {
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
  TU_LOG2("    open VS %d,%d\r\n", itfnum, altnum);
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
  void const *cur = (void const*)vc + vc->std.bLength + vc->ctl.wTotalLength;

  cur = _find_desc_itf(cur, end, itfnum, altnum);
  TU_VERIFY(cur < end);
  tusb_desc_vs_itf_t const *vs = (tusb_desc_vs_itf_t const*)cur;
  /* Support for up to 2 endpoint only. */
  TU_ASSERT(vs->std.bNumEndpoints < 3);
  /* Update to point the end of the video control interface descriptor. */
  end  = cur + vs->std.bLength + vs->stm.wTotalLength;
  /* Advance to the next descriptor after the class-specific VS interface header descriptor. */
  cur += vs->std.bLength + vs->stm.bLength;
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

/** Handle a standard request to the video control interface. */
static int handle_video_ctl_std_req(uint8_t rhport, uint8_t stage, tusb_control_request_t const *request, unsigned itf)
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

static int handle_video_ctl_cs_req(uint8_t rhport, uint8_t stage, tusb_control_request_t const *request, unsigned itf)
{
  videod_interface_t *self = &_videod_itf[itf];
  /* 4.2.1 Interface Control Request */
  switch (TU_U16_HIGH(request->wValue)) {
  case VIDEO_VC_CTL_VIDEO_POWER_MODE:
    switch (request->bRequest) {
    case VIDEO_REQUEST_SET_CUR:
      if (stage == CONTROL_STAGE_SETUP) {
        TU_LOG2("  Set Power Mode\r\n");
        TU_VERIFY(1 == request->wLength, VIDEO_UNKNOWN);
        if (!tud_control_xfer(rhport, request, &self->power_mode, sizeof(self->power_mode)))
          return VIDEO_UNKNOWN;
      } else if (stage == CONTROL_STAGE_ACK) {
        if (tud_video_power_mode_cb) return tud_video_power_mode_cb(itf, self->power_mode);
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
        if (!tud_control_xfer(rhport, request, (uint8_t*)&_cap_get_set, sizeof(_cap_get_set)))
          return VIDEO_UNKNOWN;
      }
      return VIDEO_NO_ERROR;
    default: break;
    }
    break;
  case VIDEO_VC_CTL_REQUEST_ERROR_CODE:
    switch (request->bRequest) {
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
        if (tud_control_xfer(rhport, request, (uint8_t*)&_cap_get, sizeof(_cap_get)))
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
  unsigned entity_id;
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

static int handle_video_stm_std_req(uint8_t rhport, uint8_t stage, tusb_control_request_t const *request, unsigned itf)
{
  unsigned itfnum;
  switch (request->bRequest) {
  case TUSB_REQ_GET_INTERFACE:
    if (stage != CONTROL_STAGE_SETUP) return VIDEO_NO_ERROR;
    TU_VERIFY(1 == request->wLength, VIDEO_UNKNOWN);
    itfnum = tu_u16_low(request->wIndex);
    tusb_desc_vs_itf_t const *vs = _get_desc_vs(&_videod_itf[itf], itfnum);
    if (!vs) return VIDEO_UNKNOWN;
    if (tud_control_xfer(rhport, request,
			 (void*)&vs->std.bAlternateSetting,
			 sizeof(vs->std.bAlternateSetting)))
      return VIDEO_NO_ERROR;
    return VIDEO_UNKNOWN;
  case TUSB_REQ_SET_INTERFACE:
    if (stage != CONTROL_STAGE_SETUP) return VIDEO_NO_ERROR;
    itfnum = tu_u16_low(request->wIndex);
    if (!_close_vs_itf(rhport, &_videod_itf[itf], itfnum))
      return VIDEO_UNKNOWN;
    if (!_open_vs_itf(rhport, &_videod_itf[itf], itfnum, request->wValue))
      return VIDEO_UNKNOWN;
    tud_control_status(rhport, request);
    return VIDEO_NO_ERROR;
  default: /* Unknown/Unsupported request */
    TU_BREAKPOINT();
    return VIDEO_INVALID_REQUEST;
  }
}

static int handle_video_stm_cs_req(uint8_t rhport, uint8_t stage, tusb_control_request_t const *request, unsigned itf)
{
  //  videod_interface_t *self = &_videod_itf[itf];
  (void)rhport; (void)stage; (void)itf;
  /* 4.2.1 Interface Control Request */
  switch (TU_U16_HIGH(request->wValue)) {
  case VIDEO_VS_CTL_PROBE:
    TU_LOG2("  Probe ");
    switch (request->bRequest) {
    case VIDEO_REQUEST_SET_CUR:
      TU_LOG2("Set\r\n");
      return VIDEO_UNKNOWN;
    case VIDEO_REQUEST_GET_CUR:
      TU_LOG2("Get\r\n");
      return VIDEO_UNKNOWN;
    case VIDEO_REQUEST_GET_MIN:
      TU_LOG2("Get Min\r\n");
      return VIDEO_UNKNOWN;
    case VIDEO_REQUEST_GET_MAX:
      TU_LOG2("Get Man\r\n");
      return VIDEO_UNKNOWN;
    case VIDEO_REQUEST_GET_RES:
      TU_LOG2("Get Res\r\n");
      return VIDEO_UNKNOWN;
    case VIDEO_REQUEST_GET_DEF:
      TU_LOG2("Get Def\r\n");
      return VIDEO_UNKNOWN;
    case VIDEO_REQUEST_GET_LEN:
      TU_LOG2("Get Len\r\n");
      return VIDEO_UNKNOWN;
    case VIDEO_REQUEST_GET_INFO:
      TU_LOG2("Get Info\r\n");
      return VIDEO_UNKNOWN;
    default: break;
    }
    break;
  case VIDEO_VS_CTL_COMMIT:
    TU_LOG2("  Commit ");
    switch (request->bRequest) {
    case VIDEO_REQUEST_SET_CUR:
      TU_LOG2("Set\r\n");
      return VIDEO_UNKNOWN;
    case VIDEO_REQUEST_GET_CUR:
      TU_LOG2("Get\r\n");
      return VIDEO_UNKNOWN;
    case VIDEO_REQUEST_GET_INFO:
      TU_LOG2("Get Info\r\n");
      return VIDEO_UNKNOWN;
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
bool tud_video_n_connected(uint8_t itf)
{
  (void)itf;
  // DTR (bit 0) active  is considered as connected
  return tud_ready();
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
  for (unsigned i = 0; i < CFG_TUD_VIDEO; ++i) {
    if (!_videod_itf[i].beg) {
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
  return (uintptr_t)cur - (uintptr_t)itf_desc;
}

// Invoked when a control transfer occurred on an interface of this class
// Driver response accordingly to the request and the transfer stage (setup/data/ack)
// return false to stall control endpoint (e.g unsupported request)
bool videod_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const * request)
{
  int err;
  int (*handle_video_req)(uint8_t rhport, uint8_t stage, tusb_control_request_t const *request, unsigned itf) = NULL;
  if (request->bmRequestType_bit.recipient != TUSB_REQ_RCPT_INTERFACE) {
    return false;
  }
  unsigned itfnum = tu_u16_low(request->wIndex);
  /* Identify which interface to use */
  int itf;
  tusb_desc_vc_itf_t const *vc = NULL;
  for (itf = 0; itf < CFG_TUD_VIDEO; ++itf) {
    vc = _get_desc_vc(&_videod_itf[itf]);
    if (itfnum == vc->std.bInterfaceNumber) {
      handle_video_req = handle_video_ctl_req;
      break;
    }
    int i;
    int bInCollection = vc->ctl.bInCollection;
    for (i = 0; i < bInCollection && itfnum != vc->ctl.baInterfaceNr[i]; ++i) ;
    if (i < bInCollection) {
      handle_video_req = handle_video_stm_req;
      break;
    }
  }
  if (itf == CFG_TUD_VIDEO) return false;

  err = handle_video_req(rhport, stage, request, itf);
  _videod_itf[itf].error_code = (uint8_t)err;
  if (err) return false;
  return true;
}

bool videod_xfer_cb(uint8_t rhport, uint8_t ep_addr, xfer_result_t result, uint32_t xferred_bytes)
{
  (void)rhport; (void)ep_addr; (void)result; (void)xferred_bytes;
  return false;
}

#endif
