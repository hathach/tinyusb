/*
 * usbd_host.c
 *
 *  Created on: Jan 19, 2013
 *      Author: hathach
 */

/*
 * Software License Agreement (BSD License)
 * Copyright (c) 2013, hathach (tinyusb.net)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
 * SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 *
 * This file is part of the tiny usb stack.
 */

#include "tusb_option.h"

#ifdef TUSB_CFG_HOST

#define _TINY_USB_SOURCE_FILE_

//--------------------------------------------------------------------+
// INCLUDE
//--------------------------------------------------------------------+
#include "common/common.h"
#include "osal/osal.h"
#include "usbh_hcd.h"

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF
//--------------------------------------------------------------------+


//--------------------------------------------------------------------+
// INTERNAL OBJECT & FUNCTION DECLARATION
//--------------------------------------------------------------------+
static inline uint8_t get_new_address(void) ATTR_ALWAYS_INLINE;

STATIC_ usbh_device_info_t usbh_device_info_pool[TUSB_CFG_HOST_DEVICE_MAX+1]; // including zero-address

//--------------------------------------------------------------------+
// PUBLIC API (Parameter Verification is required)
//--------------------------------------------------------------------+
tusbh_device_status_t tusbh_device_status_get (tusb_handle_device_t const device_hdl)
{
  ASSERT(device_hdl <= TUSB_CFG_HOST_DEVICE_MAX, 0);
  return usbh_device_info_pool[device_hdl].status;
}

//--------------------------------------------------------------------+
// ENUMERATION TASK & ITS DATA
//--------------------------------------------------------------------+
OSAL_TASK_DEF(enum_task, usbh_enumeration_task, 128, OSAL_PRIO_HIGH);

#define ENUM_QUEUE_DEPTH  5
OSAL_QUEUE_DEF(enum_queue, ENUM_QUEUE_DEPTH, uin32_t);
osal_queue_handle_t enum_queue_hdl;
STATIC_ uint8_t enum_data_buffer[TUSB_CFG_HOST_ENUM_BUFFER_SIZE] TUSB_CFG_ATTR_USBRAM;


void usbh_enumeration_task(void)
{
  tusb_error_t error;
  usbh_enumerate_t enum_entry;
  tusb_std_request_t request_packet;

  static uint8_t new_addr;
  static uint8_t configure_selected = 1;

  OSAL_TASK_LOOP_BEGIN

  osal_queue_receive(enum_queue_hdl, (uint32_t*)(&enum_entry), OSAL_TIMEOUT_WAIT_FOREVER, &error);

  TASK_ASSERT( hcd_port_connect_status(enum_entry.core_id) ); // device may be unplugged
  usbh_device_info_pool[0].core_id  = enum_entry.core_id;
  usbh_device_info_pool[0].hub_addr = enum_entry.hub_addr;
  usbh_device_info_pool[0].hub_port = enum_entry.hub_port;
  usbh_device_info_pool[0].speed    = enum_entry.speed;

  TASK_ASSERT_STATUS( hcd_pipe_control_open(0, 8) );

  //------------- Get first 8 bytes of device descriptor to get Control Endpoint Size -------------//
  request_packet = (tusb_std_request_t) {
        .bmRequestType = { .direction = TUSB_DIR_DEV_TO_HOST, .type = TUSB_REQUEST_TYPE_STANDARD, .recipient = TUSB_REQUEST_RECIPIENT_DEVICE },
        .bRequest = TUSB_REQUEST_GET_DESCRIPTOR,
        .wValue   = (TUSB_DESC_DEVICE << 8),
        .wLength  = 8
  };
  hcd_pipe_control_xfer(0, &request_packet, enum_data_buffer);
  osal_semaphore_wait(usbh_device_info_pool[0].sem_hdl, OSAL_TIMEOUT_NORMAL, &error); // careful of local variable without static
  TASK_ASSERT_STATUS_HANDLER(error, tusbh_device_mount_failed_cb(TUSB_ERROR_USBH_MOUNT_DEVICE_NOT_RESPOND, NULL) );

  //------------- Set new address -------------//
  new_addr = get_new_address();
  TASK_ASSERT(new_addr <= TUSB_CFG_HOST_DEVICE_MAX);

  request_packet = (tusb_std_request_t) {
        .bmRequestType = { .direction = TUSB_DIR_HOST_TO_DEV, .type = TUSB_REQUEST_TYPE_STANDARD, .recipient = TUSB_REQUEST_RECIPIENT_DEVICE },
        .bRequest = TUSB_REQUEST_SET_ADDRESS,
        .wValue   = new_addr
  };
  (void) hcd_pipe_control_xfer(0, &request_packet, NULL);
  osal_semaphore_wait(usbh_device_info_pool[0].sem_hdl, OSAL_TIMEOUT_NORMAL, &error); // careful of local variable without static
  TASK_ASSERT_STATUS_HANDLER(error, tusbh_device_mount_failed_cb(TUSB_ERROR_USBH_MOUNT_DEVICE_NOT_RESPOND, NULL) );

  //------------- update device info & open control pipe for new address -------------//
  usbh_device_info_pool[new_addr].core_id  = enum_entry.core_id;
  usbh_device_info_pool[new_addr].hub_addr = enum_entry.hub_addr;
  usbh_device_info_pool[new_addr].hub_port = enum_entry.hub_port;
  usbh_device_info_pool[new_addr].speed    = enum_entry.speed;
  usbh_device_info_pool[new_addr].status   = TUSB_DEVICE_STATUS_ADDRESSED;
  TASK_ASSERT_STATUS ( hcd_pipe_control_open(new_addr, ((tusb_descriptor_device_t*) enum_data_buffer)->bMaxPacketSize0 ) );

  //------------- Get full device descriptor -------------//
  request_packet = (tusb_std_request_t) {
      .bmRequestType = { .direction = TUSB_DIR_DEV_TO_HOST, .type = TUSB_REQUEST_TYPE_STANDARD, .recipient = TUSB_REQUEST_RECIPIENT_DEVICE },
      .bRequest = TUSB_REQUEST_GET_DESCRIPTOR,
      .wValue   = (TUSB_DESC_DEVICE << 8),
      .wLength  = 18
  };
  (void) hcd_pipe_control_xfer(new_addr, &request_packet, enum_data_buffer);
  osal_semaphore_wait(usbh_device_info_pool[new_addr].sem_hdl, OSAL_TIMEOUT_NORMAL, &error); // careful of local variable without static
  TASK_ASSERT_STATUS_HANDLER(error, tusbh_device_mount_failed_cb(TUSB_ERROR_USBH_MOUNT_DEVICE_NOT_RESPOND, NULL) );

  usbh_device_info_pool[new_addr].vendor_id       = ((tusb_descriptor_device_t*) enum_data_buffer)->idVendor;
  usbh_device_info_pool[new_addr].product_id      = ((tusb_descriptor_device_t*) enum_data_buffer)->idProduct;
  usbh_device_info_pool[new_addr].configure_count = ((tusb_descriptor_device_t*) enum_data_buffer)->bNumConfigurations;

  //------------- update device info and invoke callback to ask user which configuration to select -------------//
  if (tusbh_device_attached_cb)
  {
    configure_selected = min8_of(1, tusbh_device_attached_cb( (tusb_descriptor_device_t*) enum_data_buffer) );
  }else
  {
    configure_selected = 1;
  }

  //------------- Get 9 bytes of configuration descriptor -------------//
  request_packet = (tusb_std_request_t) {
      .bmRequestType = { .direction = TUSB_DIR_DEV_TO_HOST, .type = TUSB_REQUEST_TYPE_STANDARD, .recipient = TUSB_REQUEST_RECIPIENT_DEVICE },
      .bRequest = TUSB_REQUEST_GET_DESCRIPTOR,
      .wValue   = (TUSB_DESC_CONFIGURATION << 8) | (configure_selected - 1),
      .wLength  = 9
  };
  (void) hcd_pipe_control_xfer(new_addr, &request_packet, enum_data_buffer);
  osal_semaphore_wait(usbh_device_info_pool[new_addr].sem_hdl, OSAL_TIMEOUT_NORMAL, &error); // careful of local variable without static
  TASK_ASSERT_STATUS_HANDLER(error, tusbh_device_mount_failed_cb(TUSB_ERROR_USBH_MOUNT_DEVICE_NOT_RESPOND, NULL) );

  TASK_ASSERT_HANDLER( TUSB_CFG_HOST_ENUM_BUFFER_SIZE > ((tusb_descriptor_configuration_t*)enum_data_buffer)->wTotalLength,
                       tusbh_device_mount_failed_cb(TUSB_ERROR_USBH_MOUNT_CONFIG_DESC_TOO_LONG, NULL) );

  //------------- Get full configuration descriptor -------------//
  request_packet = (tusb_std_request_t) {
        .bmRequestType = { .direction = TUSB_DIR_DEV_TO_HOST, .type = TUSB_REQUEST_TYPE_STANDARD, .recipient = TUSB_REQUEST_RECIPIENT_DEVICE },
        .bRequest = TUSB_REQUEST_GET_DESCRIPTOR,
        .wValue   = (TUSB_DESC_CONFIGURATION << 8) | (configure_selected - 1),
        .wLength  = ((tusb_descriptor_configuration_t*)enum_data_buffer)->wTotalLength
  };
  (void) hcd_pipe_control_xfer(new_addr, &request_packet, enum_data_buffer);
  osal_semaphore_wait(usbh_device_info_pool[new_addr].sem_hdl, OSAL_TIMEOUT_NORMAL, &error); // careful of local variable without static
  TASK_ASSERT_STATUS_HANDLER(error, tusbh_device_mount_failed_cb(TUSB_ERROR_USBH_MOUNT_DEVICE_NOT_RESPOND, NULL) );

  //------------- Set Configure -------------//
  request_packet = (tusb_std_request_t) {
        .bmRequestType = { .direction = TUSB_DIR_HOST_TO_DEV, .type = TUSB_REQUEST_TYPE_STANDARD, .recipient = TUSB_REQUEST_RECIPIENT_DEVICE },
        .bRequest = TUSB_REQUEST_SET_CONFIGURATION,
        .wValue   = configure_selected,
  };
  (void) hcd_pipe_control_xfer(new_addr, &request_packet, enum_data_buffer);
  osal_semaphore_wait(usbh_device_info_pool[new_addr].sem_hdl, OSAL_TIMEOUT_NORMAL, &error); // careful of local variable without static
  TASK_ASSERT_STATUS_HANDLER(error, tusbh_device_mount_failed_cb(TUSB_ERROR_USBH_MOUNT_DEVICE_NOT_RESPOND, NULL) );

  // TODO invoke mounted callback
  OSAL_TASK_LOOP_END
}

//--------------------------------------------------------------------+
// REPORTER TASK & ITS DATA
//--------------------------------------------------------------------+


//--------------------------------------------------------------------+
// CLASS-USBD API (don't require to verify parameters)
//--------------------------------------------------------------------+
tusb_error_t usbh_init(void)
{
  uint32_t i;

  memclr_(usbh_device_info_pool, sizeof(usbh_device_info_t)*(TUSB_CFG_HOST_DEVICE_MAX+1));

  for(i=0; i<TUSB_CFG_HOST_CONTROLLER_NUM; i++)
  {
    ASSERT_STATUS( hcd_init(i) );
  }

  ASSERT_STATUS( osal_task_create(&enum_task) );
  enum_queue_hdl = osal_queue_create(&enum_queue);
  ASSERT_PTR(enum_queue_hdl, TUSB_ERROR_OSAL_QUEUE_FAILED);

  return TUSB_ERROR_NONE;
}
#endif

//--------------------------------------------------------------------+
// INTERNAL HELPER
//--------------------------------------------------------------------+
static inline uint8_t get_new_address(void)
{
  uint8_t new_addr;
  for (new_addr=1; new_addr <= TUSB_CFG_HOST_DEVICE_MAX; new_addr++)
  {
    if (usbh_device_info_pool[new_addr].status == TUSB_DEVICE_STATUS_UNPLUG)
      break;
  }
  return new_addr;
}

