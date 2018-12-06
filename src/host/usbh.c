/**************************************************************************/
/*!
    @file     usbd_host.c
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
    INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
    LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION HOWEVER CAUSED AND
    ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    INCLUDING NEGLIGENCE OR OTHERWISE ARISING IN ANY WAY OUT OF THE USE OF THIS
    SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

    This file is part of the tinyusb stack.
*/
/**************************************************************************/

#include "common/tusb_common.h"

#if MODE_HOST_SUPPORTED

#define _TINY_USB_SOURCE_FILE_

#ifndef CFG_TUH_TASK_QUEUE_SZ
#define CFG_TUH_TASK_QUEUE_SZ   16
#endif

#ifndef CFG_TUH_TASK_STACK_SZ
#define CFG_TUH_TASK_STACK_SZ 200
#endif

#ifndef CFG_TUH_TASK_PRIO
#define CFG_TUH_TASK_PRIO 0
#endif


//--------------------------------------------------------------------+
// INCLUDE
//--------------------------------------------------------------------+
#include "tusb.h"
#include "hub.h"
#include "usbh_hcd.h"

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF
//--------------------------------------------------------------------+
static host_class_driver_t const usbh_class_drivers[] =
{
  #if HOST_CLASS_HID
    [TUSB_CLASS_HID] = {
        .init         = hidh_init,
        .open_subtask = hidh_open_subtask,
        .isr          = hidh_isr,
        .close        = hidh_close
    },
  #endif

  #if CFG_TUH_CDC
    [TUSB_CLASS_CDC] = {
        .init         = cdch_init,
        .open_subtask = cdch_open_subtask,
        .isr          = cdch_isr,
        .close        = cdch_close
    },
  #endif

  #if CFG_TUH_MSC
    [TUSB_CLASS_MSC] = {
        .init         = msch_init,
        .open_subtask = msch_open_subtask,
        .isr          = msch_isr,
        .close        = msch_close
    },
  #endif

  #if CFG_TUH_HUB
    [TUSB_CLASS_HUB] = {
        .init         = hub_init,
        .open_subtask = hub_open_subtask,
        .isr          = hub_isr,
        .close        = hub_close
    },
  #endif

  #if CFG_TUSB_HOST_CUSTOM_CLASS
    [TUSB_CLASS_MAPPED_INDEX_END-1] = {
        .init         = cush_init,
        .open_subtask = cush_open_subtask,
        .isr          = cush_isr,
        .close        = cush_close
    }
  #endif
};

enum { USBH_CLASS_DRIVER_COUNT = sizeof(usbh_class_drivers) / sizeof(host_class_driver_t) };

//--------------------------------------------------------------------+
// INTERNAL OBJECT & FUNCTION DECLARATION
//--------------------------------------------------------------------+
CFG_TUSB_MEM_SECTION usbh_device_info_t usbh_devices[CFG_TUSB_HOST_DEVICE_MAX+1]; // including zero-address

OSAL_TASK_DEF(_usbh_task_def, "usbh", usbh_task, CFG_TUH_TASK_PRIO, CFG_TUH_TASK_STACK_SZ);

// Event queue
// role device/host is used by OS NONE for mutex (disable usb isr) only
OSAL_QUEUE_DEF(OPT_MODE_HOST, _usbh_qdef, CFG_TUH_TASK_QUEUE_SZ, uint32_t);
static osal_queue_t _usbh_q;

CFG_TUSB_MEM_SECTION ATTR_ALIGNED(4) STATIC_VAR uint8_t enum_data_buffer[CFG_TUSB_HOST_ENUM_BUFFER_SIZE];

//------------- Reporter Task Data -------------//

//------------- Helper Function Prototypes -------------//
static inline uint8_t get_new_address(void) ATTR_ALWAYS_INLINE;
static inline uint8_t get_configure_number_for_device(tusb_desc_device_t* dev_desc) ATTR_ALWAYS_INLINE;

//--------------------------------------------------------------------+
// PUBLIC API (Parameter Verification is required)
//--------------------------------------------------------------------+
tusb_device_state_t tuh_device_get_state (uint8_t const dev_addr)
{
  TU_ASSERT( dev_addr <= CFG_TUSB_HOST_DEVICE_MAX, TUSB_DEVICE_STATE_INVALID_PARAMETER);
  return (tusb_device_state_t) usbh_devices[dev_addr].state;
}

uint32_t tuh_device_get_mounted_class_flag(uint8_t dev_addr)
{
  return tuh_device_is_configured(dev_addr) ? usbh_devices[dev_addr].flag_supported_class : 0;
}

//--------------------------------------------------------------------+
// CLASS-USBD API (don't require to verify parameters)
//--------------------------------------------------------------------+
bool usbh_init(void)
{
  tu_memclr(usbh_devices, sizeof(usbh_device_info_t)*(CFG_TUSB_HOST_DEVICE_MAX+1));

  //------------- Enumeration & Reporter Task init -------------//
  _usbh_q = osal_queue_create( &_usbh_qdef );
  TU_ASSERT(_usbh_q != NULL);

  osal_task_create(&_usbh_task_def);

  //------------- Semaphore, Mutex for Control Pipe -------------//
  for(uint8_t i=0; i<CFG_TUSB_HOST_DEVICE_MAX+1; i++) // including address zero
  {
    usbh_device_info_t * const p_device = &usbh_devices[i];

    p_device->control.sem_hdl = osal_semaphore_create(&p_device->control.sem_def);
    TU_ASSERT(p_device->control.sem_hdl != NULL);

    p_device->control.mutex_hdl = osal_mutex_create(&p_device->control.mutex_def);
    TU_ASSERT(p_device->control.mutex_hdl != NULL);
  }

  //------------- class init -------------//
  for (uint8_t class_index = 1; class_index < USBH_CLASS_DRIVER_COUNT; class_index++)
  {
    if (usbh_class_drivers[class_index].init)
    {
      usbh_class_drivers[class_index].init();
    }
  }

  TU_ASSERT( hcd_init() == TUSB_ERROR_NONE );
  hcd_int_enable(TUH_OPT_RHPORT);

  return true;
}

//------------- USBH control transfer -------------//
bool usbh_control_xfer_subtask (uint8_t dev_addr, uint8_t bmRequestType, uint8_t bRequest,
                                       uint16_t wValue, uint16_t wIndex, uint16_t wLength, uint8_t* data)
{
  usbh_device_info_t* dev = &usbh_devices[dev_addr];

  TU_ASSERT(osal_mutex_lock(dev->control.mutex_hdl, OSAL_TIMEOUT_NORMAL));

  dev->control.request = (tusb_control_request_t ) {
                                                  {.bmRequestType = bmRequestType},
                                                  .bRequest      = bRequest,
                                                  .wValue        = wValue,
                                                  .wIndex        = wIndex,
                                                  .wLength       = wLength
                                           };
  dev->control.pipe_status = 0;

  TU_ASSERT_ERR(hcd_pipe_control_xfer(dev_addr, &dev->control.request, data), false);
  TU_ASSERT(osal_semaphore_wait(dev->control.sem_hdl, OSAL_TIMEOUT_NORMAL));

  osal_mutex_unlock(dev->control.mutex_hdl);

  if ( XFER_RESULT_STALLED == dev->control.pipe_status ) return false;
  if ( XFER_RESULT_FAILED == dev->control.pipe_status ) return false;

//  STASK_ASSERT_HDLR(TUSB_ERROR_NONE == error &&
//                              XFER_RESULT_SUCCESS == dev->control.pipe_status,
//                              tuh_device_mount_failed_cb(TUSB_ERROR_USBH_MOUNT_DEVICE_NOT_RESPOND, NULL) );

  return true;
}

tusb_error_t usbh_pipe_control_open(uint8_t dev_addr, uint8_t max_packet_size)
{
  osal_semaphore_reset( usbh_devices[dev_addr].control.sem_hdl );
  //osal_mutex_reset( usbh_devices[dev_addr].control.mutex_hdl );

  TU_ASSERT_ERR( hcd_pipe_control_open(dev_addr, max_packet_size) );

  return TUSB_ERROR_NONE;
}

static inline tusb_error_t usbh_pipe_control_close(uint8_t dev_addr)
{
  TU_ASSERT_ERR( hcd_pipe_control_close(dev_addr) );

  return TUSB_ERROR_NONE;
}

// TODO [USBH] unify pipe status get
//tusb_interface_status_t usbh_pipe_status_get(pipe_handle_t pipe_hdl)
//{
//  return TUSB_INTERFACE_STATUS_BUSY;
//}

static inline uint8_t std_class_code_to_index(uint8_t std_class_code)
{
  return  (std_class_code <= TUSB_CLASS_AUDIO_VIDEO          ) ? std_class_code                    :
          (std_class_code == TUSB_CLASS_DIAGNOSTIC           ) ? TUSB_CLASS_MAPPED_INDEX_START     :
          (std_class_code == TUSB_CLASS_WIRELESS_CONTROLLER  ) ? TUSB_CLASS_MAPPED_INDEX_START + 1 :
          (std_class_code == TUSB_CLASS_MISC                 ) ? TUSB_CLASS_MAPPED_INDEX_START + 2 :
          (std_class_code == TUSB_CLASS_APPLICATION_SPECIFIC ) ? TUSB_CLASS_MAPPED_INDEX_START + 3 :
          (std_class_code == TUSB_CLASS_VENDOR_SPECIFIC      ) ? TUSB_CLASS_MAPPED_INDEX_START + 4 : 0;
}

//--------------------------------------------------------------------+
// USBH-HCD ISR/Callback API
//--------------------------------------------------------------------+
// interrupt caused by a TD (with IOC=1) in pipe of class class_code
void usbh_xfer_isr(pipe_handle_t pipe_hdl, uint8_t class_code, xfer_result_t event, uint32_t xferred_bytes)
{
  uint8_t class_index = std_class_code_to_index(class_code);
  if (TUSB_XFER_CONTROL == pipe_hdl.xfer_type)
  {
    usbh_devices[ pipe_hdl.dev_addr ].control.pipe_status   = event;
//    usbh_devices[ pipe_hdl.dev_addr ].control.xferred_bytes = xferred_bytes; not yet neccessary
    osal_semaphore_post( usbh_devices[ pipe_hdl.dev_addr ].control.sem_hdl, true );
  }else if (usbh_class_drivers[class_index].isr)
  {
    usbh_class_drivers[class_index].isr(pipe_hdl, event, xferred_bytes);
  }else
  {
    TU_ASSERT(false, ); // something wrong, no one claims the isr's source
  }
}

void usbh_hub_port_plugged_isr(uint8_t hub_addr, uint8_t hub_port)
{
  usbh_enumerate_t enum_entry =
  {
      .core_id  = usbh_devices[hub_addr].core_id,
      .hub_addr = hub_addr,
      .hub_port = hub_port
  };

  osal_queue_send(_usbh_q, &enum_entry, true);
}

void usbh_hcd_rhport_plugged_isr(uint8_t hostid)
{
  usbh_enumerate_t enum_entry =
  {
      .core_id  = hostid,
      .hub_addr = 0,
      .hub_port = 0
  };

  osal_queue_send(_usbh_q, &enum_entry, true);
}

// a device unplugged on hostid, hub_addr, hub_port
// return true if found and unmounted device, false if cannot find
static void usbh_device_unplugged(uint8_t hostid, uint8_t hub_addr, uint8_t hub_port)
{
  bool is_found = false;
  //------------- find the all devices (star-network) under port that is unplugged -------------//
  for (uint8_t dev_addr = 0; dev_addr <= CFG_TUSB_HOST_DEVICE_MAX; dev_addr ++)
  {
    if (usbh_devices[dev_addr].core_id  == hostid   &&
        (hub_addr == 0 || usbh_devices[dev_addr].hub_addr == hub_addr) && // hub_addr == 0 & hub_port == 0 means roothub
        (hub_port == 0 || usbh_devices[dev_addr].hub_port == hub_port) &&
        usbh_devices[dev_addr].state    != TUSB_DEVICE_STATE_UNPLUG)
    {
      // TODO Hub multiple level
      for (uint8_t class_index = 1; class_index < USBH_CLASS_DRIVER_COUNT; class_index++)
      {
        if ((usbh_devices[dev_addr].flag_supported_class & BIT_(class_index)) &&
            usbh_class_drivers[class_index].close)
        {
          usbh_class_drivers[class_index].close(dev_addr);
        }
      }

      // TODO refractor
      // set to REMOVING to allow HCD to clean up its cached data for this device
      // HCD must set this device's state to TUSB_DEVICE_STATE_UNPLUG when done
      usbh_devices[dev_addr].state = TUSB_DEVICE_STATE_REMOVING;
      usbh_devices[dev_addr].flag_supported_class = 0;

      usbh_pipe_control_close(dev_addr);


      is_found = true;
    }
  }

  if (is_found) hcd_port_unplug(usbh_devices[0].core_id); // TODO hack

}

void usbh_hcd_rhport_unplugged_isr(uint8_t hostid)
{
  usbh_enumerate_t enum_entry =
  {
      .core_id = hostid,
      .hub_addr = 0,
      .hub_port = 0
  };

  osal_queue_send(_usbh_q, &enum_entry, true);
}

//--------------------------------------------------------------------+
// ENUMERATION TASK
//--------------------------------------------------------------------+
bool usbh_task_body(void)
{
  enum {
    POWER_STABLE_DELAY = 500,
    RESET_DELAY        = 200 // USB specs say only 50ms but many devices require much longer
  };

  tusb_error_t error;
  usbh_enumerate_t enum_entry;

  // for OSAL_NONE local variable won't retain value after blocking service sem_wait/queue_recv
  static uint8_t new_addr;
  static uint8_t configure_selected = 1; // TODO move
  static uint8_t *p_desc = NULL; // TODO move

  if ( !osal_queue_receive(_usbh_q, &enum_entry) ) return false;

  usbh_devices[0].core_id  = enum_entry.core_id; // TODO refractor integrate to device_pool
  usbh_devices[0].hub_addr = enum_entry.hub_addr;
  usbh_devices[0].hub_port = enum_entry.hub_port;
  usbh_devices[0].state    = TUSB_DEVICE_STATE_UNPLUG;

  //------------- connected/disconnected directly with roothub -------------//
  if ( usbh_devices[0].hub_addr == 0)
  {
    if( hcd_port_connect_status(usbh_devices[0].core_id) )
    {
      // connection event
      osal_task_delay(POWER_STABLE_DELAY); // wait until device is stable. Increase this if the first 8 bytes is failed to get

      // exit if device unplugged while delaying
      if ( !hcd_port_connect_status(usbh_devices[0].core_id) ) return true;

      hcd_port_reset( usbh_devices[0].core_id ); // port must be reset to have correct speed operation
      osal_task_delay(RESET_DELAY);

      usbh_devices[0].speed = hcd_port_speed_get( usbh_devices[0].core_id );
    }
    else
    {
      // disconnection event
      usbh_device_unplugged(usbh_devices[0].core_id, 0, 0);
      return true; // restart task
    }
  }
  #if CFG_TUH_HUB
  //------------- connected/disconnected via hub -------------//
  else
  {
    //------------- Get Port Status -------------//
    TU_VERIFY_HDLR( usbh_control_xfer_subtask( usbh_devices[0].hub_addr, bm_request_type(TUSB_DIR_IN, TUSB_REQ_TYPE_CLASS, TUSB_REQ_RCPT_OTHER),
                                               HUB_REQUEST_GET_STATUS, 0, usbh_devices[0].hub_port,
                                               4, enum_data_buffer )
                    , hub_status_pipe_queue( usbh_devices[0].hub_addr) ); // TODO hub refractor

    // Acknowledge Port Connection Change
    hub_port_clear_feature_subtask(usbh_devices[0].hub_addr, usbh_devices[0].hub_port, HUB_FEATURE_PORT_CONNECTION_CHANGE);

    hub_port_status_response_t * p_port_status;
    p_port_status = ((hub_port_status_response_t *) enum_data_buffer);

    if ( ! p_port_status->status_change.connect_status )   return true; // only handle connection change

    if ( ! p_port_status->status_current.connect_status )
    {
      // Disconnection event
      usbh_device_unplugged(usbh_devices[0].core_id, usbh_devices[0].hub_addr, usbh_devices[0].hub_port);

      (void) hub_status_pipe_queue( usbh_devices[0].hub_addr ); // done with hub, waiting for next data on status pipe
      return true; // restart task
    }
    else
    {
      // Connection Event
      TU_VERIFY_HDLR(hub_port_reset_subtask(usbh_devices[0].hub_addr, usbh_devices[0].hub_port),
                     hub_status_pipe_queue( usbh_devices[0].hub_addr) ); // TODO hub refractor

      usbh_devices[0].speed = hub_port_get_speed();

      // Acknowledge Port Reset Change
      hub_port_clear_feature_subtask(usbh_devices[0].hub_addr, usbh_devices[0].hub_port, HUB_FEATURE_PORT_RESET_CHANGE);
    }
  }
  #endif

  TU_ASSERT_ERR( usbh_pipe_control_open(0, 8) );
  usbh_devices[0].state = TUSB_DEVICE_STATE_ADDRESSED;

  //------------- Get first 8 bytes of device descriptor to get Control Endpoint Size -------------//
  bool is_ok = usbh_control_xfer_subtask(0, bm_request_type(TUSB_DIR_IN, TUSB_REQ_TYPE_STANDARD, TUSB_REQ_RCPT_DEVICE),
                                         TUSB_REQ_GET_DESCRIPTOR,
                                         (TUSB_DESC_DEVICE << 8), 0, 8, enum_data_buffer);

  //------------- Reset device again before Set Address -------------//
  if (usbh_devices[0].hub_addr == 0)
  {
    // connected directly to roothub
    TU_ASSERT(is_ok); // TODO some slow device is observed to fail the very fist controller xfer, can try more times
    hcd_port_reset( usbh_devices[0].core_id ); // reset port after 8 byte descriptor
    osal_task_delay(RESET_DELAY);
  }
  #if CFG_TUH_HUB
  else
  {
    // connected via a hub
    TU_VERIFY_HDLR(is_ok, hub_status_pipe_queue( usbh_devices[0].hub_addr) ); // TODO hub refractor

    if ( hub_port_reset_subtask(usbh_devices[0].hub_addr, usbh_devices[0].hub_port) )
    {
      // Acknowledge Port Reset Change if Reset Successful
      hub_port_clear_feature_subtask(usbh_devices[0].hub_addr, usbh_devices[0].hub_port, HUB_FEATURE_PORT_RESET_CHANGE);
    }

    (void) hub_status_pipe_queue( usbh_devices[0].hub_addr ); // done with hub, waiting for next data on status pipe
  }
  #endif

  //------------- Set new address -------------//
  new_addr = get_new_address();
  TU_ASSERT(new_addr <= CFG_TUSB_HOST_DEVICE_MAX); // TODO notify application we reach max devices

  TU_ASSERT(usbh_control_xfer_subtask( 0, bm_request_type(TUSB_DIR_OUT, TUSB_REQ_TYPE_STANDARD, TUSB_REQ_RCPT_DEVICE),
            TUSB_REQ_SET_ADDRESS, new_addr, 0,
            0, NULL ));

  //------------- update port info & close control pipe of addr0 -------------//
  usbh_devices[new_addr].core_id  = usbh_devices[0].core_id;
  usbh_devices[new_addr].hub_addr = usbh_devices[0].hub_addr;
  usbh_devices[new_addr].hub_port = usbh_devices[0].hub_port;
  usbh_devices[new_addr].speed    = usbh_devices[0].speed;
  usbh_devices[new_addr].state    = TUSB_DEVICE_STATE_ADDRESSED;

  usbh_pipe_control_close(0);
  usbh_devices[0].state = TUSB_DEVICE_STATE_UNPLUG;

  // open control pipe for new address
  TU_ASSERT_ERR ( usbh_pipe_control_open(new_addr, ((tusb_desc_device_t*) enum_data_buffer)->bMaxPacketSize0 ) );

  //------------- Get full device descriptor -------------//
  TU_ASSERT(
      usbh_control_xfer_subtask(new_addr, bm_request_type(TUSB_DIR_IN, TUSB_REQ_TYPE_STANDARD, TUSB_REQ_RCPT_DEVICE),
                                     TUSB_REQ_GET_DESCRIPTOR, (TUSB_DESC_DEVICE << 8), 0,
                                18,
                                enum_data_buffer));

  // update device info  TODO alignment issue
  usbh_devices[new_addr].vendor_id       = ((tusb_desc_device_t*) enum_data_buffer)->idVendor;
  usbh_devices[new_addr].product_id      = ((tusb_desc_device_t*) enum_data_buffer)->idProduct;
  usbh_devices[new_addr].configure_count = ((tusb_desc_device_t*) enum_data_buffer)->bNumConfigurations;

  configure_selected = get_configure_number_for_device((tusb_desc_device_t*) enum_data_buffer);
  TU_ASSERT(configure_selected <= usbh_devices[new_addr].configure_count); // TODO notify application when invalid configuration

  //------------- Get 9 bytes of configuration descriptor -------------//
  TU_ASSERT(
      usbh_control_xfer_subtask(new_addr, bm_request_type(TUSB_DIR_IN, TUSB_REQ_TYPE_STANDARD, TUSB_REQ_RCPT_DEVICE),
                                TUSB_REQ_GET_DESCRIPTOR,
                                (TUSB_DESC_CONFIGURATION << 8) | (configure_selected - 1), 0,
                                9,
                                enum_data_buffer));

  TU_VERIFY_HDLR( CFG_TUSB_HOST_ENUM_BUFFER_SIZE >= ((tusb_desc_configuration_t*)enum_data_buffer)->wTotalLength,
                  tuh_device_mount_failed_cb(TUSB_ERROR_USBH_MOUNT_CONFIG_DESC_TOO_LONG, NULL) );

  //------------- Get full configuration descriptor -------------//
  TU_ASSERT( usbh_control_xfer_subtask( new_addr, bm_request_type(TUSB_DIR_IN, TUSB_REQ_TYPE_STANDARD, TUSB_REQ_RCPT_DEVICE),
                                     TUSB_REQ_GET_DESCRIPTOR, (TUSB_DESC_CONFIGURATION << 8) | (configure_selected - 1), 0,
                                     CFG_TUSB_HOST_ENUM_BUFFER_SIZE, enum_data_buffer ) );

  // update configuration info
  usbh_devices[new_addr].interface_count = ((tusb_desc_configuration_t*) enum_data_buffer)->bNumInterfaces;

  //------------- Set Configure -------------//
  TU_ASSERT( usbh_control_xfer_subtask( new_addr, bm_request_type(TUSB_DIR_OUT, TUSB_REQ_TYPE_STANDARD, TUSB_REQ_RCPT_DEVICE),
                                     TUSB_REQ_SET_CONFIGURATION, configure_selected, 0,
                                     0, NULL ));

  usbh_devices[new_addr].state = TUSB_DEVICE_STATE_CONFIGURED;

  //------------- TODO Get String Descriptors -------------//

  //------------- parse configuration & install drivers -------------//
  p_desc = enum_data_buffer + sizeof(tusb_desc_configuration_t);

  // parse each interfaces
  while( p_desc < enum_data_buffer + ((tusb_desc_configuration_t*)enum_data_buffer)->wTotalLength )
  {
    // skip until we see interface descriptor
    if ( TUSB_DESC_INTERFACE != p_desc[DESC_OFFSET_TYPE] )
    {
      p_desc += p_desc[DESC_OFFSET_LEN]; // skip the descriptor, increase by the descriptor's length
    }else
    {
      static uint8_t class_index; // has to be static as it is used to call class's open_subtask

      class_index = std_class_code_to_index( ((tusb_desc_interface_t*) p_desc)->bInterfaceClass );
      TU_ASSERT( class_index != 0 ); // class_index == 0 means corrupted data, abort enumeration

      if (usbh_class_drivers[class_index].open_subtask &&
          !(class_index == TUSB_CLASS_HUB && usbh_devices[new_addr].hub_addr != 0))
      { // supported class, TODO Hub disable multiple level
        static uint16_t length;
        length = 0;

        if ( usbh_class_drivers[class_index].open_subtask(new_addr, (tusb_desc_interface_t*) p_desc, &length) )
        {
          TU_ASSERT( length >= sizeof(tusb_desc_interface_t) );
          usbh_devices[new_addr].flag_supported_class |= BIT_(class_index);
          p_desc += length;
        }else  // Interface open failed, for example a subclass is not supported
        {
          p_desc += p_desc[DESC_OFFSET_LEN]; // skip this interface, the rest will be skipped by the above loop
        }
      } else // unsupported class (not enable or yet implemented)
      {
        p_desc += p_desc[DESC_OFFSET_LEN]; // skip this interface, the rest will be skipped by the above loop
      }
    }
  }

  tuh_device_mount_succeed_cb(new_addr);

  return true;
}


/* USB Host task
 * Thread that handles all device events. With an real RTOS, the task must be a forever loop and never return.
 * For coding convenience with no RTOS, we use wrapped sub-function for processing to easily return at any time.
 */
void usbh_task(void* param)
{
  (void) param;

#if CFG_TUSB_OS != OPT_OS_NONE
  while (1) {
#endif

  usbh_task_body();

#if CFG_TUSB_OS != OPT_OS_NONE
  }
#endif
}

//--------------------------------------------------------------------+
// REPORTER TASK & ITS DATA
//--------------------------------------------------------------------+





//--------------------------------------------------------------------+
// INTERNAL HELPER
//--------------------------------------------------------------------+
static inline uint8_t get_new_address(void)
{
  uint8_t addr;
  for (addr=1; addr <= CFG_TUSB_HOST_DEVICE_MAX; addr++)
  {
    if (usbh_devices[addr].state == TUSB_DEVICE_STATE_UNPLUG)
      break;
  }
  return addr;
}

static inline uint8_t get_configure_number_for_device(tusb_desc_device_t* dev_desc)
{
  uint8_t config_num = 1;

  // invoke callback to ask user which configuration to select
  if (tuh_device_attached_cb)
  {
    config_num = tu_min8(1, tuh_device_attached_cb(dev_desc) );
  }

  return config_num;
}

#endif
