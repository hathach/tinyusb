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

#ifndef TUSB_HUB_H_
#define TUSB_HUB_H_

#include "common/tusb_common.h"

#ifdef __cplusplus
 extern "C" {
#endif

//--------------------------------------------------------------------+
// Configuration
//--------------------------------------------------------------------+

#ifndef CFG_TUH_HUB_BUFSIZE
  #define CFG_TUH_HUB_BUFSIZE 12
#endif

//--------------------------------------------------------------------+
//
//--------------------------------------------------------------------+
enum {
  HUB_REQUEST_GET_STATUS      = 0  ,
  HUB_REQUEST_CLEAR_FEATURE   = 1  ,
  // 2 is reserved
  HUB_REQUEST_SET_FEATURE     = 3  ,
  // 4-5 are reserved
  HUB_REQUEST_GET_DESCRIPTOR  = 6  ,
  HUB_REQUEST_SET_DESCRIPTOR  = 7  ,
  HUB_REQUEST_CLEAR_TT_BUFFER = 8  ,
  HUB_REQUEST_RESET_TT        = 9  ,
  HUB_REQUEST_GET_TT_STATE    = 10 ,
  HUB_REQUEST_STOP_TT         = 11
};

enum {
  HUB_FEATURE_HUB_LOCAL_POWER_CHANGE = 0,
  HUB_FEATURE_HUB_OVER_CURRENT_CHANGE
};

enum{
  HUB_FEATURE_PORT_CONNECTION          = 0,
  HUB_FEATURE_PORT_ENABLE              = 1,
  HUB_FEATURE_PORT_SUSPEND             = 2,
  HUB_FEATURE_PORT_OVER_CURRENT        = 3,
  HUB_FEATURE_PORT_RESET               = 4,
  // 5-7 are reserved
  HUB_FEATURE_PORT_POWER               = 8,
  HUB_FEATURE_PORT_LOW_SPEED           = 9,
  // 10-15 are reserved
  HUB_FEATURE_PORT_CONNECTION_CHANGE   = 16,
  HUB_FEATURE_PORT_ENABLE_CHANGE       = 17,
  HUB_FEATURE_PORT_SUSPEND_CHANGE      = 18,
  HUB_FEATURE_PORT_OVER_CURRENT_CHANGE = 19,
  HUB_FEATURE_PORT_RESET_CHANGE        = 20,
  HUB_FEATURE_PORT_TEST                = 21,
  HUB_FEATURE_PORT_INDICATOR           = 22
};

enum {
  HUB_CHARS_POWER_GANGED_SWITCHING = 0,
  HUB_CHARS_POWER_INDIVIDUAL_SWITCHING = 1,
};

enum {
  HUB_CHARS_OVER_CURRENT_GLOBAL = 0,
  HUB_CHARS_OVER_CURRENT_INDIVIDUAL = 1,
};

typedef struct TU_ATTR_PACKED{
  uint8_t  bLength           ; ///< Size of descriptor
  uint8_t  bDescriptorType   ; ///< Other_speed_Configuration Type
  uint8_t  bNbrPorts;
  uint16_t wHubCharacteristics;
  uint8_t  bPwrOn2PwrGood;
  uint8_t  bHubContrCurrent;
  uint8_t  DeviceRemovable; // bitmap each bit for a port (from bit1)
  uint8_t  PortPwrCtrlMask; // just for compatibility, should be 0xff
} hub_desc_cs_t;
TU_VERIFY_STATIC(sizeof(hub_desc_cs_t) == 9, "size is not correct");
TU_VERIFY_STATIC(CFG_TUH_HUB_BUFSIZE >= sizeof(hub_desc_cs_t), "buffer is not big enough");

typedef struct TU_ATTR_PACKED {
  struct TU_ATTR_PACKED {
    uint8_t logical_power_switching_mode : 2; // [0..1] gannged or individual power switching
    uint8_t compound_device              : 1; // [2] hub is part of compound device
    uint8_t over_current_protect_mode    : 2; // [3..4] global or individual port over-current protection
    uint8_t tt_think_time                : 2; // [5..6] TT think time
    uint8_t port_indicator_supported     : 1; // [7] port indicator supported
  };
  uint8_t rsv1;
} hub_characteristics_t;
TU_VERIFY_STATIC(sizeof(hub_characteristics_t) == 2, "size is not correct");

// data in response of HUB_REQUEST_GET_STATUS, wIndex = 0 (hub)
typedef struct {
  union{
    struct TU_ATTR_PACKED {
      uint16_t local_power_source : 1;
      uint16_t over_current       : 1;
      uint16_t : 14;
    };

    uint16_t value;
  } status, change;
} hub_status_response_t;
TU_VERIFY_STATIC( sizeof(hub_status_response_t) == 4, "size is not correct");

// data in response of HUB_REQUEST_GET_STATUS, wIndex = Port num
typedef struct {
  union TU_ATTR_PACKED {
    struct TU_ATTR_PACKED {
      // Bit 0-4 are for change & status
      uint16_t connection             : 1; // [0] 0 = no device, 1 = device connected
      uint16_t port_enable            : 1; // [1] port is enabled
      uint16_t suspend                : 1; // [2]
      uint16_t over_current           : 1; // [3] over-current exists
      uint16_t reset                  : 1; // [4] 0 = no reset, 1 = resetting

      // From Bit 5 are for status only
      uint16_t rsv5_7                 : 3; // [5..7] reserved
      uint16_t port_power             : 1; // [8] 0 = port is off, 1 = port is on
      uint16_t low_speed              : 1; // [9] low speed device attached
      uint16_t high_speed             : 1; // [10] high speed device attached
      uint16_t port_test_mode         : 1; // [11] port in test mode
      uint16_t port_indicator_control : 1; // [12] 0: default color, 1: indicator is software controlled
      uint16_t TU_RESERVED            : 3; // [13..15] reserved
    };

    uint16_t value;
  } status, change;
} hub_port_status_response_t;
TU_VERIFY_STATIC( sizeof(hub_port_status_response_t) == 4, "size is not correct");

//--------------------------------------------------------------------+
// HUB API
//--------------------------------------------------------------------+

// Clear port feature
bool hub_port_clear_feature(uint8_t hub_addr, uint8_t hub_port, uint8_t feature,
                            tuh_xfer_cb_t complete_cb, uintptr_t user_data);

// Set port feature
bool hub_port_set_feature(uint8_t hub_addr, uint8_t hub_port, uint8_t feature,
                          tuh_xfer_cb_t complete_cb, uintptr_t user_data);

// Get port status
// If hub_port != 0, resp is ignored. hub_port_get_status_local() can be used to retrieve the status
bool hub_port_get_status(uint8_t hub_addr, uint8_t hub_port, void *resp,
                         tuh_xfer_cb_t complete_cb, uintptr_t user_data);

// Get port status from local cache. This does not send a request to the device
bool hub_port_get_status_local(uint8_t hub_addr, uint8_t hub_port, hub_port_status_response_t* resp);

// Get status from Interrupt endpoint
bool hub_edpt_status_xfer(uint8_t daddr);

// Reset a port
TU_ATTR_ALWAYS_INLINE static inline
bool hub_port_reset(uint8_t hub_addr, uint8_t hub_port, tuh_xfer_cb_t complete_cb, uintptr_t user_data) {
  return hub_port_set_feature(hub_addr, hub_port, HUB_FEATURE_PORT_RESET, complete_cb, user_data);
}

// Clear Port Reset Change
TU_ATTR_ALWAYS_INLINE static inline
bool hub_port_clear_reset_change(uint8_t hub_addr, uint8_t hub_port, tuh_xfer_cb_t complete_cb, uintptr_t user_data) {
  return hub_port_clear_feature(hub_addr, hub_port, HUB_FEATURE_PORT_RESET_CHANGE, complete_cb, user_data);
}

// Get Hub status (port = 0)
TU_ATTR_ALWAYS_INLINE static inline
bool hub_get_status(uint8_t hub_addr, void* resp, tuh_xfer_cb_t complete_cb, uintptr_t user_data) {
  return hub_port_get_status(hub_addr, 0, resp, complete_cb, user_data);
}

// Clear Hub feature
TU_ATTR_ALWAYS_INLINE static inline
bool hub_clear_feature(uint8_t hub_addr, uint8_t feature, tuh_xfer_cb_t complete_cb, uintptr_t user_data) {
  return hub_port_clear_feature(hub_addr, 0, feature, complete_cb, user_data);
}
//--------------------------------------------------------------------+
// Internal Class Driver API
//--------------------------------------------------------------------+
bool hub_init       (void);
bool hub_deinit     (void);
bool hub_open       (uint8_t rhport, uint8_t dev_addr, tusb_desc_interface_t const *itf_desc, uint16_t max_len);
bool hub_set_config (uint8_t daddr, uint8_t itf_num);
bool hub_xfer_cb    (uint8_t daddr, uint8_t ep_addr, xfer_result_t event, uint32_t xferred_bytes);
void hub_close      (uint8_t dev_addr);

#ifdef __cplusplus
 }
#endif

#endif
