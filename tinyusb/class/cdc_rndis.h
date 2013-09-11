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
  RNDIS_MSG_SET_CMPLT        = 0x80000005UL,

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

//------------- Initialize -------------//
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
  uint32_t status;
  uint32_t major_version;
  uint32_t minor_version;
  uint32_t device_flags;
  uint32_t medium; // medium type, is 0x00 for RNDIS_MEDIUM_802_3
  uint32_t max_packet_per_xfer;
  uint32_t max_xfer_size;
  uint32_t packet_alignment_factor;
  uint32_t reserved[2];
} rndis_msg_initialize_cmplt_t;

//------------- Query -------------//
typedef struct {
  uint32_t type;
  uint32_t length;
  uint32_t request_id;
  uint32_t oid;
  uint32_t buffer_length;
  uint32_t buffer_offset; // from beginning of request_id field
  uint32_t reserved;
  uint8_t  oid_buffer[]; // flexible array member
} rndis_msg_query_t, rndis_msg_set_t;

STATIC_ASSERT(sizeof(rndis_msg_query_t) == 28, "Make sure flexible array member does not affect layout");

typedef struct {
  uint32_t type;
  uint32_t length;
  uint32_t request_id;
  uint32_t status;
  uint32_t buffer_length;
  uint32_t buffer_offset;
  uint8_t  oid_buffer[]; // flexible array member
} rndis_msg_query_cmplt_t;

STATIC_ASSERT(sizeof(rndis_msg_query_cmplt_t) == 24, "Make sure flexible array member does not affect layout");

//------------- Reset -------------//
typedef struct {
  uint32_t type;
  uint32_t length;
  uint32_t reserved;
} rndis_msg_reset_t;

typedef struct {
  uint32_t type;
  uint32_t length;
  uint32_t status;
  uint32_t addressing_reset;
} rndis_msg_reset_cmplt_t;

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
} rndis_msg_set_cmplt_t, rndis_msg_keep_alive_cmplt_t;

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

//--------------------------------------------------------------------+
// NDIS Object ID
//--------------------------------------------------------------------+

//------------- General Required OIDs -------------//
#define OID_GEN_SUPPORTED_LIST                  0x00010101
#define OID_GEN_HARDWARE_STATUS                 0x00010102
#define OID_GEN_MEDIA_SUPPORTED                 0x00010103
#define OID_GEN_MEDIA_IN_USE                    0x00010104
#define OID_GEN_MAXIMUM_LOOKAHEAD               0x00010105
#define OID_GEN_MAXIMUM_FRAME_SIZE              0x00010106
#define OID_GEN_LINK_SPEED                      0x00010107
#define OID_GEN_TRANSMIT_BUFFER_SPACE           0x00010108
#define OID_GEN_RECEIVE_BUFFER_SPACE            0x00010109
#define OID_GEN_TRANSMIT_BLOCK_SIZE             0x0001010A
#define OID_GEN_RECEIVE_BLOCK_SIZE              0x0001010B
#define OID_GEN_VENDOR_ID                       0x0001010C
#define OID_GEN_VENDOR_DESCRIPTION              0x0001010D
#define OID_GEN_CURRENT_PACKET_FILTER           0x0001010E
#define OID_GEN_CURRENT_LOOKAHEAD               0x0001010F
#define OID_GEN_DRIVER_VERSION                  0x00010110
#define OID_GEN_MAXIMUM_TOTAL_SIZE              0x00010111
#define OID_GEN_PROTOCOL_OPTIONS                0x00010112
#define OID_GEN_MAC_OPTIONS                     0x00010113
#define OID_GEN_MEDIA_CONNECT_STATUS            0x00010114
#define OID_GEN_MAXIMUM_SEND_PACKETS            0x00010115

//------------- General Optional OIDs -------------//
#define OID_GEN_VENDOR_DRIVER_VERSION           0x00010116
#define OID_GEN_SUPPORTED_GUIDS                 0x00010117
#define OID_GEN_NETWORK_LAYER_ADDRESSES         0x00010118  // Set only
#define OID_GEN_TRANSPORT_HEADER_OFFSET         0x00010119  // Set only
#define OID_GEN_MEDIA_CAPABILITIES              0x00010201
#define OID_GEN_PHYSICAL_MEDIUM                 0x00010202

//------------- 802.3 Objects (Ethernet) -------------//
#define OID_802_3_PERMANENT_ADDRESS             0x01010101
#define OID_802_3_CURRENT_ADDRESS               0x01010102
#define OID_802_3_MULTICAST_LIST                0x01010103
#define OID_802_3_MAXIMUM_LIST_SIZE             0x01010104

//
// Ndis Packet Filter Bits (OID_GEN_CURRENT_PACKET_FILTER).
//
#define NDIS_PACKET_TYPE_DIRECTED               0x00000001
#define NDIS_PACKET_TYPE_MULTICAST              0x00000002
#define NDIS_PACKET_TYPE_ALL_MULTICAST          0x00000004
#define NDIS_PACKET_TYPE_BROADCAST              0x00000008
#define NDIS_PACKET_TYPE_SOURCE_ROUTING         0x00000010
#define NDIS_PACKET_TYPE_PROMISCUOUS            0x00000020
#define NDIS_PACKET_TYPE_SMT                    0x00000040
#define NDIS_PACKET_TYPE_ALL_LOCAL              0x00000080
#define NDIS_PACKET_TYPE_GROUP                  0x00001000
#define NDIS_PACKET_TYPE_ALL_FUNCTIONAL         0x00002000
#define NDIS_PACKET_TYPE_FUNCTIONAL             0x00004000
#define NDIS_PACKET_TYPE_MAC_FRAME              0x00008000
#define NDIS_PACKET_TYPE_NO_LOCAL               0x00010000

#ifdef __cplusplus
 }
#endif

#endif /* _TUSB_CDC_RNDIS_H_ */

/** @} */
