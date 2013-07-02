/**************************************************************************/
/*!
    @file     cdc_rndis.h
    @author   hathach (tinyusb.org)

    @section LICENSE

    Software License Agreement (BSD License)

    Copyright (c) 2013, hathach (tinyusb.org)
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:
    1. Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
    2. Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.
    3. Neither the name of the copyright holders nor the
    names of its contributors may be used to endorse or promote products
    derived from this software without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ''AS IS'' AND ANY
    EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
    WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
    DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE LIABLE FOR ANY
    DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
    (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
    LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
    ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
    SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

    This file is part of the tinyusb stack.
*/
/**************************************************************************/

/** \ingroup TBD
 *  \defgroup TBD
 *  \brief TBD
 *
 *  @{
 */

#ifndef _TUSB_CDC_RNDIS_H_
#define _TUSB_CDC_RNDIS_H_

#include "cdc.h"

#ifdef __cplusplus
 extern "C" {
#endif

typedef enum {
  RNDIS_MSG_PACKET           = 0x00000001UL,

  RNDIS_MSG_INITIALIZE       = 0x00000002UL,
  RNDIS_MSG_INITIALIZE_CMPLT = 0x80000002UL,

  RNDIS_MSG_HALT             = 0x00000003UL,

  RNDIS_MSG_QUERY            = 0x00000004UL,
  RNDIS_MSG_QUERY_CMPLT      = 0x80000004UL,

  RNDIS_MSG_SET              = 0x00000005UL,

  RNDIS_MSG_RESET            = 0x00000006UL,
  RNDIS_MSG_RESET_CMPLT      = 0x80000006UL,

  RNDIS_MSG_INDICATE_STATUS  = 0x00000007UL,

  RNDIS_MSG_KEEP_ALIVE       = 0x00000008UL,
  RNDIS_MSG_KEEP_ALIVE_CMPLT = 0x80000008UL
}rndis_msg_type_t;

typedef enum {
  RNDIS_STATUS_SUCCESS          = 0x00000000UL,
  RNDIS_STATUS_FAILURE          = 0xC0000001UL,
  RNDIS_STATUS_INVALID_DATA     = 0xC0010015UL,
  RNDIS_STATUS_NOT_SUPPORTED    = 0xC00000BBUL,
  RNDIS_STATUS_MEDIA_CONNECT    = 0x4001000BUL,
  RNDIS_STATUS_MEDIA_DISCONNECT = 0x4001000CUL
}rndis_msg_status_t;

//--------------------------------------------------------------------+
// MESSAGE STRUCTURE
//--------------------------------------------------------------------+
typedef struct {
  uint32_t type;
  uint32_t length;
  uint32_t request_id;
  uint32_t major_version;
  uint32_t minor_version;
  uint32_t max_xfer_size;
}rndis_msg_initialize_t;

typedef struct {
  uint32_t type;
  uint32_t length;
  uint32_t request_id;
  uint32_t oid;
  uint32_t buffer_length;
  uint32_t buffer_offset;
  uint32_t reserved;
  uint32_t oid_buffer[0];
} rndis_msg_query_t, rndis_msg_set_t;

typedef struct {
  uint32_t type;
  uint32_t length;
  uint32_t reserved;
} rndis_msg_reset_t;

//typedef struct {
//  uint32_t type;
//  uint32_t length;
//  uint32_t status;
//  uint32_t buffer_length;
//  uint32_t buffer_offset;
//  uint32_t diagnostic_status; // optional
//  uint32_t diagnostic_error_offset; // optional
//  uint32_t status_buffer[0]; // optional
//} rndis_msg_indicate_status_t;

typedef struct {
  uint32_t type;
  uint32_t length;
  uint32_t request_id;
} rndis_msg_keep_alive_t, rndis_msg_halt_t;

typedef struct {
  uint32_t type;
  uint32_t length;
  uint32_t request_id;
  uint32_t status;
  uint32_t major_version;
  uint32_t minor_version;
  uint32_t device_flags;
  uint32_t medium;
  uint32_t max_packet_per_xfer;
  uint32_t max_xfer_size;
  uint32_t packet_alignment_factor;
  uint32_t reserved[2];
} rndis_msg_initialize_cmplt_t;

typedef struct {
  uint32_t type;
  uint32_t length;
  uint32_t request_id;
  uint32_t status;
  uint32_t buffer_length;
  uint32_t buffer_offset;
  uint32_t oid_buffer[0];
} rndis_msg_query_cmplt_t;

typedef struct {
  uint32_t type;
  uint32_t length;
  uint32_t request_id;
  uint32_t status;
} rndis_msg_set_cmplt_t, rndis_msg_keep_alive_cmplt_t;

typedef struct {
  uint32_t type;
  uint32_t length;
  uint32_t status;
  uint32_t addressing_reset;
} rndis_msg_reset_cmplt_t;

typedef struct {
  uint32_t type;
  uint32_t length;
  uint32_t data_offset;
  uint32_t data_length;
  uint32_t out_of_band_data_offet;
  uint32_t out_of_band_data_length;
  uint32_t num_out_of_band_data_elements;
  uint32_t per_packet_info_offset;
  uint32_t per_packet_info_length;
  uint32_t reserved[2];
  uint32_t payload_and_padding[0]; // Additional bytes of zeros added at the end of the message to comply with
  // the internal and external padding requirements. Internal padding SHOULD be as per the
  // specification of the out-of-band data record and per-packet-info data record. The external
  //padding size SHOULD be determined based on the PacketAlignmentFactor field specification
  //in REMOTE_NDIS_INITIALIZE_CMPLT message by the device, when multiple
  //REMOTE_NDIS_PACKET_MSG messages are bundled together in a single bus-native message.
  //In this case, all but the very last REMOTE_NDIS_PACKET_MSG MUST respect the
  //PacketAlignmentFactor field.

  // rndis_msg_packet_t [0] : (optional) more packet if multiple packet per bus transaction is supported
} rndis_msg_packet_t;

typedef struct {
  uint32_t size;
  uint32_t type;
  uint32_t offset;
  uint32_t data[0];
} rndis_msg_out_of_band_data_t, rndis_msg_per_packet_info_t;

#ifdef __cplusplus
 }
#endif

#endif /* _TUSB_CDC_RNDIS_H_ */

/** @} */
