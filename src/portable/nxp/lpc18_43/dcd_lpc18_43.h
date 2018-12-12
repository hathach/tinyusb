/**************************************************************************/
/*!
    @file     dcd_lpc43xx.h
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

/** \ingroup group_dcd
 *  \defgroup group_dcd_lpc143xx LPC43xx
 *  @{ */

#ifndef _TUSB_DCD_LPC43XX_H_
#define _TUSB_DCD_LPC43XX_H_

#include "common/tusb_common.h"

#ifdef __cplusplus
 extern "C" {
#endif

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF
//--------------------------------------------------------------------+

/*---------- ENDPTCTRL ----------*/
enum {
  ENDPTCTRL_MASK_STALL          = BIT_(0),
  ENDPTCTRL_MASK_TOGGLE_INHIBIT = BIT_(5), ///< used for test only
  ENDPTCTRL_MASK_TOGGLE_RESET   = BIT_(6),
  ENDPTCTRL_MASK_ENABLE         = BIT_(7)
};

/*---------- USBCMD ----------*/
enum {
  USBCMD_MASK_RUN_STOP         = BIT_(0),
  USBCMD_MASK_RESET            = BIT_(1),
  USBCMD_MASK_SETUP_TRIPWIRE   = BIT_(13),
  USBCMD_MASK_ADD_QTD_TRIPWIRE = BIT_(14)  ///< This bit is used as a semaphore to ensure the to proper addition of a new dTD to an active (primed) endpoint’s linked list. This bit is set and cleared by software during the process of adding a new dTD
};
// Interrupt Threshold bit 23:16

/*---------- USBSTS, USBINTR ----------*/
enum {
  INT_MASK_USB         = BIT_(0),
  INT_MASK_ERROR       = BIT_(1),
  INT_MASK_PORT_CHANGE = BIT_(2),
  INT_MASK_RESET       = BIT_(6),
  INT_MASK_SOF         = BIT_(7),
  INT_MASK_SUSPEND     = BIT_(8),
  INT_MASK_NAK         = BIT_(16)
};

//------------- PORTSC -------------//
enum {
  PORTSC_CURRENT_CONNECT_STATUS_MASK = BIT_(0),
  PORTSC_FORCE_PORT_RESUME_MASK      = BIT_(6),
  PORTSC_SUSPEND_MASK                = BIT_(7)
};

typedef struct
{
  // Word 0: Next QTD Pointer
  uint32_t next; ///< Next link pointer This field contains the physical memory address of the next dTD to be processed

  // Word 1: qTQ Token
  uint32_t                      : 3  ;
  volatile uint32_t xact_err    : 1  ;
  uint32_t                      : 1  ;
  volatile uint32_t buffer_err  : 1  ;
  volatile uint32_t halted      : 1  ;
  volatile uint32_t active      : 1  ;
  uint32_t                      : 2  ;
  uint32_t iso_mult_override    : 2  ; ///< This field can be used for transmit ISOs to override the MULT field in the dQH. This field must be zero for all packet types that are not transmit-ISO.
  uint32_t                      : 3  ;
  uint32_t int_on_complete      : 1  ;
  volatile uint32_t total_bytes : 15 ;
  uint32_t                      : 0  ;

  // Word 2-6: Buffer Page Pointer List, Each element in the list is a 4K page aligned, physical memory address. The lower 12 bits in each pointer are reserved (except for the first one) as each memory pointer must reference the start of a 4K page
  uint32_t buffer[5]; ///< buffer1 has frame_n for TODO Isochronous

  //------------- DCD Area -------------//
  uint16_t expected_bytes;
  uint8_t reserved[2];
} dcd_qtd_t;

TU_VERIFY_STATIC( sizeof(dcd_qtd_t) == 32, "size is not correct");

typedef struct
{
  // Word 0: Capabilities and Characteristics
  uint32_t                         : 15 ; ///< Number of packets executed per transaction descriptor 00 - Execute N transactions as demonstrated by the USB variable length protocol where N is computed using Max_packet_length and the Total_bytes field in the dTD. 01 - Execute one transaction 10 - Execute two transactions 11 - Execute three transactions Remark: Non-isochronous endpoints must set MULT = 00. Remark: Isochronous endpoints must set MULT = 01, 10, or 11 as needed.
  uint32_t int_on_setup            : 1  ; ///< Interrupt on setup This bit is used on control type endpoints to indicate if USBINT is set in response to a setup being received.
  uint32_t max_package_size        : 11 ; ///< This directly corresponds to the maximum packet size of the associated endpoint (wMaxPacketSize)
  uint32_t                         : 2  ;
  uint32_t zero_length_termination : 1  ; ///< This bit is used for non-isochronous endpoints to indicate when a zero-length packet is received to terminate transfers in case the total transfer length is “multiple”. 0 - Enable zero-length packet to terminate transfers equal to a multiple of Max_packet_length (default). 1 - Disable zero-length packet on transfers that are equal in length to a multiple Max_packet_length.
  uint32_t iso_mult                : 2  ; ///<
  uint32_t                         : 0  ;

  // Word 1: Current qTD Pointer
	volatile uint32_t qtd_addr;

	// Word 2-9: Transfer Overlay
	volatile dcd_qtd_t qtd_overlay;

	// Word 10-11: Setup request (control OUT only)
	volatile tusb_control_request_t setup_request;

	//--------------------------------------------------------------------+
  /// Due to the fact QHD is 64 bytes aligned but occupies only 48 bytes
	/// thus there are 16 bytes padding free that we can make use of.
  //--------------------------------------------------------------------+
	uint8_t reserved[16];
}  dcd_qhd_t;

TU_VERIFY_STATIC( sizeof(dcd_qhd_t) == 64, "size is not correct");


#ifdef __cplusplus
 }
#endif

#endif /* _TUSB_DCD_LPC43XX_H_ */

/** @} */
