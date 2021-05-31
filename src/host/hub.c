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

#include "usbh.h"
#include "hub.h"

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF
//--------------------------------------------------------------------+
typedef struct
{
  uint8_t itf_num;
  uint8_t ep_in;
  uint8_t port_count;
  uint8_t status_change; // data from status change interrupt endpoint

  hub_port_status_response_t port_status;
} hub_interface_t;

CFG_TUSB_MEM_SECTION static hub_interface_t hub_data[CFG_TUSB_HOST_DEVICE_MAX];
TU_ATTR_ALIGNED(4) CFG_TUSB_MEM_SECTION static uint8_t _hub_buffer[sizeof(descriptor_hub_desc_t)];

#if CFG_TUSB_DEBUG
static char const* const _hub_feature_str[] =
{
  [HUB_FEATURE_PORT_CONNECTION          ] = "PORT_CONNECTION",
  [HUB_FEATURE_PORT_ENABLE              ] = "PORT_ENABLE",
  [HUB_FEATURE_PORT_SUSPEND             ] = "PORT_SUSPEND",
  [HUB_FEATURE_PORT_OVER_CURRENT        ] = "PORT_OVER_CURRENT",
  [HUB_FEATURE_PORT_RESET               ] = "PORT_RESET",
  [HUB_FEATURE_PORT_POWER               ] = "PORT_POWER",
  [HUB_FEATURE_PORT_LOW_SPEED           ] = "PORT_LOW_SPEED",
  [HUB_FEATURE_PORT_CONNECTION_CHANGE   ] = "PORT_CONNECTION_CHANGE",
  [HUB_FEATURE_PORT_ENABLE_CHANGE       ] = "PORT_ENABLE_CHANGE",
  [HUB_FEATURE_PORT_SUSPEND_CHANGE      ] = "PORT_SUSPEND_CHANGE",
  [HUB_FEATURE_PORT_OVER_CURRENT_CHANGE ] = "PORT_OVER_CURRENT_CHANGE",
  [HUB_FEATURE_PORT_RESET_CHANGE        ] = "PORT_RESET_CHANGE",
  [HUB_FEATURE_PORT_TEST                ] = "PORT_TEST",
  [HUB_FEATURE_PORT_INDICATOR           ] = "PORT_INDICATOR",
};
#endif

//--------------------------------------------------------------------+
// HUB
//--------------------------------------------------------------------+
bool hub_port_clear_feature(uint8_t hub_addr, uint8_t hub_port, uint8_t feature, tuh_control_complete_cb_t complete_cb)
{
  tusb_control_request_t const request =
  {
    .bmRequestType_bit =
    {
      .recipient = TUSB_REQ_RCPT_OTHER,
      .type      = TUSB_REQ_TYPE_CLASS,
      .direction = TUSB_DIR_OUT
    },
    .bRequest = HUB_REQUEST_CLEAR_FEATURE,
    .wValue   = feature,
    .wIndex   = hub_port,
    .wLength  = 0
  };

  TU_LOG2("HUB Clear Feature: %s, addr = %u port = %u\r\n", _hub_feature_str[feature], hub_addr, hub_port);
  TU_ASSERT( tuh_control_xfer(hub_addr, &request, NULL, complete_cb) );
  return true;
}

bool hub_port_set_feature(uint8_t hub_addr, uint8_t hub_port, uint8_t feature, tuh_control_complete_cb_t complete_cb)
{
  tusb_control_request_t const request =
  {
    .bmRequestType_bit =
    {
      .recipient = TUSB_REQ_RCPT_OTHER,
      .type      = TUSB_REQ_TYPE_CLASS,
      .direction = TUSB_DIR_OUT
    },
    .bRequest = HUB_REQUEST_SET_FEATURE,
    .wValue   = feature,
    .wIndex   = hub_port,
    .wLength  = 0
  };

  TU_LOG2("HUB Set Feature: %s, addr = %u port = %u\r\n", _hub_feature_str[feature], hub_addr, hub_port);
  TU_ASSERT( tuh_control_xfer(hub_addr, &request, NULL, complete_cb) );
  return true;
}

bool hub_port_reset(uint8_t hub_addr, uint8_t hub_port, tuh_control_complete_cb_t complete_cb)
{
  return hub_port_set_feature(hub_addr, hub_port, HUB_FEATURE_PORT_RESET, complete_cb);
}

bool hub_port_get_status(uint8_t hub_addr, uint8_t hub_port, void* resp, tuh_control_complete_cb_t complete_cb)
{
  tusb_control_request_t const request =
  {
    .bmRequestType_bit =
    {
      .recipient = TUSB_REQ_RCPT_OTHER,
      .type      = TUSB_REQ_TYPE_CLASS,
      .direction = TUSB_DIR_IN
    },
    .bRequest = HUB_REQUEST_GET_STATUS,
    .wValue   = 0,
    .wIndex   = hub_port,
    .wLength  = 4
  };

  TU_LOG2("HUB Get Port Status: addr = %u port = %u\r\n", hub_addr, hub_port);
  TU_ASSERT( tuh_control_xfer( hub_addr, &request, resp, complete_cb) );
  return true;
}

//--------------------------------------------------------------------+
// CLASS-USBH API (don't require to verify parameters)
//--------------------------------------------------------------------+
void hub_init(void)
{
  tu_memclr(hub_data, CFG_TUSB_HOST_DEVICE_MAX*sizeof( hub_interface_t));
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
  
  TU_ASSERT(usbh_edpt_open(rhport, dev_addr, ep_desc));

  hub_data[dev_addr-1].itf_num = itf_desc->bInterfaceNumber;
  hub_data[dev_addr-1].ep_in   = ep_desc->bEndpointAddress;

  (*p_length) = sizeof(tusb_desc_interface_t) + sizeof(tusb_desc_endpoint_t);

  return true;
}

void hub_close(uint8_t dev_addr)
{
  tu_memclr(&hub_data[dev_addr-1], sizeof( hub_interface_t));
}

bool hub_status_pipe_queue(uint8_t dev_addr)
{
  hub_interface_t * p_hub = &hub_data[dev_addr-1];
  return usbh_edpt_xfer(dev_addr, p_hub->ep_in, &p_hub->status_change, 1);
}


//--------------------------------------------------------------------+
// Set Configure
//--------------------------------------------------------------------+

static bool config_set_port_power (uint8_t dev_addr, tusb_control_request_t const * request, xfer_result_t result);
static bool config_port_power_complete (uint8_t dev_addr, tusb_control_request_t const * request, xfer_result_t result);

bool hub_set_config(uint8_t dev_addr, uint8_t itf_num)
{
   hub_interface_t* p_hub = &hub_data[dev_addr-1];
  TU_ASSERT(itf_num == p_hub->itf_num);

  // Get Hub Descriptor
  tusb_control_request_t const request =
  {
    .bmRequestType_bit =
    {
      .recipient = TUSB_REQ_RCPT_DEVICE,
      .type      = TUSB_REQ_TYPE_CLASS,
      .direction = TUSB_DIR_IN
    },
    .bRequest = HUB_REQUEST_GET_DESCRIPTOR,
    .wValue   = 0,
    .wIndex   = 0,
    .wLength  = sizeof(descriptor_hub_desc_t)
  };

  TU_ASSERT( tuh_control_xfer(dev_addr, &request, _hub_buffer, config_set_port_power) );

  return true;
}

static bool config_set_port_power (uint8_t dev_addr, tusb_control_request_t const * request, xfer_result_t result)
{
  (void) request;
  TU_ASSERT(XFER_RESULT_SUCCESS == result);

  hub_interface_t* p_hub = &hub_data[dev_addr-1];

  // only use number of ports in hub descriptor
  descriptor_hub_desc_t const* desc_hub = (descriptor_hub_desc_t const*) _hub_buffer;
  p_hub->port_count = desc_hub->bNbrPorts;

  // May need to GET_STATUS

  // Set Port Power to be able to detect connection, starting with port 1
  uint8_t const hub_port = 1;
  return hub_port_set_feature(dev_addr, hub_port, HUB_FEATURE_PORT_POWER, config_port_power_complete);
}

static bool config_port_power_complete (uint8_t dev_addr, tusb_control_request_t const * request, xfer_result_t result)
{
  TU_ASSERT(XFER_RESULT_SUCCESS == result);
   hub_interface_t* p_hub = &hub_data[dev_addr-1];

  if (request->wIndex == p_hub->port_count)
  {
    // All ports are power -> queue notification status endpoint and
    // complete the SET CONFIGURATION
    TU_ASSERT( usbh_edpt_xfer(dev_addr, p_hub->ep_in, &p_hub->status_change, 1) );

    usbh_driver_set_config_complete(dev_addr, p_hub->itf_num);
  }else
  {
    // power next port
    uint8_t const hub_port = (uint8_t) (request->wIndex + 1);
    return hub_port_set_feature(dev_addr, hub_port, HUB_FEATURE_PORT_POWER, config_port_power_complete);
  }

  return true;
}

//--------------------------------------------------------------------+
// Connection Changes
//--------------------------------------------------------------------+

static bool connection_get_status_complete (uint8_t dev_addr, tusb_control_request_t const * request, xfer_result_t result);
static bool connection_clear_conn_change_complete (uint8_t dev_addr, tusb_control_request_t const * request, xfer_result_t result);
static bool connection_port_reset_complete (uint8_t dev_addr, tusb_control_request_t const * request, xfer_result_t result);

// callback as response of interrupt endpoint polling
bool hub_xfer_cb(uint8_t dev_addr, uint8_t ep_addr, xfer_result_t result, uint32_t xferred_bytes)
{
  (void) xferred_bytes; // TODO can be more than 1 for hub with lots of ports
  (void) ep_addr;
  TU_ASSERT(result == XFER_RESULT_SUCCESS);

  hub_interface_t * p_hub = &hub_data[dev_addr-1];

  TU_LOG2("  Port Status Change = 0x%02X\r\n", p_hub->status_change);

  // Hub ignore bit0 in status change
  for (uint8_t port=1; port <= p_hub->port_count; port++)
  {
    if ( tu_bit_test(p_hub->status_change, port) )
    {
      hub_port_get_status(dev_addr, port, &p_hub->port_status, connection_get_status_complete);
      break;
    }
  }

  // NOTE: next status transfer is queued by usbh.c after handling this request

  return true;
}

static bool connection_get_status_complete (uint8_t dev_addr, tusb_control_request_t const * request, xfer_result_t result)
{
  TU_ASSERT(result == XFER_RESULT_SUCCESS);
  hub_interface_t * p_hub = &hub_data[dev_addr-1];
  uint8_t const port_num = (uint8_t) request->wIndex;

  // Connection change
  if (p_hub->port_status.change.connection)
  {
    // Port is powered and enabled
    //TU_VERIFY(port_status.status_current.port_power && port_status.status_current.port_enable, );

    // Acknowledge Port Connection Change
    hub_port_clear_feature(dev_addr, port_num, HUB_FEATURE_PORT_CONNECTION_CHANGE, connection_clear_conn_change_complete);
  }else
  {
    // Other changes are: Enable, Suspend, Over Current, Reset, L1 state
    // TODO clear change

    // prepare for next hub status
    // TODO continue with status_change, or maybe we can do it again with status
    hub_status_pipe_queue(dev_addr);
  }

  return true;
}

static bool connection_clear_conn_change_complete (uint8_t dev_addr, tusb_control_request_t const * request, xfer_result_t result)
{
  TU_ASSERT(result == XFER_RESULT_SUCCESS);

  hub_interface_t * p_hub = &hub_data[dev_addr-1];
  uint8_t const port_num = (uint8_t) request->wIndex;

  if ( p_hub->port_status.status.connection )
  {
    // Reset port if attach event
    hub_port_reset(dev_addr, port_num, connection_port_reset_complete);
  }else
  {
    // submit detach event
    hcd_event_t event =
    {
      .rhport     = usbh_get_rhport(dev_addr),
      .event_id   = HCD_EVENT_DEVICE_REMOVE,
      .connection =
       {
         .hub_addr = dev_addr,
         .hub_port = port_num
       }
    };

    hcd_event_handler(&event, false);
  }

  return true;
}

static bool connection_port_reset_complete (uint8_t dev_addr, tusb_control_request_t const * request, xfer_result_t result)
{
  TU_ASSERT(result == XFER_RESULT_SUCCESS);

  // usbh_hub_t * p_hub = &hub_data[dev_addr-1];
  uint8_t const port_num = (uint8_t) request->wIndex;

  // submit attach event
  hcd_event_t event =
  {
    .rhport     = usbh_get_rhport(dev_addr),
    .event_id   = HCD_EVENT_DEVICE_ATTACH,
    .connection =
    {
      .hub_addr = dev_addr,
      .hub_port = port_num
    }
  };

  hcd_event_handler(&event, false);

  return true;
}

#endif
