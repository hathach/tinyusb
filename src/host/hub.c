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

#include "tusb_option.h"

#if (TUSB_OPT_HOST_ENABLED && CFG_TUH_HUB)

//--------------------------------------------------------------------+
// INCLUDE
//--------------------------------------------------------------------+
#include "hub.h"

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF
//--------------------------------------------------------------------+
typedef struct
{
  uint8_t itf_num;
  uint8_t ep_status;
  uint8_t port_number;
  uint8_t status_change; // data from status change interrupt endpoint
}usbh_hub_t;

CFG_TUSB_MEM_SECTION static usbh_hub_t hub_data[CFG_TUSB_HOST_DEVICE_MAX];
TU_ATTR_ALIGNED(4) CFG_TUSB_MEM_SECTION static uint8_t hub_enum_buffer[sizeof(descriptor_hub_desc_t)];

//OSAL_SEM_DEF(hub_enum_semaphore);
//static osal_semaphore_handle_t hub_enum_sem_hdl;

//--------------------------------------------------------------------+
// HUB
//--------------------------------------------------------------------+
bool hub_port_clear_feature_subtask(uint8_t hub_addr, uint8_t hub_port, uint8_t feature)
{
  TU_ASSERT(HUB_FEATURE_PORT_CONNECTION_CHANGE <= feature && feature <= HUB_FEATURE_PORT_RESET_CHANGE);

  tusb_control_request_t request = {
          .bmRequestType_bit = { .recipient = TUSB_REQ_RCPT_OTHER, .type = TUSB_REQ_TYPE_CLASS, .direction = TUSB_DIR_OUT },
          .bRequest = HUB_REQUEST_CLEAR_FEATURE,
          .wValue = feature,
          .wIndex = hub_port,
          .wLength = 0
  };

  //------------- Clear Port Feature request -------------//
  TU_ASSERT( usbh_control_xfer( hub_addr, &request, NULL ) );

  //------------- Get Port Status to check if feature is cleared -------------//
  request = (tusb_control_request_t ) {
        .bmRequestType_bit = { .recipient = TUSB_REQ_RCPT_OTHER, .type = TUSB_REQ_TYPE_CLASS, .direction = TUSB_DIR_IN },
        .bRequest = HUB_REQUEST_GET_STATUS,
        .wValue = 0,
        .wIndex = hub_port,
        .wLength = 4
  };

  TU_ASSERT( usbh_control_xfer( hub_addr, &request, hub_enum_buffer ) );

  //------------- Check if feature is cleared -------------//
  hub_port_status_response_t * p_port_status;
  p_port_status = (hub_port_status_response_t *) hub_enum_buffer;

  TU_ASSERT( !tu_bit_test(p_port_status->status_change.value, feature-16)  );

  return true;
}

bool hub_port_reset_subtask(uint8_t hub_addr, uint8_t hub_port)
{
  enum { RESET_DELAY = 200 }; // USB specs say only 50ms but many devices require much longer

  //------------- Set Port Reset -------------//
  tusb_control_request_t request = {
          .bmRequestType_bit = { .recipient = TUSB_REQ_RCPT_OTHER, .type = TUSB_REQ_TYPE_CLASS, .direction = TUSB_DIR_OUT },
          .bRequest = HUB_REQUEST_SET_FEATURE,
          .wValue = HUB_FEATURE_PORT_RESET,
          .wIndex = hub_port,
          .wLength = 0
  };

  TU_ASSERT( usbh_control_xfer( hub_addr, &request, NULL ) );

  osal_task_delay(RESET_DELAY); // TODO Hub wait for Status Endpoint on Reset Change

  //------------- Get Port Status to check if port is enabled, powered and reset_change -------------//
  request = (tusb_control_request_t ) {
        .bmRequestType_bit = { .recipient = TUSB_REQ_RCPT_OTHER, .type = TUSB_REQ_TYPE_CLASS, .direction = TUSB_DIR_IN },
        .bRequest = HUB_REQUEST_GET_STATUS,
        .wValue = 0,
        .wIndex = hub_port,
        .wLength = 4
  };

  TU_ASSERT( usbh_control_xfer( hub_addr, &request, hub_enum_buffer ) );

  hub_port_status_response_t * p_port_status;
  p_port_status = (hub_port_status_response_t *) hub_enum_buffer;

  TU_ASSERT ( p_port_status->status_change.reset && p_port_status->status_current.connect_status &&
              p_port_status->status_current.port_power && p_port_status->status_current.port_enable);

  return true;
}

// can only get the speed RIGHT AFTER hub_port_reset_subtask call
tusb_speed_t hub_port_get_speed(void)
{
  hub_port_status_response_t * p_port_status = (hub_port_status_response_t *) hub_enum_buffer;
  return (p_port_status->status_current.high_speed_device_attached) ? TUSB_SPEED_HIGH :
         (p_port_status->status_current.low_speed_device_attached ) ? TUSB_SPEED_LOW  : TUSB_SPEED_FULL;
}

//--------------------------------------------------------------------+
// CLASS-USBH API (don't require to verify parameters)
//--------------------------------------------------------------------+
void hub_init(void)
{
  tu_memclr(hub_data, CFG_TUSB_HOST_DEVICE_MAX*sizeof(usbh_hub_t));
//  hub_enum_sem_hdl = osal_semaphore_create( OSAL_SEM_REF(hub_enum_semaphore) );
}

bool hub_open(uint8_t rhport, uint8_t dev_addr, tusb_desc_interface_t const *itf_desc, uint16_t *p_length)
{
  // not support multiple TT yet
  if ( itf_desc->bInterfaceProtocol > 1 ) return false;

  //------------- Open Interrupt Status Pipe -------------//
  tusb_desc_endpoint_t const *ep_desc;
  ep_desc = (tusb_desc_endpoint_t const *) tu_desc_next(itf_desc);

  TU_ASSERT(TUSB_DESC_ENDPOINT == ep_desc->bDescriptorType);
  TU_ASSERT(TUSB_XFER_INTERRUPT == ep_desc->bmAttributes.xfer);
  
  TU_ASSERT(hcd_edpt_open(rhport, dev_addr, ep_desc));

  hub_data[dev_addr-1].itf_num = itf_desc->bInterfaceNumber;
  hub_data[dev_addr-1].ep_status = ep_desc->bEndpointAddress;

  (*p_length) = sizeof(tusb_desc_interface_t) + sizeof(tusb_desc_endpoint_t);

  //------------- Get Hub Descriptor -------------//
  tusb_control_request_t request = {
          .bmRequestType_bit = { .recipient = TUSB_REQ_RCPT_DEVICE, .type = TUSB_REQ_TYPE_CLASS, .direction = TUSB_DIR_IN },
          .bRequest = HUB_REQUEST_GET_DESCRIPTOR,
          .wValue = 0,
          .wIndex = 0,
          .wLength = sizeof(descriptor_hub_desc_t)
  };

  TU_ASSERT( usbh_control_xfer( dev_addr, &request, hub_enum_buffer ) );

  // only care about this field in hub descriptor
  hub_data[dev_addr-1].port_number = ((descriptor_hub_desc_t*) hub_enum_buffer)->bNbrPorts;

  //------------- Set Port_Power on all ports -------------//
  // TODO may only power port with attached
  request = (tusb_control_request_t ) {
          .bmRequestType_bit = { .recipient = TUSB_REQ_RCPT_OTHER, .type = TUSB_REQ_TYPE_CLASS, .direction = TUSB_DIR_OUT },
          .bRequest = HUB_REQUEST_SET_FEATURE,
          .wValue = HUB_FEATURE_PORT_POWER,
          .wIndex = 0,
          .wLength = 0
  };

  for(uint8_t i=1; i <= hub_data[dev_addr-1].port_number; i++)
  {
    request.wIndex = i;
    TU_ASSERT( usbh_control_xfer( dev_addr, &request, NULL ) );
  }

  //------------- Queue the initial Status endpoint transfer -------------//
  TU_ASSERT( hcd_pipe_xfer(dev_addr, hub_data[dev_addr-1].ep_status, &hub_data[dev_addr-1].status_change, 1, true) );

  return true;
}

// is the response of interrupt endpoint polling
#include "usbh_hcd.h" // FIXME remove
void hub_isr(uint8_t dev_addr, uint8_t ep_addr, xfer_result_t event, uint32_t xferred_bytes)
{
  (void) xferred_bytes; // TODO can be more than 1 for hub with lots of ports
  (void) ep_addr;

  usbh_hub_t * p_hub = &hub_data[dev_addr-1];

  if ( event == XFER_RESULT_SUCCESS )
  {
    for (uint8_t port=1; port <= p_hub->port_number; port++)
    {
      // TODO HUB ignore bit0 hub_status_change
      if ( tu_bit_test(p_hub->status_change, port) )
      {
        hcd_event_t event =
        {
          .rhport = _usbh_devices[dev_addr].rhport,
          .event_id = HCD_EVENT_DEVICE_ATTACH
        };

        event.attach.hub_addr = dev_addr;
        event.attach.hub_port = port;

        hcd_event_handler(&event, true);
        break; // handle one port at a time, next port if any will be handled in the next cycle
      }
    }
    // NOTE: next status transfer is queued by usbh.c after handling this request
  }
  else
  {
    // TODO [HUB] check if hub is still plugged before polling status endpoint since failed usually mean hub unplugged
//    TU_ASSERT ( hub_status_pipe_queue(dev_addr) );
  }
}

void hub_close(uint8_t dev_addr)
{
  tu_memclr(&hub_data[dev_addr-1], sizeof(usbh_hub_t));
//  osal_semaphore_reset(hub_enum_sem_hdl);
}

bool hub_status_pipe_queue(uint8_t dev_addr)
{
  return hcd_pipe_xfer(dev_addr, hub_data[dev_addr-1].ep_status, &hub_data[dev_addr-1].status_change, 1, true);
}


#endif
