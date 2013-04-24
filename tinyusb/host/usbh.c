/*
 * usbd_host.c
 *
 *  Created on: Jan 19, 2013
 *      Author: hathach
 */

/*
 * Software License Agreement (BSD License)
 * Copyright (c) 2013, hathach (tinyusb.org)
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

#include "common/common.h"

#if MODE_HOST_SUPPORTED

#define _TINY_USB_SOURCE_FILE_

//--------------------------------------------------------------------+
// INCLUDE
//--------------------------------------------------------------------+
#include "tusb.h"
#include "usbh_hcd.h"

//TODO temporarily
#if TUSB_CFG_OS == TUSB_OS_NONE && !defined(_TEST_)
void tusb_tick_tock(void)
{
  osal_tick_tock();
}
#endif

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF
//--------------------------------------------------------------------+
#define ENUM_QUEUE_DEPTH  5

// TODO fix/compress number of class driver
static host_class_driver_t const usbh_class_drivers[TUSB_CLASS_MAX_CONSEC_NUMBER] =
{
#if HOST_CLASS_HID
    [TUSB_CLASS_HID] = {
        .init = hidh_init,
        .open_subtask = hidh_open_subtask,
        .isr = hidh_isr,
        .close = hidh_close
    },
#endif

#if TUSB_CFG_HOST_CLASS_MSC
    [TUSB_CLASS_MSC] = {
        .init = msch_init,
        .open_subtask = msch_open_subtask,
        .isr = msch_isr,
        .close = msch_close
    }
#endif

};

//--------------------------------------------------------------------+
// INTERNAL OBJECT & FUNCTION DECLARATION
//--------------------------------------------------------------------+
usbh_device_info_t usbh_devices[TUSB_CFG_HOST_DEVICE_MAX+1] TUSB_CFG_ATTR_USBRAM; // including zero-address
OSAL_TASK_FUNCTION(usbh_enumeration_task);

//------------- Enumeration Task Data -------------//
OSAL_TASK_DEF(enum_task, usbh_enumeration_task, 128, OSAL_PRIO_HIGH);
OSAL_QUEUE_DEF(enum_queue, ENUM_QUEUE_DEPTH, uint32_t);
osal_queue_handle_t enum_queue_hdl;
STATIC_ uint8_t enum_data_buffer[TUSB_CFG_HOST_ENUM_BUFFER_SIZE] TUSB_CFG_ATTR_USBRAM;

//------------- Reporter Task Data -------------//

//------------- Helper Function Prototypes -------------//
static inline uint8_t get_new_address(void) ATTR_ALWAYS_INLINE;
static inline uint8_t get_configure_number_for_device(tusb_descriptor_device_t* dev_desc) ATTR_ALWAYS_INLINE;

//--------------------------------------------------------------------+
// PUBLIC API (Parameter Verification is required)
//--------------------------------------------------------------------+
tusb_device_state_t tusbh_device_get_state (uint8_t const dev_addr)
{
  ASSERT_INT_WITHIN(1, TUSB_CFG_HOST_DEVICE_MAX, dev_addr, TUSB_DEVICE_STATE_INVALID_PARAMETER);
  return usbh_devices[dev_addr].state;
}

//--------------------------------------------------------------------+
// CLASS-USBD API (don't require to verify parameters)
//--------------------------------------------------------------------+
tusb_error_t usbh_init(void)
{
  memclr_(usbh_devices, sizeof(usbh_device_info_t)*(TUSB_CFG_HOST_DEVICE_MAX+1));

  ASSERT_STATUS( hcd_init() );

  //------------- Semaphore for Control Pipe -------------//
  for(uint8_t i=0; i<TUSB_CFG_HOST_DEVICE_MAX+1; i++) // including address zero
  {
    usbh_devices[i].control.sem_hdl = osal_semaphore_create( OSAL_SEM_REF(usbh_devices[i].control.semaphore) );
    ASSERT_PTR(usbh_devices[i].control.sem_hdl, TUSB_ERROR_OSAL_SEMAPHORE_FAILED);
  }

  //------------- Enumeration & Reporter Task init -------------//
  ASSERT_STATUS( osal_task_create(&enum_task) );
  enum_queue_hdl = osal_queue_create(&enum_queue);
  ASSERT_PTR(enum_queue_hdl, TUSB_ERROR_OSAL_QUEUE_FAILED);

  //------------- class init -------------//
  for (uint8_t class_code = 1; class_code < TUSB_CLASS_MAX_CONSEC_NUMBER; class_code++)
  {
    if (usbh_class_drivers[class_code].init)
      usbh_class_drivers[class_code].init();
  }

  return TUSB_ERROR_NONE;
}

//------------- USBH control transfer -------------//
// function called within a task, requesting os blocking services, subtask input parameter must be static/global variables
tusb_error_t usbh_control_xfer_subtask(uint8_t dev_addr, tusb_std_request_t const* p_request, uint8_t* data)
{
  tusb_error_t error;

  OSAL_SUBTASK_BEGIN

  usbh_devices[dev_addr].control.pipe_status = TUSB_INTERFACE_STATUS_BUSY;
  usbh_devices[dev_addr].control.request = *p_request;
  (void) hcd_pipe_control_xfer(dev_addr, &usbh_devices[dev_addr].control.request, data);

  osal_semaphore_wait(usbh_devices[dev_addr].control.sem_hdl, OSAL_TIMEOUT_NORMAL, &error); // careful of local variable without static
  // TODO make handler for this function general purpose
  SUBTASK_ASSERT_WITH_HANDLER(TUSB_ERROR_NONE == error && usbh_devices[dev_addr].control.pipe_status != TUSB_INTERFACE_STATUS_ERROR,
                                     tusbh_device_mount_failed_cb(TUSB_ERROR_USBH_MOUNT_DEVICE_NOT_RESPOND, NULL) );

  OSAL_SUBTASK_END
}

tusb_error_t usbh_pipe_control_open(uint8_t dev_addr, uint8_t max_packet_size) ATTR_ALWAYS_INLINE;
tusb_error_t usbh_pipe_control_open(uint8_t dev_addr, uint8_t max_packet_size)
{
  osal_semaphore_reset( usbh_devices[dev_addr].control.sem_hdl );

  ASSERT_STATUS( hcd_pipe_control_open(dev_addr, max_packet_size) );

  return TUSB_ERROR_NONE;
}

static inline tusb_error_t usbh_pipe_control_close(uint8_t dev_addr) ATTR_ALWAYS_INLINE;
static inline tusb_error_t usbh_pipe_control_close(uint8_t dev_addr)
{
  ASSERT_STATUS( hcd_pipe_control_close(dev_addr) );

  return TUSB_ERROR_NONE;
}

tusb_interface_status_t usbh_pipe_status_get(pipe_handle_t pipe_hdl)
{
  return TUSB_INTERFACE_STATUS_BUSY;
}

//--------------------------------------------------------------------+
// USBH-HCD ISR/Callback API
//--------------------------------------------------------------------+
// interrupt caused by a TD (with IOC=1) in pipe of class class_code
void usbh_isr(pipe_handle_t pipe_hdl, uint8_t class_code, tusb_event_t event)
{
  if (class_code == 0) // Control transfer
  {
    usbh_devices[ pipe_hdl.dev_addr ].control.pipe_status = (event == TUSB_EVENT_XFER_COMPLETE) ? TUSB_INTERFACE_STATUS_COMPLETE : TUSB_INTERFACE_STATUS_ERROR;
    osal_semaphore_post( usbh_devices[ pipe_hdl.dev_addr ].control.sem_hdl );
  }else if (usbh_class_drivers[class_code].isr)
  {
    usbh_class_drivers[class_code].isr(pipe_hdl, event);
  }else
  {
    ASSERT(false, (void) 0); // something wrong, no one claims the isr's source
  }
}

void usbh_device_plugged_isr(uint8_t hostid, tusb_speed_t speed)
{
  osal_queue_send(enum_queue_hdl,
                  &(usbh_enumerate_t){ .core_id = hostid, .speed = speed} );
}

void usbh_device_unplugged_isr(uint8_t hostid)
{
  //------------- find the device address that is unplugged -------------//
  uint8_t dev_addr = 0;
  while ( dev_addr <= TUSB_CFG_HOST_DEVICE_MAX &&
          !(usbh_devices[dev_addr].core_id  == hostid &&
            usbh_devices[dev_addr].hub_addr == 0 &&
            usbh_devices[dev_addr].hub_port == 0 &&
            usbh_devices[dev_addr].state    != TUSB_DEVICE_STATE_UNPLUG ) )
  {
    dev_addr++;
  }

  // TODO close addr0 pipe (when get 8-byte desc, set addr failed)
  ASSERT(dev_addr <= TUSB_CFG_HOST_DEVICE_MAX, (void) 0 );

  if (dev_addr > 0) // device can still be unplugged when not set new address
  {
    // if device unplugged is not a hub TODO handle hub unplugged
    for (uint8_t class_code = 1; class_code < TUSB_CLASS_MAX_CONSEC_NUMBER; class_code++)
    {
      if ((usbh_devices[dev_addr].flag_supported_class & BIT_(class_code)) &&
          usbh_class_drivers[class_code].close)
      {
        usbh_class_drivers[class_code].close(dev_addr);
        }
    }
  }

  usbh_pipe_control_close(dev_addr);

  // set to REMOVING to allow HCD to clean up its cached data for this device
  // HCD must set this device's state to TUSB_DEVICE_STATE_UNPLUG when done
  usbh_devices[dev_addr].state = TUSB_DEVICE_STATE_REMOVING;
  usbh_devices[dev_addr].flag_supported_class = 0;
}

//--------------------------------------------------------------------+
// ENUMERATION TASK
//--------------------------------------------------------------------+
OSAL_TASK_FUNCTION(usbh_enumeration_task)
{
  tusb_error_t error;
  usbh_enumerate_t enum_entry;

  // for OSAL_NONE local variable won't retain value after blocking service sem_wait/queue_recv
  static uint8_t new_addr;
  static uint8_t configure_selected = 1; // TODO move
  static uint8_t *p_desc = NULL; // TODO move

  OSAL_TASK_LOOP_BEGIN

  osal_queue_receive(enum_queue_hdl, &enum_entry, OSAL_TIMEOUT_WAIT_FOREVER, &error);

  SUBTASK_ASSERT( hcd_port_connect_status(enum_entry.core_id) ); // ensure device is still plugged
  usbh_devices[0].core_id  = enum_entry.core_id; // TODO refractor integrate to device_pool
  usbh_devices[0].hub_addr = enum_entry.hub_addr;
  usbh_devices[0].hub_port = enum_entry.hub_port;
  usbh_devices[0].speed    = enum_entry.speed;

  SUBTASK_ASSERT_STATUS( usbh_pipe_control_open(0, 8) );
  usbh_devices[0].state = TUSB_DEVICE_STATE_ADDRESSED;

#ifndef _TEST_
  // TODO finalize delay after reset, hack delay 100 ms, otherwise speed is detected as LOW in most cases
  volatile uint32_t delay_us = 10000;
  delay_us *= (SystemCoreClock / 1000000) / 3;
  while(delay_us--);
#endif

  //------------- Get first 8 bytes of device descriptor to get Control Endpoint Size -------------//
  OSAL_SUBTASK_INVOKED_AND_WAIT(
    usbh_control_xfer_subtask(
      0,
      &(tusb_std_request_t)
      {
        .bmRequestType = { .direction = TUSB_DIR_DEV_TO_HOST, .type = TUSB_REQUEST_TYPE_STANDARD, .recipient = TUSB_REQUEST_RECIPIENT_DEVICE },
        .bRequest = TUSB_REQUEST_GET_DESCRIPTOR,
        .wValue   = (TUSB_DESC_DEVICE << 8),
        .wLength  = 8
      },
      enum_data_buffer
    )
  );

  hcd_port_reset( usbh_devices[0].core_id ); // reset port after 8 byte descriptor

  //------------- Set new address -------------//
  new_addr = get_new_address();
  SUBTASK_ASSERT(new_addr <= TUSB_CFG_HOST_DEVICE_MAX); // TODO notify application we reach max devices

  OSAL_SUBTASK_INVOKED_AND_WAIT(
    usbh_control_xfer_subtask(
      0,
      &(tusb_std_request_t)
      {
        .bmRequestType = { .direction = TUSB_DIR_HOST_TO_DEV, .type = TUSB_REQUEST_TYPE_STANDARD, .recipient = TUSB_REQUEST_RECIPIENT_DEVICE },
        .bRequest = TUSB_REQUEST_SET_ADDRESS,
        .wValue   = new_addr
      },
      NULL
    )
  );

  //------------- update port info & close control pipe of addr0 -------------//
  usbh_devices[new_addr].core_id  = usbh_devices[0].core_id;
  usbh_devices[new_addr].hub_addr = usbh_devices[0].hub_addr;
  usbh_devices[new_addr].hub_port = usbh_devices[0].hub_port;
  usbh_devices[new_addr].speed    = usbh_devices[0].speed;
  usbh_devices[new_addr].state    = TUSB_DEVICE_STATE_ADDRESSED;

  usbh_pipe_control_close(0);
  usbh_devices[0].state = TUSB_DEVICE_STATE_UNPLUG;

  // open control pipe for new address
  SUBTASK_ASSERT_STATUS ( usbh_pipe_control_open(new_addr, ((tusb_descriptor_device_t*) enum_data_buffer)->bMaxPacketSize0 ) );

  //------------- Get full device descriptor -------------//
  OSAL_SUBTASK_INVOKED_AND_WAIT(
    usbh_control_xfer_subtask(
      new_addr,
      &(tusb_std_request_t)
      {
        .bmRequestType = { .direction = TUSB_DIR_DEV_TO_HOST, .type = TUSB_REQUEST_TYPE_STANDARD, .recipient = TUSB_REQUEST_RECIPIENT_DEVICE },
        .bRequest = TUSB_REQUEST_GET_DESCRIPTOR,
        .wValue   = (TUSB_DESC_DEVICE << 8),
        .wLength  = 18
      },
      enum_data_buffer
    )
  );

  // update device info  TODO alignment issue
  usbh_devices[new_addr].vendor_id       = ((tusb_descriptor_device_t*) enum_data_buffer)->idVendor;
  usbh_devices[new_addr].product_id      = ((tusb_descriptor_device_t*) enum_data_buffer)->idProduct;
  usbh_devices[new_addr].configure_count = ((tusb_descriptor_device_t*) enum_data_buffer)->bNumConfigurations;

  configure_selected = get_configure_number_for_device((tusb_descriptor_device_t*) enum_data_buffer);
  SUBTASK_ASSERT(configure_selected <= usbh_devices[new_addr].configure_count); // TODO notify application when invalid configuration

  //------------- Get 9 bytes of configuration descriptor -------------//
  OSAL_SUBTASK_INVOKED_AND_WAIT(
    usbh_control_xfer_subtask(
      new_addr,
      &(tusb_std_request_t)
      {
        .bmRequestType = { .direction = TUSB_DIR_DEV_TO_HOST, .type = TUSB_REQUEST_TYPE_STANDARD, .recipient = TUSB_REQUEST_RECIPIENT_DEVICE },
        .bRequest = TUSB_REQUEST_GET_DESCRIPTOR,
        .wValue   = (TUSB_DESC_CONFIGURATION << 8) | (configure_selected - 1),
        .wLength  = 9
      },
      enum_data_buffer
    )
  );
  SUBTASK_ASSERT_WITH_HANDLER( TUSB_CFG_HOST_ENUM_BUFFER_SIZE > ((tusb_descriptor_configuration_t*)enum_data_buffer)->wTotalLength,
                            tusbh_device_mount_failed_cb(TUSB_ERROR_USBH_MOUNT_CONFIG_DESC_TOO_LONG, NULL) );

  //------------- Get full configuration descriptor -------------//
  OSAL_SUBTASK_INVOKED_AND_WAIT(
    usbh_control_xfer_subtask(
      new_addr,
      &(tusb_std_request_t)
      {
        .bmRequestType = { .direction = TUSB_DIR_DEV_TO_HOST, .type = TUSB_REQUEST_TYPE_STANDARD, .recipient = TUSB_REQUEST_RECIPIENT_DEVICE },
        .bRequest = TUSB_REQUEST_GET_DESCRIPTOR,
        .wValue   = (TUSB_DESC_CONFIGURATION << 8) | (configure_selected - 1),
        .wLength  = ((tusb_descriptor_configuration_t*) enum_data_buffer)->wTotalLength
      },
      enum_data_buffer
    )
  );

  // update configuration info
  usbh_devices[new_addr].interface_count = ((tusb_descriptor_configuration_t*) enum_data_buffer)->bNumInterfaces;

  //------------- Set Configure -------------//
  OSAL_SUBTASK_INVOKED_AND_WAIT (
    usbh_control_xfer_subtask(
        new_addr,
        &(tusb_std_request_t)
        {
          .bmRequestType = { .direction = TUSB_DIR_HOST_TO_DEV, .type = TUSB_REQUEST_TYPE_STANDARD, .recipient = TUSB_REQUEST_RECIPIENT_DEVICE },
          .bRequest = TUSB_REQUEST_SET_CONFIGURATION,
          .wValue   = configure_selected
        },
        NULL
    )
  );

  usbh_devices[new_addr].state = TUSB_DEVICE_STATE_CONFIGURED;

  //------------- parse configuration & install drivers -------------//
  p_desc = enum_data_buffer + sizeof(tusb_descriptor_configuration_t);

  // parse each interfaces
  while( p_desc < enum_data_buffer + ((tusb_descriptor_configuration_t*)enum_data_buffer)->wTotalLength )
  {
    // skip until we see interface descriptor
    if ( TUSB_DESC_INTERFACE != p_desc[DESCRIPTOR_OFFSET_TYPE] )
    {
      p_desc += p_desc[DESCRIPTOR_OFFSET_LENGTH]; // skip the descriptor, increase by the descriptor's length
    }else
    {
      uint8_t class_code = ((tusb_descriptor_interface_t*) p_desc)->bInterfaceClass;
      if (class_code == 0)
      {
        SUBTASK_ASSERT( false ); // corrupted data, abort enumeration
      }
      // supported class TODO custom class
      else if ( class_code < TUSB_CLASS_MAX_CONSEC_NUMBER && usbh_class_drivers[class_code].open_subtask)
      {
        uint16_t length=0;
        OSAL_SUBTASK_INVOKED_AND_WAIT ( // parameters in task/sub_task must be static storage (static or global)
            usbh_class_drivers[ ((tusb_descriptor_interface_t*) p_desc)->bInterfaceClass ].open_subtask(
                new_addr, (tusb_descriptor_interface_t*) p_desc, &length) );

        // TODO check class_open_subtask status
        if (length == 0) // Interface open failed, for example a subclass is not supported
        {
          p_desc += p_desc[DESCRIPTOR_OFFSET_LENGTH]; // skip this interface, the rest will be skipped by the above loop
          // TODO can optimize the length --> open_subtask return a OPEN FAILED status
        }else
        {
          usbh_devices[new_addr].flag_supported_class |= BIT_(((tusb_descriptor_interface_t*) p_desc)->bInterfaceClass);
          p_desc += length;
        }
      } else // unsupported class (not enable or yet implemented)
      {
        p_desc += p_desc[DESCRIPTOR_OFFSET_LENGTH]; // skip this interface, the rest will be skipped by the above loop
      }
    }
  }

  tusbh_device_mount_succeed_cb(new_addr);

  OSAL_TASK_LOOP_END
}

//--------------------------------------------------------------------+
// REPORTER TASK & ITS DATA
//--------------------------------------------------------------------+



#endif

//--------------------------------------------------------------------+
// INTERNAL HELPER
//--------------------------------------------------------------------+
static inline uint8_t get_new_address(void)
{
  uint8_t addr;
  for (addr=1; addr <= TUSB_CFG_HOST_DEVICE_MAX; addr++)
  {
    if (usbh_devices[addr].state == TUSB_DEVICE_STATE_UNPLUG)
      break;
  }
  return addr;
}

static inline uint8_t get_configure_number_for_device(tusb_descriptor_device_t* dev_desc)
{
  uint8_t config_num = 1;

  // invoke callback to ask user which configuration to select
  if (tusbh_device_attached_cb)
  {
    config_num = min8_of(1, tusbh_device_attached_cb(dev_desc) );
  }

  return config_num;
}
