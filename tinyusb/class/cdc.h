/**************************************************************************/
/*!
    @file     cdc.h
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
    INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
    LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION HOWEVER CAUSED AND
    ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    INCLUDING NEGLIGENCE OR OTHERWISE ARISING IN ANY WAY OUT OF THE USE OF THIS
    SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

    This file is part of the tinyusb stack.
*/
/**************************************************************************/

/** \file
 *  \brief CDC Class Driver
 *
 *  \note TBD
 */

/** 
 *  \addtogroup Group_ClassDriver Class Driver
 *  @{
 *  \defgroup Group_CDC Communication Device Class
 *  @{
 */

#ifndef _TUSB_CDC_H__
#define _TUSB_CDC_H__

#include "common/common.h"

#ifdef __cplusplus
 extern "C" {
#endif

//--------------------------------------------------------------------+
// CDC COMMUNICATION INTERFACE CLASS
//--------------------------------------------------------------------+
enum {
  CDC_COMM_SUBCLASS_DIRECT_LINE_CONTROL_MODEL = 0x01  ,
  CDC_COMM_SUBCLASS_ABSTRACT_CONTROL_MODEL            ,
  CDC_COMM_SUBCLASS_TELEPHONE_CONTROL_MODEL           ,
  CDC_COMM_SUBCLASS_MULTICHANNEL_CONTROL_MODEL        ,
  CDC_COMM_SUBCLASS_CAPI_CONTROL_MODEL                ,
  CDC_COMM_SUBCLASS_ETHERNET_NETWORKING_CONTROL_MODEL ,
  CDC_COMM_SUBCLASS_ATM_NETWORKING_CONTROL_MODEL      ,
  CDC_COMM_SUBCLASS_WIRELESS_HANDSET_CONTROL_MODEL    ,
  CDC_COMM_SUBCLASS_DEVICE_MANAGEMENT                 ,
  CDC_COMM_SUBCLASS_MOBILE_DIRECT_LINE_MODEL          ,
  CDC_COMM_SUBCLASS_OBEX                              ,
  CDC_COMM_SUBCLASS_ETHERNET_EMULATION_MODEL
};

enum {
  CDC_COMM_PROTOCOL_ATCOMMAND              = 0x01 , // ITU-T V2.50
  CDC_COMM_PROTOCOL_ATCOMMAND_PCCA_101            ,
  CDC_COMM_PROTOCOL_ATCOMMAND_PCCA_101_AND_ANNEXO ,
  CDC_COMM_PROTOCOL_ATCOMMAND_GSM_707             ,
  CDC_COMM_PROTOCOL_ATCOMMAND_3GPP_27007          ,
  CDC_COMM_PROTOCOL_ATCOMMAND_CDMA                , // defined by TIA
  CDC_COMM_PROTOCOL_ETHERNET_EMULATION_MODEL
};

//------------- SubType Descriptor in COMM Functional Descriptor -------------//
enum {
  CDC_FUNC_DESC_HEADER                                           = 0x00 ,
  CDC_FUNC_DESC_CALL_MANAGEMENT                                  = 0x01 ,
  CDC_FUNC_DESC_ABSTRACT_CONTROL_MANAGEMENT                      = 0x02 ,
  CDC_FUNC_DESC_DIRECT_LINE_MANAGEMENT                           = 0x03 ,
  CDC_FUNC_DESC_TELEPHONE_RINGER                                 = 0x04 ,
  CDC_FUNC_DESC_TELEPHONE_CALL_AND_LINE_STATE_REPORTING_CAPACITY = 0x05 ,
  CDC_FUNC_DESC_UNION                                            = 0x06 ,
  CDC_FUNC_DESC_COUNTRY_SELECTION                                = 0x07 ,
  CDC_FUNC_DESC_TELEPHONE_OPERATIONAL_MODES                      = 0x08 ,
  CDC_FUNC_DESC_USB_TERMINAL                                     = 0x09 ,
  CDC_FUNC_DESC_NETWORK_CHANNEL_TERMINAL                         = 0x0A ,
  CDC_FUNC_DESC_PROTOCOL_UNIT                                    = 0x0B ,
  CDC_FUNC_DESC_EXTENSION_UNIT                                   = 0x0C ,
  CDC_FUNC_DESC_MULTICHANEL_MANAGEMENT                           = 0x0D ,
  CDC_FUNC_DESC_CAPI_CONTROL_MANAGEMENT                          = 0x0E ,
  CDC_FUNC_DESC_ETHERNET_NETWORKING                              = 0x0F ,
  CDC_FUNC_DESC_ATM_NETWORKING                                   = 0x10 ,
  CDC_FUNC_DESC_WIRELESS_HANDSET_CONTROL_MODEL                   = 0x11 ,
  CDC_FUNC_DESC_MOBILE_DIRECT_LINE_MODEL                         = 0x12 ,
  CDC_FUNC_DESC_MOBILE_DIRECT_LINE_MODEL_DETAIL                  = 0x13 ,
  CDC_FUNC_DESC_DEVICE_MANAGEMENT_MODEL                          = 0x14 ,
  CDC_FUNC_DESC_OBEX                                             = 0x15 ,
  CDC_FUNC_DESC_COMMAND_SET                                      = 0x16 ,
  CDC_FUNC_DESC_COMMAND_SET_DETAIL                               = 0x17 ,
  CDC_FUNC_DESC_TELEPHONE_CONTROL_MODEL                          = 0x18 ,
  CDC_FUNC_DESC_OBEX_SERVICE_IDENTIFIER                          = 0x19
};

//--------------------------------------------------------------------+
// CDC DATA INTERFACE CLASS
//--------------------------------------------------------------------+

// SUBCLASS code of Data Interface is not used and should/must be zero

enum{
  CDC_DATA_PROTOCOL_ISDN_BRI                               = 0x30,
  CDC_DATA_PROTOCOL_HDLC                                   = 0x31,
  CDC_DATA_PROTOCOL_TRANSPARENT                            = 0x32,
  CDC_DATA_PROTOCOL_Q921_MANAGEMENT                        = 0x50,
  CDC_DATA_PROTOCOL_Q921_DATA_LINK                         = 0x51,
  CDC_DATA_PROTOCOL_Q921_TEI_MULTIPLEXOR                   = 0x52,
  CDC_DATA_PROTOCOL_V42BIS_DATA_COMPRESSION                = 0x90,
  CDC_DATA_PROTOCOL_EURO_ISDN                              = 0x91,
  CDC_DATA_PROTOCOL_V24_RATE_ADAPTION_TO_ISDN              = 0x92,
  CDC_DATA_PROTOCOL_CAPI_COMMAND                           = 0x93,
  CDC_DATA_PROTOCOL_HOST_BASED_DRIVER                      = 0xFD,
  CDC_DATA_PROTOCOL_IN_PROTOCOL_UNIT_FUNCTIONAL_DESCRIPTOR = 0xFE
};

//--------------------------------------------------------------------+
// MANAGEMENT ELEMENT REQUEST (CONTROL ENDPOINT)
//--------------------------------------------------------------------+
typedef enum {
  SEND_ENCAPSULATED_COMMAND                    = 0x00,
  GET_ENCAPSULATED_RESPONSE                    = 0x01,
  SET_COMM_FEATURE                             = 0x02,
  GET_COMM_FEATURE                             = 0x03,
  CLEAR_COMM_FEATURE                           = 0x04,

  SET_AUX_LINE_STATE                           = 0x10,
  SET_HOOK_STATE                               = 0x11,
  PULSE_SETUP                                  = 0x12,
  SEND_PULSE                                   = 0x13,
  SET_PULSE_TIME                               = 0x14,
  RING_AUX_JACK                                = 0x15,

  SET_LINE_CODING                              = 0x20,
  GET_LINE_CODING                              = 0x21,
  SET_CONTROL_LINE_STATE                       = 0x22,
  SEND_BREAK                                   = 0x23,

  SET_RINGER_PARMS                             = 0x30,
  GET_RINGER_PARMS                             = 0x31,
  SET_OPERATION_PARMS                          = 0x32,
  GET_OPERATION_PARMS                          = 0x33,
  SET_LINE_PARMS                               = 0x34,
  GET_LINE_PARMS                               = 0x35,
  DIAL_DIGITS                                  = 0x36,
  SET_UNIT_PARAMETER                           = 0x37,
  GET_UNIT_PARAMETER                           = 0x38,
  CLEAR_UNIT_PARAMETER                         = 0x39,
  GET_PROFILE                                  = 0x3A,

  SET_ETHERNET_MULTICAST_FILTERS               = 0x40,
  SET_ETHERNET_POWER_MANAGEMENT_PATTERN_FILTER = 0x41,
  GET_ETHERNET_POWER_MANAGEMENT_PATTERN_FILTER = 0x42,
  SET_ETHERNET_PACKET_FILTER                   = 0x43,
  GET_ETHERNET_STATISTIC                       = 0x44,

  SET_ATM_DATA_FORMAT                          = 0x50,
  GET_ATM_DEVICE_STATISTICS                    = 0x51,
  SET_ATM_DEFAULT_VC                           = 0x52,
  GET_ATM_VC_STATISTICS                        = 0x53,

  MDLM_SEMANTIC_MODEL                          = 0x60,
}tusb_cdc_management_request_t;

//--------------------------------------------------------------------+
// MANAGEMENT ELEMENENT NOTIFICATION (NOTIFICATION ENDPOINT)
//--------------------------------------------------------------------+
typedef enum {
  NETWORK_CONNECTION               = 0x00,
  RESPONSE_AVAILABLE               = 0x01,

  AUX_JACK_HOOK_STATE              = 0x08,
  RING_DETECT                      = 0x09,

  SERIAL_STATE                     = 0x20,

  CALL_STATE_CHANGE                = 0x28,
  LINE_STATE_CHANGE                = 0x29,
  CONNECTION_SPEED_CHANGE          = 0x2A,
  MDLM_SEMANTIC_MODEL_NOTIFICATION = 0x40,
}tusb_cdc_notification_t;

//--------------------------------------------------------------------+
// FUNCTIONAL DESCRIPTOR
//--------------------------------------------------------------------+
typedef ATTR_PACKED_STRUCT(struct) {
  uint8_t bLength            ; ///< Size of this descriptor in bytes.
  uint8_t bDescriptorType    ; ///< Descriptor Type, must be Class-Specific
  uint8_t bDescriptorSubType ; ///< Descriptor SubType one of above CDC_FUCN_DESC_
  uint16_t bcdCDC             ; ///< CDC release number in Binary-Coded Decimal
}tusb_cdc_func_header_t;

typedef ATTR_PACKED_STRUCT(struct) {
  uint8_t bLength                  ; ///< Size of this descriptor in bytes.
  uint8_t bDescriptorType          ; ///< Descriptor Type, must be Class-Specific
  uint8_t bDescriptorSubType       ; ///< Descriptor SubType one of above CDC_FUCN_DESC_
  uint8_t bControlInterface        ; ///< Interface number of Communication Interface
  uint8_t bSubordinateInterface    ; ///< Array of Interface number of Data Interface
}tusb_cdc_func_union_t;

#define tusb_cdc_func_union_n_t(no_slave)\
 ATTR_PACKED_STRUCT(struct) { \
  uint8_t bLength                         ;\
  uint8_t bDescriptorType                 ;\
  uint8_t bDescriptorSubType              ;\
  uint8_t bControlInterface               ;\
  uint8_t bSubordinateInterface[no_slave] ;\
}

typedef ATTR_PACKED_STRUCT(struct) {
  uint8_t bLength             ; ///< Size of this descriptor in bytes.
  uint8_t bDescriptorType     ; ///< Descriptor Type, must be Class-Specific
  uint8_t bDescriptorSubType  ; ///< Descriptor SubType one of above CDC_FUCN_DESC_
  uint8_t iCountryCodeRelDate ; ///< Index of a string giving the release date for the implemented ISO 3166 Country Codes.
  uint16_t wCountryCode[]     ; ///< Country code in the format as defined in [ISO3166], release date as specified inoffset 3 for the first supported country.
}tusb_cdc_func_country_selection_t;

#define tusb_cdc_func_country_selection_n_t(no_country) \
  ATTR_PACKED_STRUCT(struct) {\
  uint8_t bLength                   ;\
  uint8_t bDescriptorType           ;\
  uint8_t bDescriptorSubType        ;\
  uint8_t iCountryCodeRelDate       ;\
  uint16_t wCountryCode[no_country] ;\
}

//--------------------------------------------------------------------+
// PUBLIC SWITCHED TELEPHONE NETWORK (PSTN) SUBCLASS
//--------------------------------------------------------------------+
typedef ATTR_PACKED_STRUCT(struct) {
  uint8_t bLength            ; ///< Size of this descriptor in bytes.
  uint8_t bDescriptorType    ; ///< Descriptor Type, must be Class-Specific
  uint8_t bDescriptorSubType ; ///< Descriptor SubType one of above CDC_FUCN_DESC_

  struct {
    uint8_t handle_call    : 1; ///< 0 - Device sends/receives call management information only over the Communications Class interface. 1 - Device can send/receive call management information over a Data Class interface.
    uint8_t send_recv_call : 1; ///< 0 - Device does not handle call management itself. 1 - Device handles call management itself.
    uint8_t : 0;
  } bmCapabilities;

  uint8_t bDataInterface;
}tusb_cdc_func_call_management_t;

typedef struct {
  uint8_t support_comm_request                    : 1; ///< Device supports the request combination of Set_Comm_Feature, Clear_Comm_Feature, and Get_Comm_Feature.
  uint8_t support_line_request                    : 1; ///< Device supports the request combination of Set_Line_Coding, Set_Control_Line_State, Get_Line_Coding, and the notification Serial_State.
  uint8_t support_send_break                      : 1; ///< Device supports the request Send_Break
  uint8_t support_notification_network_connection : 1; ///< Device supports the notification Network_Connection.
  uint8_t : 0;
}cdc_fun_acm_capability_t;

STATIC_ASSERT(sizeof(cdc_fun_acm_capability_t) == 1, "mostly problem with compiler");

typedef ATTR_PACKED_STRUCT(struct) {
  uint8_t bLength                  ; ///< Size of this descriptor in bytes.
  uint8_t bDescriptorType          ; ///< Descriptor Type, must be Class-Specific
  uint8_t bDescriptorSubType       ; ///< Descriptor SubType one of above CDC_FUCN_DESC_
  cdc_fun_acm_capability_t bmCapabilities ;
}tusb_cdc_func_abstract_control_management_t;

typedef ATTR_PACKED_STRUCT(struct) {
  uint8_t bLength            ; ///< Size of this descriptor in bytes.
  uint8_t bDescriptorType    ; ///< Descriptor Type, must be Class-Specific
  uint8_t bDescriptorSubType ; ///< Descriptor SubType one of above CDC_FUCN_DESC_
  struct {
    uint8_t require_pulse_setup   : 1; ///< Device requires extra Pulse_Setup request during pulse dialing sequence to disengage holding circuit.
    uint8_t support_aux_request   : 1; ///< Device supports the request combination of Set_Aux_Line_State, Ring_Aux_Jack, and notification Aux_Jack_Hook_State.
    uint8_t support_pulse_request : 1; ///< Device supports the request combination of Pulse_Setup, Send_Pulse, and Set_Pulse_Time.
    uint8_t : 0;
  } bmCapabilities;
}tusb_cdc_func_direct_line_management_t;

typedef ATTR_PACKED_STRUCT(struct) {
  uint8_t bLength            ; ///< Size of this descriptor in bytes.
  uint8_t bDescriptorType    ; ///< Descriptor Type, must be Class-Specific
  uint8_t bDescriptorSubType ; ///< Descriptor SubType one of above CDC_FUCN_DESC_
  uint8_t bRingerVolSteps    ;
  uint8_t bNumRingerPatterns ;
}tusb_cdc_func_telephone_ringer_t;

typedef ATTR_PACKED_STRUCT(struct) {
  uint8_t bLength            ; ///< Size of this descriptor in bytes.
  uint8_t bDescriptorType    ; ///< Descriptor Type, must be Class-Specific
  uint8_t bDescriptorSubType ; ///< Descriptor SubType one of above CDC_FUCN_DESC_
  struct {
    uint8_t simple_mode           : 1;
    uint8_t standalone_mode       : 1;
    uint8_t computer_centric_mode : 1;
    uint8_t : 0;
  } bmCapabilities;
}tusb_cdc_func_telephone_operational_modes_t;

typedef ATTR_PACKED_STRUCT(struct) {
  uint8_t bLength            ; ///< Size of this descriptor in bytes.
  uint8_t bDescriptorType    ; ///< Descriptor Type, must be Class-Specific
  uint8_t bDescriptorSubType ; ///< Descriptor SubType one of above CDC_FUCN_DESC_
  struct {
    uint32_t interrupted_dialtone   : 1; ///< 0 : Reports only dialtone (does not differentiate between normal and interrupted dialtone). 1 : Reports interrupted dialtone in addition to normal dialtone
    uint32_t ringback_busy_fastbusy : 1; ///< 0 : Reports only dialing state. 1 : Reports ringback, busy, and fast busy states.
    uint32_t caller_id              : 1; ///< 0 : Does not report caller ID. 1 : Reports caller ID information.
    uint32_t incoming_distinctive   : 1; ///< 0 : Reports only incoming ringing. 1 : Reports incoming distinctive ringing patterns.
    uint32_t dual_tone_multi_freq   : 1; ///< 0 : Cannot report dual tone multi-frequency (DTMF) digits input remotely over the telephone line. 1 : Can report DTMF digits input remotely over the telephone line.
    uint32_t line_state_change      : 1; ///< 0 : Does not support line state change notification. 1 : Does support line state change notification
    uint32_t : 0;
  } bmCapabilities;
}tusb_cdc_func_telephone_call_state_reporting_capabilities_t;

static inline uint8_t functional_desc_typeof(uint8_t const * p_desc) ATTR_PURE ATTR_ALWAYS_INLINE;
static inline uint8_t functional_desc_typeof(uint8_t const * p_desc)
{
  return p_desc[2];
}


#ifdef __cplusplus
 }
#endif

#endif
