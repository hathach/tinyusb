/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2021 XMOS LIMITED
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

#if (TUSB_OPT_DEVICE_ENABLED && CFG_TUD_DFU_RUNTIME_AND_MODE)

#include "dfu_rt_and_mode_device.h"

#include "dfu_mode_device.h"
#include "dfu_rt_device.h"
#include "device/usbd_pvt.h"

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF
//--------------------------------------------------------------------+

//--------------------------------------------------------------------+
// INTERNAL OBJECT & FUNCTION DECLARATION
//--------------------------------------------------------------------+
typedef struct
{
  void     (* init             ) (void);
  void     (* reset            ) (uint8_t rhport);
  uint16_t (* open             ) (uint8_t rhport, tusb_desc_interface_t const * desc_intf, uint16_t max_len);
  bool     (* control_xfer_cb  ) (uint8_t rhport, uint8_t stage, tusb_control_request_t const * request);
} dfu_local_driver_t;

static dfu_local_driver_t ctx =
{
  .init            = NULL,
  .reset           = NULL,
  .open            = NULL,
  .control_xfer_cb = NULL
};

static dfu_protocol_type_t mode = 0x00;

//--------------------------------------------------------------------+
// USBD Driver API
//--------------------------------------------------------------------+
void dfu_d_init(void)
{
  mode = tud_dfu_init_in_mode_cb();

  switch (mode)
  {
    case DFU_PROTOCOL_RT:
    {
        ctx.init             = dfu_rtd_init;
        ctx.reset            = dfu_rtd_reset;
        ctx.open             = dfu_rtd_open;
        ctx.control_xfer_cb  = dfu_rtd_control_xfer_cb;
    }
    break;

    case DFU_PROTOCOL_DFU:
    {
      ctx.init             = dfu_moded_init;
      ctx.reset            = dfu_moded_reset;
      ctx.open             = dfu_moded_open;
      ctx.control_xfer_cb  = dfu_moded_control_xfer_cb;
    }
    break;

    default:
    {
      TU_LOG2("  DFU Unexpected mode: %u\r\n", mode);
    }
    break;
  }

  ctx.init();
}

void dfu_d_reset(uint8_t rhport)
{
  ctx.reset(rhport);
}

uint16_t dfu_d_open(uint8_t rhport, tusb_desc_interface_t const * itf_desc, uint16_t max_len)
{
  return ctx.open(rhport, itf_desc, max_len);
}

bool dfu_d_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const * request)
{
  return ctx.control_xfer_cb(rhport, stage, request);
}

#endif
