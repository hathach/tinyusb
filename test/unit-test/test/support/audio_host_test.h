/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 TinyUSB contributors
 * SPDX-License-Identifier: MIT
 */

#ifndef TUSB_AUDIO_HOST_TEST_H_
#define TUSB_AUDIO_HOST_TEST_H_

#include "common/tusb_common.h"

typedef struct tuh_xfer_s tuh_xfer_t;
typedef void (*tuh_xfer_cb_t)(tuh_xfer_t *xfer);

struct tuh_xfer_s {
  uint8_t       daddr;
  uint8_t       ep_addr;
  uint8_t       TU_RESERVED;
  xfer_result_t result;
  uint32_t      actual_len;
  union {
    const tusb_control_request_t *setup;
    uint32_t                      buflen;
  };
  uint8_t      *buffer;
  tuh_xfer_cb_t complete_cb;
  uintptr_t     user_data;
};

#define TU_API_SYNC(...) return false
#include "class/audio/audio_host.h"
#undef TU_API_SYNC

#include "host/usbh_pvt.h"

#endif
