/*
 * The MIT License (MIT)
 *
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

#ifndef TUSB_VIDEO_H_
#define TUSB_VIDEO_H_

#include "common/tusb_common.h"

/* 4.2.1.2 */
typedef enum {
  VIDEO_NO_ERROR = 0, /* The request succeeded. */
  VIDEO_NOT_READY,
  VIDEO_WRONG_STATE,
  VIDEO_POWER,
  VIDEO_OUT_OF_RANGE,
  VIDEO_INVALID_UNIT,
  VIDEO_INVALID_CONTROL,
  VIDEO_INVALID_REQUEST,
  VIDEO_INVALID_VALUE_WITHIN_RANGE,
  VIDEO_UNKNOWN = 0xFF,
} video_error_code_t;

/* A.2 */
typedef enum {
  VIDEO_SUBCLASS_UNDEFINED = 0x00,
  VIDEO_SUBCLASS_CONTROL,
  VIDEO_SUBCLASS_STREAMING,
  VIDEO_SUBCLASS_INTERFACE_COLLECTION,
} video_subclass_type_t;

/* A.3 */
typedef enum
{
  VIDEO_INT_PROTOCOL_CODE_UNDEF = 0x00,
  VIDEO_INT_PROTOCOL_CODE_15,
} video_interface_protocol_code_t;

/* A.5 */
typedef enum
{
  VIDEO_CS_VC_INTERFACE_VC_DESCRIPTOR_UNDEF = 0x00,
  VIDEO_CS_VC_INTERFACE_HEADER,
  VIDEO_CS_VC_INTERFACE_INPUT_TERMINAL,
  VIDEO_CS_VC_INTERFACE_OUTPUT_TERMINAL,
  VIDEO_CS_VC_INTERFACE_SELECTOR_UNIT,
  VIDEO_CS_VC_INTERFACE_PROCESSING_UNIT,
  VIDEO_CS_VC_INTERFACE_EXTENSION_UNIT,
  VIDEO_CS_VC_INTERFACE_ENCODING_UNIT,
} vide_cs_vc_interface_subtype_t;

/* A.6 */
typedef enum
{
  VIDEO_CS_VS_INTERFACE_VS_DESCRIPTOR_UNDEF = 0x00,
  VIDEO_CS_VS_INTERFACE_INPUT_HEADER,
  VIDEO_CS_VS_INTERFACE_OUTPUT_HEADER,
  VIDEO_CS_VS_INTERFACE_STILL_IMAGE_FRAME,
  VIDEO_CS_VS_INTERFACE_FORMAT_UNCOMPRESSED,
  VIDEO_CS_VS_INTERFACE_FRAME_UNCOMPRESSED,
  VIDEO_CS_VS_INTERFACE_FORMAT_MJPEG,
  VIDEO_CS_VS_INTERFACE_FRAME_MJPEG,
  VIDEO_CS_VS_INTERFACE_FORMAT_MPEG2TS      = 0x0A,
  VIDEO_CS_VS_INTERFACE_FORMAT_DV           = 0x0c,
  VIDEO_CS_VS_INTERFACE_COLORFORMAT,
  VIDEO_CS_VS_INTERFACE_FORMAT_FRAME_BASED  = 0x10,
  VIDEO_CS_VS_INTERFACE_FRAME_FRAME_BASED,
  VIDEO_CS_VS_INTERFACE_FORMAT_STREAM_BASED,
  VIDEO_CS_VS_INTERFACE_FORMAT_H264,
  VIDEO_CS_VS_INTERFACE_FRAME_H264,
  VIDEO_CS_VS_INTERFACE_FORMAT_H264_SIMULCAST,
  VIDEO_CS_VS_INTERFACE_FORMAT_VP8,
  VIDEO_CS_VS_INTERFACE_FRAME_VP8,
  VIDEO_CS_VS_INTERFACE_FORMAT_VP8_SIMULCAST,
} video_cs_vs_interface_subtype_t;

/* A.8 */
typedef enum
{
  VIDEO_REQUEST_UNDEF = 0x00,
  VIDEO_REQUEST_SET_CUR,
  VIDEO_REQUEST_SET_CUR_ALL = 0x11,
  VIDEO_REQUEST_GET = 0x80,
  VIDEO_REQUEST_GET_CUR,
  VIDEO_REQUEST_GET_MIN,
  VIDEO_REQUEST_GET_MAX,
  VIDEO_REQUEST_GET_RES,
  VIDEO_REQUEST_GET_LEN,
  VIDEO_REQUEST_GET_INFO,
  VIDEO_REQUEST_GET_DEF,
  VIDEO_REQUEST_GET_CUR_ALL = 0x91,
  VIDEO_REQUEST_GET_MIN_ALL,
  VIDEO_REQUEST_GET_MAX_ALL,
  VIDEO_REQUEST_GET_RES_ALL,
  VIDEO_REQUEST_GET_DEF_ALL = 0x97
} video_control_request_t;

/* A.9.1 */
typedef enum
{
  VIDEO_VC_CTL_UNDEF = 0x00,
  VIDEO_VC_CTL_VIDEO_POWER_MODE,
  VIDEO_VC_CTL_REQUEST_ERROR_CODE,
} video_interface_control_selector_t;

/* A.9.8 */
typedef enum
{
  VIDEO_VS_CTL_UNDEF = 0x00,
  VIDEO_VS_CTL_PROBE,
  VIDEO_VS_CTL_COMMIT,
  VIDEO_VS_CTL_STILL_PROBE,
  VIDEO_VS_CTL_STILL_COMMIT,
  VIDEO_VS_CTL_STILL_IMAGE_TRIGGER,
  VIDEO_VS_CTL_STREAM_ERROR_CODE,
  VIDEO_VS_CTL_GENERATE_KEY_FRAME,
  VIDEO_VS_CTL_UPDATE_FRAME_SEGMENT,
  VIDEO_VS_CTL_SYNCH_DELAY_CONTROL,
} video_interface_streaming_selector_t;

/* B. Terminal Types */
typedef enum
{
  VIDEO_TT_VENDOR_SPECIFIC         = 0x0100,
  VIDEO_TT_STREAMING,
  VIDEO_ITT_VENDOR_SPECIFIC        = 0x0200,
  VIDEO_ITT_CAMERA,
  VIDEO_ITT_MEDIA_TRANSPORT_INPUT,
  VIDEO_OTT_VENDOR_SPECIFIC        = 0x0300,
  VIDEO_OTT_DISPLAY,
  VIDEO_OTT_MEDIA_TRANSPORT_OUTPUT,
  VIDEO_TT_EXTERNAL_VENDOR_SPEIFIC = 0x0400,
  VIDEO_TT_COMPOSITE_CONNECTOR,
  VIDEO_TT_SVIDEO_CONNECTOR,
  VIDEO_TT_COMPONENT_CONNECTOR,
} video_terminal_type_t;

/* 2.3.4.2 */
typedef struct TU_ATTR_PACKED {
  uint8_t  bLength;
  uint8_t  bDescriptorType;
  uint8_t  bDescriptorSubType;
  uint16_t bcdUVC;
  uint16_t wTotalLength;
  uint32_t dwClockFrequency;
  uint8_t  bInCollection;
  uint8_t  baInterfaceNr[];
} tusb_desc_cs_video_ctl_itf_hdr_t;

/* 3.9.2.1 */
typedef struct TU_ATTR_PACKED {
  uint8_t  bLength;
  uint8_t  bDescriptorType;
  uint8_t  bDescriptorSubType;
  uint8_t  bNumFormats;
  uint16_t wTotalLength;
  uint8_t  bEndpointAddress;
  uint8_t  bmInfo;
  uint8_t  bTerminalLink;
  uint8_t  bStillCaptureMethod;
  uint8_t  bTriggerSupport;
  uint8_t  bTriggerUsage;
  uint8_t  bControlSize;
  uint8_t  bmaControls[];
} tusb_desc_cs_video_stm_itf_in_hdr_t;

/* 3.9.2.2 */
typedef struct TU_ATTR_PACKED {
  uint8_t  bLength;
  uint8_t  bDescriptorType;
  uint8_t  bDescriptorSubType;
  uint8_t  bNumFormats;
  uint16_t wTotalLength;
  uint8_t  bEndpointAddress;
  uint8_t  bTerminalLink;
  uint8_t  bControlSize;
  uint8_t  bmaControls[];
} tusb_desc_cs_video_stm_itf_out_hdr_t;

typedef struct TU_ATTR_PACKED {
  uint8_t  bLength;
  uint8_t  bDescriptorType;
  uint8_t  bDescriptorSubType;
  uint8_t  bNumFormats;
  uint16_t wTotalLength;
  uint8_t  bEndpointAddress;
  union {
    struct {
      uint8_t  bmInfo;
      uint8_t  bTerminalLink;
      uint8_t  bStillCaptureMethod;
      uint8_t  bTriggerSupport;
      uint8_t  bTriggerUsage;
      uint8_t  bControlSize;
      uint8_t  bmaControls[];
    } input;
    struct {
      uint8_t  bEndpointAddress;
      uint8_t  bTerminalLink;
      uint8_t  bControlSize;
      uint8_t  bmaControls[];
    } output;
  };
} tusb_desc_cs_video_stm_itf_hdr_t;

//--------------------------------------------------------------------+
// Requests
//--------------------------------------------------------------------+

/* 4.3.1.1 */
typedef struct TU_ATTR_PACKED {
  struct TU_ATTR_PACKEDt {
    uint16_t dwFrameInterval: 1;
    uint16_t wKeyFrameRatel : 1;
    uint16_t wPFrameRate    : 1;
    uint16_t wCompQuality   : 1;
    uint16_t wCompWindowSize: 1;
    uint16_t                : 0;
  } bmHint;
  uint8_t  bFormatIndex;
  uint8_t  bFrameIndex;
  uint32_t dwFrameInterval;
  uint16_t wKeyFrameRate;
  uint16_t wPFrameRate;
  uint16_t wCompQuality;
  uint16_t wCompWindowSize;
  uint16_t wDelay;
  uint32_t dwMaxVideoFrameSize;
  uint32_t dwMaxPayloadTransferSize;
  uint32_t dwClockFrequency;
  struct TU_ATTR_PACKED {
    uint8_t FrameID   : 1;
    uint8_t EndOfFrame: 1;
    uint8_t EndOfSlice: 1;
    uint8_t           : 0;
  } bmFramingInfo;
  uint8_t  bPreferedVersion;
  uint8_t  bMinVersion;
  uint8_t  bMaxVersion;
  uint8_t  bUsage;
  uint8_t  bBitDepthLum;
  uint8_t  bmSettings;
  uint8_t  bMaxNumberOfRefFramesPlus1;
  uint16_t bmRateControlModes;
  uint64_t bmLayoutPerStream;
} video_probe_and_commit_t;

#define TUD_VIDEO_DESC_IAD_LEN                    8
#define TUD_VIDEO_DESC_STD_VC_LEN                 9
#define TUD_VIDEO_DESC_CS_VC_LEN                  12
#define TUD_VIDEO_DESC_INPUT_TERM_LEN             8
#define TUD_VIDEO_DESC_OUTPUT_TERM_LEN            9
#define TUD_VIDEO_DESC_STD_VS_LEN                 9
#define TUD_VIDEO_DESC_CS_VS_IN_LEN               13
#define TUD_VIDEO_DESC_CS_VS_OUT_LEN              9
#define TUD_VIDEO_DESC_CS_VS_FMT_UNCOMPR_LEN      27
#define TUD_VIDEO_DESC_CS_VS_FRM_UNCOMPR_CONT_LEN 38
#define TUD_VIDEO_DESC_CS_VS_FRM_UNCOMPR_DISC_LEN 26

/* 2.2 compression formats */
#define TUD_VIDEO_GUID_YUY2   0x59,0x55,0x59,0x32,0x00,0x00,0x10,0x00,0x00,0x80,0x71,0x9B,0x38,0x00,0xAA,0x00
#define TUD_VIDEO_GUID_NV12   0x4E,0x56,0x31,0x32,0x00,0x00,0x10,0x00,0x00,0x80,0x71,0x9B,0x38,0x00,0xAA,0x00
#define TUD_VIDEO_GUID_M420   0x4D,0x34,0x32,0x30,0x00,0x00,0x10,0x00,0x00,0x80,0x71,0x9B,0x38,0x00,0xAA,0x00
#define TUD_VIDEO_GUID_I420   0x49,0x34,0x32,0x30,0x00,0x00,0x10,0x00,0x00,0x80,0x71,0x9B,0x38,0x00,0xAA,0x00

#define TUD_VIDEO_DESC_IAD(_firstitfs, _nitfs, _stridx) \
  TUD_VIDEO_DESC_IAD_LEN, TUSB_DESC_INTERFACE_ASSOCIATION, \
  _firstitfs, _nitfs, TUSB_CLASS_VIDEO, VIDEO_SUBCLASS_INTERFACE_COLLECTION, \
  VIDEO_INT_PROTOCOL_CODE_UNDEF, _stridx

#define TUD_VIDEO_DESC_STD_VC(_itfnum, _nEPs, _stridx) \
  TUD_VIDEO_DESC_STD_VC_LEN, TUSB_DESC_INTERFACE, _itfnum, /* fixed to zero */ 0x00, \
  _nEPs, TUSB_CLASS_VIDEO, VIDEO_SUBCLASS_CONTROL, VIDEO_INT_PROTOCOL_CODE_15, _stridx

/* 3.7.2 */
#define TUD_VIDEO_DESC_CS_VC(_bcdUVC, _totallen, _clkfreq, _coll, ...)	\
  TUD_VIDEO_DESC_CS_VC_LEN + _coll, TUSB_DESC_CS_INTERFACE, VIDEO_CS_VC_INTERFACE_HEADER, \
  U16_TO_U8S_LE(_bcdUVC), U16_TO_U8S_LE(_totallen + TUD_VIDEO_DESC_CS_VC_LEN), \
  U32_TO_U8S_LE(_clkfreq), _coll, __VA_ARGS__

/* 3.7.2.1 */
#define TUD_VIDEO_DESC_INPUT_TERM(_tid, _tt, _at, _stridx) \
  TUD_VIDEO_DESC_INPUT_TERM_LEN, TUSB_DESC_CS_INTERFACE, VIDEO_CS_VC_INTERFACE_INPUT_TERMINAL, \
    _tid, U16_TO_U8S_LE(_tt), _at, _stridx

/* 3.7.2.2 */
#define TUD_VIDEO_DESC_OUTPUT_TERM(_tid, _tt, _at, _srcid, _stridx) \
  TUD_VIDEO_DESC_INPUT_TERM_LEN, TUSB_DESC_CS_INTERFACE, VIDEO_CS_VC_INTERFACE_OUTPUT_TERMINAL, \
    _tid, U16_TO_U8S_LE(_tt), _at, _stridx

/* 3.9.1 */
#define TUD_VIDEO_DESC_STD_VS(_itfnum, _alt, _epn, _stridx)   \
  TUD_VIDEO_DESC_STD_VC_LEN, TUSB_DESC_INTERFACE, _itfnum, _alt, \
  _epn, TUSB_CLASS_VIDEO, VIDEO_SUBCLASS_STREAMING, VIDEO_INT_PROTOCOL_CODE_15, _stridx

/* 3.9.2.1 */
#define TUD_VIDEO_DESC_CS_VS_INPUT(_numfmt, _totlen, _epn, _inf, _termlnk, _sticaptmeth, _trgspt, _trgusg, _ctlsz, ...) \
  TUD_VIDEO_DESC_CS_VS_IN_LEN + (_numfmt) * (_ctlsz), TUSB_DESC_CS_INTERFACE, \
  VIDEO_CS_VS_INTERFACE_INPUT_HEADER, _numfmt, \
  U16_TO_U8S_LE(_totlen + TUD_VIDEO_DESC_CS_VS_IN_LEN + (_numfmt) * (_ctlsz)), \
  _epn, _inf, _termlnk, _sticaptmeth, _trgspt, _trgusg, _ctlsz, __VA_ARGS__

/* 3.9.2.2 */
#define TUD_VIDEO_DESC_CS_VS_OUTPUT(_numfmt, _totlen, _epn, _inf, _termlnk, _ctlsz, ...) \
  TUD_VIDEO_DESC_CS_VS_OUT_LEN + (_numfmt) * (_ctlsz), TUSB_DESC_CS_INTERFACE, \
  VIDEO_CS_VS_INTERFACE_OUTPUT_HEADER, _numfmt, \
  U16_TO_U8S_LE(_totlen + TUD_VIDEO_DESC_CS_VS_OUT_LEN + (_numfmt) * (_ctlsz)), \
  _epn, _inf, _termlnk,  _trgusg, _ctlsz, __VA_ARGS__

/* Uncompressed 3.1.1 */
#define TUD_VIDEO_GUID(_g0,_g1,_g2,_g3,_g4,_g5,_g6,_g7,_g8,_g9,_g10,_g11,_g12,_g13,_g14,_g15) _g0,_g1,_g2,_g3,_g4,_g5,_g6,_g7,_g8,_g9,_g10,_g11,_g12,_g13,_g14,_g15
#define TUD_VIDEO_DESC_CS_VS_FMT_UNCOMPR(_fmtidx, _numfrmdesc, \
  _guid, _bitsperpix, _frmidx, _asrx, _asry, _interlace, _cp) \
  TUD_VIDEO_DESC_CS_VS_FMT_UNCOMPR_LEN, TUSB_DESC_CS_INTERFACE, VIDEO_CS_VS_INTERFACE_FORMAT_UNCOMPRESSED, \
  _fmtidx, _numfrmdesc, TUD_VIDEO_GUID(_guid), \
  _bitsperpix, _frmidx, _asrx,  _asry, _interlace, _cp

//  _g0,_g1,_g2,_g3,_g4,_g5,_g6,_g7,_g8,_g9,_g10,_g11,_g12,_g13,_g14,_g15,

/* Uncompressed 3.1.2 Table 3-3 */
#define TUD_VIDEO_DESC_CS_VS_FRM_UNCOMPR_CONT(_frmidx, _cap, _width, _height, _minbr, _maxbr, _maxfrmbufsz, _frminterval, _minfrminterval, _maxfrminterval, _frmintervalstep) \
  TUD_VIDEO_DESC_CS_VS_FRM_UNCOMPR_CONT_LEN, TUSB_DESC_CS_INTERFACE, VIDEO_CS_VS_INTERFACE_FRAME_UNCOMPRESSED, \
  _frmidx, _cap, U16_TO_U8S_LE(_width), U16_TO_U8S_LE(_height), U32_TO_U8S_LE(_minbr), U32_TO_U8S_LE(_maxbr), \
  U32_TO_U8S_LE(_maxfrmbufsz), U32_TO_U8S_LE(_frminterval), 0, \
  U32_TO_U8S_LE(_minfrminterval), U32_TO_U8S_LE(_maxfrminterval), U32_TO_U8S_LE(_frmintervalstep)

/* Uncompressed 3.1.2 Table 3-4 */
#define TUD_VIDEO_DESC_CS_VS_FRM_UNCOMPR_DISC(_frmidx, _cap, _width, _height, _minbr, _maxbr, _maxfrmbufsz, _frminterval, _numfrminterval, ...) \
  TUD_VIDEO_DESC_CS_VS_FRM_UNCOMPR_DISC_LEN + _numfrminterval * 4, \
  TUSB_DESC_CS_INTERFACE, VIDEO_CS_VS_INTERFACE_FRAME_UNCOMPRESSED, \
  _frmidx, _cap, U16_TO_U8S_LE(_width), U16_TO_U8S_LE(_height), U32_TO_U8S_LE(_minbr), U32_TO_U8S_LE(_maxbr), \
  U32_TO_U8S_LE(_maxfrmbufsz), U32_TO_U8S_LE(_frminterval), _numfrminterval, __VA_ARGS__

/* 3.10.1.1 */
#define TUD_VIDEO_DESC_EP_ISO(_ep, _epsize, _ep_interval) \
  7, TUSB_DESC_ENDPOINT, _ep, TUSB_XFER_ISOCHRONOUS | TUSB_ISO_EP_ATT_ASYNCHRONOUS,\
  U16_TO_U8S_LE(_epsize), _ep_interval

/* 3.10.1.2 */
#define TUD_VIDEO_DESC_EP_BULK(_ep, _epsize, _ep_interval) \
  7, TUSB_DESC_ENDPOINT, _ep, TUSB_XFER_BULK, U16_TO_U8S_LE(_epsize), _ep_interval

#endif
