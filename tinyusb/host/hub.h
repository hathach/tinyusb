/**************************************************************************/
/*!
    @file     hub.h
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
    (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
    LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
    ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
    SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

    This file is part of the tinyusb stack.
*/
/**************************************************************************/

/** \ingroup group_class
 *  \defgroup ClassDriver_Hub Hub (Host only)
 *  \details  Like most PC's OS, Hub support is completely hidden from Application. In fact, application cannot determine whether
 *            a device is mounted directly via roothub or via a hub's port. All Hub-related procedures are performed and managed
 *            by tinyusb stack. Unless you are trying to develop the stack itself, there are nothing else can be used by Application.
 *  \note     Due to my laziness, only 1-level of Hub is supported. In other way, the stack cannot mount a hub via another hub.
 *  @{
 */

#ifndef _TUSB_HUB_H_
#define _TUSB_HUB_H_

#include "common/common.h"
#include "usbh.h"

#ifdef __cplusplus
 extern "C" {
#endif

//D1...D0: Logical Power Switching Mode
//00:  Ganged power switching (all ports’power at
//once)
//01:  Individual port power switching
//1X:  Reserved. Used only on 1.0 compliant hubs
//that implement no power switching
//D2:  Identifies a Compound Device
//0: Hub is not part of a compound device.
//1: Hub is part of a compound device.
//D4...D3: Over-current Protection Mode
//00: Global Over-current Protection. The hub
//reports over-current as a summation of all
//ports’current draw, without a breakdown of
//individual port over-current status.
//01: Individual Port Over-current Protection. The
//hub reports over-current on a per-port basis.
//Each port has an over-current status.
//1X: No Over-current Protection. This option is
//allowed only for bus-powered hubs that do not
//implement over-current protection.
//
//D6...D5: TT Think TIme
//00:  TT requires at most 8 FS bit times of inter
//transaction gap on a full-/low-speed
//downstream bus.
//01:  TT requires at most 16 FS bit times.
//10:  TT requires at most 24 FS bit times.
//11:  TT requires at most 32 FS bit times.
//D7: Port Indicators Supported
//0:  Port Indicators are not supported on its
//downstream facing ports and the
//PORT_INDICATOR request has no effect.
//1:  Port Indicators are supported on its
//downstream facing ports and the
//PORT_INDICATOR request controls the
//indicators. See Section 11.5.3.
//D15...D8: Reserved

typedef struct ATTR_PACKED{
  uint8_t  bLength           ; ///< Size of descriptor
  uint8_t  bDescriptorType   ; ///< Other_speed_Configuration Type
  uint8_t  bNbrPorts;
  uint16_t wHubCharacteristics;
  uint8_t  bPwrOn2PwrGood;
  uint8_t  bHubContrCurrent;
  uint8_t  DeviceRemovable; // bitmap each bit for a port (from bit1)
  uint8_t  PortPwrCtrlMask; // just for compatibility, should be 0xff
} descriptor_hub_desc_t;

STATIC_ASSERT( sizeof(descriptor_hub_desc_t) == 9, "size is not correct");

enum {
  HUB_REQUEST_GET_STATUS      = 0  ,
  HUB_REQUEST_CLEAR_FEATURE   = 1  ,

  HUB_REQUEST_SET_FEATURE     = 3  ,

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

  HUB_FEATURE_PORT_POWER               = 8,
  HUB_FEATURE_PORT_LOW_SPEED           = 9,

  HUB_FEATURE_PORT_CONNECTION_CHANGE   = 16,
  HUB_FEATURE_PORT_ENABLE_CHANGE       = 17,
  HUB_FEATURE_PORT_SUSPEND_CHANGE      = 18,
  HUB_FEATURE_PORT_OVER_CURRENT_CHANGE = 19,
  HUB_FEATURE_PORT_RESET_CHANGE        = 20,
  HUB_FEATURE_PORT_TEST                = 21,
  HUB_FEATURE_PORT_INDICATOR           = 22
};

// data in response of HUB_REQUEST_GET_STATUS, wIndex = 0 (hub)
typedef struct {
  union{
    struct ATTR_PACKED {
      uint16_t local_power_source : 1;
      uint16_t over_current       : 1;
      uint16_t : 14;
    };

    uint16_t value;
  } status, status_change;
} hub_status_response_t;

STATIC_ASSERT( sizeof(hub_status_response_t) == 4, "size is not correct");

// data in response of HUB_REQUEST_GET_STATUS, wIndex = Port num
typedef struct {
  union {
    struct ATTR_PACKED {
      uint16_t connect_status             : 1;
      uint16_t port_enable                : 1;
      uint16_t suspend                    : 1;
      uint16_t over_current               : 1;
      uint16_t reset                      : 1;

      uint16_t                            : 3;
      uint16_t port_power                 : 1;
      uint16_t low_speed_device_attached  : 1;
      uint16_t high_speed_device_attached : 1;
      uint16_t port_test_mode             : 1;
      uint16_t port_indicator_control     : 1;
      uint16_t : 0;
    };

    uint16_t value;
  } status_current, status_change;
} hub_port_status_response_t;

STATIC_ASSERT( sizeof(hub_port_status_response_t) == 4, "size is not correct");

tusb_error_t hub_port_reset_subtask(uint8_t hub_addr, uint8_t hub_port);
tusb_error_t hub_port_clear_feature_subtask(uint8_t hub_addr, uint8_t hub_port, uint8_t feature);
tusb_speed_t hub_port_get_speed(void);
tusb_error_t hub_status_pipe_queue(uint8_t dev_addr);

//--------------------------------------------------------------------+
// USBH-CLASS DRIVER API
//--------------------------------------------------------------------+
#ifdef _TINY_USB_SOURCE_FILE_

void         hub_init(void);
tusb_error_t hub_open_subtask(uint8_t dev_addr, tusb_descriptor_interface_t const *p_interface_desc, uint16_t *p_length) ATTR_WARN_UNUSED_RESULT;
void         hub_isr(pipe_handle_t pipe_hdl, tusb_event_t event, uint32_t xferred_bytes);
void         hub_close(uint8_t dev_addr);

#endif

#ifdef __cplusplus
 }
#endif

#endif /* _TUSB_HUB_H_ */

/** @} */
