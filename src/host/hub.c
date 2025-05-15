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

#if (CFG_TUH_ENABLED && CFG_TUH_HUB)

#include "hcd.h"
#include "usbh.h"
#include "usbh_pvt.h"
#include "hub.h"

// Debug level, TUSB_CFG_DEBUG must be at least this level for debug message
#define HUB_DEBUG   2
#define TU_LOG_DRV(...)   TU_LOG(HUB_DEBUG, __VA_ARGS__)

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF
//--------------------------------------------------------------------+
typedef struct {
  uint8_t itf_num;
  uint8_t ep_in;

  // from hub descriptor
  uint8_t bNbrPorts;
  uint8_t bPwrOn2PwrGood_2ms; // port power on to good, in 2ms unit
  // uint16_t wHubCharacteristics;

  hub_port_status_response_t port_status;
} hub_interface_t;

typedef struct {
  TUH_EPBUF_DEF(status_change, 4); // interrupt endpoint
  TUH_EPBUF_DEF(ctrl_buf, CFG_TUH_HUB_BUFSIZE);
} hub_epbuf_t;

static tuh_xfer_cb_t user_complete_cb = NULL;
static hub_interface_t hub_itfs[CFG_TUH_HUB];
CFG_TUH_MEM_SECTION static hub_epbuf_t hub_epbufs[CFG_TUH_HUB];


TU_ATTR_ALWAYS_INLINE static inline hub_interface_t* get_hub_itf(uint8_t daddr) {
  return &hub_itfs[daddr-1-CFG_TUH_DEVICE_MAX];
}

TU_ATTR_ALWAYS_INLINE static inline hub_epbuf_t* get_hub_epbuf(uint8_t daddr) {
  return &hub_epbufs[daddr-1-CFG_TUH_DEVICE_MAX];
}

#if CFG_TUSB_DEBUG >= HUB_DEBUG
static char const* const _hub_feature_str[] = {
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
bool hub_port_clear_feature(uint8_t hub_addr, uint8_t hub_port, uint8_t feature,
                            tuh_xfer_cb_t complete_cb, uintptr_t user_data) {
  tusb_control_request_t const request = {
    .bmRequestType_bit = {
      .recipient = (hub_port == 0) ? TUSB_REQ_RCPT_DEVICE : TUSB_REQ_RCPT_OTHER,
      .type      = TUSB_REQ_TYPE_CLASS,
      .direction = TUSB_DIR_OUT
    },
    .bRequest = HUB_REQUEST_CLEAR_FEATURE,
    .wValue   = feature,
    .wIndex   = hub_port,
    .wLength  = 0
  };

  tuh_xfer_t xfer = {
    .daddr       = hub_addr,
    .ep_addr     = 0,
    .setup       = &request,
    .buffer      = NULL,
    .complete_cb = complete_cb,
    .user_data   = user_data
  };

  TU_LOG_DRV("HUB Clear Feature: %s, addr = %u port = %u\r\n", _hub_feature_str[feature], hub_addr, hub_port);
  TU_ASSERT(tuh_control_xfer(&xfer));
  return true;
}

bool hub_port_set_feature(uint8_t hub_addr, uint8_t hub_port, uint8_t feature,
                          tuh_xfer_cb_t complete_cb, uintptr_t user_data) {
  tusb_control_request_t const request = {
    .bmRequestType_bit = {
      .recipient = (hub_port == 0) ? TUSB_REQ_RCPT_DEVICE : TUSB_REQ_RCPT_OTHER,
      .type      = TUSB_REQ_TYPE_CLASS,
      .direction = TUSB_DIR_OUT
    },
    .bRequest = HUB_REQUEST_SET_FEATURE,
    .wValue   = feature,
    .wIndex   = hub_port,
    .wLength  = 0
  };

  tuh_xfer_t xfer = {
    .daddr       = hub_addr,
    .ep_addr     = 0,
    .setup       = &request,
    .buffer      = NULL,
    .complete_cb = complete_cb,
    .user_data   = user_data
  };

  TU_LOG_DRV("HUB Set Feature: %s, addr = %u port = %u\r\n", _hub_feature_str[feature], hub_addr, hub_port);
  TU_ASSERT(tuh_control_xfer(&xfer));
  return true;
}

static void port_get_status_complete (tuh_xfer_t* xfer) {
  if (xfer->result == XFER_RESULT_SUCCESS) {
    hub_interface_t* p_hub = get_hub_itf(xfer->daddr);
    p_hub->port_status = *((const hub_port_status_response_t *) (uintptr_t) xfer->buffer);
  }

  xfer->complete_cb = user_complete_cb;
  user_complete_cb = NULL;
  if (xfer->complete_cb) {
    xfer->complete_cb(xfer);
  }
}

bool hub_port_get_status(uint8_t hub_addr, uint8_t hub_port, void* resp,
                         tuh_xfer_cb_t complete_cb, uintptr_t user_data) {
  tusb_control_request_t const request = {
    .bmRequestType_bit = {
      .recipient = (hub_port == 0) ? TUSB_REQ_RCPT_DEVICE : TUSB_REQ_RCPT_OTHER,
      .type      = TUSB_REQ_TYPE_CLASS,
      .direction = TUSB_DIR_IN
    },
    .bRequest = HUB_REQUEST_GET_STATUS,
    .wValue   = 0,
    .wIndex   = hub_port,
    .wLength  = tu_htole16(4)
  };

  tuh_xfer_t xfer = {
    .daddr       = hub_addr,
    .ep_addr     = 0,
    .setup       = &request,
    .buffer      = resp,
    .complete_cb = complete_cb,
    .user_data   = user_data
  };

  if (hub_port != 0) {
    // intercept complete callback to save port status, ignore resp
    hub_epbuf_t* p_epbuf = get_hub_epbuf(hub_addr);
    xfer.complete_cb = port_get_status_complete;
    xfer.buffer = p_epbuf->ctrl_buf;
    user_complete_cb = complete_cb;
  } else {
    user_complete_cb = NULL;
  }

  TU_LOG_DRV("HUB Get Port Status: addr = %u port = %u\r\n", hub_addr, hub_port);
  TU_VERIFY(tuh_control_xfer(&xfer));
  return true;
}

bool hub_port_get_status_local(uint8_t hub_addr, uint8_t hub_port, hub_port_status_response_t* resp) {
  (void) hub_port;
  hub_interface_t* p_hub = get_hub_itf(hub_addr);
  *resp = p_hub->port_status;
  return true;
}

//--------------------------------------------------------------------+
// CLASS-USBH API (don't require to verify parameters)
//--------------------------------------------------------------------+
bool hub_init(void) {
  tu_memclr(hub_itfs, sizeof(hub_itfs));
  return true;
}

bool hub_deinit(void) {
  return true;
}

bool hub_open(uint8_t rhport, uint8_t dev_addr, tusb_desc_interface_t const *itf_desc, uint16_t max_len) {
  (void) rhport;

  TU_VERIFY(TUSB_CLASS_HUB == itf_desc->bInterfaceClass &&
            0              == itf_desc->bInterfaceSubClass);
  TU_VERIFY(itf_desc->bInterfaceProtocol <= 1); // not support multiple TT yet

  uint16_t const drv_len = sizeof(tusb_desc_interface_t) + sizeof(tusb_desc_endpoint_t);
  TU_ASSERT(drv_len <= max_len);

  // Interrupt Status endpoint
  tusb_desc_endpoint_t const *desc_ep = (tusb_desc_endpoint_t const *) tu_desc_next(itf_desc);
  TU_ASSERT(TUSB_DESC_ENDPOINT  == desc_ep->bDescriptorType &&
            TUSB_XFER_INTERRUPT == desc_ep->bmAttributes.xfer, 0);
  TU_ASSERT(tuh_edpt_open(dev_addr, desc_ep));

  hub_interface_t* p_hub = get_hub_itf(dev_addr);
  p_hub->itf_num = itf_desc->bInterfaceNumber;
  p_hub->ep_in   = desc_ep->bEndpointAddress;

  return true;
}

void hub_close(uint8_t dev_addr) {
  TU_VERIFY(dev_addr > CFG_TUH_DEVICE_MAX, );
  hub_interface_t* p_hub = get_hub_itf(dev_addr);

  if (p_hub->ep_in) {
    TU_LOG_DRV("  HUB close addr = %d\r\n", dev_addr);
    tu_memclr(p_hub, sizeof( hub_interface_t));
  }
}

bool hub_edpt_status_xfer(uint8_t daddr) {
  hub_interface_t* p_hub = get_hub_itf(daddr);
  hub_epbuf_t* p_epbuf = get_hub_epbuf(daddr);

  TU_VERIFY(usbh_edpt_claim(daddr, p_hub->ep_in));
  if (!usbh_edpt_xfer(daddr, p_hub->ep_in, p_epbuf->status_change, 1)) {
    usbh_edpt_release(daddr, p_hub->ep_in);
    return false;
  }

  return true;
}

//--------------------------------------------------------------------+
// Set Configure
//--------------------------------------------------------------------+
static void config_set_port_power (tuh_xfer_t* xfer);
static void config_port_power_complete (tuh_xfer_t* xfer);

bool hub_set_config(uint8_t daddr, uint8_t itf_num) {
  hub_interface_t* p_hub = get_hub_itf(daddr);
  TU_ASSERT(itf_num == p_hub->itf_num);
  hub_epbuf_t* p_epbuf = get_hub_epbuf(daddr);

  // Get Hub Descriptor
  tusb_control_request_t const request = {
    .bmRequestType_bit = {
      .recipient = TUSB_REQ_RCPT_DEVICE,
      .type      = TUSB_REQ_TYPE_CLASS,
      .direction = TUSB_DIR_IN
    },
    .bRequest = HUB_REQUEST_GET_DESCRIPTOR,
    .wValue   = 0,
    .wIndex   = 0,
    .wLength  = sizeof(hub_desc_cs_t)
  };

  tuh_xfer_t xfer = {
    .daddr       = daddr,
    .ep_addr     = 0,
    .setup       = &request,
    .buffer      = p_epbuf->ctrl_buf,
    .complete_cb = config_set_port_power,
    .user_data    = 0
  };

  TU_ASSERT(tuh_control_xfer(&xfer));
  return true;
}

static void config_set_port_power (tuh_xfer_t* xfer) {
  TU_ASSERT(XFER_RESULT_SUCCESS == xfer->result, );

  uint8_t const daddr = xfer->daddr;
  hub_interface_t* p_hub = get_hub_itf(daddr);
  hub_epbuf_t* p_epbuf = get_hub_epbuf(daddr);

  // only use number of ports in hub descriptor
  hub_desc_cs_t const* desc_hub = (hub_desc_cs_t const*) p_epbuf->ctrl_buf;
  p_hub->bNbrPorts = desc_hub->bNbrPorts;
  p_hub->bPwrOn2PwrGood_2ms = desc_hub->bPwrOn2PwrGood;

  // May need to GET_STATUS

  // Set Port Power to be able to detect connection, starting with port 1
  uint8_t const hub_port = 1;
  hub_port_set_feature(daddr, hub_port, HUB_FEATURE_PORT_POWER, config_port_power_complete, 0);
}

static void config_port_power_complete (tuh_xfer_t* xfer) {
  TU_ASSERT(XFER_RESULT_SUCCESS == xfer->result, );

  uint8_t const daddr = xfer->daddr;
  hub_interface_t* p_hub = get_hub_itf(daddr);

  if (xfer->setup->wIndex == p_hub->bNbrPorts) {
    // All ports are power -> queue notification status endpoint and
    // complete the SET CONFIGURATION
    if (!hub_edpt_status_xfer(daddr)) {
      TU_MESS_FAILED();
      TU_BREAKPOINT();
    }
    usbh_driver_set_config_complete(daddr, p_hub->itf_num);
  } else {
    // power next port
    uint8_t const hub_port = (uint8_t) (xfer->setup->wIndex + 1);
    hub_port_set_feature(daddr, hub_port, HUB_FEATURE_PORT_POWER, config_port_power_complete, 0);
  }
}

//--------------------------------------------------------------------+
// Connection Changes
//--------------------------------------------------------------------+
enum {
  STATE_IDLE = 0,
  STATE_HUB_STATUS,
  STATE_CLEAR_CHANGE,
  STATE_CHECK_CONN,
  STATE_COMPLETE
};

static void process_new_status(tuh_xfer_t* xfer);

// callback as response of interrupt endpoint polling
bool hub_xfer_cb(uint8_t daddr, uint8_t ep_addr, xfer_result_t result, uint32_t xferred_bytes) {
  (void) xferred_bytes;
  (void) ep_addr;

  bool processed = false; // true if new status is processed

  if (result == XFER_RESULT_SUCCESS) {
    hub_interface_t* p_hub = get_hub_itf(daddr);
    hub_epbuf_t *p_epbuf = get_hub_epbuf(daddr);
    const uint8_t status_change = p_epbuf->status_change[0];
    TU_LOG_DRV("  Hub Status Change = 0x%02X\r\n", status_change);

    if (status_change == 0) {
      // The status change event was neither for the hub, nor for any of its ports.
      // This shouldn't happen, but it does with some devices. Re-Initiate the interrupt poll.
      processed = false;
    } else if (tu_bit_test(status_change, 0)) {
      // Hub bit 0 is for the hub device events
      processed = hub_get_status(daddr, p_epbuf->ctrl_buf, process_new_status, STATE_HUB_STATUS);
    } else {
      // Hub bits 1 to n are hub port events
      for (uint8_t port=1; port <= p_hub->bNbrPorts; port++) {
        if (tu_bit_test(status_change, port)) {
          processed = hub_port_get_status(daddr, port, NULL, process_new_status, STATE_CLEAR_CHANGE);
          break; // after completely processed one port, we will re-queue the status poll and handle next one
        }
      }
    }
  }

  // If new status event is processed: next status pool is queued by usbh.c after handled this request
  // Otherwise re-queue the status poll here
  if (!processed) {
    TU_ASSERT(hub_edpt_status_xfer(daddr));
  }

  return true;
}

static void process_new_status(tuh_xfer_t* xfer) {
  const uint8_t daddr = xfer->daddr;

  if (xfer->result != XFER_RESULT_SUCCESS) {
    TU_ASSERT(hub_edpt_status_xfer(daddr),);
    return;
  }

  const uint8_t port_num = (uint8_t) tu_le16toh(xfer->setup->wIndex);
  hub_interface_t *p_hub = get_hub_itf(daddr);
  const uintptr_t state = xfer->user_data;
  bool processed = false; // true if new status is processed

  switch (state) {
    case STATE_HUB_STATUS: {
      hub_status_response_t hub_status = *((const hub_status_response_t *) (uintptr_t) xfer->buffer);
      TU_LOG_DRV("HUB Got hub status, addr = %u, status = %04x\r\n", daddr, hub_status.change.value);
      if (hub_status.change.local_power_source) {
        TU_LOG_DRV("  Local Power Change\r\n");
        processed = hub_clear_feature(daddr, HUB_FEATURE_HUB_LOCAL_POWER_CHANGE,
                                      process_new_status, STATE_COMPLETE);
      } else if (hub_status.change.over_current) {
        TU_LOG_DRV("  Over Current\r\n");
        processed = hub_clear_feature(daddr, HUB_FEATURE_HUB_OVER_CURRENT_CHANGE,
                                      process_new_status, STATE_COMPLETE);
      }
      break;
    }

    case STATE_CLEAR_CHANGE:
      // Get port status complete --> clear change
      if (p_hub->port_status.change.connection) {
        // Connection change
        // Port is powered and enabled
        //TU_VERIFY(port_status.status_current.port_power && port_status.status_current.port_enable, );

        // Acknowledge Port Connection Change
        processed = hub_port_clear_feature(daddr, port_num, HUB_FEATURE_PORT_CONNECTION_CHANGE,
                                           process_new_status, STATE_CHECK_CONN);
      } else if (p_hub->port_status.change.port_enable) {
        processed = hub_port_clear_feature(daddr, port_num, HUB_FEATURE_PORT_ENABLE_CHANGE,
                                           process_new_status, STATE_COMPLETE);
      } else if (p_hub->port_status.change.suspend) {
        processed = hub_port_clear_feature(daddr, port_num, HUB_FEATURE_PORT_SUSPEND_CHANGE,
                                           process_new_status, STATE_COMPLETE);
      } else if (p_hub->port_status.change.over_current) {
        processed = hub_port_clear_feature(daddr, port_num, HUB_FEATURE_PORT_OVER_CURRENT_CHANGE,
                                           process_new_status, STATE_COMPLETE);
      } else if (p_hub->port_status.change.reset) {
        processed = hub_port_clear_feature(daddr, port_num, HUB_FEATURE_PORT_RESET_CHANGE,
                                           process_new_status, STATE_COMPLETE);
      }
      break;

    case STATE_CHECK_CONN: {
      const hcd_event_t event = {
        .rhport     = usbh_get_rhport(daddr),
        .event_id   = p_hub->port_status.status.connection ? HCD_EVENT_DEVICE_ATTACH : HCD_EVENT_DEVICE_REMOVE,
        .connection = {
          .hub_addr = daddr,
          .hub_port = port_num
        }
      };
      hcd_event_handler(&event, false);
      // skip status for attach event, usbh will do it after handled this enumeration
      processed = (event.event_id == HCD_EVENT_DEVICE_ATTACH);
      break;
    }

    case STATE_COMPLETE:
    default:
      processed = false; // complete this status, queue next status
      break;

  }

  if (!processed) {
    TU_ASSERT(hub_edpt_status_xfer(daddr),);
  }
}

#endif
