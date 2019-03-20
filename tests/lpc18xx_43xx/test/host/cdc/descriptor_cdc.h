/* 
 * The MIT License (MIT)
 *
 * Copyright (c) 2018, hathach (tinyusb.org)
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
  tusb_desc_configuration_t              configuration;

  tusb_desc_interface_assoc_t      cdc_iad;

  //CDC Control Interface
  tusb_desc_interface_t                  cdc_comm_interface;
  cdc_desc_func_header_t                       cdc_header;
  cdc_desc_func_acm_t  cdc_acm;
  cdc_desc_func_union_t                        cdc_union;
  tusb_desc_endpoint_t                   cdc_endpoint_notification;

  //CDC Data Interface
  tusb_desc_interface_t                  cdc_data_interface;
  tusb_desc_endpoint_t                   cdc_endpoint_out;
  tusb_desc_endpoint_t                   cdc_endpoint_in;

} cdc_configuration_desc_t;

extern const cdc_configuration_desc_t cdc_config_descriptor;
extern const cdc_configuration_desc_t rndis_config_descriptor;

#ifdef __cplusplus
 }
#endif

#endif /* _TUSB_DESCRIPTOR_CDC_H_ */

/** @} */
