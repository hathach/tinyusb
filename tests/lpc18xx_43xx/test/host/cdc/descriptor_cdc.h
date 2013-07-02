/**************************************************************************/
/*!
    @file     descriptor_cdc.h
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

#ifndef _TUSB_DESCRIPTOR_CDC_H_
#define _TUSB_DESCRIPTOR_CDC_H_

#include "common/common.h"
#include "class/cdc.h"

#ifdef __cplusplus
 extern "C" {
#endif

typedef struct
{
  tusb_descriptor_configuration_t              configuration;

  tusb_descriptor_interface_association_t      cdc_iad;

  //CDC Control Interface
  tusb_descriptor_interface_t                  cdc_comm_interface;
  tusb_cdc_func_header_t                       cdc_header;
  tusb_cdc_func_abstract_control_management_t  cdc_acm;
  tusb_cdc_func_union_t                        cdc_union;
  tusb_descriptor_endpoint_t                   cdc_endpoint_notification;

  //CDC Data Interface
  tusb_descriptor_interface_t                  cdc_data_interface;
  tusb_descriptor_endpoint_t                   cdc_endpoint_out;
  tusb_descriptor_endpoint_t                   cdc_endpoint_in;

} cdc_configuration_desc_t;

extern const cdc_configuration_desc_t cdc_config_descriptor;

#ifdef __cplusplus
 }
#endif

#endif /* _TUSB_DESCRIPTOR_CDC_H_ */

/** @} */
