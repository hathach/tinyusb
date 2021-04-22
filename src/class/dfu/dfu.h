/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2021 XMOS LIMITED
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

#ifndef _TUSB_DFU_H_
#define _TUSB_DFU_H_

#include "common/tusb_common.h"

#ifdef __cplusplus
  extern "C" {
#endif

//--------------------------------------------------------------------+
// Common Definitions
//--------------------------------------------------------------------+
// DFU Protocol
typedef enum
{
  DFU_PROTOCOL_RT  = 0x01,
  DFU_PROTOCOL_DFU = 0x02,
} dfu_protocol_type_t;

// DFU Descriptor Type
typedef enum
{
  DFU_DESC_FUNCTIONAL = 0x21,
} dfu_descriptor_type_t;

// DFU Requests
typedef enum {
  DFU_REQUEST_DETACH         = 0,
  DFU_REQUEST_DNLOAD         = 1,
  DFU_REQUEST_UPLOAD         = 2,
  DFU_REQUEST_GETSTATUS      = 3,
  DFU_REQUEST_CLRSTATUS      = 4,
  DFU_REQUEST_GETSTATE       = 5,
  DFU_REQUEST_ABORT          = 6,
} dfu_requests_t;

// DFU States
typedef enum {
  APP_IDLE                   = 0,
  APP_DETACH                 = 1,
  DFU_IDLE                   = 2,
  DFU_DNLOAD_SYNC            = 3,
  DFU_DNBUSY                 = 4,
  DFU_DNLOAD_IDLE            = 5,
  DFU_MANIFEST_SYNC          = 6,
  DFU_MANIFEST               = 7,
  DFU_MANIFEST_WAIT_RESET    = 8,
  DFU_UPLOAD_IDLE            = 9,
  DFU_ERROR                  = 10,
} dfu_state_t;

// DFU Status
typedef enum {
  DFU_STATUS_OK              = 0x00,
  DFU_STATUS_ERRTARGET       = 0x01,
  DFU_STATUS_ERRFILE         = 0x02,
  DFU_STATUS_ERRWRITE        = 0x03,
  DFU_STATUS_ERRERASE        = 0x04,
  DFU_STATUS_ERRCHECK_ERASED = 0x05,
  DFU_STATUS_ERRPROG         = 0x06,
  DFU_STATUS_ERRVERIFY       = 0x07,
  DFU_STATUS_ERRADDRESS      = 0x08,
  DFU_STATUS_ERRNOTDONE      = 0x09,
  DFU_STATUS_ERRFIRMWARE     = 0x0A,
  DFU_STATUS_ERRVENDOR       = 0x0B,
  DFU_STATUS_ERRUSBR         = 0x0C,
  DFU_STATUS_ERRPOR          = 0x0D,
  DFU_STATUS_ERRUNKNOWN      = 0x0E,
  DFU_STATUS_ERRSTALLEDPKT   = 0x0F,
} dfu_device_status_t;

#define DFU_FUNC_ATTR_CAN_DOWNLOAD_BITMASK              (1 << 0)
#define DFU_FUNC_ATTR_CAN_UPLOAD_BITMASK                (1 << 1)
#define DFU_FUNC_ATTR_MANIFESTATION_TOLERANT_BITMASK    (1 << 2)
#define DFU_FUNC_ATTR_WILL_DETACH_BITMASK               (1 << 3)

// DFU Status Request Payload
typedef struct TU_ATTR_PACKED
{
  uint8_t bStatus;
  uint8_t bwPollTimeout[3];
  uint8_t bState;
  uint8_t iString;
} dfu_status_req_payload_t;

TU_VERIFY_STATIC( sizeof(dfu_status_req_payload_t) == 6, "size is not correct");


//--------------------------------------------------------------------+
// Debug
//--------------------------------------------------------------------+
#if CFG_TUSB_DEBUG >= 2

static tu_lookup_entry_t const _dfu_request_lookup[] =
{
  { .key = DFU_REQUEST_DETACH         , .data = "DETACH" },
  { .key = DFU_REQUEST_DNLOAD         , .data = "DNLOAD" },
  { .key = DFU_REQUEST_UPLOAD         , .data = "UPLOAD" },
  { .key = DFU_REQUEST_GETSTATUS      , .data = "GETSTATUS" },
  { .key = DFU_REQUEST_CLRSTATUS      , .data = "CLRSTATUS" },
  { .key = DFU_REQUEST_GETSTATE       , .data = "GETSTATE" },
  { .key = DFU_REQUEST_ABORT          , .data = "ABORT" },
};

static tu_lookup_table_t const _dfu_request_table =
{
  .count = TU_ARRAY_SIZE(_dfu_request_lookup),
  .items = _dfu_request_lookup
};

static tu_lookup_entry_t const _dfu_state_lookup[] =
{
  { .key = APP_IDLE                   , .data = "APP_IDLE" },
  { .key = APP_DETACH                 , .data = "APP_DETACH" },
  { .key = DFU_IDLE                   , .data = "DFU_IDLE" },
  { .key = DFU_DNLOAD_SYNC            , .data = "DFU_DNLOAD_SYNC" },
  { .key = DFU_DNBUSY                 , .data = "DFU_DNBUSY" },
  { .key = DFU_DNLOAD_IDLE            , .data = "DFU_DNLOAD_IDLE" },
  { .key = DFU_MANIFEST_SYNC          , .data = "DFU_MANIFEST_SYNC" },
  { .key = DFU_MANIFEST               , .data = "DFU_MANIFEST" },
  { .key = DFU_MANIFEST_WAIT_RESET    , .data = "DFU_MANIFEST_WAIT_RESET" },
  { .key = DFU_UPLOAD_IDLE            , .data = "DFU_UPLOAD_IDLE" },
  { .key = DFU_ERROR                  , .data = "DFU_ERROR" },
};

static tu_lookup_table_t const _dfu_state_table =
{
  .count = TU_ARRAY_SIZE(_dfu_state_lookup),
  .items = _dfu_state_lookup
};

static tu_lookup_entry_t const _dfu_status_lookup[] =
{
  { .key = DFU_STATUS_OK              , .data = "OK" },
  { .key = DFU_STATUS_ERRTARGET       , .data = "errTARGET" },
  { .key = DFU_STATUS_ERRFILE         , .data = "errFILE" },
  { .key = DFU_STATUS_ERRWRITE        , .data = "errWRITE" },
  { .key = DFU_STATUS_ERRERASE        , .data = "errERASE" },
  { .key = DFU_STATUS_ERRCHECK_ERASED , .data = "errCHECK_ERASED" },
  { .key = DFU_STATUS_ERRPROG         , .data = "errPROG" },
  { .key = DFU_STATUS_ERRVERIFY       , .data = "errVERIFY" },
  { .key = DFU_STATUS_ERRADDRESS      , .data = "errADDRESS" },
  { .key = DFU_STATUS_ERRNOTDONE      , .data = "errNOTDONE" },
  { .key = DFU_STATUS_ERRFIRMWARE     , .data = "errFIRMWARE" },
  { .key = DFU_STATUS_ERRVENDOR       , .data = "errVENDOR" },
  { .key = DFU_STATUS_ERRUSBR         , .data = "errUSBR" },
  { .key = DFU_STATUS_ERRPOR          , .data = "errPOR" },
  { .key = DFU_STATUS_ERRUNKNOWN      , .data = "errUNKNOWN" },
  { .key = DFU_STATUS_ERRSTALLEDPKT   , .data = "errSTALLEDPKT" },
};

static tu_lookup_table_t const _dfu_status_table =
{
  .count = TU_ARRAY_SIZE(_dfu_status_lookup),
  .items = _dfu_status_lookup
};

#endif

#define dfu_debug_print_context()                                              \
{                                                                              \
  TU_LOG2("  DFU at State: %s\r\n         Status: %s\r\n",                     \
          tu_lookup_find(&_dfu_state_table, _dfu_state_ctx.state),        \
          tu_lookup_find(&_dfu_status_table, _dfu_state_ctx.status) );    \
}


#ifdef __cplusplus
 }
#endif

#endif /* _TUSB_DFU_H_ */
