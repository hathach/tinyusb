/**************************************************************************/
/*!
    @file     dcd_lpc175x_6x.h
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

#ifndef _TUSB_DCD_LPC175X_6X_H_
#define _TUSB_DCD_LPC175X_6X_H_

#include "common/common.h"

#ifdef __cplusplus
 extern "C" {
#endif


typedef struct
{
	//------------- Word 0 -------------//
	uint32_t next;

	//------------- Word 1 -------------//
	uint16_t mode            : 2; // either 00 normal or 01 ATLE(auto length extraction)
	uint16_t is_next_valid   : 1;
	uint16_t                 : 1;
	uint16_t is_isochronous  : 1; // is an iso endpoint
	uint16_t max_packet_size : 11;
	volatile uint16_t buffer_length;

	//------------- Word 2 -------------//
	volatile uint32_t buffer_start_addr;

	//------------- Word 3 -------------//
	volatile uint16_t is_retired                   : 1; // initialized to zero
	volatile uint16_t status                       : 4;
	volatile uint16_t iso_last_packet_valid        : 1;
	volatile uint16_t atle_is_lsb_extracted        : 1;	// used in ATLE mode
	volatile uint16_t atle_is_msb_extracted        : 1;	// used in ATLE mode
	volatile uint16_t atle_message_length_position : 6; // used in ATLE mode
	uint16_t                                       : 2;
	volatile uint16_t present_count; // The number of bytes transferred by the DMA engine. The DMA engine updates this field after completing each packet transfer.

	//------------- Word 4 -------------//
//	uint32_t iso_packet_size_addr;		// iso only, can be omitted for non-iso
} ATTR_ALIGNED(4) dcd_dma_descriptor_t;

#define DCD_MAX_DD 32 // TODO scale with configure

//typedef struct {
//  dcd_dma_descriptor_t dd[DCD_MAX_DD];
//
//}dcd_data_t;

//--------------------------------------------------------------------+
// Register Interface
//--------------------------------------------------------------------+

//------------- USB Interrupt USBIntSt -------------//
//enum {
//  DCD_USB_REQ_LOW_PRIO_MASK   = BIT_(0),
//  DCD_USB_REQ_HIGH_PRIO_MASK  = BIT_(1),
//  DCD_USB_REQ_DMA_MASK        = BIT_(2),
//  DCD_USB_REQ_NEED_CLOCK_MASK = BIT_(8),
//  DCD_USB_REQ_ENABLE_MASK     = BIT_(31)
//};

//------------- Device Interrupt USBDevInt -------------//
enum {
  DEV_INT_FRAME_MASK              = BIT_(0),
  DEV_INT_ENDPOINT_FAST_MASK      = BIT_(1),
  DEV_INT_ENDPOINT_SLOW_MASK      = BIT_(2),
  DEV_INT_DEVICE_STATUS_MASK      = BIT_(3),
  DEV_INT_COMMAND_CODE_EMPTY_MASK = BIT_(4),
  DEV_INT_COMMAND_DATA_FULL_MASK  = BIT_(5),
  DEV_INT_RX_ENDPOINT_PACKET_MASK = BIT_(6),
  DEV_INT_TX_ENDPOINT_PACKET_MASK = BIT_(7),
  DEV_INT_ENDPOINT_REALIZED_MASK  = BIT_(8),
  DEV_INT_ERROR_MASK              = BIT_(9),

  DEV_INT_ALL_MASK = DEV_INT_FRAME_MASK | DEV_INT_ENDPOINT_FAST_MASK | DEV_INT_ENDPOINT_SLOW_MASK |
    DEV_INT_DEVICE_STATUS_MASK | DEV_INT_COMMAND_CODE_EMPTY_MASK | DEV_INT_COMMAND_DATA_FULL_MASK |
    DEV_INT_RX_ENDPOINT_PACKET_MASK | DEV_INT_TX_ENDPOINT_PACKET_MASK | DEV_INT_ENDPOINT_REALIZED_MASK |
    DEV_INT_ERROR_MASK
};

//------------- DMA Interrupt USBDMAInt-------------//
enum {
  DMA_INT_END_OF_XFER_MASK    = BIT_(0),
  DMA_INT_NEW_DD_REQUEST_MASK = BIT_(1),
  DMA_INT_ERROR_MASK          = BIT_(2)
};

//------------- USBCtrl -------------//
enum {
  SLAVE_CONTROL_READ_ENABLE_MASK  = BIT_(0),
  SLAVE_CONTROL_WRITE_ENABLE_MASK = BIT_(1),
  SLAVE_CONTROL_READ_ENABLE_POS   = 2
};

//------------- USBRxPLen -------------//
enum {
  SLAVE_RXPLEN_PACKET_LENGTH_MASK = (BIT_(10)-1),
  SLAVE_RXPLEN_DATA_VALID_MASK = BIT_(10),
  SLAVE_RXPLEN_PACKET_READY_MASK = BIT_(11),
};

//------------- SIE Command Code -------------//
typedef enum {
  SIE_CMDPHASE_WRITE   = 1,
  SIE_CMDPHASE_READ    = 2,
  SIE_CMDPHASE_COMMAND = 5
} sie_cmdphase_t;

enum {
  // device commands
  SIE_CMDCODE_SET_ADDRESS                     = 0xd0,
  SIE_CMDCODE_CONFIGURE_DEVICE                = 0xd8,
  SIE_CMDCODE_SET_MODE                        = 0xf3,
  SIE_CMDCODE_READ_FRAME_NUMBER               = 0xf5,
  SIE_CMDCODE_READ_TEST_REGISTER              = 0xfd,
  SIE_CMDCODE_DEVICE_STATUS                   = 0xfe,
  SIE_CMDCODE_GET_ERROR                       = 0xff,
  SIE_CMDCODE_READ_ERROR_STATUS               = 0xfb,

  // endpoint commands
  SIE_CMDCODE_ENDPOINT_SELECT                 = 0x00, // + endpoint index
  SIE_CMDCODE_ENDPOINT_SELECT_CLEAR_INTERRUPT = 0x40, // + endpoint index, should use USBEpIntClr instead
  SIE_CMDCODE_ENDPOINT_SET_STATUS             = 0x40, // + endpoint index
  SIE_CMDCODE_BUFFER_CLEAR                    = 0xf2,
  SIE_CMDCODE_BUFFER_VALIDATE                 = 0xfa
};

//------------- SIE Device Status (get/set from SIE_CMDCODE_DEVICE_STATUS) -------------//
enum {
  SIE_DEV_STATUS_CONNECT_MASK        = BIT_(0),
  SIE_DEV_STATUS_CONNECT_CHANGE_MASK = BIT_(1),
  SIE_DEV_STATUS_SUSPEND_MASK        = BIT_(2),
  SIE_DEV_STATUS_SUSPEND_CHANGE_MASK = BIT_(3),
  SIE_DEV_STATUS_RESET_MASK          = BIT_(4)
};

//------------- SIE Endpoint Status -------------//
enum {
  SIE_ENDPOINT_STATUS_FULL_EMPTY_MASK         = BIT_(0), // 0: empty, 1 full. IN endpoint checks empty, OUT endpoint check full
  SIE_ENDPOINT_STATUS_STALL_MASK              = BIT_(1),
  SIE_ENDPOINT_STATUS_SETUP_RECEIVED_MASK     = BIT_(2), // clear by SIE_CMDCODE_ENDPOINT_SELECT_CLEAR_INTERRUPT
  SIE_ENDPOINT_STATUS_PACKET_OVERWRITTEN_MASK = BIT_(3), // previous packet is overwritten by a SETUP packet
  SIE_ENDPOINT_STATUS_NAK_MASK                = BIT_(4), // last packet response is NAK (auto clear by an ACK)
  SIE_ENDPOINT_STATUS_BUFFER1_FULL_MASK       = BIT_(5),
  SIE_ENDPOINT_STATUS_BUFFER2_FULL_MASK       = BIT_(6)
};

//------------- DMA Descriptor Status -------------//
enum {
  DD_STATUS_NOT_SERVICED = 0,
  DD_STATUS_BEING_SERVICED,
  DD_STATUS_NORMAL,
  DD_STATUS_DATA_UNDERUN, // short packet
  DD_STATUS_DATA_OVERRUN,
  DD_STATUS_SYSTEM_ERROR
};

#ifdef __cplusplus
 }
#endif

#endif /* _TUSB_DCD_LPC175X_6X_H_ */

/** @} */
