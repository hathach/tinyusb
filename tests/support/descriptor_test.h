/* 
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
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

/** \file
 *  \brief TBD
 *
 *  \note TBD
 */

/** \ingroup TBD
 *  \defgroup TBD
 *  \brief TBD
 *
 *  @{
 */

#ifndef _TUSB_TEST_DESCRIPTOR_H_
#define _TUSB_TEST_DESCRIPTOR_H_

#ifdef __cplusplus
 extern "C" {
#endif

#include "common/common.h"
#include "class/hid.h"
#include "class/msc.h"
#include "class/cdc.h"

typedef struct
{
  tusb_desc_configuration_t                configuration;

#if 0 //&& IAD_DESC_REQUIRED
  tusb_desc_interface_assoc_t     CDC_IAD;
#endif

#if 0 //&& CFG_TUD_CDC
  //CDC - Serial
  //CDC Control Interface
  tusb_desc_interface_t                 CDC_CCI_Interface;
  CDC_HEADER_DESCRIPTOR                       CDC_Header;
  CDC_ABSTRACT_CONTROL_MANAGEMENT_DESCRIPTOR  CDC_ACM;
  CDC_UNION_1SLAVE_DESCRIPTOR                 CDC_Union;
  tusb_desc_endpoint_t                  CDC_NotificationEndpoint;

  //CDC Data Interface
  tusb_desc_interface_t                 CDC_DCI_Interface;
  tusb_desc_endpoint_t                  CDC_DataOutEndpoint;
  tusb_desc_endpoint_t                  CDC_DataInEndpoint;
#endif

  //------------- HID Keyboard -------------//
  tusb_desc_interface_t                    keyboard_interface;
  tusb_hid_descriptor_hid_t                      keyboard_hid;
  tusb_desc_endpoint_t                     keyboard_endpoint;

  //------------- HID Mouse -------------//
  tusb_desc_interface_t                    mouse_interface;
  tusb_hid_descriptor_hid_t                      mouse_hid;
  tusb_desc_endpoint_t                     mouse_endpoint;

  //------------- Mass Storage -------------//
  tusb_desc_interface_t                    msc_interface;
  tusb_desc_endpoint_t                     msc_endpoint_in;
  tusb_desc_endpoint_t                     msc_endpoint_out;

  //------------- CDC Serial -------------//
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


  unsigned char                               ConfigDescTermination;
} app_configuration_desc_t;

extern tusb_desc_device_t const desc_device;
extern app_configuration_desc_t const desc_configuration;
extern const uint8_t keyboard_report_descriptor[];

#ifdef __cplusplus
 }
#endif

#endif /* _TUSB_TEST_DESCRIPTOR_H_ */

/** @} */
