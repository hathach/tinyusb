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

#if (TUSB_OPT_DEVICE_ENABLED && CFG_TUD_VENDOR)

#include "common/tusb_common.h"
#include "vendor_device.h"
#include "device/usbd_pvt.h"

/*------------------------------------------------------------------*/
/* MACRO TYPEDEF CONSTANT ENUM
 *------------------------------------------------------------------*/

/*------------------------------------------------------------------*/
/* VARIABLE DECLARATION
 *------------------------------------------------------------------*/
typedef struct {
  uint8_t itf_num;

  uint8_t ep_in;
  uint8_t ep_out;

} vendord_interface_t;

static vendord_interface_t _vendord_itf;

/*------------------------------------------------------------------*/
/* FUNCTION DECLARATION
 *------------------------------------------------------------------*/
void vendord_init(void)
{
  tu_varclr(&_vendord_itf);
}

void vendord_reset(uint8_t rhport)
{

}


bool vendord_open(uint8_t rhport, tusb_desc_interface_t const * p_desc_itf, uint16_t *p_len)
{
  vendord_interface_t* p_itf = &_vendord_itf;

  // Open endpoint pair with usbd helper
  tusb_desc_endpoint_t const *p_desc_ep = (tusb_desc_endpoint_t const *) tu_desc_next(p_desc_itf);
  TU_ASSERT( usbd_open_edpt_pair(rhport, p_desc_ep, TUSB_XFER_BULK, &p_itf->ep_out, &p_itf->ep_in) );

  p_itf->itf_num = p_desc_itf->bInterfaceNumber;

  (*p_len) = sizeof(tusb_desc_interface_t) + 2*sizeof(tusb_desc_endpoint_t);

  // TODO Prepare for incoming data
//  TU_ASSERT( usbd_edpt_xfer(rhport, p_itf->ep_out, (uint8_t*) &p_msc->cbw, sizeof(msc_cbw_t)) );

  return true;
}

bool vendord_xfer_cb(uint8_t rhport, uint8_t ep_addr, xfer_result_t event, uint32_t xferred_bytes)
{
  return true;
}


#endif
