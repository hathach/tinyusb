/**************************************************************************/
/*!
    @file     msc.h
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

#ifndef _TUSB_MSC_H_
#define _TUSB_MSC_H_

#include "common/common.h"

#ifdef __cplusplus
 extern "C" {
#endif

//--------------------------------------------------------------------+
// SCSI Primary Command (SPC-4)
//--------------------------------------------------------------------+
typedef ATTR_PACKED_STRUCT(struct)
{
  uint8_t peripheral_device_type     : 5;
  uint8_t peripheral_qualifier       : 3;

  uint8_t                            : 7;
  uint8_t is_removable               : 1;

  uint8_t version;

  uint8_t response_data_format       : 4;
  uint8_t hierarchical_support       : 1;
  uint8_t normal_aca                 : 1;
  uint8_t                            : 2;

  uint8_t  additional_length;

  uint8_t protect                    : 1;
  uint8_t                            : 2;
  uint8_t third_party_copy           : 1;
  uint8_t target_port_group_support  : 2;
  uint8_t access_control_coordinator : 1;
  uint8_t scc_support                : 1;

  uint8_t addr16                     : 1;
  uint8_t                            : 3;
  uint8_t multi_port                 : 1;
  uint8_t                            : 1; // vendor specific
  uint8_t enclosure_service          : 1;
  uint8_t                            : 1;

  uint8_t                            : 1; // vendor specific
  uint8_t cmd_que                    : 1;
  uint8_t                            : 2;
  uint8_t sync                       : 1;
  uint8_t wbus16                     : 1;
  uint8_t                            : 2;

  uint8_t  vendor_id[8];
  uint8_t  product_id[16];
  uint8_t  product_revision[4];
} msc_scsi_inquiry_t;

//--------------------------------------------------------------------+
// SCSI Block Command (SBC-3)
//--------------------------------------------------------------------+
typedef struct {
  uint8_t logical_block_addr[4];
  uint8_t block_length[4];
} msc_scsi_read_capacity10_t;

#ifdef __cplusplus
 }
#endif

#endif /* _TUSB_MSC_H_ */

/** @} */
