/*
 * SPDX-FileCopyrightText: Copyright (c) 2019 Ha Thach (tinyusb.org)
 * SPDX-License-Identifier: MIT
 *
 * This file is part of the TinyUSB stack.
 */

#ifndef TUSB_VENDOR_HOST_H_
#define TUSB_VENDOR_HOST_H_

#include "common/tusb_common.h"

#ifdef __cplusplus
 extern "C" {
#endif

typedef struct {
  pipe_handle_t pipe_in;
  pipe_handle_t pipe_out;
}custom_interface_info_t;

//--------------------------------------------------------------------+
// USBH-CLASS DRIVER API
//--------------------------------------------------------------------+
static inline bool tusbh_custom_is_mounted(uint8_t dev_addr, uint16_t vendor_id, uint16_t product_id)
{
  (void) vendor_id; // TODO check this later
  (void) product_id;
//  return (tusbh_device_get_mounted_class_flag(dev_addr) & TU_BIT(TUSB_CLASS_MAPPED_INDEX_END-1) ) != 0;
  return false;
}

bool tusbh_custom_read(uint8_t dev_addr, uint16_t vendor_id, uint16_t product_id, void * p_buffer, uint16_t length);
bool tusbh_custom_write(uint8_t dev_addr, uint16_t vendor_id, uint16_t product_id, void const * p_data, uint16_t length);

//--------------------------------------------------------------------+
// Internal Class Driver API
//--------------------------------------------------------------------+
void cush_init(void);
bool cush_open_subtask(uint8_t dev_addr, tusb_desc_interface_t const *p_interface_desc, uint16_t *p_length);
void cush_isr(pipe_handle_t pipe_hdl, xfer_result_t event);
void cush_close(uint8_t dev_addr);

#ifdef __cplusplus
 }
#endif

#endif /* TUSB_VENDOR_HOST_H_ */
