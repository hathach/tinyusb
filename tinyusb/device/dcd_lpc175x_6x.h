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
	uint16_t mode            : 2; // either normal or ATLE(auto length extraction)
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

#define DCD_DD_NUM 10 // TODO scale with configure
typedef struct {
  dcd_dma_descriptor_t dd[DCD_DD_NUM];

}dcd_data_t;

#ifdef __cplusplus
 }
#endif

#endif /* _TUSB_DCD_LPC175X_6X_H_ */

/** @} */
