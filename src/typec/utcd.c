/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2023 Ha Thach (thach@tinyusb.org)
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

#if CFG_TUC_ENABLED

#include "tcd.h"
#include "utcd.h"

//--------------------------------------------------------------------+
//
//--------------------------------------------------------------------+

// Debug level of USBD
#define UTCD_DEBUG   2
#define TU_LOG_UTCD(...)   TU_LOG(UTCD_DEBUG, __VA_ARGS__)

// Event queue
// utcd_int_set() is used as mutex in OS NONE config
void utcd_int_set(bool enabled);
OSAL_QUEUE_DEF(utcd_int_set, _utcd_qdef, CFG_TUC_TASK_QUEUE_SZ, tcd_event_t);
tu_static osal_queue_t _utcd_q;

// if stack is initialized
static bool _utcd_inited = false;

// if port is initialized
static bool _port_inited[TUP_TYPEC_RHPORTS_NUM];

// Max possible PD size is 262 bytes
static uint8_t _rx_buf[262] TU_ATTR_ALIGNED(4);
static uint8_t _tx_buf[100] TU_ATTR_ALIGNED(4);

//--------------------------------------------------------------------+
//
//--------------------------------------------------------------------+
bool tuc_inited(uint8_t rhport) {
  return _utcd_inited && _port_inited[rhport];
}

bool tuc_init(uint8_t rhport, uint32_t port_type) {
  // Initialize stack
  if (!_utcd_inited) {
    tu_memclr(_port_inited, sizeof(_port_inited));

    _utcd_q = osal_queue_create(&_utcd_qdef);
    TU_ASSERT(_utcd_q != NULL);

    _utcd_inited = true;
  }

  // skip if port already initialized
  if ( _port_inited[rhport] ) {
    return true;
  }

  TU_LOG_UTCD("UTCD init on port %u\r\n", rhport);

  TU_ASSERT(tcd_init(rhport, port_type));
  tcd_int_enable(rhport);

  _port_inited[rhport] = true;
  return true;
}

//--------------------------------------------------------------------+
//
//--------------------------------------------------------------------+

bool utcd_msg_send(uint8_t rhport, pd_header_t const* header, void const* data) {
  // copy header
  memcpy(_tx_buf, header, sizeof(pd_header_t));

  // copy data objcet if available
  uint16_t const n_data_obj = header->n_data_obj;
  if (n_data_obj > 0) {
    memcpy(_tx_buf + sizeof(pd_header_t), data, n_data_obj * 4);
  }

  return tcd_msg_send(rhport, _tx_buf, sizeof(pd_header_t) + n_data_obj * 4);
}

bool parse_message(uint8_t rhport, uint8_t const* buf, uint16_t len) {
  (void) rhport;
  uint8_t const* p_end = buf + len;
  pd_header_t const* header = (pd_header_t const*) buf;
  uint8_t const * ptr = buf + sizeof(pd_header_t);

  if (header->n_data_obj == 0) {
    // control message
    switch (header->msg_type) {
      case PD_CTRL_GOOD_CRC:
        break;

      case PD_CTRL_ACCEPT:
        break;

      case PD_CTRL_REJECT:
        break;

      case PD_CTRL_PS_READY:
        break;

      default: break;
    }
  } else {
    // data message
    switch (header->msg_type) {
      case PD_DATA_SOURCE_CAP: {
        // Examine source capability and select a suitable PDO (starting from 1 with safe5v)
        uint8_t obj_pos = 1;

        for(size_t i=0; i<header->n_data_obj; i++) {
          TU_VERIFY(ptr < p_end);
          uint32_t const pdo = tu_le32toh(tu_unaligned_read32(ptr));

          switch ((pdo >> 30) & 0x03ul) {
            case PD_PDO_TYPE_FIXED: {
              pd_pdo_fixed_t const* fixed = (pd_pdo_fixed_t const*) &pdo;
              TU_LOG3("[Fixed] %u mV %u mA\r\n", fixed->voltage_50mv*50, fixed->current_max_10ma*10);
              break;
            }

            case PD_PDO_TYPE_BATTERY:
              break;

            case PD_PDO_TYPE_VARIABLE:
              break;

            case PD_PDO_TYPE_APDO:
              break;
          }

          ptr += 4;
        }

        // Send request with selected PDO position as response to Source Cap
        pd_rdo_fixed_variable_t rdo = {
            .current_extremum_10ma = 50, // max 500mA
            .current_operate_10ma = 30, // 300mA
            .reserved = 0,
            .epr_mode_capable = 0,
            .unchunked_ext_msg_support = 0,
            .no_usb_suspend = 0,
            .usb_comm_capable = 1,
            .capability_mismatch = 0,
            .give_back_flag = 0, // exteremum is max
            .object_position = obj_pos,
        };

        pd_header_t const req_header = {
            .msg_type = PD_DATA_REQUEST,
            .data_role = PD_DATA_ROLE_UFP,
            .specs_rev = PD_REV_20,
            .power_role = PD_POWER_ROLE_SINK,
            .msg_id = 0,
            .n_data_obj = 1,
            .extended = 0,
        };

        utcd_msg_send(rhport, &req_header, &rdo);

        break;
      }

      default: break;
    }
  }

  return true;
}

void tcd_event_handler(tcd_event_t const * event, bool in_isr) {
  (void) in_isr;
  switch(event->event_id) {
    case TCD_EVENT_CC_CHANGED:
      if (event->cc_changed.cc_state[0] || event->cc_changed.cc_state[1]) {
        // Attach
        tcd_msg_receive(event->rhport, _rx_buf, sizeof(_rx_buf));
      }else {
        // Detach
      }
      break;

    case TCD_EVENT_RX_COMPLETE:
      // TODO process message here in ISR, move to thread later
      if (event->rx_complete.result == XFER_RESULT_SUCCESS) {
        parse_message(event->rhport, _rx_buf, event->rx_complete.xferred_bytes);
      }

      // start new rx
      tcd_msg_receive(event->rhport, _rx_buf, sizeof(_rx_buf));
      break;

    default: break;
  }
}

//--------------------------------------------------------------------+
//
//--------------------------------------------------------------------+
void utcd_int_set(bool enabled) {
  // Disable all controllers since they shared the same event queue
  for (uint8_t p = 0; p < TUP_TYPEC_RHPORTS_NUM; p++) {
    if ( _port_inited[p] ) {
      if (enabled) {
        tcd_int_enable(p);
      }else {
        tcd_int_disable(p);
      }
    }
  }
}

#endif
